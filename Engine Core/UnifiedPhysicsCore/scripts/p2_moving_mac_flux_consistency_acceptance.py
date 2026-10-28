#!/usr/bin/env python3
"""P2 partial: independently gate moving primal-to-dual flux correction audit."""
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
LOG=RUNS/"p2_moving_mac_flux_consistency_full_ctest_20261028.txt"
EXE=CORE/"build"/"upc_p2_moving_mac_flux_consistency_tests.exe"
REPORT=RUNS/"p2_moving_mac_flux_consistency_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_moving_mac_flux_consistency3d.hpp",
    "include/upc/p2_moving_mac_shared_predictor3d.hpp",
    "include/upc/p2_moving_mac_dual_transport3d.hpp",
    "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
    "tests/p2_moving_mac_flux_consistency3d_tests.cpp",
    "scripts/p2_moving_mac_flux_consistency_full_build.cmd",
    "scripts/p2_moving_mac_flux_consistency_acceptance.py",
    "docs/P2_MOVING_MAC_FLUX_CONSISTENCY_PARTIAL.md",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main()->None:
    checks=[]
    def verify(name:str,passed:bool,detail:object=None)->None:
        checks.append({"name":name,"pass":bool(passed),"detail":detail})
        if not passed:
            raise RuntimeError(f"FAIL {name}: {detail}")
    log=LOG.read_text(encoding="utf-8",errors="replace")
    verify("complete MSVC CTest 74/74",
           "100% tests passed, 0 tests failed out of 74" in log)
    verify("new native test present",
           "physics_p2_moving_mac_flux_consistency_partial" in log)
    proc=subprocess.run([str(EXE)],check=True,capture_output=True,
                        text=True,timeout=180)
    m=re.search(r"PASS:\s+(\d+) native flux consistency audit assertions",
                proc.stdout)
    count=int(m.group(1)) if m else 0
    verify("120000+ native assertions",count>=120000,count)
    rows=re.findall(
        r"^FLUX_AUDIT\s+(\d+)\s+(\d+)\s+(-?\d+)\s+"
        r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)$",
        proc.stdout,re.MULTILINE)
    verify("12 independently measured moving cases",len(rows)==12,len(rows))
    expected=set(itertools.product((8,12),range(3),(-1,1)))
    seen=set()
    references=[]
    for row in rows:
        n,axis,sign=map(int,row[:3])
        primal,dual,correction,maximum=map(float,row[3:])
        key=(n,axis,sign)
        verify(f"unique case {key}",key not in seen)
        seen.add(key)
        verify(f"finite, GCL and nonzero dual correction {key}",
               all(map(math.isfinite,(primal,dual,correction,maximum))) and
               0<=primal<1e-9 and 0<=dual<1e-9 and
               correction>1e-6 and 0<maximum<=correction,
               [primal,dual,correction,maximum])
        references.append(dict(grid=n,axis=axis,sign=sign,
                               primal_gcl_m3=primal,dual_gcl_m3=dual,
                               correction_l1_m3=correction,
                               correction_max_m3=maximum))
    verify("all axes, directions and grids",seen==expected)
    report={
        "phase":"P2",
        "milestone":"moving_mac_primal_dual_flux_consistency_diagnostic",
        "partial_accepted":True,"phase_complete":False,
        "ctest_suites":74,"native_assertions":count,
        "checks":checks,"reference":references,
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "ctest_log_sha256":sha(LOG),
        "unproven":[
            "single physical momentum tensor across three MAC duals",
            "moving rigid-body reaction impulse and full energy",
            "stress-correct free surface and independent 3D CFD measurement",
            "live CoupledMacWater3D integration and Unreal 5.8 Native-E2E",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_FLUX_CONSISTENCY_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} cases={len(rows)}")
    print(f"REPORT={REPORT}")
if __name__=="__main__":
    main()
