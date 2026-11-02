"""Independent bounded QA for fixed-grid 3D two-phase shear; NEVER P2 full PASS."""
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
PREFIX="p2_two_phase_grid_shear"
BINARY=CORE/"build"/"upc_p2_two_phase_grid_shear_tests.exe"
NATIVE=RUNS/(PREFIX+"_native.txt")
BUILD=RUNS/(PREFIX+"_build.txt")
CTEST=RUNS/(PREFIX+"_ctest.txt")
GATE=RUNS/(PREFIX+"_phase_gate.txt")
REPORT=RUNS/(PREFIX+"_partial.json")
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_two_phase_grid_shear3d.hpp",
    "include/upc/p2_two_phase_shear_exchange3d.hpp",
    "tests/p2_two_phase_grid_shear3d_tests.cpp",
    "scripts/p2_two_phase_grid_shear_build.cmd",
    "scripts/p2_two_phase_grid_shear_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_TWO_PHASE_GRID_SHEAR_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
PLANAR=re.compile(r"GRID_PLANAR faces=(\d+) water_mass=([0-9.eE+-]+) air_mass=([0-9.eE+-]+) slip=([0-9.eE+-]+) impulse_y=([0-9.eE+-]+) energy_residual=([0-9.eE+-]+)")
GRID3D=re.compile(r"GRID_3D faces=(\d+) axes=(\d+),(\d+),(\d+) momentum_residual=([0-9.eE+-]+) energy_residual=([0-9.eE+-]+)")
PASS=re.compile(r"PASS: TWO_PHASE_GRID_SHEAR assertions=(\d+)")

def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def close(actual:float,expected:float,tol:float,name:str)->None:
    if not math.isfinite(actual) or abs(actual-expected)>tol:
        raise ValueError(f"{name}: {actual} != {expected} within {tol}")

def parse(log:str)->dict:
    lines=log.strip().splitlines()
    if len(lines)!=3:
        raise ValueError("Expected exactly two native references and one PASS")
    p=PLANAR.fullmatch(lines[0])
    g=GRID3D.fullmatch(lines[1])
    end=PASS.fullmatch(lines[2])
    if p is None or g is None or end is None or int(end[1])<325:
        raise ValueError("Missing native 3D grid references or negative tests")
    faces,wm,am,slip,impulse,energy=map(float,p.groups())
    f3,ax,ay,az,mom,e3=map(float,g.groups())
    if (faces, f3,ax,ay,az)!=(16.,54.,18.,18.,18.):
        raise ValueError("Incorrect 3D interface enumeration")
    close(wm,32,1e-10,"water mass")
    close(am,32*1.2*.001,1e-12,"air mass")
    # Independently computed implicit two-cell Couette finite-volume reference.
    dx=.1
    dt=.05
    mw=1000*dx**3
    ma=1.2*dx**3
    resistance=(dx/2)/1+(dx/2)/.1
    b=dt*dx**2/resistance
    d=1/(1+b*(1/mw+1/ma))
    close(slip,d,1e-13,"analytical 3D planar slip")
    close(impulse,b*d,1e-13,"analytical 3D planar impulse")
    close(energy,0,1e-11,"planar global energy")
    close(mom,0,1e-11,"3D global momentum")
    close(e3,0,1e-11,"3D global energy")
    return {
        "planar_interfaces":int(faces),
        "three_dimensional_interfaces":int(f3),
        "axis_faces":[int(ax),int(ay),int(az)],
        "water_mass_kg":wm,
        "air_mass_kg":am,
        "measured_slip_m_s":slip,
        "independent_slip_m_s":d,
        "measured_face_impulse_n_s":impulse,
        "independent_face_impulse_n_s":b*d,
        "max_global_momentum_residual_n_s":mom,
        "global_energy_residual_j":e3,
        "native_assertions":int(end[1]),
    }

def main()->None:
    native=NATIVE.read_text(encoding="utf-8")
    values=parse(native)
    build=BUILD.read_text(encoding="utf-8")
    ctest=CTEST.read_text(encoding="utf-8")
    if "upc_p2_two_phase_grid_shear_tests.exe" not in build:
        raise ValueError("Missing real MSVC binary build evidence")
    if "100% tests passed, 0 tests failed out of 133" not in ctest:
        raise ValueError("Full 133 native CTest suites did not pass")
    rerun=subprocess.run([str(BINARY)],capture_output=True,text=True,timeout=90)
    if rerun.returncode!=0 or rerun.stdout.strip()!=native.strip():
        raise ValueError("Independent native rerun differs from recorded log")
    mutations=[
        ("faces=16","faces=15"),
        ("slip=0.5686713768740307","slip=9"),
        ("impulse_y=0.0005169739789763915","impulse_y=9"),
        ("faces=54","faces=53"),
        ("axes=18,18,18","axes=18,18,17"),
        ("assertions=325","assertions=1"),
    ]
    rejected=0
    for old,new in mutations:
        forged=native.replace(old,new,1)
        if forged==native:
            raise ValueError(f"Mutation fixture not applied: {old}")
        try:parse(forged)
        except ValueError:rejected+=1
        else:raise ValueError(f"Forged native trace accepted: {old}")
    marin=RUNS/"p2_marin_pcg_6s_partial.json"
    history=json.loads(marin.read_text(encoding="utf-8"))
    if history.get("height_rmse_threshold_met") is not False or history.get("phase_complete") is not False:
        raise ValueError("Negative MARIN 6-second experimental finding lost")
    gate=subprocess.run([sys.executable,str(CORE/"scripts"/"phase_gate.py")],
                        capture_output=True,text=True,timeout=240)
    GATE.write_text(gate.stdout+gate.stderr,encoding="utf-8")
    if gate.returncode!=0 or "ALL_COMPLETE=False" not in gate.stdout:
        raise ValueError("Official phase gate did not retain incomplete status")
    if "P2" not in gate.stdout.split("GATES_INCOMPLETE=",1)[-1].splitlines()[0]:
        raise ValueError("Official gate incorrectly accepted P2")
    report={
        "phase":"P2",
        "milestone":"fixed_3d_grid_two_phase_tangential_impulse_energy_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "native_reference":values,
        "ctest_passed":133,
        "forged_native_traces_rejected":rejected,
        "historical_marin_6s_height_validation":"FAIL",
        "historical_marin_sha256":sha(marin),
        "limitations":[
            "Fixed sharp cell-centered 3D interface; not staggered MAC/VOF/PLIC advection",
            "No coupled two-phase normal stress, pressure projection, capillary energy or normal flux constraint",
            "No full >=6s MARIN H1-H4/P1-P8 three-grid measured validation",
            "No moving solid FSI or global complete multiphysics energy/momentum acceptance",
            "No UE5.8 Native E2E for this component",
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
    print(f"P2 GRID SHEAR PARTIAL PASS: {values['native_assertions']} native assertions, "
          f"133/133 CTest, {rejected} forged traces rejected; P2_FULL=false")

if __name__=="__main__":
    main()
