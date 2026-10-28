"""P2 PARTIAL: independent Fourier pressure and sequential MAC projection gate."""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
LAB = CORE.parents[1] / "ENGINE TEST STUBE"
LOG = LAB / "runs" / "p2_periodic_projection_full_ctest_20261028.txt"
OUT = LAB / "runs" / "p2_periodic_projection_partial.json"
EXE = CORE / "build" / "upc_p2_periodic_mac_projection_tests.exe"
GRIDS = {(8, 10, 12), (12, 14, 16), (16, 18, 20)}

def check(name, actual, expected, tolerance):
    if not math.isfinite(actual) or abs(actual-expected)>tolerance:
        raise AssertionError(f"{name}: actual={actual}, expected={expected}, tol={tolerance}")
    return {"name": name, "actual": actual, "expected": expected,
            "tolerance": tolerance, "pass": True}

def main():
    log = LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 66" not in log:
        raise AssertionError("Full 66-suite native CTest regression missing")
    if "physics_p2_periodic_nonlinear_mac_projection_partial" not in log:
        raise AssertionError("New native projection suite missing")
    run = subprocess.run([str(EXE)], cwd=CORE, text=True, capture_output=True,
                         check=True, timeout=180)
    checks, modes, steps = [], set(), set()
    for line in run.stdout.splitlines():
        fields = line.split()
        if not fields: continue
        if fields[0]=="MODE":
            if len(fields)!=10: raise AssertionError(f"Bad MODE: {line}")
            nx,ny,nz=map(int,fields[1:4])
            key=(nx,ny,nz)
            if key not in GRIDS or key in modes: raise AssertionError(f"Unexpected MODE: {key}")
            modes.add(key)
            p000,px,py,pz,maxerr,div=map(float,fields[4:])
            # Independently evaluate the periodic Fourier mode at four
            # lattice points. No C++ pressure values enter the reference.
            for label,value,expected in [
                ("p000",p000,160.0),("px",px,-40.0),
                ("py",py,80.0),("pz",pz,120.0)]:
                checks.append(check(f"{label}_{key}",value,expected,1e-7))
            checks.append(check(f"velocity_error_{key}",maxerr,0.0,1e-10))
            checks.append(check(f"divergence_{key}",div,0.0,1e-11))
        elif fields[0]=="STEP":
            if len(fields)!=7: raise AssertionError(f"Bad STEP: {line}")
            nx,ny,nz,step=map(int,fields[1:5])
            key=(nx,ny,nz,step)
            if (nx,ny,nz) not in GRIDS or step not in range(1,5) or key in steps:
                raise AssertionError(f"Unexpected STEP: {key}")
            steps.add(key)
            before,after=map(float,fields[5:])
            checks.append(check(f"nonlinear_nonzero_{key}",float(before>1e-6),1.0,0.0))
            checks.append(check(f"projected_divergence_{key}",after,0.0,1e-10))
    if len(modes)!=3 or len(steps)!=12 or len(checks)!=42:
        raise AssertionError(f"Coverage incomplete: modes={len(modes)}, steps={len(steps)}, checks={len(checks)}")
    match=re.search(r"PASS:\s+(\d+)\s+native periodic projected MAC assertions",run.stdout)
    if not match or int(match.group(1))<10000:
        raise AssertionError("Native periodic projected MAC assertions missing")
    files=[
        "include/upc/p2_periodic_mac_projection3d.hpp",
        "include/upc/p2_variable_mac_transport3d.hpp",
        "tests/p2_periodic_mac_projection3d_tests.cpp",
        "scripts/p2_periodic_projection_acceptance.py",
        "scripts/p2_periodic_projection_full_build.cmd",
        "docs/P2_PERIODIC_NONLINEAR_MAC_PROJECTION_PARTIAL.md",
        "docs/PHASENPLAN.md",
        "CMakeLists.txt",
    ]
    hashes={f:hashlib.sha256((CORE/f).read_bytes()).hexdigest() for f in files}
    report={
        "phase":"P2",
        "milestone":"periodic_nonlinear_mac_transport_pressure_projection_partial",
        "partial_accepted":True,"phase_complete":False,"accepted":False,
        "full_ctest_suites":66,"native_assertions":int(match.group(1)),
        "independent_numerical_checks":len(checks),"checks":checks,
        "ctest_log_sha256":hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256":hashes,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "limitations":[
            "Periodic fully filled constant-density 3D MAC domain only",
            "Pressure PCG manufactured modes are not external CFD measurements",
            "No free surface, moving cut cells, CoupledMacWater3D integration or two-way FSI",
            "Component integral conservation is not global physical momentum/energy closure",
            "No Unreal 5.8 Native E2E for this operator",
            "P2-P8 full phases remain open",
        ],
    }
    OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(f"PASS: {len(checks)}/{len(checks)} independent numerical checks; "
          f"{report['native_assertions']} native assertions; 66/66 CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")

if __name__=="__main__":
    main()
