"""P2 PARTIAL: compare actual native 3D MAC pressure tangential interpolation
against original MARIN/SPHERIC Test-02 P1-P8 (2s, 0.1m and 0.05m).
No pressure-stress validation, sub-grid geometry, or phase acceptance.
"""
from __future__ import annotations

import csv
import hashlib
import json
import math
from datetime import datetime,timezone
from pathlib import Path
import subprocess
import sys
import p2_spheric_marin_compare as ref
import p2_marin_pressure_compare as historical

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
BINARY=CORE/"build"/"p2_marin_dambreak_heights.exe"
COARSE=RUNS/"p2_marin_wall_interp_coarse200.txt"
FINE=RUNS/"p2_marin_wall_interp_fine200.txt"
REPORT=RUNS/"p2_marin_wall_interp_comparison_partial.json"
BASELINE=RUNS/"p2_marin_pressure_coarse_measured_partial.json"
GAUGES=[f"P{i}_Pa" for i in range(1,9)]
FIELDS=["time_s","H1_m","H2_m","H3_m","H4_m",
        "water_volume_m3","max_projection_divergence_s_inv",*GAUGES,
        "pressure_probe_wet_count"]
END="PASS MARIN_COARSE_BASELINE partial_only=true steps=200 phase_complete=false"
OFFICIAL_SHA="a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1"

def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def run_native():
    for grid,out,extra in [
        ("coarse",COARSE,["200","throughflow","dense","interp"]),
        ("fine",FINE,["200","throughflow","fine","dense","interp"]),
    ]:
        p=subprocess.run([str(BINARY),*extra],cwd=CORE,
                         capture_output=True,text=True,encoding="utf-8",
                         errors="replace",timeout=240)
        if p.returncode!=0 or p.stderr.strip() or not p.stdout.strip().endswith(END):
            raise RuntimeError("Native "+grid+" pressure interpolation run failed "+
                               p.stderr[-1000:]+p.stdout[-400:])
        out.write_text(p.stdout,encoding="utf-8",newline="\n")
        print("MARIN_WALL_INTERP_NATIVE_"+grid.upper()+"_200_STEPS_PASS",flush=True)

def load(path:Path,grid:str):
    raw=Path(path).read_bytes()
    content=raw.decode("utf-16" if raw.startswith(bytes((255,254))) else "utf-8-sig")
    lines=content.splitlines()
    if len(lines)!=204 or not lines[0].startswith("# MARIN_COARSE_3D "):
        raise ValueError("MARIN interpolated native trace length/header invalid")
    if grid not in ("coarse","fine"):
        raise ValueError("Unknown grid resolution")
    shape="nx=32 ny=10 nz=10" if grid=="coarse" else "nx=64 ny=20 nz=20"
    tokens=[shape,f"transport=throughflow grid={grid} sampling=dense",
            "pressure_sensors_NOT_simulated=false",
            "pressure_proxy_only=interpolated_tangent",
            "obstacle_x_m=[0.6635,0.8245]"]
    if not all(t in lines[0] for t in tokens):
        raise ValueError("Native interpolated pressure provenance mismatch")
    if lines[-1]!=END:
        raise ValueError("Native interpolated pressure execution not PASS")
    rows=list(csv.DictReader(lines[1:-1]))
    if not rows or list(rows[0])!=FIELDS:
        raise ValueError("Native interpolated pressure columns mismatch")
    parsed=[{k:ref.read_float(row[k]) for k in FIELDS} for row in rows]
    if len(parsed)!=201:
        raise ValueError("Native interpolated pressure row count mismatch")
    volume=parsed[0]["water_volume_m3"]
    for t,row in enumerate(parsed):
        if abs(row["time_s"]-t*.01)>1e-10:
            raise ValueError("Native interpolated pressure time mismatch")
        wet=row["pressure_probe_wet_count"]
        if wet!=int(wet) or wet<0 or wet>8:
            raise ValueError("Native interpolated pressure wet count invalid")
        if abs(row["water_volume_m3"]-volume)>1e-8:
            raise ValueError("Native interpolated pressure volume drift")
        if max(abs(row[k]) for k in GAUGES)>1e8:
            raise ValueError("Native interpolated pressure numeric overflow")
    return parsed

def verify_height_invariant(rows,prior):
    if len(prior)!=len(rows):
        raise ValueError("Historical native height comparison missing")
    invariant=max(
        abs(row[k]-old[k])
        for row,old in zip(rows,prior)
        for k in ["H1_m","H2_m","H3_m","H4_m","water_volume_m3"])
    if invariant>1e-12:
        raise ValueError("Pressure diagnostic altered native fluid height or volume")
    return invariant

def evaluate():
    experimental,_,source=ref.load()
    if source["source_zip_sha256"]!=OFFICIAL_SHA:
        raise ValueError("Official MARIN experimental source hash changed")
    base=json.loads(BASELINE.read_text(encoding="utf-8"))
    if not (base["phase_complete"] is False and
            base["physical_pressure_validation"] is False and
            len(base["metrics_pa"])==8 and len(base["fine_metrics_pa"])==8):
        raise ValueError("Previous native cell-centre baseline missing or misrepresented")
    grids={}
    for name,path in [("coarse",COARSE),("fine",FINE)]:
        rows=load(path,name)
        prior_path=historical.OBS if name=="coarse" else historical.OBS_FINE
        prior=historical.load_native(prior_path,grid=name)
        invariant=verify_height_invariant(rows,prior)
        heights={}
        for k in ("H1_m","H2_m","H3_m","H4_m"):
            err=[row[k]-ref.lerp(experimental,row["time_s"],k)
                 for row in rows[1:]]
            heights[k]=math.sqrt(sum(x*x for x in err)/len(err))
        metric={}
        for gauge in GAUGES:
            errors=[row[gauge]-ref.lerp(experimental,row["time_s"],gauge)
                    for row in rows[1:]]
            metric[gauge]={
                "rmse_pa":math.sqrt(sum(x*x for x in errors)/len(errors)),
                "mae_pa":sum(abs(x) for x in errors)/len(errors),
                "bias_pa":sum(errors)/len(errors),
                "native_peak_abs_pa":max(abs(row[gauge]) for row in rows[1:]),
                "samples":len(errors),
            }
        grids[name]={
            "rows":len(rows),
            "wet_final":int(rows[-1]["pressure_probe_wet_count"]),
            "rmse_pa":metric,
            "native_sha256":sha(path),
            "historical_native_sha256":sha(prior_path),
            "historical_height_volume_max_difference":invariant,
            "height_rmse_m":heights,
        }
    improvement={}
    for grid in ("coarse","fine"):
        baseline=base["metrics_pa"] if grid=="coarse" else base["fine_metrics_pa"]
        improvement[grid]={
            k:grids[grid]["rmse_pa"][k]["rmse_pa"]<baseline[k]["rmse_pa"]
            for k in GAUGES}
    result={
        "phase":"P2","milestone":"MARIN_P1_P8_wall_tangential_pressure_interpolation",
        "native_wall_tangent_reconstruction":True,
        "normal_wall_stress_reconstructed":False,
        "independent_spatial_resolution_of_transducers":False,
        "experimental_pressure_validation":False,
        "experimental_height_validation":False,
        "phase_complete":False,
        "original_experiment_sha256":OFFICIAL_SHA,
        "baseline_comparison_sha256":sha(BASELINE),
        "grids":grids,
        "improved_vs_nearest_cell_rmse":improvement,
        "fine_vs_coarse_rmse_improved":{
            k:grids["fine"]["rmse_pa"][k]["rmse_pa"]<
              grids["coarse"]["rmse_pa"][k]["rmse_pa"] for k in GAUGES},
        "source_sha256":{
            "header":sha(CORE/"include"/"upc"/"p2_marin_wall_pressure_interp3d.hpp"),
            "benchmark":sha(CORE/"benchmarks"/"p2_marin_dambreak_heights.cpp"),
            "experimental_csv":sha(ref.MEASURED),
            "binary":sha(BINARY),
        },
        "known_deficiencies":[
            "bilinear interpolation of fluid-side MAC cell pressure is not physical wall stress",
            "wall-normal derivative and exact cut-cell boundary geometry not reconstructed",
            "0.1m/0.05m grids cannot independently resolve 0.04m sensor spacing",
            "only 2s of required >=6s external measurement window simulated",
            "no independent dynamic FSI/energy closure or free-surface stress validation",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(result,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print("MARIN_INTERP_PRESSURE_PHYSICAL_PASS=False P2_COMPLETE=False")
    for g in ("coarse","fine"):
        print(g.upper()+" HEIGHT_RMSE_M="+",".join(
            f"{k}:{v:.6g}" for k,v in grids[g]["height_rmse_m"].items())+
            f" HISTORICAL_HEIGHT_DIFF={grids[g]['historical_height_volume_max_difference']:.3g}")
    for k in GAUGES:
        a=grids["coarse"]["rmse_pa"][k]["rmse_pa"]
        b=grids["fine"]["rmse_pa"][k]["rmse_pa"]
        print(f"{k} INTERP_COARSE_RMSE_PA={a:.6g} INTERP_FINE_RMSE_PA={b:.6g} "
              f"FINE_IMPROVED={result['fine_vs_coarse_rmse_improved'][k]}")
    return result

if __name__=="__main__":
    if sys.argv[1:] not in ([],["--run"]):
        raise SystemExit("Usage: p2_marin_wall_interp_compare.py [--run]")
    if sys.argv[1:]:run_native()
    evaluate()
