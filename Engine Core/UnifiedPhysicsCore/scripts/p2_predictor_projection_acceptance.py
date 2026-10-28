"""P2 bounded acceptance: nonzero MAC predictor through moving cut-cell projection.
Numerical references are independently derived discrete curl/gradient fields.
Never upgrades P2 to full acceptance.
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
LAB=CORE.parents[1]/"ENGINE TEST STUBE"
LOG=LAB/"runs"/"p2_predictor_full_ctest_20261028.txt"
OUT=LAB/"runs"/"p2_predictor_projection_partial.json"
EXE=CORE/"build"/"upc_p2_moving_box_predictor_projection_tests.exe"

def check(name,actual,expected,tol):
    if not math.isfinite(actual) or abs(actual-expected)>tol:
        raise AssertionError(f"{name}: {actual} != {expected} tol={tol}")
    return {"name":name,"actual":actual,"expected":expected,
            "tolerance":tol,"pass":True}

def main():
    log=LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 61" not in log:
        raise AssertionError("Full 61-suite native CTest regression not passed")
    if "physics_p2_moving_box_predicted_mac_projection_partial" not in log:
        raise AssertionError("Predicted cut-cell native CTest absent")
    run=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                       text=True,check=True,timeout=120)
    checks=[]
    seen=set()
    for line in run.stdout.splitlines():
        if not line.startswith("PRED "):
            continue
        fields=line.split()
        if len(fields)!=8:
            raise AssertionError(f"Malformed native predictor case: {line}")
        _,n,axis,sign,shift,error,pressure_delta,residual=fields
        n,axis,sign=map(int,(n,axis,sign))
        shift,error,pressure_delta,residual=map(float,
                                                (shift,error,pressure_delta,residual))
        key=(n,axis,sign,shift)
        if n not in (8,12,16) or axis not in (0,1,2) or sign not in (-1,1) or shift not in (0.25,0.5) or key in seen:
            raise AssertionError(f"Invalid/duplicate case: {key}")
        seen.add(key)
        # Discrete gradient chi=0.03 m3 on 1 m grid, dt=0.25 s,
        # rho=1000 kg/m3: pressure difference = -rho*chi/(dt^2*dx).
        checks.append(check(f"{key}_pressure_difference_pa",pressure_delta,
                            -1000.0*0.03/(0.25**2*1.0),2e-6))
        checks.append(check(f"{key}_gradient_removed_and_curl_preserved_m3",
                            error,0.0,3e-10))
        checks.append(check(f"{key}_moving_continuity_m3",
                            residual,0.0,2e-10))
        checks.append(check(f"{key}_finite_pressure",
                            float(math.isfinite(pressure_delta)),1.0,0.0))
    if len(seen)!=36 or len(checks)!=144:
        raise AssertionError("Missing 3D predicted-velocity cases")
    m=re.search(r"PASS:\s+(\d+)\s+native predicted MAC cut-cell assertions",
                run.stdout)
    if not m or int(m.group(1))<390000:
        raise AssertionError("Native moving predictor assertions insufficient")
    files=[
        "include/upc/p2_moving_box_open_volume3d.hpp",
        "include/upc/p2_moving_box_mac_apertures3d.hpp",
        "include/upc/p2_moving_box_swept_wall_flux3d.hpp",
        "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
        "include/upc/p2_fractional_boundary_vof_flux3d.hpp",
        "tests/p2_moving_box_predictor_projection3d_tests.cpp",
        "scripts/p2_predictor_projection_acceptance.py",
        "docs/P2_PREDICTED_MAC_CUT_CELL_PROJECTION_PARTIAL.md",
        "docs/PHASENPLAN.md",
        "CMakeLists.txt",
    ]
    hashes={name:hashlib.sha256((CORE/name).read_bytes()).hexdigest()
            for name in files}
    report={
        "phase":"P2",
        "milestone":"moving_box_nonzero_predicted_mac_projection_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "accepted":False,
        "full_ctest_suites":61,
        "native_assertions":int(m.group(1)),
        "independent_numerical_checks":len(checks),
        "checks":checks,
        "native_stdout":run.stdout,
        "ctest_log_sha256":hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256":hashes,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "limitations":[
            "Opt-in single moving axis-aligned box, closed fully saturated domain",
            "Predicted MAC velocities supplied externally; no time-resolved live solver integration",
            "No free-surface stress or validated dynamic two-way solid impulse and energy",
            "No independent measured 3D CFD data or new Unreal 5.8 native E2E",
        ],
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"PASS: {len(checks)}/{len(checks)} independent checks; "
          f"{report['native_assertions']} native assertions; 61/61 CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")

if __name__=="__main__":
    main()
