#!/usr/bin/env python3
"""P2 PARTIAL: native three-dual moving pressure-work identity evidence."""
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
LOG=RUNS/"p2_moving_mac_dual_pressure_work_full_ctest_20261028.txt"
EXE=CORE/"build"/"upc_p2_moving_mac_dual_pressure_work_tests.exe"
REPORT=RUNS/"p2_moving_mac_dual_pressure_work_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "docs/PHASENPLAN.md",
    "docs/P2_MOVING_MAC_DUAL_PRESSURE_WORK_PARTIAL.md",
    "include/upc/p2_moving_mac_dual_pressure_work3d.hpp",
    "include/upc/p2_moving_mac_shared_predictor3d.hpp",
    "include/upc/p2_moving_cut_cell_energy3d.hpp",
    "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
    "tests/p2_moving_mac_dual_pressure_work3d_tests.cpp",
    "scripts/p2_moving_mac_dual_pressure_work_full_build.cmd",
    "scripts/p2_moving_mac_dual_pressure_work_acceptance.py",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main()->None:
    checks=[]
    def verify(name:str,ok:bool,detail=None)->None:
        checks.append({"name":name,"pass":bool(ok),"detail":detail})
        if not ok:
            raise RuntimeError(f"FAIL {name}: {detail}")
    log=LOG.read_text(encoding="utf-8",errors="replace")
    verify("full 76/76 CTest",
           "100% tests passed, 0 tests failed out of 76" in log)
    verify("new test present",
           "physics_p2_moving_mac_dual_pressure_work_partial" in log)
    run=subprocess.run([str(EXE)],capture_output=True,text=True,
                       check=True,timeout=180)
    count_match=re.search(
        r"PASS:\s+(\d+) native moving dual pressure work assertions",
        run.stdout)
    count=int(count_match.group(1)) if count_match else 0
    verify("490+ native assertions",count>=490,count)
    rows=re.findall(
        r"^DUAL_PRESSURE\s+(\d+)\s+(\d+)\s+(-?\d+)\s+(\d+)\s+"
        r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)$",
        run.stdout,re.MULTILINE)
    verify("14 stationary/moving cases",len(rows)==14,len(rows))
    expected={(n,0,1,0) for n in (8,12)}
    expected.update(itertools.product((8,12),range(3),(-1,1),(1,)))
    seen=set()
    references=[]
    for row in rows:
        n,axis,sign,moving=map(int,row[:4])
        change,wall,correction,residual=map(float,row[4:])
        key=(n,axis,sign,moving)
        verify(f"unique {key}",key not in seen)
        seen.add(key)
        verify(f"finite pressure terms {key}",
               all(map(math.isfinite,(change,wall,correction,residual))))
        verify(f"pressure work identity {key}",
               abs(change-(wall-correction))<1e-7,
               [change,wall,correction])
        verify(f"nonnegative correction {key}",correction>=-1e-8,correction)
        verify(f"max residual {key}",residual<1e-7,residual)
        if moving:
            verify(f"moving pressure correction {key}",
                   correction>1e-5,correction)
        else:
            verify(f"static zero pressure work {key}",
                   abs(change)<1e-8 and abs(wall)<1e-8,
                   [change,wall])
        references.append({"grid":n,"axis":axis,"direction":sign,
                           "moving":bool(moving),"projection_change_j":change,
                           "geometric_pressure_work_j":wall,
                           "pressure_correction_energy_j":correction,
                           "identity_residual_j":residual})
    verify("complete 3D movement coverage",seen==expected)
    # Regression anchors only, NOT independent measured physical references.
    anchors={(12,0,-1,1):273.47342429416176,
             (12,1,-1,1):177.53167982148818}
    for key,reference in anchors.items():
        actual=next(x["projection_change_j"] for x in references
                    if (x["grid"],x["axis"],x["direction"],int(x["moving"]))==key)
        verify(f"numerical regression {key}",abs(actual-reference)<1e-5,
               [actual,reference])
    report={
        "phase":"P2","milestone":"moving_mac_three_dual_pressure_work_identity",
        "partial_accepted":True,"phase_complete":False,
        "ctest_suites":76,"native_assertions":count,"checks":checks,
        "reference":references,
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "ctest_log_sha256":sha(LOG),
        "unproven":[
            "common physical momentum tensor across all three MAC duals",
            "two-way moving rigid-body momentum and total energy",
            "stress-correct free surface and measured 3D CFD",
            "live MAC/VOF integration and Unreal 5.8 native E2E",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_DUAL_PRESSURE_WORK_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} cases={len(rows)}")
    print(f"REPORT={REPORT}")
if __name__=="__main__":
    main()
