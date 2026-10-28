"""Independent exact integral checks for moving-box 3D MAC apertures.

P2 bounded partial gate only. No moving-wall pressure/FSI acceptance.
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
LOG = LAB / "runs" / "p2_box_apertures_full_ctest_20261028.txt"
OUT = LAB / "runs" / "p2_moving_box_mac_apertures_partial.json"
EXE = CORE / "build" / "upc_p2_moving_box_apertures_tests.exe"


def check(name, actual, expected, tol=1e-12):
    if not math.isfinite(actual) or abs(actual - expected) > tol:
        raise ValueError(
            f"{name}: actual={actual}, expected={expected}, tol={tol}")
    return {"name": name, "actual": actual, "expected": expected,
            "tolerance": tol, "pass": True}


def main():
    log = LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 58" not in log:
        raise ValueError("Complete 58-suite CTest regression missing")
    if "physics_p2_moving_box_mac_face_apertures_partial" not in log:
        raise ValueError("Native moving-box MAC face aperture CTest missing")
    run = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                         text=True, check=True, timeout=120)
    checks = []
    seen = {"normal": set(), "tangent": set(), "diagonal": set(),
            "mixed": set(), "static": set(), "aligned": set()}
    expected = {"normal": (0.25, 0.75), "tangent": (0.5,),
                "diagonal": (35.0 / 48.0,), "mixed": (37.0 / 64.0,),
                "static": (7.0 / 16.0,), "aligned": (0.0, 0.0, 0.5)}
    for line in run.stdout.splitlines():
        row = line.split(",")
        kind = row[0]
        if kind not in expected:
            continue
        if len(row) != (4 if kind in ("normal", "tangent") else 3) + len(expected[kind]):
            raise ValueError(f"Malformed {kind} row: {row}")
        n, axis = int(row[1]), int(row[2])
        if n not in (8, 12, 16) or axis not in (0, 1, 2):
            raise ValueError(f"Unexpected grid or axis: {row}")
        if kind in ("normal", "tangent"):
            sign = int(row[3])
            if sign not in (-1, 1):
                raise ValueError(f"Unexpected direction: {row}")
            key = (n, axis, sign)
            values = row[4:]
        else:
            key = (n, axis)
            values = row[3:]
        if key in seen[kind]:
            raise ValueError(f"Duplicate reference case: {kind}/{key}")
        seen[kind].add(key)
        for j, (value, ref) in enumerate(zip(values, expected[kind])):
            checks.append(check(f"{kind}_{key}_value{j}", float(value), ref))
    for kind, expected_count in (("normal", 18), ("tangent", 18),
                                 ("diagonal", 9), ("mixed", 9),
                                 ("static", 9), ("aligned", 9)):
        if len(seen[kind]) != expected_count:
            raise ValueError(f"Incomplete {kind} reference matrix")
    if len(checks) != 108:
        raise ValueError("Independent analytic reference count mismatch")
    match = re.search(r"assertions=(\d+) phase_complete=false", run.stdout)
    if not match or int(match.group(1)) < 1000000:
        raise ValueError("Native assertion threshold or partial marker missing")
    source_files = [
        "include/upc/p2_moving_box_open_volume3d.hpp",
        "include/upc/p2_moving_box_mac_apertures3d.hpp",
        "tests/p2_moving_box_mac_apertures3d_tests.cpp",
        "scripts/p2_moving_box_mac_apertures_acceptance.py",
        "docs/P2_MOVING_BOX_MAC_APERTURES_PARTIAL.md",
        "CMakeLists.txt",
    ]
    hashes = {name: hashlib.sha256((CORE / name).read_bytes()).hexdigest()
              for name in source_files}
    report = {
        "phase": "P2",
        "milestone": "time_averaged_moving_box_mac_face_apertures_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "accepted": False,
        "full_ctest_suites": 58,
        "native_assertions": int(match.group(1)),
        "independent_numerical_checks": len(checks),
        "checks": checks,
        "native_stdout": run.stdout,
        "ctest_log_sha256": hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256": hashes,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "limitations": [
            "Only translating axis-aligned boxes, no rotation/deformation",
            "Time-averaged geometric face apertures, NOT swept MAC fluxes",
            "No moving-wall GCL, pressure, water/solid impulse or energy closure",
            "No independent 3D CFD measurements or Unreal 5.8 Native E2E",
        ],
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(f"PASS: {len(checks)} independent exact face integrals, "
          f"{report['native_assertions']} native assertions, "
          "58/58 CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")


if __name__ == "__main__":
    main()
