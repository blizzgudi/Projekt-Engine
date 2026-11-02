"""Independent dense-reference QA for global 3D water/air shear, NOT P2 full."""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
ROOT=CORE.parents[1]
RUNS=ROOT/"ENGINE TEST STUBE"/"runs"
PREFIX="p2_two_phase_global_shear"
BINARY=CORE/"build"/"upc_p2_two_phase_global_shear_tests.exe"
NATIVE=RUNS/(PREFIX+"_native.txt")
BUILD=RUNS/(PREFIX+"_build.txt")
CTEST=RUNS/(PREFIX+"_ctest.txt")
GATE=RUNS/(PREFIX+"_phase_gate.txt")
REPORT=RUNS/(PREFIX+"_partial.json")
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_two_phase_global_shear3d.hpp",
    "include/upc/p2_two_phase_grid_shear3d.hpp",
    "include/upc/p2_two_phase_shear_exchange3d.hpp",
    "tests/p2_two_phase_global_shear3d_tests.cpp",
    "scripts/p2_two_phase_global_shear_build.cmd",
    "scripts/p2_two_phase_global_shear_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_TWO_PHASE_GLOBAL_SHEAR_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
P=re.compile(r"GLOBAL_PLANAR faces=(\d+) slip=([0-9.eE+-]+) water_y=([0-9.eE+-]+) air_y=([0-9.eE+-]+) energy_residual=([0-9.eE+-]+)")
G=re.compile(r"GLOBAL_3D faces=(\d+) axes=(\d+),(\d+),(\d+) v0=([0-9.eE+,-]+) v13=([0-9.eE+,-]+) v26=([0-9.eE+,-]+) momentum_residual=([0-9.eE+-]+) energy_residual=([0-9.eE+-]+) pcg_max=([0-9.eE+-]+)")
H=re.compile(r"HETEROGENEOUS_3D faces=(\d+) dense_max_error=([0-9.eE+-]+) v0=([0-9.eE+,-]+) v13=([0-9.eE+,-]+) v26=([0-9.eE+,-]+) energy_residual=([0-9.eE+-]+)")
PASS=re.compile(r"PASS: GLOBAL_TWO_PHASE_SHEAR assertions=(\d+)")

def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def close(actual:float,expected:float,tol:float,name:str)->None:
    if not math.isfinite(actual) or abs(actual-expected)>tol:
        raise ValueError(f"{name}: measured={actual} expected={expected} tol={tol}")

def solve_dense(a:list[list[float]],rhs:list[float])->list[float]:
    """Independent Gaussian elimination with partial pivoting; no PCG."""
    n=len(rhs)
    m=[row[:]+[rhs[i]] for i,row in enumerate(a)]
    for k in range(n):
        pivot=max(range(k,n),key=lambda i:abs(m[i][k]))
        if abs(m[pivot][k])<1e-18:
            raise ValueError("Singular dense reference")
        m[k],m[pivot]=m[pivot],m[k]
        for i in range(k+1,n):
            f=m[i][k]/m[k][k]
            for j in range(k+1,n+1):
                m[i][j]-=f*m[k][j]
            m[i][k]=0.0
    x=[0.0]*n
    for i in range(n-1,-1,-1):
        x[i]=(m[i][n]-sum(m[i][j]*x[j] for j in range(i+1,n)))/m[i][i]
    return x

def dense_checkerboard()->list[list[float]]:
    n=27
    dx=.08
    dt=.03
    mw=1000*dx**3
    ma=1.2*dx**3
    b=dt*dx**2/(dx/2/1.3+dx/2/.18)
    phases=[]
    vel=[]
    for z in range(3):
        for y in range(3):
            for x in range(3):
                phases.append((x+y+z)%2==0)
                vel.append((.1*x+.03*y,-.2*y+.04*z,.12*z+.02*x))
    result=[[0.0]*3 for _ in range(n)]
    for k in range(3):
        a=[[0.0]*n for _ in range(n)]
        rhs=[0.0]*n
        for i in range(n):
            mass=mw if phases[i] else ma
            a[i][i]=mass
            rhs[i]=mass*vel[i][k]
        for z in range(3):
            for y in range(3):
                for x in range(3):
                    i=(z*3+y)*3+x
                    for axis in range(3):
                        if axis==k:
                            continue
                        xx=x+(axis==0)
                        yy=y+(axis==1)
                        zz=z+(axis==2)
                        if xx>=3 or yy>=3 or zz>=3:
                            continue
                        j=(zz*3+yy)*3+xx
                        if phases[i]==phases[j]:
                            continue
                        a[i][i]+=b
                        a[j][j]+=b
                        a[i][j]-=b
                        a[j][i]-=b
        x=solve_dense(a,rhs)
        for i in range(n):
            result[i][k]=x[i]
    return result


def dense_heterogeneous()->list[list[float]]:
    """Independent 27-cell variable-mass/resistance FV reference."""
    n=27
    dx=.08
    dt=.03
    area=dx**2
    vol=dx**3
    phase=[]
    density=[]
    viscosity=[]
    vel=[]
    for z in range(3):
        for y in range(3):
            for x in range(3):
                wet=(x+y+z)%2==0
                phase.append(wet)
                density.append((850+10*x+5*y+2*z) if wet
                               else (1+.1*x+.02*y+.01*z))
                viscosity.append((.5+.1*x+.05*y+.02*z) if wet
                                 else (.06+.01*x+.005*y+.002*z))
                vel.append((.1*x+.03*y,-.2*y+.04*z,.12*z+.02*x))
    result=[[0.0]*3 for _ in range(n)]
    for k in range(3):
        a=[[0.0]*n for _ in range(n)]
        rhs=[0.0]*n
        for i in range(n):
            a[i][i]=density[i]*vol
            rhs[i]=density[i]*vol*vel[i][k]
        for z in range(3):
            for y in range(3):
                for x in range(3):
                    i=(z*3+y)*3+x
                    for axis in range(3):
                        if axis==k:
                            continue
                        xx=x+(axis==0)
                        yy=y+(axis==1)
                        zz=z+(axis==2)
                        if xx>=3 or yy>=3 or zz>=3:
                            continue
                        j=(zz*3+yy)*3+xx
                        if phase[i]==phase[j]:
                            continue
                        resistance=.5*dx/viscosity[i]+.5*dx/viscosity[j]
                        b=dt*area/resistance
                        a[i][i]+=b
                        a[j][j]+=b
                        a[i][j]-=b
                        a[j][i]-=b
        sol=solve_dense(a,rhs)
        for i in range(n):
            result[i][k]=sol[i]
    return result

def parse(native:str)->dict:
    lines=native.strip().splitlines()
    if len(lines)!=4:
        raise ValueError("Expected planar, 3D, heterogeneous and native PASS")
    p=P.fullmatch(lines[0])
    g=G.fullmatch(lines[1])
    h=H.fullmatch(lines[2])
    end=PASS.fullmatch(lines[3])
    if not p or not g or not h or not end or int(end[1])<450:
        raise ValueError("Missing native references or negative assertions")
    if int(p[1])!=16 or (int(g[1]),int(g[2]),int(g[3]),int(g[4]))!=(54,18,18,18):
        raise ValueError("Wrong planar or three-axis interface count")
    mw=1000*.1**3
    ma=1.2*.1**3
    b=.05*.1**2/(.05/1+.05/.1)
    slip=1/(1+b*(1/mw+1/ma))
    close(float(p[2]),slip,1e-13,"Couette analytic slip")
    close(float(p[3]),b*slip/mw,1e-13,"Couette water velocity")
    close(float(p[4]),1-b*slip/ma,1e-13,"Couette air velocity")
    close(float(p[5]),0,1e-12,"Couette energy")
    ref=dense_checkerboard()
    samples={}
    for idx,group in ((0,g[5]),(13,g[6]),(26,g[7])):
        measured=[float(s) for s in group.split(",")]
        if len(measured)!=3:
            raise ValueError("Missing 3D velocity component")
        for k in range(3):
            close(measured[k],ref[idx][k],2e-12,f"Dense 3D cell {idx} axis {k}")
        samples[str(idx)]={"native":measured,"independent_dense":ref[idx]}
    close(float(g[8]),0,1e-12,"Global momentum")
    close(float(g[9]),0,1e-12,"Global energy")
    if not 0<=float(g[10])<1e-12:
        raise ValueError("Global PCG residual too high")
    if int(h[1])!=54:
        raise ValueError("Heterogeneous interface topology changed")
    if not 0<=float(h[2])<2e-12:
        raise ValueError("Heterogeneous C++ dense error too high")
    close(float(h[6]),0,2e-12,"Heterogeneous energy")
    href=dense_heterogeneous()
    heterogeneous_samples={}
    for idx,group in ((0,h[3]),(13,h[4]),(26,h[5])):
        measured=[float(v) for v in group.split(",")]
        if len(measured)!=3:
            raise ValueError("Heterogeneous 3D velocity sample incomplete")
        for k in range(3):
            close(measured[k],href[idx][k],2e-12,
                  f"Independent heterogeneous 3D cell {idx} axis {k}")
        heterogeneous_samples[str(idx)]={
            "native":measured,"independent_dense":href[idx]}
    return {
        "heterogeneous_3d_dense_samples":heterogeneous_samples,
        "heterogeneous_dense_max_error":float(h[2]),
        "heterogeneous_energy_residual_j":float(h[6]),
        "native_assertions":int(end[1]),
        "planar_interfaces":int(p[1]),
        "three_dimensional_interfaces":int(g[1]),
        "axis_faces":[int(g[2]),int(g[3]),int(g[4])],
        "planar_slip_m_s":float(p[2]),
        "independent_planar_slip_m_s":slip,
        "dense_gaussian_elimination_samples":samples,
        "max_momentum_residual_n_s":float(g[8]),
        "energy_residual_j":float(g[9]),
        "pcg_max_residual_kg_m_s":float(g[10]),
    }

def main()->None:
    native=NATIVE.read_text(encoding="utf-8")
    measured=parse(native)
    build=BUILD.read_text(encoding="utf-8")
    ctest=CTEST.read_text(encoding="utf-8")
    if "upc_p2_two_phase_global_shear_tests.exe" not in build:
        raise ValueError("No real MSVC global shear build evidence")
    if "100% tests passed, 0 tests failed out of 134" not in ctest:
        raise ValueError("134 native CTest suites did not pass")
    rerun=subprocess.run([str(BINARY)],capture_output=True,text=True,timeout=90)
    if rerun.returncode!=0 or rerun.stdout.strip()!=native.strip():
        raise ValueError("Native rerun differs from build trace")
    mutations=[
        ("faces=16","faces=15"),
        ("slip=","slip=9"),
        ("faces=54","faces=53"),
        ("axes=18,18,18","axes=18,18,17"),
        ("v0=","v0=9"),
        ("pcg_max=5.5943312324395099e-17","pcg_max=9"),
        ("dense_max_error=6.1388394367867249e-14","dense_max_error=9"),
        ("v13=","v13=9"),
        (f"assertions={measured['native_assertions']}","assertions=1"),
    ]
    rejected=0
    for old,new in mutations:
        forged=native.replace(old,new,1)
        if forged==native:
            raise ValueError(f"Mutation not applied: {old}")
        try:
            parse(forged)
        except ValueError:
            rejected+=1
        else:
            raise ValueError(f"Forged trace accepted: {old}")
    historical=RUNS/"p2_marin_pcg_6s_partial.json"
    old=json.loads(historical.read_text(encoding="utf-8"))
    if old.get("height_rmse_threshold_met") is not False or old.get("phase_complete") is not False:
        raise ValueError("Negative measured MARIN height validation not retained")
    gate=subprocess.run([sys.executable,str(CORE/"scripts"/"phase_gate.py")],
                        capture_output=True,text=True,timeout=240)
    GATE.write_text(gate.stdout+gate.stderr,encoding="utf-8")
    if gate.returncode!=0 or "ALL_COMPLETE=False" not in gate.stdout:
        raise ValueError("Official gate not run or P2 falsely accepted")
    if "P2" not in gate.stdout.split("GATES_INCOMPLETE=",1)[-1].splitlines()[0]:
        raise ValueError("P2 missing from incomplete official gate")
    report={
        "phase":"P2",
        "milestone":"global_spd_3d_water_air_shear_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "independent_qa":measured,
        "ctest_passed":134,
        "forged_native_traces_rejected":rejected,
        "historical_marin_6s_height_validation":"FAIL",
        "historical_marin_sha256":sha(historical),
        "limitations":[
            "Fixed sharp cell-centered interface, not dynamic MAC/VOF/PLIC",
            "No fully coupled two-phase normal stress, capillarity or pressure projection",
            "No new >=6s H1-H4/P1-P8 three-grid measured convergence",
            "No dynamic two-way fluid-solid momentum/energy acceptance",
            "No UE5.8 Native E2E for this operator",
        ],
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "binary_sha256":sha(BINARY),
        "build_log_sha256":sha(BUILD),
        "native_log_sha256":sha(NATIVE),
        "ctest_log_sha256":sha(CTEST),
        "phase_gate_log_sha256":sha(GATE),
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print(f"P2 GLOBAL SHEAR PARTIAL PASS: {measured['native_assertions']} assertions, "
          f"134/134 CTest, constant and heterogeneous dense 27-cell references, {rejected} forgeries rejected; "
          "P2_FULL=false")

if __name__=="__main__":
    main()
