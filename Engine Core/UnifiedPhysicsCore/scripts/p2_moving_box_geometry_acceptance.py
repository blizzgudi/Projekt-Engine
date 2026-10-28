"""Independent P2 axis-aligned moving-box cut-cell geometry partial gate."""
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
LAB = CORE.parents[1] / "ENGINE TEST STUBE"
LOG = LAB / "runs" / "p2_box_geometry_full_ctest_20261028.txt"
OUT = LAB / "runs" / "p2_moving_box_geometry_partial.json"
EXE = CORE / "build" / "upc_p2_moving_box_geometry_tests.exe"


def check(name, actual, expected, tol=1e-11):
    if not math.isfinite(actual) or abs(actual - expected) > tol:
        raise ValueError(f"{name}: actual={actual}, expected={expected}, tol={tol}")
    return {"name": name, "actual": actual, "expected": expected,
            "tolerance": tol, "pass": True}


def main():
    log = LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 57" not in log:
        raise ValueError("Full 57-suite CTest regression missing")
    if "physics_p2_moving_box_cut_cell_geometry_partial" not in log:
        raise ValueError("Native moving-box CTest missing")
    run = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                         text=True, check=True, timeout=120)
    checks, seen = [], {"aligned": set(), "subcell": set()}
    for line in run.stdout.splitlines():
        if line.startswith("aligned,"):
            r = line.split(",")
            if len(r) != 8:
                raise ValueError(f"Malformed aligned row: {r}")
            n, axis, sign = map(int, r[1:4])
            fraction = float(r[4])
            key = (n, axis, sign, fraction)
            if (n not in (8, 12, 16) or axis not in (0, 1, 2)
                    or sign not in (-1, 1) or fraction not in (0.25, 0.5)
                    or key in seen["aligned"]):
                raise ValueError(f"Unexpected/duplicate aligned case {key}")
            seen["aligned"].add(key)
            # Independent analytic volume of a 2x1x1 cell box with dx=2 m.
            solid_volume = 2.0 * 1.0 * 1.0 * 2.0**3
            cross_section = 1.0 if axis == 0 else 2.0
            for col, expected in ((5, solid_volume),
                                  (6, 2.0 * cross_section * fraction),
                                  (7, 0.0)):
                checks.append(check(f"aligned_{key}_col{col}",
                                    float(r[col]), expected))
        elif line.startswith("subcell,"):
            r = line.split(",")
            if len(r) != 6:
                raise ValueError(f"Malformed subcell row: {r}")
            n = int(r[1])
            if n not in (8, 12, 16) or n in seen["subcell"]:
                raise ValueError(f"Unexpected/duplicate subcell grid {n}")
            seen["subcell"].add(n)
            for col, expected in ((2, 1.0 - .75 * .5 * .25),
                                  (3, 1.0 - .5 * .5 * .25),
                                  (4, 1.0 - 1.0 * .5 * .75),
                                  (5, 0.0)):
                checks.append(check(f"subcell_{n}_col{col}",
                                    float(r[col]), expected))
    if len(seen["aligned"]) != 36 or len(seen["subcell"]) != 3:
        raise ValueError("Incomplete axis/direction/subcell reference matrix")
    m = re.search(r"assertions=(\d+) phase_complete=false", run.stdout)
    if not m or int(m.group(1)) < 76000:
        raise ValueError("Native assertion threshold or partial marker missing")
    sources = [
        "include/upc/p2_moving_box_open_volume3d.hpp",
        "tests/p2_moving_box_open_volume3d_tests.cpp",
        "scripts/p2_moving_box_geometry_acceptance.py",
        "docs/P2_MOVING_BOX_CUT_CELL_GEOMETRY_PARTIAL.md",
        "CMakeLists.txt",
    ]
    hashes = {name: hashlib.sha256((CORE / name).read_bytes()).hexdigest()
              for name in sources}
    report = {
        "phase": "P2",
        "milestone": "rigid_axis_aligned_moving_box_cut_cell_geometry_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "accepted": False,
        "full_ctest_suites": 57,
        "native_assertions": int(m.group(1)),
        "independent_numerical_checks": len(checks),
        "checks": checks,
        "native_stdout": run.stdout,
        "ctest_log_sha256": hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256": hashes,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "limitations": [
            "Axis-aligned rigid box only, no rotation/deformation",
            "No physical swept MAC face fluxes or coupled pressure projection",
            "No water mass, fluid/solid impulse or global energy closure",
            "No independent 3D CFD measurement or Unreal 5.8 native E2E",
        ],
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(f"PASS: {len(checks)} independent geometry checks, "
          f"{report['native_assertions']} native assertions, "
          "57/57 CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")


if __name__ == "__main__":
    main()
