"""Independent exact-rational geometry QA for MARIN column-height wall traction.
Only a diagnostic P2 partial; historical 6s CFD measurement FAIL is retained.
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
EXE=CORE/"build"/"upc_p2_marin_heightfield_wall_traction_tests.exe"
BUILD=RUNS/"p2_marin_heightfield_build_20261102.log"
PRE=RUNS/"p2_marin_heightfield_pre_fix_build_20261102.log"
NATIVE=RUNS/"p2_marin_heightfield_native_20261102.log"
CTEST=RUNS/"p2_marin_heightfield_ctest_20261102.log"
REPORT=RUNS/"p2_marin_heightfield_wall_partial.json"
LEGACY=RUNS/"p2_marin_pcg_full6s_three_grid_partial.json"
NUM=r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"
ROW=re.compile(r"^HEIGHTFIELD_TRACTION nx=(32|48|64) "+
    r"area=("+NUM+r") fx=("+NUM+r") fy=("+NUM+r") fz=("+NUM+
    r") tx=("+NUM+r") ty=("+NUM+r") tz=("+NUM+r")$")
END=re.compile(r"^PASS HEIGHTFIELD_TRACTION assertions=(\d+) "+
    r"live_vof=false measured_validation=false phase_complete=false$")
LO=(F(".6635"),F(".295"),F(0))
HI=(F(".8245"),F(".698"),F(".161"))
P=F(3000)
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def clipped(a,b,lo,hi):
    return max(F(0),min(b,hi)-max(a,lo))
def ref(nx:int):
    ny=nx*10//32
    dx=F(16,5)/nx
    center=tuple((a+b)/2 for a,b in zip(LO,HI))
    def h(x,y):
        return F(".025")+F(".020")*((x+2*y)%9)
    force=[F(0)]*3
    torque=[F(0)]*3
    area=F(0)
    # Independent closed-form pressure times wetted column-strip areas,
    # with centroid moment arms (no C++ quadrature or pressure stencil).
    for side in (-1,1):
        face=LO[0] if side<0 else HI[0]
        q=face/dx-F(1,2)
        ni=math.floor(q) if side<0 else math.ceil(q)
        for y in range(ny):
            width=clipped(y*dx,(y+1)*dx,LO[1],HI[1])
            wet=min(h(ni,y),HI[2])
            if width<=0 or wet<=0:continue
            mid=(max(y*dx,LO[1])+min((y+1)*dx,HI[1]))/2
            f=-side*P*width*wet
            force[0]+=f
            torque[1]+=(wet/2-center[2])*f
            torque[2]-=(mid-center[1])*f
            area+=width*wet
    for side in (-1,1):
        face=LO[1] if side<0 else HI[1]
        q=face/dx-F(1,2)
        ni=math.floor(q) if side<0 else math.ceil(q)
        for x in range(nx):
            width=clipped(x*dx,(x+1)*dx,LO[0],HI[0])
            wet=min(h(x,ni),HI[2])
            if width<=0 or wet<=0:continue
            mid=(max(x*dx,LO[0])+min((x+1)*dx,HI[0]))/2
            f=-side*P*width*wet
            force[1]+=f
            torque[0]-=(wet/2-center[2])*f
            torque[2]+=(mid-center[0])*f
            area+=width*wet
    for y in range(ny):
        for x in range(nx):
            if h(x,y)<HI[2]:continue
            wx=clipped(x*dx,(x+1)*dx,LO[0],HI[0])
            wy=clipped(y*dx,(y+1)*dx,LO[1],HI[1])
            if wx<=0 or wy<=0:continue
            mx=(max(x*dx,LO[0])+min((x+1)*dx,HI[0]))/2
            my=(max(y*dx,LO[1])+min((y+1)*dx,HI[1]))/2
            f=-P*wx*wy
            force[2]+=f
            torque[0]+=(my-center[1])*f
            torque[1]-=(mx-center[0])*f
            area+=wx*wy
    return tuple(float(x) for x in (area,*force,*torque))

def verify(raw:str):
    lines=raw.strip().splitlines()
    if len(lines)!=4:
        raise ValueError("Need exactly 3 native grid cases and PASS")
    values=[]
    for i,grid in enumerate((32,48,64)):
        match=ROW.fullmatch(lines[i])
        if not match or int(match[1])!=grid:
            raise ValueError("Invalid native grid case")
        actual=tuple(float(v) for v in match.groups()[1:])
        expected=ref(grid)
        for a,b in zip(actual,expected):
            if not math.isfinite(a) or abs(a-b)>2e-8:
                raise ValueError(f"Independent rational reference FAIL {grid}: {a} vs {b}")
        values.append(dict(nx=grid,wetted_area_m2=actual[0],
                           force_n=list(actual[1:4]),torque_nm=list(actual[4:])))
    match=END.fullmatch(lines[-1])
    if not match or int(match[1])<60:
        raise ValueError("Missing real native assertions / false full phase")
    return values,int(match[1])

def main():
    raw=NATIVE.read_text(encoding="utf-8")
    values,assertions=verify(raw)
    pre=PRE.read_text(encoding="utf-8",errors="replace")
    if "C2039" not in pre or "audit_heightfield" not in pre:
        raise AssertionError("Pre-fix compilation failure not reproduced")
    independent=subprocess.run([str(EXE)],capture_output=True,text=True,timeout=60)
    if independent.returncode!=0 or independent.stdout.strip()!=raw.strip():
        raise AssertionError("Independent native rerun does not match recorded output")
    ctest=CTEST.read_text(encoding="utf-8",errors="replace")
    match=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctest)
    if not match or int(match[1])<154:
        raise AssertionError("Native full CTest suite not verified")
    old=json.loads(LEGACY.read_text(encoding="utf-8"))
    if (old.get("experimental_three_grid_cfd_validated") is not False or
        old.get("complete_phase") is not False or
        old.get("full_six_second_height_rmse_target_m")!=.05 or
        len(old.get("fine_height_rmse_m",{}))!=4 or
        not all(v>.05 for v in old["fine_height_rmse_m"].values()) or
        len(old.get("fine_pressure_rmse_pa",{}))!=8):
        raise AssertionError("Historical measured FAIL hidden")
    mutations={
        "fake_full_phase":raw.replace("phase_complete=false","phase_complete=true"),
        "fake_live_vof":raw.replace("live_vof=false","live_vof=true"),
        "fake_measurement":raw.replace("measured_validation=false","measured_validation=true"),
        "fake_area":re.sub(r"(area=)"+NUM,r"\g<1>1.0",raw,count=1),
        "fake_force":re.sub(r"(fx=)"+NUM,r"\g<1>0.0",raw,count=1),
        "fake_torque":re.sub(r"(tz=)"+NUM,r"\g<1>0.0",raw,count=1),
        "nan":re.sub(r"(fy=)"+NUM,r"\g<1>nan",raw,count=1),
        "wrong_grid":raw.replace("nx=48","nx=64"),
        "missing_grid":"\n".join(l for l in raw.splitlines() if "nx=64" not in l),
        "duplicate_grid":raw.replace("nx=32","nx=48"),
        "few_assertions":re.sub(r"assertions=\d+","assertions=1",raw),
        "missing_end":"\n".join(raw.splitlines()[:-1]),
    }
    for name,bad in mutations.items():
        if bad==raw:
            raise AssertionError("Inactive negative mutation: "+name)
        try:verify(bad)
        except (ValueError,TypeError,OverflowError):pass
        else:raise AssertionError("Forged native log accepted: "+name)
    sources=[
        "CMakeLists.txt",
        "include/upc/p2_marin_box_wall_traction3d.hpp",
        "tests/p2_marin_heightfield_wall_traction3d_tests.cpp",
        "scripts/p2_marin_heightfield_wall_native.cmd",
        "scripts/p2_marin_heightfield_wall_audit.py",
        "docs/P2_MARIN_HEIGHTFIELD_WALL_PARTIAL.md",
        "docs/PHASENPLAN.md",
        "scripts/phase_gate.py",
    ]
    report={
        "phase":"P2","milestone":"marin_column_heightfield_wall_traction_partial",
        "analytic_partial_accepted":True,"phase_complete":False,
        "live_vof_interface_integrated":False,
        "production_cut_cell_solver_integrated":False,
        "dynamic_bidirectional_fsi_validated":False,
        "measured_six_second_validation":False,
        "native_ctest_suites":int(match[1]),"native_assertions":assertions,
        "independent_native_rerun":True,
        "independent_exact_rational_reference":True,
        "negative_forged_traces_rejected":len(mutations),
        "negative_case_names":list(mutations),
        "grid_cases":values,
        "historical_experiment_sha256":old["experimental_original_csv_sha256"],
        "historical_fine_height_rmse_m":old["fine_height_rmse_m"],
        "historical_fine_pressure_rmse_pa":old["fine_pressure_rmse_pa"],
        "height_rmse_limit_m":.05,
        "native_binary_sha256":sha(EXE),
        "native_log_sha256":sha(NATIVE),
        "native_build_log_sha256":sha(BUILD),
        "pre_fix_failed_build_log_sha256":sha(PRE),
        "native_ctest_log_sha256":sha(CTEST),
        "source_sha256":{name:sha(CORE/name) for name in sources},
        "known_negative_results":[
            "Column heightfield prescribed, not reconstructed from evolving live 3D VOF",
            "Discontinuous columns only; overhangs, bubbles and real interface stress unresolved",
            "Wall force/torque diagnostic only; no conservative production cut-cell pressure solve",
            "No bidirectional moving-body FSI or global physical momentum/energy closure",
            "Full 6s three-grid MARIN H1-H4 and P1-P8 measured convergence remains FAIL",
            "No new UE5.8 Native/measurement E2E acceptance",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print("PASS MARIN_HEIGHTFIELD_WALL_QA grids=3 native_ctest="+match[1]+
          " assertions="+str(assertions)+" negative="+str(len(mutations))+
          " phase_complete=false")
    print("EVIDENCE="+str(REPORT))

if __name__=="__main__":
    main()
