"""Independent analytic acceptance for P2 translating-box swept solid MAC GCL.

Only a bounded geometry partial. Never claim a complete fluid/FSI phase.
"""
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
LAB = CORE.parents[1] / "ENGINE TEST STUBE"
LOG = LAB / "runs" / "p2_swept_wall_full_ctest_20261028.txt"
OUT = LAB / "runs" / "p2_moving_box_swept_wall_partial.json"
EXE = CORE / "build" / "upc_p2_moving_box_swept_wall_flux_tests.exe"


def compare(name, actual, expected, tolerance=2e-12):
    if not math.isfinite(actual) or abs(actual - expected) > tolerance:
        raise ValueError(
            f"{name}: actual={actual}, expected={expected}, tol={tolerance}")
    return {"name": name, "actual": actual, "expected": expected,
            "tolerance": tolerance, "pass": True}


def main():
    log = LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 59" not in log:
        raise ValueError("59-suite CTest regression missing")
    if "physics_p2_moving_box_swept_wall_gcl_partial" not in log:
        raise ValueError("New native swept-wall CTest missing")
    run = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                         text=True, check=True, timeout=120)
    seen = {"axis": set(), "diagonal": set()}
    checks = []
    for line in run.stdout.splitlines():
        fields = line.split(",")
        kind = fields[0]
        if kind == "axis":
            if len(fields) != 7:
                raise ValueError(f"Malformed axis line: {line}")
            n, axis, sign = map(int, fields[1:4])
            key = (n, axis, sign)
            if n not in (8, 12, 16) or axis not in (0, 1, 2) or sign not in (-1, 1):
                raise ValueError(f"Invalid axis reference key: {key}")
            if key in seen[kind]:
                raise ValueError(f"Duplicate axis key: {key}")
            seen[kind].add(key)
            checks.extend([
                compare(f"axis_{key}_near", float(fields[4]), sign * 0.75),
                compare(f"axis_{key}_far", float(fields[5]), sign * 0.25),
                compare(f"axis_{key}_gcl", float(fields[6]), 0.0),
            ])
        elif kind == "diagonal":
            if len(fields) != 5:
                raise ValueError(f"Malformed diagonal line: {line}")
            n, axis = map(int, fields[1:3])
            key = (n, axis)
            if n not in (8, 12, 16) or axis not in (0, 1, 2):
                raise ValueError(f"Invalid diagonal key: {key}")
            if key in seen[kind]:
                raise ValueError(f"Duplicate diagonal key: {key}")
            seen[kind].add(key)
            checks.extend([
                compare(f"diagonal_{key}_polynomial",
                        float(fields[3]), 45.0 / 128.0),
                compare(f"diagonal_{key}_gcl",
                        float(fields[4]), 0.0),
            ])
    if len(seen["axis"]) != 18 or len(seen["diagonal"]) != 9:
        raise ValueError("Incomplete reference matrix")
    if len(checks) != 72:
        raise ValueError("Independent reference check count mismatch")
    match = re.search(
        r"assertions=(\d+) phase_complete=false", run.stdout)
    if not match or int(match.group(1)) < 100000:
        raise ValueError("Native assertion threshold or partial marker missing")
    files = [
        "include/upc/p2_moving_box_open_volume3d.hpp",
        "include/upc/p2_moving_box_mac_apertures3d.hpp",
        "include/upc/p2_moving_box_swept_wall_flux3d.hpp",
        "tests/p2_moving_box_swept_wall_flux3d_tests.cpp",
        "scripts/p2_moving_box_swept_wall_acceptance.py",
        "docs/P2_MOVING_BOX_SWEPT_WALL_GCL_PARTIAL.md",
        "docs/PHASENPLAN.md",
        "CMakeLists.txt",
    ]
    hashes = {name: hashlib.sha256((CORE / name).read_bytes()).hexdigest()
              for name in files}
    report = {
        "phase": "P2",
        "milestone": "translating_box_exact_swept_solid_mac_gcl_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "accepted": False,
        "full_ctest_suites": 59,
        "native_assertions": int(match.group(1)),
        "independent_numerical_checks": len(checks),
        "checks": checks,
        "native_stdout": run.stdout,
        "ctest_log_sha256": hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256": hashes,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "limitations": [
            "Signed swept solid volume is a geometric source, not fluid MAC flux",
            "No cut-cell pressure projection, VOF positivity or dynamic FSI",
            "Axis-aligned linear translation only, no rotation/deformation",
            "No independent measured 3D CFD or new UE5.8 Native-E2E",
        ],
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(f"PASS: {len(checks)} independent analytic checks; "
          f"{report['native_assertions']} native assertions; "
          "59/59 CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")


if __name__ == "__main__":
    main()
