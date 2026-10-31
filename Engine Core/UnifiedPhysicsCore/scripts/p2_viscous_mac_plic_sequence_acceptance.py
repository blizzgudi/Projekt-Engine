"""P2 PARTIAL ONLY: independent multi-interval viscous MAC/PLIC acceptance."""
from __future__ import annotations
import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
PREFIX = "p2_viscous_mac_plic_sequence"
NATIVE = RUNS / (PREFIX + "_native_20261031.txt")
COMPILE = RUNS / (PREFIX + "_compile_20261031.txt")
CTEST = RUNS / (PREFIX + "_ctest_20261031.txt")
EXE = CORE / "build" / "upc_p2_viscous_mac_plic_sequence_tests.exe"
OUT = RUNS / (PREFIX + "_partial.json")
SOURCES = [
    "include/upc/p2_viscous_mac_plic_sequence3d.hpp",
    "include/upc/p2_viscous_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_plic_adaptive_subcycle3d.hpp",
    "tests/p2_viscous_mac_plic_sequence3d_tests.cpp",
    "scripts/p2_viscous_mac_plic_sequence_build.cmd",
    "scripts/p2_viscous_mac_plic_sequence_acceptance.py",
    "docs/P2_VISCOUS_MAC_PLIC_SEQUENCE_PARTIAL.md",
]

def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main() -> None:
    native = NATIVE.read_text(encoding="utf-8")
    compile_log = COMPILE.read_text(encoding="utf-8")
    ctest = CTEST.read_text(encoding="utf-8")
    checks = []
    def check(name: str, value, ok: bool) -> None:
        checks.append({"name": name, "value": value, "pass": bool(ok)})
    match = re.search(
        r"PASS: viscous MAC PLIC sequence assertions=(\d+) phase_complete=false",
        native)
    assertions = int(match.group(1)) if match else 0
    check("native_assertions", assertions, assertions >= 11000)
    check("native_no_fail", "FAIL:" not in native, "FAIL:" not in native)
    check("msvc_compile", compile_log.strip(),
          bool(compile_log.strip()) and "error C" not in compile_log and
          "fatal error" not in compile_log.lower())
    check("existing_ctest_regression",
          ctest.splitlines()[-1] if ctest else "",
          "100% tests passed" in ctest and
          "0 tests failed out of 106" in ctest)
    check("nine_invalid_inputs", "NEGATIVE 9" in native,
          "NEGATIVE 9" in native)
    eigen_seen = set()
    references = []
    for line in native.splitlines():
        p = line.split()
        if not p or p[0] != "EIGEN":
            continue
        if len(p) != 7:
            raise AssertionError(f"Invalid eigen line: {line}")
        n, sub, factor, err, plic, mass = (
            int(p[1]), int(p[2]), float(p[3]), float(p[4]),
            int(p[5]), float(p[6]))
        if n not in (8, 12) or n in eigen_seen:
            raise AssertionError(f"Unexpected eigen grid: {line}")
        eigen_seen.add(n)
        independent = 1.0
        expected_sub = 0
        for dt, nu in ((.2, .002), (.4, .004), (.15, 0.0)):
            if nu == 0:
                continue
            alpha_total = nu * dt / (.1 * .1)
            nsub = math.ceil(alpha_total * 8 - 1e-14)
            expected_sub += nsub
            independent *= (1 - 4 * (alpha_total / nsub) *
                            math.sin(math.pi/n)**2)**nsub
        check(f"eigen_{n}_viscous_substeps", sub, sub == expected_sub)
        check(f"eigen_{n}_independent_fourier_factor", factor,
              math.isfinite(factor) and abs(factor-independent) < 2e-13)
        check(f"eigen_{n}_face_error", err,
              math.isfinite(err) and err < 3e-11)
        check(f"eigen_{n}_plic_budget", plic, plic >= 3)
        check(f"eigen_{n}_water_inventory", mass,
              math.isfinite(mass) and abs(mass) < 2e-11)
        references.append({"n": n, "expected_fourier_factor": independent,
                           "observed_fourier_factor": factor,
                           "max_face_error": err, "viscous_substeps": sub})
    check("both_3d_resolutions", sorted(eigen_seen), eigen_seen == {8, 12})
    rows = [x.split() for x in native.splitlines()
            if x.startswith("COUPLED ")]
    if len(rows) != 1 or len(rows[0]) != 5:
        raise AssertionError("Missing live 3D multistep coupling")
    moved, vsub, psub, residual = map(float, rows[0][1:])
    check("coupled_water_moved", moved, math.isfinite(moved) and moved > 1e-5)
    check("coupled_viscous_budget", vsub, vsub >= 2)
    check("coupled_plic_budget", psub, psub >= 3)
    check("coupled_water_mass", residual,
          math.isfinite(residual) and abs(residual) < 1e-11)
    ledgers = [line.split() for line in native.splitlines()
               if line.startswith("LEDGER ")]
    if len(ledgers) != 1 or len(ledgers[0]) != 8:
        raise AssertionError("Missing full-sequence water ledger")
    initial_v, final_v, initial_kg, final_kg, chain, telescoping, divergence = (
        map(float, ledgers[0][1:]))
    check("ledger_initial_volume_m3", initial_v,
          math.isfinite(initial_v) and abs(initial_v-.432) < 1e-11)
    check("ledger_final_volume_m3", final_v,
          math.isfinite(final_v) and abs(final_v-.432) < 1e-11)
    check("ledger_initial_mass_kg", initial_kg,
          math.isfinite(initial_kg) and abs(initial_kg-432) < 1e-8)
    check("ledger_final_mass_kg", final_kg,
          math.isfinite(final_kg) and abs(final_kg-432) < 1e-8)
    check("ledger_interinterval_continuity_m3", chain,
          math.isfinite(chain) and abs(chain) < 1e-12)
    check("ledger_telescoping_closure_m3", telescoping,
          math.isfinite(telescoping) and abs(telescoping) < 1e-12)
    check("ledger_projected_divergence_s_inv", divergence,
          math.isfinite(divergence) and 0 <= divergence < 1e-10)
    hashes = {name: sha(CORE / name) for name in SOURCES}
    check("eight_source_hashes", len(hashes), len(hashes) == 8)
    accepted = all(c["pass"] for c in checks)
    report = {
        "phase": "P2", "milestone": "viscous_nonlinear_mac_plic_sequence_partial",
        "partial_accepted": accepted, "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "native_assertions": assertions, "checks": checks,
        "reference": references, "source_sha256": hashes,
        "native_binary_sha256": sha(EXE), "native_log_sha256": sha(NATIVE),
        "compile_log_sha256": sha(COMPILE), "ctest_log_sha256": sha(CTEST),
        "limitations": [
            "Periodic constant-density split scheme with passive VOF",
            "No stress-consistent two-phase CFD or dynamic moving-solid FSI",
            "No independent measured 3D CFD or global physical energy balance",
            "No new Unreal 5.8 Native E2E; P2-P8 remain incomplete",
        ],
    }
    OUT.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(f"P2_VISCOUS_MAC_PLIC_SEQUENCE_PARTIAL={accepted} "
          f"CHECKS={sum(x['pass'] for x in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        raise SystemExit(1)

if __name__ == "__main__":
    main()
