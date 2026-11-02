"""P2 PARTIAL: native live 3D two-phase momentum+MAC tensor stress+PCG QA.

Checks a genuinely running CPU solver, independent repeatability, genuine
full-suite CTest and falsified-record rejection. Does not accept global
Navier-Stokes, moving solid FSI or MARIN/SPHERIC full measured CFD phase.
All writes remain under D:\\Projekt Engine.
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
ROOT=CORE.parents[1]
RUNS=ROOT/"ENGINE TEST STUBE"/"runs"
BASE="p2_two_phase_full_tensor_momentum"
BINARY=CORE/"build"/"upc_p2_two_phase_full_tensor_momentum_live_tests.exe"
NATIVE=RUNS/(BASE+"_native.txt")
BUILD=RUNS/(BASE+"_build.txt")
CTEST=RUNS/(BASE+"_ctest.txt")
GATE_LOG=RUNS/(BASE+"_phase_gate.txt")
REPORT=RUNS/(BASE+"_partial.json")
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_two_phase_momentum_viscous_vof3d.hpp",
    "include/upc/p2_two_phase_full_tensor_viscous_mac3d.hpp",
    "include/upc/p2_two_phase_momentum_vof3d.hpp",
    "include/upc/p2_two_phase_mac_momentum_remap3d.hpp",
    "include/upc/p2_two_phase_mac_vof3d.hpp",
    "include/upc/p2_two_phase_mac_pressure3d.hpp",
    "tests/p2_two_phase_momentum_viscous_vof3d_tests.cpp",
    "scripts/p2_two_phase_full_tensor_momentum_build.cmd",
    "scripts/p2_two_phase_full_tensor_momentum_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_TWO_PHASE_MOMENTUM_FULL_TENSOR_LIVE_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
TRACE=re.compile(
    r"^LIVE_FULL_TENSOR grid=(\d+)x(\d+)x(\d+)"
    r" steps=(\d+)"
    r" viscosity_effect_m_s=([-+0-9.eE]+)"
    r" stress_work_j=([-+0-9.eE]+)"
    r" impulse_wall_error=([-+0-9.eE]+)"
    r" energy_substep_error=([-+0-9.eE]+)"
    r" density_error=([-+0-9.eE]+)"
    r" full_phase=false$"
)
EXPECTED={"4x4x4","6x5x4","8x6x5"}
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()
def read(path:Path)->str:
    raw=path.read_bytes()
    encoding="utf-16" if raw.startswith((b"\xff\xfe",b"\xfe\xff")) else "utf-8"
    return raw.decode(encoding,errors="replace").replace("\r\n","\n").strip()
def parse(log:str)->dict:
    rows={}
    for line in log.replace("\r\n","\n").splitlines():
        if not line.startswith("LIVE_FULL_TENSOR "):
            continue
        m=TRACE.fullmatch(line)
        if m is None:raise ValueError("Invalid full tensor live evidence row")
        x,y,z,steps=map(int,m.group(1,2,3,4))
        grid=f"{x}x{y}x{z}"
        effect,work,impulse,energy,density=map(float,m.group(5,6,7,8,9))
        if not all(map(math.isfinite,(effect,work,impulse,energy,density))):
            raise ValueError("Nonfinite live full tensor physical metrics")
        if grid in rows or steps!=6:
            raise ValueError("Missing/duplicate full 3D live tensor steps")
        if not(1e-7<effect<1 and 1e-10<work<10 and
               0<=impulse<1e-11 and 0<=energy<1e-11 and
               0<=density<1e-8):
            raise ValueError("Misreported live pressure/tensor stress budget")
        rows[grid]={
            "steps":steps,
            "measured_viscosity_face_delta_m_s":effect,
            "actual_3d_stress_work_j":work,
            "combined_tensor_and_pressure_wall_impulse_error":impulse,
            "combined_tensor_and_pressure_kinetic_error_j":energy,
            "density_error_kg_m3":density,
        }
    if set(rows)!=EXPECTED:
        raise ValueError("Native 3D full tensor live grid coverage incomplete")
    hits=re.findall(
        r"^PASS P2_TWO_PHASE_LIVE_FULL_TENSOR assertions=(\d+) "
        r"phase_complete=false$",log,re.M)
    if len(hits)!=1 or int(hits[0])<5459:
        raise ValueError("True native tensor integration assertion count missing")
    return {"rows":rows,"native_assertions":int(hits[0])}
def main():
    for p in (BINARY,NATIVE,BUILD,CTEST):
        if not p.is_file():
            raise RuntimeError("Missing native C++ proof: "+str(p))
    native=read(NATIVE)
    audit=parse(native)
    rerun=subprocess.run([str(BINARY)],cwd=CORE,capture_output=True,
                         text=True,timeout=90)
    if rerun.returncode!=0 or rerun.stderr.strip() or (
        rerun.stdout.replace("\r\n","\n").strip()!=native):
        raise RuntimeError("Real 3D full-tensor native rerun not reproducible")
    ctest=read(CTEST)
    expected_tests=[
        "physics_p2_two_phase_consistent_momentum_partial",
        "physics_p2_two_phase_live_momentum_partial",
        "physics_p2_two_phase_mac_remap_partial",
        "physics_p2_two_phase_full_tensor_mac_partial",
        "physics_p2_two_phase_full_tensor_live_partial",
        "physics_p2_two_phase_full_tensor_momentum_live_partial",
    ]
    if ("100% tests passed, 0 tests failed out of 144" not in ctest or
        any(name not in ctest for name in expected_tests)):
        raise ValueError("Complete 144-suite native tensor/VOF/PCG regression missing")
    corruptions={
        "missing_finest_grid":"\n".join(
            row for row in native.splitlines() if "grid=8x6x5" not in row),
        "false_complete":native.replace("phase_complete=false","phase_complete=true"),
        "false_full":native.replace("full_phase=false","full_phase=true"),
        "faked_no_viscous_effect":re.sub(
            r"(LIVE_FULL_TENSOR grid=4x4x4 .*?)viscosity_effect_m_s=[-+0-9.eE]+",
            r"\g<1>viscosity_effect_m_s=0",native,count=1),
        "faked_no_viscous_work":re.sub(
            r"(LIVE_FULL_TENSOR grid=6x5x4 .*?)stress_work_j=[-+0-9.eE]+",
            r"\g<1>stress_work_j=0",native,count=1),
        "nonconservative_wall":re.sub(
            r"(LIVE_FULL_TENSOR grid=8x6x5 .*?)impulse_wall_error=[-+0-9.eE]+",
            r"\g<1>impulse_wall_error=0.02",native,count=1),
        "nonconservative_kinetic":re.sub(
            r"(LIVE_FULL_TENSOR grid=8x6x5 .*?)energy_substep_error=[-+0-9.eE]+",
            r"\g<1>energy_substep_error=0.02",native,count=1),
        "unbounded_density":re.sub(
            r"(LIVE_FULL_TENSOR grid=4x4x4 .*?)density_error=[-+0-9.eE]+",
            r"\g<1>density_error=nan",native,count=1),
        "short_runtime":native.replace("steps=6","steps=5",1),
        "truncated_assertions":re.sub(
            r"assertions=\d+","assertions=1",native,count=1),
        "duplicate_data":native+"\n"+native.splitlines()[0],
    }
    rejected=[]
    for name,fake in corruptions.items():
        if fake==native:
            raise RuntimeError("Corruption was not applied: "+name)
        try:parse(fake)
        except (TypeError,ValueError):rejected.append(name)
        else:raise RuntimeError("Fabricated tensor proof accepted: "+name)
    prior=RUNS/"p2_marin_pcg_6s_partial.json"
    if not prior.is_file():
        raise RuntimeError("Independent existing negative MARIN experimental benchmark absent")
    previous=json.loads(prior.read_text(encoding="utf-8"))
    if (previous.get("phase_complete") is not False or
        previous.get("height_rmse_threshold_met") is not False or
        previous.get("pressure_experimental_validation") is not False):
        raise RuntimeError("Measured MARIN full-phase negative reference not retained")
    official=subprocess.run(
        ["py","-3",str(CORE/"scripts"/"phase_gate.py")],
        cwd=ROOT,capture_output=True,text=True,timeout=260)
    GATE_LOG.write_text(official.stdout+official.stderr,encoding="utf-8")
    if official.returncode!=0 or "ALL_COMPLETE=False" not in official.stdout or (
       "P2" not in official.stdout.split("GATES_INCOMPLETE=")[-1]):
        raise RuntimeError("Official P2 full acceptance was not independently guarded")
    report={
        "phase":"P2",
        "milestone":"true_live_two_phase_momentum_full_tensor_viscosity_three_mac_pcg",
        "partial_accepted":True,
        "physical_full_phase_complete":False,
        "phase_complete":False,
        "global_physical_fsi_energy_closed":False,
        "moving_sharp_3D_water_air_stress_validated":False,
        "external_marin_6s_full_measurement_accepted":False,
        "native_assertions":audit["native_assertions"],
        "native_ctest_suites":144,
        "independent_forged_log_rejections":rejected,
        "real_native_3D_grids":audit["rows"],
        "sources_sha256":{name:sha(CORE/name) for name in SOURCES},
        "artifact_sha256":{
            "native_executable":sha(BINARY),
            "native_trace":sha(NATIVE),
            "native_build":sha(BUILD),
            "ctest":sha(CTEST),
            "official_phase_gate":sha(GATE_LOG),
            "historical_negative_marin":sha(prior),
        },
        "still_unmet":[
            "Physical moving water-air stress jumps, sharp geometric interface and surface tension",
            "Real six-second independent experimental MARIN H1-H4/P1-P8 tolerances on three grids",
            "Dynamic two-way rigid/deformable FSI and true wall half-cell impulse accounting",
            "Coupled total physical mechanical energy/momentum with all transport sources",
            "UE5.8 native game and PIE E2E for this new exact numerical integration path",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(
        json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(f"P2_LIVE_MOMENTUM_FULL_TENSOR_PARTIAL=True NATIVE_ASSERTIONS="
          f"{audit['native_assertions']} CTEST=144 CORRUPTIONS_REJECTED="
          f"{len(rejected)} PHYSICAL_P2_COMPLETE=False")
    print("REPORT="+str(REPORT))
if __name__=="__main__":
    main()
