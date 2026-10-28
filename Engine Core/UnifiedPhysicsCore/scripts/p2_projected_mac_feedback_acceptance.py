"""Independent P2 partial gate: feedback of projected open-face MAC velocity.

Analytic water-volume reference, continuity, coverage, hashes and CTest.
Does NOT certify P2 full phase or real momentum transport.
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
LOG=LAB/"runs"/"p2_projected_mac_feedback_full_ctest_20261028.txt"
OUT=LAB/"runs"/"p2_projected_mac_feedback_partial.json"
EXE=CORE/"build"/"upc_p2_projected_mac_feedback_tests.exe"

def control(name, actual, expected, tolerance):
    if not math.isfinite(actual) or abs(actual-expected)>tolerance:
        raise AssertionError(f"{name}: {actual} != {expected} +/- {tolerance}")
    return {"name":name,"actual":actual,"expected":expected,
            "tolerance":tolerance,"pass":True}

def main():
    log=LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 63" not in log:
        raise AssertionError("Full 63-suite CTest regression missing")
    if "physics_p2_projected_mac_feedback_partial" not in log:
        raise AssertionError("New feedback CTest not in full regression")
    proc=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                        text=True,check=True,timeout=180)
    seen=set()
    checks=[]
    for line in proc.stdout.splitlines():
        if not line.startswith("FEEDBACK "):
            continue
        fields=line.split()
        if len(fields)!=10:
            raise AssertionError(f"Malformed feedback case: {line}")
        _,n,axis,sign,step,sat,mass,uniform,continuity,circulation=fields
        n,axis,sign,step=map(int,(n,axis,sign,step))
        sat,mass,uniform,continuity,circulation=map(
            float,(sat,mass,uniform,continuity,circulation))
        key=(n,axis,sign,step,sat)
        if (n not in (8,12,16) or axis not in (0,1,2) or
            sign not in (-1,1) or step not in (1,2,3) or
            sat not in (0.5,1.0) or key in seen):
            raise AssertionError(f"Missing/duplicate feedback case {key}")
        seen.add(key)
        expected=sat*(n**3-1.5**3)
        checks.append(control(f"{key}_water_m3",mass,expected,2e-9))
        checks.append(control(f"{key}_uniform_saturation",uniform,0.0,5e-10))
        checks.append(control(f"{key}_continuity_m3",continuity,0.0,2e-10))
        checks.append(control(f"{key}_nonzero_circulation",
                              float(circulation>0.01),1.0,0.0))
        checks.append(control(f"{key}_finite_circulation",
                              float(math.isfinite(circulation)),1.0,0.0))
    if len(seen)!=108 or len(checks)!=540:
        raise AssertionError("3-step / 3D-grid / axes / direction coverage incomplete")
    match=re.search(r"PASS:\s+(\d+)\s+native projected-MAC feedback assertions",
                    proc.stdout)
    if not match or int(match.group(1))<900000:
        raise AssertionError("Native MAC aperture, GCL and rollback tests absent")
    files=[
        "include/upc/p2_moving_box_open_volume3d.hpp",
        "include/upc/p2_moving_box_mac_apertures3d.hpp",
        "include/upc/p2_moving_box_swept_wall_flux3d.hpp",
        "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
        "include/upc/p2_fractional_boundary_vof_flux3d.hpp",
        "include/upc/p2_prescribed_box_mac_vof3d.hpp",
        "tests/p2_projected_mac_feedback3d_tests.cpp",
        "scripts/p2_projected_mac_feedback_acceptance.py",
        "docs/P2_PROJECTED_MAC_FEEDBACK_PARTIAL.md",
        "CMakeLists.txt",
    ]
    hashes={name:hashlib.sha256((CORE/name).read_bytes()).hexdigest()
            for name in files}
    report={
        "phase":"P2",
        "milestone":"projected_open_face_mac_feedback_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "accepted":False,
        "full_ctest_suites":63,
        "native_assertions":int(match.group(1)),
        "independent_numerical_checks":len(checks),
        "checks":checks,
        "native_stdout":proc.stdout,
        "ctest_log_sha256":hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256":hashes,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "limitations":[
            "Only prescribed translating axis-aligned box in closed 3D domain",
            "Feedback of previous projected MAC field; no momentum advection",
            "Closed-face remasking is not a momentum-conservative remap",
            "Not integrated into general CoupledMacWater3D solver",
            "No free surface, two-way FSI, global energy/impulse validation",
            "No independent measured 3D CFD or new Unreal 5.8 Native E2E",
        ],
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"PASS: {len(checks)}/{len(checks)} independent controls; "
          f"{report['native_assertions']} native assertions; "
          "63/63 CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")

if __name__=="__main__":
    main()
