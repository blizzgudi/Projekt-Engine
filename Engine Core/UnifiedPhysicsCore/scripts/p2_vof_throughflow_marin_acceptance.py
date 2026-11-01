"""P2 partial: native throughflow analytical test + real MARIN measured FAIL.

A green partial report certifies reproducible tests only. It is expressly
forbidden to infer complete P2 CFD/FSI acceptance from this script.
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
REGRESSION=BUILD/"upc_p2_vof_throughflow_tests.exe"
MARIN=BUILD/"p2_marin_dambreak_heights.exe"
LOG=RUNS/"p2_marin_throughflow_200steps.txt"
CTEST=RUNS/"p2_plic_substep_fix_ctest.txt"
SOURCE_FILES=[
    "include/upc/p2_vof_throughflow3d.hpp",
    "include/upc/coupled_water3d.hpp",
    "tests/p2_vof_throughflow3d_tests.cpp",
    "benchmarks/p2_marin_dambreak_heights.cpp",
    "scripts/p2_spheric_marin_throughflow_probe.py",
    "scripts/p2_vof_throughflow_marin_acceptance.py",
    "docs/P2_SPHERIC_MARIN_THROUGHFLOW_PARTIAL.md",
    "CMakeLists.txt",
]
def sha(file:Path)->str:
    return hashlib.sha256(file.read_bytes()).hexdigest()

def run(command:list[str],timeout:int=120)->str:
    env=os.environ.copy()
    env["PYTHONDONTWRITEBYTECODE"]="1"
    proc=subprocess.run(command,cwd=CORE,env=env,capture_output=True,
                        encoding="utf-8",errors="replace",text=True,timeout=timeout)
    if proc.returncode:
        raise RuntimeError("Native/analytic comparison failed: "+
                           (proc.stdout+proc.stderr)[-1200:])
    return proc.stdout

def main():
    if not REGRESSION.is_file() or not MARIN.is_file() or not CTEST.is_file():
        raise RuntimeError("Required compiled native binary or CTest log missing")
    analytic=run([str(REGRESSION)],120)
    m=re.search(r"PASS P2_THROUGHFLOW assertions=(\d+) phase_complete=false",analytic)
    assertions=int(m.group(1)) if m else 0
    if assertions<2400:
        raise RuntimeError("Analytic/live 3D throughflow native tests not PASS")
    data=RUNS/"p2_spheric_marin_partial_acceptance.json"
    if not data.is_file():
        raise RuntimeError("Official measured baseline not verified")
    origin=json.loads(data.read_text(encoding="utf-8"))
    if origin.get("partial_accepted") is not True or origin.get("phase_complete") is not False:
        raise RuntimeError("Measured MARIN baseline partial evidence missing")
    native=run([str(MARIN),"200","throughflow"],240)
    if not native.rstrip().endswith(
        "PASS MARIN_COARSE_BASELINE partial_only=true steps=200 phase_complete=false"):
        raise RuntimeError("Throughflow native run not fully complete")
    LOG.write_text(native,encoding="utf-8",newline="\n")
    measured_probe=run([sys.executable,str(CORE/"scripts"/"p2_spheric_marin_throughflow_probe.py")],90)
    result=json.loads(
        (RUNS/"p2_spheric_marin_throughflow_comparison.json").read_text(encoding="utf-8"))
    ctest=CTEST.read_text(encoding="utf-8",errors="replace")
    match=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctest)
    suite_count=int(match.group(1)) if match else 0
    metrics=result["metrics"]
    checks={
        "native_analytical_3d_translation":assertions>=2400,
        "conservative_six_face_3d_transport_live":(
            "LIVE_THROUGHFLOW legacy_volume_m3=0.09" in analytic),
        "registered_native_ctest_at_least_113":(
            suite_count>=113 and "physics_p2_vof_throughflow_partial" in ctest),
        "official_7395_measured_rows_ingested":origin.get("measured_data_rows")==7395,
        "native_200_step_3d_throughflow_executed":result["throughflow_executed"] is True,
        "all_four_sensor_rmse_improved":result["improved_probe_count"]==4,
        "3d_water_mass_conserved":result["max_volume_drift_m3"]<1e-8,
        "experimental_fail_remains_explicit":(
            result["experimental_cfd_validated"] is False and
            result["phase_complete"] is False and
            result["all_height_rmse_within_0.05m"] is False),
        "sources_and_results_present":all((CORE/f).is_file() for f in SOURCE_FILES),
        "external_8_pressure_sensors_still_open":(
            "pressure_sensors_not_modeled" in result["blockers"]),
    }
    accepted=all(checks.values())
    evidence={
        "phase":"P2",
        "milestone":"vof_simultaneous_saturated_throughflow_3d_and_marin_reference",
        "partial_accepted":accepted,
        "phase_complete":False,
        "experimental_cfd_validated":False,
        "all_phases_complete":False,
        "native_assertions":assertions,
        "native_ctest_suites":suite_count,
        "checks":checks,
        "height_rmse_m":{k:v["rmse_m"] for k,v in metrics.items()},
        "legacy_rmse_m":{k:v["legacy_rmse_m"] for k,v in metrics.items()},
        "height_rmse_target_m":0.05,
        "source_sha256":{f:sha(CORE/f) for f in SOURCE_FILES},
        "artifact_sha256":{
            "throughflow_native_log":sha(LOG),
            "analytic_native_binary":sha(REGRESSION),
            "throughflow_native_binary":sha(MARIN),
            "marin_comparison":sha(RUNS/"p2_spheric_marin_throughflow_comparison.json"),
            "ctest":sha(CTEST),
        },
        "blocked_by":result["blockers"],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    report=RUNS/"p2_vof_throughflow_marin_partial.json"
    report.write_text(json.dumps(evidence,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"THROUGHFLOW_PARTIAL={accepted} NATIVE_ASSERTIONS={assertions} "
          f"CTEST={suite_count} MEASURED_CFD_VALIDATED=False PHASE_COMPLETE=False")
    if not accepted:
        print("FAILED_CHECKS",[name for name,val in checks.items() if not val])
        raise SystemExit(1)

if __name__=="__main__":
    main()
