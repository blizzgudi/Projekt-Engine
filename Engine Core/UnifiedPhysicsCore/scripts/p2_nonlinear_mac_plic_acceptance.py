"""P2 PARTIAL: quantitative, independent slab and nonlinear MAC/PLIC gate.

Run only after p2_nonlinear_mac_plic_build.cmd has completed successfully.
Does not mark any P2-P8 full phase complete.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
PREFIX = "p2_nonlinear_mac_plic"
NATIVE = RUNS / (PREFIX + "_native_20261031.txt")
COMPILE = RUNS / (PREFIX + "_compile_20261031.txt")
CTEST = RUNS / (PREFIX + "_ctest_20261031.txt")
EXE = CORE / "build" / "upc_p2_nonlinear_mac_plic_tests.exe"
OUT = RUNS / (PREFIX + "_partial.json")
SOURCES = [
    "include/upc/p2_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_periodic_mac_projection3d.hpp",
    "include/upc/p2_variable_mac_transport3d.hpp",
    "include/upc/p2_plic_adaptive_subcycle3d.hpp",
    "include/upc/p2_plic_unsplit_mac3d.hpp",
    "tests/p2_nonlinear_mac_plic3d_tests.cpp",
    "scripts/p2_nonlinear_mac_plic_build.cmd",
    "scripts/p2_nonlinear_mac_plic_acceptance.py",
    "docs/P2_NONLINEAR_MAC_PLIC_PARTIAL.md",
]

def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main() -> None:
    native = NATIVE.read_text(encoding="utf-8")
    ctest = CTEST.read_text(encoding="utf-8")
    compile_log = COMPILE.read_text(encoding="utf-8")
    checks: list[dict] = []
    def check(name: str, value, ok: bool) -> None:
        checks.append({"name": name, "value": value, "pass": bool(ok)})

    match = re.search(
        r"PASS: nonlinear MAC PLIC assertions=(\d+) phase_complete=false",
        native)
    assertions = int(match.group(1)) if match else 0
    check("native_assertions", assertions, assertions >= 200000)
    check("native_no_failure", "FAIL:" not in native, "FAIL:" not in native)
    check("ctest_regression_106", "0 tests failed out of 106" in ctest,
          "100% tests passed" in ctest and
          "0 tests failed out of 106" in ctest)
    check("msvc_compiled", compile_log,
          "error C" not in compile_log and
          "fatal error" not in compile_log.lower())
    check("negative_cases", "NEGATIVE 9" in native, "NEGATIVE 9" in native)

    slabs, vortices = set(), set()
    reference = []
    for line in native.splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "VORTEX":
            if len(parts) != 8:
                raise AssertionError(f"Invalid vortex row: {line}")
            n = int(parts[1])
            key = n
            if n not in (8, 12) or key in vortices:
                raise AssertionError(f"Unexpected vortex case: {line}")
            vortices.add(key)
            before, after, mass, parity = map(float, parts[2:6])
            iterations, steps = map(int, parts[6:8])
            check(f"vortex_{n}_nonlinear_predictor", before,
                  math.isfinite(before) and before > 5e-5)
            check(f"vortex_{n}_projection", after,
                  math.isfinite(after) and abs(after) < 1e-10)
            check(f"vortex_{n}_water_mass", mass,
                  math.isfinite(mass) and abs(mass) < 1e-11)
            check(f"vortex_{n}_manual_parity", parity,
                  math.isfinite(parity) and parity < 1e-12)
            check(f"vortex_{n}_pressure_iterations", iterations,
                  iterations > 0)
            check(f"vortex_{n}_time_steps", steps, steps == 3)
            reference.append({"kind": "vortex", "grid": n,
                              "divergence_before": before,
                              "divergence_after": after,
                              "water_volume_error_m3": mass,
                              "manual_parity_error": parity,
                              "pcg_iterations": iterations})
    # Parse slabs separately: fixed eight-column schema.
    for line in native.splitlines():
        parts = line.split()
        if not parts or parts[0] != "SLAB":
            continue
        if len(parts) != 8:
            raise AssertionError(f"Invalid slab row: {line}")
        n, axis, sign = map(int, parts[1:4])
        volume, error, divergence = map(float, parts[4:7])
        steps = int(parts[7])
        key = (n, axis, sign)
        if n not in (8, 12) or axis not in (0, 1, 2) or sign not in (-1, 1) or key in slabs:
            raise AssertionError(f"Unexpected slab case: {line}")
        slabs.add(key)
        expected = 3*n*n*.1**3
        check(f"slab_{key}_analytic_volume", volume,
              math.isfinite(volume) and abs(volume-expected) < 1e-12)
        check(f"slab_{key}_analytic_translation", error,
              math.isfinite(error) and error < 2e-9)
        check(f"slab_{key}_divergence", divergence,
              math.isfinite(divergence) and abs(divergence) < 1e-11)
        check(f"slab_{key}_time_steps", steps, steps == 3)
        reference.append({"kind": "slab", "grid": n, "axis": axis,
                          "sign": sign, "volume_m3": volume,
                          "analytic_max_error": error,
                          "max_divergence_s_inv": divergence})
    check("twelve_slab_cases", len(slabs), len(slabs) == 12)
    check("two_vortex_cases", len(vortices), len(vortices) == 2)
    hashes = {name: sha(CORE / name) for name in SOURCES}
    check("source_fingerprints", len(hashes), len(hashes) == 9)
    accepted = all(c["pass"] for c in checks)
    report = {
        "phase": "P2",
        "milestone": "nonlinear_periodic_mac_momentum_projection_geometric_plic_partial",
        "partial_accepted": accepted, "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "native_assertions": assertions,
        "checks": checks, "reference": reference,
        "source_sha256": hashes,
        "native_binary_sha256": sha(EXE),
        "native_log_sha256": sha(NATIVE),
        "compile_log_sha256": sha(COMPILE),
        "ctest_log_sha256": sha(CTEST),
        "limitations": [
            "Constant-density fully periodic 3D domain; VOF is passive",
            "First-order MAC predictor and approximate geometric unsplit PLIC",
            "No two-phase free-surface stress, moving-solid FSI, or measured CFD",
            "No global physical energy/momentum or UE5.8 Native E2E",
            "P2-P8 full phases remain incomplete",
        ],
    }
    OUT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"P2_NONLINEAR_MAC_PLIC_PARTIAL={accepted} "
          f"CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        raise SystemExit(1)

if __name__ == "__main__":
    main()
