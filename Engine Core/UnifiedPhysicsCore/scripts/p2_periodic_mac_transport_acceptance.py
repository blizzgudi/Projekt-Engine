"""Independent P2 PARTIAL gate for conservative periodic linear MAC transport.

The analytic checkerboard eigenmode of the 3D first-order upwind operator
has G=1-2*sum(Courant). This is not a nonlinear CFD or FSI acceptance.
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
LAB=CORE.parents[1]/"ENGINE TEST STUBE"
LOG=LAB/"runs"/"p2_periodic_mac_full_ctest_20261028.txt"
OUT=LAB/"runs"/"p2_periodic_mac_transport_partial.json"
EXE=CORE/"build"/"upc_p2_periodic_mac_transport_tests.exe"

def control(name: str, actual: float, expected: float, tolerance: float):
    if not math.isfinite(actual) or abs(actual-expected)>tolerance:
        raise AssertionError(f"{name}: {actual} != {expected} +/- {tolerance}")
    return {"name":name,"actual":actual,"expected":expected,
            "tolerance":tolerance,"pass":True}

def main():
    log=LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 64" not in log:
        raise AssertionError("Full 64-suite CTest regression missing")
    if "physics_p2_periodic_mac_linear_transport_partial" not in log:
        raise AssertionError("New periodic MAC CTest missing from full regression")
    proc=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                        text=True,check=True,timeout=180)
    checks=[]
    seen=set()
    for line in proc.stdout.splitlines():
        if not line.startswith("TRANSPORT "):
            continue
        fields=line.split()
        if len(fields)!=11:
            raise AssertionError(f"Malformed transport output: {line}")
        _,nx,ny,nz,sx,sy,sz,step,maxerr,sumerr,ratio=fields
        nx,ny,nz,sx,sy,sz,step=map(int,(nx,ny,nz,sx,sy,sz,step))
        maxerr,sumerr,ratio=map(float,(maxerr,sumerr,ratio))
        key=(nx,ny,nz,sx,sy,sz,step)
        if ((nx,ny,nz) not in ((8,10,12),(12,14,16),(16,18,20))
                or (sx,sy,sz) not in tuple(
                    (x,y,z) for x in (-1,1) for y in (-1,1) for z in (-1,1))
                or step not in range(1,6) or key in seen):
            raise AssertionError(f"Invalid/duplicate transport case: {key}")
        seen.add(key)
        # Independent analytical Fourier eigenvalue for k=(pi,pi,pi).
        cfl=(abs(sx*0.07)+abs(sy*0.11)+abs(sz*0.13))
        amplification=1.0-2.0*cfl
        checks.append(control(f"{key}_amplitude_error",maxerr,0.0,1e-12))
        checks.append(control(f"{key}_momentum_sum_error",sumerr,0.0,2e-9))
        checks.append(control(f"{key}_L2_eigenmode_ratio",ratio,
                              amplification**(2*step),2e-12))
        checks.append(control(f"{key}_monotone_dissipation",
                              float(0.0<=ratio<=1.0),1.0,0.0))
    if len(seen)!=120 or len(checks)!=480:
        raise AssertionError("Grid, direction, and five-step coverage incomplete")
    match=re.search(
        r"PASS:\s+(\d+)\s+native periodic MAC linear-transport assertions",
        proc.stdout)
    if not match or int(match.group(1))<1000000:
        raise AssertionError("Native periodic seam/rollback/invariant coverage absent")
    files=[
        "include/upc/p2_periodic_mac_transport3d.hpp",
        "tests/p2_periodic_mac_transport3d_tests.cpp",
        "scripts/p2_periodic_mac_transport_acceptance.py",
        "docs/P2_PERIODIC_MAC_LINEAR_TRANSPORT_PARTIAL.md",
        "docs/PHASENPLAN.md",
        "CMakeLists.txt",
    ]
    hashes={name:hashlib.sha256((CORE/name).read_bytes()).hexdigest()
            for name in files}
    report={
        "phase":"P2",
        "milestone":"periodic_staggered_mac_linear_transport_partial",
        "partial_accepted":True,"phase_complete":False,"accepted":False,
        "full_ctest_suites":64,
        "native_assertions":int(match.group(1)),
        "independent_numerical_checks":len(checks),
        "checks":checks,
        "ctest_log_sha256":hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256":hashes,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "limitations":[
            "Only constant prescribed linear transport velocity on periodic grid",
            "No nonlinear staggered-MAC Navier-Stokes self-advection",
            "No moving cut-cell, free surface, pressure coupling or two-way FSI",
            "Numerical L2 decay is not a physical total energy/impulse budget",
            "No independent measured 3D CFD or Unreal 5.8 Native E2E",
            "P2-P8 full phases remain open",
        ],
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"PASS: {len(checks)}/{len(checks)} independent numerical checks; "
          f"{report['native_assertions']} native assertions; "
          "64/64 CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")

if __name__=="__main__":
    main()
