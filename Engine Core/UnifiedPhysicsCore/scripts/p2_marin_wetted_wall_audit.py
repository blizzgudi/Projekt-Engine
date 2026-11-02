"""Independent rational-reference QA for P2 MARIN wet wall pressure traction.
No expensive 6s CFD runs; no phase completion claim.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from fractions import Fraction as F
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
EXE=CORE/"build"/"upc_p2_marin_wetted_wall_traction_tests.exe"
BUILD=RUNS/"p2_marin_wetted_build_20261102.log"
NATIVE=RUNS/"p2_marin_wetted_native_20261102.log"
CTEST=RUNS/"p2_marin_wetted_ctest_20261102.log"
REPORT=RUNS/"p2_marin_wetted_wall_partial.json"
LEGACY=RUNS/"p2_marin_pcg_full6s_three_grid_partial.json"
NUM=r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"
ROW=re.compile(r"^WET_TRACTION nx=(32|48|64) h=("+NUM+
    r") area=("+NUM+r") fx=("+NUM+r") fy=("+NUM+
    r") fz=("+NUM+r") hydro_fz=("+NUM+r")$")
END=re.compile(r"^PASS WET_TRACTION assertions=(\d+) "+
    r"measured_validation=false cut_solver=false phase_complete=false$")
SX,SY,SZ=F(".161"),F(".403"),F(".161")
ROOF=SX*SY
HEIGHTS=tuple(map(F,("0",".03",".103",".161",".55")))

def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def verify(raw:str):
    lines=raw.strip().splitlines()
    if len(lines)!=16:
        raise ValueError("Expected 15 independent grid/height cases plus PASS")
    rows=[]
    for i,line in enumerate(lines[:-1]):
        m=ROW.fullmatch(line)
        if not m:
            raise ValueError(f"Missing or malformed native case {i}")
        grid=(32,48,64)[i//5]
        h=HEIGHTS[i%5]
        if int(m[1])!=grid or abs(float(m[2])-float(h))>1e-13:
            raise ValueError("Wrong grid/waterline ordering")
        a,fx,fy,fz,hfz=map(float,m.groups()[2:])
        wet=min(h,SZ)
        expect=[
            2*(SX+SY)*wet+(ROOF if h>=SZ else 0),
            -F(1300)*ROOF*wet,
            F(700)*ROOF*wet,
            -ROOF*(F(2200)+F(1300)*(F(".6635")+SX/2)
                   -F(700)*(F(".295")+SY/2)-F(2000)*SZ) if h>=SZ else F(0),
            -F(9810)*ROOF*(h-SZ) if h>=SZ else F(0),
        ]
        for actual,reference in zip((a,fx,fy,fz,hfz),expect):
            if not math.isfinite(actual) or abs(actual-float(reference))>2e-8:
                raise ValueError(f"Independent analytic reference failed: {grid}, {h}")
        rows.append(dict(nx=grid,height_m=float(h),wetted_area_m2=a,
                         force_n=[fx,fy,fz],hydrostatic_roof_force_n=hfz))
    end=END.fullmatch(lines[-1])
    if not end or int(end[1])<200:
        raise ValueError("Missing native assertions or false full-phase claim")
    return rows,int(end[1])

def main():
    raw=NATIVE.read_text(encoding="utf-8")
    rows,assertions=verify(raw)
    independent=subprocess.run([str(EXE)],capture_output=True,text=True,timeout=60)
    if independent.returncode!=0 or independent.stdout.strip()!=raw.strip():
        raise AssertionError("Native independent rerun disagrees with recorded log")
    ctest=CTEST.read_text(encoding="utf-8",errors="replace")
    match=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctest)
    if not match or int(match[1])<153:
        raise AssertionError("Complete native CTest suite not proven")
    historic=json.loads(LEGACY.read_text(encoding="utf-8"))
    if (historic.get("experimental_three_grid_cfd_validated") is not False
        or historic.get("complete_phase") is not False
        or historic.get("full_six_second_height_rmse_target_m")!=.05
        or len(historic.get("fine_height_rmse_m",{}))!=4
        or not all(float(v)>.05 for v in historic["fine_height_rmse_m"].values())):
        raise AssertionError("Historical experimental nonconvergence hidden")
    mutations={
        "false_phase":raw.replace("phase_complete=false","phase_complete=true"),
        "false_measurement":raw.replace("measured_validation=false","measured_validation=true"),
        "false_cut_solver":raw.replace("cut_solver=false","cut_solver=true"),
        "forged_area":raw.replace("area=0.033839999999999995","area=0.055"),
        "forged_force":raw.replace("fx=-8.6878337000000325","fx=-8.0"),
        "missing_grid":"\n".join(l for l in raw.splitlines() if "nx=64" not in l),
        "wrong_grid":raw.replace("nx=48","nx=64"),
        "wrong_height":raw.replace("h=0.10299999999999999","h=0.09"),
        "nan":re.sub(r"(hydro_fz=)"+NUM,r"\1nan",raw,count=1),
        "too_few_assertions":re.sub(r"assertions=\d+","assertions=1",raw),
    }
    for name,bad in mutations.items():
        if bad==raw:
            raise AssertionError("Inactive negative mutation "+name)
        try:verify(bad)
        except (ValueError,TypeError,OverflowError):pass
        else:raise AssertionError("Forged trace accepted: "+name)
    sources=[
        "CMakeLists.txt",
        "include/upc/p2_marin_box_wall_traction3d.hpp",
        "tests/p2_marin_wetted_wall_traction3d_tests.cpp",
        "scripts/p2_marin_wetted_wall_native.cmd",
        "scripts/p2_marin_wetted_wall_audit.py",
        "docs/P2_MARIN_WETTED_WALL_PARTIAL.md",
        "docs/PHASENPLAN.md",
        "scripts/phase_gate.py",
    ]
    report={
        "phase":"P2","milestone":"marin_horizontal_wet_wall_traction_partial",
        "analytic_partial_accepted":True,"phase_complete":False,
        "production_cut_cell_solver":False,"measured_six_second_validation":False,
        "native_ctest_suites":int(match[1]),"native_assertions":assertions,
        "independent_native_rerun":True,"independent_rational_reference":True,
        "negative_forged_traces_rejected":len(mutations),
        "negative_case_names":list(mutations),"grid_height_cases":rows,
        "original_six_second_experiment_sha256":
            historic["experimental_original_csv_sha256"],
        "unchanged_fine_height_rmse_m":historic["fine_height_rmse_m"],
        "unchanged_fine_pressure_rmse_pa":historic["fine_pressure_rmse_pa"],
        "height_rmse_limit_m":.05,
        "native_binary_sha256":sha(EXE),
        "native_log_sha256":sha(NATIVE),
        "native_build_log_sha256":sha(BUILD),
        "native_ctest_log_sha256":sha(CTEST),
        "source_sha256":{name:sha(CORE/name) for name in sources},
        "known_negative_results":[
            "Only prescribed horizontal waterline; no reconstructed moving 3D VOF interface",
            "Pressure is diagnostic only and must already be physically meaningful on fluid side",
            "Production MARIN cut-cell pressure solver still binary and not corrected",
            "No bidirectional dynamic FSI or global physical momentum/energy balance",
            "Historical full 6s H1-H4 and P1-P8 three-grid measured convergence remains FAIL",
            "No full Native/UE5.8 E2E physical acceptance",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print("PASS MARIN_WETTED_WALL_QA grids=3 cases=15 native_ctest="+
          match[1]+" assertions="+str(assertions)+" negative="+
          str(len(mutations))+" phase_complete=false")
    print("EVIDENCE="+str(REPORT))
if __name__=="__main__":
    main()
