"""Independent bounded P2 QA for static 90-degree contact at lateral walls.

This is NOT a full CFD, moving contact-line, MARIN/SPHERIC or FSI acceptance.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
ROOT=CORE.parents[1]
RUNS=ROOT/"ENGINE TEST STUBE"/"runs"
EXE=CORE/"build"/"upc_p2_wall90_height_capillary_tests.exe"
NATIVE=RUNS/"p2_wall90_height_native_20261102.txt"
FULL=RUNS/"p2_wall90_full_ctest_20261102.txt"
GATE=RUNS/"unified_physics_gate.json"
REPORT=RUNS/"p2_wall90_height_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_vof_height_curvature_field3d.hpp",
    "include/upc/p2_two_phase_capillary_mac_vof3d.hpp",
    "tests/p2_wall90_height_capillary3d_tests.cpp",
    "scripts/p2_wall90_height_build.cmd",
    "scripts/p2_wall90_full_regression.cmd",
    "scripts/p2_wall90_height_acceptance.py",
    "docs/P2_WALL90_HEIGHT_CAPILLARY_PARTIAL.md",
]
GEO=re.compile(r"WALL90_GEOMETRY axis=(\d+) grid=(\d+) curvature=(\S+) analytic=(\S+) error=(\S+) legacy_error=(\S+) phase_complete=false")
PROJECT=re.compile(r"WALL90_PROJECT effect=(\S+) divergence=(\S+) phase_complete=false")
MOVING=re.compile(r"WALL90_MOVING divergence=(\S+) water_residual=(\S+) phase_complete=false")
PASS=re.compile(r"PASS P2_WALL90_HEIGHT_CAPILLARY assertions=(\d+) phase_complete=false")
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path:Path)->str:
    b=path.read_bytes()
    return b.decode("utf-16" if b.startswith((b"\xff\xfe",b"\xfe\xff")) else "utf-8").replace("\r\n","\n")
def number(s:str)->float:
    x=float(s)
    if not math.isfinite(x):raise ValueError("Nonfinite native value")
    return x
def independent_analytic(n:int)->float:
    dx=1.2/n
    U=V=.5*dx
    w=2*math.pi/1.2
    hu=-.02*w*math.sin(w*U)
    hv=-.015*w*math.sin(w*V)
    huu=-.02*w*w*math.cos(w*U)
    hvv=-.015*w*w*math.cos(w*V)
    d=1+hu*hu+hv*hv
    return -(huu*(1+hv*hv)+hvv*(1+hu*hu))/(d*math.sqrt(d))
def parse(log:str)->dict:
    grids={}
    project=moving=passed=None
    for line in log.strip().splitlines():
        m=GEO.fullmatch(line)
        if m:
            axis,n=map(int,m.group(1,2))
            if axis not in (0,1,2) or n not in (12,20,32) or (axis,n) in grids:
                raise ValueError("Invalid/duplicate 3D geometry axis or grid")
            k,ref,err,old=map(number,m.group(3,4,5,6))
            independent=independent_analytic(n)
            if not (abs(ref-independent)<1e-12 and
                    abs(err-abs(k-independent))<2e-12 and
                    0<err<.03 and old>err*2):
                raise ValueError("Analytic wall curvature, error or legacy comparison invalid")
            grids[axis,n]={"curvature_inv_m":k,"analytic_inv_m":independent,
                           "error_inv_m":err,"legacy_error_inv_m":old}
            continue
        m=PROJECT.fullmatch(line)
        if m:
            if project is not None:raise ValueError("Duplicate project")
            effect,div=map(number,m.groups())
            if not (1e-12<effect<1e-3 and 0<=div<1e-9):
                raise ValueError("Invalid projected MAC effect/divergence")
            project={"effect_m_s":effect,"divergence_s_inv":div}
            continue
        m=MOVING.fullmatch(line)
        if m:
            if moving is not None:raise ValueError("Duplicate moving")
            div,mass=map(number,m.groups())
            if not (0<=div<1e-9 and abs(mass)<1e-8):
                raise ValueError("Invalid moving PCG or mass balance")
            moving={"divergence_s_inv":div,"water_mass_residual_kg":mass}
            continue
        m=PASS.fullmatch(line)
        if m:
            if passed is not None or int(m[1])<66:
                raise ValueError("Native assertion coverage missing")
            passed=int(m[1])
    if len(grids)!=9 or project is None or moving is None or passed is None:
        raise ValueError("Missing 3-axis/3-grid/PCG/moving/negative evidence")
    for axis in range(3):
        if not (grids[axis,32]["error_inv_m"]<
                grids[axis,20]["error_inv_m"]<
                grids[axis,12]["error_inv_m"]):
            raise ValueError("No manufactured wall curvature grid convergence")
    return {"grids":{f"{axis}:{n}":row for (axis,n),row in grids.items()},
            "project":project,"moving":moving,"native_assertions":passed}
def tamper(log:str)->int:
    lines=log.strip().splitlines()
    forged=[
        log.replace("axis=0 grid=12","axis=0 grid=11",1),
        log.replace("axis=1 grid=20","axis=0 grid=20",1),
        log.replace("axis=2 grid=32","axis=2 grid=20",1),
        log.replace("phase_complete=false","phase_complete=true",1),
        log.replace("error=0.020864768850109128","error=NaN",1),
        log.replace("legacy_error=0.26786309619430904","legacy_error=0",1),
        log.replace("WALL90_PROJECT effect=","WALL90_PROJECT effect=-",1),
        log.replace("WALL90_MOVING divergence=","WALL90_MOVING divergence=nan",1),
        log.replace("assertions=66","assertions=1",1),
        "\n".join(line for line in lines if not line.startswith("WALL90_MOVING "))+"\n",
    ]
    rejected=0
    for bad in forged:
        if bad==log:raise ValueError("Tamper fixture did not alter source")
        try:parse(bad)
        except (ValueError,OverflowError):rejected+=1
        else:raise ValueError("Forged native evidence accepted")
    return rejected
def main():
    for f in (EXE,NATIVE,FULL,GATE):
        if not f.is_file():raise RuntimeError("Missing real evidence: "+str(f))
    log=read(NATIVE)
    measurements=parse(log)
    r=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,text=True,timeout=90)
    if r.returncode!=0 or r.stdout.strip()!=log.strip():
        raise RuntimeError("Independent native rerun changed/fails")
    full=read(FULL)
    if "100% tests passed, 0 tests failed out of 150" not in full:
        raise RuntimeError("Full 150 CTest regression not verified")
    gate=json.loads(read(GATE))
    if (gate["phases"]["P2"]["status"]=="PASS" or
        gate["all_phases_complete"] is not False):
        raise RuntimeError("Full P2 gate was incorrectly marked PASS")
    rejected=tamper(log)
    if rejected!=10:raise RuntimeError("Tamper QA insufficient")
    report={
        "phase":"P2","milestone":"static_90_degree_lateral_wall_height_curvature_partial",
        "partial_accepted":True,"phase_complete":False,"full_P2_accepted":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native":measurements,
        "checks":{
            "three_axes_three_grid_analytic_wall_curvature":True,
            "grid_refinement_manufactured_converged":True,
            "old_boundary_extrapolation_worse":True,
            "live_variable_density_mac_pcg":True,
            "moving_3d_vof_mass_and_projection":True,
            "independent_native_rerun_byte_equal":True,
            "full_ctest_150_of_150":True,
            "official_full_P2_gate_still_open":True,
            "negative_tamper_rejected_10":True,
        },
        "tamper_rejected":rejected,
        "negative_results":{
            "initial_no_vcvars_compile_failure_captured":True,
            "initial_missing_feature_compile_log_persisted":False,
            "physical_dynamic_contact_angle_validated":False,
            "surface_tension_grid_convergence_validated":False,
            "global_fluid_surface_energy_closed":False,
            "marin_spheric_6s_3grid_measured_pass":False,
            "bidirectional_fsi_validated":False,
            "unreal58_native_e2e_of_this_path":False,
        },
        "native_exe_sha256":sha(EXE),
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "evidence_sha256":{
            name:sha(RUNS/name) for name in (
                "p2_wall90_initial_expected_failure_20261102.txt",
                "p2_wall90_height_build_20261102.txt",
                "p2_wall90_height_native_20261102.txt",
                "p2_wall90_full_build_20261102.txt",
                "p2_wall90_full_ctest_20261102.txt",
            )
        },
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print("P2_WALL90_QA=PASS_PARTIAL")
    print("NATIVE_ASSERTIONS="+str(measurements["native_assertions"]))
    print("TAMPER_REJECTED="+str(rejected))
    print("FULL_P2_ACCEPTED=False")
    print("REPORT_SHA256="+sha(REPORT))
if __name__=="__main__":main()
