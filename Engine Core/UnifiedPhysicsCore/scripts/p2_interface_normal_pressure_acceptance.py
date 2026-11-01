"""Independent P2 normal-interface-pressure PARTIAL QA, never full acceptance."""
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
NATIVE = RUNS / "p2_interface_normal_pressure_native.txt"
CTEST = RUNS / "p2_interface_normal_pressure_ctest.txt"
BUILD = RUNS / "p2_interface_normal_pressure_build.txt"
BINARY = CORE / "build" / "upc_p2_interface_normal_pressure_tests.exe"
GATE_LOG = RUNS / "p2_interface_normal_pressure_phase_gate.txt"
GATE_JSON = RUNS / "unified_physics_gate.json"
MARIN = RUNS / "p2_marin_pcg_6s_partial.json"
REPORT = RUNS / "p2_interface_normal_pressure_partial.json"
SOURCES = (
    "CMakeLists.txt",
    "include/upc/mac3d.hpp",
    "include/upc/coupled_water3d.hpp",
    "tests/p2_interface_normal_pressure3d_tests.cpp",
    "scripts/p2_interface_normal_pressure_build.cmd",
    "scripts/p2_interface_normal_pressure_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_INTERFACE_NORMAL_PRESSURE_PARTIAL.md",
    "docs/PHASENPLAN.md",
)
ISOLATED = re.compile(
    r"NORMAL_PRESSURE_ISOLATED expected_pa=([0-9.eE+\-]+) "
    r"divergence_s_inv=([0-9.eE+\-]+) iterations=(\d+)"
)
LIVE = re.compile(
    r"NORMAL_PRESSURE_LIVE mass_kg=([0-9.eE+\-]+) "
    r"divergence_default=([0-9.eE+\-]+) "
    r"divergence_shifted=([0-9.eE+\-]+)"
)
PASS = "PASS P2_INTERFACE_NORMAL_PRESSURE assertions=4785 phase_complete=false"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_native(log: str) -> dict:
    lines = log.strip().splitlines()
    if len(lines) != 3 or lines[-1] != PASS:
        raise ValueError("Native PASS, exact assertions or partial marker missing")
    a, b = ISOLATED.fullmatch(lines[0]), LIVE.fullmatch(lines[1])
    if not a or not b:
        raise ValueError("Missing native analytic or coupled-fluid row")
    pressure, divergence, iterations = float(a[1]), float(a[2]), int(a[3])
    theta = (.25, .5, .75, .25, .5, .75)
    boundary = (100, 400, 800, -200, 500, -100)
    reference = sum(p / t for p, t in zip(boundary, theta)) / sum(1 / t for t in theta)
    if not math.isfinite(pressure) or abs(pressure - reference) > 0.0005:
        raise ValueError("Manufactured 6-face finite-volume pressure invalid")
    if iterations != 1 or not math.isfinite(divergence) or abs(divergence) > 1e-11:
        raise ValueError("Single-cell PCG residual or divergence invalid")
    mass, d0, d1 = map(float, b.groups())
    if abs(mass - 144) > 1e-10 or not all(
        math.isfinite(d) and 0 <= d < 1e-7 for d in (d0, d1)
    ):
        raise ValueError("Live water mass or divergence invalid")
    return {
        "independent_six_face_pressure_reference_pa": reference,
        "native_pressure_pa_rounded": pressure,
        "single_cell_pcg_iterations": iterations,
        "single_cell_divergence_s_inv": divergence,
        "live_water_mass_kg": mass,
        "live_divergence_default_s_inv": d0,
        "live_divergence_shifted_s_inv": d1,
    }


def deliberate_corruption_rejections(log: str) -> int:
    changes = (
        ("assertions=4785", "assertions=4784"),
        ("phase_complete=false", "phase_complete=true"),
        ("expected_pa=159.091", "expected_pa=500.091"),
        ("iterations=1", "iterations=0"),
        ("divergence_shifted=1.29621e-10", "divergence_shifted=0.01"),
        ("mass_kg=144", "mass_kg=140"),
    )
    for old, new in changes:
        if old not in log:
            raise ValueError("QA corruption fixture no longer matches native log")
        modified = log.replace(old, new, 1)
        try:
            verify_native(modified)
        except ValueError:
            continue
        raise ValueError("Corrupted native evidence accepted: " + old)
    return len(changes)


def main() -> None:
    log = NATIVE.read_text(encoding="utf-8")
    metrics = verify_native(log)
    rejections = deliberate_corruption_rejections(log)
    rerun = subprocess.run(
        [str(BINARY)], cwd=CORE, capture_output=True, text=True, timeout=60
    )
    if rerun.returncode or rerun.stdout.strip() != log.strip() or rerun.stderr:
        raise ValueError("Independent MSVC native re-execution mismatch")
    build = BUILD.read_text(encoding="utf-8", errors="replace")
    ctest = CTEST.read_text(encoding="utf-8", errors="replace")
    if ("Build files have been written" not in build
        or "100% tests passed, 0 tests failed out of 127" not in ctest
        or "physics_p2_interface_normal_pressure_partial" not in ctest):
        raise ValueError("Full MSVC build/127 CTest evidence missing")
    gate_log = GATE_LOG.read_text(encoding="utf-16")
    gate = json.loads(GATE_JSON.read_text(encoding="utf-8"))
    if ("ALL_COMPLETE=False" not in gate_log
        or gate["phases"]["P2"]["status"] != "PARTIAL"
        or gate.get("all_phases_complete") is not False):
        raise ValueError("Full phase gate must remain P2 PARTIAL")
    measured = json.loads(MARIN.read_text(encoding="utf-8"))
    if (measured.get("phase") != "P2"
        or measured.get("phase_complete") is not False
        or measured.get("height_rmse_threshold_met") is not False):
        raise ValueError("Original negative 6s MARIN measurement evidence missing")
    rmse = {
        name: measured["metrics"][name + "_m"]["rmse"]
        for name in ("H1", "H2", "H3", "H4")
    }
    if not all(math.isfinite(v) and v > .05 for v in rmse.values()):
        raise ValueError("Unresolved experimental height failure concealed")
    source_hashes = {name: sha(CORE / name) for name in SOURCES}
    report = {
        "phase": "P2",
        "milestone": "inhomogeneous_normal_pressure_mac_pcg_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "native_assertions": 4785,
        "full_ctest_suites": 127,
        "independent_native_reexecution": True,
        "native_log_mutation_rejections": rejections,
        "manufactured_reference": metrics,
        "prior_real_6s_marin_height_rmse_m": rmse,
        "prior_real_6s_marin_height_validation_pass": False,
        "stress_consistent_two_phase_interface": False,
        "computed_surface_curvature_and_viscous_stress": False,
        "dynamic_two_way_fsi": False,
        "global_physical_energy_momentum_closure": False,
        "three_grid_full_6s_measured_convergence": False,
        "ue58_native_e2e_for_new_path": False,
        "source_sha256": source_hashes,
        "native_binary_sha256": sha(BINARY),
        "native_log_sha256": sha(NATIVE),
        "build_log_sha256": sha(BUILD),
        "ctest_log_sha256": sha(CTEST),
        "official_phase_gate_status": gate["phases"]["P2"]["status"],
        "official_phase_gate_log_sha256": sha(GATE_LOG),
        "official_phase_gate_json_sha256": sha(GATE_JSON),
        "prior_real_marin_evidence_sha256": sha(MARIN),
        "generated_utc": datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                      encoding="utf-8")
    print("P2_INTERFACE_NORMAL_PRESSURE_PARTIAL_PASS=TRUE")
    print(f"NATIVE_ASSERTIONS=4785 CTEST=127 MUTATION_REJECTIONS={rejections}")
    print("MARIN_6S_HEIGHT_VALIDATION=FAIL " + json.dumps(rmse))
    print("PHASE_COMPLETE=FALSE")
    print("EVIDENCE=" + str(REPORT))


if __name__ == "__main__":
    main()
