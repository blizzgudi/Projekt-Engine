"""P2 partial: measured ABC3D temporal self-convergence on MSVC.
Never upgrades P2 full acceptance; verifies source and native evidence.
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
NATIVE = RUNS / "p2_abc3d_temporal_native.txt"
COMPILE = RUNS / "p2_abc3d_temporal_compile.txt"
CTEST = RUNS / "p2_abc3d_temporal_ctest.txt"
EXE = CORE / "build" / "upc_p2_abc3d_temporal_convergence_tests.exe"
OUT = RUNS / "p2_abc3d_temporal_partial.json"
SOURCES = [
    "include/upc/p2_viscous_mac_plic_roundoff_schedule3d.hpp",
    "include/upc/p2_viscous_mac_plic_roundoff_sequence3d.hpp",
    "include/upc/p2_viscous_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_nonlinear_mac_plic3d.hpp",
    "tests/p2_abc3d_temporal_convergence_tests.cpp",
    "scripts/p2_abc3d_temporal_build.cmd",
    "scripts/p2_abc3d_temporal_acceptance.py",
    "docs/P2_ABC3D_TEMPORAL_CONVERGENCE_PARTIAL.md",
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
    check("msvc_release_build", compile_log.strip().splitlines()[-6:],
          "-- Configuring done" in compile_log
          and "-- Generating done" in compile_log
          and ("Linking CXX executable" in compile_log
               or "ninja: no work to do." in compile_log)
          and "error C" not in compile_log
          and "fatal error" not in compile_log.lower())
    check("ctest_108", ctest.strip().splitlines()[-4:],
          "100% tests passed, 0 tests failed out of 108" in ctest)
    check("native_pass", "PASS ABC_TEMPORAL" in native,
          "PASS ABC_TEMPORAL" in native
          and "FAIL ABC_TEMPORAL" not in native)
    match = re.search(r"PASS ABC_TEMPORAL assertions=(\d+) phase_complete=false", native)
    assertions = int(match.group(1)) if match else 0
    check("native_assertions", assertions, assertions >= 20000)
    match = re.search(r"^ABC_TEMPORAL (.+)$", native, re.M)
    if not match:
        raise ValueError("Missing native ABC temporal metrics")
    metrics: dict[str, float] = {}
    for field in match.group(1).split():
        key, value = field.split("=", 1)
        metrics[key] = float(value)
    required = {
        "coarse_l2", "medium_l2", "fine_l2", "diff_5_10",
        "diff_10_20", "self_ratio", "coarse_energy", "medium_energy",
        "fine_energy", "analytic_energy", "coarse_volume", "fine_volume",
        "fine_divergence", "fine_ledger", "fine_snap",
        "fine_viscous", "fine_plic",
    }
    check("complete_metrics", sorted(metrics), set(metrics) == required)
    check("all_finite", all(math.isfinite(v) for v in metrics.values()),
          all(math.isfinite(v) for v in metrics.values()))
    check("analytic_l2_bounded", [metrics[k] for k in
          ("coarse_l2", "medium_l2", "fine_l2")],
          0 < metrics["fine_l2"] < metrics["medium_l2"]
          < metrics["coarse_l2"] < .02)
    check("measured_temporal_self_ratio", metrics["self_ratio"],
          1.7 < metrics["self_ratio"] < 2.4)
    check("pairwise_difference", [metrics["diff_5_10"],
          metrics["diff_10_20"]],
          0 < metrics["diff_10_20"] < metrics["diff_5_10"] < .001)
    check("self_ratio_consistent", metrics["self_ratio"],
          math.isclose(metrics["self_ratio"],
                       metrics["diff_5_10"]/metrics["diff_10_20"],
                       rel_tol=1e-12))
    integrated_nu = sum(dt*nu for dt,nu in [
        (.012,.002),(.018,.003),(.020,.004),(.014,.0025),(.036,.0035)])
    exact_energy = math.exp(-8*math.pi*math.pi*integrated_nu)
    check("independent_analytic_energy", metrics["analytic_energy"],
          math.isclose(metrics["analytic_energy"],exact_energy,rel_tol=1e-12))
    check("numerical_energy_decay", [metrics[k] for k in
          ("coarse_energy", "medium_energy", "fine_energy")],
          0 < metrics["coarse_energy"] < metrics["medium_energy"]
          < metrics["fine_energy"] < metrics["analytic_energy"] < 1)
    check("energy_refinement", None,
          metrics["fine_energy"]-metrics["medium_energy"]
          < metrics["medium_energy"]-metrics["coarse_energy"])
    check("water_volume", [metrics["coarse_volume"],metrics["fine_volume"]],
          abs(metrics["coarse_volume"]-.25)<1e-10
          and abs(metrics["fine_volume"]-.25)<1e-10)
    check("water_ledger", metrics["fine_ledger"],
          abs(metrics["fine_ledger"])<1e-10)
    check("roundoff_only", metrics["fine_snap"],
          0<=metrics["fine_snap"]<1e-12)
    check("divergence", metrics["fine_divergence"],
          metrics["fine_divergence"]<1e-9)
    check("20_viscous_and_plic_steps",
          [metrics["fine_viscous"],metrics["fine_plic"]],
          metrics["fine_viscous"]>=20 and metrics["fine_plic"]>=20)
    hashes = {name:sha(CORE/name) for name in SOURCES}
    check("source_fingerprints", len(hashes), len(hashes)==10)
    binary_hashes = {str(p.relative_to(CORE.parents[1])):sha(p)
                     for p in (NATIVE, COMPILE, CTEST, EXE)}
    check("native_binary_log_hashes", len(binary_hashes),
          len(binary_hashes)==4)
    accepted = all(c["pass"] for c in checks)
    report = {
        "phase":"P2", "milestone":"abc3d_temporal_self_convergence_partial",
        "partial_accepted":accepted, "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":assertions, "metrics":metrics,
        "checks":checks, "source_sha256":hashes,
        "artifact_sha256":binary_hashes,
        "limitations":[
            "fixed 12^3 grid: spatial error remains nonzero",
            "self-convergence is not external measured CFD validation",
            "constant-density passive VOF, periodic boundaries",
            "no stress-consistent free surface or dynamic two-way FSI",
            "no global physical energy/momentum or UE5.8 native E2E",
        ],
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"P2_ABC_TEMPORAL_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} ASSERTIONS={assertions}")
    print(f"EVIDENCE={OUT}")
    if not accepted:
        raise SystemExit(1)

if __name__ == "__main__":
    main()
