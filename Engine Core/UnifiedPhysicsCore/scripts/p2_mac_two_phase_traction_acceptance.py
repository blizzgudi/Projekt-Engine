"""Independent evidence QA for P2 manufactured MAC two-phase stress PARTIAL."""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
ROOT=CORE.parents[1]
RUNS=ROOT/"ENGINE TEST STUBE"/"runs"
PREFIX="p2_mac_two_phase_traction"
BINARY=CORE/"build"/"upc_p2_mac_two_phase_traction_tests.exe"
NATIVE=RUNS/(PREFIX+"_native.txt")
BUILD=RUNS/(PREFIX+"_build.txt")
CTEST=RUNS/(PREFIX+"_ctest.txt")
GATE=RUNS/(PREFIX+"_phase_gate.txt")
REPORT=RUNS/(PREFIX+"_partial.json")
SOURCES=[
    "CMakeLists.txt",
    "include/upc/mac3d.hpp",
    "include/upc/p2_sdf_capillary3d.hpp",
    "include/upc/p2_sdf_normal_stress3d.hpp",
    "include/upc/p2_mac_two_phase_traction3d.hpp",
    "tests/p2_mac_two_phase_traction3d_tests.cpp",
    "scripts/p2_mac_two_phase_traction_build.cmd",
    "scripts/p2_mac_two_phase_traction_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_MAC_TWO_PHASE_TRACTION_PARTIAL.md",
]
ROW=re.compile(r"MAC_TWO_PHASE n=(\d+) faces=(\d+) max_gradient_error=([\d.eE+-]+) max_tangential_residual_pa=([\d.eE+-]+) max_normal_pressure_error_pa=([\d.eE+-]+)")
PASS=re.compile(r"PASS: MAC_TWO_PHASE assertions=(\d+)")

def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def parse(log:str)->list[dict]:
    lines=log.strip().splitlines()
    if len(lines)!=3:
        raise ValueError("Exactly two manufactured-grid rows and native PASS required")
    rows=[]
    for line,n in zip(lines[:2],(12,24)):
        match=ROW.fullmatch(line)
        if not match:raise ValueError("Missing or forged grid evidence")
        size,faces=map(int,match.group(1,2))
        errors=list(map(float,match.group(3,4,5)))
        if size!=n or faces!=n*n or not all(math.isfinite(v) and 0<=v<1e-11 for v in errors):
            raise ValueError("3D MAC gradient, traction or normal-pressure reference failed")
        rows.append({"grid":size,"faces":faces,"max_gradient_error_s_inv":errors[0],
                     "max_tangential_residual_pa":errors[1],
                     "max_normal_pressure_error_pa":errors[2]})
    passed=PASS.fullmatch(lines[2])
    if not passed or int(passed[1])<3600:
        raise ValueError("Native negative cases or assertions absent")
    return rows

def main()->None:
    native=NATIVE.read_text(encoding="utf-8")
    rows=parse(native)
    build=BUILD.read_text(encoding="utf-8")
    ctest=CTEST.read_text(encoding="utf-8")
    if "upc_p2_mac_two_phase_traction_tests.exe" not in build or "Building CXX object" not in build:
        raise ValueError("Native MSVC target build evidence missing")
    if "100% tests passed, 0 tests failed out of 131" not in ctest:
        raise ValueError("Full native 131-suite CTest not PASS")
    rerun=subprocess.run([str(BINARY)],capture_output=True,text=True,timeout=90)
    if rerun.returncode!=0 or rerun.stdout.strip()!=native.strip():
        raise ValueError("Independent native executable rerun mismatch")
    forged=[
        native.replace("faces=144","faces=0",1),
        native.replace("n=24","n=12",1),
        native.replace("max_gradient_error=7.77156e-16","max_gradient_error=9",1),
        native.replace("max_tangential_residual_pa=1.35064e-15","max_tangential_residual_pa=9",1),
        native.replace("max_normal_pressure_error_pa=0","max_normal_pressure_error_pa=9",1),
        native.replace("assertions=3616","assertions=1",1),
    ]
    rejected=0
    for changed in forged:
        if changed==native:raise ValueError("Forged fixture did not modify native trace")
        try:parse(changed)
        except ValueError:rejected+=1
        else:raise ValueError("Forged native trace accepted")
    marin=RUNS/"p2_marin_pcg_6s_partial.json"
    historical=json.loads(marin.read_text(encoding="utf-8"))
    if historical.get("height_rmse_threshold_met") is not False or historical.get("phase_complete") is not False:
        raise ValueError("Historical 6s MARIN measurement failure missing")
    gate=subprocess.run([sys.executable,str(CORE/"scripts"/"phase_gate.py")],
                        capture_output=True,text=True,timeout=240)
    GATE.write_text(gate.stdout+gate.stderr,encoding="utf-8")
    if gate.returncode!=0 or "ALL_COMPLETE=False" not in gate.stdout or (
        "P2" not in gate.stdout.split("GATES_INCOMPLETE=",1)[-1].splitlines()[0]):
        raise ValueError("Official gate incorrectly promoted P2")
    report={
        "phase":"P2","milestone":"measured_mac_water_air_gradient_traction_partial",
        "partial_accepted":True,"phase_complete":False,
        "manufactured_reference":{"mu_water_pa_s":2.0,"mu_air_pa_s":0.5,
                                  "tangential_stress_pa":[0,3.295,-1.14],
                                  "normal_pressure_pa":120.0},
        "native_grid_rows":rows,"native_assertions":3616,"ctest_passed":131,
        "forged_native_traces_rejected":rejected,
        "historical_marin_6s_height_validation":"FAIL",
        "historical_marin_sha256":sha(marin),
        "limitations":["No dynamic air solve or MAC ghost stress feedback",
                       "No measured 6s MARIN H1-H4/P1-P8 three-grid convergence",
                       "No dynamic two-way FSI or global physical energy/momentum closure",
                       "No UE5.8 Native E2E for this path"],
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "binary_sha256":sha(BINARY),"build_log_sha256":sha(BUILD),
        "native_log_sha256":sha(NATIVE),"ctest_log_sha256":sha(CTEST),
        "phase_gate_log_sha256":sha(GATE),
        "generated_utc":datetime.now(timezone.utc).isoformat()
    }
    REPORT.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    print(f"P2 MAC TWO PHASE PARTIAL PASS: 3616 assertions; 131/131 CTest; {rejected} forged traces rejected; P2_FULL=false")

if __name__=="__main__":
    main()
