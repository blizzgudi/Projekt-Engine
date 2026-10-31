"""P2 PARTIAL ONLY: independent periodic viscous MAC/PLIC numeric acceptance."""
from __future__ import annotations
import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
PREFIX = "p2_viscous_nonlinear_mac_plic"
NATIVE = RUNS / (PREFIX + "_native_20261031.txt")
COMPILE = RUNS / (PREFIX + "_compile_20261031.txt")
CTEST = RUNS / (PREFIX + "_ctest_20261031.txt")
EXE = CORE / "build" / "upc_p2_viscous_nonlinear_mac_plic_tests.exe"
OUT = RUNS / (PREFIX + "_partial.json")
SOURCES = [
    "include/upc/p2_viscous_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_periodic_mac_projection3d.hpp",
    "include/upc/p2_variable_mac_transport3d.hpp",
    "include/upc/p2_plic_adaptive_subcycle3d.hpp",
    "include/upc/p2_plic_unsplit_mac3d.hpp",
    "tests/p2_viscous_nonlinear_mac_plic3d_tests.cpp",
    "scripts/p2_viscous_nonlinear_mac_plic_build.cmd",
    "scripts/p2_viscous_nonlinear_mac_plic_acceptance.py",
    "docs/P2_VISCOUS_NONLINEAR_MAC_PLIC_PARTIAL.md",
]

def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main() -> None:
    native = NATIVE.read_text(encoding="utf-8")
    compile_log = COMPILE.read_text(encoding="utf-8")
    ctest = CTEST.read_text(encoding="utf-8")
    checks: list[dict] = []
    def check(name: str, value, ok: bool) -> None:
        checks.append({"name": name, "value": value, "pass": bool(ok)})

    match = re.search(
        r"PASS: viscous nonlinear MAC PLIC assertions=(\d+) phase_complete=false",
        native)
    assertions = int(match.group(1)) if match else 0
    check("native_assertions", assertions, assertions >= 11000)
    check("native_no_failure", "FAIL:" not in native, "FAIL:" not in native)
    check("msvc_compiled", compile_log.strip(),
          "error C" not in compile_log and "fatal error" not in compile_log.lower())
    check("ctest_regression_106", ctest.splitlines()[-1] if ctest else "",
          "100% tests passed" in ctest and "0 tests failed out of 106" in ctest)
    check("zero_viscosity_bitwise_parity", "ZERO 1" in native, "ZERO 1" in native)
    check("six_invalid_cases", "NEGATIVE 6" in native, "NEGATIVE 6" in native)

    reference = []
    eigen_seen = set()
    for line in native.splitlines():
        p = line.split()
        if not p or p[0] != "EIGEN":
            continue
        if len(p) != 9:
            raise AssertionError(f"Malformed eigen row: {line}")
        n, dt, sub, err, before, after, mass, div = (
            int(p[1]), float(p[2]), int(p[3]), *map(float, p[4:]))
        if n not in (8, 12) or n in eigen_seen:
            raise AssertionError(f"Unexpected eigen case: {line}")
        eigen_seen.add(n)
        nu, dx = .002, .1
        alpha_total = nu*dt/dx**2
        expected_sub = math.ceil(alpha_total*8 - 1e-14)
        factor = (1 - (alpha_total/expected_sub)*4*
                  math.sin(math.pi/n)**2)**expected_sub
        analytic_before = n**3*(.02**2+.04**2/2)
        analytic_after = n**3*(.02**2+(.04*factor)**2/2)
        check(f"eigen_{n}_substeps", sub, sub == expected_sub)
        check(f"eigen_{n}_max_error", err, math.isfinite(err) and err < 2e-11)
        check(f"eigen_{n}_initial_face_energy", before,
              math.isfinite(before) and abs(before-analytic_before) < 1e-10)
        check(f"eigen_{n}_final_face_energy", after,
              math.isfinite(after) and abs(after-analytic_after) < 1e-10)
        check(f"eigen_{n}_dissipation", after-before,
              math.isfinite(after) and after < before)
        check(f"eigen_{n}_water_mass", mass, math.isfinite(mass) and abs(mass) < 1e-11)
        check(f"eigen_{n}_divergence", div, math.isfinite(div) and abs(div) < 1e-10)
        reference.append({"grid": n, "dt_s": dt, "substeps": sub,
                          "independent_discrete_fourier_factor": factor,
                          "measured_max_error": err,
                          "analytical_face_energy_before": analytic_before,
                          "analytical_face_energy_after": analytic_after})
    check("two_eigen_resolutions", sorted(eigen_seen), eigen_seen == {8, 12})

    coupled = [line.split() for line in native.splitlines()
               if line.startswith("COUPLED ")]
    if len(coupled) != 1 or len(coupled[0]) != 4:
        raise AssertionError("Missing coupled 3D water transport")
    moved, mass, div = map(float, coupled[0][1:])
    check("coupled_water_moved", moved, math.isfinite(moved) and moved > 1e-5)
    check("coupled_water_mass", mass, math.isfinite(mass) and abs(mass) < 1e-11)
    check("coupled_pressure_divergence", div,
          math.isfinite(div) and abs(div) < 1e-10)
    hashes = {name: sha(CORE/name) for name in SOURCES}
    check("ten_source_fingerprints", len(hashes), len(hashes) == 10)
    accepted = all(c["pass"] for c in checks)
    report = {
        "phase": "P2",
        "milestone": "viscous_periodic_nonlinear_mac_projection_plic_partial",
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
            "Periodic constant-density single-velocity model, passive water fraction",
            "Explicit split viscosity, nonlinear predictor, pressure projection, PLIC",
            "Face square-sum is a numerical proxy, not physical total energy",
            "No moving-body FSI, free-surface stress or measured 3D CFD validation",
            "No new Unreal 5.8 Native E2E; P2-P8 full acceptance incomplete",
        ],
    }
    OUT.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(f"P2_VISCOUS_NONLINEAR_MAC_PLIC_PARTIAL={accepted} "
          f"CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        raise SystemExit(1)

if __name__ == "__main__":
    main()
