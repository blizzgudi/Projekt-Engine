"""P2 partial-only independent native RED/GREEN evidence audit.

Uses real on-disk MSVC/CTest logs and the prior full 6 s MARIN lab report.
Never reruns costly three-grid CFD, never declares full P2 PASS.
"""
from __future__ import annotations

import hashlib
import json
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
ROOT = CORE.parents[1]
RUNS = ROOT / "ENGINE TEST STUBE" / "runs"
RED = RUNS / "p2_footprint_interface_red_repro"
BASELINE = RUNS / "p2_marin_pcg_full6s_three_grid_partial.json"
REPORT = RUNS / "p2_footprint_partial_interface_fallback_20261102.json"

ARTIFACTS = {
    "old_header_isolated": RED / "include/upc/p2_marin_footprint_pressure3d.hpp",
    "old_header_native_build": RED / "red_build.txt",
    "old_header_native_failure": RED / "red_run.txt",
    "corrected_native_green": RUNS / "p2_footprint_partial_interface_green_20261102.txt",
    "native_full_build": RUNS / "p2_footprint_interface_full_build_20261102.txt",
    "native_full_ctest": RUNS / "p2_footprint_interface_full_ctest_20261102.txt",
    "old_full6s_three_grid_experiment": BASELINE,
    "corrected_header": CORE / "include/upc/p2_marin_footprint_pressure3d.hpp",
    "regression_source": CORE / "tests/p2_marin_footprint_pressure3d_tests.cpp",
    "audit_source": CORE / "scripts/p2_footprint_partial_interface_audit.py",
    "documented_scope": CORE / "docs/P2_FOOTPRINT_PARTIAL_INTERFACE_FALLBACK.md",
    "native_test_binary": CORE / "build/upc_p2_marin_footprint_tests.exe",
}

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def content(path: Path) -> str:
    raw = path.read_bytes()
    return raw.decode("utf-16" if raw[:2] == b"\xff\xfe" else "utf-8-sig",
                      errors="replace")

def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)

def red_check(s: str) -> None:
    require("FAIL P2_MARIN_FOOTPRINT partial front VOF must not extrapolate "
            "through interface assertions=70" in s,
            "Historical old-header native failure absent")

def green_check(s: str) -> None:
    require(re.search(r"PASS P2_MARIN_FOOTPRINT assertions=170\b", s) is not None,
            "Current native 170-assertion test not green")
    require("phase_complete=false" in s, "Native output forged phase completion")
    require("all_8_affine_pass=true" in s, "Manufactured hydrostatic control missing")

def ctest_check(s: str) -> None:
    require("100% tests passed, 0 tests failed out of 154" in s,
            "Full 154-suite native CTest missing")
    require("physics_p2_marin_footprint_partial" in s,
            "Footprint native test not included in full CTest")

def check() -> dict:
    for label, path in ARTIFACTS.items():
        require(path.is_file() and path.stat().st_size > 0,
                f"Missing native evidence: {label}")
    red_check(content(ARTIFACTS["old_header_native_failure"]))
    green_check(content(ARTIFACTS["corrected_native_green"]))
    ctest_check(content(ARTIFACTS["native_full_ctest"]))
    # The previous native source is recovered from a real Git commit,
    # never reconstructed from a manually edited failure message.
    parent = subprocess.run(
        ["git", "-C", str(ROOT), "rev-parse", "b0b8248"],
        check=True, capture_output=True, text=True).stdout.strip()
    old_git = subprocess.run(
        ["git", "-C", str(ROOT), "show",
         "b0b8248:Engine Core/UnifiedPhysicsCore/include/upc/"
         "p2_marin_footprint_pressure3d.hpp"],
        check=True, capture_output=True).stdout
    require(old_git.replace(b"\r\n", b"\n").strip() ==
            ARTIFACTS["old_header_isolated"].read_bytes().replace(
                b"\r\n", b"\n").strip(),
            "RED compiled header differs from original committed source")
    require(sha(ARTIFACTS["old_header_isolated"]) !=
            sha(ARTIFACTS["corrected_header"]),
            "RED/GREEN header source indistinguishable")
    baseline = json.loads(BASELINE.read_text(encoding="utf-8-sig"))
    require(baseline["complete_phase"] is False and
            baseline["experimental_three_grid_cfd_validated"] is False,
            "Previous physical validation unexpectedly green")
    height = baseline["fine_height_rmse_m"]
    target = baseline["full_six_second_height_rmse_target_m"]
    require(target == .05 and all(v > target for v in height.values()),
            "Previous six-second height failure not preserved")
    pressure_monotonic = {
        f"P{i}_Pa": baseline["rmse_refinement_monotonic"][f"P{i}_Pa"]
        for i in range(1, 9)
    }
    require(not any(pressure_monotonic.values()),
            "Previous all-eight pressure divergence not preserved")
    negative_cases = [
        ("forged_old_pass", lambda: red_check(content(ARTIFACTS["old_header_native_failure"]).replace(
            "FAIL P2_MARIN_FOOTPRINT", "PASS P2_MARIN_FOOTPRINT"))),
        ("forged_green_assertions", lambda: green_check(content(ARTIFACTS["corrected_native_green"]).replace(
            "assertions=170", "assertions=0"))),
        ("forged_phase_pass", lambda: green_check(content(ARTIFACTS["corrected_native_green"]).replace(
            "phase_complete=false", "phase_complete=true"))),
        ("forged_ctest_count", lambda: ctest_check(content(ARTIFACTS["native_full_ctest"]).replace(
            "out of 154", "out of 1"))),
        ("forged_ctest_failure", lambda: ctest_check(content(ARTIFACTS["native_full_ctest"]).replace(
            "0 tests failed", "1 tests failed"))),
    ]
    rejected = []
    for name, fn in negative_cases:
        try:
            fn()
        except ValueError:
            rejected.append(name)
    require(len(rejected) == len(negative_cases), "Forged evidence accepted")
    return {
        "phase": "P2",
        "milestone": "marin_finite_disk_partial_vof_normal_fallback",
        "partial_accepted": True,
        "phase_complete": False,
        "physical_marin_measurement_validated": False,
        "old_header_parent_git_commit": parent,
        "red_old_header_native_exit_code": 1,
        "red_failure_assertions": 70,
        "green_native_assertions": 170,
        "full_native_ctest_suites": 154,
        "negative_evidence_mutations_rejected": rejected,
        "real_full6s_lab_height_rmse_fine_m": height,
        "height_rmse_target_m": target,
        "full6s_height_acceptance": False,
        "full6s_pressure_refinement_monotonic": pressure_monotonic,
        "full6s_pressure_convergence_acceptance": False,
        "full6s_native_recomputed": False,
        "source_and_artifact_sha256": {name: sha(path) for name, path in ARTIFACTS.items()},
        "unmet": [
            "actual measured MARIN H1-H4 <=0.05m over >=6s on 3 grids",
            "resolved calibrated P1-P8 transducer wet area and time response",
            "exact obstacle cut-cell pressure boundary integrated in live MAC",
            "moving water/air stress continuity and global energy/momentum",
            "bidirectional dynamic fluid-solid FSI and UE5.8 full native E2E",
        ],
        "generated_utc": datetime.now(timezone.utc).isoformat(),
    }

if __name__ == "__main__":
    result = check()
    # Rechecking unchanged, hashed evidence must not dirty the local Git
    # worktree by regenerating a new timestamp on every automation run.
    if REPORT.is_file():
        previous = json.loads(REPORT.read_text(encoding="utf-8-sig"))
        stable = lambda obj: {k: v for k, v in obj.items()
                              if k != "generated_utc"}
        if stable(previous) == stable(result):
            result["generated_utc"] = previous["generated_utc"]
    serialized = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if not REPORT.is_file() or REPORT.read_text(encoding="utf-8-sig") != serialized:
        REPORT.write_text(serialized, encoding="utf-8")
    print("P2_FOOTPRINT_PARTIAL_PASS=True RED_NATIVE=FAIL_EXPECTED "
          "GREEN_NATIVE=PASS CTEST=154/154 NEGATIVE_QA=5/5 PHASE_COMPLETE=False")
    print("EVIDENCE=" + str(REPORT))
