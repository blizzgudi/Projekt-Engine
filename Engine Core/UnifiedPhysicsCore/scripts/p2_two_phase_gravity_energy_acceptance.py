"""Independent native/physical audit for the P2 two-phase gravitational
potential-energy flux and explicitly UNRESOLVED mechanical-energy ledger.
Never certifies full P2 CFD/FSI or experimental convergence.
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
PREFIX="p2_two_phase_gravity_energy"
BINARY=CORE/"build"/"upc_p2_two_phase_gravity_energy_tests.exe"
NATIVE=RUNS/(PREFIX+"_native.txt")
BUILD=RUNS/(PREFIX+"_build.txt")
FULL_BUILD=RUNS/(PREFIX+"_full_build.txt")
CTEST=RUNS/(PREFIX+"_ctest.txt")
GATE=RUNS/(PREFIX+"_phase_gate.txt")
REPORT=RUNS/(PREFIX+"_partial.json")
MARIN=RUNS/"p2_marin_pcg_6s_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_two_phase_mac_vof3d.hpp",
    "include/upc/p2_two_phase_mac_pressure3d.hpp",
    "include/upc/p2_conservative_vof_flux3d.hpp",
    "tests/p2_two_phase_gravity_energy3d_tests.cpp",
    "scripts/p2_two_phase_gravity_energy_build.cmd",
    "scripts/p2_two_phase_gravity_energy_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_TWO_PHASE_GRAVITY_ENERGY_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
PATTERN=re.compile(
    r"^POTENTIAL grid=(\d+)x(\d+)x(\d+) oblique=([01])"
    r" flux_j=([-+0-9.eE]+) unclosed_j=([-+0-9.eE]+)"
    r" remap_j=([-+0-9.eE]+) flux_error_j=([-+0-9.eE]+)"
    r" steps=(\d+)$"
)
EXPECTED={
    ("4x4x4",0),("6x5x4",0),("6x5x4",1),("8x6x5",1)
}
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path:Path)->str:
    raw=path.read_bytes()
    return raw.decode("utf-16" if raw.startswith((b"\xff\xfe",b"\xfe\xff"))
                      else "utf-8",errors="replace")
def parse(text:str)->dict:
    text=text.replace("\r\n","\n").replace("\r","\n")
    rows={}
    for line in text.splitlines():
        if not line.startswith("POTENTIAL "):continue
        match=PATTERN.fullmatch(line)
        if match is None:raise ValueError("Malformed native potential trace")
        x,y,z,oblique=map(int,match.group(1,2,3,4))
        grid=f"{x}x{y}x{z}"
        key=(grid,oblique)
        if key in rows or int(match.group(9))!=6:
            raise ValueError("Duplicate or incomplete grid/steps")
        flux,unclosed,remap,error=map(float,match.group(5,6,7,8))
        if not all(map(math.isfinite,(flux,unclosed,remap,error))) or (
            min(flux,unclosed,remap,error)<0):
            raise ValueError("Nonfinite/negative absolute energy diagnostic")
        if error>2e-9:raise ValueError("3D donor-face potential flux did not close")
        if oblique:
            # These are intentionally nonzero NEGATIVE physical findings:
            # never accept a fabricated zero as full energy conservation.
            if not(flux>1e-5 and unclosed>1e-4 and remap>1e-8):
                raise ValueError("Missing observed moving-flow energy deficit")
        elif flux>1e-8 or unclosed>1e-8 or remap>1e-8:
            raise ValueError("Static hydrostatic potential inventory failed")
        rows[key]={"flux_j":flux,"unclosed_mechanical_energy_j":unclosed,
                   "density_remap_kinetic_delta_j":remap,
                   "potential_face_cell_residual_j":error}
    if set(rows)!=EXPECTED:raise ValueError("Required independent 3D grids missing")
    match=re.search(r"^PASS: TWO_PHASE_POTENTIAL_AUDIT assertions=(\d+)$",
                    text,re.M)
    if match is None or int(match.group(1))<200:
        raise ValueError("Native assertion proof missing")
    return {"grids":{f"{k[0]}_oblique{k[1]}":v for k,v in rows.items()},
            "native_assertions":int(match.group(1))}
def main()->None:
    for path in (BINARY,NATIVE,BUILD,FULL_BUILD,CTEST,MARIN):
        if not path.is_file():raise ValueError(f"Missing artifact: {path}")
    native=read(NATIVE)
    rows=parse(native)
    ctest=read(CTEST)
    if ("100% tests passed, 0 tests failed out of 138" not in ctest or
        "physics_p2_two_phase_gravity_energy_partial" not in ctest):
        raise ValueError("Native 138/138 CTest suite evidence missing")
    rerun=subprocess.run([str(BINARY)],cwd=CORE,capture_output=True,
                         text=True,timeout=60)
    if rerun.returncode!=0 or parse(rerun.stdout)!=rows:
        raise ValueError("Independent native rerun not reproducible")
    for label,corrupted in {
        "missing_grid":"\n".join(line for line in native.splitlines()
                                  if "grid=8x6x5" not in line),
        "fake_closed_energy":re.sub(
            r"(POTENTIAL grid=6x5x4 oblique=1 .*?)unclosed_j=[-+0-9.eE]+",
            r"\g<1>unclosed_j=0",native,count=1),
        "fake_moving_closure":re.sub(
            r"(POTENTIAL grid=8x6x5 oblique=1 .*?)unclosed_j=[-+0-9.eE]+",
            r"\g<1>unclosed_j=0",native,count=1),
        "bad_flux":re.sub(r"flux_error_j=[-+0-9.eE]+",
                          "flux_error_j=1",native,count=1),
        "nan_flux":re.sub(r"flux_j=[-+0-9.eE]+",
                          "flux_j=nan",native,count=1),
        "missing_assertions":re.sub(r"assertions=\d+",
                                    "assertions=1",native,count=1),
        "truncated_steps":native.replace("steps=6","steps=5",1),
    }.items():
        if corrupted==native:raise ValueError(f"No mutation: {label}")
        try:parse(corrupted)
        except (ValueError,TypeError):pass
        else:raise ValueError(f"Forged native trace accepted: {label}")
    historical=json.loads(MARIN.read_text(encoding="utf-8"))
    if (historical.get("phase_complete") is not False or
        historical.get("height_rmse_threshold_met") is not False or
        historical.get("pressure_experimental_validation") is not False):
        raise ValueError("Historical negative MARIN measurement evidence missing")
    gate=subprocess.run(["py","-3",str(CORE/"scripts"/"phase_gate.py")],
                        cwd=ROOT,capture_output=True,text=True,timeout=300)
    GATE.write_text(gate.stdout+gate.stderr,encoding="utf-8")
    if gate.returncode or "ALL_COMPLETE=False" not in gate.stdout or (
        "P2" not in gate.stdout.split("GATES_INCOMPLETE=")[-1]):
        raise ValueError("Official full P2 phase gate must remain PARTIAL")
    report={
        "phase":"P2","milestone":"moving_two_phase_gravity_potential_inventory_partial",
        "partial_accepted":True,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "independent_qa":rows,
        "negative_trace_forgery_rejections":7,
        "historical_marin_6s_validation_pass":False,
        "measured_unclosed_mechanical_energy_j":{
            k:v["unclosed_mechanical_energy_j"]
            for k,v in rows["grids"].items() if k.endswith("oblique1")},
        "limitations":[
            "Conservative 3D VOF potential flux identity is NOT a global mechanical-energy closure",
            "Missing conservative momentum advection and stress-consistent moving water-air boundary",
            "No dynamic bidirectional FSI or independent full-window MARIN H1-H4/P1-P8 three-grid measurement convergence",
            "No UE5.8 native rendering E2E for this new path",
        ],
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "artifact_sha256":{
            "native_binary":sha(BINARY),"native_trace":sha(NATIVE),
            "build_log":sha(BUILD),"full_build_log":sha(FULL_BUILD),
            "ctest_log":sha(CTEST),"phase_gate_log":sha(GATE),
            "historical_negative_marin":sha(MARIN),
        },
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print("INDEPENDENT_QA=PASS 4 native 3D cases, 7 forgery rejections, 138 CTest")
    print("MECHANICAL_ENERGY_CLOSURE=False")
    print("P2_FULL_ACCEPTANCE=False")
    print("REPORT="+str(REPORT))
if __name__=="__main__":main()
