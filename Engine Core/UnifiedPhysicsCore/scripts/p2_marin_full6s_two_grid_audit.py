r"""P2 partial: real independent MARIN Test-02 6s water heights + pressure.

Run BOTH identical-date MSVC builds of the UPC native 3D MAC-PCG solver
at 0.1 m and 0.05 m over 600 native steps. This script only audits already
finished real native traces; it does not simulate, modify source data or
assert physical validation. Laboratory XLS/CSV provenance is SHA-checked.
A green numerical integrity gate is NOT a P2 full CFD/FSI gate.
"""
from __future__ import annotations
import csv
import hashlib
import io
import json
import math
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
import p2_spheric_marin_compare as official

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
LOGS={
    "coarse":RUNS/"p2_marin_pcg_coarse600_threegrid.txt",
    "fine":RUNS/"p2_marin_pcg_fine600_threegrid.txt",
}
SPEC={
    "coarse":(32,10,10,.1),
    "fine":(64,20,20,.05),
}
ROWS=["time_s","H1_m","H2_m","H3_m","H4_m",
      "water_volume_m3","max_projection_divergence_s_inv",
      "P1_Pa","P2_Pa","P3_Pa","P4_Pa","P5_Pa","P6_Pa","P7_Pa","P8_Pa",
      "pressure_probe_wet_count"]
PROBES=[f"H{k}_m" for k in range(1,5)]+[f"P{k}_Pa" for k in range(1,9)]
TARGET_HEIGHT=.05
SOURCE_FILES=[
    "benchmarks/p2_marin_dambreak_heights.cpp",
    "include/upc/coupled_water3d.hpp",
    "include/upc/mac3d.hpp",
    "include/upc/p2_marin_wall_pressure_interp3d.hpp",
    "scripts/p2_marin_full6s_two_grid_audit.py",
    "tests/p2_marin_full6s_two_grid_regression.py",
    "docs/P2_MARIN_FULL6S_TWO_GRID_PARTIAL.md",
]
REPORT=RUNS/"p2_marin_pcg_full6s_two_grid_partial.json"
EXPECTED_FOOTER="PASS MARIN_COARSE_BASELINE partial_only=true steps=600 phase_complete=false"
SOURCE_HASH="a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1"

def digest(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def parse(raw:str,grid:str)->list[dict[str,float]]:
    if grid not in SPEC:raise ValueError("Unexpected experiment grid")
    lines=raw.replace("\r\n","\n").splitlines()
    if len(lines)!=604 or not lines[0].startswith("# MARIN_COARSE_3D ") or (
       lines[-1]!=EXPECTED_FOOTER):
        raise ValueError("Native full six-second MARIN process did not finish")
    nx,ny,nz,dx=SPEC[grid]
    header=lines[0]
    tokens=[
        f"dx_m={dx:.17g}",f"nx={nx} ny={ny} nz={nz}",
        "transport=throughflow",f"grid={grid}",
        "sampling=dense","pressure_solver=pcg",
        "obstacle_x_m=[0.6635,0.8245]",
        "obstacle_y_m=[0.295,0.698]",
        "obstacle_z_m=[0,0.161]",
        "probe_x_m=[0.496,0.992,1.488,2.632]",
        "pressure_sensors_NOT_simulated=false",
        "pressure_proxy_only=interpolated_tangent",
    ]
    if not all(token in header for token in tokens):
        raise ValueError("Unverified 3D geometry, sensor layout or pressure solver")
    if "height_sampling=" in header or "pressure_sampling=" in header:
        raise ValueError("Mixed pressure/height gauge mode not benchmark controlled")
    csv_reader=csv.DictReader(io.StringIO("\n".join(lines[1:-1])))
    if csv_reader.fieldnames!=ROWS:raise ValueError("Native channels reordered")
    records=[]
    for i,row in enumerate(csv_reader):
        data={key:official.read_float(row[key]) for key in ROWS}
        if abs(data["time_s"]-i*.01)>1e-10:
            raise ValueError("Native time window discontinuous")
        if not (0<=data["pressure_probe_wet_count"]<=8 and
                int(data["pressure_probe_wet_count"])==data["pressure_probe_wet_count"]):
            raise ValueError("Invalid 3D wall transducer mask")
        if not (0<=data["max_projection_divergence_s_inv"]<=1e-6):
            raise ValueError("Actual 3D MAC-PCG pressure not converged")
        if any(abs(data[ch])>1e8 for ch in PROBES):
            raise ValueError("Nonfinite/overflow native height or pressure")
        records.append(data)
    if len(records)!=601 or abs(records[-1]["time_s"]-6)>1e-10:
        raise ValueError("Incomplete native 601-state six-second trajectory")
    initial=records[0]["water_volume_m3"]
    if abs(initial-.6644)>1e-9:
        raise ValueError("Original 3.20m tank initial reservoir changed")
    if max(abs(r["water_volume_m3"]-initial) for r in records)>1e-8:
        raise ValueError("Physical water mass conservation failed")
    return records

def regression_negatives(raw:str,grid:str)->dict[str,bool]:
    lines=raw.replace("\r\n","\n").splitlines()
    variants={
        "unfinished_native_run":"\n".join(lines[:-4])+"\n",
        "forged_FULL_P2_PASS":raw.replace("phase_complete=false","phase_complete=true",1),
        "wrong_pressure_solver":raw.replace("pressure_solver=pcg","pressure_solver=gauss_seidel",1),
        "wrong_obstacle_geometry":raw.replace("obstacle_x_m=[0.6635,0.8245]","obstacle_x_m=[0.65,0.83]",1),
        "wrong_p1_to_p8_probe_kind":raw.replace("pressure_proxy_only=interpolated_tangent","pressure_proxy_only=none",1),
        "missing_time":_change(lines,4,0,"3.8"),
        "nonfinite_height":_change(lines,10,1,"nan"),
        "nonfinite_pressure":_change(lines,10,8,"inf"),
        "false_volume_drift":_change(lines,15,5,"0.5"),
        "pressure_divergence":_change(lines,16,6,"0.5"),
        "wet_count_invalid":_change(lines,16,15,"9"),
        "changed_grid":raw.replace(f"grid={grid}",f"grid={'fine' if grid=='coarse' else 'coarse'}",1),
    }
    verified={}
    for name,change in variants.items():
        if change==raw:raise RuntimeError("Test mutation failed to alter source: "+name)
        try:parse(change,grid)
        except (ValueError,KeyError,TypeError):verified[name]=True
        else:raise RuntimeError("FAKE native 3D experimental evidence accepted: "+name)
    return verified

def _change(lines:list[str],line:int,col:int,value:str)->str:
    modified=lines.copy()
    cols=modified[line].split(",")
    cols[col]=value
    modified[line]=",".join(cols)
    return "\n".join(modified)+"\n"

def compare() -> dict:
    originals,_,metadata=official.load()
    if metadata["source_zip_sha256"]!=SOURCE_HASH:
        raise RuntimeError("Original MARIN/SPHERIC 12-channel dataset not verified")
    result,checks={},{}
    grids={}
    for name,path in LOGS.items():
        raw=path.read_bytes()
        content=raw.decode("utf-16" if raw.startswith(b"\xff\xfe")
                           else "utf-8-sig")
        values=parse(content,name)
        negative=regression_negatives(content,name)
        metrics={}
        for ch in PROBES:
            delta=[v[ch]-official.lerp(originals,v["time_s"],ch)
                   for v in values[1:]]
            metrics[ch]={
                "rmse":math.sqrt(sum(x*x for x in delta)/len(delta)),
                "mae":sum(abs(x) for x in delta)/len(delta),
                "bias":sum(delta)/len(delta),
                "n":len(delta),"units":"m" if ch.startswith("H") else "Pa",
            }
        mass_before=values[0]["water_volume_m3"]
        grids[name]={
            "native_trace_sha256":digest(path),
            "3D_shape":list(SPEC[name][:3]),
            "dx_m":SPEC[name][3],
            "dt_s":.01,
            "seconds":6.0,
            "state_count":len(values),
            "max_volume_drift_m3":max(
                abs(v["water_volume_m3"]-mass_before) for v in values),
            "max_projected_divergence_s_inv":max(
                v["max_projection_divergence_s_inv"] for v in values),
            "wet_probe_count_start":values[0]["pressure_probe_wet_count"],
            "wet_probe_count_final":values[-1]["pressure_probe_wet_count"],
            "metrics":metrics,
            "negative_mutations_rejected":negative,
        }
        checks[f"{name}_6seconds_601_records_12channels"]=(
            all(v["n"]==600 for v in metrics.values()) and
            len(values)==601 and values[-1]["time_s"]==6)
        checks[f"{name}_native_water_mass_conserved"]=(
            grids[name]["max_volume_drift_m3"]<=1e-8)
        checks[f"{name}_wet_pcG_divergence_pass"]=(
            grids[name]["max_projected_divergence_s_inv"]<=1e-6)
        checks[f"{name}_negative_provenance_mutations"]=all(negative.values())
    height_improvement={
        name:grids["fine"]["metrics"][name]["rmse"]<
             grids["coarse"]["metrics"][name]["rmse"]
        for name in PROBES[:4]
    }
    pressure_improvement={
        name:grids["fine"]["metrics"][name]["rmse"]<
             grids["coarse"]["metrics"][name]["rmse"]
        for name in PROBES[4:]
    }
    height_pass={
        name:grids["fine"]["metrics"][name]["rmse"]<=TARGET_HEIGHT
        for name in PROBES[:4]
    }
    numerical=all(checks.values())
    physical=False
    report={
        "phase":"P2",
        "milestone":"full_6s_native_two_spatial_grid_marin_12_channels",
        "numerical_partial_accepted":numerical,
        "physical_full_p2_validated":physical,
        "phase_complete":False,
        "true_CFD_MARIN_measured_validation":False,
        "six_second_window_both_grids_completed":True,
        "three_spatial_grids_completed":False,
        "pressure_transducer_footprint_calibrated":False,
        "dynamic_two_way_FSI_validated":False,
        "original_experimental_zip_sha256":SOURCE_HASH,
        "original_measured_csv_sha256":digest(official.MEASURED),
        "native_binary_sha256":digest(CORE/"build"/"p2_marin_dambreak_heights.exe"),
        "source_sha256":{source:digest(CORE/source) for source in SOURCE_FILES},
        "checks":checks,
        "grids":grids,
        "heights_improve_on_refinement":height_improvement,
        "pressure_rmse_improve_on_refinement":pressure_improvement,
        "fine_heights_within_0_05m":height_pass,
        "all_fine_heights_physical_pass":all(height_pass.values()),
        "all_heights_improve":all(height_improvement.values()),
        "unmet":[
            "H1-H4 <=0.05m measured RMSE across experiment",
            "P1-P8 physically validated true 3D wall pressure/transducer force",
            ">=3 independent spatial grids with numerical asymptotic convergence",
            "stress-consistent moving free-water boundary, energy/impulse",
            "bidirectional dynamic 3D fluid/rigid-body coupling",
            "full Unreal native E2E for current coupled physics path",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",
                      encoding="utf-8")
    print("P2_MARIN_6S_TWO_GRID_NATIVE_NUMERICAL="+str(numerical)+
          " PHYSICAL_VALIDATED=False FULL_P2=False")
    print("REFINEMENT_HEIGHTS_ALL_IMPROVED="+str(report["all_heights_improve"]))
    for ch in PROBES:
        print(ch,"RMSE coarse",f"{grids['coarse']['metrics'][ch]['rmse']:.7g}",
              "fine",f"{grids['fine']['metrics'][ch]['rmse']:.7g}",
              grids["fine"]["metrics"][ch]["units"])
    if not numerical:raise SystemExit(1)
    return report

if __name__=="__main__":
    if sys.argv[1:]:
        raise SystemExit("Read-only audit of two complete six-second native traces")
    compare()
