"""P2 PARTIAL: real MSVC ABC variable-step 3D MAC/PLIC acceptance.
The analytic reference and all ledger checks are bounded; never P2 full.
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
EXE = CORE / "build" / "upc_p2_abc3d_variable_schedule_tests.exe"
NATIVE = RUNS / "p2_abc3d_variable_schedule_native.txt"
COMPILE = RUNS / "p2_abc3d_variable_schedule_compile.txt"
CTEST = RUNS / "p2_abc3d_variable_schedule_ctest.txt"
OUT = RUNS / "p2_abc3d_variable_schedule_partial.json"
SOURCES = [
    "include/upc/p2_viscous_mac_plic_roundoff_schedule3d.hpp",
    "include/upc/p2_viscous_mac_plic_roundoff_sequence3d.hpp",
    "include/upc/p2_viscous_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_nonlinear_mac_plic3d.hpp",
    "tests/p2_abc3d_variable_schedule_tests.cpp",
    "scripts/p2_abc3d_variable_schedule_build.cmd",
    "scripts/p2_abc3d_variable_schedule_acceptance.py",
    "docs/P2_ABC3D_VARIABLE_SCHEDULE_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]
def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> None:
    native = NATIVE.read_text(encoding="utf-8")
    compile_log = COMPILE.read_text(encoding="utf-8")
    ctest = CTEST.read_text(encoding="utf-8")
    checks: list[dict] = []
    def check(name: str, value, ok: bool) -> None:
        checks.append({"name": name, "value": value, "pass": bool(ok)})
    check("msvc_compile", compile_log.strip(),
          "-- Configuring done" in compile_log and "-- Generating done" in compile_log
          and ("Linking CXX executable" in compile_log or "ninja: no work to do." in compile_log)
          and "error C" not in compile_log and "fatal error" not in compile_log.lower())
    check("ctest_107", ctest.strip().splitlines()[-2:],
          "100% tests passed, 0 tests failed out of 107" in ctest)
    check("native_pass", "PASS ABC_VARIABLE" in native,
          "PASS ABC_VARIABLE" in native and "FAIL ABC_VARIABLE" not in native)
    m = re.search(r"PASS ABC_VARIABLE assertions=(\d+) phase_complete=false", native)
    assertions = int(m.group(1)) if m else 0
    check("native_assertions", assertions, assertions >= 6900)
    m = re.search(r"^ABC_VARIABLE (.+)$", native, re.M)
    if not m:
        raise ValueError("Missing ABC variable native metric record")
    metrics = {}
    for item in m.group(1).split():
        key, value = item.split("=", 1)
        metrics[key] = float(value)
    required = {
        "rel_l2", "max_abs", "energy_ratio", "analytic_energy_ratio",
        "volume_m3", "ledger_m3", "snap_abs_m3", "snap_cells",
        "divergence", "chain_diff", "legacy_diff", "viscous_substeps",
        "plic_substeps",
    }
    check("complete_native_metrics", sorted(metrics), set(metrics) == required)
    for key, value in metrics.items():
        check(f"finite_{key}", value, math.isfinite(value))
    check("abc_3d_analytic_l2", metrics["rel_l2"],
          0 < metrics["rel_l2"] < .01)
    check("abc_3d_max_error", metrics["max_abs"],
          0 < metrics["max_abs"] < .001)
    check("volume_conserved", metrics["volume_m3"],
          abs(metrics["volume_m3"] - .25) < 1e-10)
    check("snap_aware_mass_ledger", metrics["ledger_m3"],
          abs(metrics["ledger_m3"]) < 1e-10)
    check("endpoint_roundoff_bounded", metrics["snap_abs_m3"],
          0 < metrics["snap_abs_m3"] < 1e-12)
    check("endpoint_snap_exercised", metrics["snap_cells"],
          metrics["snap_cells"] >= 1)
    check("projected_divergence", metrics["divergence"],
          0 <= metrics["divergence"] < 1e-9)
    check("variable_schedule_composition", metrics["chain_diff"],
          metrics["chain_diff"] < 1e-14)
    check("legacy_constant_schedule_compatibility", metrics["legacy_diff"],
          metrics["legacy_diff"] < 1e-14)
    check("actual_viscous_substeps", metrics["viscous_substeps"],
          metrics["viscous_substeps"] >= 5)
    check("actual_plic_substeps", metrics["plic_substeps"],
          metrics["plic_substeps"] >= 5)
    nu_integral = sum(dt * nu for dt, nu in [
        (.012, .002), (.018, .003), (.020, .004),
        (.014, .0025), (.036, .0035),
    ])
    exact_energy_ratio = math.exp(-8 * math.pi**2 * nu_integral)
    check("independent_abc_energy_reference", metrics["analytic_energy_ratio"],
          math.isclose(metrics["analytic_energy_ratio"],
                       exact_energy_ratio, rel_tol=1e-12))
    check("numerical_energy_dissipation", metrics["energy_ratio"],
          0 < metrics["energy_ratio"] < metrics["analytic_energy_ratio"])
    hashes = {name: sha(CORE / name) for name in SOURCES}
    check("ten_source_fingerprints", len(hashes), len(hashes) == 10)
    check("native_binary_present", EXE.stat().st_size,
          EXE.stat().st_size > 0)
    report = {
        "phase": "P2",
        "milestone": "variable_dt_nu_abc3d_roundoff_schedule_partial",
        "partial_accepted": all(c["pass"] for c in checks),
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "native_assertions": assertions,
        "metrics": metrics,
        "checks": checks,
        "source_sha256": hashes,
        "native_binary_sha256": sha(EXE),
        "native_log_sha256": sha(NATIVE),
        "compile_log_sha256": sha(COMPILE),
        "ctest_log_sha256": sha(CTEST),
        "limitations": [
            "Analytic ABC reference, not independent measured 3D CFD",
            "Periodic constant-density model with passive VOF, not stress-consistent two-phase flow",
            "No dynamic two-way solid-fluid FSI or global physical energy conservation",
            "No UE5.8 Native-E2E of this new adapter",
            "P2-P8 remain incomplete",
        ],
    }
    previous = None
    if OUT.exists():
        try:
            previous = json.loads(OUT.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            pass
    if isinstance(previous, dict) and {
        k: v for k, v in previous.items() if k != "generated_utc"
    } == {k: v for k, v in report.items() if k != "generated_utc"}:
        report["generated_utc"] = previous["generated_utc"]
    else:
        OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                       encoding="utf-8")
    passed = sum(c["pass"] for c in checks)
    print(f"P2_ABC_VARIABLE_PARTIAL={report['partial_accepted']} "
          f"CHECKS={passed}/{len(checks)} ASSERTIONS={assertions} "
          "PHASE_COMPLETE=False")
    if not report["partial_accepted"]:
        raise SystemExit(1)

if __name__ == "__main__":
    main()
