"""Independent MARIN Test-02 floor-contact wall-traction QA; P2 remains PARTIAL.

Reruns the native MSVC binary, checks analytic loads using exact rational
arithmetic and rejects forged traces. Does NOT rerun the costly 6s CFD grids.
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
ROOT=CORE.parents[1]
RUNS=ROOT/"ENGINE TEST STUBE"/"runs"
EXE=CORE/"build"/"upc_p2_marin_box_wall_traction_tests.exe"
NATIVE=RUNS/"p2_marin_wall_traction_native_20261102.log"
BUILD=RUNS/"p2_marin_wall_traction_build_20261102.log"
CTEST=RUNS/"p2_marin_wall_traction_ctest_20261102.log"
LEGACY=RUNS/"p2_marin_pcg_full6s_three_grid_partial.json"
REPORT=RUNS/"p2_marin_wall_traction_partial.json"
LO=(F(".6635"),F(".295"),F(0))
HI=(F(".8245"),F(".698"),F(".161"))
NUM=r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"
FIELDS=("hydro_fx","hydro_fy","hydro_fz","affine_fx","affine_fy",
        "affine_fz","wetted_m2","floor_m2")
GRID_RE=re.compile(
    r"^MARIN_TRACTION grid=(coarse|medium|fine) dx=("+NUM+r") "+
    r" ".join(rf"{field}=({NUM})" for field in FIELDS)+
    r" tiles=(\d+)$")
LIVE_RE=re.compile(
    rf"^LIVE_TRACTION step_s=({NUM}) fx=({NUM}) fy=({NUM}) "+
    rf"fz=({NUM}) volume_m3=({NUM}) physical_cut_solver=false$")
END_RE=re.compile(
    r"^PASS MARIN_TRACTION assertions=(\d+) floor_contact=true "+
    r"physical_cut_solver=false measured_validation=false phase_complete=false$")

def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def expected():
    size=tuple(HI[i]-LO[i] for i in range(3))
    x,y,z=size
    volume=x*y*z
    roof=x*y
    area=2*(x*y+x*z+y*z)-roof
    xm=(HI[0]+LO[0])/2
    ym=(HI[1]+LO[1])/2
    return (
        F(0),F(0),-roof*F(9810)*(F(".55")-HI[2]),
        -F(1300)*volume,F(700)*volume,
        -roof*(F(2200)+F(1300)*xm-F(700)*ym-F(2000)*HI[2]),
        area,roof
    )

def parse(raw:str):
    lines=raw.strip().splitlines()
    if len(lines)!=5:
        raise ValueError("Require 3 grids, live native MAC and partial marker")
    result={}
    for i,name in enumerate(("coarse","medium","fine")):
        m=GRID_RE.fullmatch(lines[i])
        if not m or m.group(1)!=name:
            raise ValueError("Missing, reordered or forged grid")
        dx=float(m.group(2))
        if abs(dx-{"coarse":.1,"medium":3.2/48,"fine":.05}[name])>1e-14:
            raise ValueError("Wrong grid spacing")
        values=[float(m.group(j+3)) for j in range(len(FIELDS))]
        for actual,reference in zip(values,expected()):
            if not math.isfinite(actual) or abs(actual-float(reference))>1e-8:
                raise ValueError(f"{name}: nonconverged analytic pressure load")
        tiles=int(m.group(3+len(FIELDS)))
        if tiles<=0:
            raise ValueError("No pressure face tiles")
        result[name]={"dx_m":dx,"analytic_force_n":values[:3],
                      "affine_force_n":values[3:6],
                      "wetted_area_m2":values[6],
                      "floor_contact_area_m2":values[7],
                      "tiles":tiles}
    if not (result["coarse"]["tiles"]<result["medium"]["tiles"]<
            result["fine"]["tiles"]):
        raise ValueError("Face tile refinement missing")
    live=LIVE_RE.fullmatch(lines[3])
    if not live or any(not math.isfinite(float(v)) for v in live.groups()):
        raise ValueError("Missing or nonfinite actual MAC pressure snapshot")
    if abs(float(live.group(1))-.001)>1e-12 or float(live.group(5))<=0:
        raise ValueError("Invalid native MAC step or water volume")
    end=END_RE.fullmatch(lines[4])
    if not end or int(end.group(1))<70:
        raise ValueError("Native assertion count or partial marker forged")
    return result,int(end.group(1)),[float(v) for v in live.groups()]

def audit():
    raw=NATIVE.read_text(encoding="utf-8")
    grids,assertions,live=parse(raw)
    rerun=subprocess.run([str(EXE)],capture_output=True,text=True,
                         timeout=90,check=True)
    if rerun.stdout.replace("\r\n","\n").strip()!=raw.replace("\r\n","\n").strip():
        raise AssertionError("Independent native rerun does not match saved log")
    if rerun.stderr.strip():
        raise AssertionError("Native stderr not empty")
    ctest=CTEST.read_text(encoding="utf-8",errors="replace")
    match=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctest)
    if not match or int(match.group(1))<152:
        raise AssertionError("Full native CTest suite not green")
    build=BUILD.read_text(encoding="utf-8",errors="replace")
    if "Linking CXX executable upc_p2_marin_box_wall_traction_tests.exe" not in build:
        raise AssertionError("Native MSVC target build not evidenced")
    legacy=json.loads(LEGACY.read_text(encoding="utf-8"))
    if (legacy.get("experimental_three_grid_cfd_validated") is not False or
        legacy.get("complete_phase") is not False or
        legacy.get("full_six_second_height_rmse_target_m")!=.05):
        raise AssertionError("Historic measured nonconvergence changed")
    heights=legacy["fine_height_rmse_m"]
    if len(heights)!=4 or not all(float(v)>.05 for v in heights.values()):
        raise AssertionError("Historic measured height failure hidden")
    mutations={
        "false_full_phase":raw.replace("phase_complete=false","phase_complete=true"),
        "false_cut_solver":raw.replace("physical_cut_solver=false","physical_cut_solver=true"),
        "false_measurement":raw.replace("measured_validation=false","measured_validation=true"),
        "forged_coarse_force":raw.replace("hydro_fz=-247.59936747","hydro_fz=-246.59936747"),
        "forged_medium_force":raw.replace("affine_fx=-13.580011899999912","affine_fx=99.0"),
        "forged_area":raw.replace("wetted_m2=0.24649099999999999","wetted_m2=0.5"),
        "missing_fine":"\n".join(l for l in raw.splitlines() if "grid=fine" not in l),
        "duplicate_coarse":raw.replace("grid=medium","grid=coarse"),
        "wrong_grid_name":raw.replace("grid=coarse","grid=unknown"),
        "forged_assertions":re.sub(r"assertions=\d+","assertions=1",raw),
        "nan_live":re.sub(r"(LIVE_TRACTION step_s=0.001 fx=)"+NUM,r"\1nan",raw),
        "missing_live":"\n".join(l for l in raw.splitlines() if not l.startswith("LIVE_TRACTION")),
        "swapped_grids":"\n".join([raw.splitlines()[1],raw.splitlines()[0],*raw.splitlines()[2:]]),
    }
    for name,corrupt in mutations.items():
        if corrupt==raw:
            raise AssertionError("Negative QA mutation did not change trace: "+name)
        try:
            parse(corrupt)
        except (ValueError,TypeError,OverflowError):
            pass
        else:
            raise AssertionError("Forged native trace accepted: "+name)
    source_names=[
        "CMakeLists.txt",
        "include/upc/p2_marin_box_wall_traction3d.hpp",
        "tests/p2_marin_box_wall_traction3d_tests.cpp",
        "scripts/p2_marin_wall_traction_native.cmd",
        "scripts/p2_marin_wall_traction_audit.py",
        "docs/P2_MARIN_WALL_TRACTION_PARTIAL.md",
        "docs/PHASENPLAN.md",
        "scripts/phase_gate.py",
    ]
    report={
        "phase":"P2",
        "milestone":"marin_test02_exact_floor_contact_five_face_pressure_traction_partial",
        "analytic_partial_accepted":True,
        "phase_complete":False,
        "production_cut_cell_pressure_integrated":False,
        "measured_six_second_validation":False,
        "native_ctest_suites":int(match.group(1)),
        "native_assertions":assertions,
        "independent_native_rerun":True,
        "independent_rational_reference":True,
        "negative_forged_traces_rejected":len(mutations),
        "negative_case_names":list(mutations),
        "grids":grids,
        "live_mac_snapshot":{"step_s":live[0],"force_n":live[1:4],
                             "water_volume_m3":live[4],
                             "not_a_cut_cell_solver":True},
        "original_six_second_experiment_sha256":
            legacy["experimental_original_csv_sha256"],
        "unchanged_fine_height_rmse_m":heights,
        "unchanged_fine_pressure_rmse_pa":legacy["fine_pressure_rmse_pa"],
        "height_rmse_limit_m":.05,
        "native_binary_sha256":sha(EXE),
        "native_log_sha256":sha(NATIVE),
        "native_build_log_sha256":sha(BUILD),
        "native_ctest_log_sha256":sha(CTEST),
        "source_sha256":{name:sha(CORE/name) for name in source_names},
        "known_negative_results":[
            "Production MARIN benchmark still uses binary obstacle and cannot use this cut-face pressure traction as a conservative pressure boundary",
            "Native one-step force is diagnostic only: no equal-opposite momentum applied to water or moving rigid body",
            "Five-face floor contact has a nonzero net uniform-pressure load; treating it as a closed immersed box would be wrong",
            "Full six-second 3-grid H1-H4 and P1-P8 laboratory errors remain nonconverged",
            "No dynamic bidirectional FSI, full stress-consistent free surface or full UE5.8 Native-E2E acceptance",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print("PASS MARIN_WALL_TRACTION_QA three_grids=3 native_ctest="+match.group(1)+
          " assertions="+str(assertions)+" negative="+str(len(mutations))+
          " phase_complete=false")
    print("EVIDENCE="+str(REPORT))

if __name__=="__main__":
    audit()
