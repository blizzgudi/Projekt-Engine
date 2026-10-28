#!/usr/bin/env python3
"""P2 PARTIAL: gate moving MAC-dual numerical upwind L2 energy stability."""
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
LOG=RUNS/"p2_moving_mac_dual_energy_full_ctest_20261028.txt"
EXE=CORE/"build"/"upc_p2_moving_mac_dual_energy_tests.exe"
REPORT=RUNS/"p2_moving_mac_dual_energy_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "docs/PHASENPLAN.md",
    "docs/P2_MOVING_MAC_DUAL_ENERGY_PARTIAL.md",
    "include/upc/p2_moving_mac_dual_energy3d.hpp",
    "include/upc/p2_moving_mac_shared_predictor3d.hpp",
    "include/upc/p2_moving_mac_dual_transport3d.hpp",
    "include/upc/p2_moving_cut_cell_scalar_transport3d.hpp",
    "tests/p2_moving_mac_dual_energy3d_tests.cpp",
    "scripts/p2_moving_mac_dual_energy_full_build.cmd",
    "scripts/p2_moving_mac_dual_energy_acceptance.py",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main()->None:
    checks=[]
    def verify(name:str,condition:bool,detail:object=None)->None:
        checks.append(dict(name=name,pass_=bool(condition),detail=detail))
        if not condition:
            raise RuntimeError(f"FAIL {name}: {detail}")
    log=LOG.read_text(encoding="utf-8",errors="replace")
    verify("complete MSVC CTest 75/75",
           "100% tests passed, 0 tests failed out of 75" in log)
    verify("new native CTest present",
           "physics_p2_moving_mac_dual_energy_partial" in log)
    result=subprocess.run([str(EXE)],check=True,capture_output=True,
                          text=True,timeout=180)
    match=re.search(r"PASS:\s+(\d+) native moving dual energy assertions",
                    result.stdout)
    count=int(match.group(1)) if match else 0
    verify("140000+ native assertions",count>=140000,count)
    rows=re.findall(
        r"^DUAL_ENERGY\s+(\d+)\s+(\d+)\s+(-?\d+)\s+(\d+)\s+(\d+)\s+"
        r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)$",
        result.stdout,re.MULTILINE)
    verify("48 moving step cases",len(rows)==48,len(rows))
    expected=set(itertools.product((8,12),range(3),(-1,1),(0,1),(0,1)))
    seen=set()
    records={}
    for row in rows:
        n,axis,sign,mode,step=map(int,row[:5])
        before,after,loss=map(float,row[5:])
        key=(n,axis,sign,mode,step)
        verify(f"unique case {key}",key not in seen)
        seen.add(key)
        verify(f"finite and nonincreasing L2 {key}",
               all(map(math.isfinite,(before,after,loss))) and
               before>0 and after>0 and
               after<=before+1e-6 and
               abs(before-after-loss)<1e-7,
               [before,after,loss])
        if mode==0:
            verify(f"uniform state near-zero loss {key}",
                   abs(loss)<1e-6,loss)
        else:
            verify(f"nonuniform state strictly dissipates {key}",
                   loss>0.05,loss)
        records[key]=(before,after,loss)
    verify("all grid, axis, direction, mode and step combinations",
           seen==expected)
    for n,axis,sign,mode in itertools.product((8,12),range(3),(-1,1),(0,1)):
        previous=records[(n,axis,sign,mode,0)][1]
        current=records[(n,axis,sign,mode,1)][0]
        verify(f"sequential energy continuity {(n,axis,sign,mode)}",
               abs(previous-current)<1e-7,[previous,current])
    report={
        "phase":"P2",
        "milestone":"moving_mac_dual_numerical_l2_energy_stability",
        "partial_accepted":True,"phase_complete":False,
        "ctest_suites":75,"native_assertions":count,
        "checks":[{"name":c["name"],"pass":c["pass_"],"detail":c["detail"]}
                  for c in checks],
        "reference":[dict(grid=n,axis=axis,sign=sign,mode=mode,step=step,
                          energy_before_j=vals[0],energy_after_j=vals[1],
                          numerical_upwind_loss_j=vals[2])
                     for (n,axis,sign,mode,step),vals in sorted(records.items())],
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "ctest_log_sha256":sha(LOG),
        "unproven":[
            "single physical momentum tensor across three MAC duals",
            "moving rigid-body reaction impulse and total mechanical energy",
            "stress-correct free surface and independent measured 3D CFD",
            "live CoupledMacWater3D integration and Unreal 5.8 Native-E2E",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_MOVING_MAC_DUAL_ENERGY_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} cases={len(rows)}")
    print(f"REPORT={REPORT}")
if __name__=="__main__":
    main()
