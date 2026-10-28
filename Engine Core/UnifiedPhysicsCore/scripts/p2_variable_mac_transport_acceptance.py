"""P2 PARTIAL: independent manufactured variable-MAC advection gate.

Only periodic, solenoidal advectors; not a CFD/FSI or Unreal full acceptance.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
LAB = CORE.parents[1] / "ENGINE TEST STUBE"
LOG = LAB / "runs" / "p2_variable_mac_full_ctest_20261028.txt"
OUT = LAB / "runs" / "p2_variable_mac_transport_partial.json"
EXE = CORE / "build" / "upc_p2_variable_mac_transport_tests.exe"
GRIDS = {(8, 10, 12), (12, 14, 16), (16, 18, 20)}
MEAN = (0.2, -0.3, 0.4)
AMP = (0.1, 0.08, -0.12)

def control(name: str, actual: float, expected: float, tol: float):
    if not math.isfinite(actual) or abs(actual - expected) > tol:
        raise AssertionError(f"{name}: actual={actual}, expected={expected}, tolerance={tol}")
    return {"name": name, "actual": actual, "expected": expected,
            "tolerance": tol, "pass": True}

def speed(c: int, j: int, ny: int, sign: int):
    def row(y):
        return sign * (0.32 + 0.12 * math.cos(2.0 * math.pi * y / ny))
    return 0.5 * (row((j-1) % ny) + row(j)) if c == 1 else row(j)

def main():
    log = LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 65" not in log:
        raise AssertionError("Full 65-suite native CTest regression missing")
    if "physics_p2_variable_mac_flux_transport_partial" not in log:
        raise AssertionError("Variable MAC native test missing from CTest log")
    proc = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                          text=True, check=True, timeout=180)
    checks = []
    samples = set()
    summaries = set()
    self_cases = set()
    for line in proc.stdout.splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "SAMPLE":
            if len(parts) != 9:
                raise AssertionError(f"Bad SAMPLE: {line}")
            nx, ny, nz, sign, step, c, j = map(int, parts[1:8])
            actual = float(parts[8]) if len(parts) == 9 else None
            # 9 tokens expected: SAMPLE + 7 indices + value.
            if actual is None:
                raise AssertionError(f"Bad SAMPLE length: {line}")
            key = (nx, ny, nz, sign, step, c, j)
            if ((nx, ny, nz) not in GRIDS or sign not in (-1, 1)
                or step not in range(1, 5) or c not in range(3)
                or j not in (0, ny//3, ny-1) or key in samples):
                raise AssertionError(f"Duplicate/invalid SAMPLE: {key}")
            samples.add(key)
            factor = 1.0 - 2.0 * 0.6 * abs(speed(c, j, ny, sign))
            expected = MEAN[c] + AMP[c] * (-1 if j % 2 else 1) * factor**step
            checks.append(control(f"shear_fourier_{key}", actual, expected, 2e-12))
        elif parts[0] == "SHEAR":
            if len(parts) != 9:
                raise AssertionError(f"Bad SHEAR: {line}")
            nx, ny, nz, sign, step = map(int, parts[1:6])
            error, mass_error, cfl = map(float, parts[6:])
            key = (nx, ny, nz, sign, step)
            if ((nx, ny, nz) not in GRIDS or sign not in (-1, 1)
                or step not in range(1, 5) or key in summaries):
                raise AssertionError(f"Duplicate/invalid SHEAR: {key}")
            summaries.add(key)
            checks.append(control(f"shear_error_{key}", error, 0.0, 2e-12))
            checks.append(control(f"shear_sum_{key}", mass_error, 0.0, 2e-9))
            checks.append(control(f"shear_cfl_{key}", cfl, 0.264, 1e-13))
        elif parts[0] == "SELF":
            if len(parts) != 7:
                raise AssertionError(f"Bad SELF: {line}")
            nx, ny, nz = map(int, parts[1:4])
            change, mass_error, divergence = map(float, parts[4:])
            key = (nx, ny, nz)
            if key not in GRIDS or key in self_cases:
                raise AssertionError(f"Duplicate/invalid SELF: {key}")
            self_cases.add(key)
            checks.append(control(f"self_nontrivial_{key}",
                                  float(1e-5 < change < 0.01), 1.0, 0.0))
            checks.append(control(f"self_integral_{key}", mass_error, 0.0, 2e-9))
            checks.append(control(f"self_divergence_{key}", divergence, 0.0, 1e-13))
    if len(samples) != 216 or len(summaries) != 24 or len(self_cases) != 3:
        raise AssertionError(f"Coverage incomplete: {len(samples)} samples, "
                             f"{len(summaries)} summaries, {len(self_cases)} self")
    if len(checks) != 297:
        raise AssertionError(f"Independent check count incorrect: {len(checks)}")
    match = re.search(r"PASS:\s+(\d+)\s+native variable MAC transport assertions",
                      proc.stdout)
    if not match or int(match.group(1)) < 200000:
        raise AssertionError("Native MAC transport assertions missing")
    files = [
        "include/upc/p2_variable_mac_transport3d.hpp",
        "tests/p2_variable_mac_transport3d_tests.cpp",
        "scripts/p2_variable_mac_transport_acceptance.py",
        "scripts/p2_variable_mac_full_build.cmd",
        "docs/P2_VARIABLE_MAC_FLUX_TRANSPORT_PARTIAL.md",
        "docs/PHASENPLAN.md",
        "CMakeLists.txt",
    ]
    hashes = {name: hashlib.sha256((CORE / name).read_bytes()).hexdigest()
              for name in files}
    report = {
        "phase": "P2",
        "milestone": "variable_solenoidal_periodic_mac_flux_transport_partial",
        "partial_accepted": True, "phase_complete": False, "accepted": False,
        "full_ctest_suites": 65,
        "native_assertions": int(match.group(1)),
        "independent_numerical_checks": len(checks),
        "checks": checks,
        "ctest_log_sha256": hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256": hashes,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "limitations": [
            "Periodic constant-density staggered MAC dual volumes only",
            "Advector must be discrete divergence-free before each step",
            "Self-advection alone does not maintain divergence-free output",
            "No coupled pressure projection, VOF free surface, cut cells, or FSI",
            "Conserved component sums are not validated total physical momentum/energy",
            "No independent measured 3D CFD or new Unreal 5.8 Native E2E",
            "P2-P8 full phases remain open",
        ],
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(f"PASS: {len(checks)}/{len(checks)} independent checks; "
          f"{report['native_assertions']} native assertions; "
          "65/65 CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")

if __name__ == "__main__":
    main()
