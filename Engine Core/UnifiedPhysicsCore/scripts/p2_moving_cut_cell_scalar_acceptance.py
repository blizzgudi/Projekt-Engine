#!/usr/bin/env python3
"""Independent bounded P2 cut-cell signed-scalar acceptance, NOT a full phase gate."""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
LOG = RUNS / "p2_moving_cut_cell_scalar_full_ctest_20261028.txt"
EXE = CORE / "build" / "upc_p2_moving_cut_cell_scalar_transport_tests.exe"
REPORT = RUNS / "p2_moving_cut_cell_scalar_partial.json"
FILES = [
    "CMakeLists.txt",
    "docs/PHASENPLAN.md",
    "docs/P2_MOVING_CUT_CELL_SIGNED_SCALAR_PARTIAL.md",
    "include/upc/p2_moving_cut_cell_scalar_transport3d.hpp",
    "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
    "include/upc/p2_moving_box_swept_wall_flux3d.hpp",
    "tests/p2_moving_cut_cell_scalar_transport3d_tests.cpp",
    "scripts/p2_moving_cut_cell_scalar_full_build.cmd",
    "scripts/p2_moving_cut_cell_scalar_acceptance.py",
]

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def overlap(cell: int, lo: float, hi: float) -> float:
    return max(0.0, min(cell + 1.0, hi) - max(float(cell), lo))

def independent_integral(n: int) -> float:
    # Geometric analytic intersection, independent of native cut-cell code.
    lo = (3.2, 3.1, 3.3)
    hi = (4.7, 4.6, 4.8)
    total = 0.0
    for z in range(n):
        for y in range(n):
            for x in range(n):
                blocked = (overlap(x, lo[0], hi[0]) *
                           overlap(y, lo[1], hi[1]) *
                           overlap(z, lo[2], hi[2]))
                alpha = 1.0 - blocked
                total += alpha * (0.25 + 0.01*x + 0.02*y - 0.015*z)
    return total

def main() -> None:
    checks = []
    def verify(name: str, good: bool, detail: object = None) -> None:
        checks.append({"name": name, "pass": bool(good), "detail": detail})
        if not good:
            raise RuntimeError(f"FAILED: {name}: {detail}")
    log = LOG.read_text(encoding="utf-8")
    verify("MSVC CTest complete 70/70",
           "100% tests passed, 0 tests failed out of 70" in log)
    verify("new native CTest present",
           "physics_p2_moving_cut_cell_signed_scalar_partial" in log)
    proc = subprocess.run([str(EXE)], capture_output=True, text=True,
                          timeout=90, check=True)
    m = re.search(r"PASS:\s+(\d+) native moving cut-cell signed scalar assertions",
                  proc.stdout)
    assertions = int(m.group(1)) if m else 0
    verify("native assertions >= 200000", assertions >= 200000, assertions)
    rows = re.findall(r"^SCALAR\s+(\d+)\s+(\d+)\s+(-?\d+)\s+([\d.eE+-]+)$",
                      proc.stdout, re.MULTILINE)
    verify("18 distinct grid-axis-direction references", len(rows) == 18, len(rows))
    seen = set()
    refs = []
    for ns, axis_s, sign_s, integral_s in rows:
        n, axis, sign = int(ns), int(axis_s), int(sign_s)
        key = (n, axis, sign)
        verify(f"unique configuration {n}/{axis}/{sign}", key not in seen)
        seen.add(key)
        expected = independent_integral(n)
        actual = float(integral_s)
        error = abs(actual - expected)
        verify(f"analytic initial integral {n}/{axis}/{sign}",
               math.isfinite(actual) and error <= 1e-9,
               {"expected": expected, "actual": actual, "abs_error": error})
        refs.append({"grid": n, "axis": axis, "direction": sign,
                     "analytic_integral_m3": expected,
                     "native_integral_m3": actual, "abs_error": error})
    verify("complete Cartesian test product",
           seen == {(n,a,s) for n in (8,12,16)
                   for a in (0,1,2) for s in (-1,1)})
    hashes = {name: sha(CORE / name) for name in FILES}
    report = {
        "phase": "P2", "partial_accepted": True, "phase_complete": False,
        "scope": "signed cell-centred passive scalar through moving projected 3D cut-cell flux",
        "native_assertions": assertions, "ctest_suites": 70,
        "checks": checks, "reference": refs,
        "source_sha256": hashes, "ctest_log_sha256": sha(LOG),
        "unproven": [
            "staggered MAC dual-volume conservative momentum transport",
            "two-way fluid-solid reaction and global energy closure",
            "free-surface stress and variable-density CFD",
            "independent measured 3D CFD validation",
            "CoupledMacWater3D and UE5.8 native E2E integration"
        ],
    }
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                      encoding="utf-8")
    print(f"P2_SCALAR_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={assertions} references={len(refs)}")
    print(f"REPORT={REPORT}")

if __name__ == "__main__":
    main()
