r"""Native 3D MAC-PCG / real SPHERIC MARIN P1-P8 and H1-H4 P2 PARTIAL.

A passing script is a demonstrable numerical improvement. It explicitly
CANNOT approve P2 physical two-phase CFD, free-surface stress or dynamic FSI.

Optional --rebuild-and-rerun recompiles MSVC and executes actual 200-step
native simulations on 32x10x10 and 64x20x20. All writes stay within
D:\Projekt Engine. The original measured archive is untouched.
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
ROOT=CORE.parents[1]
RUNS=ROOT/"ENGINE TEST STUBE"/"runs"
BINARY=CORE/"build"/"p2_marin_dambreak_heights.exe"
CTEST_LOG=RUNS/"p2_pcg_ctest_full.log"
NATIVE={
    "coarse":RUNS/"p2_marin_pcg_coarse200_dense.txt",
    "fine":RUNS/"p2_marin_pcg_fine200_dense.txt",
}
SOURCE=[
    "CMakeLists.txt",
    "include/upc/mac3d.hpp",
    "include/upc/coupled_water3d.hpp",
    "include/upc/p2_marin_wall_pressure_interp3d.hpp",
    "benchmarks/p2_marin_dambreak_heights.cpp",
    "tests/p2_mac_pcg_pressure_tests.cpp",
    "tests/p2_live_pcg_coupled_tests.cpp",
    "scripts/p2_marin_pcg_compare.py",
    "tests/p2_marin_pcg_experimental_regression.py",
    "scripts/p2_marin_pcg_measured_partial_acceptance.py",
    "scripts/p2_marin_pcg_build.cmd",
    "docs/P2_MARIN_PCG_MEASURED_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def run(args:list[str],timeout:int=240)->str:
    env=os.environ.copy()
    tmp=CORE/"build"/"p2_marin_pcg_temporaries"
    tmp.mkdir(exist_ok=True)
    env.update({"TEMP":str(tmp),"TMP":str(tmp),
                "PYTHONDONTWRITEBYTECODE":"1","PYTHONIOENCODING":"utf-8"})
    p=subprocess.run(args,cwd=CORE,env=env,text=True,encoding="utf-8",
                     errors="replace",capture_output=True,timeout=timeout)
    if p.returncode:
        raise RuntimeError("Required native command failed: "+str(args)+
                           "\n"+(p.stdout+p.stderr)[-1200:])
    return p.stdout+p.stderr

def rebuild_and_rerun():
    compile_log=run(["cmd.exe","/d","/c",
                     str(CORE/"scripts"/"p2_marin_pcg_build.cmd")],120)
    if not BINARY.is_file():
        raise RuntimeError("Native MARIN Win64 binary missing after build")
    print("P2_MARIN_PCG_NATIVE_REBUILD=PASS",flush=True)
    for grid,extra in (
        ("coarse",["200","throughflow","dense","interp","pcg"]),
        ("fine",["200","throughflow","fine","dense","interp","pcg"]),
    ):
        output=run([str(BINARY),*extra],240)
        if not output.strip().endswith(
            "PASS MARIN_COARSE_BASELINE partial_only=true steps=200 phase_complete=false"):
            raise RuntimeError("Native PCG "+grid+" 3D MARIN run missing PASS")
        NATIVE[grid].write_text(output,encoding="utf-8",newline="\n")
        print("P2_MARIN_PCG_3D_"+grid.upper()+"_200_STEPS=PASS",flush=True)

def main():
    RUNS.mkdir(exist_ok=True)
    if sys.argv[1:] not in ([],["--rebuild-and-rerun"]):
        raise SystemExit("Expected only optional --rebuild-and-rerun")
    if sys.argv[1:]:
        rebuild_and_rerun()
    if not BINARY.is_file():
        raise RuntimeError("Native benchmark binary absent")
    for grid,path in NATIVE.items():
        if not path.is_file():
            raise RuntimeError("Missing native measured MARIN "+grid+" evidence")
    compare=run([sys.executable,str(CORE/"scripts"/"p2_marin_pcg_compare.py"),
                 "--fine"],120)
    test=run([sys.executable,str(
        CORE/"tests"/"p2_marin_pcg_experimental_regression.py")],120)
    (RUNS/"p2_marin_pcg_regression.txt").write_text(test,encoding="utf-8")
    match=re.search(r"Ran (\d+) tests? in ",test)
    tests=int(match.group(1)) if match else 0
    report_path=RUNS/"p2_marin_pcg_experimental_partial.json"
    report=json.loads(report_path.read_text(encoding="utf-8"))
    if not CTEST_LOG.is_file():
        raise RuntimeError("Native 3D CTest full suite log missing")
    ctest=CTEST_LOG.read_text(encoding="utf-8",errors="replace")
    cm=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctest)
    suite=int(cm.group(1)) if cm else 0
    checks={
        "native_3d_pcg_two_grid_200_steps":report["two_spatial_grids_completed"] is True,
        "actual_9_positive_negative_experimental_regressions":tests>=9,
        "full_117_ctest_suites":suite>=117,
        "actual_native_3d_pcg_pressure_projection_present":(
            "physics_p2_mac_pcg_pressure_partial" in ctest and
            "physics_p2_live_pcg_coupled_partial" in ctest),
        "both_grids_wet_divergence_below_one_millionth":all(
            g["maximum_real_wet_projection_divergence_s_inv"]<1e-6
            for g in report["grids"].values()),
        "both_grids_water_mass_conserved":all(
            g["maximum_volume_drift_m3"]<1e-8
            for g in report["grids"].values()),
        "all_eight_pressure_rmse_improve_vs_gauss_seidel_on_both_grids":all(
            g["metrics"][f"P{k}_Pa"]["rmse_improved"]
            for g in report["grids"].values()
            for k in range(1,9)),
        "all_eight_pcg_pressure_rmse_improve_on_spatial_refinement":all(
            report["grids"]["fine"]["metrics"][f"P{k}_Pa"]["rmse_pcg"]<
            report["grids"]["coarse"]["metrics"][f"P{k}_Pa"]["rmse_pcg"]
            for k in range(1,9)),
        "all_four_physical_height_acceptance_thresholds_still_fail":all(
            report["grids"]["fine"]["metrics"][f"H{k}_m"]["rmse_pcg"]>.05
            for k in range(1,5)),
        "physical_phase_not_falsely_accepted":(
            report["phase_complete"] is False and
            report["real_pressure_physically_validated"] is False and
            report["real_water_height_physically_validated"] is False),
        "all_evidence_source_files_exist":all((CORE/f).is_file() for f in SOURCE),
    }
    passed=all(checks.values())
    evidence={
        "phase":"P2",
        "milestone":"native_two_grid_pcg_pressure_improvement_against_real_marin",
        "partial_accepted":passed,
        "physical_p2_phase_accepted":False,
        "phase_complete":False,
        "measured_pressure_physically_validated":False,
        "measured_water_height_physically_validated":False,
        "native_ctest_suites":suite,
        "independent_regression_tests":tests,
        "checks":checks,
        "source_sha256":{f:sha(CORE/f) for f in SOURCE},
        "artifact_sha256":{
            **{name:sha(p) for name,p in NATIVE.items()},
            "ctest":sha(CTEST_LOG),
            "measured_comparison":sha(report_path),
            "regression":sha(RUNS/"p2_marin_pcg_regression.txt"),
            "native_binary":sha(BINARY),
        },
        "rmse_pressure_pa":{
            grid:{f"P{k}_Pa":r["metrics"][f"P{k}_Pa"]["rmse_pcg"]
                  for k in range(1,9)}
            for grid,r in report["grids"].items()
        },
        "rmse_height_m":{
            grid:{f"H{k}_m":r["metrics"][f"H{k}_m"]["rmse_pcg"]
                  for k in range(1,5)}
            for grid,r in report["grids"].items()
        },
        "open_full_P2_acceptance_items":[
            "actual 3D water/air stress consistency and exact wall geometry",
            "independently measured P1-P8 wall pressure error within physical tolerance",
            "measured H1-H4 heights <=0.05m RMSE on all probes",
            "full >=6s external transient and independent grid-order convergence",
            "dynamic two-way rigid/deformable FSI and global energy/impulse closure",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    output=RUNS/"p2_marin_pcg_measured_partial_acceptance.json"
    output.write_text(json.dumps(evidence,ensure_ascii=False,indent=2)+"\n",
                      encoding="utf-8")
    print(f"P2_MAC_PCG_MEASURED_PARTIAL={passed} "
          f"NATIVE_CTEST={suite} REGRESSIONS={tests} "
          "PHYSICAL_VALIDATION=False PHASE_COMPLETE=False")
    if not passed:
        raise SystemExit(1)

if __name__=="__main__":
    main()
