#!/usr/bin/env python3
"""P2 exact moving MAC dual-volume GCL: independent analytic PARTIAL gate."""
from __future__ import annotations
import hashlib
import itertools
import json
import math
import re
import subprocess
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
LOG = RUNS / "p2_moving_mac_dual_full_ctest_20261028.txt"
EXE = CORE / "build" / "upc_p2_moving_mac_dual_volumes_tests.exe"
REPORT = RUNS / "p2_moving_mac_dual_partial.json"
FILES = [
    "CMakeLists.txt", "docs/PHASENPLAN.md",
    "docs/P2_MOVING_MAC_DUAL_VOLUME_PARTIAL.md",
    "include/upc/p2_moving_mac_dual_volumes3d.hpp",
    "include/upc/p2_moving_box_open_volume3d.hpp",
    "include/upc/p2_moving_box_swept_wall_flux3d.hpp",
    "tests/p2_moving_mac_dual_volumes3d_tests.cpp",
    "scripts/p2_moving_mac_dual_full_build.cmd",
    "scripts/p2_moving_mac_dual_acceptance.py",
]

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def analytic_open(move_axis: int, sign: int, fraction: float,
                  step: int, component: int) -> float:
    lo = [3.2, 3.1, 3.3]
    hi = [4.7, 4.6, 4.8]
    delta = step * sign * fraction
    lo[move_axis] += delta
    hi[move_axis] += delta
    solid = 1.0
    for axis in range(3):
        left = 3.0 + (0.5 if axis == component else 0.0)
        solid *= max(0.0, min(left + 1.0, hi[axis]) - max(left, lo[axis]))
    return 1.0 - solid

def main() -> None:
    checks = []
    def verify(name: str, ok: bool, detail: object = None) -> None:
        checks.append({"name": name, "pass": bool(ok), "detail": detail})
        if not ok:
            raise RuntimeError(f"FAILED {name}: {detail}")
    log = LOG.read_text(encoding="utf-8")
    verify("complete MSVC CTest 71/71",
           "100% tests passed, 0 tests failed out of 71" in log)
    verify("dual-volume native suite in full CTest",
           "physics_p2_moving_mac_dual_volume_gcl_partial" in log)
    proc = subprocess.run([str(EXE)], capture_output=True, text=True,
                          timeout=120, check=True)
    m = re.search(r"PASS:\s+(\d+) native exact moving MAC dual geometry assertions",
                  proc.stdout)
    count = int(m.group(1)) if m else 0
    verify("native assertions >= 1,000,000", count >= 1_000_000, count)
    rows = re.findall(
        r"^DUAL\s+(\d+)\s+(\d+)\s+(-?\d+)\s+([\d.eE+-]+)\s+(\d+)\s+(\d+)\s+"
        r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)$",
        proc.stdout, re.MULTILINE)
    expected_keys = set(itertools.product(
        (8, 12, 16), range(3), (-1, 1), (0.25, 0.5), (0, 1), range(3)))
    verify("216 dual geometry samples", len(rows) == 216, len(rows))
    seen = set()
    references = []
    for row in rows:
        n, axis, sign, frac, step, component = (
            int(row[0]), int(row[1]), int(row[2]), float(row[3]),
            int(row[4]), int(row[5]))
        old, new, gcl, volume = map(float, row[6:])
        key = (n, axis, sign, frac, step, component)
        verify(f"unique dual case {key}", key not in seen)
        seen.add(key)
        exp_old = analytic_open(axis, sign, frac, step, component)
        exp_new = analytic_open(axis, sign, frac, step + 1, component)
        error = max(abs(old-exp_old), abs(new-exp_new))
        verify(f"independent dual overlap {key}",
               error <= 2e-12 and math.isfinite(gcl) and
               gcl <= 5e-10 and abs(volume-3.375) <= 1e-10,
               {"overlap_error": error, "gcl_m3": gcl, "solid_m3": volume})
        references.append({"grid": n, "move_axis": axis, "direction": sign,
                           "fraction": frac, "step": step,
                           "dual_component": component,
                           "old_alpha": old, "new_alpha": new,
                           "reference_old_alpha": exp_old,
                           "reference_new_alpha": exp_new,
                           "overlap_abs_error": error, "gcl_m3": gcl})
    verify("complete grid/motion/dual product", seen == expected_keys)
    hashes = {name: sha(CORE / name) for name in FILES}
    report = {
        "phase": "P2", "milestone": "exact_moving_mac_dual_geometry_gcl",
        "partial_accepted": True, "phase_complete": False,
        "native_assertions": count, "ctest_suites": 71,
        "checks": checks, "reference": references,
        "source_sha256": hashes, "ctest_log_sha256": sha(LOG),
        "unproven": [
            "conservative staggered dual momentum transport with physical boundary traction",
            "dynamic fluid-solid impulse reaction and energy conservation",
            "free-surface stress and variable-density multiphase CFD",
            "independent measured 3D CFD validation",
            "CoupledMacWater3D and UE5.8 native E2E integration"
        ],
    }
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                      encoding="utf-8")
    print(f"P2_MAC_DUAL_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} independent_references={len(references)}")
    print(f"REPORT={REPORT}")

if __name__ == "__main__":
    main()
