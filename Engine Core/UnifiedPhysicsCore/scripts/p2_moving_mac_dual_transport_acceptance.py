#!/usr/bin/env python3
"""P2 moving staggered dual conservative transport, bounded partial gate."""
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
LOG=RUNS/"p2_moving_mac_dual_transport_full_ctest_20261028.txt"
EXE=CORE/"build"/"upc_p2_moving_mac_dual_transport_tests.exe"
REPORT=RUNS/"p2_moving_mac_dual_transport_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "docs/P2_MOVING_MAC_DUAL_TRANSPORT_PARTIAL.md",
    "include/upc/p2_moving_mac_dual_transport3d.hpp",
    "include/upc/p2_moving_mac_dual_volumes3d.hpp",
    "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
    "include/upc/p2_moving_cut_cell_scalar_transport3d.hpp",
    "tests/p2_moving_mac_dual_transport3d_tests.cpp",
    "scripts/p2_moving_mac_dual_transport_full_build.cmd",
    "scripts/p2_moving_mac_dual_transport_acceptance.py",
]

def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main()->None:
    checks=[]
    def verify(name:str,ok:bool,detail:object=None)->None:
        checks.append({"name":name,"pass":bool(ok),"detail":detail})
        if not ok: raise RuntimeError(f"FAIL {name}: {detail}")
    log=LOG.read_text(encoding="utf-8")
    verify("full CTest 72/72",
           "100% tests passed, 0 tests failed out of 72" in log)
    verify("new dual transport in CTest",
           "physics_p2_moving_mac_dual_conservative_transport_partial" in log)
    proc=subprocess.run([str(EXE)],capture_output=True,text=True,
                        timeout=120,check=True)
    m=re.search(r"PASS:\s+(\d+) native dual conservative transport assertions",
                proc.stdout)
    count=int(m.group(1)) if m else 0
    verify("native checks >= 400000",count>=400000,count)
    rows=re.findall(
        r"^DUALTRANSPORT\s+(\d+)\s+(\d+)\s+(-?\d+)\s+([\d.eE+-]+)\s+"
        r"(\d+)\s+(\d+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
        r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+(\d+)$",
        proc.stdout,re.MULTILINE)
    expected=set(itertools.product((8,12,16),range(3),(-1,1),
                                    (0.25,0.5),(0,1),range(3)))
    verify("216 dual transport cases",len(rows)==216,len(rows))
    seen=set()
    references=[]
    for r in rows:
        n,axis,sign,frac,step,component=(
            int(r[0]),int(r[1]),int(r[2]),float(r[3]),int(r[4]),int(r[5]))
        before,after,gcl,cfl=float(r[6]),float(r[7]),float(r[8]),float(r[9])
        active=int(r[10])
        key=(n,axis,sign,frac,step,component)
        verify(f"unique {key}",key not in seen)
        seen.add(key)
        err=abs(after-before)
        verify(f"conservative dual {key}",
               all(map(math.isfinite,(before,after,gcl,cfl))) and
               err<=1e-8 and gcl<=1e-10 and 0<cfl<=1.0+1e-11 and
               active>0,
               {"integral_error":err,"gcl":gcl,"cfl":cfl,"active":active})
        references.append({"grid":n,"axis":axis,"sign":sign,
                           "fraction":frac,"step":step,"component":component,
                           "integral_before":before,"integral_after":after,
                           "integral_error":err,"max_gcl":gcl,
                           "max_donor_cfl":cfl,"active_faces":active})
    verify("complete Cartesian coverage",seen==expected)
    const_rows=re.findall(
        r"^CONST\s+(\d+)\s+(\d+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)$",
        proc.stdout,re.MULTILINE)
    verify("nine uniform dual references",len(const_rows)==9,len(const_rows))
    for n,c,before,after in const_rows:
        n=int(n);c=int(c);before=float(before);after=float(after)
        # Independent analytic open volume: shifted interior MAC domain
        # n*n*(n-1) minus 1.5^3 rigid-box volume, with dx=1.
        reference=(n*n*(n-1)-1.5**3)*0.4*(c+1)
        verify(f"analytic uniform dual volume {n},{c}",
               abs(before-reference)<=1e-8 and
               abs(after-reference)<=1e-8,
               {"expected":reference,"before":before,"after":after})
    hashes={name:sha(CORE/name) for name in SOURCES}
    report={
        "phase":"P2","milestone":"moving_mac_dual_conservative_transport",
        "partial_accepted":True,"phase_complete":False,
        "ctest_suites":72,"native_assertions":count,
        "checks":checks,"reference":references,
        "source_sha256":hashes,"ctest_log_sha256":sha(LOG),
        "unproven":[
            "single physically consistent nonlinear MAC dual momentum flux",
            "dynamic bidirectional fluid-solid impulse and energy",
            "free-surface stress, variable density, measured 3D CFD",
            "live CoupledMacWater3D and UE5.8 Native E2E integration",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_DUAL_TRANSPORT_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} transport_cases={len(references)} "
          f"analytic_uniform={len(const_rows)}")
    print(f"REPORT={REPORT}")

if __name__=="__main__":
    main()
