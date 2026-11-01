"""Independent native-output/negative-measurement P2 PARTIAL QA; never full acceptance."""
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
BINARY = CORE / "build" / "upc_p2_live_auto_planar_tests.exe"
BUILD = RUNS / "p2_live_auto_planar_build.txt"
CTEST = RUNS / "p2_live_auto_planar_ctest.txt"
NATIVE = RUNS / "p2_live_auto_planar_native.txt"
MARIN = RUNS / "p2_marin_pcg_6s_partial.json"
PHASE_GATE_LOG = RUNS / "p2_live_auto_planar_phase_gate.txt"
PHASE_GATE_JSON = RUNS / "unified_physics_gate.json"
REPORT = RUNS / "p2_live_auto_planar_partial.json"
SOURCES = [
    "CMakeLists.txt",
    "include/upc/coupled_water3d.hpp",
    "include/upc/p2_local_planar_normal_fit3d.hpp",
    "include/upc/p2_planar_vof_pcg_geometry3d.hpp",
    "include/upc/p2_oblique_plic_geometry3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_live_auto_planar_vof_pcg3d_tests.cpp",
    "scripts/p2_live_auto_planar_build.cmd",
    "scripts/p2_live_auto_planar_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_LIVE_AUTO_PLANAR_PLIC_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
ROW = re.compile(
    r"LIVE_AUTO_PLIC n=(\d+) first_fitted=(\d+) first_faces=(\d+) "
    r"first_div=([0-9.eE+\-]+) second_div=([0-9.eE+\-]+) rollback=PASS"
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_native(log: str) -> list[dict]:
    lines = log.splitlines()
    if len(lines) != 3 or lines[-1] != (
        "PASS P2_LIVE_AUTO_PLANAR assertions=60 phase_complete=false"
    ):
        raise ValueError("Missing exact native PASS count or partial marker")
    result = []
    for line, expected in zip(lines[:2], ((8, 104, 96), (12, 166, 165))):
        m = ROW.fullmatch(line)
        if not m:
            raise ValueError("Malformed or missing native live PLIC row")
        n, cells, faces = map(int, m.group(1, 2, 3))
        if (n, cells, faces) != expected:
            raise ValueError("Grid or geometric fit face counts inconsistent")
        div, second = float(m.group(4)), float(m.group(5))
        if not all(math.isfinite(x) and 0 <= x < 1e-7 for x in (div, second)):
            raise ValueError("MAC-PCG wet divergence exceeds tolerance")
        result.append({
            "grid": n, "fitted_partial_cells": cells,
            "resolved_oblique_faces": faces,
            "first_divergence_s_inv": div,
            "second_frame_divergence_s_inv": second,
            "rollback_negative_test": True,
        })
    return result


def mutation_rejections(log: str) -> int:
    mutations = [
        log.replace("assertions=60", "assertions=59"),
        log.replace("phase_complete=false", "phase_complete=true"),
        log.replace("rollback=PASS", "rollback=FAIL", 1),
        log.replace("n=12", "n=10", 1),
        log.replace("first_faces=165", "first_faces=0", 1),
        log.replace("first_div=2.654347146810801e-16", "first_div=0.01", 1),
        "\n".join(log.splitlines()[1:]) + "\n",
    ]
    for altered in mutations:
        if altered == log:
            raise ValueError("A deliberate corruption mutation did not change output")
        try:
            verify_native(altered)
        except ValueError:
            continue
        raise ValueError("A deliberately corrupted native log was accepted")
    return len(mutations)


def main() -> None:
    log = NATIVE.read_text(encoding="utf-8")
    rows = verify_native(log)
    rejected = mutation_rejections(log)
    native = subprocess.run(
        [str(BINARY)], cwd=CORE, capture_output=True, text=True, timeout=45
    )
    if native.returncode or native.stdout.strip() != log.strip():
        raise ValueError("Re-executed MSVC native test differs from recorded log")
    if native.stderr:
        raise ValueError("Native stderr not empty")
    build = BUILD.read_text(encoding="utf-8", errors="replace")
    ctest = CTEST.read_text(encoding="utf-8", errors="replace")
    if ("Build files have been written" not in build or
        "upc_p2_live_auto_planar_tests.exe" not in build or
        "100% tests passed, 0 tests failed out of 126" not in ctest or
        "physics_p2_live_auto_planar_vof_pcg_partial" not in ctest):
        raise ValueError("Full native MSVC build / 126 CTest evidence missing")
    gate_log = PHASE_GATE_LOG.read_text(encoding="utf-16")
    gate = json.loads(PHASE_GATE_JSON.read_text(encoding="utf-8"))
    if ("ALL_COMPLETE=False" not in gate_log or
        "P2" not in gate_log.split("GATES_INCOMPLETE=")[-1] or
        gate["phases"]["P2"]["status"] != "PARTIAL" or
        gate.get("all_phases_complete") is not False):
        raise ValueError("Official full P2 phase gate must remain PARTIAL")
    measurement = json.loads(MARIN.read_text(encoding="utf-8"))
    if measurement.get("phase") != "P2" or measurement.get("phase_complete") is not False:
        raise ValueError("Prior measured P2 phase status not conservative")
    if measurement.get("height_rmse_threshold_met") is not False:
        raise ValueError("Prior MARIN measurement FAIL must remain visible")
    rmse = {
        name: measurement["metrics"][name + "_m"]["rmse"]
        for name in ("H1", "H2", "H3", "H4")
    }
    if not all(math.isfinite(x) and x > .05 for x in rmse.values()):
        raise ValueError("Existing MARIN H1-H4 experimental FAIL not reproduced")
    source_hashes = {name: sha(CORE / name) for name in SOURCES}
    report = {
        "phase": "P2",
        "milestone": "live_dynamic_local_planar_vof_pcg_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "native_assertions": 60,
        "full_ctest_suites": 126,
        "independent_native_reexecution": True,
        "native_log_mutation_rejections": rejected,
        "manufactured_live_3d_grids": rows,
        "prior_real_6s_marin_height_rmse_m": rmse,
        "prior_real_6s_marin_height_validation_pass": False,
        "stress_consistent_two_phase_interface": False,
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
        "official_phase_gate_log_sha256": sha(PHASE_GATE_LOG),
        "official_phase_gate_json_sha256": sha(PHASE_GATE_JSON),
        "prior_real_marin_evidence_sha256": sha(MARIN),
        "generated_utc": datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                      encoding="utf-8")
    print("P2_LIVE_AUTO_PLANAR_PARTIAL_PASS=TRUE")
    print("NATIVE_ASSERTIONS=60 CTEST=126 MUTATION_REJECTIONS=" + str(rejected))
    print("MARIN_6S_HEIGHT_VALIDATION=FAIL " + json.dumps(rmse))
    print("PHASE_COMPLETE=FALSE")
    print("EVIDENCE=" + str(REPORT))


if __name__ == "__main__":
    main()
