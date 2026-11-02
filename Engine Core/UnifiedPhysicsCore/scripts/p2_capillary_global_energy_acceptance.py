"""Independent P2 partial capillary energy audit. Never a P2 full gate."""
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
BINARY=CORE/"build"/"upc_p2_capillary_global_energy_tests.exe"
NATIVE=RUNS/"p2_capillary_global_energy_native.txt"
BUILD=RUNS/"p2_capillary_global_energy_build.txt"
CTEST=RUNS/"p2_capillary_global_energy_full_ctest.txt"
GATE=RUNS/"p2_capillary_global_energy_phase_gate.txt"
REPORT=RUNS/"p2_capillary_global_energy_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_two_phase_capillary_global_energy3d.hpp",
    "include/upc/p2_two_phase_capillary_mac_vof3d.hpp",
    "include/upc/p2_two_phase_momentum_viscous_vof3d.hpp",
    "tests/p2_two_phase_capillary_global_energy3d_tests.cpp",
    "scripts/p2_capillary_global_energy_acceptance.py",
    "docs/P2_CAPILLARY_GLOBAL_ENERGY_PARTIAL.md",
]
P_PLANAR=re.compile(r"CAP_ENERGY_PLANAR n=(\d+) area=(\S+) analytic=(\S+) phase_complete=false")
P_LIVE=re.compile(
    r"CAP_ENERGY_LIVE before_j=(\S+) after_j=(\S+) surface_delta_j=(\S+) "
    r"kinetic_delta_j=(\S+) potential_delta_j=(\S+) final_loss_j=(\S+) "
    r"unresolved_j=(\S+) water_residual_kg=(\S+) air_residual_kg=(\S+) phase_complete=false"
)
P_PASS=re.compile(r"PASS P2_CAPILLARY_GLOBAL_ENERGY assertions=(\d+) phase_complete=false")
NEGATIVE="CAP_ENERGY_NEGATIVE invalid_vof=reject nonfinite_velocity=reject underiterated_pcg=rollback phase_complete=false"

def digest(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def parse(log:str)->dict:
    lines=log.strip().splitlines()
    if len(lines)!=6 or lines[4]!=NEGATIVE:
        raise ValueError("Missing negative tests or unexpected trace")
    planar={}
    for line in lines[:3]:
        m=P_PLANAR.fullmatch(line)
        if not m:raise ValueError("Invalid planar trace")
        n=int(m[1]);area=float(m[2]);analytic=float(m[3])
        if n in planar or n not in {4,7,10} or not all(map(math.isfinite,[area,analytic])):
            raise ValueError("Bad planar grid")
        expected=(n*.1)**2
        if abs(area-expected)>1e-12 or abs(analytic-expected)>1e-12:
            raise ValueError("Analytic independent planar area mismatch")
        planar[n]=area
    if set(planar)!={4,7,10}:raise ValueError("Missing grids")
    m=P_LIVE.fullmatch(lines[3])
    if not m:raise ValueError("Missing live energy fields")
    v=list(map(float,m.groups()))
    if not all(map(math.isfinite,v)):raise ValueError("Nonfinite energy")
    before,after,surface,kinetic,potential,loss,gap,water,air=v
    if not (before>0 and after>0 and loss>=0 and abs(water)<1e-9 and abs(air)<1e-9):
        raise ValueError("Unphysical or missing mass/energy diagnostics")
    if abs((after-before)-(surface+kinetic+potential))>1e-10:
        raise ValueError("Independent global energy partition mismatch")
    if abs(gap-(after-before+loss))>1e-10:
        raise ValueError("Open global energy residual hidden")
    if not (abs(gap)>1e-9 and abs(surface)>1e-12):
        raise ValueError("Negative energy closure evidence missing")
    m=P_PASS.fullmatch(lines[5])
    if not m or int(m[1])<39:raise ValueError("Missing native assertions")
    return {"planar_area_m2":planar,"before_j":before,"after_j":after,
        "surface_delta_j":surface,"kinetic_delta_j":kinetic,
        "potential_delta_j":potential,"last_viscous_pressure_loss_j":loss,
        "unresolved_global_energy_j":gap,"water_mass_residual_kg":water,
        "air_mass_residual_kg":air,"native_assertions":int(m[1])}

def tamper(log:str)->int:
    attacks=[
        ("n=4 area=","n=5 area="),
        ("phase_complete=false","phase_complete=true"),
        ("CAP_ENERGY_LIVE before_j=","CAP_ENERGY_LIVE before_j=NaN"),
        ("unresolved_j=","unresolved_j=0"),
        ("CAP_ENERGY_NEGATIVE invalid_vof=reject","CAP_ENERGY_NEGATIVE invalid_vof=pass"),
        ("assertions=39","assertions=0"),
    ]
    count=0
    for a,b in attacks:
        if a not in log:raise ValueError("Tamper source missing: "+a)
        try:parse(log.replace(a,b,1))
        except (ValueError,OverflowError):count+=1
        else:raise ValueError("Forged trace accepted: "+a)
    return count

def read_log(p:Path)->str:
    data=p.read_bytes()
    encoding="utf-16" if data.startswith((b"\xff\xfe",b"\xfe\xff")) else "utf-8-sig"
    return data.decode(encoding).replace("\r\n","\n")

def main()->None:
    for p in [BINARY,NATIVE,BUILD,CTEST,GATE,*[CORE/x for x in SOURCES]]:
        if not p.is_file():raise RuntimeError("Missing audit artifact: "+str(p))
    raw=read_log(NATIVE)
    metrics=parse(raw)
    result=subprocess.run([str(BINARY)],cwd=CORE,capture_output=True,text=True,timeout=120)
    if result.returncode!=0 or result.stdout.strip()!=raw.strip():
        raise RuntimeError("Native rerun disagrees with recorded evidence")
    ctest=read_log(CTEST)
    if "100% tests passed, 0 tests failed out of 146" not in ctest:
        raise RuntimeError("Full 146/146 native CTest not proven")
    gate=read_log(GATE)
    if "GATES_INCOMPLETE=" not in gate or "P2" not in gate or "ALL_COMPLETE=False" not in gate:
        raise RuntimeError("Official full P2 phase gate not open")
    if tamper(raw)!=6:raise RuntimeError("Tamper QA incomplete")
    report={
        "phase":"P2","milestone":"live_diffuse_capillary_global_energy_inventory_partial",
        "partial_accepted":True,"phase_complete":False,"full_P2_accepted":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native_metrics":metrics,"independent_negative_tamper_rejections":6,
        "negative_results":{
            "unresolved_global_energy_nonzero":True,
            "surface_area_is_diffuse_proxy_not_PLIC":True,
            "full_physical_energy_conservation_validated":False,
            "static_droplet_spurious_current_grid_convergence":False,
            "MARIN_6s_H1_H4_P1_P8_three_grid_validated":False,
            "bidirectional_dynamic_FSI_validated":False,
            "UE58_native_E2E_for_new_chain":False,
        },
        "source_sha256":{s:digest(CORE/s) for s in SOURCES},
        "artifact_sha256":{p.name:digest(p) for p in [BINARY,NATIVE,BUILD,CTEST,GATE]},
    }
    RUNS.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print("P2_CAPILLARY_ENERGY_QA=PARTIAL_PASS")
    print("UNRESOLVED_GLOBAL_ENERGY_J="+str(metrics["unresolved_global_energy_j"]))
    print("TAMPER_REJECTIONS=6")
    print("P2_FULL_ACCEPTED=False")
    print("REPORT="+str(REPORT))

if __name__=="__main__":main()
