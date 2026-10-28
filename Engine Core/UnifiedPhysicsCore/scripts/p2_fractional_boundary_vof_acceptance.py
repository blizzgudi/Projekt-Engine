"""Independent P2 subcell open-volume GCL / VOF partial acceptance."""
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
LAB = CORE.parents[1] / "ENGINE TEST STUBE"
LOG = LAB / "runs" / "p2_fractional_full_ctest_20261028.txt"
OUT = LAB / "runs" / "p2_fractional_boundary_vof_partial.json"
EXE = CORE / "build" / "upc_p2_fractional_boundary_vof_tests.exe"


def check(name, actual, expected, tolerance=1e-12):
    if not math.isfinite(actual) or abs(actual - expected) > tolerance:
        raise ValueError(f"{name}: {actual} != {expected} (tol={tolerance})")
    return {"name": name, "actual": actual, "expected": expected,
            "tolerance": tolerance, "pass": True}


def main():
    log = LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 56" not in log:
        raise ValueError("Full 56-suite native CTest regression missing")
    if "physics_p2_fractional_moving_boundary_gcl_vof_partial" not in log:
        raise ValueError("Fractional moving-boundary CTest missing")
    run = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                         text=True, check=True, timeout=120)
    rows = [line.strip().split(",") for line in run.stdout.splitlines()
            if line.startswith(("partial,", "opened,", "split,", "ring,"))]
    checks, seen = [], {"partial": set(), "opened": set(),
                        "split": set(), "ring": set()}
    for r in rows:
        typ = r[0]
        if typ in ("partial", "opened"):
            if len(r) != (14 if typ == "partial" else 8):
                raise ValueError(f"Malformed {typ} row: {r}")
            n, axis, sign = map(int, r[1:4])
            c = float(r[4])
            key = (n, axis, sign, c)
            if (n not in (8, 12, 16) or axis not in (0, 1, 2) or
                    sign not in (-1, 1) or c not in (0.25, 0.5)):
                raise ValueError(f"Unexpected fractional case {key}")
            if key in seen[typ]:
                raise ValueError(f"Duplicate fractional case {key}")
            seen[typ].add(key)
            if typ == "partial":
                expected = [
                    (5, 8.0), (6, 8.0), (7, 0.0), (8, 0.0),
                    (9, 0.8 * (1 - c)), (10, 0.2 + 0.8 * c),
                    (11, 1.0), (12, 0.0), (13, c),
                ]
            else:
                expected = [(5, 0.8 * (1 - c)), (6, 0.8 * c),
                            (7, 0.0)]
            for col, value in expected:
                checks.append(check(f"{typ}_{key}_col{col}",
                                    float(r[col]), value))
        elif typ == "split":
            if len(r) != 8:
                raise ValueError(f"Malformed split row: {r}")
            n = int(r[1])
            if n not in (8, 12, 16) or n in seen[typ]:
                raise ValueError(f"Duplicate/unexpected split {n}")
            seen[typ].add(n)
            for col, value in [(2, 0.3), (3, 0.25), (4, 0.2),
                               (5, 0.0), (6, 0.0), (7, 2.0)]:
                checks.append(check(f"split_{n}_col{col}", float(r[col]), value))
        else:
            if len(r) != 6:
                raise ValueError(f"Malformed ring row: {r}")
            n = int(r[1])
            if n not in (8, 12, 16) or n in seen[typ]:
                raise ValueError(f"Duplicate/unexpected ring {n}")
            seen[typ].add(n)
            for col, value in [(2, 0.75), (3, 0.25),
                               (4, 0.0), (5, 4.0)]:
                checks.append(check(f"ring_{n}_col{col}", float(r[col]), value))
    if {k: len(v) for k, v in seen.items()} != {
            "partial": 36, "opened": 36, "split": 3, "ring": 3}:
        raise ValueError("Incomplete 3D axis/direction/subcell reference matrix")
    match = re.search(r"assertions=(\d+) phase_complete=false", run.stdout)
    if not match or int(match.group(1)) < 200000:
        raise ValueError("Native assertions/explicit partial status missing")
    sources = [
        "include/upc/p2_fractional_boundary_vof_flux3d.hpp",
        "tests/p2_fractional_boundary_vof_flux3d_tests.cpp",
        "scripts/p2_fractional_boundary_vof_acceptance.py",
        "docs/P2_FRACTIONAL_BOUNDARY_GCL_VOF_PARTIAL.md",
        "CMakeLists.txt",
    ]
    hashes = {name: hashlib.sha256((CORE / name).read_bytes()).hexdigest()
              for name in sources}
    report = {
        "phase": "P2",
        "milestone": "fractional_subcell_moving_open_volume_gcl_vof_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "accepted": False,
        "full_ctest_suites": 56,
        "native_assertions": int(match.group(1)),
        "independent_numerical_checks": len(checks),
        "checks": checks,
        "native_stdout": run.stdout,
        "ctest_log_sha256": hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256": hashes,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "limitations": [
            "MAC face fluxes are externally prescribed, not derived from solid kinematics",
            "Fractional open-cell volumes are caller-provided, not computed from moving geometry",
            "First-order donor-cell scalar transport only, no pressure/momentum/energy closure",
            "No live CoupledMacWater3D or Unreal 5.8 native E2E integration",
            "No independent measured 3D CFD reference or complete two-way FSI",
        ],
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(f"PASS: {len(checks)} independent numerical checks, "
          f"{report['native_assertions']} native assertions, "
          "56/56 CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")


if __name__ == "__main__":
    main()
