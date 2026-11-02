r"""P2 numerical PARTIAL: independent 600-step SPHERIC/MARIN Test02
height/pressure comparison on three real native 3D grid resolutions.

No numerical observation is a substitute for externally measured pressure,
validated interface/FSI physics or global energy/momentum. This audit
NEVER sets the full P2 gate true and never relaxes 0.05m height RMSE.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
import sys
from datetime import datetime,timezone
from pathlib import Path

import p2_spheric_marin_compare as reference
import p2_marin_full6s_two_grid_audit as native

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
REPORT=RUNS/"p2_marin_pcg_full6s_three_grid_partial.json"
GRIDS=("coarse","medium","fine")
FILES={
    name:RUNS/f"p2_marin_pcg_{name}600_threegrid.txt"
    for name in GRIDS
}
DX={"coarse":.1,"medium":3.2/48.,"fine":.05}
SHAPES={"coarse":(32,10,10),"medium":(48,15,15),"fine":(64,20,20)}
HEIGHTS=native.PROBES[:4]
PRESSURES=native.PROBES[4:]
EXPECTED_SHA="a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1"

def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def parse(raw:str,grid:str):
    if grid not in GRIDS:
        raise ValueError("Unrecognized three-grid geometry")
    if grid=="medium":
        # Reuse the same native time, mass, pressure and provenance checks
        # as the two established resolutions, with explicit third grid
        # shape and dx. Never alter the native file to spoof a coarse label.
        native.SPEC["medium"]=(*SHAPES["medium"],DX["medium"])
    return native.parse(raw,grid)

def metrics(records,experimental):
    out={}
    for key in native.PROBES:
        error=[r[key]-reference.lerp(experimental,r["time_s"],key)
               for r in records[1:]]
        out[key]={
            "rmse":math.sqrt(sum(v*v for v in error)/len(error)),
            "mae":sum(abs(v) for v in error)/len(error),
            "bias":sum(error)/len(error),
            "units":"m" if key.startswith("H") else "Pa",
            "samples":len(error),
        }
    return out

def displacement(a,b):
    return math.sqrt(sum((ra["water_volume_m3"]-rb["water_volume_m3"])**2
                          for ra,rb in zip(a,b))/len(a))

def compare():
    official,_,origin=reference.load()
    if origin["source_zip_sha256"]!=EXPECTED_SHA:
        raise RuntimeError("Official SPHERIC/MARIN experimental dataset changed")
    fields={}
    checks={}
    ctest_log=RUNS/"p2_marin_full6s_three_grid_ctest.txt"
    build_log=RUNS/"p2_marin_full6s_three_grid_rebuild.txt"
    if not ctest_log.is_file() or not build_log.is_file():
        raise RuntimeError("Real current MSVC build/CTest provenance missing")
    ctest_bytes=ctest_log.read_bytes()
    ctest_text=ctest_bytes.decode(
        "utf-16" if ctest_bytes[:2]==bytes((255,254)) else "utf-8-sig",
        errors="replace")
    ctest_match=re.search(r"100% tests passed, 0 tests failed out of (\d+)",
                          ctest_text)
    native_suites=int(ctest_match.group(1)) if ctest_match else 0
    if native_suites<150:
        raise RuntimeError("Actual full native CTest 150-suite regression missing")
    checks["native_150_suite_full_ctest_pass"]=True
    native_source=CORE/"benchmarks"/"p2_marin_dambreak_heights.cpp"
    binary=CORE/"build"/"p2_marin_dambreak_heights.exe"
    if not binary.is_file() or binary.stat().st_mtime_ns<native_source.stat().st_mtime_ns:
        raise RuntimeError("Actual 3D MARIN executable older than benchmark source")
    for grid in GRIDS:
        trace=FILES[grid]
        if not trace.is_file():
            raise RuntimeError(f"Independent native 6-second grid missing: {grid}")
        errlog=RUNS/f"p2_marin_pcg_{grid}600_threegrid_stderr.txt"
        if not errlog.is_file() or errlog.stat().st_size:
            raise RuntimeError(f"Native MARIN {grid} executable produced errors")
        raw=trace.read_text(encoding="utf-8-sig")
        sampled=parse(raw,grid)
        mutations=native.regression_negatives(raw,grid)
        if len(mutations)!=12 or not all(mutations.values()):
            raise RuntimeError("Fake native 3D measurement trace not rejected")
        stats=metrics(sampled,official)
        before=sampled[0]["water_volume_m3"]
        max_drift=max(abs(r["water_volume_m3"]-before) for r in sampled)
        divergence=max(r["max_projection_divergence_s_inv"] for r in sampled)
        fields[grid]={
            "shape":list(SHAPES[grid]),
            "cell_size_m":DX[grid],
            "native_trace_sha256":sha(trace),
            "water_initial_m3":before,
            "water_volume_max_drift_m3":max_drift,
            "max_wet_pressure_divergence_s_inv":divergence,
            "time_step_s":.01,
            "time_states":len(sampled),
            "t_stop_s":sampled[-1]["time_s"],
            "experimental_metrics":stats,
            "negative_source_mutations_rejected":list(mutations),
        }
        checks[f"{grid}_full_6s_601_real_native_3d_states"]=(
            len(sampled)==601 and abs(sampled[-1]["time_s"]-6)<1e-12)
        checks[f"{grid}_physical_mass_conservation"]=max_drift<=1e-8
        checks[f"{grid}_real_3d_pressure_projection"]=divergence<=1e-6
        checks[f"{grid}_twelve_pressure_height_channels"]=all(
            v["samples"]==600 for v in stats.values())
        checks[f"{grid}_twelve_forged_source_cases_rejected"]=len(mutations)==12
    fine_height_errors={
        key:fields["fine"]["experimental_metrics"][key]["rmse"]
        for key in HEIGHTS
    }
    fine_pressure_errors={
        key:fields["fine"]["experimental_metrics"][key]["rmse"]
        for key in PRESSURES
    }
    monotone_vs_measured={
        key:fields["coarse"]["experimental_metrics"][key]["rmse"]>
            fields["medium"]["experimental_metrics"][key]["rmse"]>
            fields["fine"]["experimental_metrics"][key]["rmse"]
        for key in native.PROBES
    }
    data={
        "phase":"P2",
        "milestone":"full_six_second_native_spatial_three_grid_vs_real_marin_12_channels",
        "numerical_partial_accepted":all(checks.values()),
        "complete_phase":False,
        "experimental_three_grid_cfd_validated":False,
        "stress_consistent_water_air_physics_validated":False,
        "bidirectional_dynamic_3d_FSI_validated":False,
        "global_energy_momentum_closed":False,
        "independent_native_spatial_grid_count":3,
        "native_simulation_seconds_each_grid":6.,
        "experimental_original_archive_sha256":origin["source_zip_sha256"],
        "experimental_original_csv_sha256":sha(reference.MEASURED),
        "native_binary_sha256":sha(binary),
        "native_ctest_suites":native_suites,
        "full_native_ctest_sha256":sha(ctest_log),
        "native_build_log_sha256":sha(build_log),
        "benchmarked_source_sha256":{
            file:sha(CORE/file) for file in [
                "benchmarks/p2_marin_dambreak_heights.cpp",
                "include/upc/coupled_water3d.hpp",
                "include/upc/mac3d.hpp",
                "include/upc/p2_marin_wall_pressure_interp3d.hpp",
                "scripts/p2_marin_full6s_three_grid_audit.py",
                "scripts/p2_marin_full6s_two_grid_audit.py",
                "scripts/p2_spheric_marin_compare.py",
                "tests/p2_marin_full6s_three_grid_regression.py",
                "docs/P2_MARIN_FULL6S_THREE_GRID_PARTIAL.md",
            ]
        },
        "checks":checks,
        "grids":fields,
        "rmse_refinement_monotonic":monotone_vs_measured,
        "all_12_channels_improve_monotonically":all(monotone_vs_measured.values()),
        "full_six_second_height_rmse_target_m":.05,
        "fine_height_rmse_m":fine_height_errors,
        "fine_height_acceptance":{
            key:value<=.05 for key,value in fine_height_errors.items()},
        "fine_pressure_rmse_pa":fine_pressure_errors,
        "native_tests_pass_does_not_mean_physics_pass":True,
        "unmet":[
            "all four height probes H1-H4 reach predeclared <=0.05m RMSE over 6s",
            "P1-P8 calibrated resolved wall transducer physics/force and accurate measurement agreement",
            "three-grid true asymptotic order validation of physical observables",
            "stress-consistent moving sharp water/air interface and global energy/momentum",
            "fully dynamic bidirectional rigid/deformable 3D body-fluid FSI",
            "new full coupled UE5.8 native E2E",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(data,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print("P2_MARIN_NATIVE_THREE_GRID_6S="+str(data["numerical_partial_accepted"])+
          " INDEPENDENT_12_CHANNEL_LAB_VALIDATION=False PHASE_COMPLETE=False")
    for key in native.PROBES:
        print(key,"RMSE",*(f"{grid}={fields[grid]['experimental_metrics'][key]['rmse']:.8g}"
                          for grid in GRIDS),
              "units",fields["coarse"]["experimental_metrics"][key]["units"],
              "monotone",monotone_vs_measured[key])
    if not data["numerical_partial_accepted"]:
        raise SystemExit(1)
    return data

if __name__=="__main__":
    if sys.argv[1:]:
        raise SystemExit("Use no arguments; analyze ONLY real completed native 3D traces")
    compare()
