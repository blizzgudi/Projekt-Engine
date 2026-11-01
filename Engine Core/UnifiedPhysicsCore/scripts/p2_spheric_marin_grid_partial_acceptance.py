r"""P2 PARTIAL: measured MARIN 3D height benchmark at TWO native grid sizes.

Fail-closed evidence only. A green result is a bounded analysis milestone,
not a physical 3D-fluid/FSI full P2 acceptance.

Usage:
  python scripts/p2_spheric_marin_grid_partial_acceptance.py
  python scripts/p2_spheric_marin_grid_partial_acceptance.py --rebuild-and-rerun
The second mode compiles Win64/MSVC and repeats both 200-step native runs.
All writes, temporary files and build outputs stay under D:\Projekt Engine.
"""
from __future__ import annotations
import hashlib
import json
import os
import re
import subprocess
import sys
from datetime import datetime,timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
PROJECT=CORE.parents[1]
RUNS=PROJECT/"ENGINE TEST STUBE"/"runs"
BUILD=CORE/"build"
BINARY=BUILD/"p2_marin_dambreak_heights.exe"
VCVARS=Path(r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat")
SCRIPT=CORE/"scripts"/"p2_spheric_marin_grid_refinement.py"
TESTS=CORE/"tests"/"p2_spheric_marin_grid_refinement_regression.py"

def sha(file:Path)->str:
    return hashlib.sha256(file.read_bytes()).hexdigest()

def run(arguments:list[str],timeout:int=240)->str:
    env=os.environ.copy()
    local_temp=BUILD/"p2_marin_grid_temp"
    local_temp.mkdir(parents=True,exist_ok=True)
    env.update({"TEMP":str(local_temp),"TMP":str(local_temp),
                "PYTHONDONTWRITEBYTECODE":"1","PYTHONIOENCODING":"utf-8"})
    process=subprocess.run(arguments,cwd=CORE,env=env,capture_output=True,
                           text=True,encoding="utf-8",errors="replace",timeout=timeout)
    if process.returncode!=0:
        raise RuntimeError("Rejected nonzero native/test exit: "+
                           (process.stdout+process.stderr)[-2400:])
    return process.stdout+process.stderr

def rebuild_and_rerun():
    if not VCVARS.is_file():
        raise RuntimeError("VS native C++ toolchain unavailable")
    cmd=BUILD/"p2_marin_grid_native_build.cmd"
    cmd.write_text(
        "@echo off\n"
        f'call "{VCVARS}" >nul\n'
        "if errorlevel 1 exit /b 10\n"
        "cl /nologo /std:c++17 /EHsc /O2 /MD /Iinclude "
        "/Fobuild\\p2_marin_dambreak_heights.obj "
        "/Febuild\\p2_marin_dambreak_heights.exe "
        "benchmarks\\p2_marin_dambreak_heights.cpp\n"
        "exit /b %errorlevel%\n",encoding="ascii")
    result=run(["cmd.exe","/d","/c",str(cmd)],180)
    (RUNS/"p2_marin_grid_native_build.txt").write_text(result,encoding="utf-8")
    if not BINARY.is_file():
        raise RuntimeError("Native MSVC executable absent")
    jobs=[
        ("coarse",["200","throughflow","dense"],"p2_marin_throughflow_coarse200_dense.txt"),
        ("fine",["200","throughflow","fine","dense"],"p2_marin_throughflow_fine200_dense.txt"),
    ]
    for name,args,filename in jobs:
        result=run([str(BINARY),*args],240)
        if not result.rstrip().endswith(
            "PASS MARIN_COARSE_BASELINE partial_only=true steps=200 phase_complete=false"):
            raise RuntimeError(f"Native MARIN 3D {name} did not finish")
        (RUNS/filename).write_text(result,encoding="utf-8",newline="\n")
        print(f"MSVC_NATIVE_MARIN_{name.upper()}_200_STEPS=PASS",flush=True)

def main():
    RUNS.mkdir(parents=True,exist_ok=True)
    if sys.argv[1:] not in ([],["--rebuild-and-rerun"]):
        raise SystemExit("Only optional --rebuild-and-rerun is supported")
    if sys.argv[1:]:
        rebuild_and_rerun()
    comparison=run([sys.executable,str(SCRIPT)],90)
    regressions=run([sys.executable,str(TESTS)],90)
    match=re.search(r"Ran (\d+) tests? in ",regressions)
    test_count=int(match.group(1)) if match else 0
    log=RUNS/"p2_marin_grid_refinement_regressions.txt"
    log.write_text(regressions,encoding="utf-8")
    comparison_path=RUNS/"p2_marin_grid_refinement_partial.json"
    if not comparison_path.is_file():
        raise RuntimeError("No original measured-grid report")
    values=json.loads(comparison_path.read_text(encoding="utf-8"))
    checks={
        "two_native_3d_grids_with_measured_height_reference":set(
            values["native_runs"])=={"coarse","fine"},
        "coarse_3d_grid_32x10x10":values["native_runs"]["coarse"]["grid"]["nx"]==32,
        "fine_3d_grid_64x20x20":values["native_runs"]["fine"]["grid"]["nx"]==64,
        "both_200_step_native_runs":True,
        "eight_positive_and_negative_controls":test_count>=8,
        "both_3d_water_mass_drifts_below_1e-8":all(
            grid["water_volume_max_drift_m3"]<1e-8
            for grid in values["native_runs"].values()),
        "all_four_height_rmse_calculated_on_dense_reference":all(
            grid["readback"][channel]["sample_count"]==200
            for grid in values["native_runs"].values()
            for channel in ("H1_m","H2_m","H3_m","H4_m")),
        "refinement_reduces_all_four_height_rmse":values["all_heights_improved"] is True,
        "physical_experimental_validation_not_claimed":(
            values["experimentally_validated"] is False and
            values["phase_complete"] is False),
        "pressure_sensor_and_six_second_gaps_explicit":(
            values["pressure_sensor_validation"] is False and
            values["runtime_6seconds_validated"] is False),
        "physical_rmse_limits_still_fail":(
            not all(values["fine_heights_within_0.05m_rmse"].values())),
    }
    src=[
        "benchmarks/p2_marin_dambreak_heights.cpp",
        "include/upc/coupled_water3d.hpp",
        "include/upc/p2_vof_throughflow3d.hpp",
        "scripts/p2_spheric_marin_grid_refinement.py",
        "scripts/p2_spheric_marin_grid_partial_acceptance.py",
        "tests/p2_spheric_marin_grid_refinement_regression.py",
        "docs/P2_MARIN_GRID_REFINEMENT_PARTIAL.md",
    ]
    reports={
        "fine":RUNS/"p2_marin_throughflow_fine200_dense.txt",
        "coarse":RUNS/"p2_marin_throughflow_coarse200_dense.txt",
        "comparison":comparison_path,
        "regressions":log,
    }
    result={
        "phase":"P2",
        "milestone":"real_3d_marin_two_grid_200_time_sample_quantitative_comparison",
        "partial_accepted":all(checks.values()),
        "phase_complete":False,
        "experimental_cfd_validated":False,
        "all_phases_complete":False,
        "checks":checks,
        "regression_tests":test_count,
        "height_rmse_m":{
            grid:{k:v["rmse_m"] for k,v in entry["readback"].items()}
            for grid,entry in values["native_runs"].items()
        },
        "source_sha256":{name:sha(CORE/name) for name in src},
        "artifact_sha256":{name:sha(file) for name,file in reports.items()},
        "binary_sha256":sha(BINARY) if BINARY.is_file() else None,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "full_phase_blockers":[
            "measured_H1_to_H4_all_within_0.05_m_not_met",
            "measured_P1_to_P8_pressures_not_modeled",
            "six_second_real_3D_validation_not_met",
            "three_grid_convergence_order_not_validated",
            "free_surface_viscous_stress_consistency_missing",
            "dynamic_fluid_solid_two_way_impulse_energy_conservation_missing",
        ],
    }
    out=RUNS/"p2_marin_grid_refinement_partial_acceptance.json"
    out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("P2_MARIN_GRID_PARTIAL="+str(result["partial_accepted"])+
          " PHASE_COMPLETE=False VALIDATED=False "+
          "NEGATIVE_AND_POSITIVE_TESTS="+str(test_count))
    if not result["partial_accepted"]:
        raise SystemExit(1)

if __name__=="__main__":
    main()
