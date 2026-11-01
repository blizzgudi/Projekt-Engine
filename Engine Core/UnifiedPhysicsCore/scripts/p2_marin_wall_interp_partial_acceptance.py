"""P2 PARTIAL: verify real Win64/3D MAC wall-tangential MARIN pressure
readback, native tests, experimental failure, 2 grids, SHA provenance.
This cannot set the P2 phase gate to PASS.
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
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
BUILD=CORE/"build"
REPORT=RUNS/"p2_marin_wall_interp_partial_acceptance.json"

def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def execute(cmd,timeout=100):
    env=os.environ.copy()
    env["PYTHONDONTWRITEBYTECODE"]="1"
    env["PYTHONIOENCODING"]="utf-8"
    p=subprocess.run(cmd,cwd=CORE,capture_output=True,text=True,
                     encoding="utf-8",errors="replace",timeout=timeout,env=env)
    if p.returncode!=0:
        raise RuntimeError("Required P2 QA failed: "+str(cmd)+" "+
                           (p.stdout+p.stderr)[-2200:])
    return p.stdout+p.stderr

def main():
    if sys.argv[1:]:
        raise SystemExit("No arguments; use p2_marin_wall_interp_compare.py --run separately")
    binary=BUILD/"p2_marin_dambreak_heights.exe"
    unit=BUILD/"upc_p2_marin_wall_pressure_interp3d_tests.exe"
    if not binary.is_file() or not unit.is_file():
        raise RuntimeError("Native MSVC benchmark or unit binary missing")
    native=execute([str(unit)])
    m=re.search(r"PASS P2_MARIN_WALL_PRESSURE_INTERP assertions=(\d+) phase_complete=false",
                native)
    assertions=int(m.group(1)) if m else 0
    unit_log=RUNS/"p2_marin_wall_interp_native_unit.txt"
    unit_log.write_text(native,encoding="utf-8")
    regression=execute([sys.executable,str(CORE/"tests"/"p2_marin_wall_interp_regression.py")])
    regress_log=RUNS/"p2_marin_wall_interp_regression.txt"
    regress_log.write_text(regression,encoding="utf-8")
    m=re.search(r"Ran (\d+) tests? in ",regression)
    regressions=int(m.group(1)) if m else 0
    ctest=RUNS/"p2_marin_wall_interp_ctest.txt"
    content=ctest.read_text(encoding="utf-8-sig",errors="replace")
    m=re.search(r"100% tests passed, 0 tests failed out of (\d+)",content)
    suites=int(m.group(1)) if m else 0
    measured=RUNS/"p2_marin_wall_interp_comparison_partial.json"
    result=json.loads(measured.read_text(encoding="utf-8"))
    checks={
        "native_manufactured_and_live_MAC_175_assertions":assertions>=175,
        "15_positive_and_corruption_regressions":regressions>=15,
        "full_native_CTest_115_suites":suites>=115,
        "two_real_3d_native_pressure_runs":all(
            result["grids"][g]["rows"]==201 and result["grids"][g]["wet_final"]==8
            for g in ("coarse","fine")),
        "eight_external_pressure_channels_each_grid":all(
            len(result["grids"][g]["rmse_pa"])==8 for g in ("coarse","fine")),
        "four_original_height_channels_fail_005m_limit":all(
            len(result["grids"][g]["height_rmse_m"])==4 and
            all(v>0.05 for v in result["grids"][g]["height_rmse_m"].values())
            for g in ("coarse","fine")),
        "pressure_diagnostic_does_not_change_native_heights_or_mass":all(
            result["grids"][g]["historical_height_volume_max_difference"]==0
            for g in ("coarse","fine")),
        "native_interpolation_only_not_wall_stress":(
            result["native_wall_tangent_reconstruction"] is True and
            result["normal_wall_stress_reconstructed"] is False and
            result["independent_spatial_resolution_of_transducers"] is False),
        "all_eight_fine_grid_pressure_RMSE_worsen":all(
            v is False for v in result["fine_vs_coarse_rmse_improved"].values()),
        "experimental_validation_rejected":(
            result["experimental_pressure_validation"] is False and
            result["phase_complete"] is False),
        "native_binary_hash_matches_report":(
            sha(binary)==result["source_sha256"]["binary"]),
        "native_header_hash_matches_report":(
            sha(CORE/"include"/"upc"/"p2_marin_wall_pressure_interp3d.hpp")==
            result["source_sha256"]["header"]),
        "native_benchmark_hash_matches_report":(
            sha(CORE/"benchmarks"/"p2_marin_dambreak_heights.cpp")==
            result["source_sha256"]["benchmark"]),
        "native_output_hashes_match_report":all(
            sha(RUNS/f"p2_marin_wall_interp_{g}200.txt")==
            result["grids"][g]["native_sha256"] for g in ("coarse","fine")),
    }
    sources=[
        "CMakeLists.txt",
        "benchmarks/p2_marin_dambreak_heights.cpp",
        "include/upc/p2_marin_pressure_sensors3d.hpp",
        "include/upc/p2_marin_wall_pressure_interp3d.hpp",
        "tests/p2_marin_wall_pressure_interp3d_tests.cpp",
        "tests/p2_marin_wall_interp_regression.py",
        "scripts/p2_marin_wall_interp_build.cmd",
        "scripts/p2_marin_wall_interp_compare.py",
        "scripts/p2_marin_wall_interp_partial_acceptance.py",
        "docs/P2_MARIN_WALL_TANGENT_PRESSURE_INTERP_PARTIAL.md",
    ]
    artifacts={
        "native_coarse":RUNS/"p2_marin_wall_interp_coarse200.txt",
        "native_fine":RUNS/"p2_marin_wall_interp_fine200.txt",
        "measured":measured,
        "native_compile":RUNS/"p2_marin_wall_interp_compile.txt",
        "native_ctest":ctest,
        "native_unit":unit_log,
        "negative_regressions":regress_log,
        "historical_cell_centre_comparison":RUNS/"p2_marin_pressure_coarse_measured_partial.json",
    }
    partial=all(checks.values())
    evidence={
        "phase":"P2",
        "milestone":"native_MARIN_wall_tangent_interpolation_two_grids",
        "partial_accepted":partial,
        "experimental_pressure_validated":False,
        "phase_complete":False,
        "all_phases_complete":False,
        "native_assertions":assertions,
        "regression_tests":regressions,
        "ctest_suites":suites,
        "checks":checks,
        "coarse_height_rmse_m":result["grids"]["coarse"]["height_rmse_m"],
        "fine_height_rmse_m":result["grids"]["fine"]["height_rmse_m"],
        "coarse_rmse_pa":{
            k:v["rmse_pa"] for k,v in result["grids"]["coarse"]["rmse_pa"].items()},
        "fine_rmse_pa":{
            k:v["rmse_pa"] for k,v in result["grids"]["fine"]["rmse_pa"].items()},
        "interpolation_improved_vs_historical_nearest_cell":result["improved_vs_nearest_cell_rmse"],
        "sources_sha256":{name:sha(CORE/name) for name in sources},
        "artifacts_sha256":{name:sha(path) for name,path in artifacts.items()},
        "native_binaries_sha256":{
            "benchmark":sha(binary),"unit":sha(unit)},
        "blockers":[
            "Real measured pressure RMSE grows on all eight channels at fine grid",
            "Wall-normal stress and exact cut-cell pressure geometry not reconstructed",
            "0.04m transducer separation still under-resolved on both grids",
            "Only 2s out of required >=6s simulated",
            "Dynamic two-way FSI and global fluid impulse/energy closure not validated",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(evidence,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_WALL_INTERP_PARTIAL={partial} P2_COMPLETE=False "
          f"ASSERTIONS={assertions} REGRESSIONS={regressions} CTEST={suites}")
    if not partial:raise SystemExit(1)

if __name__=="__main__":
    main()
