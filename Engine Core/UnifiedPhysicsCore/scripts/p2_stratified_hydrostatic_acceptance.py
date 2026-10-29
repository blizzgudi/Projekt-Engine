#!/usr/bin/env python3
"""P2 PARTIAL: stratified water-fraction hydrostatic load on a rigid box."""
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
LOG=RUNS/"p2_stratified_hydrostatic_full_ctest_20261029.txt"
EXE=CORE/"build"/"upc_p2_stratified_hydrostatic_load_tests.exe"
REPORT=RUNS/"p2_stratified_hydrostatic_partial.json"
SOURCES=[
    "CMakeLists.txt","docs/PHASENPLAN.md",
    "docs/P2_STRATIFIED_HYDROSTATIC_PARTIAL.md",
    "include/upc/p2_stratified_hydrostatic_load3d.hpp",
    "include/upc/p2_moving_box_dynamic_pressure3d.hpp",
    "include/upc/p2_moving_box_pressure_traction3d.hpp",
    "include/upc/rigid_pressure_response3d.hpp",
    "tests/p2_stratified_hydrostatic_load3d_tests.cpp",
    "scripts/p2_stratified_hydrostatic_acceptance.py",
    "scripts/p2_stratified_hydrostatic_full_build.cmd",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main()->None:
    checks=[]
    def check(name,ok,detail=None):
        checks.append({"name":name,"pass":bool(ok),"detail":detail})
        if not ok:
            raise RuntimeError(f"FAIL {name}: {detail}")
    log=LOG.read_text(encoding="utf-8",errors="replace")
    check("full 82/82 CTest","100% tests passed, 0 tests failed out of 82" in log)
    check("new native suite included",
          "physics_p2_stratified_hydrostatic_load_partial" in log)
    result=subprocess.run([str(EXE)],capture_output=True,text=True,
                          timeout=120,check=True)
    m=re.search(r"PASS:\s+(\d+) native stratified hydrostatic body-load assertions",
                result.stdout)
    count=int(m.group(1)) if m else 0
    check("native assertions >= 29000",count>=29000,count)
    rows=re.findall(r"^HYDRO\s+(\d+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
                    r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)$",
                    result.stdout,re.MULTILINE)
    check("24 hydrostatic configurations",len(rows)==24,len(rows))
    seen=set()
    records=[]
    for row in rows:
        n=int(row[0]);height,g,expected,actual,v=map(float,row[1:])
        key=(n,height,g)
        check(f"unique {key}",key not in seen)
        seen.add(key)
        independent=2.56*1000.0*g*max(0.0,min(height-3.2,1.6))
        check(f"independent force {key}",
              abs(actual-independent)<1e-7,(actual,independent))
        check(f"reported force {key}",
              abs(expected-independent)<1e-7,(expected,independent))
        check(f"independent velocity {key}",
              abs(v-independent*0.01/10000.0)<1e-11,v)
        records.append({"grid":n,"height_m":height,"gravity_m_s2":g,
                        "force_N":actual,"velocity_m_s":v})
    check("complete grid/height/gravity coverage",
          seen==set((n,h,g) for n in (8,12)
                   for h in (0.0,4.0,float(n),3.125,4.35,5.75)
                   for g in (0.0,9.81)))
    report={
        "phase":"P2",
        "milestone":"stratified_vof_hydrostatic_rigid_load_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "ctest_suites":82,
        "native_assertions":count,
        "checks":checks,
        "analytic_reference":records,
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "ctest_log_sha256":sha(LOG),
        "unproven":[
            "only stratified quiescent water columns, no general 3D free-surface CFD",
            "water-derived pressure updates rigid body, but fluid counterimpulse is NOT applied",
            "water-volume-averaged pressure is diagnostic, not surface traction",
            "no global fluid-solid energy or momentum closure, independent measured CFD, or UE5.8 Native-E2E",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_STRATIFIED_HYDROSTATIC_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} cases={len(rows)}")
    print(f"REPORT={REPORT}")
if __name__=="__main__":
    main()
