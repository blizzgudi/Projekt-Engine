#!/usr/bin/env python3
"""P2 PARTIAL: analytic box pressure traction and swept-work evidence."""
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
LOG=RUNS/"p2_moving_box_pressure_traction_full_ctest_20261029.txt"
EXE=CORE/"build"/"upc_p2_moving_box_pressure_traction_tests.exe"
REPORT=RUNS/"p2_moving_box_pressure_traction_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "docs/PHASENPLAN.md",
    "docs/P2_MOVING_BOX_PRESSURE_TRACTION_PARTIAL.md",
    "include/upc/p2_moving_box_pressure_traction3d.hpp",
    "include/upc/p2_moving_box_open_volume3d.hpp",
    "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
    "tests/p2_moving_box_pressure_traction3d_tests.cpp",
    "scripts/p2_moving_box_pressure_traction_full_build.cmd",
    "scripts/p2_moving_box_pressure_traction_acceptance.py",
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
    verify("full 77/77 CTest",
           "100% tests passed, 0 tests failed out of 77" in log)
    verify("new native CTest present",
           "physics_p2_moving_box_pressure_traction_partial" in log)
    run=subprocess.run([str(EXE)],capture_output=True,text=True,
                       check=True,timeout=180)
    match=re.search(
        r"PASS:\s+(\d+) native moving box pressure traction assertions",
        run.stdout)
    count=int(match.group(1)) if match else 0
    verify("288+ native assertions",count>=288,count)
    rows=re.findall(
        r"^TRACTION\s+(\d+)\s+(\d+)\s+(-?\d+)\s+"
        r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)$",
        run.stdout,re.MULTILINE)
    verify("12 moving 3D cases",len(rows)==12,len(rows))
    expected=set(itertools.product((8,12),range(3),(-1,1)))
    seen=set()
    cases=[]
    for row in rows:
        n,axis,sign=map(int,row[:3])
        force,work,difference=map(float,row[3:])
        key=(n,axis,sign)
        verify(f"unique {key}",key not in seen)
        seen.add(key)
        verify(f"finite {key}",all(map(math.isfinite,(force,work,difference))))
        verify(f"independent pressure force {key}",abs(force-40.0)<1e-9,force)
        verify(f"independent swept pressure work {key}",
               abs(work-8.0*sign)<1e-9,work)
        verify(f"midpoint/swept agreement for aligned pressure step {key}",
               abs(difference)<1e-9,difference)
        cases.append({"grid":n,"axis":axis,"direction":sign,
                      "solid_force_n":force,"solid_geometric_work_j":work,
                      "midpoint_work_difference_j":difference})
    verify("complete movement coverage",seen==expected)
    projected=re.search(
        r"^PROJECTED\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)$",
        run.stdout,re.MULTILINE)
    verify("real projected pressure case",projected is not None)
    values=[float(x) for x in projected.groups()] if projected else []
    verify("finite nontrivial projected force/work",
           len(values)==3 and all(map(math.isfinite,values)) and
           abs(values[0])>1.0 and abs(values[1])>1.0,values)
    report={
        "phase":"P2",
        "milestone":"prescribed_box_pressure_traction_and_counter_impulse_diagnostic",
        "partial_accepted":True,"phase_complete":False,
        "ctest_suites":77,"native_assertions":count,
        "checks":checks,"analytic_reference":cases,
        "projected_pressure_reference":values,
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "ctest_log_sha256":sha(LOG),
        "unproven":[
            "counter-impulse is diagnostic and not applied to a dynamic rigid body",
            "common conservative MAC momentum tensor and global physical energy",
            "stress-correct free surface and independent measured 3D CFD",
            "coupled live MAC/VOF and Unreal 5.8 native E2E",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_BOX_PRESSURE_TRACTION_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} cases={len(rows)}")
    print(f"REPORT={REPORT}")
if __name__=="__main__":
    main()
