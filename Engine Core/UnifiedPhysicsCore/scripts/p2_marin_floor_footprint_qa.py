"""Independent manufactured hydrostatic QA for P2 MARIN footprint floor sampling.

No six-second CFD run. Strictly PARTIAL; previous lab nonconvergence remains FAIL.
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
PROJECT = CORE.parents[1]
RUNS = PROJECT / "ENGINE TEST STUBE" / "runs"
BINARY = CORE / "build" / "upc_p2_marin_footprint_tests.exe"
LOG = RUNS / "p2_marin_floor_footprint_qa_native.txt"
REPORT = RUNS / "p2_marin_floor_footprint_partial.json"
CTEST = RUNS / "p2_marin_floor_footprint_ctest_all_20261102.log"
OLD_LAB = RUNS / "p2_marin_pcg_full6s_three_grid_partial.json"
BASELINE_LOG = RUNS / "p2_floor_footprint_baseline_20261102" / "baseline_failure.log"
LINE = re.compile(
    r"^FLOOR_HYDROSTATIC dx_m=(\S+) p1_native_pa=(\S+) "
    r"p1_analytic_pa=(\S+) p1_clamped_centre_pa=(\S+) "
    r"all_8_affine_pass=true$"
)
PASS = re.compile(
    r"^PASS P2_MARIN_FOOTPRINT assertions=(\d+) "
    r"radius_m_is_unverified=true phase_complete=false$"
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_output(output: str) -> list[dict]:
    lines = output.splitlines()
    floor_lines = [line for line in lines if line.startswith("FLOOR_HYDROSTATIC ")]
    if len(floor_lines) != 2:
        raise ValueError("Missing/duplicate independent floor references")
    results = []
    for line in floor_lines:
        match = LINE.fullmatch(line)
        if not match:
            raise ValueError("Malformed or forged floor native reference")
        dx, got, printed, centre = map(float, match.groups())
        if not all(math.isfinite(x) for x in (dx, got, printed, centre)):
            raise ValueError("Nonfinite manufactured reference")
        if dx not in (0.1, 0.05):
            raise ValueError("Unexpected grid resolution")
        expected = 9810.0 * (1.0 - 0.021)
        centre_expected = 9810.0 * (1.0 - 0.5 * dx)
        if abs(got - expected) > 1e-7 or abs(printed - expected) > 1e-10:
            raise ValueError("P1 hydrostatic disk pressure mismatch")
        if abs(centre - centre_expected) > 1e-10:
            raise ValueError("Wrong cell-centre baseline")
        results.append({
            "dx_m": dx,
            "p1_native_pa": got,
            "p1_analytic_pa": expected,
            "nearest_cell_centre_pa_not_disk_integral": centre,
            "nearest_centre_bias_pa": expected - centre,
        })
    if set(row["dx_m"] for row in results) != {0.1, 0.05}:
        raise ValueError("Both independent resolutions required")
    markers = [PASS.fullmatch(line) for line in lines if line.startswith("PASS P2_MARIN_FOOTPRINT")]
    if len(markers) != 1 or markers[0] is None or int(markers[0].group(1)) < 164:
        raise ValueError("Native test completion marker missing/invalid")
    native = [line for line in lines if line.startswith("NATIVE_MAC_FOOTPRINT ")]
    if len(native) != 2:
        raise ValueError("Two native 3D MAC samples required")
    return sorted(results, key=lambda row: row["dx_m"], reverse=True)


def main() -> None:
    if not BINARY.is_file():
        raise RuntimeError("No freshly compiled native MSVC binary")
    run = subprocess.run([str(BINARY)], capture_output=True, text=True, timeout=60)
    if run.returncode != 0:
        raise RuntimeError(f"Native failed: {run.returncode}: {run.stderr[-1000:]}")
    output = run.stdout
    reference = verify_output(output)
    negatives = {
        "wrong_native_pressure": output.replace(
            "p1_native_pa=9603.9899999999943", "p1_native_pa=9500", 1),
        "wrong_grid": output.replace("dx_m=0.10000000000000001", "dx_m=0.2", 1),
        "missing_fine_reference": "\n".join(
            x for x in output.splitlines() if not x.startswith("FLOOR_HYDROSTATIC dx_m=0.050000000000000003")),
        "forged_full_phase": output.replace("phase_complete=false", "phase_complete=true"),
        "forged_affine_pass": output.replace("all_8_affine_pass=true", "all_8_affine_pass=false", 1),
        "duplicate_grid": output + "\n" + next(
            x for x in output.splitlines() if x.startswith("FLOOR_HYDROSTATIC ")),
        "nonfinite_reference": output.replace("p1_analytic_pa=9603.9899999999998",
                                              "p1_analytic_pa=nan", 1),
        "missing_native_pass": "\n".join(
            x for x in output.splitlines() if not x.startswith("PASS P2_MARIN_FOOTPRINT")),
    }
    # Mutations must actually change the original bytes, not silently no-op.
    rejected = []
    for label, tampered in negatives.items():
        if tampered == output:
            raise RuntimeError(f"Negative QA mutation not applied: {label}")
        try:
            verify_output(tampered)
        except (ValueError, TypeError):
            rejected.append(label)
        else:
            raise RuntimeError(f"Tampered evidence accepted: {label}")
    ctest_raw = CTEST.read_bytes()
    ctest_text = ctest_raw.decode(
        "utf-16" if ctest_raw.startswith(bytes((255, 254))) else "utf-8-sig")
    if "100% tests passed, 0 tests failed out of 151" not in ctest_text:
        raise RuntimeError("Full 151-test native CTest evidence missing")
    lab = json.loads(OLD_LAB.read_text(encoding="utf-8"))
    fine = lab["fine_height_rmse_m"]
    if not all(fine[f"H{i}_m"] > 0.05 for i in range(1, 5)):
        raise RuntimeError("Unexpected historical lab acceptance; investigate")
    baseline = BASELINE_LOG.read_text(encoding="utf-8-sig")
    if ("expected_red=true" not in baseline or "native_exit=1" not in baseline or
        "FAIL P2_MARIN_FOOTPRINT manufactured hydrostatic wall disk "
        "including floor subcell assertions=51" not in baseline):
        raise RuntimeError("Unmodified HEAD negative native reproduction absent")
    LOG.write_text(output, encoding="utf-8")
    sources = {
        "include/upc/p2_marin_footprint_pressure3d.hpp":
            sha(CORE / "include/upc/p2_marin_footprint_pressure3d.hpp"),
        "tests/p2_marin_footprint_pressure3d_tests.cpp":
            sha(CORE / "tests/p2_marin_footprint_pressure3d_tests.cpp"),
        "scripts/p2_marin_floor_footprint_qa.py": sha(Path(__file__)),
    }
    report = {
        "phase": "P2",
        "scope": "P1 floor-adjacent finite disk pressure interpolation",
        "partial_accepted": True,
        "phase_complete": False,
        "full_p2_pass": False,
        "unverified_real_transducer_radius": True,
        "manufactured_hydrostatic": reference,
        "negative_mutations_rejected": rejected,
        "negative_mutation_count": len(rejected),
        "native_assertions_at_least": 164,
        "native_full_ctest_passed": 151,
        "full_six_second_marin_rmse_target_m": 0.05,
        "historical_full6s_fine_h1_h4_rmse_m": fine,
        "physical_lab_validation": "FAIL (unchanged historical three-grid data)",
        "independent_three_grid_recomputed": False,
        "baseline_git_commit": "2074a00",
        "baseline_expected_negative_native_exit": 1,
        "baseline_failure_log_sha256": sha(BASELINE_LOG),
        "native_binary_sha256": sha(BINARY),
        "native_stdout_sha256": sha(LOG),
        "ctest_log_sha256": sha(CTEST),
        "historical_lab_report_sha256": sha(OLD_LAB),
        "source_sha256": sources,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                      encoding="utf-8")
    print(f"PASS PARTIAL: {len(reference)} manufactured grids, "
          f"{len(rejected)} negative mutations, 151 CTests; P2 full FAIL")
    print("report=" + str(REPORT))


if __name__ == "__main__":
    main()
