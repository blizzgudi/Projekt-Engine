"""P2 PARTIAL: independent 3D water-weighted MAC quadrature acceptance."""
from __future__ import annotations
import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
PREFIX="p2_water_mac_inventory"
NATIVE=RUNS/(PREFIX+"_native_20261101.txt")
COMPILE=RUNS/(PREFIX+"_compile_20261101.txt")
CTEST=RUNS/(PREFIX+"_ctest_20261101.txt")
EXE=CORE/"build"/"upc_p2_water_mac_inventory_tests.exe"
OUT=RUNS/(PREFIX+"_partial.json")
SOURCES=[
    "include/upc/p2_water_mac_inventory3d.hpp",
    "include/upc/p2_viscous_mac_plic_sequence3d.hpp",
    "include/upc/p2_viscous_nonlinear_mac_plic3d.hpp",
    "tests/p2_water_mac_inventory3d_tests.cpp",
    "scripts/p2_water_mac_inventory_build.cmd",
    "scripts/p2_water_mac_inventory_acceptance.py",
    "docs/P2_WATER_MAC_INVENTORY_PARTIAL.md",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main()->None:
    native=NATIVE.read_text(encoding="utf-8")
    compile_log=COMPILE.read_text(encoding="utf-8")
    ctest=CTEST.read_text(encoding="utf-8")
    checks=[]
    def check(name,value,ok):
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    m=re.search(r"PASS: water MAC inventory assertions=(\d+) phase_complete=false",native)
    count=int(m.group(1)) if m else 0
    check("native_assertions",count,count>=40)
    check("native_no_fail","FAIL:" not in native,"FAIL:" not in native)
    check("msvc_release_compile",compile_log.strip(),
          bool(compile_log.strip()) and
          "error C" not in compile_log and "fatal error" not in compile_log.lower())
    check("full_ctest",ctest.splitlines()[-1] if ctest else "",
          "100% tests passed" in ctest and "0 tests failed out of 106" in ctest)
    seen=set()
    for line in native.splitlines():
        p=line.split()
        if not p or p[0] not in ("UNIFORM","SHEAR","CURL"):
            continue
        kind=p[0]
        n=int(p[1])
        if n not in (8,12) or (kind,n) in seen:
            raise AssertionError("Unexpected or duplicate 3D grid")
        seen.add((kind,n))
        observed=list(map(float,p[2:]))
        if kind=="UNIFORM":
            mass=n**3*.25
            expected=[mass,mass*.2,.5*mass*(.2**2+.1**2+.05**2)]
        else:
            mass=0
            momentum=[0.0,0.0,0.0]
            energy=0.0
            for k in range(n):
                for j in range(n):
                    for i in range(n):
                        if kind=="SHEAR":
                            f=.5 if j<n//2 else 0.0
                            vel=[.03+.04*math.sin(2*math.pi*j/n),0.0,0.0]
                        else:
                            f=((i+2*j+3*k)%5)/4
                            vel=[.02+.04*math.sin(2*math.pi*j/n),
                                 .01*math.cos(2*math.pi*k/n),
                                 .03*math.sin(2*math.pi*i/n)]
                        mass+=f
                        for c in range(3):
                            momentum[c]+=f*vel[c]
                            energy+=.5*f*vel[c]**2
            expected=([mass,momentum[0],energy] if kind=="SHEAR" else
                      [mass,*momentum,energy])
        check(f"{kind}_{n}_shape",len(observed),len(observed)==len(expected))
        for ix,(actual,reference) in enumerate(zip(observed,expected)):
            check(f"{kind}_{n}_field_{ix}",actual,
                  math.isfinite(actual) and abs(actual-reference)<5e-9)
    check("six_independent_grid_references",sorted(seen),
          seen=={(kind,n) for kind in ("UNIFORM","SHEAR","CURL") for n in (8,12)})
    sequence=[line.split() for line in native.splitlines()
              if line.startswith("SEQUENCE ")]
    if len(sequence)!=1 or len(sequence[0])!=7:
        raise AssertionError("Missing real sequence endpoint measurements")
    before_mass,after_mass,kin0,kin1,delta,residual=map(float,sequence[0][1:])
    check("sequence_initial_mass",before_mass,abs(before_mass-432)<1e-8)
    check("sequence_final_mass",after_mass,abs(after_mass-432)<1e-8)
    check("sequence_mass_residual",residual,abs(residual)<1e-8)
    check("sequence_positive_kinetic",kin1,kin0>0 and kin1>0)
    check("sequence_energy_delta_is_measurement",delta,
          abs(delta-(kin1-kin0))<1e-10)
    check("twelve_negative_inputs","NEGATIVE 12" in native,
          "NEGATIVE 12" in native)
    hashes={name:sha(CORE/name) for name in SOURCES}
    check("source_hash_coverage",len(hashes),len(hashes)==7)
    accepted=all(x["pass"] for x in checks)
    report={
        "phase":"P2",
        "milestone":"water_weighted_mac_momentum_kinetic_inventory_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":count,"checks":checks,
        "source_sha256":hashes,
        "native_binary_sha256":sha(EXE),
        "native_log_sha256":sha(NATIVE),
        "compile_log_sha256":sha(COMPILE),
        "ctest_log_sha256":sha(CTEST),
        "limitations":[
            "Water VOF weighted cell-center quadrature; not a conservative two-phase momentum solver",
            "Subcell interface velocity unresolved; changes are diagnostic, not conservation proof",
            "No dynamic FSI, measured 3D CFD, stress-consistent free surface, or UE5.8 native E2E",
            "P2-P8 full phases remain incomplete"
        ],
    }
    OUT.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"P2_WATER_MAC_INVENTORY_PARTIAL={accepted} "
          f"CHECKS={sum(x['pass'] for x in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={count} PHASE_COMPLETE=False")
    if not accepted:
        raise SystemExit(1)
if __name__=="__main__":
    main()
