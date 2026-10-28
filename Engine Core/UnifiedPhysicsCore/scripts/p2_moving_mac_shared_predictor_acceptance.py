#!/usr/bin/env python3
"""Bounded P2 acceptance: one primal 3D MAC predictor on three moving dual grids."""
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
REPORT=RUNS/"p2_moving_mac_shared_predictor_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "docs/P2_MOVING_MAC_SHARED_PREDICTOR_PARTIAL.md",
    "include/upc/p2_moving_mac_shared_predictor3d.hpp",
    "include/upc/p2_moving_mac_dual_transport3d.hpp",
    "include/upc/p2_moving_mac_dual_volumes3d.hpp",
    "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
    "include/upc/p2_moving_cut_cell_scalar_transport3d.hpp",
    "tests/p2_moving_mac_shared_predictor3d_tests.cpp",
    "scripts/p2_moving_mac_shared_predictor_full_build.cmd",
    "scripts/p2_moving_mac_shared_predictor_acceptance.py",
]

def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main()->None:
    checks=[]
    def verify(name:str,ok:bool,detail:object=None)->None:
        checks.append({"name":name,"pass":bool(ok),"detail":detail})
        if not ok:
            raise RuntimeError(f"FAIL {name}: {detail}")
    log=LOG.read_text(encoding="utf-8")
    verify("full CTest 73/73",
           "100% tests passed, 0 tests failed out of 73" in log)
    verify("shared predictor included in CTest",
           "physics_p2_moving_mac_shared_predictor_partial" in log)
    proc=subprocess.run([str(EXE)],capture_output=True,text=True,
                        timeout=180,check=True)
    m=re.search(r"PASS:\s+(\d+) native shared MAC predictor assertions",
                proc.stdout)
    count=int(m.group(1)) if m else 0
    verify("native checks >= 200000",count>=200000,count)
    m=re.search(r"^NONZERO_DIFFERENCE\s+([\d.eE+-]+)$",
                proc.stdout,re.MULTILINE)
    diff=float(m.group(1)) if m else 0.0
    verify("nontrivial shared primal advection",
           math.isfinite(diff) and diff>1e-6,diff)
    rows=re.findall(
        r"^SHAREDMAC\s+(\d+)\s+(\d+)\s+(-?\d+)\s+(\d+)\s+(\d+)\s+"
        r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
        r"([\d.eE+-]+)\s+(\d+)$",proc.stdout,re.MULTILINE)
    expected=set(itertools.product((8,12,16),range(3),(-1,1),(0,1),range(3)))
    verify("108 3D motion/step/component cases",len(rows)==108,len(rows))
    seen=set()
    references=[]
    for r in rows:
        n,axis,sign,step,component=map(int,r[:5])
        before,after,gcl,cfl=map(float,r[5:9])
        active=int(r[9])
        key=(n,axis,sign,step,component)
        verify(f"unique {key}",key not in seen)
        seen.add(key)
        error=abs(after-before)
        verify(f"conservation GCL CFL {key}",
               all(map(math.isfinite,(before,after,gcl,cfl))) and
               error<=1e-8 and gcl<=1e-10 and
               0<cfl<=1.0+1e-11 and active>0,
               {"error":error,"gcl":gcl,"cfl":cfl,"active":active})
        references.append({"grid":n,"axis":axis,"sign":sign,"step":step,
                           "component":component,"before":before,
                           "after":after,"error":error,"gcl":gcl,
                           "donor_cfl":cfl,"active_faces":active})
    verify("complete 3D case coverage",seen==expected)
    hashes={name:sha(CORE/name) for name in SOURCES}
    report={
        "phase":"P2","milestone":"moving_mac_shared_primal_predictor",
        "partial_accepted":True,"phase_complete":False,
        "ctest_suites":73,"native_assertions":count,
        "checks":checks,"reference":references,
        "source_sha256":hashes,"ctest_log_sha256":sha(LOG),
        "unproven":[
            "single corrected physical MAC dual momentum flux tensor",
            "nonlinear moving-wall Navier-Stokes and bidirectional FSI",
            "free-surface stress, variable density, measured 3D CFD",
            "live CoupledMacWater3D and UE5.8 Native E2E integration",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_SHARED_MAC_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} transport_cases={len(references)} "
          f"nonzero_difference={diff:.12g}")
    print(f"REPORT={REPORT}")

if __name__=="__main__":
    main()
