"""Independent evidence QA: implicit water/air tangential shear exchange, P2 PARTIAL."""
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
PREFIX="p2_two_phase_shear_exchange"
BINARY=CORE/"build"/"upc_p2_two_phase_shear_exchange_tests.exe"
NATIVE=RUNS/(PREFIX+"_native.txt")
BUILD=RUNS/(PREFIX+"_build.txt")
CTEST=RUNS/(PREFIX+"_ctest.txt")
GATE=RUNS/(PREFIX+"_phase_gate.txt")
REPORT=RUNS/(PREFIX+"_partial.json")
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_two_phase_shear_exchange3d.hpp",
    "tests/p2_two_phase_shear_exchange3d_tests.cpp",
    "scripts/p2_two_phase_shear_exchange_build.cmd",
    "scripts/p2_two_phase_shear_exchange_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_TWO_PHASE_SHEAR_EXCHANGE_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
ZERO=re.compile(r"SHEAR_ZERO slip=([0-9.eE+-]+) impulse=([0-9.eE+-]+) energy_residual_j=([0-9.eE+-]+)")
MAR=re.compile(r"SHEAR_MARANGONI surface_impulse_y=([0-9.eE+-]+) energy_residual_j=([0-9.eE+-]+)")
PASS=re.compile(r"PASS: TWO_PHASE_SHEAR assertions=(\d+)")

def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def parse(log:str)->dict:
    lines=log.strip().splitlines()
    if len(lines)!=3:
        raise ValueError("Exactly two native cases and PASS required")
    a=ZERO.fullmatch(lines[0])
    b=MAR.fullmatch(lines[1])
    c=PASS.fullmatch(lines[2])
    if not a or not b or not c or int(c[1])<51:
        raise ValueError("Missing native tests or negative assertions")
    slip,impulse,e0=map(float,a.groups())
    surface,e1=map(float,b.groups())
    # Independent resistance and implicit two-cell momentum reference.
    rw=.04/2
    ra=.06/.5
    coupling=.2*.01/(rw+ra)
    d=1/(1+coupling*(1/2+1/1))
    i=coupling*d
    expected_surface=.2*.01*3.295
    for measured,reference,name,tol in [
        (slip,d,"implicit relative velocity",1e-13),
        (impulse,i,"equal/opposite shear impulse",1e-13),
        (surface,expected_surface,"Marangoni surface impulse",1e-13),
        (e0,0,"zero-gradient energy closure",1e-12),
        (e1,0,"surface-work energy closure",1e-12),
    ]:
        if not math.isfinite(measured) or abs(measured-reference)>tol:
            raise ValueError(f"{name} failed: {measured} vs {reference}")
    return {"slip_m_s":slip,"water_impulse_n_s":impulse,
            "surface_impulse_y_n_s":surface,
            "zero_gradient_energy_residual_j":e0,
            "marangoni_energy_residual_j":e1,
            "native_assertions":int(c[1])}

def main()->None:
    native=NATIVE.read_text(encoding="utf-8")
    values=parse(native)
    build=BUILD.read_text(encoding="utf-8")
    ctest=CTEST.read_text(encoding="utf-8")
    if ("Building CXX object" not in build or
        "upc_p2_two_phase_shear_exchange_tests.exe" not in build):
        raise ValueError("Actual MSVC native build evidence missing")
    if "100% tests passed, 0 tests failed out of 132" not in ctest:
        raise ValueError("132 native CTest suites not PASS")
    rerun=subprocess.run([str(BINARY)],capture_output=True,text=True,timeout=90)
    if rerun.returncode!=0 or rerun.stdout.strip()!=native.strip():
        raise ValueError("Independent native rerun mismatch")
    forged=[
        native.replace("slip=0.9790209790209791","slip=9",1),
        native.replace("impulse=0.01398601398601399","impulse=9",1),
        native.replace("surface_impulse_y=0.00659","surface_impulse_y=9",1),
        native.replace("energy_residual_j=-1.090707385520417e-16","energy_residual_j=1",1),
        native.replace("energy_residual_j=2.299050706755512e-16","energy_residual_j=1",1),
        native.replace("assertions=51","assertions=1",1),
    ]
    rejected=0
    for altered in forged:
        if altered==native:
            raise ValueError("Forged trace fixture did not alter original")
        try:parse(altered)
        except ValueError:rejected+=1
        else:raise ValueError("Forged native evidence accepted")
    marin=RUNS/"p2_marin_pcg_6s_partial.json"
    historical=json.loads(marin.read_text(encoding="utf-8"))
    if historical.get("height_rmse_threshold_met") is not False or historical.get("phase_complete") is not False:
        raise ValueError("Negative original MARIN 6s comparison not preserved")
    gate=subprocess.run([sys.executable,str(CORE/"scripts"/"phase_gate.py")],
                        capture_output=True,text=True,timeout=240)
    GATE.write_text(gate.stdout+gate.stderr,encoding="utf-8")
    if gate.returncode!=0 or "ALL_COMPLETE=False" not in gate.stdout or (
        "P2" not in gate.stdout.split("GATES_INCOMPLETE=",1)[-1].splitlines()[0]):
        raise ValueError("Official phase gate incorrectly promoted P2")
    report={
        "phase":"P2",
        "milestone":"implicit_two_phase_planar_tangential_shear_exchange_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "native_reference":values,
        "ctest_passed":132,
        "forged_native_traces_rejected":rejected,
        "historical_marin_6s_height_validation":"FAIL",
        "historical_marin_sha256":sha(marin),
        "limitations":[
            "Two prescribed planar finite-volume cells, not a live two-phase 3D MAC/VOF step",
            "No coupled free-surface pressure/curvature/tangential stress solver",
            "No full >=6s MARIN H1-H4/P1-P8 three-grid measured convergence",
            "No dynamic solid FSI or global multiphase energy and momentum closure",
            "No UE5.8 Native E2E for this feature",
        ],
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "binary_sha256":sha(BINARY),
        "build_log_sha256":sha(BUILD),
        "native_log_sha256":sha(NATIVE),
        "ctest_log_sha256":sha(CTEST),
        "phase_gate_log_sha256":sha(GATE),
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print(f"P2 SHEAR EXCHANGE PARTIAL PASS: {values['native_assertions']} assertions; "
          f"132/132 CTest; {rejected} forged traces rejected; P2_FULL=false")

if __name__=="__main__":
    main()
