"""P2 PARTIAL: independently audit native periodic MAC/VOF step and CTest."""
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
LAB = PROJECT / "ENGINE TEST STUBE" / "runs"
LOG = LAB / "p2_periodic_mac_vof_full_ctest_20261028.txt"
OUT = LAB / "p2_periodic_mac_vof_partial.json"
EXE = CORE / "build" / "upc_p2_periodic_mac_vof_tests.exe"
GRIDS = {(6, 7, 8), (8, 9, 10), (10, 12, 14)}
SOURCES = [
    "CMakeLists.txt",
    "docs/PHASENPLAN.md",
    "docs/P2_PERIODIC_PROJECTED_MAC_VOF_PARTIAL.md",
    "include/upc/p2_periodic_mac_vof3d.hpp",
    "include/upc/p2_periodic_mac_projection3d.hpp",
    "include/upc/p2_variable_mac_transport3d.hpp",
    "tests/p2_periodic_mac_vof3d_tests.cpp",
    "scripts/p2_periodic_mac_vof_full_build.cmd",
    "scripts/p2_periodic_mac_vof_acceptance.py",
]


def check(name: str, actual: float, expected: float, tol: float):
    if not math.isfinite(actual) or abs(actual - expected) > tol:
        raise AssertionError(f"{name}: {actual} != {expected} +/- {tol}")
    return {"name": name, "actual": actual, "expected": expected,
            "tolerance": tol, "pass": True}


def main():
    log = LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 67" not in log:
        raise AssertionError("67-suite full CTest regression missing")
    if "physics_p2_periodic_projected_mac_vof_partial" not in log:
        raise AssertionError("New P2 periodic MAC/VOF test not in CTest")
    p = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                       text=True, check=True, timeout=180)
    lines = p.stdout.splitlines()
    cases = set()
    checks = []
    for line in lines:
        tokens = line.split()
        if tokens and tokens[0] in ("CONSTANT", "NONLINEAR"):
            mode = tokens[0]
            required = 7 if mode == "CONSTANT" else 8
            if len(tokens) != required:
                raise AssertionError(f"Malformed native output: {line}")
            nx, ny, nz = map(int, tokens[1:4])
            if (nx, ny, nz) not in GRIDS:
                raise AssertionError("Unknown reference grid")
            sign = int(tokens[4]) if mode == "CONSTANT" else 0
            if mode == "CONSTANT" and sign not in (-1, 1):
                raise AssertionError("Unknown reference flow direction")
            key = (mode, nx, ny, nz, sign)
            if key in cases:
                raise AssertionError(f"Duplicate native case {key}")
            cases.add(key)
            initial, final = map(float, tokens[5:7] if mode == "CONSTANT"
                                 else tokens[4:6])
            expected = nx * ny * nz * 0.45
            checks.append(check(f"{key} initial mass", initial, expected, 1e-8))
            checks.append(check(f"{key} final mass", final, expected, 1e-8))
            if mode == "NONLINEAR":
                divergence, cfl = map(float, tokens[6:8])
                if not (0 <= divergence < 1e-10 and 0 < cfl < 1):
                    raise AssertionError(f"Nonlinear projection/CFL: {line}")
                checks.append(check(f"{key} divergence bound", divergence, 0.0, 1e-10))
            else:
                # Native test independently checks exact 3D upwind reference
                # at every cell over four steps, both flow signs.
                pass
    expected_cases = {
        (mode, *dims, sign)
        for dims in GRIDS
        for mode, signs in (("CONSTANT", (-1, 1)), ("NONLINEAR", (0,)))
        for sign in signs
    }
    if cases != expected_cases:
        raise AssertionError(f"Missing/extra cases: {expected_cases ^ cases}")
    if "INVALID 7" not in lines:
        raise AssertionError("Negative/atomic-input tests missing")
    match = re.search(r"PASS: (\d+) periodic MAC/VOF native assertions", p.stdout)
    if not match or int(match.group(1)) < 60000:
        raise AssertionError("Native reference and bounds assertions incomplete")
    # A complete full-phase P2 acceptance is deliberately NOT claimed.
    result = {
        "phase": "P2",
        "milestone": "periodic_nonlinear_projected_mac_passive_vof_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "constant-density periodic 3D MAC nonlinear self-advection + PCG + passive bounded conservative VOF",
        "limits": [
            "no physical free-surface stress or density contrast",
            "no moving cut cells, pressure/solid impulse or FSI",
            "not integrated into general CoupledMacWater3D",
            "no independent measured 3D CFD or Unreal 5.8 native E2E for this new path",
        ],
        "ctest_suites": 67,
        "native_assertions": int(match.group(1)),
        "native_cases": len(cases),
        "checks": checks,
        "source_sha256": {
            name: hashlib.sha256((CORE / name).read_bytes()).hexdigest()
            for name in SOURCES
        },
        "ctest_log_sha256": hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "native_stdout_sha256": hashlib.sha256(p.stdout.encode()).hexdigest(),
    }
    OUT.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(f"PARTIAL_PASS: {len(checks)} independent checks, "
          f"{len(cases)} cases, {match.group(1)} native assertions, 67/67 CTest")
    print(f"REPORT={OUT}")
    print("PHASE_COMPLETE=False")


if __name__ == "__main__":
    main()
