#!/usr/bin/env python3
"""P2 PARTIAL: hydrostatic buoyancy, external gravity and fixed-level potential audit."""
from __future__ import annotations
import hashlib
import itertools
import json
import math
import re
import subprocess
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
LOG=RUNS/"p2_hydrostatic_gravity_full_ctest_20261029.txt"
EXE=CORE/"build"/"upc_p2_hydrostatic_gravity_tests.exe"
REPORT=RUNS/"p2_hydrostatic_gravity_partial.json"
SOURCES=[
    "CMakeLists.txt","docs/PHASENPLAN.md",
    "docs/P2_HYDROSTATIC_GRAVITY_PARTIAL.md",
    "include/upc/p2_hydrostatic_gravity3d.hpp",
    "include/upc/p2_stratified_hydrostatic_load3d.hpp",
    "include/upc/p2_moving_box_open_volume3d.hpp",
    "include/upc/rigid_pressure_response3d.hpp",
    "tests/p2_hydrostatic_gravity3d_tests.cpp",
    "scripts/p2_hydrostatic_gravity_acceptance.py",
    "scripts/p2_hydrostatic_gravity_full_build.cmd",
]
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()
def main()->None:
    checks=[]
    def check(name,ok,detail=None):
        checks.append({"name":name,"pass":bool(ok),"detail":detail})
        if not ok:
            raise RuntimeError(f"FAIL {name}: {detail}")
    log=LOG.read_text(encoding="utf-8",errors="replace")
    check("83/83 full CTest",
          "100% tests passed, 0 tests failed out of 83" in log)
    check("native gravity suite included",
          "physics_p2_hydrostatic_gravity_energy_partial" in log)
    native=subprocess.run([str(EXE)],capture_output=True,text=True,
                          timeout=120,check=True).stdout
    m=re.search(r"PASS:\s+(\d+) native hydrostatic gravity energy assertions",native)
    count=int(m.group(1)) if m else 0
    check("native assertions >= 500",count>=500,count)
    rows=re.findall(r"^GRAV\s+(\d+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
                    r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
                    r"([\d.eE+-]+)\s+([\d.eE+-]+)$",
                    native,re.MULTILINE)
    check("24 independent analytic cases",len(rows)==24,len(rows))
    seen=set()
    reference=[]
    for row in rows:
        n=int(row[0])
        h,g,mass,buoyancy,net,dz,defect=map(float,row[1:])
        key=(n,round(h,10),round(g,10))
        check(f"unique {key}",key not in seen)
        seen.add(key)
        expected_b=1000.0*g*2.56*max(0.0,min(h-3.2,1.6))
        expected_net=expected_b-mass*g
        expected_dz=0.5*expected_net*0.01**2/mass
        check(f"Archimedes {key}",abs(buoyancy-expected_b)<1e-8,
              (buoyancy,expected_b))
        check(f"gravity net {key}",abs(net-expected_net)<1e-8,
              (net,expected_net))
        check(f"rigid translation {key}",abs(dz-expected_dz)<1e-12,
              (dz,expected_dz))
        # Piecewise potential integration: for both endpoints in partial
        # immersion the frozen-force work defect is rho*g*A*dz^2/2.
        d0=h-3.2
        d1=d0-dz
        expected_defect=(0.5*1000.0*g*2.56*dz*dz
                         if 0<d0<1.6 and 0<d1<1.6 else 0.0)
        check(f"exact fixed-level potential {key}",
              abs(defect-expected_defect)<1e-7,
              (defect,expected_defect))
        reference.append({"grid":n,"level_m":h,"gravity_m_s2":g,
                          "mass_kg":mass,"buoyancy_N":buoyancy,
                          "net_N":net,"translation_m":dz,
                          "mechanical_defect_J":defect})
    expected_keys={(n,round(h,10),g) for n in (8,12)
                   for h in (0.0,3.125,4.0,4.35,5.75,float(n))
                   for g in (0.0,9.81)}
    check("grid level gravity coverage",seen==expected_keys)
    check("moving partial surface has nonzero defect",
          any(r["mechanical_defect_J"]>0.001 for r in reference))
    hashes={name:sha(CORE/name) for name in SOURCES}
    report={
        "phase":"P2","milestone":"hydrostatic_body_gravity_potential_partial",
        "partial_accepted":True,"phase_complete":False,
        "ctest_suites":83,"native_assertions":count,
        "checks":checks,"analytic_reference":reference,
        "source_sha256":hashes,"ctest_log_sha256":sha(LOG),
        "limitations":[
            "quiescent fixed-level stratified hydrostatics only",
            "buoyancy fluid counterimpulse is diagnostic and UNAPPLIED",
            "frozen-force integration has nonzero interface potential defect",
            "no free-surface CFD, global FSI energy/momentum, external 3D measurements or UE5.8 native E2E",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_HYDROSTATIC_GRAVITY_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} cases={len(rows)}")
    print(f"REPORT={REPORT}")
if __name__=="__main__":
    main()
