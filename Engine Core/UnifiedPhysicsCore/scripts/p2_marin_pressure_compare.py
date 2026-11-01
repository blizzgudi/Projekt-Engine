"""P2 PARTIAL: compare real native MAC 3D pressure-cell readbacks at eight
MARIN/SPHERIC Test 02 box transducer locations against measured pressure.

This deliberately reports FAILED physical agreement/limitations and does
NOT claim pressure validation: a grid cell-centre pressure is NOT a
wall-transducer traction or impulse with subcell stress reconstruction.
"""
from __future__ import annotations

import csv
import hashlib
import json
import math
from datetime import datetime,timezone
from pathlib import Path
import p2_spheric_marin_compare as ref

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
OBS=RUNS/"p2_marin_pressure_coarse200_dense.txt"
OBS_FINE=RUNS/"p2_marin_pressure_fine200_dense.txt"
REPORT=RUNS/"p2_marin_pressure_coarse_measured_partial.json"
GAUGES=[f"P{i}_Pa" for i in range(1,9)]
FIELDS=["time_s","H1_m","H2_m","H3_m","H4_m",
        "water_volume_m3","max_projection_divergence_s_inv",*GAUGES,
        "pressure_probe_wet_count"]

def digest(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def load_native(path=OBS,grid="coarse"):
    raw=Path(path).read_bytes()
    text=raw.decode("utf-16" if raw.startswith(bytes((255,254))) else "utf-8-sig")
    lines=text.splitlines()
    if len(lines)!=204 or not lines[0].startswith("# MARIN_COARSE_3D "):
        raise ValueError("Invalid pressure 3D native trace length/header")
    if grid not in ("coarse","fine"):
        raise ValueError("Unknown MARIN pressure grid")
    shape="nx=32 ny=10 nz=10" if grid=="coarse" else "nx=64 ny=20 nz=20"
    required=[
        shape,
        f"transport=throughflow grid={grid} sampling=dense",
        "obstacle_x_m=[0.6635,0.8245]",
        "pressure_sensors_NOT_simulated=false",
        "pressure_proxy_only=true",
    ]
    if not all(token in lines[0] for token in required):
        raise ValueError("Native gauge geometry/proxy provenance mismatch")
    if lines[-1]!="PASS MARIN_COARSE_BASELINE partial_only=true steps=200 phase_complete=false":
        raise ValueError("Native pressure benchmark process not successful")
    reader=csv.DictReader(lines[1:-1])
    if reader.fieldnames!=FIELDS:
        raise ValueError("Native 3D pressure gauge column order mismatch")
    rows=[{key:ref.read_float(row[key]) for key in FIELDS} for row in reader]
    if len(rows)!=201:
        raise ValueError("Native 3D pressure gauge time series truncated")
    for t,rec in enumerate(rows):
        if abs(rec["time_s"]-t*.01)>1e-10:
            raise ValueError("Native time step missing, duplicated or reordered")
        wet=rec["pressure_probe_wet_count"]
        if wet!=int(wet) or wet<0 or wet>8:
            raise ValueError("Impossible wet gauge state")
        if rec["water_volume_m3"]<0:
            raise ValueError("Impossible native water volume")
        if max(abs(rec[k]) for k in GAUGES)>1e8:
            raise ValueError("Pressure gauge exceeds plausible numeric bound")
    if any(abs(row["water_volume_m3"]-rows[0]["water_volume_m3"])>1e-8
           for row in rows):
        raise ValueError("Mass conservation violation in native trace")
    return rows

def compare(native_path=OBS,report_path=REPORT):
    experimental,_,evidence=ref.load()
    if evidence["source_zip_sha256"]!="a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1":
        raise ValueError("Experiment ZIP SHA256 mismatch")
    rows=load_native(native_path)
    fine=load_native(OBS_FINE,grid="fine")
    metrics={}
    for k in GAUGES:
        err=[row[k]-ref.lerp(experimental,row["time_s"],k) for row in rows[1:]]
        measurements=[ref.lerp(experimental,row["time_s"],k) for row in rows[1:]]
        metrics[k]={
            "rmse_pa":math.sqrt(sum(x*x for x in err)/len(err)),
            "mae_pa":sum(abs(x) for x in err)/len(err),
            "signed_bias_pa":sum(err)/len(err),
            "max_absolute_error_pa":max(abs(x) for x in err),
            "measured_peak_abs_pa":max(abs(x) for x in measurements),
            "native_peak_abs_pa":max(abs(row[k]) for row in rows[1:]),
            "sample_count":len(err),
        }
    fine_metrics={}
    for k in GAUGES:
        err=[row[k]-ref.lerp(experimental,row["time_s"],k) for row in fine[1:]]
        fine_metrics[k]={
            "rmse_pa":math.sqrt(sum(x*x for x in err)/len(err)),
            "mae_pa":sum(abs(x) for x in err)/len(err),
            "signed_bias_pa":sum(err)/len(err),
            "max_absolute_error_pa":max(abs(x) for x in err),
            "native_peak_abs_pa":max(abs(row[k]) for row in fine[1:]),
            "sample_count":len(err),
        }
    # The coarse 0.1 m grid demonstrably aliases P1/P2 (z) and P6/P7
    # (x). It cannot resolve all eight independent laboratory footprints.
    aliases={"P1_P2":all(a["P1_Pa"]==a["P2_Pa"] for a in rows),
             "P6_P7":all(a["P6_Pa"]==a["P7_Pa"] for a in rows)}
    fine_alias={"P3_P4":all(a["P3_Pa"]==a["P4_Pa"] for a in fine)}
    all_finite=all(math.isfinite(v["rmse_pa"]) and
                   math.isfinite(v["native_peak_abs_pa"]) for v in metrics.values())
    if not all_finite or not all(aliases.values()) or not all(fine_alias.values()):
        raise ValueError("Unexpected nonfinite pressure or unrecognized sensor alias")
    result={
        "phase":"P2","milestone":"independent_marin_eight_native_pressure_trace_comparison",
        "native_8_pressure_traces_executed":True,
        "native_8_pressure_traces_fine_executed":True,
        "fine_versus_coarse_rmse_improved":{
            k:fine_metrics[k]["rmse_pa"]<metrics[k]["rmse_pa"] for k in GAUGES},
        "official_measurement_channels":GAUGES,
        "physical_pressure_validation":False,
        "stress_consistent_wall_pressure":False,
        "full_3d_fluid_pressure_abnahme":False,
        "phase_complete":False,
        "source_sha256":{
            "native":digest(Path(native_path)),
            "native_fine":digest(OBS_FINE),
            "experimental_csv":digest(ref.MEASURED),
            "source_header":digest(CORE/"include"/"upc"/"p2_marin_pressure_sensors3d.hpp"),
            "native_benchmark":digest(CORE/"benchmarks"/"p2_marin_dambreak_heights.cpp"),
        },
        "native_time_samples":len(rows),
        "fine_native_time_samples":len(fine),
        "end_time_s":rows[-1]["time_s"],
        "wet_sensor_count_final":int(rows[-1]["pressure_probe_wet_count"]),
        "sensor_aliases_from_unresolved_coarse_grid":aliases,
        "sensor_aliases_from_unresolved_fine_grid":fine_alias,
        "metrics_pa":metrics,
        "fine_metrics_pa":fine_metrics,
        "known_deficiencies":[
            "box transducer pressure sampled from first liquid-side MAC cell centre, not wall traction",
            "coarse grid aliases P1-P2 and P6-P7; fine grid aliases P3-P4, so still not eight independent transducers",
            "2s comparison, external 6s requirement not met",
            "no calibrated pressure impulse/peak timing or two-way solid coupling",
            "3D free-surface stress and global energy/momentum closure not validated",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    Path(report_path).write_text(
        json.dumps(result,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print("MARIN_PRESSURE_8_NATIVE_CHANNELS=True "
          "EXPERIMENTALLY_VALIDATED=False FULL_PHASE=False "
          "ALIAS_COARSE=True ALIAS_FINE=True")
    for k,v in metrics.items():
        print(f"{k} COARSE_RMSE_PA={v['rmse_pa']:.7g} "
              f"FINE_RMSE_PA={fine_metrics[k]['rmse_pa']:.7g} "
              f"NATIVE_PEAK_PA={v['native_peak_abs_pa']:.7g} "
              f"MEASURED_PEAK_PA={v['measured_peak_abs_pa']:.7g}")
    return result

if __name__=="__main__":
    compare()
