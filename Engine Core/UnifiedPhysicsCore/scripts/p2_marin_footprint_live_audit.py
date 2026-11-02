"""P2 PARTIAL: independent audit of native 3D MARIN disk-footprint sampling.

No claim of measured transducer diameter, 6-second disk comparison, stress
validation, dynamic FSI, or full P2 phase acceptance. Never re-run 6s grids.
"""
from __future__ import annotations

import csv
import hashlib
import io
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
ROOT = CORE.parents[1]
RUNS = ROOT / "ENGINE TEST STUBE" / "runs"
EXE = CORE / "build" / "p2_marin_dambreak_heights.exe"
GRIDS = ("coarse", "medium", "fine")
SIZES = {"coarse": (32, 10, 10), "medium": (48, 15, 15),
         "fine": (64, 20, 20)}
FIELDS = tuple(f"P{i}_Pa" for i in range(1, 9))
SHARED = ("time_s", "H1_m", "H2_m", "H3_m", "H4_m",
          "water_volume_m3", "max_projection_divergence_s_inv")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse(raw: str, grid: str, mode: str):
    lines = raw.lstrip("\ufeff").splitlines()
    if len(lines) != 64 or not lines[0].startswith("# MARIN_COARSE_3D "):
        raise ValueError("Native 60-step record length/header invalid")
    meta = lines[0]
    if f"grid={grid} " not in meta or "pressure_solver=pcg " not in meta:
        raise ValueError("Native 3D grid or PCG identity changed")
    nx, ny, nz = SIZES[grid]
    for key, val in (("nx", nx), ("ny", ny), ("nz", nz)):
        if f" {key}={val} " not in meta:
            raise ValueError("Native grid geometry changed")
    expected = ("pressure_proxy_only=footprint_disk_hypothesis"
                if mode == "disk" else
                "pressure_proxy_only=interpolated_tangent")
    if expected not in meta:
        raise ValueError("Native pressure-sampling mode changed")
    if " height_sampling=bilinear_column" not in meta:
        raise ValueError("3D height interpolation not enabled")
    if mode == "disk" and not all(tag in meta for tag in
            ("pressure_footprint_radius_m=0.01",
             "pressure_footprint_rings=8",
             "pressure_footprint_angles=32",
             "pressure_sampling=finite_disk_unverified_radius")):
        raise ValueError("Unverified disk footprint configuration changed")
    if "phase_complete=false" not in lines[-1] or "steps=60" not in lines[-1]:
        raise ValueError("Native partial-only final marker missing")
    reader = csv.DictReader(io.StringIO("\n".join(lines[1:-1])))
    rows = list(reader)
    if len(rows) != 61 or reader.fieldnames is None:
        raise ValueError("Native time series is not 61 states")
    if set(reader.fieldnames) != set(SHARED + FIELDS +
                                      ("pressure_probe_wet_count",)):
        raise ValueError("Native measurement columns invalid")
    for i, row in enumerate(rows):
        for key in SHARED + FIELDS + ("pressure_probe_wet_count",):
            value = float(row[key])
            if not math.isfinite(value):
                raise ValueError("Nonfinite native state")
        if abs(float(row["time_s"]) - i * .01) > 1e-11:
            raise ValueError("Native timestamp missing or forged")
        if abs(float(row["water_volume_m3"]) - .6644) > 1e-8:
            raise ValueError("Native fluid mass balance failed")
        if float(row["max_projection_divergence_s_inv"]) > 1e-6:
            raise ValueError("Native PCG divergence failed")
        if not 0 <= float(row["pressure_probe_wet_count"]) <= 8:
            raise ValueError("Native wet count outside physical range")
    return rows


def audit():
    if not EXE.is_file():
        raise RuntimeError("Actual Win64 MSVC benchmark binary missing")
    source = CORE / "benchmarks" / "p2_marin_dambreak_heights.cpp"
    if EXE.stat().st_mtime_ns < source.stat().st_mtime_ns:
        raise RuntimeError("Native executable predates current source")
    ctest = RUNS / "p2_marin_footprint_live_ctest_20261102.txt"
    build = RUNS / "p2_marin_footprint_live_build_20261102.txt"
    if not ctest.is_file() or not build.is_file():
        raise RuntimeError("MSVC build or full CTest evidence missing")
    ctest_bytes = ctest.read_bytes()
    ctest_text = ctest_bytes.decode(
        "utf-16" if ctest_bytes[:2] == bytes((255, 254)) else "utf-8-sig",
        errors="replace")
    if not re.search(r"100% tests passed, 0 tests failed out of 150", ctest_text):
        raise RuntimeError("150 native CTest suites not independently confirmed")
    outcomes = {}
    all_trace_hashes = {}
    for grid in GRIDS:
        files = {mode: RUNS / f"p2_marin_footprint_live_{grid}_{mode}_60step.txt"
                 for mode in ("point", "disk")}
        rows = {}
        for mode, path in files.items():
            if not path.is_file():
                raise RuntimeError(f"Missing native {grid}/{mode} trace")
            rows[mode] = parse(path.read_text(encoding="utf-8-sig"), grid, mode)
            all_trace_hashes[f"{grid}_{mode}"] = sha(path)
        a, b = rows["point"], rows["disk"]
        for x, y in zip(a, b):
            for field in SHARED:
                if abs(float(x[field]) - float(y[field])) > 1e-11:
                    raise RuntimeError("Changing sensor readout changed fluid dynamics")
        differences = [abs(float(x[field]) - float(y[field]))
                       for x, y in zip(a, b) for field in FIELDS]
        wet_steps = sum(int(float(x["pressure_probe_wet_count"])) > 0 for x in b)
        if wet_steps < 10 or max(differences) <= 1e-3:
            raise RuntimeError("Live native sensor footprint was not exercised")
        # Independent unchanged 6s point-probe record is used as a physics
        # fingerprint, not as a new 6s footprint result. Height interpolation
        # is deliberately different, so compare only pressure/mass/PCG.
        old = RUNS / f"p2_marin_pcg_{grid}600_threegrid.txt"
        old_lines = old.read_text(encoding="utf-8-sig").splitlines()
        old_rows = list(csv.DictReader(io.StringIO("\n".join(old_lines[1:-1]))))
        if len(old_rows) != 601:
            raise RuntimeError("Historical six-second native evidence missing")
        for x, y in zip(a, old_rows[:61]):
            for field in ("time_s", "water_volume_m3",
                          "max_projection_divergence_s_inv") + FIELDS:
                if abs(float(x[field]) - float(y[field])) > 1e-8:
                    raise RuntimeError("Existing 6s solver baseline changed")
        outcomes[grid] = {
            "native_3d_shape": list(SIZES[grid]),
            "native_time_states": len(a),
            "end_time_s": .6,
            "wet_time_states": wet_steps,
            "max_disk_vs_point_pressure_delta_pa": max(differences),
            "historical_6s_point_pressure_reproduced_first_61_states": True,
            "pressure_sensor_radius_m": .01,
            "radius_is_actual_measured_sensor": False,
        }
    # Independent invalid native CLI configurations must fail BEFORE solving.
    negatives = {
        "zero_radius": ["1", "throughflow", "dense", "interp", "pcg",
                        "height_interp", "footprint=0"],
        "oversize_radius": ["1", "throughflow", "dense", "interp", "pcg",
                            "height_interp", "footprint=0.021"],
        "nonfinite_radius": ["1", "throughflow", "dense", "interp", "pcg",
                             "height_interp", "footprint=nan"],
        "junk_radius": ["1", "throughflow", "dense", "interp", "pcg",
                        "height_interp", "footprint=0.01junk"],
        "no_interpolation": ["1", "throughflow", "dense", "pressure", "pcg",
                             "footprint=0.01"],
        "conflicting_normal_extrap": ["1", "throughflow", "dense", "interp",
                                      "pcg", "height_interp", "normal_extrap",
                                      "footprint=0.01"],
    }
    rejected = {}
    for key, args in negatives.items():
        p = subprocess.run([str(EXE), *args], capture_output=True, text=True,
                           timeout=10, cwd=CORE)
        rejected[key] = p.returncode != 0 and "FAIL MARIN_COARSE_BASELINE" in p.stderr
        if not rejected[key]:
            raise RuntimeError(f"Unsafe footprint configuration accepted: {key}")
    # Mutated evidence must not pass the independent parser.
    sample = (RUNS / "p2_marin_footprint_live_fine_disk_60step.txt"
              ).read_text(encoding="utf-8-sig")
    corruptions = {
        "false_phase_pass": sample.replace("phase_complete=false", "phase_complete=true"),
        "forged_radius": sample.replace("pressure_footprint_radius_m=0.01",
                                         "pressure_footprint_radius_m=0.02"),
        "missing_timestep": "\n".join(sample.splitlines()[:20] +
                                        sample.splitlines()[21:]) + "\n",
        "nan_pressure": sample.replace("P1_Pa", "fake_P1_Pa"),
    }
    for name, forged in corruptions.items():
        try:
            parse(forged, "fine", "disk")
        except (ValueError, KeyError, TypeError):
            continue
        raise RuntimeError(f"Forged native trace accepted: {name}")
    source_files = (
        "benchmarks/p2_marin_dambreak_heights.cpp",
        "include/upc/p2_marin_footprint_pressure3d.hpp",
        "include/upc/p2_marin_wall_pressure_interp3d.hpp",
        "include/upc/coupled_water3d.hpp",
        "scripts/p2_marin_footprint_live_audit.py",
        "docs/P2_MARIN_LIVE_FOOTPRINT_SENSOR_PARTIAL.md",
        "docs/PHASENPLAN.md",
    )
    report = {
        "phase": "P2",
        "milestone": "live_native_three_grid_disk_pressure_sampling_short_window",
        "partial_accepted": True,
        "phase_complete": False,
        "independent_measured_pressure_validation": False,
        "sensor_radius_physically_verified": False,
        "native_6s_disk_pressure_validation": False,
        "native_ctest_suites_passed": 150,
        "native_executable_sha256": sha(EXE),
        "native_build_log_sha256": sha(build),
        "native_ctest_log_sha256": sha(ctest),
        "source_sha256": {f: sha(CORE / f) for f in source_files},
        "native_trace_sha256": all_trace_hashes,
        "negative_cli_cases_rejected": rejected,
        "forged_trace_cases_rejected": list(corruptions),
        "grids": outcomes,
        "known_negative_results": [
            "Disk sampling is only a hypothetical 10mm-radius transducer, not the actual MARIN diameter",
            "Disk vs point pressure differs substantially even at 0.6s; no measured 6s RMSE improvement established",
            "No wall-normal resolved stress, wetting force, sharp PLIC interface or calibrated impact response",
            "No full three-grid 6s disk-versus-labor P1-P8 pressure acceptance",
            "No global energy/momentum/FSI or Unreal 5.8 native E2E acceptance",
        ],
        "generated_utc": datetime.now(timezone.utc).isoformat(),
    }
    out = RUNS / "p2_marin_footprint_live_partial.json"
    out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("P2_FOOTPRINT_LIVE_NATIVE_PARTIAL=True PHASE_COMPLETE=False")
    for grid, record in outcomes.items():
        print(grid, "wet_states", record["wet_time_states"],
              "max_delta_pa", record["max_disk_vs_point_pressure_delta_pa"])
    print("CLI_NEGATIVES_REJECTED", len(rejected),
          "FORGED_TRACES_REJECTED", len(corruptions))
    return report


if __name__ == "__main__":
    audit()
