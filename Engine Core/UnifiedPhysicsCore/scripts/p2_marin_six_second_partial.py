"""P2 partial: audit a real 600-step 3D MAC/PCG run against MARIN Test 02.

The numerical trace is not evidence of physically validated two-phase CFD.
No 3D solid motion, stress-consistent surface or independent grid convergence.
"""
from __future__ import annotations

import csv
import hashlib
import io
import json
import math
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

import p2_spheric_marin_compare as reference

CORE = Path(__file__).resolve().parents[1]
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
BINARY = CORE / "build" / "p2_marin_dambreak_heights.exe"
TRACE = RUNS / "p2_marin_pcg_coarse600_dense.txt"
REPORT = RUNS / "p2_marin_pcg_6s_partial.json"
CHANNELS = [f"H{i}_m" for i in range(1, 5)] + [f"P{i}_Pa" for i in range(1, 9)]
FIELDS = ["time_s", *CHANNELS[:4], "water_volume_m3",
          "max_projection_divergence_s_inv", *CHANNELS[4:],
          "pressure_probe_wet_count"]
FOOTER = "PASS MARIN_COARSE_BASELINE partial_only=true steps=600 phase_complete=false"
HEADER_TOKENS = ("nx=32 ny=10 nz=10", "transport=throughflow",
                 "grid=coarse sampling=dense", "pressure_solver=pcg",
                 "pressure_proxy_only=interpolated_tangent",
                 "pressure_sensors_NOT_simulated=false",
                 "obstacle_x_m=[0.6635,0.8245]")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_native(raw: str) -> list[dict[str, float]]:
    lines = raw.splitlines()
    if len(lines) != 604 or not lines[0].startswith("# MARIN_COARSE_3D "):
        raise ValueError("Missing native 600-step header or complete trace")
    if not all(token in lines[0] for token in HEADER_TOKENS):
        raise ValueError("Wrong native solver, geometry or provenance")
    if lines[-1] != FOOTER:
        raise ValueError("Native 600-step PASS footer missing")
    reader = csv.DictReader(io.StringIO("\n".join(lines[1:-1])))
    if reader.fieldnames != FIELDS:
        raise ValueError("Wrong sensor columns")
    rows = [{key: reference.read_float(row[key]) for key in FIELDS}
            for row in reader]
    if len(rows) != 601:
        raise ValueError("Wrong native state count")
    volume = rows[0]["water_volume_m3"]
    for i, row in enumerate(rows):
        if abs(row["time_s"] - i * .01) > 1e-10:
            raise ValueError("Missing or misaligned 10 ms time state")
        if abs(row["water_volume_m3"] - volume) > 1e-8:
            raise ValueError("Physical water volume drift")
        if not 0 <= row["max_projection_divergence_s_inv"] <= 1e-6:
            raise ValueError("MAC pressure projection divergence not converged")
        wet = row["pressure_probe_wet_count"]
        if wet != int(wet) or not 0 <= wet <= 8:
            raise ValueError("Invalid wet pressure-probe count")
        if any(abs(row[key]) > 1e8 for key in CHANNELS[4:]):
            raise ValueError("Pressure overflow")
    return rows


def corrupt_column(lines: list[str], row: int, col: int, replacement: str) -> str:
    modified = lines.copy()
    values = modified[row].split(",")
    values[col] = replacement
    modified[row] = ",".join(values)
    return "\n".join(modified) + "\n"

def regression_negatives(raw: str) -> dict[str, bool]:
    """Verify fail-closed behavior using mutations of the *actual* native trace."""
    lines = raw.splitlines()
    variants = {
        "truncated": "\n".join(lines[:-3]) + "\n",
        "wrong_solver": raw.replace("pressure_solver=pcg", "pressure_solver=gauss_seidel", 1),
        "wrong_geometry": raw.replace("nx=32 ny=10 nz=10", "nx=64 ny=20 nz=20", 1),
        "missing_footer": "\n".join(lines[:-1]) + "\n",
        "nonfinite": corrupt_column(lines, 3, 1, "nan"),
        "time_corrupted": corrupt_column(lines, 4, 0, "0.123456"),
    }
    result = {}
    for name, content in variants.items():
        try:
            parse_native(content)
        except (ValueError, TypeError, KeyError):
            result[name] = True
        else:
            result[name] = False
    return result


def run_native() -> None:
    args = [str(BINARY), "600", "throughflow", "dense", "interp", "pcg"]
    process = subprocess.run(args, cwd=CORE, capture_output=True, text=True,
                             encoding="utf-8", errors="replace", timeout=900)
    # Persist even a failed native run. Never convert a truncated run into PASS.
    TRACE.write_text(process.stdout, encoding="utf-8", newline="\n")
    if process.stderr:
        (RUNS / "p2_marin_pcg_6s_stderr.txt").write_text(
            process.stderr, encoding="utf-8")
    if process.returncode != 0 or process.stderr.strip():
        raise RuntimeError(f"Native 6s run FAILED (exit={process.returncode}): "
                           + process.stderr[-600:])
    parse_native(process.stdout)


def evaluate() -> dict:
    experimental, _, source = reference.load()
    if source["source_zip_sha256"] != (
        "a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1"
    ):
        raise ValueError("Original SPHERIC source fingerprint changed")
    raw = TRACE.read_text(encoding="utf-8")
    rows = parse_native(raw)
    negatives = regression_negatives(raw)
    if not all(negatives.values()):
        raise RuntimeError("Native-trace corruption was not rejected")
    if not experimental[0]["time_s"] <= rows[1]["time_s"] or (
        experimental[-1]["time_s"] < rows[-1]["time_s"]
    ):
        raise ValueError("Measured campaign does not span simulation time")
    metrics = {}
    for channel in CHANNELS:
        errors = [row[channel] - reference.lerp(experimental, row["time_s"], channel)
                  for row in rows[1:]]
        metrics[channel] = {
            "samples": len(errors),
            "rmse": math.sqrt(sum(x*x for x in errors) / len(errors)),
            "mae": sum(abs(x) for x in errors) / len(errors),
            "bias": sum(errors) / len(errors),
            "unit": "m" if channel.startswith("H") else "Pa",
        }
    heights_pass = all(metrics[key]["rmse"] <= .05 for key in CHANNELS[:4])
    source_paths = ["tests/p2_marin_six_second_regression.py",
                    "benchmarks/p2_marin_dambreak_heights.cpp",
                    "include/upc/coupled_water3d.hpp", "include/upc/mac3d.hpp",
                    "include/upc/p2_marin_wall_pressure_interp3d.hpp",
                    "scripts/p2_marin_six_second_partial.py"]
    test_log = RUNS / "p2_marin_6s_regression.txt"
    ctest_log = RUNS / "p2_marin_6s_ctest.txt"
    test_text = test_log.read_text(encoding="utf-8")
    ctest_text = ctest_log.read_text(encoding="utf-8")
    if "Ran 6 tests" not in test_text or not test_text.rstrip().endswith("OK"):
        raise RuntimeError("Independent Python regression not PASS")
    if "100% tests passed, 0 tests failed out of 117" not in ctest_text:
        raise RuntimeError("Native 117-suite CTest regression not PASS")
    if BINARY.stat().st_mtime_ns < (CORE / "benchmarks/p2_marin_dambreak_heights.cpp").stat().st_mtime_ns:
        raise RuntimeError("Native binary older than benchmark source")
    report = {
        "phase": "P2",
        "milestone": "native_mac_pcg_6s_single_grid_vs_original_marin",
        "native_6s_execution_verified": True,
        "native_6s_numerical_integrity": True,
        "height_rmse_threshold_m": .05,
        "height_rmse_threshold_met": heights_pass,
        "pressure_experimental_validation": False,
        "two_grid_full_6s_completed": False,
        "stress_consistent_water_air_interface": False,
        "dynamic_two_way_fsi": False,
        "global_physical_energy_momentum_closure": False,
        "phase_complete": False,
        "grid": {"shape": [32, 10, 10], "dx_m": .1, "dt_s": .01,
                 "states": len(rows), "duration_s": rows[-1]["time_s"]},
        "source_zip_sha256": source["source_zip_sha256"],
        "experiment_csv_sha256": sha(reference.MEASURED),
        "native_trace_sha256": sha(TRACE),
        "native_binary_sha256": sha(BINARY),
        "python_regression_log_sha256": sha(test_log),
        "native_ctest_log_sha256": sha(ctest_log),
        "source_sha256": {p: sha(CORE / p) for p in source_paths},
        "max_water_volume_drift_m3": max(abs(row["water_volume_m3"] -
                                               rows[0]["water_volume_m3"])
                                           for row in rows),
        "max_projected_wet_divergence_s_inv": max(
            row["max_projection_divergence_s_inv"] for row in rows),
        "corruption_rejections": negatives,
        "metrics": metrics,
        "unmet": ["height RMSE <= 0.05 m for all four probes" if not heights_pass else
                  "independent height convergence across resolved grids",
                  "resolved transducer footprint and validated wall traction",
                  "independent >=6s fine grid and three-grid convergence",
                  "two-phase stress and bidirectional moving-solid coupling",
                  "global physical impulse, mass and energy closure"],
        "generated_utc": datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                      encoding="utf-8")
    print("P2_NATIVE_6S_INTEGRITY=True P2_PHYSICAL_VALIDATION=False "
          "P2_COMPLETE=False")
    for key in CHANNELS:
        print(key, "RMSE", f"{metrics[key]['rmse']:.8g}", metrics[key]["unit"])
    return report


if __name__ == "__main__":
    if sys.argv[1:] not in ([], ["--run"]):
        raise SystemExit("Use --run for new native 600-step execution, else audit saved trace")
    if sys.argv[1:] == ["--run"]:
        run_native()
    evaluate()
