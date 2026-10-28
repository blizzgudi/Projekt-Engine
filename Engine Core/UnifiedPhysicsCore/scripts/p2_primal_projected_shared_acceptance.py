#!/usr/bin/env python3
"""Bounded P2 primal projected shared-MAC acceptance; never closes P2."""
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
LOG=RUNS/"p2_moving_mac_shared_predictor_full_ctest_20261028.txt"
EXE=CORE/"build"/"upc_p2_moving_mac_shared_predictor_tests.exe"
REPORT=RUNS/"p2_primal_projected_shared_partial.json"
SOURCES=[
    "include/upc/p2_moving_mac_shared_predictor3d.hpp",
    "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
    "include/upc/p2_moving_box_swept_wall_flux3d.hpp",
    "include/upc/p2_moving_mac_dual_transport3d.hpp",
    "tests/p2_moving_mac_shared_predictor3d_tests.cpp",
    "scripts/p2_primal_projected_shared_acceptance.py",
    "docs/P2_PRIMAL_PROJECTED_SHARED_PARTIAL.md",
]
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()
def main()->None:
    checks=[]
    def verify(name:str,condition:bool,detail:object=None)->None:
        checks.append({"name":name,"pass":bool(condition),"detail":detail})
        if not condition:
            raise RuntimeError(f"FAIL {name}: {detail}")
    log=LOG.read_text(encoding="utf-8")
    verify("complete 73/73 native CTest",
           "100% tests passed, 0 tests failed out of 73" in log)
    verify("new shared-MAC test included",
           "physics_p2_moving_mac_shared_predictor_partial" in log)
    proc=subprocess.run([str(EXE)],capture_output=True,text=True,
                        timeout=180,check=True)
    m=re.search(r"PASS:\s+(\d+) native shared MAC predictor assertions",
                proc.stdout)
    count=int(m.group(1)) if m else 0
    verify("native assertions >= 300000",count>=300000,count)
    rows=re.findall(
        r"^PRIMALPROJ\s+(\d+)\s+(\d+)\s+(-?\d+)\s+"
        r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)$",
        proc.stdout,re.MULTILINE)
    expected=set(itertools.product((8,12),range(3),(-1,1)))
    seen=set()
    reference=[]
    verify("12 moving primal projection cases",len(rows)==12,len(rows))
    for row in rows:
        n,axis,sign=map(int,row[:3])
        residual,velocity_l1,cfl=map(float,row[3:])
        key=(n,axis,sign)
        verify(f"unique {key}",key not in seen)
        seen.add(key)
        verify(f"independent GCL/active transport/CFL {key}",
               all(map(math.isfinite,(residual,velocity_l1,cfl))) and
               0<=residual<1e-9 and velocity_l1>1e-6 and
               0<cfl<=1.0+1e-11,
               {"residual_m3":residual,"velocity_l1":velocity_l1,
                "donor_cfl":cfl})
        reference.append({"grid":n,"axis":axis,"sign":sign,
                          "residual_m3":residual,
                          "velocity_l1_m_s":velocity_l1,"donor_cfl":cfl})
    verify("all 12 cases covered",seen==expected)
    gradients=re.findall(
        r"^PRIMAL_GRADIENT\s+(\d+)\s+([\d.eE+-]+)$",
        proc.stdout,re.MULTILINE)
    verify("2 independent manufactured gradients",len(gradients)==2)
    verify("both grids gradient coverage",
           {int(n) for n,_ in gradients}=={8,12})
    for n,err in gradients:
        value=float(err)
        verify(f"gradient annihilation n={n}",
               math.isfinite(value) and 0<=value<1e-9,value)
    report={
        "phase":"P2","milestone":"primal_projected_shared_mac",
        "partial_accepted":True,"phase_complete":False,
        "ctest_suites":73,"native_assertions":count,
        "checks":checks,"reference":reference,
        "manufactured_gradient":gradients,
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "ctest_log_sha256":sha(LOG),
        "unproven":[
            "one corrected physical momentum tensor on all dual grids",
            "bidirectional moving-solid fluid momentum and energy",
            "free-surface stresses and measured independent 3D CFD",
            "live CoupledMacWater3D and Unreal 5.8 Native E2E",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_PRIMAL_PROJECTED_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} cases={len(rows)}")
    print(f"REPORT={REPORT}")
if __name__=="__main__":
    main()
