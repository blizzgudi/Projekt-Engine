"""Fail-closed MARIN/SPHERIC Test-02 vs native UPC 3D water height audit.

Never interpret 'benchmark executed' as 'CFD experimentally validated'.
Simulated 3D pressure sensor traces are absent; time horizon is too short.
"""
from __future__ import annotations
import csv
import hashlib
import json
import math
from datetime import datetime,timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
REF=CORE/"references"
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
MEASURED=REF/"SPHERIC_Test02_measured.csv"
NATIVE=RUNS/"p2_marin_baseline_200steps.txt"
SOURCE_REPORT=RUNS/"p2_spheric_marin_measured_ingestion.json"
HEIGHTS=[f"H{i}_m" for i in range(1,5)]
SIM_COLUMNS=["time_s",*HEIGHTS,"water_volume_m3","max_projection_divergence_s_inv"]

def sha(path:Path)->str: return hashlib.sha256(path.read_bytes()).hexdigest()
def read_float(s:str)->float:
    n=float(s)
    if not math.isfinite(n):raise ValueError("Nonfinite measured or simulated value")
    return n
def load():
    integrity=json.loads(SOURCE_REPORT.read_text(encoding="utf-8"))
    if (integrity.get("data_integrity_accepted") is not True or
        integrity.get("source_zip_sha256")!="a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1" or
        sha(MEASURED)!=integrity.get("measurements_csv_sha256")):
        raise RuntimeError("Invalid pinned experimental source / CSV SHA256")
    with MEASURED.open(encoding="utf-8",newline="") as input_file:
        reader=csv.DictReader(input_file)
        if reader.fieldnames!=integrity["channels"]:
            raise RuntimeError("Measured 12-channel column mapping changed")
        raw=list(reader)
    if len(raw)!=7395:raise RuntimeError("Experimental data incomplete")
    measurements=[{k:read_float(row[k]) for k in reader.fieldnames} for row in raw]
    native_bytes=NATIVE.read_bytes()
    native_encoding="utf-16" if native_bytes.startswith(bytes((255,254))) else "utf-8-sig"
    lines=native_bytes.decode(native_encoding).splitlines()
    if (not lines or not lines[0].startswith("# MARIN_COARSE_3D dx_m=") or
        not lines[-1].startswith("PASS MARIN_COARSE_BASELINE partial_only=true steps=200 ")):
        raise RuntimeError("Native 3D solver process missing PASS or geometry")
    if ("nx=32 ny=10 nz=10" not in lines[0] or
        "obstacle_x_m=[0.6635,0.8245]" not in lines[0] or
        "obstacle_y_m=[0.295,0.698]" not in lines[0] or
        "probe_x_m=[0.496,0.992,1.488,2.632]" not in lines[0] or
        "pressure_sensors_NOT_simulated=true" not in lines[0]):
        raise RuntimeError("Native benchmark geometry or scope changed")
    rows=list(csv.DictReader(lines[1:-1]))
    if not rows or list(rows[0])!=SIM_COLUMNS:
        raise RuntimeError("Native 3D gauge column mapping invalid")
    simulation=[{k:read_float(row[k]) for k in SIM_COLUMNS} for row in rows]
    if not all(b["time_s"]>a["time_s"] for a,b in zip(simulation,simulation[1:])):
        raise RuntimeError("Non-monotonic simulation times")
    if not all(b["time_s"]>a["time_s"] for a,b in zip(measurements,measurements[1:])):
        raise RuntimeError("Non-monotonic measured times")
    return measurements,simulation,integrity

def lerp(data:list[dict],t:float,channel:str)->float:
    if not data[0]["time_s"]<=t<=data[-1]["time_s"]:
        raise ValueError("Requested time outside measured range")
    low,high=0,len(data)-1
    while low<high:
        mid=(low+high)//2
        if data[mid]["time_s"]<t:low=mid+1
        else:high=mid
    if low==0:return data[0][channel]
    a,b=data[low-1],data[low]
    f=(t-a["time_s"])/(b["time_s"]-a["time_s"])
    return a[channel]+f*(b[channel]-a[channel])

def main():
    measured,sim,source=load()
    initial=sim[0]
    if initial["time_s"]!=0 or abs(initial["H4_m"]-.55)>1e-10:
        raise RuntimeError("Bad MARIN dam-break initial state")
    if not 1.9<sim[-1]["time_s"]<=2.1:
        raise RuntimeError("Incomplete native 2-second trajectory")
    min_volume=max(abs(row["water_volume_m3"]-initial["water_volume_m3"]) for row in sim)
    if min_volume>1e-8:
        raise RuntimeError("Native water conservation failed")
    comparisons=[]
    for row in sim[1:]:
        t=row["time_s"]
        for h in HEIGHTS:
            ref=lerp(measured,t,h)
            comparisons.append({"time_s":t,"probe":h,
                                "experimental_m":ref,
                                "upc_m":row[h],
                                "signed_error_m":row[h]-ref})
    metrics={}
    for h in HEIGHTS:
        sub=[x for x in comparisons if x["probe"]==h]
        metrics[h]={
            "samples":len(sub),
            "mae_m":sum(abs(x["signed_error_m"]) for x in sub)/len(sub),
            "rmse_m":math.sqrt(sum(x["signed_error_m"]**2 for x in sub)/len(sub)),
            "max_abs_error_m":max(abs(x["signed_error_m"]) for x in sub),
        }
    # A priori benchmark limits (not retrofitted to results):
    # <=5 cm RMSE on all height probes and real pressure-trace agreement
    # across the full >=6 s MARIN campaign, at >=2 grid resolutions.
    height_criterion=all(r["rmse_m"]<=.05 for r in metrics.values())
    validation_fail_reasons=[]
    if not height_criterion:
        validation_fail_reasons.append("measured_4_probe_height_rmse_exceeds_0.05m")
    validation_fail_reasons.extend([
        "no_simulated_8_pressure_sensor_signals",
        "only_2.0s_simulation_vs_6s_required",
        "only_0.1m_grid_and_truncated_3.20m_tank_vs_3.22m_experiment",
        "binary_obstacle_approximation_vs_measured_0.161x0.403x0.161m",
        "actual_water_air_viscous_stress_boundary_not_validated",
    ])
    report={
        "phase":"P2",
        "milestone":"external_spheric_marin_3d_dambreak_coarse_comparison",
        "benchmark_executed":True,
        "data_integrity_accepted":True,
        "physical_water_mass_conserved":True,
        "experimental_cfd_validated":False,
        "phase_complete":False,
        "source_archive_sha256":source["source_zip_sha256"],
        "source_workbook_sha256":source["source_workbook_sha256"],
        "experimental_csv_sha256":sha(MEASURED),
        "upc_native_log_sha256":sha(NATIVE),
        "upc_source_sha256":sha(CORE/"benchmarks"/"p2_marin_dambreak_heights.cpp"),
        "actual_duration_s":sim[-1]["time_s"],
        "experimental_duration_s":source["time_range_s"][-1],
        "represented_dimensions_m":[3.2,1,1],
        "experiment_dimensions_m":[3.22,1,1],
        "original_water_volume_m3":initial["water_volume_m3"],
        "max_water_mass_volume_drift_m3":min_volume,
        "height_rmse_target_m":.05,
        "heights_meet_rmse_target":height_criterion,
        "metrics":metrics,
        "height_samples":comparisons,
        "validation_fail_reasons":validation_fail_reasons,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    output=RUNS/"p2_spheric_marin_cfd_comparison_fail.json"
    output.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(f"MARIN_COMPARISON_RUN=True PHYSICAL_VALIDATION=False "
          f"HEIGHT_RMSE_CRITERION={height_criterion} "
          f"WATER_VOLUME_DRIFT={min_volume:.4g} "
          f"BLOCKERS={len(validation_fail_reasons)}")
    for key,metric in metrics.items():
        print(key,"RMSE_m",f"{metric['rmse_m']:.7g}",
              "MAX_ABS_m",f"{metric['max_abs_error_m']:.7g}")

if __name__=="__main__":
    main()
