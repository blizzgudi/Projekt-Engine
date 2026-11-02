"""Independent native QA of P2 graph-only height curvature -> MAC/PCG/live VOF.
A bounded partial milestone, never a P2 full-phase or experimental CFD PASS.
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
EXE=CORE/"build"/"upc_p2_vof_height_capillary_live_tests.exe"
NATIVE=RUNS/"p2_height_capillary_live_native_20261102.txt"
BUILD=RUNS/"p2_height_capillary_live_build_20261102.txt"
FULL_BUILD=RUNS/"p2_height_capillary_live_full_build_20261102.txt"
FAILED_BUILD=RUNS/"p2_height_capillary_live_build_initial_20261102.txt"
GATE=RUNS/"unified_physics_gate.json"
REPORT=RUNS/"p2_height_capillary_live_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_vof_height_curvature3d.hpp",
    "include/upc/p2_vof_height_curvature_field3d.hpp",
    "include/upc/p2_two_phase_capillary_mac_vof3d.hpp",
    "tests/p2_vof_height_capillary_live3d_tests.cpp",
    "scripts/p2_vof_height_capillary_live_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_VOF_HEIGHT_CAPILLARY_LIVE_PARTIAL.md",
]
FIELD=re.compile(r"HEIGHT_CAPILLARY_FIELD axis=(\d+) grid=(\d+) curvature=(\S+) error=(\S+) phase_complete=false")
STATIC=re.compile(r"HEIGHT_CAPILLARY_STATIC grid=(\d+) spurious_velocity=(\S+) phase_complete=false")
PROJECT=re.compile(r"HEIGHT_CAPILLARY_PROJECT grid=12 curvature=(\S+) velocity=(\S+) diffuse_velocity=(\S+) effect=(\S+) phase_complete=false")
LIVE=re.compile(r"HEIGHT_CAPILLARY_LIVE grid=8 effect=(\S+) divergence=(\S+) phase_complete=false")
PASS=re.compile(r"PASS P2_VOF_HEIGHT_CAPILLARY_LIVE assertions=(\d+) phase_complete=false")
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path:Path)->str:
    b=path.read_bytes()
    return b.decode("utf-16" if b.startswith((b"\xff\xfe",b"\xfe\xff")) else "utf-8").replace("\r\n","\n")
def number(s:str)->float:
    x=float(s)
    if not math.isfinite(x):raise ValueError("Nonfinite native numeric evidence")
    return x
def independent_curvature(dx:float)->float:
    # Independently reconstruct all nine height samples of a manufactured
    # radius-one sphere cap; no call into C++ production implementation.
    def h(i:int,j:int)->float:
        U=(i-.5)*dx
        V=(j-.5)*dx
        return .05+math.sqrt(1-U*U-V*V)
    hu=(h(1,0)-h(-1,0))/(2*dx)
    hv=(h(0,1)-h(0,-1))/(2*dx)
    huu=(h(1,0)-2*h(0,0)+h(-1,0))/(dx*dx)
    hvv=(h(0,1)-2*h(0,0)+h(0,-1))/(dx*dx)
    huv=(h(1,1)-h(1,-1)-h(-1,1)+h(-1,-1))/(4*dx*dx)
    d=1+hu*hu+hv*hv
    return -(huu*(1+hv*hv)+hvv*(1+hu*hu)-2*hu*hv*huv)/(d*math.sqrt(d))
def parse(s:str)->dict:
    fields={}
    static={}
    project=None
    live=None
    passed=None
    for line in s.strip().splitlines():
        m=FIELD.fullmatch(line)
        if m:
            axis,n=map(int,m.group(1,2))
            k,e=map(number,m.group(3,4))
            if axis not in (0,1,2) or n not in (12,20,40) or (axis,n) in fields:
                raise ValueError("Invalid/duplicate height grid or axis")
            independent=independent_curvature(1.2/n)
            if abs(k-independent)>2e-11 or abs(e-abs(k-2))>2e-12 or e>=.007:
                raise ValueError("Height curvature disagrees with independent 3D sphere-cap reference")
            fields[axis,n]={"curvature_inv_m":k,"error_inv_m":e,
                            "independent_inv_m":independent}
            continue
        m=STATIC.fullmatch(line)
        if m:
            n=int(m[1]);v=number(m[2])
            if n not in (8,12,16) or n in static or not (0<=v<1e-6):
                raise ValueError("Invalid/duplicate static spurious-current grid")
            static[n]=v
            continue
        m=PROJECT.fullmatch(line)
        if m:
            if project is not None:raise ValueError("Duplicate projection")
            k,v,diff,e=map(number,m.groups())
            if not (1.9<k<2.1 and 0<=v<1e-6 and diff>v and
                    abs(e-(diff-v))<1e-9 and e>1e-9):
                raise ValueError("Capillary height vs diffuse MAC projection invalid")
            project={"mean_curvature_inv_m":k,"height_spurious_m_s":v,
                     "diffuse_spurious_m_s":diff,"face_effect_m_s":e}
            continue
        m=LIVE.fullmatch(line)
        if m:
            if live is not None:raise ValueError("Duplicate moving live step")
            effect,divergence=map(number,m.groups())
            if not (1e-12<effect<1e-2 and 0<=divergence<1e-9):
                raise ValueError("Moving MAC/VOF height source/PCG invalid")
            live={"effect_m_s":effect,"divergence_s_inv":divergence}
            continue
        m=PASS.fullmatch(line)
        if m:
            if passed is not None or int(m[1])<72:
                raise ValueError("Native assertion coverage invalid")
            passed=int(m[1])
            continue
        raise ValueError("Unknown or forged native evidence line")
    if set(fields)!={(axis,n) for axis in range(3) for n in (12,20,40)} or \
       set(static)!={8,12,16} or project is None or live is None or passed is None:
        raise ValueError("Incomplete 3D geometry, static PCG or moving live evidence")
    for axis in range(3):
        if not (fields[axis,40]["error_inv_m"]<
                fields[axis,20]["error_inv_m"]/2<
                fields[axis,12]["error_inv_m"]):
            raise ValueError("Manufactured sphere-cap curvature does not refine")
    return {"manufactured_sphere_cap":{f"{axis}:{n}":value for (axis,n),value in fields.items()},"static_spurious_m_s":static,
            "static_spurious_grid_converged":static[16]<static[12]<static[8],
            "projected":project,"moving":live,"native_assertions":passed}
def tamper(log:str)->int:
    cases=[
        ("HEIGHT_CAPILLARY_FIELD axis=0 grid=12",
         "HEIGHT_CAPILLARY_FIELD axis=9 grid=12"),
        ("HEIGHT_CAPILLARY_FIELD axis=1 grid=40",
         "HEIGHT_CAPILLARY_FIELD axis=1 grid=12"),
        ("curvature=2.0050121702597439","curvature=NaN"),
        ("error=0.0050121702597438755","error=0.7"),
        ("phase_complete=false","phase_complete=true"),
        ("HEIGHT_CAPILLARY_STATIC grid=8","HEIGHT_CAPILLARY_STATIC grid=7"),
        ("spurious_velocity=2.3730534371958646e-09","spurious_velocity=-1"),
        ("HEIGHT_CAPILLARY_PROJECT grid=12","HEIGHT_CAPILLARY_PROJECT grid=15"),
        ("effect=2.2876417475664349e-06","effect=-1"),
        ("HEIGHT_CAPILLARY_LIVE grid=8","HEIGHT_CAPILLARY_LIVE grid=7"),
        ("divergence=1.3254371558635292e-16","divergence=1"),
        ("assertions=72","assertions=1"),
    ]
    rejected=0
    for old,new in cases:
        if old not in log:raise ValueError("Missing tamper fixture: "+old)
        try:parse(log.replace(old,new,1))
        except (ValueError,OverflowError):rejected+=1
        else:raise ValueError("Forged native result accepted: "+old)
    return rejected
def main():
    for path in (EXE,NATIVE,BUILD,FULL_BUILD,FAILED_BUILD,GATE):
        if not path.is_file():raise RuntimeError("Missing native evidence: "+str(path))
    log=read(NATIVE)
    metrics=parse(log)
    rerun=subprocess.run([str(EXE)],cwd=CORE,text=True,capture_output=True,timeout=120)
    if rerun.returncode!=0 or rerun.stdout.strip()!=log.strip():
        raise RuntimeError("Independent native rerun does not match saved output")
    if "Linking CXX executable upc_p2_vof_height_capillary_live_tests.exe" not in read(BUILD):
        raise RuntimeError("No successful native MSVC link evidence")
    if "No such file or directory" not in read(FAILED_BUILD):
        raise RuntimeError("Initial MSVC environment failure not documented")
    gate=json.loads(GATE.read_text(encoding="utf-8"))
    p2=gate["phases"]["P2"]
    ctest=gate["ctest"]
    if not (ctest["ok"] and "149" in ctest["tail"] and
            p2["status"]=="PARTIAL" and
            len(p2["full_acceptance_open"])>=3 and
            gate["all_phases_complete"] is False):
        raise RuntimeError("Official 149-suite native regression or P2 partial gate missing")
    n_tamper=tamper(log)
    result={
        "phase":"P2",
        "milestone":"strict_3d_height_function_capillary_mac_pcg_moving_vof_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "full_p2_accepted":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":metrics["native_assertions"],
        "ctest_suites":149,
        "tamper_rejected":n_tamper,
        "metrics":metrics,
        "negative_results":{
            "static_spurious_grid_converged":metrics["static_spurious_grid_converged"],
            "sharp_plic_interface_stress_validated":False,
            "full_spherical_droplet_supported":False,
            "global_surface_energy_closed":False,
            "moving_bidirectional_fsi_validated":False,
            "MARIN_SPHERIC_six_second_three_grid_measured_pass":False,
            "UE58_native_e2e_of_this_chain":False,
        },
        "independent_native_rerun_byte_equal":True,
        "source_sha256":{s:sha(CORE/s) for s in SOURCES},
        "native_exe_sha256":sha(EXE),
        "artifact_sha256":{p.name:sha(p) for p in
                           (NATIVE,BUILD,FULL_BUILD,FAILED_BUILD,GATE)},
    }
    REPORT.write_text(json.dumps(result,indent=2,ensure_ascii=False),
                      encoding="utf-8")
    print("HEIGHT_CAPILLARY_QA=PASS_PARTIAL")
    print("STATIC_SPURIOUS_GRID_CONVERGED="+
          str(metrics["static_spurious_grid_converged"]))
    print("TAMPER_REJECTED="+str(n_tamper))
    print("FULL_P2_ACCEPTED=False")
    print("REPORT_SHA256="+sha(REPORT))
if __name__=="__main__":main()
