"""P2 PARTIAL: independent closed moving cut-cell pressure-work audit gate."""
from __future__ import annotations

import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
PROJECT = CORE.parents[1]
RUNS = PROJECT / "ENGINE TEST STUBE" / "runs"
LOG = RUNS / "p2_moving_cut_cell_energy_full_ctest_20261028.txt"
OUT = RUNS / "p2_moving_cut_cell_energy_partial.json"
EXE = CORE / "build" / "upc_p2_moving_cut_cell_energy_tests.exe"
SOURCES = [
    "CMakeLists.txt",
    "docs/PHASENPLAN.md",
    "docs/P2_MOVING_CUT_CELL_PRESSURE_ENERGY_PARTIAL.md",
    "include/upc/p2_moving_cut_cell_energy3d.hpp",
    "include/upc/p2_prescribed_box_mac_vof3d.hpp",
    "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
    "include/upc/p2_moving_box_swept_wall_flux3d.hpp",
    "tests/p2_moving_cut_cell_energy3d_tests.cpp",
    "scripts/p2_moving_cut_cell_energy_full_build.cmd",
    "scripts/p2_moving_cut_cell_energy_acceptance.py",
    "scripts/phase_gate.py",
]


def numeric(name: str, actual: float, expected: float, tolerance: float) -> dict:
    if not math.isfinite(actual) or abs(actual - expected) > tolerance:
        raise AssertionError(
            f"{name}: {actual} != {expected} +/- {tolerance}")
    return {"name": name, "actual": actual, "expected": expected,
            "tolerance": tolerance, "pass": True}


def main() -> None:
    log = LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 69" not in log:
        raise AssertionError("Full 69-suite CTest regression missing")
    if "physics_p2_moving_cut_cell_pressure_energy_partial" not in log:
        raise AssertionError("Native cut-cell pressure-work suite absent")
    run = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                         text=True, timeout=120, check=True)
    cases = {}
    audited = set()
    checks = []
    for line in run.stdout.splitlines():
        parts = line.split()
        if not parts or parts[0] not in ("CUTENERGY", "AUDITED"):
            continue
        kind = parts[0]
        if len(parts) != (11 if kind == "CUTENERGY" else 6):
            raise AssertionError(f"Malformed native record: {line}")
        n, axis, sign = map(int, parts[1:4])
        if n not in (8, 12, 16) or axis not in (0, 1, 2) or sign not in (-1, 1):
            raise AssertionError(f"Unexpected grid/movement: {line}")
        if kind == "AUDITED":
            if (n, axis, sign) in audited:
                raise AssertionError(f"Duplicate audited case: {line}")
            audited.add((n, axis, sign))
            checks.append(numeric(f"{n}/{axis}/{sign} sequential step count",
                                  float(parts[4]), 2.0, 0))
            checks.append(numeric(f"{n}/{axis}/{sign} saturated water volume",
                                  float(parts[5]), n**3 - 1.5**3, 2e-9))
            continue
        mode = int(parts[4])
        key = (n, axis, sign, mode)
        if mode not in (0, 1) or key in cases:
            raise AssertionError(f"Unexpected/duplicate predictor case: {line}")
        pred, post, geom, correction, closure, continuity = map(float, parts[5:])
        cases[key] = (pred, post, geom, correction)
        checks.append(numeric(f"{key} moving-wall energy identity",
                              closure, 0.0, 2e-7))
        checks.append(numeric(f"{key} cut-cell continuity",
                              continuity, 0.0, 2e-10))
        checks.append(numeric(f"{key} predicted kinetic reference",
                              pred, 56.0 if mode else 0.0, 1e-9))
        if not all(map(math.isfinite, (post, geom, correction))) or (
                post <= 0 or geom <= 0 or correction <= 0):
            raise AssertionError(f"Nonpositive/nonfinite moving-box pressure energy: {key}")
        if mode == 0:
            checks.append(numeric(f"{key} zero-predictor kinetic correction",
                                  post, correction, 2e-8))
            checks.append(numeric(f"{key} independent wall pressure work",
                                  geom, 2 * correction, 2e-7))
    expected_base = {(n, axis, sign) for n in (8, 12, 16)
                     for axis in (0, 1, 2) for sign in (-1, 1)}
    if audited != expected_base or set(cases) != {
        (*base, mode) for base in expected_base for mode in (0, 1)
    }:
        raise AssertionError("Missing 36 pressure-energy / 18 audited sequential cases")
    for n, axis, sign in sorted(expected_base):
        zero = cases[(n, axis, sign, 0)]
        manufactured = cases[(n, axis, sign, 1)]
        # Independent four-face discrete curl: four faces at 0.08 m/s,
        # rho=1000 kg/m^3, dx=1 m => 0.5*1000*4*(0.08)^2 = 12.8 J.
        checks.append(numeric(f"{n}/{axis}/{sign} preserved curl energy",
                              manufactured[1] - zero[1], 12.8, 2e-7))
        checks.append(numeric(f"{n}/{axis}/{sign} gradient gauge-independent wall work",
                              manufactured[2], zero[2], 2e-7))
    match = re.search(
        r"PASS: (\d+) native moving cut-cell pressure energy assertions",
        run.stdout)
    if not match or int(match.group(1)) < 76000:
        raise AssertionError("Native per-face and rollback assertions absent")
    report = {
        "phase": "P2",
        "milestone": "moving_cut_cell_pressure_work_and_sequential_vof_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "closed constant-density moving-box 3D cut-cell pressure-work identity and opt-in sequential MAC/VOF audit",
        "limits": [
            "time-averaged MAC-face kinetic proxy, not physical total energy",
            "prescribed box trajectory, no bidirectional fluid/solid impulse",
            "no conservative cut-cell nonlinear momentum advection",
            "no stress-correct free surface or independently measured 3D CFD",
            "no integration into general CoupledMacWater3D or new UE5.8 native E2E",
        ],
        "ctest_suites": 69,
        "native_assertions": int(match.group(1)),
        "reference_cases": len(cases),
        "sequential_cases": len(audited),
        "checks": checks,
        "source_sha256": {
            path: hashlib.sha256((CORE / path).read_bytes()).hexdigest()
            for path in SOURCES
        },
        "ctest_log_sha256": hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "native_stdout_sha256": hashlib.sha256(run.stdout.encode()).hexdigest(),
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(f"PARTIAL_PASS: {len(checks)} independent checks, "
          f"{len(cases)} pressure cases, {len(audited)} sequential cases, "
          f"{match.group(1)} native assertions, 69/69 CTest")
    print(f"REPORT={OUT}")
    print("PHASE_COMPLETE=False")


if __name__ == "__main__":
    main()
