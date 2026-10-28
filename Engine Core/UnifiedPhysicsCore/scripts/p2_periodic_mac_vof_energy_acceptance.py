"""P2 PARTIAL: audit periodic MAC/VOF discrete kinetic/pressure-work identity."""
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
LOG = RUNS / "p2_periodic_mac_vof_energy_full_ctest_20261028.txt"
OUT = RUNS / "p2_periodic_mac_vof_energy_partial.json"
EXE = CORE / "build" / "upc_p2_periodic_mac_vof_energy_tests.exe"
GRIDS = {(6, 7, 8), (8, 9, 10), (10, 12, 14)}
SOURCES = [
    "CMakeLists.txt",
    "docs/PHASENPLAN.md",
    "docs/P2_PERIODIC_MAC_VOF_ENERGY_PARTIAL.md",
    "include/upc/p2_periodic_mac_vof_energy3d.hpp",
    "include/upc/p2_periodic_mac_vof3d.hpp",
    "include/upc/p2_periodic_mac_projection3d.hpp",
    "include/upc/p2_variable_mac_transport3d.hpp",
    "tests/p2_periodic_mac_vof_energy3d_tests.cpp",
    "scripts/p2_periodic_mac_vof_energy_full_build.cmd",
    "scripts/p2_periodic_mac_vof_energy_acceptance.py",
]


def checked(name, actual, expected, tol):
    if not math.isfinite(actual) or abs(actual - expected) > tol:
        raise AssertionError(f"{name}: {actual} != {expected} +/- {tol}")
    return {"name": name, "actual": actual, "expected": expected,
            "tolerance": tol, "pass": True}


def main():
    log = LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 68" not in log:
        raise AssertionError("Full 68-suite regression absent")
    if "physics_p2_periodic_mac_vof_energy_partial" not in log:
        raise AssertionError("New native energy test absent from CTest")
    run = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                         text=True, timeout=180, check=True)
    lines = run.stdout.splitlines()
    seen = set()
    checks = []
    for line in lines:
        a = line.split()
        if not a or a[0] not in ("ENERGY", "GRADIENT"):
            continue
        if len(a) != (8 if a[0] == "ENERGY" else 7):
            raise AssertionError(f"Malformed native record: {line}")
        kind = a[0]
        dims = tuple(map(int, a[1:4]))
        if dims not in GRIDS or (kind, dims) in seen:
            raise AssertionError(f"Unknown/duplicate case: {line}")
        seen.add((kind, dims))
        values = list(map(float, a[4:]))
        if kind == "ENERGY":
            mass, closure, orthog, loss = values
            checks.append(checked(f"{dims} passive mass", mass,
                                  math.prod(dims) * 0.5, 1e-8))
            checks.append(checked(f"{dims} pressure identity", closure, 0, 1e-8))
            checks.append(checked(f"{dims} orthogonality", orthog, 0, 1e-8))
            if not 0 < loss < 10:
                raise AssertionError("Nonlinear pressure energy not dissipative")
        else:
            before, after, residual = values
            if not 1 < before < 200:
                raise AssertionError("Manufactured pressure gradient absent")
            checks.append(checked(f"{dims} gradient removal", after, 0, 1e-10))
            checks.append(checked(f"{dims} gradient pressure identity",
                                  residual, 0, 1e-10))
    if seen != {(k, g) for k in ("ENERGY", "GRADIENT") for g in GRIDS}:
        raise AssertionError("Not all six native 3D reference cases present")
    m = re.search(r"PASS: (\d+) periodic MAC energy native assertions", run.stdout)
    if not m or int(m.group(1)) < 47000:
        raise AssertionError("Native per-face tests/negative cases missing")
    report = {
        "phase": "P2",
        "milestone": "periodic_mac_vof_pressure_energy_ledger_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "periodic constant-density MAC/VOF kinetic pressure-work identity",
        "limits": [
            "passive VOF marker, no physical free-surface stress",
            "no moving cut cells or bidirectional rigid-body impulse",
            "no global mechanical/thermal energy conservation",
            "no measured 3D CFD or new UE5.8 Native E2E",
        ],
        "ctest_suites": 68,
        "native_assertions": int(m.group(1)),
        "reference_cases": len(seen),
        "checks": checks,
        "source_sha256": {
            f: hashlib.sha256((CORE / f).read_bytes()).hexdigest()
            for f in SOURCES
        },
        "ctest_log_sha256": hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "native_stdout_sha256": hashlib.sha256(run.stdout.encode()).hexdigest(),
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(f"PARTIAL_PASS: {len(checks)} independent checks, "
          f"{len(seen)} cases, {m.group(1)} native assertions, 68/68 CTest")
    print(f"REPORT={OUT}")
    print("PHASE_COMPLETE=False")


if __name__ == "__main__":
    main()
