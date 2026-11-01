r"""Rebuild and independently recheck bounded 3D MARIN/SPHERIC Test-02 study.

P2 partial milestone only. An execution PASS is never a physics acceptance.
All temporary files and reports remain under D:\Projekt Engine.
"""
from __future__ import annotations
from datetime import datetime,timezone
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

CORE=Path(__file__).resolve().parents[1]
PROJECT=CORE.parents[1]
RUNS=PROJECT/"ENGINE TEST STUBE"/"runs"
BUILD=CORE/"build"
REF=CORE/"references"
BINARY=BUILD/"p2_marin_dambreak_heights.exe"
RUN_LOG=RUNS/"p2_marin_baseline_200steps.txt"
VCVARS=Path(r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat")

def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(args:list[str],name:str,timeout:int=180,env:dict|None=None):
    proc=subprocess.run(args,cwd=CORE,env=env,capture_output=True,text=True,
                        encoding="utf-8",errors="replace",timeout=timeout)
    print(f"{name} exit={proc.returncode}")
    if proc.returncode:
        print((proc.stdout+"\n"+proc.stderr)[-4500:])
        raise RuntimeError(f"{name} failed (exit {proc.returncode})")
    return proc

def main():
    RUNS.mkdir(parents=True,exist_ok=True)
    BUILD.mkdir(parents=True,exist_ok=True)
    if not VCVARS.is_file():
        raise RuntimeError("MSVC vcvars64.bat missing; cannot prove native rebuild")
    temp=BUILD/"p2_marin_tmp"
    temp.mkdir(exist_ok=True)
    env=os.environ.copy()
    env.update({"TEMP":str(temp),"TMP":str(temp),
                "PYTHONDONTWRITEBYTECODE":"1","PYTHONIOENCODING":"utf-8"})
    batch=BUILD/"p2_marin_rebuild.cmd"
    batch.write_text(
        "@echo off\n"
        f'call "{VCVARS}" >nul\n'
        "if errorlevel 1 exit /b 1\n"
        "cl /nologo /std:c++17 /EHsc /O2 /MD /Iinclude "
        "/Fobuild\\p2_marin_dambreak_heights.obj "
        "/Febuild\\p2_marin_dambreak_heights.exe "
        "benchmarks\\p2_marin_dambreak_heights.cpp\n"
        "exit /b %errorlevel%\n",
        encoding="ascii")
    compiled=run(["cmd.exe","/d","/c",str(batch)],"MSVC_NATIVE_REBUILD",120,env)
    (RUNS/"p2_marin_baseline_compile.txt").write_text(
        compiled.stdout+compiled.stderr,encoding="utf-8")
    if not BINARY.is_file():
        raise RuntimeError("MSVC did not produce native benchmark executable")
    experiment=run([sys.executable,str(CORE/"scripts"/"p2_spheric_experimental_ingest.py")],
                   "PINNED_EXPERIMENTAL_INGEST",90,env)
    if "CFD_VALIDATED=False" not in experiment.stdout:
        raise RuntimeError("Experimental data ingestion overstated CFD validity")
    native=run([str(BINARY),"200"],"NATIVE_3D_WATER_200_STEPS",180,env)
    if not native.stdout.strip().endswith(
        "PASS MARIN_COARSE_BASELINE partial_only=true steps=200 phase_complete=false"):
        raise RuntimeError("Native simulation did not emit bounded PASS")
    RUN_LOG.write_text(native.stdout,encoding="utf-8",newline="\n")
    comparison=run([sys.executable,str(CORE/"scripts"/"p2_spheric_marin_compare.py")],
                   "EXPERIMENTAL_VS_UPC_COMPARISON",90,env)
    if "PHYSICAL_VALIDATION=False" not in comparison.stdout:
        raise RuntimeError("Comparator incorrectly claims physics validation")
    regression=run([sys.executable,str(CORE/"tests"/"p2_spheric_marin_regression_tests.py")],
                   "SOURCE_AND_NEGATIVE_CONTROL_REGRESSIONS",90,env)
    test_output=regression.stdout+"\n"+regression.stderr
    match=re.search(r"Ran (\d+) tests? in ",test_output)
    tests=int(match.group(1)) if match else 0
    (RUNS/"p2_spheric_marin_regressions.txt").write_text(test_output,encoding="utf-8")
    compare_path=RUNS/"p2_spheric_marin_cfd_comparison_fail.json"
    ingest_path=RUNS/"p2_spheric_marin_measured_ingestion.json"
    d=json.loads(compare_path.read_text(encoding="utf-8"))
    ing=json.loads(ingest_path.read_text(encoding="utf-8"))
    checks={
        "native_msvc_rebuilt":BINARY.is_file(),
        "native_200_step_simulation_executed":True,
        "native_water_mass_conserved":d.get("physical_water_mass_conserved") is True
                                     and d.get("max_water_mass_volume_drift_m3",1)>0
                                     and d["max_water_mass_volume_drift_m3"]<1e-8,
        "official_archive_sha256_verified":ing.get("data_integrity_accepted") is True,
        "official_experiment_all_7395_rows":ing.get("rows")==7395,
        "official_experiment_12_sensors":len(ing.get("channels",[]))==13,
        "all_four_height_rmse_reported":len(d.get("metrics",{}))==4,
        "experimental_mismatch_not_silenced":d.get("heights_meet_rmse_target") is False,
        "native_pressure_signals_absent_explicit":(
            "no_simulated_8_pressure_sensor_signals" in d.get("validation_fail_reasons",[])),
        "full_phase_remains_open":d.get("experimental_cfd_validated") is False
                                  and d.get("phase_complete") is False,
        "nine_regression_negative_and_positive_tests":tests>=9,
    }
    paths=[
        CORE/"benchmarks"/"p2_marin_dambreak_heights.cpp",
        CORE/"scripts"/"p2_spheric_experimental_ingest.py",
        CORE/"scripts"/"p2_spheric_marin_compare.py",
        CORE/"scripts"/"p2_spheric_marin_partial_acceptance.py",
        CORE/"tests"/"p2_spheric_marin_regression_tests.py",
        REF/"SPHERIC_Test02_original.zip",
        REF/"SPHERIC_Test02"/"test_case_2_exp_data.xls",
        REF/"SPHERIC_Test02_measured.csv",
        BINARY,RUN_LOG,ingest_path,compare_path,
        RUNS/"p2_spheric_marin_regressions.txt",
    ]
    hashes={str(path.relative_to(PROJECT)).replace("\\","/"):sha(path) for path in paths}
    accepted=all(checks.values())
    report={
        "phase":"P2",
        "milestone":"official_spheric_marin_test02_3d_coarse_200step_partial",
        "partial_accepted":accepted,
        "phase_complete":False,
        "all_phases_complete":False,
        "experimental_cfd_validated":False,
        "checks":checks,
        "regression_tests":tests,
        "measured_data_rows":ing["rows"],
        "experiment_sensor_channels":ing["channels"],
        "native_run_duration_s":d["actual_duration_s"],
        "experiment_duration_s":d["experimental_duration_s"],
        "height_rmse_m":{k:v["rmse_m"] for k,v in d["metrics"].items()},
        "height_rmse_target_m":d["height_rmse_target_m"],
        "validation_fail_reasons":d["validation_fail_reasons"],
        "source_and_run_sha256":hashes,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "limitations":"Fail-closed external 3D benchmark comparison, NOT P2 acceptance. "
                      "Pressure traces, six-second time horizon, grid refinement, "
                      "air-water stress and dynamic FSI remain unvalidated.",
    }
    dest=RUNS/"p2_spheric_marin_partial_acceptance.json"
    dest.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(f"P2_MARIN_PARTIAL_ACCEPTED={accepted} PHASE_COMPLETE=False "
          f"REGRESSION_TESTS={tests} REPORT={dest}")
    if not accepted:raise SystemExit(1)

if __name__=="__main__":
    main()
