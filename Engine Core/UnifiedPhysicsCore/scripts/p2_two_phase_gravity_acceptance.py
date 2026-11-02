"""Independent audit for opt-in gravity on moving 3D two-phase MAC/VOF.
Bounded P2 milestone only. Never marks P2 fully accepted.
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
P="p2_two_phase_gravity"
BINARY=CORE/"build"/"upc_p2_two_phase_gravity_tests.exe"
NATIVE=RUNS/(P+"_native.txt")
BUILD=RUNS/(P+"_build.txt")
FULL_BUILD=RUNS/(P+"_full_build.txt")
CTEST=RUNS/(P+"_ctest.txt")
NEGATIVE=RUNS/(P+"_initial_negative.txt")
GATE=RUNS/(P+"_phase_gate.txt")
REPORT=RUNS/(P+"_partial.json")
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_two_phase_mac_vof3d.hpp",
    "include/upc/p2_two_phase_mac_pressure3d.hpp",
    "include/upc/p2_conservative_vof_flux3d.hpp",
    "tests/p2_two_phase_gravity3d_tests.cpp",
    "scripts/p2_two_phase_gravity_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_TWO_PHASE_GRAVITY_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
HYDRO=re.compile(
    r"^GRAVITY_HYDRO grid=(\d+)x(\d+)x(\d+)"
    r" pressure_error_pa=([-+0-9.eE]+) velocity_error_m_s=([-+0-9.eE]+)"
    r" energy_residual_j=([-+0-9.eE]+) steps=(\d+)$")
TILTED=re.compile(
    r"^GRAVITY_TILTED grid=(\d+)x(\d+)x(\d+)"
    r" u_m_s=([-+0-9.eE]+) w_m_s=([-+0-9.eE]+)"
    r" alpha_delta=([-+0-9.eE]+) div_s_inv=([-+0-9.eE]+)"
    r" mass_error_kg=([-+0-9.eE]+) energy_residual_j=([-+0-9.eE]+)"
    r" steps=(\d+)$")
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()
def read_log(p:Path)->str:
    raw=p.read_bytes()
    return raw.decode("utf-16" if raw.startswith((b"\xff\xfe",b"\xfe\xff")) else "utf-8",errors="replace")
def hydro_reference(nz:int)->float:
    # Independent analytic hydrostatic pressure drop for the known
    # manufactured layered mixture; no production code or pressure solver.
    rho=[1.2+998.8*(.90 if z<nz//2 else .05) for z in range(nz)]
    return sum(-9.81*.1*.5*(rho[z-1]+rho[z]) for z in range(1,nz))
def parse(s:str)->dict:
    s=s.replace("\r\n","\n").replace("\r","\n")
    hydro={};tilt={}
    for line in s.splitlines():
        if line.startswith("GRAVITY_HYDRO "):
            m=HYDRO.fullmatch(line)
            if not m:raise ValueError("Malformed hydrostatic measurement")
            nx,ny,nz=map(int,m.group(1,2,3))
            key=f"{nx}x{ny}x{nz}"
            if key in hydro or int(m.group(7))!=4:raise ValueError("Duplicate/wrong hydro grid")
            err,vel,energy=map(float,m.group(4,5,6))
            if not all(map(math.isfinite,(err,vel,energy))) or min(err,vel,energy)<0:
                raise ValueError("Nonfinite/negative hydro error")
            if err>1e-7 or vel>1e-7 or energy>1e-8:
                raise ValueError("Hydrostatic gravity reference failed")
            hydro[key]={"pressure_error_pa":err,"velocity_error_m_s":vel,
                        "energy_residual_j":energy,
                        "independent_expected_top_minus_bottom_pressure_pa":
                        hydro_reference(nz)}
        elif line.startswith("GRAVITY_TILTED "):
            m=TILTED.fullmatch(line)
            if not m:raise ValueError("Malformed tilted measurement")
            nx,ny,nz=map(int,m.group(1,2,3))
            key=f"{nx}x{ny}x{nz}"
            if key in tilt or int(m.group(10))!=8:raise ValueError("Duplicate/wrong tilted grid")
            u,w,alpha,div,mass,energy=map(float,m.group(4,5,6,7,8,9))
            if not all(map(math.isfinite,(u,w,alpha,div,mass,energy))):
                raise ValueError("Nonfinite tilted gravity result")
            if not(u>1e-5 and w>1e-5 and alpha>1e-6 and
                   0<=div<1e-9 and 0<=mass<1e-8 and 0<=energy<1e-8):
                raise ValueError("Tilted gravity moving-interface budget failed")
            tilt[key]={"u_m_s":u,"w_m_s":w,"alpha_delta":alpha,
                       "div_s_inv":div,"mass_error_kg":mass,
                       "energy_residual_j":energy}
    if set(hydro)!={"4x4x4","6x5x4","8x6x5"} or set(tilt)!={"6x5x4","8x6x5"}:
        raise ValueError("Missing independent hydro/tilted 3D grids")
    m=re.search(r"^PASS: GRAVITY_TWO_PHASE_VOF assertions=(\d+)$",s,re.M)
    if not m or int(m.group(1))<1000:raise ValueError("Missing native assertion proof")
    return {"hydrostatic":hydro,"tilted":tilt,"native_assertions":int(m.group(1))}
def main()->None:
    for p in (BINARY,NATIVE,BUILD,FULL_BUILD,CTEST,NEGATIVE):
        if not p.is_file():raise ValueError(f"Missing native artifact: {p}")
    native=read_log(NATIVE)
    rows=parse(native)
    ctest=read_log(CTEST)
    if "100% tests passed, 0 tests failed out of 137" not in ctest or (
        "physics_p2_two_phase_gravity_partial" not in ctest):
        raise ValueError("Full native CTest regression missing")
    negative=read_log(NEGATIVE)
    if not ("p2_two_phase_mac_vof_step3d" in negative and
            "gravity_source" in negative):
        raise ValueError("Initial negative compilation evidence missing")
    replay=subprocess.run([str(BINARY)],cwd=CORE,capture_output=True,text=True,timeout=60)
    if replay.returncode or parse(replay.stdout)!=rows:
        raise ValueError("Native gravity measurement not reproducible")
    # Falsification tests: tamper individual measurements and the PASS
    # assertion count, then require the independent parser to reject them.
    corruptions={
        "pressure":re.sub(r"pressure_error_pa=[-+0-9.eE]+",
                          "pressure_error_pa=1",native,count=1),
        "hydro_velocity":re.sub(r"velocity_error_m_s=[-+0-9.eE]+",
                                "velocity_error_m_s=1",native,count=1),
        "moving_mass":re.sub(r"mass_error_kg=[-+0-9.eE]+",
                             "mass_error_kg=1",native,count=1),
        "moving_vertical":re.sub(r" w_m_s=[-+0-9.eE]+",
                                 " w_m_s=0",native,count=1),
        "missing_grid":"\n".join(x for x in native.splitlines()
                                  if "grid=8x6x5" not in x),
        "assertions":re.sub(r"assertions=\d+","assertions=1",native,count=1),
        "nonfinite":re.sub(r"div_s_inv=[-+0-9.eE]+",
                           "div_s_inv=nan",native,count=1),
    }
    rejected={}
    for key,forged in corruptions.items():
        if forged==native:raise ValueError(f"Mutation did not change {key}")
        try:parse(forged)
        except (ValueError,TypeError):rejected[key]=True
        else:raise ValueError(f"Corrupt native trace accepted: {key}")
    historical=RUNS/"p2_marin_pcg_6s_partial.json"
    marin=json.loads(historical.read_text(encoding="utf-8"))
    if (marin.get("phase_complete") is not False or
        marin.get("height_rmse_threshold_met") is not False or
        marin.get("pressure_experimental_validation") is not False):
        raise ValueError("Historical negative 6s MARIN evidence unavailable")
    gate=subprocess.run(["py","-3",str(CORE/"scripts"/"phase_gate.py")],
                        cwd=ROOT,capture_output=True,text=True,timeout=300)
    GATE.write_text(gate.stdout+gate.stderr,encoding="utf-8")
    if gate.returncode or "ALL_COMPLETE=False" not in gate.stdout or (
        "P2" not in gate.stdout.split("GATES_INCOMPLETE=")[-1]):
        raise ValueError("Official full P2 phase gate must remain open")
    report={
        "phase":"P2","milestone":"moving_two_phase_gravity_hydrostatic_partial",
        "partial_accepted":True,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "independent_qa":rows,"negative_fixture_rejections":rejected,
        "negative_fixture_sha256":{k:hashlib.sha256(v.encode()).hexdigest()
                                   for k,v in corruptions.items()},
        "ctest_total":137,"historical_marin_6s_validation_pass":False,
        "limitations":[
            "Discrete staggered-face gravity source/projection energy, not global potential energy closure",
            "No momentum self-advection or stress-consistent moving water-air interface",
            "No dynamic bidirectional solid impulse coupling",
            "No full-window MARIN H1-H4/P1-P8 independent measured grid convergence",
            "No Unreal 5.8 Native E2E for new gravity path",
        ],
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "artifact_sha256":{
            "native_binary":sha(BINARY),"native_trace":sha(NATIVE),
            "initial_negative":sha(NEGATIVE),"build_log":sha(BUILD),
            "full_build_log":sha(FULL_BUILD),"ctest_log":sha(CTEST),
            "phase_gate_log":sha(GATE),"historical_negative_marin":sha(historical),
        },
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print("INDEPENDENT_QA=PASS 3 hydrostatic + 2 tilted 3D grids, 7 tamper rejections, 137 CTest")
    print("P2_FULL_ACCEPTANCE=False")
    print("REPORT="+str(REPORT))
if __name__=="__main__":
    main()
