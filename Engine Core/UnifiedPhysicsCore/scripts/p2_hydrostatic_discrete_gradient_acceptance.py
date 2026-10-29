#!/usr/bin/env python3
"""P2 PARTIAL: fixed-bath hydrostatic discrete-gradient rigid energy gate."""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
LOG = RUNS / "p2_hydrostatic_discrete_gradient_full_ctest_20261029.txt"
EXE = CORE / "build" / "upc_p2_hydrostatic_discrete_gradient_tests.exe"
REPORT = RUNS / "p2_hydrostatic_discrete_gradient_partial.json"
SOURCES = [
    "CMakeLists.txt", "docs/PHASENPLAN.md",
    "docs/P2_HYDROSTATIC_DISCRETE_GRADIENT_PARTIAL.md",
    "include/upc/p2_hydrostatic_discrete_gradient3d.hpp",
    "include/upc/p2_hydrostatic_gravity3d.hpp",
    "include/upc/p2_stratified_hydrostatic_load3d.hpp",
    "include/upc/p2_moving_box_open_volume3d.hpp",
    "include/upc/rigid_pressure_response3d.hpp",
    "tests/p2_hydrostatic_discrete_gradient3d_tests.cpp",
    "scripts/p2_hydrostatic_discrete_gradient_acceptance.py",
    "scripts/p2_hydrostatic_discrete_gradient_full_build.cmd",
]
def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def buoyancy_potential(z: float, level: float, g: float) -> float:
    # Independent exact primitive of the piecewise hydrostatic force.
    height, area = 1.6, 2.56
    depth = level - (z - 0.5 * height)
    wet = min(height, max(0.0, depth))
    return 1000.0 * g * area * (
        0.5 * wet * wet + height * max(0.0, depth - height))

def main() -> None:
    checks = []
    def check(name: str, passed: bool, detail=None) -> None:
        checks.append({"name": name, "pass": bool(passed), "detail": detail})
        if not passed:
            raise RuntimeError(f"{name} FAILED: {detail}")

    log = LOG.read_text(encoding="utf-8", errors="replace")
    check("84/84 CTest", "100% tests passed, 0 tests failed out of 84" in log)
    check("new native suite included",
          "physics_p2_hydrostatic_discrete_gradient_partial" in log)
    p = subprocess.run([str(EXE)], capture_output=True, text=True,
                       check=True, timeout=120)
    native = p.stdout
    match = re.search(
        r"PASS:\s+(\d+) native hydrostatic discrete-gradient assertions",
        native)
    count = int(match.group(1)) if match else 0
    check("native assertions >= 640", count >= 640, count)
    pattern = (r"^DG\s+(\d+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
               r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
               r"([\d.eE+-]+)$")
    rows = re.findall(pattern, native, re.MULTILINE)
    check("48 analytic configurations", len(rows) == 48, len(rows))
    seen = set()
    references = []
    for row in rows:
        n = int(row[0])
        level, g, v0, dz, f_avg, defect = map(float, row[1:])
        key = (n, round(level, 9), round(g, 9), round(v0, 9))
        check(f"unique {key}", key not in seen)
        seen.add(key)
        z0 = 4.0
        expected_force = -(buoyancy_potential(z0 + dz, level, g) -
                           buoyancy_potential(z0, level, g)) / dz
        check(f"independent potential gradient {key}",
              abs(f_avg - expected_force) < 2e-6,
              [f_avg, expected_force])
        v1 = 2 * dz / 0.01 - v0
        impulse_expected = (f_avg - 10000.0 * g) * 0.01
        check(f"momentum {key}",
              abs(10000.0 * (v1 - v0) - impulse_expected) < 2e-7)
        kinetic = 0.5 * 10000.0 * (v1*v1 - v0*v0)
        grav_potential = 10000.0 * g * dz
        buoy_potential = (buoyancy_potential(z0 + dz, level, g) -
                          buoyancy_potential(z0, level, g))
        check(f"independent body energy {key}",
              abs(kinetic + grav_potential + buoy_potential) < 1e-7)
        check(f"reported body defect {key}", abs(defect) < 1e-7, defect)
        references.append({
            "grid": n, "level_m": level, "gravity_m_s2": g,
            "initial_velocity_m_s": v0, "dz_m": dz,
            "mean_buoyancy_N": f_avg, "body_energy_defect_J": defect,
        })
    expected_keys = {
        (n, round(level, 9), round(g, 9), round(v, 9))
        for n in (8, 12)
        for level in (0.0, 3.125, 4.0, 4.35, 5.75, float(n))
        for g in (0.0, 9.81)
        for v in (-0.4, 0.4)
    }
    check("full grid/water/velocity coverage", seen == expected_keys)
    check("max energy defect < 1e-7 J",
          max(abs(r["body_energy_defect_J"]) for r in references) < 1e-7)
    hashes = {name: sha(CORE / name) for name in SOURCES}
    report = {
        "phase": "P2",
        "milestone": "hydrostatic_discrete_gradient_body_only_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "ctest_suites": 84,
        "native_assertions": count,
        "checks": checks,
        "analytic_reference": references,
        "source_sha256": hashes,
        "ctest_log_sha256": sha(LOG),
        "limitations": [
            "prescribed quiescent horizontal water level only",
            "body-only discrete mechanical energy, NOT global fluid energy",
            "fluid counterimpulse UNAPPLIED to MAC/VOF",
            "no dynamic free surface, measured 3D CFD or UE5.8 native E2E",
        ],
    }
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                      encoding="utf-8")
    print(f"P2_HYDROSTATIC_DG_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} cases={len(rows)}")
    print(f"REPORT={REPORT}")

if __name__ == "__main__":
    main()
