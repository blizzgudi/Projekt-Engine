"""P2 partial gate: sequential prescribed moving-box MAC pressure + VOF.
Independent rigid box volume, 3D saturation and GCL references.
This NEVER marks the full P2 phase complete.
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
LOG=LAB/"runs"/"p2_prescribed_sequence_full_ctest_20261028.txt"
OUT=LAB/"runs"/"p2_prescribed_sequence_partial.json"
EXE=CORE/"build"/"upc_p2_prescribed_box_mac_vof_tests.exe"

def check(name,actual,expected,tolerance):
    if not math.isfinite(actual) or abs(actual-expected)>tolerance:
        raise AssertionError(f"{name}: {actual} != {expected} +/- {tolerance}")
    return {"name":name,"actual":actual,"expected":expected,
            "tolerance":tolerance,"pass":True}

def main():
    log=LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 62" not in log:
        raise AssertionError("Full 62-suite CTest regression absent")
    if "physics_p2_prescribed_box_sequential_mac_vof_partial" not in log:
        raise AssertionError("Sequential MAC/VOF CTest absent")
    proc=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                        text=True,check=True,timeout=180)
    seen=set()
    checks=[]
    for line in proc.stdout.splitlines():
        if not line.startswith("SEQ "):
            continue
        fields=line.split()
        if len(fields)!=9:
            raise AssertionError(f"Malformed sequential case: {line}")
        _,n,axis,sign,sat,step,mass,uniform,gcl=fields
        n,axis,sign,step=map(int,(n,axis,sign,step))
        sat,mass,uniform,gcl=map(float,(sat,mass,uniform,gcl))
        key=(n,axis,sign,sat,step)
        if n not in (8,12,16) or axis not in (0,1,2) or sign not in (-1,1) or sat not in (0.5,1.0) or step not in (1,2) or key in seen:
            raise AssertionError(f"Invalid/duplicate sequential case: {key}")
        seen.add(key)
        expected=sat*(n**3-(1.5**3))
        checks.append(check(f"{key}_water_volume_m3",mass,expected,2e-9))
        checks.append(check(f"{key}_uniform_saturation",uniform,0.0,5e-10))
        checks.append(check(f"{key}_local_GCL",gcl,0.0,2e-10))
        checks.append(check(f"{key}_finite_water",float(math.isfinite(mass)),1.0,0.0))
    if len(seen)!=72 or len(checks)!=288:
        raise AssertionError("Missing multi-step 3D movement/saturation cases")
    match=re.search(r"PASS:\s+(\d+)\s+native sequential cut-cell MAC/VOF assertions",
                    proc.stdout)
    if not match or int(match.group(1))<300000:
        raise AssertionError("Native rollback, water, pressure and GCL checks absent")
    files=[
        "include/upc/p2_moving_box_open_volume3d.hpp",
        "include/upc/p2_moving_box_mac_apertures3d.hpp",
        "include/upc/p2_moving_box_swept_wall_flux3d.hpp",
        "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
        "include/upc/p2_fractional_boundary_vof_flux3d.hpp",
        "include/upc/p2_prescribed_box_mac_vof3d.hpp",
        "tests/p2_prescribed_box_mac_vof3d_tests.cpp",
        "scripts/p2_prescribed_sequence_acceptance.py",
        "docs/P2_PRESCRIBED_SEQUENCE_PARTIAL.md",
        "docs/PHASENPLAN.md",
        "CMakeLists.txt",
    ]
    hashes={name:hashlib.sha256((CORE/name).read_bytes()).hexdigest()
            for name in files}
    report={
        "phase":"P2",
        "milestone":"prescribed_moving_box_multistep_mac_vof_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "accepted":False,
        "full_ctest_suites":62,
        "native_assertions":int(match.group(1)),
        "independent_numerical_checks":len(checks),
        "checks":checks,
        "native_stdout":proc.stdout,
        "ctest_log_sha256":hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256":hashes,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "limitations":[
            "Prescribed axis-aligned rigid box in closed 3D domain",
            "Externally supplied per-step MAC predictor, no feedback to live solver",
            "No free surface, advected momentum or validated two-way FSI",
            "No global physical energy/impulse balance or measured 3D CFD reference",
            "No new Unreal 5.8 native E2E for this operator",
        ],
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"PASS: {len(checks)}/{len(checks)} independent checks; "
          f"{report['native_assertions']} native assertions; "
          "62/62 CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")

if __name__=="__main__":
    main()
