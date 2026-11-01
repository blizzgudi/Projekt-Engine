"""P2 PARTIAL: quantify opt-in saturated-cell throughflow against SPHERIC 02.

Never upgrade a numerical improvement or benchmark execution to full P2 PASS.
"""
from __future__ import annotations
import hashlib
import json
import math
from datetime import datetime,timezone
from pathlib import Path
import p2_spheric_marin_compare as base

RUNS=base.RUNS
NATIVE=RUNS/"p2_marin_throughflow_200steps.txt"
REFERENCE=RUNS/"p2_spheric_marin_cfd_comparison_fail.json"

def sha(p:Path)->str:return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    if not NATIVE.is_file() or not REFERENCE.is_file():
        raise RuntimeError("Missing measured baseline or opt-in native log")
    original=base.NATIVE
    try:
        base.NATIVE=NATIVE
        measured,sim,ref=base.load()
    finally:
        base.NATIVE=original
    first=NATIVE.read_bytes()
    decoded=first.decode("utf-16" if first.startswith(bytes((255,254))) else "utf-8-sig")
    if "transport=throughflow" not in decoded:
        raise RuntimeError("Native solver did not enable opt-in throughflow")
    legacy=json.loads(REFERENCE.read_text(encoding="utf-8"))
    if legacy["experimental_cfd_validated"] is not False or legacy["phase_complete"] is not False:
        raise RuntimeError("Legacy benchmark full-phase flags unsafe")
    metrics={}
    for h in base.HEIGHTS:
        residuals=[row[h]-base.lerp(measured,row["time_s"],h) for row in sim[1:]]
        metrics[h]={
            "rmse_m":math.sqrt(sum(r*r for r in residuals)/len(residuals)),
            "mae_m":sum(abs(r) for r in residuals)/len(residuals),
            "legacy_rmse_m":legacy["metrics"][h]["rmse_m"],
        }
        metrics[h]["rmse_change_m"]=metrics[h]["rmse_m"]-metrics[h]["legacy_rmse_m"]
    improved_count=sum(x["rmse_change_m"]<0 for x in metrics.values())
    initial=sim[0]["water_volume_m3"]
    volume_error=max(abs(row["water_volume_m3"]-initial) for row in sim)
    if volume_error>1e-8:
        raise RuntimeError("Throughflow native mass conservation failed")
    report={
        "phase":"P2",
        "milestone":"spheric_marin_optin_throughflow_vs_legacy",
        "throughflow_executed":True,
        "experimental_cfd_validated":False,
        "phase_complete":False,
        "improved_probe_count":improved_count,
        "all_height_rmse_within_0.05m":all(x["rmse_m"]<=.05 for x in metrics.values()),
        "metrics":metrics,
        "max_volume_drift_m3":volume_error,
        "native_log_sha256":sha(NATIVE),
        "legacy_report_sha256":sha(REFERENCE),
        "source_sha256":{
            "include/upc/p2_vof_throughflow3d.hpp":sha(base.CORE/"include"/"upc"/"p2_vof_throughflow3d.hpp"),
            "include/upc/coupled_water3d.hpp":sha(base.CORE/"include"/"upc"/"coupled_water3d.hpp"),
            "benchmarks/p2_marin_dambreak_heights.cpp":sha(base.CORE/"benchmarks"/"p2_marin_dambreak_heights.cpp"),
        },
        "blockers":[
            "quantitative_height_rmse_not_all_within_0.05m",
            "pressure_sensors_not_modeled",
            "6s_experimental_window_not_simulated",
            "only_one_coarse_3d_grid",
            "free_surface_stress_and_dynamic_rigid_FSI_not_validated",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    target=RUNS/"p2_spheric_marin_throughflow_comparison.json"
    target.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("THROUGHFLOW_MARIN_COMPARISON=True VALIDATED=False "
          "IMPROVED_PROBES=",improved_count,"RMSE:",
          " ".join(f"{k}:{x['rmse_m']:.6g}" for k,x in metrics.items()))

if __name__=="__main__":
    main()
