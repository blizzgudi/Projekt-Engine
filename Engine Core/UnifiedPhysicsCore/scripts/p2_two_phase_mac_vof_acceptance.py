"""Independent 3D analytic-curl/finite-volume audit of a moving water-air
VOF field with variable-density MAC projection. P2 PARTIAL ONLY.
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
PREFIX="p2_two_phase_mac_vof"
BINARY=CORE/"build"/"upc_p2_two_phase_mac_vof_tests.exe"
NATIVE=RUNS/(PREFIX+"_native.txt")
BUILD=RUNS/(PREFIX+"_build.txt")
CTEST=RUNS/(PREFIX+"_ctest.txt")
GATE=RUNS/(PREFIX+"_phase_gate.txt")
REPORT=RUNS/(PREFIX+"_partial.json")
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_two_phase_mac_pressure3d.hpp",
    "include/upc/p2_conservative_vof_flux3d.hpp",
    "include/upc/p2_two_phase_mac_vof3d.hpp",
    "tests/p2_two_phase_mac_vof3d_tests.cpp",
    "scripts/p2_two_phase_mac_vof_build.cmd",
    "scripts/p2_two_phase_mac_vof_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_TWO_PHASE_MOVING_MAC_VOF_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
LINE=re.compile(
    r"^MOVING_TWO_PHASE_VOF grid=(\d+)x(\d+)x(\d+) steps=(\d+)"
    r" water_mass=([-+0-9.eE]+) air_mass=([-+0-9.eE]+)"
    r" mass_residual=([-+0-9.eE]+) pmax=([-+0-9.eE]+)"
    r" u_err=([-+0-9.eE]+) vof_err=([-+0-9.eE]+)"
    r" vof_delta=([-+0-9.eE]+) w111=([-+0-9.eE]+)"
    r" f000=([-+0-9.eE]+) fmid=([-+0-9.eE]+)"
    r" flast=([-+0-9.eE]+) div_after=([-+0-9.eE]+)"
    r" max_courant=([-+0-9.eE]+) pcg_iterations=(\d+)$")
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def close(a:float,b:float,tol:float,name:str)->None:
    if not math.isfinite(a) or abs(a-b)>tol:
        raise ValueError(f"{name}: actual={a}, expected={b}, tol={tol}")
def reference(nx:int,ny:int,nz:int)->dict:
    # Analytic edge potentials -> discrete curl, all six wall-normal
    # components zero; independent cell-centered FV donor flux for 8 steps.
    pi=math.pi
    dx=.1;dt=.015
    ci=lambda x,y,z:(z*ny+y)*nx+x
    ui=lambda x,y,z:(z*ny+y)*(nx+1)+x
    vi=lambda x,y,z:(z*(ny+1)+y)*nx+x
    wi=lambda x,y,z:(z*ny+y)*nx+x
    def az(x:int,y:int,z:int)->float:
        if x in (0,nx) or y in (0,ny):return 0.
        return .012*math.sin(pi*x/nx)*math.sin(pi*y/ny)*math.sin(pi*(z+.5)/nz)
    def ay(x:int,y:int,z:int)->float:
        if x in (0,nx) or z in (0,nz):return 0.
        return .009*math.sin(pi*x/nx)*math.sin(pi*z/nz)*math.sin(pi*(y+.5)/ny)
    u=[0.]*((nx+1)*ny*nz)
    v=[0.]*(nx*(ny+1)*nz)
    w=[0.]*(nx*ny*(nz+1))
    for z in range(nz):
        for y in range(ny):
            for x in range(1,nx):
                u[ui(x,y,z)]=(az(x,y+1,z)-az(x,y,z)-
                    ay(x,y,z+1)+ay(x,y,z))/dx
    for z in range(nz):
        for y in range(1,ny):
            for x in range(nx):
                v[vi(x,y,z)]=-(az(x+1,y,z)-az(x,y,z))/dx
    for z in range(1,nz):
        for y in range(ny):
            for x in range(nx):
                w[wi(x,y,z)]=(ay(x+1,y,z)-ay(x,y,z))/dx
    fraction=[0.]*(nx*ny*nz)
    for z in range(nz):
        for y in range(ny):
            for x in range(nx):
                fraction[ci(x,y,z)]=.08+(.72 if x<nx//2 and z<nz//2 else 0)+.04*y/(ny-1)
    initial=fraction[:]
    max_courant=0.
    for step in range(8):
        updated=fraction[:]
        for z in range(nz):
            for y in range(ny):
                for x in range(nx):
                    k=ci(x,y,z);c=fraction[k]
                    ux0=u[ui(x,y,z)];ux1=u[ui(x+1,y,z)]
                    vy0=v[vi(x,y,z)];vy1=v[vi(x,y+1,z)]
                    wz0=w[wi(x,y,z)];wz1=w[wi(x,y,z+1)]
                    get=lambda xx,yy,zz:fraction[ci(xx,yy,zz)]
                    # Independent local 6-face donor balance.
                    flux=ux1*(c if ux1>=0 else get(min(nx-1,x+1),y,z))-\
                        ux0*(get(max(0,x-1),y,z) if ux0>=0 else c)+\
                        vy1*(c if vy1>=0 else get(x,min(ny-1,y+1),z))-\
                        vy0*(get(x,max(0,y-1),z) if vy0>=0 else c)+\
                        wz1*(c if wz1>=0 else get(x,y,min(nz-1,z+1)))-\
                        wz0*(get(x,y,max(0,z-1)) if wz0>=0 else c)
                    updated[k]=c-dt/dx*flux
                    courant=dt/dx*(max(ux1,0)+max(-ux0,0)+
                        max(vy1,0)+max(-vy0,0)+max(wz1,0)+max(-wz0,0))
                    max_courant=max(max_courant,courant)
        fraction=updated
    cell=dx**3
    water=sum(initial)*cell*1000.
    air=(nx*ny*nz*cell-sum(initial)*cell)*1.2
    return {
        "water_mass":water,"air_mass":air,
        "w111":w[wi(1,1,1)],
        "f000":fraction[ci(0,0,0)],
        "fmid":fraction[ci(nx//2,ny//2,nz//2)],
        "flast":fraction[-1],
        "vof_delta":max(abs(a-b) for a,b in zip(initial,fraction)),
        "max_courant":max_courant,
        "independent_final_water_volume_m3":sum(fraction)*cell,
    }
def parse(text:str)->dict:
    rows={}
    for line in text.splitlines():
        if not line.startswith("MOVING_TWO_PHASE_VOF "):continue
        m=LINE.fullmatch(line)
        if not m:raise ValueError("Malformed native moving-interface measurement")
        nx,ny,nz,steps=map(int,m.group(1,2,3,4))
        key=f"{nx}x{ny}x{nz}"
        if key in rows or steps!=8:raise ValueError("Duplicate grid or wrong steps")
        keys=("water_mass","air_mass","mass_residual","pmax","u_err",
              "vof_err","vof_delta","w111","f000","fmid","flast",
              "div_after","max_courant")
        vals=dict(zip(keys,map(float,m.groups()[4:17])))
        vals["pcg_iterations"]=int(m.group(18))
        if not all(math.isfinite(x) for x in vals.values()):
            raise ValueError("Nonfinite native moving-interface result")
        if (abs(vals["mass_residual"])>=1e-9 or vals["pmax"]>=1e-6 or
            vals["u_err"]>=1e-7 or vals["vof_err"]>=1e-9 or
            vals["vof_delta"]<=1e-4 or vals["div_after"]>=1e-9 or
            vals["max_courant"]>=1 or vals["pcg_iterations"]<1):
            raise ValueError("Moving two-phase native conservation/pressure bound failed")
        ref=reference(nx,ny,nz)
        for name in ("water_mass","air_mass","w111","f000","fmid",
                     "flast","vof_delta","max_courant"):
            close(vals[name],ref[name],1e-9,name)
        rows[key]={"native":vals,"independent_reference":ref}
    if set(rows)!={"4x4x4","6x5x4","8x6x5"}:
        raise ValueError("Missing 3 independent 3D grids")
    match=re.search(r"^PASS: MOVING_TWO_PHASE_VOF assertions=(\d+)$",text,re.M)
    if not match or int(match.group(1))<5000:
        raise ValueError("Missing native assertions")
    return rows
def main()->None:
    for f in (BINARY,NATIVE,BUILD,CTEST):
        if not f.is_file():raise ValueError(f"Missing build artifact: {f}")
    native=NATIVE.read_text(encoding="utf-8")
    rows=parse(native)
    ctest=CTEST.read_text(encoding="utf-8")
    if "100% tests passed, 0 tests failed out of 136" not in ctest:
        raise ValueError("Complete 136 native CTest not verified")
    if "upc_p2_two_phase_mac_vof_tests" not in BUILD.read_text(encoding="utf-8",errors="replace"):
        raise ValueError("Native MSVC build target absent")
    replay=subprocess.run([str(BINARY)],cwd=CORE,capture_output=True,text=True,timeout=60)
    if replay.returncode or parse(replay.stdout)!=rows:
        raise ValueError("Native binary rerun not reproducible")
    corruptions={
        "water_mass":re.sub(r" water_mass=[-+0-9.eE]+"," water_mass=9",native,count=1),
        "vof_fraction":re.sub(r" fmid=[-+0-9.eE]+"," fmid=9",native,count=1),
        "vertical_velocity":re.sub(r" w111=[-+0-9.eE]+"," w111=9",native,count=1),
        "mass_residual":re.sub(r" mass_residual=[-+0-9.eE]+"," mass_residual=9",native,count=1),
        "missing_grid":"\n".join(x for x in native.splitlines() if "grid=8x6x5" not in x),
        "assertions":re.sub(r"assertions=\d+","assertions=2",native,count=1),
        "nonfinite":re.sub(r" pmax=[-+0-9.eE]+"," pmax=nan",native,count=1),
    }
    rejected={}
    forged_hashes={name:hashlib.sha256(value.encode("utf-8")).hexdigest() for name,value in corruptions.items()}
    for name,forged in corruptions.items():
        try:parse(forged)
        except (ValueError,TypeError,OverflowError):rejected[name]=True
        else:raise ValueError(f"Forged {name} native trace accepted")
    historical=RUNS/"p2_marin_pcg_6s_partial.json"
    marin=json.loads(historical.read_text(encoding="utf-8"))
    if (marin.get("phase_complete") is not False or
        marin.get("height_rmse_threshold_met") is not False or
        marin.get("pressure_experimental_validation") is not False):
        raise ValueError("Historical negative MARIN evidence missing")
    gate=subprocess.run(["py","-3",str(CORE/"scripts"/"phase_gate.py")],
        cwd=ROOT,capture_output=True,text=True,timeout=300)
    GATE.write_text(gate.stdout+gate.stderr,encoding="utf-8")
    if gate.returncode or "ALL_COMPLETE=False" not in gate.stdout or (
        "P2" not in gate.stdout.split("GATES_INCOMPLETE=")[-1]):
        raise ValueError("Official phase gate did not confirm P2 open")
    report={
        "phase":"P2",
        "milestone":"moving_variable_density_water_air_mac_vof_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "independent_qa":rows,
        "negative_fixture_rejections":rejected,
        "negative_fixture_sha256":forged_hashes,
        "native_assertions":int(re.search(
            r"^PASS: MOVING_TWO_PHASE_VOF assertions=(\d+)$",native,re.M).group(1)),
        "ctest_total":136,
        "historical_marin_6s_validation_pass":False,
        "historical_marin_6s_height_rmse_m":{
            k:v.get("rmse") for k,v in marin.get("metrics",{}).items()
            if k in ("H1_m","H2_m","H3_m","H4_m")},
        "limitations":[
            "Moving VOF with shared pressure only; no momentum self-advection",
            "No stress-consistent two-phase tangential/normal interface coupling",
            "No dynamic bidirectional fluid-solid pressure/viscous impulse",
            "No full-window 6s H1-H4/P1-P8 MARIN measured three-grid PASS",
            "No Unreal 5.8 Native E2E for this new operator",
        ],
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "artifact_sha256":{
            "native_binary":sha(BINARY),
            "native_trace":sha(NATIVE),
            "build_log":sha(BUILD),
            "ctest_log":sha(CTEST),
            "phase_gate_log":sha(GATE),
            "historical_marin_6s_report":sha(historical),
            "first_native_negative_excerpt":sha(RUNS/"p2_two_phase_mac_vof_initial_negative.txt"),
        },
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print("INDEPENDENT_QA=PASS 3 moving 3D grids, 8 steps, 7 corruptions, 136 CTest")
    print("P2_FULL_ACCEPTANCE=False")
    print("REPORT="+str(REPORT))
if __name__=="__main__":
    main()
