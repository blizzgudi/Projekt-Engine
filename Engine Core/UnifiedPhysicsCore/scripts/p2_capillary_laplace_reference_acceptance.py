"""Independent P2 PARTIAL QA: native Young-Laplace manufactured equilibrium.

Does not certify moving free-surface curvature, global energy, FSI, or MARIN.
The analytic 2/R curvature is an externally supplied reference, not a
production interface reconstruction. Negative convergence is mandatory.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime,timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
EXE=CORE/"build"/"upc_p2_capillary_laplace_reference_tests.exe"
NATIVE=RUNS/"p2_laplace_reference_native_20261102.txt"
BUILD=RUNS/"p2_laplace_reference_build_20261102.txt"
TARGET=RUNS/"p2_laplace_reference_target_ctest_20261102.txt"
FULL=RUNS/"p2_laplace_reference_full_ctest_20261102.txt"
GATE=RUNS/"unified_physics_gate.json"
REPORT=RUNS/"p2_capillary_laplace_reference_partial.json"
SOURCE=[
    "CMakeLists.txt",
    "include/upc/p2_capillary_laplace_reference3d.hpp",
    "include/upc/p2_two_phase_capillary_mac_vof3d.hpp",
    "include/upc/p2_two_phase_mac_pressure3d.hpp",
    "tests/p2_capillary_laplace_reference3d_tests.cpp",
    "scripts/p2_capillary_laplace_reference_build.cmd",
    "scripts/p2_capillary_laplace_reference_acceptance.py",
    "docs/P2_CAPILLARY_LAPLACE_REFERENCE_PARTIAL.md",
    "scripts/phase_gate.py",
]
LINE=re.compile(
    r"^LAPLACE_REFERENCE grid=(\d+)x(\d+)x(\d+) dx_m=(\S+) "
    r"analytic_jump_pa=(\S+) measured_jump_pa=(\S+) "
    r"max_pressure_error_pa=(\S+) ideal_post_velocity_m_s=(\S+) "
    r"diffuse_post_velocity_m_s=(\S+) pure_water=(\d+) pure_air=(\d+) "
    r"phase_complete=false$")
PASS=re.compile(r"^PASS P2_LAPLACE_REFERENCE assertions=(\d+) phase_complete=false$")

def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def finite(s:str)->float:
    value=float(s)
    if not math.isfinite(value):
        raise ValueError("Nonfinite Young-Laplace evidence")
    return value

def parse(raw:str)->dict:
    lines=raw.strip().splitlines()
    if len(lines)!=4:
        raise ValueError("Expected three native grid rows and one PASS footer")
    grids={}
    for line in lines[:3]:
        m=LINE.fullmatch(line)
        if not m:
            raise ValueError("Corrupt native Laplace row")
        nx,ny,nz=map(int,m.group(1,2,3))
        if nx!=ny or ny!=nz or nx in grids or nx not in (8,12,16):
            raise ValueError("Wrong or duplicate 3D grid")
        dx,analytic,measured,perror,ideal,diffuse=map(finite,m.group(4,5,6,7,8,9))
        water,air=map(int,m.group(10,11))
        if (abs(nx*dx-1.2)>1e-12 or
            abs(analytic-2*.072/.31)>1e-12 or
            abs(measured-analytic)>1e-7 or
            not 0<=perror<1e-7 or
            not 0<=ideal<1e-8 or
            not diffuse>ideal+1e-8 or
            water<1 or air<1 or water+air>=nx**3):
            raise ValueError("Young-Laplace reference did not balance or has bad phases")
        grids[nx]={
            "dx_m":dx,
            "analytic_jump_pa":analytic,
            "native_recovered_jump_pa":measured,
            "max_pressure_error_pa":perror,
            "ideal_post_velocity_m_s":ideal,
            "diffuse_post_velocity_m_s":diffuse,
            "pure_water_cells":water,"pure_air_cells":air,
        }
    if set(grids)!={8,12,16}:
        raise ValueError("Missing one native 3D resolution")
    m=PASS.fullmatch(lines[3])
    if not m or int(m.group(1))<52:
        raise ValueError("Native assertion count or phase flag invalid")
    # Preserve the observed failure; do not silently report grid convergence.
    if not (grids[8]["diffuse_post_velocity_m_s"]<
            grids[12]["diffuse_post_velocity_m_s"]<
            grids[16]["diffuse_post_velocity_m_s"]):
        raise ValueError("Expected nonconvergent diffuse reference not reproduced")
    return {"grids":grids,"native_assertions":int(m.group(1)),
            "diffuse_spurious_velocity_converged":False,
            "manufactured_constant_curvature_pcg_balanced":True}

def corrupt(raw:str)->dict:
    lines=raw.strip().splitlines()
    changes={
        "missing_grid":"\n".join(lines[:2]+lines[3:]),
        "duplicate_grid":raw.replace("grid=12x12x12","grid=8x8x8",1),
        "forged_full_phase":raw.replace("phase_complete=false","phase_complete=true",1),
        "false_jump":re.sub(r"measured_jump_pa=\S+","measured_jump_pa=0",raw,count=1),
        "false_pressure_error":re.sub(r"max_pressure_error_pa=\S+",
                                      "max_pressure_error_pa=0.1",raw,count=1),
        "nonfinite_velocity":re.sub(r"ideal_post_velocity_m_s=\S+",
                                    "ideal_post_velocity_m_s=nan",raw,count=1),
        "false_diffuse_velocity":re.sub(r"diffuse_post_velocity_m_s=\S+",
                                        "diffuse_post_velocity_m_s=0",raw,count=1),
        "wrong_dx":re.sub(r"dx_m=\S+","dx_m=0.2",raw,count=1),
        "no_pure_water":re.sub(r"pure_water=\d+","pure_water=0",raw,count=1),
        "assertion_forgery":raw.replace("assertions=52","assertions=0",1),
    }
    rejected={}
    for name,mutant in changes.items():
        if mutant==raw:
            raise ValueError("Negative fixture failed to mutate "+name)
        try:
            parse(mutant)
        except (ValueError,TypeError,KeyError):
            rejected[name]=True
        else:
            rejected[name]=False
    return rejected

def main():
    for file in (EXE,NATIVE,BUILD,TARGET,FULL,GATE):
        if not file.is_file():
            raise RuntimeError("Missing native or official gate evidence "+str(file))
    raw=NATIVE.read_text(encoding="utf-8")
    metrics=parse(raw)
    rerun=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,text=True,
                         encoding="utf-8",timeout=90)
    if rerun.returncode!=0 or rerun.stdout.strip()!=raw.strip():
        raise RuntimeError("Independent native executable rerun failed/changed")
    negatives=corrupt(raw)
    if not all(negatives.values()):
        raise RuntimeError("Forged Young-Laplace evidence accepted")
    def read_log(path:Path)->str:
        raw=path.read_bytes()
        return raw.decode("utf-16" if raw.startswith((b"\xff\xfe",b"\xfe\xff"))
                          else "utf-8-sig",errors="replace")
    full=read_log(FULL)
    target=read_log(TARGET)
    if ("100% tests passed, 0 tests failed out of 147" not in full or
        "100% tests passed, 0 tests failed out of 1" not in target):
        raise RuntimeError("Native full or targeted CTest not PASS")
    gate=json.loads(GATE.read_text(encoding="utf-8"))
    if (gate["phases"]["P2"]["status"]!="PARTIAL" or
        gate["all_phases_complete"] is not False):
        raise RuntimeError("Official P2 full-phase gate falsely approved")
    report={
        "phase":"P2",
        "milestone":"independent_3d_exact_curvature_young_laplace_pcg_reference",
        "partial_accepted":True,
        "phase_complete":False,
        "full_p2_accepted":False,
        "analytic_curvature_externally_supplied":True,
        "moving_plic_stress_validated":False,
        "independent_marin_measured_6s_3grid_validated":False,
        "bidirectional_fsi_validated":False,
        "global_capillary_energy_closed":False,
        "metrics":metrics,
        "negative_evidence":{
            "diffuse_spurious_velocity_grows_with_grid_refinement":True,
            "diffuse_curvature_not_grid_converged":True,
            "tamper_rejections":negatives,
        },
        "native_rerun_identical":True,
        "native_ctest_pass":147,
        "source_sha256":{name:sha(CORE/name) for name in SOURCE},
        "artifact_sha256":{
            "native_executable":sha(EXE),
            "native_stdout":sha(NATIVE),
            "build_log":sha(BUILD),
            "target_ctest":sha(TARGET),
            "full_ctest":sha(FULL),
            "official_phase_gate":sha(GATE),
        },
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "limits":"Exact 2/R imposed on manufactured static sphere only; "
                 "diffuse curvature spurious velocity increases 8^3->12^3->16^3. "
                 "No sharp moving interface, physical energy closure, FSI, "
                 "independent 6s MARIN H1-H4/P1-P8 convergence or UE Native E2E.",
    }
    REPORT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",
                      encoding="utf-8")
    print("P2_LAPLACE_REFERENCE_QA=PASS_PARTIAL NATIVE_ASSERTIONS="+
          str(metrics["native_assertions"])+" CTEST=147/147")
    print("FORGED_TRACES_REJECTED="+str(sum(negatives.values())))
    print("DIFFUSE_SPURIOUS_GRID_CONVERGED=False FULL_P2_ACCEPTED=False")
    print("REPORT_SHA256="+sha(REPORT))

if __name__=="__main__":
    main()
