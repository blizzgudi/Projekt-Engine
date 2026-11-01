"""P2 PARTIAL: compare ACTUAL converged 3D MAC-PCG fluid pressure and
water-height trajectories against the original SPHERIC/MARIN experiment.

Strictly separate solver residual convergence from measured physical
validation. A converged Poisson system does not imply continuum CFD PASS.
"""
from __future__ import annotations

import hashlib
import json
import math
import sys
from pathlib import Path
from datetime import datetime,timezone

import p2_spheric_marin_compare as measured
import p2_marin_wall_interp_compare as historical

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
COARSE=RUNS/"p2_marin_pcg_coarse200_dense.txt"
FINE=RUNS/"p2_marin_pcg_fine200_dense.txt"
OUTPUT=RUNS/"p2_marin_pcg_experimental_partial.json"
CHANNELS=[f"P{k}_Pa" for k in range(1,9)]+[f"H{k}_m" for k in range(1,5)]

def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def load(path:Path,grid:str):
    data=path.read_bytes()
    lines=data.decode("utf-16" if data.startswith(bytes((255,254))) else "utf-8-sig").splitlines()
    if len(lines)!=204 or "pressure_solver=pcg" not in lines[0] or (
       f" grid={grid} sampling=dense" not in lines[0]):
        raise RuntimeError("Native PCG MARIN trace provenance or length invalid")
    if "pressure_proxy_only=interpolated_tangent" not in lines[0]:
        raise RuntimeError("Expected independent wall pressure interpolation")
    return historical.load(path,grid)

def evaluate(require_fine=False):
    original,_,source=measured.load()
    if source["source_zip_sha256"]!=historical.OFFICIAL_SHA:
        raise RuntimeError("Modified real experiment source")
    checks={}
    result={}
    all_inputs=[("coarse",COARSE)]
    if require_fine:
        all_inputs.append(("fine",FINE))
    for grid,filename in all_inputs:
        rows=load(filename,grid)
        prior=historical.load(
            historical.COARSE if grid=="coarse" else historical.FINE,grid)
        if len(rows)!=len(prior):
            raise RuntimeError("Incomparable native run lengths")
        residual_max=max(row["max_projection_divergence_s_inv"] for row in rows)
        water_before=rows[0]["water_volume_m3"]
        water_drift=max(abs(row["water_volume_m3"]-water_before) for row in rows)
        metrics={}
        for channel in CHANNELS:
            error=[row[channel]-measured.lerp(original,row["time_s"],channel)
                   for row in rows[1:]]
            old=[row[channel]-measured.lerp(original,row["time_s"],channel)
                 for row in prior[1:]]
            metrics[channel]={
                "rmse_pcg":math.sqrt(sum(x*x for x in error)/len(error)),
                "rmse_gauss_seidel":math.sqrt(sum(x*x for x in old)/len(old)),
                "rmse_improved":sum(x*x for x in error)<sum(x*x for x in old),
                "samples":len(error),
            }
        result[grid]={
            "native_log_sha256":sha(filename),
            "comparison_log_sha256":sha(historical.COARSE if grid=="coarse" else historical.FINE),
            "201_native_time_states":len(rows),
            "maximum_real_wet_projection_divergence_s_inv":residual_max,
            "maximum_volume_drift_m3":water_drift,
            "metrics":metrics,
        }
        checks[grid+"_matrix_projected_divergence_below_1e6"]=residual_max<=1e-6
        checks[grid+"_water_conserved"]=water_drift<=1e-8
        checks[grid+"_all_200_measured_times"]=len(rows)==201 and all(
            c["samples"]==200 for c in metrics.values())
    success=all(checks.values())
    evidence={
        "phase":"P2",
        "milestone":"native_3d_mac_ghost_fluid_pcg_pressure_convergence_vs_marin",
        "numerical_partial_accepted":success,
        "real_pressure_physically_validated":False,
        "real_water_height_physically_validated":False,
        "complete_3d_fluid_phase":False,
        "phase_complete":False,
        "full_6_second_record_completed":False,
        "two_spatial_grids_completed":require_fine,
        "original_experiment_archive_sha256":source["source_zip_sha256"],
        "source_sha256":{
            "include/upc/mac3d.hpp":sha(CORE/"include"/"upc"/"mac3d.hpp"),
            "include/upc/coupled_water3d.hpp":sha(CORE/"include"/"upc"/"coupled_water3d.hpp"),
            "benchmarks/p2_marin_dambreak_heights.cpp":sha(
                CORE/"benchmarks"/"p2_marin_dambreak_heights.cpp"),
            "scripts/p2_marin_pcg_compare.py":sha(CORE/"scripts"/"p2_marin_pcg_compare.py"),
        },
        "checks":checks,
        "grids":result,
        "known_unmet":[
            "Measured pressure gauge RMSE and transducer spatial resolution",
            "Measured H1-H4 height RMSE <= .05m all probes",
            "Stress-consistent real water/air free surface and wall traction",
            "Full >=6s window and three-grid independent convergence",
            "Dynamic bidirectional fluid/rigid-solid impulse and global energy closure",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    OUTPUT.write_text(json.dumps(evidence,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print("P2_MAC_PCG_NUMERICAL_PARTIAL="+str(success)+
          " EXPERIMENTAL_CFD_VALIDATED=False P2_COMPLETE=False")
    for grid,val in result.items():
        print(grid,"max_divergence_s_inv",val["maximum_real_wet_projection_divergence_s_inv"],
              "water_drift_m3",val["maximum_volume_drift_m3"])
        for name,metric in val["metrics"].items():
            print(name,"PCG_RMSE",f"{metric['rmse_pcg']:.7g}",
                  "GS_RMSE",f"{metric['rmse_gauss_seidel']:.7g}",
                  "improved",metric["rmse_improved"])
    if not success:raise SystemExit(1)
    return evidence

if __name__=="__main__":
    if sys.argv[1:] not in ([],["--fine"]):
        raise SystemExit("Only optional --fine mode")
    evaluate(require_fine=sys.argv[1:]==["--fine"])
