"""P2 PARTIAL: independent 2-s native H1-H4 bilinear-column diagnostic.

Compare the actual new 3D MAC/PCG trace with the pinned previous native
single-column trace and the original MARIN/SPHERIC laboratory time series.
This is neither a full six-second rerun nor a full CFD acceptance.
"""
from __future__ import annotations

import csv
import hashlib
import io
import json
import math
from datetime import datetime, timezone
from pathlib import Path

import p2_spheric_marin_compare as measured

CORE = Path(__file__).resolve().parents[1]
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
NEW = RUNS / "p2_marin_height_interp_coarse200.txt"
OLD = RUNS / "p2_marin_pcg_coarse200_dense.txt"
REPORT = RUNS / "p2_marin_height_interp_partial.json"
BINARY = CORE / "build" / "p2_marin_dambreak_heights.exe"
NATIVE_TEST = CORE / "build" / "upc_p2_marin_height_interp_tests.exe"
FIELDS = ["time_s", *(f"H{i}_m" for i in range(1, 5)),
          "water_volume_m3", "max_projection_divergence_s_inv",
          *(f"P{i}_Pa" for i in range(1, 9)), "pressure_probe_wet_count"]
HEIGHTS = [f"H{i}_m" for i in range(1, 5)]
PRESSURES = [f"P{i}_Pa" for i in range(1, 9)]
OTHER = [k for k in FIELDS if k not in HEIGHTS]
ZIP_SHA = "a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse(raw: str, interpolated: bool) -> list[dict[str, float]]:
    lines = raw.splitlines()
    if len(lines) != 204 or not lines[0].startswith("# MARIN_COARSE_3D "):
        raise ValueError("Missing 201 native states or provenance")
    header = lines[0]
    required = ("nx=32 ny=10 nz=10", "grid=coarse sampling=dense",
                "transport=throughflow", "pressure_solver=pcg",
                "pressure_proxy_only=interpolated_tangent",
                "pressure_sensors_NOT_simulated=false")
    if not all(token in header for token in required):
        raise ValueError("Wrong native geometry, transport or pressure mode")
    if ("height_sampling=bilinear_column" in header) != interpolated:
        raise ValueError("Wrong height interpolation mode")
    if lines[-1] != ("PASS MARIN_COARSE_BASELINE partial_only=true "
                     "steps=200 phase_complete=false"):
        raise ValueError("Missing native partial-only footer")
    reader = csv.DictReader(io.StringIO("\n".join(lines[1:-1])))
    if reader.fieldnames != FIELDS:
        raise ValueError("Unexpected native signal schema")
    rows = []
    for i, row in enumerate(reader):
        item = {k: measured.read_float(row[k]) for k in FIELDS}
        if abs(item["time_s"] - i * .01) > 1e-10:
            raise ValueError("Nonmonotone/misaligned native time")
        if any(not 0 <= item[k] <= 1.0 for k in HEIGHTS):
            raise ValueError("Invalid water column height")
        if not 0 <= item["max_projection_divergence_s_inv"] <= 1e-6:
            raise ValueError("Unconverged MAC pressure projection")
        wet = item["pressure_probe_wet_count"]
        if wet != int(wet) or not 0 <= wet <= 8:
            raise ValueError("Invalid wet pressure probe count")
        rows.append(item)
    if len(rows) != 201:
        raise ValueError("Incomplete native trajectory")
    if max(abs(r["water_volume_m3"]-rows[0]["water_volume_m3"])
           for r in rows) > 1e-8:
        raise ValueError("Native volume drift")
    return rows


def compare(new: list[dict[str, float]],
            old: list[dict[str, float]]) -> dict:
    if len(new) != len(old):
        raise ValueError("Incomparable native trajectories")
    solver_differences = {k: max(abs(a[k]-b[k]) for a, b in zip(new, old))
                          for k in OTHER}
    if any(v > 1e-10 for v in solver_differences.values()):
        raise ValueError("Height-only mode changed the 3D solver signals")
    height_differences = {
        k: max(abs(a[k]-b[k]) for a, b in zip(new, old)) for k in HEIGHTS
    }
    if not any(v > 1e-8 for v in height_differences.values()):
        raise ValueError("No observable H1-H4 subcell interpolation effect")
    return {"max_solver_signal_differences": solver_differences,
            "max_height_signal_differences_m": height_differences}


def mutations_rejected(raw: str) -> dict[str, bool]:
    lines = raw.splitlines()
    variants = {
        "truncated": "\n".join(lines[:-4]) + "\n",
        "wrong_mode": raw.replace("height_sampling=bilinear_column",
                                  "height_sampling=nearest_cell", 1),
        "wrong_pressure": raw.replace("pressure_solver=pcg",
                                      "pressure_solver=gauss_seidel", 1),
        "nonfinite_height": replace_column(lines, 4, 1, "nan"),
        "time_corrupted": replace_column(lines, 7, 0, "0.42"),
        "invalid_wet_count": replace_column(lines, 8, 15, "9"),
    }
    result = {}
    for key, content in variants.items():
        try:
            parse(content, True)
        except (ValueError, KeyError, TypeError):
            result[key] = True
        else:
            result[key] = False
    return result


def replace_column(lines: list[str], index: int, col: int, value: str) -> str:
    changed = lines.copy()
    parts = changed[index].split(",")
    parts[col] = value
    changed[index] = ",".join(parts)
    return "\n".join(changed) + "\n"


def evaluate() -> dict:
    original, _, source = measured.load()
    if source["source_zip_sha256"] != ZIP_SHA:
        raise ValueError("Official experiment archive fingerprint changed")
    raw = NEW.read_text(encoding="utf-8-sig")
    new = parse(raw, True)
    old = parse(OLD.read_text(encoding="utf-8-sig"), False)
    comparison = compare(new, old)
    negatives = mutations_rejected(raw)
    if not all(negatives.values()):
        raise ValueError("Corrupted native trace was not rejected")
    if original[0]["time_s"] > new[1]["time_s"] or (
        original[-1]["time_s"] < new[-1]["time_s"]
    ):
        raise ValueError("Laboratory time window does not cover native states")
    metrics = {}
    for channel in HEIGHTS + PRESSURES:
        m = [row[channel]-measured.lerp(original, row["time_s"], channel)
             for row in new[1:]]
        o = [row[channel]-measured.lerp(original, row["time_s"], channel)
             for row in old[1:]]
        metrics[channel] = {
            "new_rmse": math.sqrt(sum(v*v for v in m)/len(m)),
            "previous_rmse": math.sqrt(sum(v*v for v in o)/len(o)),
            "samples": len(m),
            "unit": "m" if channel in HEIGHTS else "Pa",
        }
    native_log = RUNS / "p2_marin_height_interp_native.txt"
    ctest_log = RUNS / "p2_marin_height_interp_ctest.txt"
    py_log = RUNS / "p2_marin_height_interp_regression.txt"
    if "PASS P2_MARIN_HEIGHT_INTERP assertions=60 phase_complete=false" not in (
        native_log.read_text(encoding="utf-8")
    ):
        raise ValueError("Missing native manufactured/live height tests")
    ctext = ctest_log.read_text(encoding="utf-8")
    if "100% tests passed, 0 tests failed out of 118" not in ctext:
        raise ValueError("Missing complete 118-suite native CTest")
    ptext = py_log.read_text(encoding="utf-8")
    if "Ran 6 tests" not in ptext or not ptext.rstrip().endswith("OK"):
        raise ValueError("Independent 6-test Python QA not passed")
    sources = [
        "CMakeLists.txt", "benchmarks/p2_marin_dambreak_heights.cpp",
        "include/upc/p2_marin_height_interp3d.hpp",
        "tests/p2_marin_height_interp3d_tests.cpp",
        "tests/p2_marin_height_interp_regression.py",
        "scripts/p2_marin_height_interp_build.cmd",
        "scripts/p2_marin_height_interp_partial_acceptance.py",
    ]
    height_pass = all(metrics[k]["new_rmse"] <= .05 for k in HEIGHTS)
    report = {
        "phase": "P2",
        "milestone": "subcell_bilinear_H1_H4_real_native_3d_mac_pcg_2s",
        "numerical_probe_integrity_partial": True,
        "native_2s_execution_verified": True,
        "full_6s_recomputed_with_new_probe": False,
        "two_grid_independent_full_6s": False,
        "height_rmse_threshold_m": .05,
        "height_experimentally_validated": height_pass,
        "pressure_experimentally_validated": False,
        "stress_consistent_water_air_interface": False,
        "dynamic_bidirectional_fsi": False,
        "global_physical_impulse_energy_closure": False,
        "phase_complete": False,
        "grid": {"shape": [32, 10, 10], "dx_m": .1,
                 "states": len(new), "duration_s": new[-1]["time_s"]},
        "original_experiment_archive_sha256": ZIP_SHA,
        "measured_csv_sha256": sha(measured.MEASURED),
        "new_native_trace_sha256": sha(NEW),
        "old_native_trace_sha256": sha(OLD),
        "native_benchmark_binary_sha256": sha(BINARY),
        "native_test_binary_sha256": sha(NATIVE_TEST),
        "native_test_log_sha256": sha(native_log),
        "native_ctest_log_sha256": sha(ctest_log),
        "python_regression_log_sha256": sha(py_log),
        "source_sha256": {p: sha(CORE / p) for p in sources},
        "native_trajectory_comparison": comparison,
        "negative_corruption_rejections": negatives,
        "metrics": metrics,
        "unmet": [
            "Measured H1-H4 RMSE <=0.05 m over full >=6 s and 3 independent grids",
            "Measured P1-P8 footprint/traction and pressure convergence",
            "True stress-consistent 3D water-air free surface",
            "Dynamic bidirectional moving-solid FSI and physical impulse/energy closure",
        ],
        "generated_utc": datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                      encoding="utf-8")
    print("P2_HEIGHT_INTERPOLATION_NATIVE_PARTIAL=True "
          "P2_EXPERIMENTAL_HEIGHT_PASS=" + str(height_pass)
          + " P2_COMPLETE=False")
    for channel in HEIGHTS:
        m = metrics[channel]
        print(channel, "RMSE_new", f"{m['new_rmse']:.9g}",
              "RMSE_previous", f"{m['previous_rmse']:.9g}")
    return report


if __name__ == "__main__":
    evaluate()
