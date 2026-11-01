"""P2 partial: 3D dam-break grid-refinement check against real MARIN heights.

Two grid resolutions and fixed 2 s time window are NOT full experimental
validation. Quantitative nonconvergence remains a recorded FAIL, never fixed
by relaxing thresholds.
"""
from __future__ import annotations

import csv
import hashlib
import json
import math
from datetime import datetime, timezone
from pathlib import Path

import p2_spheric_marin_compare as official

RUNS=official.RUNS
LOGS={
    "coarse":RUNS/"p2_marin_throughflow_coarse200_dense.txt",
    "fine":RUNS/"p2_marin_throughflow_fine200_dense.txt",
}
EXPECTED={
    "coarse":{"dx":.1,"nx":32,"ny":10,"nz":10},
    "fine":{"dx":.05,"nx":64,"ny":20,"nz":20},
}
HEADERS=["time_s","H1_m","H2_m","H3_m","H4_m",
         "water_volume_m3","max_projection_divergence_s_inv"]

def digest(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def load_native(path:Path,grid:str)->list[dict]:
    raw=path.read_bytes()
    data=raw.decode("utf-16" if raw.startswith(bytes((255,254))) else "utf-8-sig")
    lines=data.splitlines()
    if len(lines)<5:
        raise RuntimeError(f"Incomplete native log {grid}")
    head=lines[0]
    params=EXPECTED[grid]
    if (not head.startswith("# MARIN_COARSE_3D ") or
        f"nx={params['nx']} ny={params['ny']} nz={params['nz']}" not in head or
        f"grid={grid}" not in head or
        "transport=throughflow" not in head or
        "sampling=dense" not in head or
        f"dx_m={params['dx']:.17g}" not in head or
        "represented_extent_x_m=3.2000000000000002" not in head or
        "pressure_sensors_NOT_simulated=true" not in head):
        raise RuntimeError(f"Unverified native MARIN grid geometry {grid}")
    if lines[-1]!="PASS MARIN_COARSE_BASELINE partial_only=true steps=200 phase_complete=false":
        raise RuntimeError(f"Native MARIN grid {grid} missing actual PASS")
    reader=csv.DictReader(lines[1:-1])
    if reader.fieldnames!=HEADERS:
        raise RuntimeError(f"Invalid MARIN native height channels {grid}")
    values=[{key:official.read_float(row[key]) for key in HEADERS} for row in reader]
    if len(values)!=201 or abs(values[0]["time_s"])>1e-14 or abs(values[-1]["time_s"]-2)>1e-10:
        raise RuntimeError(f"Missing MARIN time samples {grid}")
    if any(abs((b["time_s"]-a["time_s"])-.01)>1e-10 for a,b in zip(values,values[1:])):
        raise RuntimeError(f"Nonmonotone simulation clock {grid}")
    return values

def compare():
    measured,_,source=official.load()
    base={}
    results={}
    for grid,path in LOGS.items():
        vals=load_native(path,grid)
        before=vals[0]["water_volume_m3"]
        drift=max(abs(row["water_volume_m3"]-before) for row in vals)
        if drift>1e-8:
            raise RuntimeError(f"Native {grid} simulation violates water conservation")
        probes={}
        for channel in official.HEIGHTS:
            residuals=[
                row[channel]-official.lerp(measured,row["time_s"],channel)
                for row in vals[1:]
            ]
            probes[channel]={
                "rmse_m":math.sqrt(sum(r*r for r in residuals)/len(residuals)),
                "max_abs_error_m":max(abs(r) for r in residuals),
                "final_error_m":residuals[-1],
                "sample_count":len(residuals),
            }
        results[grid]={
            "grid":EXPECTED[grid],
            "water_volume_initial_m3":before,
            "water_volume_max_drift_m3":drift,
            "readback":probes,
            "max_projection_divergence_s_inv":max(
                row["max_projection_divergence_s_inv"] for row in vals),
            "runtime_log_sha256":digest(path),
        }
    improved={
        name:results["fine"]["readback"][name]["rmse_m"]<
             results["coarse"]["readback"][name]["rmse_m"]
        for name in official.HEIGHTS
    }
    within={
        name:results["fine"]["readback"][name]["rmse_m"]<=.05
        for name in official.HEIGHTS
    }
    # Two spatial grids and 200 0.01-second samples cannot identify
    # asymptotic order or validate the full flow.
    study={
        "phase":"P2","scope":"independent_marinsphERIC_3d_height_grid_refinement_partial",
        "initial_conditions":"identical 3.20 x 1 x 1 m represented tank; 0.55 m reservoir; binary cuboid",
        "experimental_source_url":"https://www.spheric-sph.org/tests/test-02",
        "experimental_source_zip_sha256":source["source_zip_sha256"],
        "measured_csv_sha256":digest(official.MEASURED),
        "native_runs":results,
        "heights_improve_with_refinement":improved,
        "fine_heights_within_0.05m_rmse":within,
        "all_heights_improved":all(improved.values()),
        "experimentally_validated":False,
        "pressure_sensor_validation":False,
        "runtime_6seconds_validated":False,
        "full_grid_order_verified":False,
        "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "unmet":[
            "physical_height_rmse_0.05m_for_all_channels",
            "pressure_P1_P8_native_vs_measured",
            "full_6_second_experiment",
            "three_grid_asymptotic_convergence",
            "stress_consistent_free_surface_and_dynamic_FSI",
        ],
    }
    dest=RUNS/"p2_marin_grid_refinement_partial.json"
    dest.write_text(json.dumps(study,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("P2_MARIN_TWO_GRIDS=True PHASE_COMPLETE=False VALIDATED=False "
          "ALL_IMPROVED="+str(study["all_heights_improved"]))
    for ch in official.HEIGHTS:
        print(ch, "coarse_rmse_m",results["coarse"]["readback"][ch]["rmse_m"],
              "fine_rmse_m",results["fine"]["readback"][ch]["rmse_m"])
    return study

if __name__=="__main__":
    compare()
