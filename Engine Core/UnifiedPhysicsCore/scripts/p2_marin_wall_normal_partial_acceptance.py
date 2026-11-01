"""P2 partial: audit real native wall-normal MARIN pressure probes.

One 2-second coarse-grid 3D MAC/PCG run vs pinned original native trace.
Does NOT certify measured 6-second convergence, FSI, or wall traction.
"""
from __future__ import annotations
import csv
import hashlib
import io
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path
import p2_spheric_marin_compare as measured

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
OLD=RUNS/"p2_marin_pcg_coarse200_dense.txt"
NEW=RUNS/"p2_marin_wall_normal_coarse200.txt"
REPORT=RUNS/"p2_marin_wall_normal_partial.json"
PRESSURE=[f"P{i}_Pa" for i in range(1,9)]
HEIGHT=[f"H{i}_m" for i in range(1,5)]
COMMON=["time_s",*HEIGHT,"water_volume_m3","max_projection_divergence_s_inv",
        *PRESSURE,"pressure_probe_wet_count"]
EXTRA=["pressure_normal_extrapolated_count","pressure_normal_fallback_count"]
ZIP_SHA="a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1"

def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def parse(raw:str,new:bool)->list[dict[str,float]]:
    lines=raw.splitlines()
    if len(lines)!=204 or not lines[0].startswith("# MARIN_COARSE_3D "):
        raise ValueError("Missing full 201-state native trace")
    header=lines[0]
    required=["nx=32 ny=10 nz=10","grid=coarse sampling=dense",
              "transport=throughflow","pressure_solver=pcg",
              "pressure_sensors_NOT_simulated=false"]
    required+=["pressure_proxy_only=wall_normal_extrap",
               "pressure_sampling=two_liquid_planes_fallback"] if new else [
               "pressure_proxy_only=interpolated_tangent"]
    if not all(s in header for s in required):
        raise ValueError("Native solver/probe provenance mismatch")
    if lines[-1]!="PASS MARIN_COARSE_BASELINE partial_only=true steps=200 phase_complete=false":
        raise ValueError("Missing explicit native partial-only footer")
    reader=csv.DictReader(io.StringIO("\n".join(lines[1:-1])))
    if reader.fieldnames!=COMMON+(EXTRA if new else []):
        raise ValueError("Native sensor schema changed")
    rows=[]
    for i,row in enumerate(reader):
        r={k:measured.read_float(row[k]) for k in reader.fieldnames}
        if abs(r["time_s"]-i*.01)>1e-10:
            raise ValueError("Native time grid invalid")
        if r["max_projection_divergence_s_inv"]<0 or r["max_projection_divergence_s_inv"]>1e-6:
            raise ValueError("Native projection residual too large")
        if any(not 0<=r[k]<=1 for k in HEIGHT):
            raise ValueError("Invalid height")
        for k in ["pressure_probe_wet_count",*(EXTRA if new else [])]:
            if r[k]!=int(r[k]) or not 0<=r[k]<=8:
                raise ValueError("Invalid sensor count")
        if new and r[EXTRA[0]]+r[EXTRA[1]]!=r["pressure_probe_wet_count"]:
            raise ValueError("Extrapolation/fallback census inconsistent")
        rows.append(r)
    if len(rows)!=201:
        raise ValueError("Incomplete native trajectory")
    if max(abs(r["water_volume_m3"]-rows[0]["water_volume_m3"]) for r in rows)>1e-8:
        raise ValueError("Native water volume drift")
    return rows

def corruptions(raw:str)->dict[str,bool]:
    lines=raw.splitlines()
    def edit(line:int,col:int,value:str)->str:
        tmp=lines.copy()
        fields=tmp[line].split(",")
        fields[col]=value
        tmp[line]=",".join(fields)
        return "\n".join(tmp)+"\n"
    variants={
        "truncated":"\n".join(lines[:-2])+"\n",
        "wrong_pressure_mode":raw.replace("pressure_proxy_only=wall_normal_extrap",
                                           "pressure_proxy_only=interpolated_tangent",1),
        "wrong_solver":raw.replace("pressure_solver=pcg","pressure_solver=gauss_seidel",1),
        "nonfinite_pressure":edit(4,7,"nan"),
        "bad_extrapolation_census":edit(4,16,"9"),
        "bad_time":edit(7,0,"0.99"),
    }
    out={}
    for k,variant in variants.items():
        try:parse(variant,True)
        except (ValueError,TypeError,KeyError):out[k]=True
        else:out[k]=False
    return out

def evaluate()->dict:
    original,_,source=measured.load()
    if source["source_zip_sha256"]!=ZIP_SHA:
        raise ValueError("Original SPHERIC archive fingerprint mismatch")
    raw=NEW.read_text(encoding="utf-8-sig")
    new=parse(raw,True)
    old=parse(OLD.read_text(encoding="utf-8-sig"),False)
    negatives=corruptions(raw)
    if not all(negatives.values()):
        raise ValueError("Mutation rejection incomplete")
    if original[0]["time_s"]>new[1]["time_s"] or original[-1]["time_s"]<new[-1]["time_s"]:
        raise ValueError("Experiment does not cover native times")
    untouched=HEIGHT+["time_s","water_volume_m3",
                       "max_projection_divergence_s_inv","pressure_probe_wet_count"]
    invariant={k:max(abs(a[k]-b[k]) for a,b in zip(new,old)) for k in untouched}
    if any(v>1e-10 for v in invariant.values()):
        raise ValueError("Pressure-only diagnostic changed native flow signals")
    pressure_delta={k:max(abs(a[k]-b[k]) for a,b in zip(new,old)) for k in PRESSURE}
    if not any(v>1e-8 for v in pressure_delta.values()):
        raise ValueError("No wall-normal reconstruction observable")
    metrics={}
    for k in PRESSURE:
        old_errors=[r[k]-measured.lerp(original,r["time_s"],k) for r in old[1:]]
        new_errors=[r[k]-measured.lerp(original,r["time_s"],k) for r in new[1:]]
        metrics[k]={"old_rmse_pa":math.sqrt(sum(e*e for e in old_errors)/len(old_errors)),
                    "new_rmse_pa":math.sqrt(sum(e*e for e in new_errors)/len(new_errors)),
                    "samples":len(new_errors)}
    native=RUNS/"p2_marin_wall_normal_native.txt"
    ctest=RUNS/"p2_marin_wall_normal_ctest.txt"
    build=RUNS/"p2_marin_wall_normal_build.txt"
    ntext=native.read_text(encoding="utf-8")
    match=re.search(r"PASS P2_MARIN_WALL_NORMAL assertions=(\d+) phase_complete=false",ntext)
    if not match or int(match.group(1))<60:
        raise ValueError("Native manufactured/live pressure tests not proven")
    ctext=ctest.read_text(encoding="utf-8")
    count=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctext)
    if not count or int(count.group(1))<119:
        raise ValueError("Complete native CTest suite missing")
    if "error" in build.read_text(encoding="utf-8").lower():
        raise ValueError("Build output contains error")
    sources=[
        "CMakeLists.txt",
        "benchmarks/p2_marin_dambreak_heights.cpp",
        "include/upc/p2_marin_wall_normal_pressure3d.hpp",
        "include/upc/p2_marin_pressure_sensors3d.hpp",
        "tests/p2_marin_wall_normal_pressure3d_tests.cpp",
        "scripts/p2_marin_wall_normal_build.cmd",
        "scripts/p2_marin_wall_normal_partial_acceptance.py",
    ]
    report={
        "phase":"P2","milestone":"marin_wall_normal_two_plane_native_pcg_2s_partial",
        "phase_complete":False,"partial_numerical_integrity":True,
        "measured_pressure_validated":all(m["new_rmse_pa"]<=500 for m in metrics.values()),
        "full_6s_experimental_pressure_convergence":False,
        "independent_three_grid_convergence":False,
        "stress_consistent_water_air_interface":False,
        "dynamic_bidirectional_fsi":False,
        "physical_global_impulse_energy_closure":False,
        "native_2s_states":len(new),"duration_s":new[-1]["time_s"],
        "native_assertions":int(match.group(1)),"native_ctest_suites":int(count.group(1)),
        "original_experiment_archive_sha256":ZIP_SHA,
        "measured_csv_sha256":sha(measured.MEASURED),
        "old_trace_sha256":sha(OLD),"new_trace_sha256":sha(NEW),
        "native_test_exe_sha256":sha(CORE/"build/upc_p2_marin_wall_normal_tests.exe"),
        "benchmark_exe_sha256":sha(CORE/"build/p2_marin_wall_normal_benchmark.exe"),
        "native_log_sha256":sha(native),"ctest_log_sha256":sha(ctest),
        "build_log_sha256":sha(build),
        "source_sha256":{name:sha(CORE/name) for name in sources},
        "unchanged_solver_signal_max_abs":invariant,
        "max_pressure_change_pa":pressure_delta,
        "extrapolation_counts":{
            "min":min(r[EXTRA[0]] for r in new),
            "max":max(r[EXTRA[0]] for r in new),
            "fallback_max":max(r[EXTRA[1]] for r in new),
        },
        "negative_trace_mutations_rejected":negatives,
        "measured_metrics":metrics,
        "limitations":[
            "Wall-normal linear extrapolation is not resolved wall traction or fluid-solid coupling",
            "Binary obstacle voxelization and one-phase air boundary remain unchanged",
            "Only 2 s coarse native trajectory; full >=6 s multi-grid convergence not demonstrated",
            "P1-P8 measured pressure errors are retained even when above thresholds",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print("P2_WALL_NORMAL_PARTIAL=True P2_FULL_ACCEPTANCE=False")
    for k,m in metrics.items():
        print(k,"old_rmse_pa",round(m["old_rmse_pa"],4),
              "new_rmse_pa",round(m["new_rmse_pa"],4))
    return report

if __name__=="__main__":
    evaluate()
