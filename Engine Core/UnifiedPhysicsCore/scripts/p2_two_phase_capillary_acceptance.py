"""Independent P2 capillary-partial QA: native rerun, analytic sphere, tamper tests.

This is NOT a P2 full gate. In particular the measured spurious currents
increase between the two grids and there is no MARIN/SPHERIC 6s comparison.
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
BASE="p2_two_phase_capillary"
BINARY=CORE/"build"/"upc_p2_two_phase_capillary_mac_vof_tests.exe"
NATIVE=RUNS/(BASE+"_native.txt")
BUILD=RUNS/(BASE+"_build.txt")
TARGET_CTEST=RUNS/(BASE+"_target_ctest.txt")
FULL_CTEST=RUNS/(BASE+"_full_ctest.txt")
PHASE_GATE=RUNS/(BASE+"_phase_gate.txt")
REPORT=RUNS/(BASE+"_partial.json")
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_two_phase_capillary_mac_vof3d.hpp",
    "include/upc/p2_two_phase_momentum_viscous_vof3d.hpp",
    "include/upc/p2_two_phase_mac_pressure3d.hpp",
    "include/upc/p2_two_phase_mac_vof3d.hpp",
    "tests/p2_two_phase_capillary_mac_vof3d_tests.cpp",
    "scripts/p2_two_phase_capillary_build.cmd",
    "scripts/p2_two_phase_capillary_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_TWO_PHASE_CAPILLARY_MAC_VOF_PARTIAL.md",
]
PAT_PLANAR=re.compile(r"^CAPILLARY_PLANAR grid=7x7x7 curvature=(\S+) velocity=(\S+) phase_complete=false$")
PAT_SPHERE=re.compile(
    r"^CAPILLARY_SPHERE grid=(\d+)x(\d+)x(\d+) curvature=(\S+) "
    r"analytic=(\S+) active_faces=(\d+) source_velocity=(\S+) "
    r"spurious_velocity=(\S+) energy_proxy_j=(\S+) phase_complete=false$"
)
PAT_LIVE=re.compile(r"^CAPILLARY_LIVE grid=6x6x6 effect=(\S+) divergence=(\S+) phase_complete=false$")
PAT_PASS=re.compile(r"^PASS P2_TWO_PHASE_CAPILLARY assertions=(\d+) phase_complete=false$")
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path:Path)->str:
    data=path.read_bytes()
    return data.decode("utf-16" if data.startswith((b"\xff\xfe",b"\xfe\xff")) else "utf-8").replace("\r\n","\n")
def num(s:str)->float:
    x=float(s)
    if not math.isfinite(x):
        raise ValueError("Nonfinite capillary trace")
    return x
def parse(s:str)->dict:
    planar=[];sphere={};live=[];passed=[]
    for line in s.strip().splitlines():
        if line.startswith("CAPILLARY_PLANAR "):
            m=PAT_PLANAR.fullmatch(line)
            if not m:raise ValueError("Corrupt planar trace")
            planar.append(tuple(num(x) for x in m.groups()))
        if line.startswith("CAPILLARY_SPHERE "):
            m=PAT_SPHERE.fullmatch(line)
            if not m:raise ValueError("Corrupt sphere trace")
            n,n2,n3=map(int,m.group(1,2,3))
            if n!=n2 or n!=n3 or n in sphere:
                raise ValueError("Duplicate/wrong 3D sphere grid")
            k,analytic=num(m[4]),num(m[5])
            faces=int(m[6])
            source,spurious,energy=map(num,m.group(7,8,9))
            sphere[n]={"mean_curvature_m_inv":k,"analytic_m_inv":analytic,
                       "active_faces":faces,"source_velocity_m_s":source,
                       "spurious_velocity_m_s":spurious,"energy_proxy_j":energy}
        if line.startswith("CAPILLARY_LIVE "):
            m=PAT_LIVE.fullmatch(line)
            if not m:raise ValueError("Corrupt live trace")
            live.append(tuple(num(x) for x in m.groups()))
        if line.startswith("PASS P2_TWO_PHASE_CAPILLARY "):
            m=PAT_PASS.fullmatch(line)
            if not m:raise ValueError("Corrupt PASS line")
            passed.append(int(m[1]))
    if len(planar)!=1 or planar[0]!=(0,0):
        raise ValueError("Analytic planar no-curvature equilibrium failed")
    if set(sphere)!={8,12} or len(live)!=1 or len(passed)!=1 or passed[0]<75:
        raise ValueError("Missing 3D capillary native coverage")
    target=2/.31
    for n,row in sphere.items():
        if abs(row["analytic_m_inv"]-target)>1e-12:
            raise ValueError("Sphere analytic reference substituted")
        if not (0<row["mean_curvature_m_inv"]<2*target and
                abs(row["mean_curvature_m_inv"]-target)<.75*target and
                row["active_faces"]>50 and
                1e-9<row["source_velocity_m_s"]<.1 and
                0<row["spurious_velocity_m_s"]<.1 and
                0<row["energy_proxy_j"]<1):
            raise ValueError("Spherical 3D capillary metrics invalid")
    if abs(sphere[12]["mean_curvature_m_inv"]-target)>=abs(sphere[8]["mean_curvature_m_inv"]-target):
        raise ValueError("No observed curvature improvement")
    if not (1e-11<live[0][0]<.1 and 0<=live[0][1]<1e-9):
        raise ValueError("No real live MAC/VOF capillary feedback")
    return {"planar_curvature_m_inv":planar[0][0],
            "sphere":sphere,"live_face_effect_m_s":live[0][0],
            "live_divergence_s_inv":live[0][1],
            "native_assertions":passed[0],
            "spurious_velocity_grid_converged":sphere[12]["spurious_velocity_m_s"]<
                                                sphere[8]["spurious_velocity_m_s"]}
def tamper_tests(log:str)->int:
    changes=[
        ("CAPILLARY_PLANAR grid=7x7x7","CAPILLARY_PLANAR grid=8x8x8"),
        ("curvature=0 velocity=0","curvature=NaN velocity=0"),
        ("phase_complete=false","phase_complete=true"),
        ("CAPILLARY_SPHERE grid=8x8x8","CAPILLARY_SPHERE grid=12x12x12"),
        ("analytic=6.4516129032258069","analytic=3.0"),
        ("active_faces=408","active_faces=0"),
        ("spurious_velocity=5.9607262065858177e-07","spurious_velocity=-1"),
        ("CAPILLARY_LIVE grid=6x6x6","CAPILLARY_LIVE grid=5x5x5"),
        ("assertions=75","assertions=0"),
    ]
    rejected=0
    for old,new in changes:
        if old not in log:raise ValueError("Expected tamper input missing: "+old)
        try:parse(log.replace(old,new,1))
        except (ValueError,OverflowError):rejected+=1
        else:raise ValueError("Forged capillary evidence accepted: "+old)
    return rejected
def main():
    for p in [BINARY,NATIVE,BUILD,TARGET_CTEST,FULL_CTEST,PHASE_GATE]:
        if not p.is_file():raise RuntimeError("Missing native artifact "+str(p))
    log=read(NATIVE)
    metrics=parse(log)
    rerun=subprocess.run([str(BINARY)],cwd=CORE,capture_output=True,text=True,timeout=90)
    if rerun.returncode!=0 or rerun.stdout.strip()!=log.strip():
        raise RuntimeError("Independent native capillary rerun changed/fails")
    ctest=read(FULL_CTEST)
    target=read(TARGET_CTEST)
    if "100% tests passed, 0 tests failed out of 145" not in ctest:
        raise RuntimeError("Full CTest 145/145 not confirmed")
    if "100% tests passed, 0 tests failed out of 1" not in target:
        raise RuntimeError("Capillary targeted CTest failed")
    gate=read(PHASE_GATE)
    if "GATES_INCOMPLETE=" not in gate or "P2" not in gate or "ALL_COMPLETE=False" not in gate:
        raise RuntimeError("Official P2 full-phase gate not confirmed incomplete")
    checks={
        "analytic_planar_zero_curvature":True,
        "independent_sphere_mean_curvature_reference_two_grids":True,
        "actual_3d_mac_pressure_and_live_momentum_tensor":True,
        "native_rerun_byte_equal":True,
        "full_ctest_145_of_145":True,
        "official_phase_gate_P2_open":True,
        "all_forged_traces_rejected":tamper_tests(log)==9,
    }
    report={
        "phase":"P2",
        "milestone":"diffuse_3d_capillary_mac_vof_pressure_partial",
        "partial_accepted":all(checks.values()),
        "phase_complete":False,
        "full_P2_accepted":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,
        "negative_results":{
            "spurious_velocity_grid_converged":metrics["spurious_velocity_grid_converged"],
            "spurious_velocity_8_m_s":metrics["sphere"][8]["spurious_velocity_m_s"],
            "spurious_velocity_12_m_s":metrics["sphere"][12]["spurious_velocity_m_s"],
            "sharp_PLIC_stress_validated":False,
            "MARIN_H1_H4_P1_P8_six_seconds_three_grids_pass":False,
            "bidirectional_dynamic_FSI_validated":False,
            "global_surface_energy_validated":False,
        },
        "metrics":metrics,
        "tamper_rejected":9,
        "native_binary_sha256":sha(BINARY),
        "logs_sha256":{p.name:sha(p) for p in
                       [NATIVE,BUILD,TARGET_CTEST,FULL_CTEST,PHASE_GATE]},
        "source_sha256":{p:sha(CORE/p) for p in SOURCES},
        "limitations":"CSF diffuse finite-difference curvature, not resolved PLIC jump; "
                      "spurious currents increase on tested refinement; "
                      "no 6s MARIN/SPHERIC pressure/H1-H4 grid convergence; "
                      "no moving two-way FSI or full physical surface-energy closure.",
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print("CAPILLARY_QA=PASS_PARTIAL tamper_rejected=9 native_assertions="+
          str(metrics["native_assertions"]))
    print("SPURIOUS_GRID_CONVERGED="+str(metrics["spurious_velocity_grid_converged"]))
    print("FULL_P2_ACCEPTED=False")
    print("REPORT_SHA256="+sha(REPORT))
if __name__=="__main__":
    main()
