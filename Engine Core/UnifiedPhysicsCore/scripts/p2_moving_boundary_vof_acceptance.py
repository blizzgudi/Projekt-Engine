"""P2 partial: independent exact-reference acceptance of moving binary-mask GCL/VOF."""
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
LAB = CORE.parents[1] / "ENGINE TEST STUBE"
LOG = LAB / "runs" / "p2_moving_boundary_vof_full_ctest_20261028.txt"
OUT = LAB / "runs" / "p2_moving_boundary_vof_partial.json"
EXE = CORE / "build" / "upc_p2_moving_boundary_vof_flux_tests.exe"


def check(name, actual, expected, tol=0.0):
    if not math.isfinite(actual) or abs(actual - expected) > tol:
        raise ValueError(f"{name}: {actual} != {expected} (tol {tol})")
    return {"name": name, "actual": actual, "expected": expected,
            "tolerance": tol, "pass": True}


def main():
    log = LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 55" not in log:
        raise ValueError("Complete 55-suite CTest proof missing")
    if "physics_p2_moving_boundary_gcl_vof_flux_partial" not in log:
        raise ValueError("New native test not included in complete CTest")
    proc = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                          text=True, timeout=120, check=True)
    rows = [line.strip().split(",") for line in proc.stdout.splitlines()
            if line.startswith(("single,", "double,", "ring,"))]
    singles = [r for r in rows if r[0] == "single"]
    doubles = [r for r in rows if r[0] == "double"]
    rings = [r for r in rows if r[0] == "ring"]
    if len(singles) != 36 or len(doubles) != 18 or len(rings) != 3:
        raise ValueError("Missing 36 single, 18 double or 3 static references")
    checks, seen_single, seen_double, seen_ring = [], set(), set(), set()
    for r in singles:
        if len(r) != 12:
            raise ValueError(f"Malformed single sweep: {r}")
        n, axis, sign, side, faces, changed = map(int, r[1:7])
        key = (n, axis, sign, side)
        if (n not in (8, 12, 16) or axis not in (0, 1, 2) or
            sign not in (-1, 1) or side not in (0, 1, 2) or side == axis):
            raise ValueError(f"Unexpected single sweep {key}")
        seen_single.add(key)
        for name, actual, expected, tol in (
            ("faces", faces, 3, 0),
            ("changed_mask", changed, 2, 0),
            ("gcl", float(r[7]), 0, 1e-13),
            ("water_residual", float(r[8]), 0, 1e-12),
            ("first_exterior", float(r[9]), 0.6, 1e-12),
            ("second_exterior", float(r[10]), 0.2, 1e-12),
            ("vacated", float(r[11]), 0.8, 1e-12),
        ):
            checks.append(check(f"single_{key}_{name}", actual, expected, tol))
    for r in doubles:
        if len(r) != 8:
            raise ValueError(f"Malformed double sweep: {r}")
        n, axis, sign, faces = map(int, r[1:5])
        key = (n, axis, sign)
        if n not in (8, 12, 16) or axis not in (0, 1, 2) or sign not in (-1, 1):
            raise ValueError(f"Unexpected double sweep {key}")
        seen_double.add(key)
        for name, actual, expected, tol in (
            ("faces", faces, 4, 0),
            ("mass", float(r[5]), 0, 1e-12),
            ("first_exterior", float(r[6]), 0.9, 1e-12),
            ("vacated", float(r[7]), 0.5, 1e-12),
        ):
            checks.append(check(f"double_{key}_{name}", actual, expected, tol))
    for r in rings:
        if len(r) != 5:
            raise ValueError(f"Malformed static ring: {r}")
        n = int(r[1])
        if n not in (8, 12, 16):
            raise ValueError("Unexpected static ring grid")
        seen_ring.add(n)
        checks.extend((
            check(f"ring_{n}_source", float(r[2]), 0.75, 1e-12),
            check(f"ring_{n}_receiver", float(r[3]), 0.25, 1e-12),
            check(f"ring_{n}_mass", float(r[4]), 0, 1e-12),
        ))
    if len(seen_single) != 36 or len(seen_double) != 18 or len(seen_ring) != 3:
        raise ValueError("Duplicate/missing numerical scenarios")
    m = re.search(r"assertions=(\d+) phase_complete=false", proc.stdout)
    if not m or int(m.group(1)) < 110000:
        raise ValueError("Native assertions and explicit partial status missing")
    sources = [
        "include/upc/p2_moving_boundary_vof_flux3d.hpp",
        "tests/p2_moving_boundary_vof_flux3d_tests.cpp",
        "scripts/p2_moving_boundary_vof_acceptance.py",
        "docs/P2_MOVING_BOUNDARY_GCL_VOF_PARTIAL.md",
        "CMakeLists.txt",
    ]
    hashes = {name: hashlib.sha256((CORE / name).read_bytes()).hexdigest()
              for name in sources}
    report = {
        "phase": "P2",
        "milestone": "binary_moving_boundary_local_gcl_vof_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "accepted": False,
        "full_ctest_suites": 55,
        "native_assertions": int(m.group(1)),
        "checks": checks,
        "native_stdout": proc.stdout,
        "ctest_log_sha256": hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256": hashes,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "limitations": [
            "MAC face volume flux is externally supplied, not computed from body kinematics",
            "Binary cell occupancy with unit-cell sweeps, no subcell geometry",
            "No pressure projection, velocity remap, fluid impulse or global energy closure",
            "Not integrated into live CoupledMacWater3D",
            "No independent measured 3D CFD reference or new Unreal 5.8 native E2E",
        ],
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(f"PASS: {len(checks)} independent numerical checks, "
          f"{report['native_assertions']} native assertions, "
          "55/55 CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")


if __name__ == "__main__":
    main()
