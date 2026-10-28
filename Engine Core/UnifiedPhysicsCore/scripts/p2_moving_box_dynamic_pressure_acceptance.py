#!/usr/bin/env python3
"""P2 PARTIAL: bounded dynamic frozen-pressure box translation acceptance."""
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
LOG=RUNS/"p2_moving_box_dynamic_pressure_full_ctest_20261029.txt"
EXE=CORE/"build"/"upc_p2_moving_box_dynamic_pressure_tests.exe"
REPORT=RUNS/"p2_moving_box_dynamic_pressure_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "docs/PHASENPLAN.md",
    "docs/P2_MOVING_BOX_DYNAMIC_PRESSURE_PARTIAL.md",
    "include/upc/p2_moving_box_dynamic_pressure3d.hpp",
    "include/upc/p2_moving_box_pressure_traction3d.hpp",
    "include/upc/p2_moving_box_open_volume3d.hpp",
    "include/upc/rigid_pressure_response3d.hpp",
    "tests/p2_moving_box_dynamic_pressure3d_tests.cpp",
    "scripts/p2_moving_box_dynamic_pressure_full_build.cmd",
    "scripts/p2_moving_box_dynamic_pressure_acceptance.py",
]
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()
def main()->None:
    checks=[]
    def verify(name:str,passed:bool,detail=None)->None:
        checks.append({"name":name,"pass":bool(passed),"detail":detail})
        if not passed:
            raise RuntimeError(f"FAIL {name}: {detail}")
    log=LOG.read_text(encoding="utf-8",errors="replace")
    verify("full 78/78 CTest",
           "100% tests passed, 0 tests failed out of 78" in log)
    verify("new native CTest present",
           "physics_p2_moving_box_dynamic_pressure_partial" in log)
    proc=subprocess.run([str(EXE)],capture_output=True,text=True,
                        check=True,timeout=180)
    match=re.search(
        r"PASS:\s+(\d+) native dynamic pressure box assertions",
        proc.stdout)
    count=int(match.group(1)) if match else 0
    verify("native assertions >= 300",count>=300,count)
    rows=re.findall(
        r"^DYNAMIC\s+(\d+)\s+(\d+)\s+(-?\d+)\s+"
        r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
        r"([\d.eE+-]+)$",proc.stdout,re.MULTILINE)
    verify("12 analytic 3D two-step cases",len(rows)==12,len(rows))
    expected=set(itertools.product((8,12),range(3),(-1,1)))
    seen=set()
    cases=[]
    for row in rows:
        n,axis,sign=map(int,row[:3])
        v,centre,energy,counter=map(float,row[3:])
        key=(n,axis,sign)
        verify(f"unique {key}",key not in seen)
        seen.add(key)
        verify(f"finite {key}",all(map(math.isfinite,(v,centre,energy,counter))))
        verify(f"independent velocity {key}",
               abs(v+0.512*sign)<1e-10,v)
        verify(f"independent displacement {key}",
               abs(centre-(4.0-0.0512*sign))<1e-10,centre)
        verify(f"independent kinetic work {key}",
               abs(energy-1.31072)<1e-10,energy)
        verify(f"diagnostic opposite impulse {key}",
               abs(counter-5.12*sign)<1e-10,counter)
        cases.append({"grid":n,"axis":axis,"gradient_sign":sign,
                      "velocity_m_s":v,"centre_m":centre,
                      "frozen_pressure_work_J":energy,
                      "unapplied_fluid_counter_impulse_Ns":counter})
    verify("complete axis/gradient/grid coverage",seen==expected)
    report={
        "phase":"P2","milestone":"frozen_pressure_dynamic_box_translation_partial",
        "partial_accepted":True,"phase_complete":False,
        "ctest_suites":78,"native_assertions":count,
        "checks":checks,"analytic_reference":cases,
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "ctest_log_sha256":sha(LOG),
        "unproven":[
            "opposite impulse is NOT applied to fluid MAC faces",
            "no bidirectional dynamic FSI or common momentum tensor",
            "no full fluid/solid energy conservation or free-surface stress",
            "no independent measured 3D CFD or UE5.8 native E2E",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_DYNAMIC_PRESSURE_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} cases={len(rows)}")
    print(f"REPORT={REPORT}")
if __name__=="__main__":
    main()
