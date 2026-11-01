r"""P2 bounded experimental pressure-sensor audit; NEVER a full P2 acceptance.

Normal use checks previously executed native runs and all 11 regressions.
--rebuild-and-rerun performs native Win64/MSVC rebuild and two independent
200-step simulations (fine grid is substantially more computationally costly).
All generated files remain within D:\Projekt Engine.
"""
from __future__ import annotations

import hashlib
import json
import os
from datetime import datetime,timezone
from pathlib import Path
import re
import subprocess
import sys

CORE=Path(__file__).resolve().parents[1]
PROJECT=CORE.parents[1]
RUNS=PROJECT/"ENGINE TEST STUBE"/"runs"
BUILD=CORE/"build"
BINARY=BUILD/"p2_marin_dambreak_heights.exe"
UNIT=BINARY.parent/"upc_p2_marin_pressure_sensors3d_tests.exe"
VCVARS=Path(r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat")

def sha(p:Path):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def execute(cmd:list[str],timeout=240):
    env=os.environ.copy()
    tmp=BUILD/"p2_marin_pressure_tmp"
    tmp.mkdir(exist_ok=True)
    env.update({"TMP":str(tmp),"TEMP":str(tmp),"PYTHONDONTWRITEBYTECODE":"1",
                "PYTHONIOENCODING":"utf-8"})
    p=subprocess.run(cmd,cwd=CORE,capture_output=True,text=True,
                     encoding="utf-8",errors="replace",env=env,timeout=timeout)
    if p.returncode!=0:
        raise RuntimeError("Required native/test execution failed "+
                           str(cmd)+":\n"+(p.stdout+p.stderr)[-1700:])
    return p.stdout+p.stderr

def rebuild_rerun():
    if not VCVARS.is_file():
        raise RuntimeError("Windows MSVC build tools are unavailable")
    cmd=BUILD/"p2_marin_pressure_native_build.cmd"
    cmd.write_text(
        "@echo off\n"
        f'call "{VCVARS}" >nul\n'
        "if errorlevel 1 exit /b 1\n"
        "cl /nologo /std:c++17 /EHsc /O2 /MD /Iinclude "
        "/Fobuild\\p2_marin_dambreak_heights.obj "
        "/Febuild\\p2_marin_dambreak_heights.exe "
        "benchmarks\\p2_marin_dambreak_heights.cpp\n"
        "exit /b %errorlevel%\n",
        encoding="ascii")
    log=execute(["cmd.exe","/d","/c",str(cmd)],180)
    (RUNS/"p2_marin_pressure_native_build.txt").write_text(log,encoding="utf-8")
    for scope,extra,out in [
        ("coarse",["200","throughflow","dense","pressure"],
         RUNS/"p2_marin_pressure_coarse200_dense.txt"),
        ("fine",["200","throughflow","fine","dense","pressure"],
         RUNS/"p2_marin_pressure_fine200_dense.txt"),
    ]:
        log=execute([str(BINARY),*extra],240)
        if not log.strip().endswith(
                "PASS MARIN_COARSE_BASELINE partial_only=true steps=200 phase_complete=false"):
            raise RuntimeError("Native "+scope+" pressure process had no true terminal PASS")
        out.write_text(log,encoding="utf-8",newline="\n")
        print("NATIVE_8_PRESSURES_"+scope.upper()+"_200_STEPS_PASS",flush=True)

def main():
    RUNS.mkdir(exist_ok=True)
    if sys.argv[1:] not in ([],["--rebuild-and-rerun"]):
        raise SystemExit("Only --rebuild-and-rerun optional argument allowed")
    if sys.argv[1:]:rebuild_rerun()
    if not BINARY.is_file() or not UNIT.is_file():
        raise RuntimeError("Missing real MSVC native executable")
    output=execute([str(UNIT)],120)
    match=re.search(r"PASS P2_MARIN_PRESSURE_SENSORS assertions=(\d+) phase_complete=false",
                    output)
    assertions=int(match.group(1)) if match else 0
    (RUNS/"p2_marin_pressure_native_unit.txt").write_text(
        output,encoding="utf-8")
    reg=execute([sys.executable,str(CORE/"tests"/"p2_marin_pressure_compare_regression.py")],
                100)
    (RUNS/"p2_marin_pressure_regression.txt").write_text(reg,encoding="utf-8")
    m=re.search(r"Ran (\d+) tests? in ",reg)
    count=int(m.group(1)) if m else 0
    result_path=RUNS/"p2_marin_pressure_coarse_measured_partial.json"
    result=json.loads(result_path.read_text(encoding="utf-8"))
    ctest_log=RUNS/"p2_marin_pressure_ctest.txt"
    suites=0
    if ctest_log.exists():
        txt=ctest_log.read_bytes()
        content=txt.decode("utf-16" if txt.startswith(bytes((255,254))) else "utf-8-sig",
                           errors="replace")
        mm=re.search(r"100% tests passed, 0 tests failed out of (\d+)",content)
        suites=int(mm.group(1)) if mm else 0
    checks={
        "native_grid_mapping_8_gauges_184_assertions":assertions>=184,
        "eleven_data_integrity_and_negative_regressions":count>=11,
        "both_200step_real_pressure_traces":(
            result["native_time_samples"]==201 and
            result["fine_native_time_samples"]==201 and
            result["native_8_pressure_traces_executed"] is True and
            result["native_8_pressure_traces_fine_executed"] is True),
        "8_measured_pressure_channels_compared_on_both_grids":(
            len(result["metrics_pa"])==8 and
            len(result["fine_metrics_pa"])==8),
        "all_pressures_reproduce_rejected_physics":(
            all(not passed for passed in result["fine_versus_coarse_rmse_improved"].values()) and
            result["physical_pressure_validation"] is False),
        "P2_phase_status_remains_unapproved":result["phase_complete"] is False,
        "sensor_aliases_not_silently_hidden":(
            all(result["sensor_aliases_from_unresolved_coarse_grid"].values()) and
            all(result["sensor_aliases_from_unresolved_fine_grid"].values())),
        "full_native_ctest_114_suite_run":suites>=114,
    }
    source=[
        "benchmarks/p2_marin_dambreak_heights.cpp",
        "include/upc/p2_marin_pressure_sensors3d.hpp",
        "tests/p2_marin_pressure_sensors3d_tests.cpp",
        "scripts/p2_marin_pressure_compare.py",
        "tests/p2_marin_pressure_compare_regression.py",
        "scripts/p2_marin_pressure_partial_acceptance.py",
        "docs/P2_MARIN_3D_PRESSURE_SENSORS_PARTIAL.md",
        "CMakeLists.txt",
        "scripts/p2_marin_pressure_build.cmd",
    ]
    artifacts={
        "native_coarse":RUNS/"p2_marin_pressure_coarse200_dense.txt",
        "native_fine":RUNS/"p2_marin_pressure_fine200_dense.txt",
        "comparison":result_path,
        "unit_log":RUNS/"p2_marin_pressure_native_unit.txt",
        "regressions":RUNS/"p2_marin_pressure_regression.txt",
        "ctest":ctest_log,
        "native_build_log":RUNS/"p2_marin_pressure_native_build.txt",
        "cmake_compile_log":RUNS/"p2_marin_pressure_compile.txt",
    }
    accepted=all(checks.values())
    evidence={
        "phase":"P2",
        "milestone":"two_native_3d_grids_independent_measured_8_pressure_channels",
        "partial_accepted":accepted,
        "physical_pressure_validated":False,
        "phase_complete":False,
        "all_phases_complete":False,
        "native_assertions":assertions,
        "regression_tests":count,
        "ctest_suites":suites,
        "checks":checks,
        "coarse_pressure_rmse_pa":{
            k:v["rmse_pa"] for k,v in result["metrics_pa"].items()},
        "fine_pressure_rmse_pa":{
            k:v["rmse_pa"] for k,v in result["fine_metrics_pa"].items()},
        "sources_sha256":{p:sha(CORE/p) for p in source},
        "artifacts_sha256":{key:sha(path) for key,path in artifacts.items()},
        "native_binary_sha256":sha(BINARY),
        "unit_binary_sha256":sha(UNIT),
        "blockers":[
            "Native pressure probes sample fluid-side cell-centre, not wall transducers",
            "Both 3D grids alias independent physical pressure sensor positions",
            "All 8 sensor RMSE worsen at finer resolution, no pressure convergence",
            "Full 6s experiment, free-surface stress, dynamic two-way FSI unvalidated",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    dest=RUNS/"p2_marin_pressure_partial_acceptance.json"
    dest.write_text(json.dumps(evidence,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(f"P2_PRESSURE_PARTIAL={accepted} P2_COMPLETE=False NATIVE_ASSERTIONS={assertions} "
          f"PRESSURE_REGRESSIONS={count} CTEST={suites}")
    if not accepted:raise SystemExit(1)

if __name__=="__main__":
    main()
