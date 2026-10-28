"""Independent bounded P2 cut-cell pressure/VOF partial acceptance."""
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
LOG=LAB/"runs"/"p2_cut_cell_projection_full_ctest_20261028.txt"
OUT=LAB/"runs"/"p2_cut_cell_projection_partial.json"
EXE=CORE/"build"/"upc_p2_moving_box_cut_cell_projection_tests.exe"

def check(name,actual,expected,tol):
    if not math.isfinite(actual) or abs(actual-expected)>tol:
        raise AssertionError(f"{name}: {actual} != {expected} (tol={tol})")
    return {"name":name,"actual":actual,"expected":expected,
            "tolerance":tol,"pass":True}

def main():
    log=LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 60" not in log:
        raise AssertionError("Complete 60-suite CTest regression missing")
    if "physics_p2_moving_box_cut_cell_pressure_partial" not in log:
        raise AssertionError("Native cut-cell projection suite not run")
    run=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                       text=True,check=True,timeout=120)
    checks=[]
    seen=set()
    for line in run.stdout.splitlines():
        if not line.startswith("CASE "):
            continue
        fields=line.split()
        if len(fields)!=9:
            raise AssertionError(f"Malformed native reference: {line}")
        _,n,axis,sign,shift,l1,residual,iterations,flux=fields
        n,axis,sign,iterations=map(int,(n,axis,sign,iterations))
        shift,l1,residual,flux=map(float,(shift,l1,residual,flux))
        key=(n,axis,sign,shift)
        if (n not in (8,12,16) or axis not in (0,1,2) or
            sign not in (-1,1) or shift not in (0.25,0.5) or key in seen):
            raise AssertionError(f"Duplicate/invalid case {key}")
        seen.add(key)
        # Independent geometry: translating a 1.5x1.5x1.5 m box
        # by s along one axis changes 2*area*s of open-cell volume.
        checks.append(check(f"{key}_L1",l1,2*(1.5*1.5)*shift,2e-12))
        checks.append(check(f"{key}_continuity",residual,0.0,1.1e-12))
        checks.append(check(f"{key}_iterations_nonzero",float(iterations>0),1.0,0.0))
        checks.append(check(f"{key}_flux_positive",float(flux>0),1.0,0.0))
    if len(seen)!=36 or len(checks)!=144:
        raise AssertionError(f"Incomplete 3D reference matrix: {len(seen)}")
    m=re.search(r"PASS:\s+(\d+)\s+native moving-box cut-cell pressure assertions",
                run.stdout)
    if not m or int(m.group(1))<400000:
        raise AssertionError("Native assertions missing or insufficient")
    files=[
        "include/upc/p2_moving_box_open_volume3d.hpp",
        "include/upc/p2_moving_box_mac_apertures3d.hpp",
        "include/upc/p2_moving_box_swept_wall_flux3d.hpp",
        "include/upc/p2_fractional_boundary_vof_flux3d.hpp",
        "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
        "tests/p2_moving_box_cut_cell_projection3d_tests.cpp",
        "scripts/p2_cut_cell_projection_acceptance.py",
        "docs/P2_CUT_CELL_PRESSURE_VOF_PARTIAL.md",
        "docs/PHASENPLAN.md",
        "CMakeLists.txt",
    ]
    hashes={name:hashlib.sha256((CORE/name).read_bytes()).hexdigest()
            for name in files}
    report={
        "phase":"P2",
        "milestone":"moving_box_closed_cut_cell_pressure_vof_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "accepted":False,
        "full_ctest_suites":60,
        "native_assertions":int(m.group(1)),
        "independent_numerical_checks":len(checks),
        "checks":checks,
        "native_stdout":run.stdout,
        "ctest_log_sha256":hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256":hashes,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "limitations":[
            "Only closed fully saturated initially stationary fluid, single translating box",
            "Pressure/VOF projection is opt-in, not integrated into the live MAC solver",
            "No validated post-projection FSI impulse/energy, moving-body response or measured CFD",
            "No free-surface stress model, multi-body dynamics or new UE5.8 Native-E2E",
        ],
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"PASS: {len(checks)}/{len(checks)} independent checks; "
          f"{report['native_assertions']} native assertions; "
          "60/60 CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")

if __name__=="__main__":
    main()
