"""Independent Python dense/analytic QA for bounded two-phase 3D MAC pressure.
Never grants P2 full-phase acceptance or substitutes for MARIN/SPHERIC data.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
ROOT=CORE.parents[1]
RUNS=ROOT/"ENGINE TEST STUBE"/"runs"
PREFIX="p2_two_phase_mac_pressure"
BINARY=CORE/"build"/"upc_p2_two_phase_mac_pressure_tests.exe"
NATIVE=RUNS/(PREFIX+"_native.txt")
BUILD=RUNS/(PREFIX+"_build.txt")
CTEST=RUNS/(PREFIX+"_ctest.txt")
GATE=RUNS/(PREFIX+"_phase_gate.txt")
REPORT=RUNS/(PREFIX+"_partial.json")
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_two_phase_mac_pressure3d.hpp",
    "tests/p2_two_phase_mac_pressure3d_tests.cpp",
    "scripts/p2_two_phase_mac_pressure_build.cmd",
    "scripts/p2_two_phase_mac_pressure_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_TWO_PHASE_MAC_PRESSURE_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
LINE=re.compile(r"^MANUFACTURED_3D grid=(\d+)x(\d+)x(\d+) pmax=([-+0-9.eE]+) vmax=([-+0-9.eE]+) div_before=([-+0-9.eE]+) div_after=([-+0-9.eE]+) energy_residual=([-+0-9.eE]+) momentum_residual=([-+0-9.eE]+) p0=([-+0-9.eE]+) pmid=([-+0-9.eE]+) plast=([-+0-9.eE]+) u111=([-+0-9.eE]+) v111=([-+0-9.eE]+) w111=([-+0-9.eE]+) pcg_iterations=(\d+)$")
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def close(actual:float,expected:float,tol:float,name:str)->None:
    if not math.isfinite(actual) or abs(actual-expected)>tol:
        raise ValueError(f"{name}: actual={actual}, expected={expected}, tol={tol}")
def pressure(x:int,y:int,z:int)->float:
    return .4*x*x-.6*y+.2*y*y+.35*z*z+.13*x*y-.09*y*z
def psi(x:int,y:int,z:int,nx:int,ny:int)->float:
    if x==0 or x==nx or y==0 or y==ny:return 0.0
    return .004*(1+.15*z)*math.sin(math.pi*x/nx)*math.sin(math.pi*y/ny)
def reference(nx:int,ny:int,nz:int)->dict:
    """Independent Python assembly using cell stencils and a dense gauge solve."""
    dx=.08;dt=.015
    n=nx*ny*nz
    ci=lambda x,y,z:(z*ny+y)*nx+x
    ui=lambda x,y,z:(z*ny+y)*(nx+1)+x
    vi=lambda x,y,z:(z*(ny+1)+y)*nx+x
    wi=lambda x,y,z:(z*ny+y)*nx+x
    density=[0.0]*n
    q=[0.0]*n
    for z in range(nz):
        for y in range(ny):
            for x in range(nx):
                i=ci(x,y,z)
                density[i]=985+2*z+3*y+.5*x if x<nx//2 else 1.15+.02*y+.03*z
                q[i]=pressure(x,y,z)
    u=[0.0]*((nx+1)*ny*nz)
    v=[0.0]*(nx*(ny+1)*nz)
    w=[0.0]*(nx*ny*(nz+1))
    for z in range(nz):
        for y in range(ny):
            for x in range(1,nx):
                beta=2/(density[ci(x-1,y,z)]+density[ci(x,y,z)])
                u[ui(x,y,z)]=dt*beta*(q[ci(x,y,z)]-q[ci(x-1,y,z)])/dx+(
                    psi(x,y+1,z,nx,ny)-psi(x,y,z,nx,ny))/dx
    for z in range(nz):
        for y in range(1,ny):
            for x in range(nx):
                beta=2/(density[ci(x,y-1,z)]+density[ci(x,y,z)])
                v[vi(x,y,z)]=dt*beta*(q[ci(x,y,z)]-q[ci(x,y-1,z)])/dx-(
                    psi(x+1,y,z,nx,ny)-psi(x,y,z,nx,ny))/dx
    for z in range(1,nz):
        for y in range(ny):
            for x in range(nx):
                beta=2/(density[ci(x,y,z-1)]+density[ci(x,y,z)])
                w[wi(x,y,z)]=dt*beta*(q[ci(x,y,z)]-q[ci(x,y,z-1)])/dx
    divergence=[]
    for z in range(nz):
        for y in range(ny):
            for x in range(nx):
                divergence.append((u[ui(x+1,y,z)]-u[ui(x,y,z)]+
                    v[vi(x,y+1,z)]-v[vi(x,y,z)]+
                    w[wi(x,y,z+1)]-w[wi(x,y,z)])/dx)
    qmean=sum(q)/n
    result={
        "p0":q[0]-qmean,
        "pmid":q[ci(nx//2,ny//2,nz//2)]-qmean,
        "plast":q[-1]-qmean,
        "u111":(psi(1,2,1,nx,ny)-psi(1,1,1,nx,ny))/dx,
        "v111":-(psi(2,1,1,nx,ny)-psi(1,1,1,nx,ny))/dx,
        "w111":0.0,
        "div_before":max(abs(x) for x in divergence),
    }
    if (nx,ny,nz)!=(4,3,3):return result
    # Dense independent pressure reference: construct the full variable-beta
    # finite-volume Laplacian, pin one gauge cell, pivoted Gaussian solve.
    a=[[0.0]*n for _ in range(n)]
    b=[-d/dt for d in divergence]
    for z in range(nz):
        for y in range(ny):
            for x in range(nx):
                i=ci(x,y,z)
                for xx,yy,zz in ((x-1,y,z),(x+1,y,z),
                                 (x,y-1,z),(x,y+1,z),
                                 (x,y,z-1),(x,y,z+1)):
                    if not (0<=xx<nx and 0<=yy<ny and 0<=zz<nz):continue
                    j=ci(xx,yy,zz)
                    coefficient=2/(density[i]+density[j])/dx**2
                    a[i][i]+=coefficient
                    a[i][j]-=coefficient
    a[0]=[0.0]*n
    a[0][0]=1.0
    b[0]=0.0
    for k in range(n):
        pivot=max(range(k,n),key=lambda i:abs(a[i][k]))
        if abs(a[pivot][k])<1e-15:raise ValueError("Dense pressure matrix singular")
        a[k],a[pivot]=a[pivot],a[k]
        b[k],b[pivot]=b[pivot],b[k]
        for i in range(k+1,n):
            factor=a[i][k]/a[k][k]
            for j in range(k+1,n):a[i][j]-=factor*a[k][j]
            b[i]-=factor*b[k]
            a[i][k]=0.0
    sol=[0.0]*n
    for i in range(n-1,-1,-1):
        sol[i]=(b[i]-sum(a[i][j]*sol[j] for j in range(i+1,n)))/a[i][i]
    mean=sum(sol)/n
    sol=[x-mean for x in sol]
    dense_error=max(abs(sol[i]-(q[i]-qmean)) for i in range(n))
    close(dense_error,0,1e-8,"Independent dense/analytic pressure")
    result["dense_pressure_max_error_pa"]=dense_error
    for key,i in (("p0",0),("pmid",ci(nx//2,ny//2,nz//2)),("plast",n-1)):
        result[key]=sol[i]
    return result
def parse_native(text:str)->dict:
    rows={}
    for line in text.splitlines():
        if not line.startswith("MANUFACTURED_3D "):continue
        m=LINE.fullmatch(line)
        if not m:raise ValueError("Malformed native measurement")
        nx,ny,nz=map(int,m.group(1,2,3))
        key=f"{nx}x{ny}x{nz}"
        if key in rows:raise ValueError("Duplicate native grid")
        keys=("pmax","vmax","div_before","div_after","energy_residual",
              "momentum_residual","p0","pmid","plast","u111","v111","w111")
        vals={k:float(v) for k,v in zip(keys,m.groups()[3:15])}
        vals["pcg_iterations"]=int(m.group(16))
        if not all(math.isfinite(x) for x in vals.values()):
            raise ValueError("Nonfinite native measurement")
        if vals["pmax"]>=3e-6 or vals["vmax"]>=3e-7 or vals["div_after"]>=1e-9 or (
            abs(vals["energy_residual"])>=1e-8 or vals["momentum_residual"]>=1e-9 or
            vals["pcg_iterations"]<1 or vals["pcg_iterations"]>=4000):
            raise ValueError("Native pressure/divergence/ledger bound failed")
        ref=reference(nx,ny,nz)
        for k in ("p0","pmid","plast","u111","v111","w111"):
            close(vals[k],ref[k],3e-6 if k.startswith("p") else 3e-7,k)
        close(vals["div_before"],ref["div_before"],1e-9,"Independent divergence")
        rows[key]={"native":vals,"independent_reference":ref}
    if set(rows)!={"4x3x3","6x4x4","8x6x5"}:
        raise ValueError("Missing independent 3D grid references")
    match=re.search(r"^PASS: TWO_PHASE_MAC_PRESSURE assertions=(\d+)$",text,re.M)
    if not match or int(match.group(1))<2600:
        raise ValueError("Native assertions missing")
    return rows
def main()->None:
    for f in (BINARY,NATIVE,BUILD,CTEST):
        if not f.is_file():raise ValueError(f"Missing actual build artifact: {f}")
    native_text=NATIVE.read_text(encoding="utf-8")
    rows=parse_native(native_text)
    ctest_text=CTEST.read_text(encoding="utf-8")
    if "100% tests passed, 0 tests failed out of 135" not in ctest_text:
        raise ValueError("Complete native CTest suite missing")
    build_text=BUILD.read_text(encoding="utf-8",errors="replace")
    if "upc_p2_two_phase_mac_pressure_tests" not in build_text:
        raise ValueError("MSVC target build not documented")
    replay=subprocess.run([str(BINARY)],cwd=CORE,capture_output=True,text=True,timeout=30)
    if replay.returncode!=0 or parse_native(replay.stdout)!=rows:
        raise ValueError("Native binary reproducibility mismatch")
    corruptions={
        "pressure":re.sub(r" p0=[-+0-9.eE]+"," p0=9",native_text,count=1),
        "divergence":re.sub(r" div_after=[-+0-9.eE]+"," div_after=9",native_text,count=1),
        "assertions":re.sub(r"assertions=\d+","assertions=2",native_text,count=1),
        "missing_grid":"\n".join(line for line in native_text.splitlines()
            if "grid=8x6x5" not in line),
        "nonfinite":re.sub(r" pmax=[-+0-9.eE]+"," pmax=nan",native_text,count=1),
        "wrong_iteration":re.sub(r" pcg_iterations=\d+"," pcg_iterations=99999",native_text,count=1),
    }
    rejected={}
    for name,forged in corruptions.items():
        try:parse_native(forged)
        except (ValueError,TypeError,OverflowError):rejected[name]=True
        else:raise ValueError(f"Independent QA accepted forged {name} trace")
    historical=RUNS/"p2_marin_pcg_6s_partial.json"
    marin=json.loads(historical.read_text(encoding="utf-8"))
    if (marin.get("phase_complete") is not False or
        marin.get("height_rmse_threshold_met") is not False or
        marin.get("pressure_experimental_validation") is not False):
        raise ValueError("MARIN negative evidence changed")
    gate=subprocess.run(["py","-3",str(CORE/"scripts"/"phase_gate.py")],
        cwd=ROOT,capture_output=True,text=True,timeout=240)
    GATE.write_text(gate.stdout+gate.stderr,encoding="utf-8")
    if gate.returncode!=0 or "ALL_COMPLETE=False" not in gate.stdout or (
        "P2" not in gate.stdout.split("GATES_INCOMPLETE=")[-1]):
        raise ValueError("Official phase gate did not confirm P2 incomplete")
    source_hashes={name:sha(CORE/name) for name in SOURCES}
    report={
        "phase":"P2",
        "milestone":"variable_density_two_phase_closed_box_mac_pcg_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "independent_qa":rows,
        "negative_fixture_rejections":rejected,
        "historical_marin_6s_validation_pass":False,
        "historical_marin_6s_height_rmse_m":{
            k:v.get("rmse") for k,v in marin.get("metrics",{}).items()
            if k in ("H1_m","H2_m","H3_m","H4_m")},
        "limitations":[
            "Not integrated into live CoupledMacWater3D VOF/PLIC",
            "No resolved moving water-air interface or viscous stress matching",
            "No dynamic bidirectional fluid-solid coupling",
            "No MARIN H1-H4/P1-P8 full-window three-grid measured validation",
            "No Unreal 5.8 Native E2E for this operator",
        ],
        "native_assertions":int(re.search(
            r"^PASS: TWO_PHASE_MAC_PRESSURE assertions=(\d+)$",
            native_text,re.M).group(1)),
        "ctest_total":135,
        "source_sha256":source_hashes,
        "artifact_sha256":{
            "native_binary":sha(BINARY),
            "native_trace":sha(NATIVE),
            "build_log":sha(BUILD),
            "ctest_log":sha(CTEST),
            "phase_gate_log":sha(GATE),
            "historical_marin_6s_report":sha(historical),
        },
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print("INDEPENDENT_QA=PASS 3 grids, dense 36x36, 6 corruptions, 135 CTest")
    print("P2_FULL_ACCEPTANCE=False")
    print("REPORT="+str(REPORT))
if __name__=="__main__":
    main()
