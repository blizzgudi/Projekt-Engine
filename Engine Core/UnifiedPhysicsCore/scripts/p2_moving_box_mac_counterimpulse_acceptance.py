#!/usr/bin/env python3
"""P2 PARTIAL: applied, unprojected moving-box MAC counterimpulse acceptance."""
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
LOG=RUNS/"p2_moving_box_mac_counterimpulse_full_ctest_20261029.txt"
EXE=CORE/"build"/"upc_p2_moving_box_mac_counterimpulse_tests.exe"
REPORT=RUNS/"p2_moving_box_mac_counterimpulse_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "docs/PHASENPLAN.md",
    "docs/P2_MOVING_BOX_MAC_COUNTERIMPULSE_PARTIAL.md",
    "include/upc/p2_moving_box_mac_counterimpulse3d.hpp",
    "include/upc/p2_moving_box_dynamic_pressure3d.hpp",
    "include/upc/p2_moving_box_pressure_traction3d.hpp",
    "include/upc/p2_moving_box_mac_apertures3d.hpp",
    "include/upc/p2_moving_mac_dual_volumes3d.hpp",
    "tests/p2_moving_box_mac_counterimpulse3d_tests.cpp",
    "scripts/p2_moving_box_mac_counterimpulse_full_build.cmd",
    "scripts/p2_moving_box_mac_counterimpulse_acceptance.py",
]
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()
def main()->None:
    checks=[]
    def check(name:str,good:bool,detail=None)->None:
        checks.append({"name":name,"pass":bool(good),"detail":detail})
        if not good:raise RuntimeError(f"FAIL {name}: {detail}")
    log=LOG.read_text(encoding="utf-8",errors="replace")
    check("full 79/79 CTest", "100% tests passed, 0 tests failed out of 79" in log)
    check("native new test registered",
          "physics_p2_moving_box_mac_counterimpulse_partial" in log)
    run=subprocess.run([str(EXE)],capture_output=True,text=True,timeout=180,check=True)
    m=re.search(r"PASS:\s+(\d+) native moving MAC counterimpulse assertions",run.stdout)
    count=int(m.group(1)) if m else 0
    check("native assertions >= 600",count>=600,count)
    rows=re.findall(
        r"^COUNTER\s+(\d+)\s+(\d+)\s+(-?\d+)\s+"
        r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
        r"([\d.eE+-]+)\s+([\d.eE+-]+)$",
        run.stdout,re.MULTILINE)
    check("12 two-step independent cases",len(rows)==12,len(rows))
    expected=set(itertools.product((8,12),range(3),(-1,1)))
    seen=set();references=[]
    for row in rows:
        n,axis,sign=map(int,row[:3])
        body,fluid,velocity,centre=map(float,row[3:])
        key=(n,axis,sign)
        check(f"unique {key}",key not in seen)
        seen.add(key)
        check(f"finite {key}",all(map(math.isfinite,(body,fluid,velocity,centre))))
        check(f"body impulse {key}",abs(body+5.12*sign)<1e-10,body)
        check(f"applied fluid impulse {key}",abs(fluid-5.12*sign)<1e-10,fluid)
        check(f"combined impulse {key}",abs(body+fluid)<1e-10,body+fluid)
        check(f"rigid velocity {key}",abs(velocity+0.512*sign)<1e-10,velocity)
        check(f"rigid centre {key}",
              abs(centre-(4.0-0.0512*sign))<1e-10,centre)
        references.append({"grid":n,"axis":axis,"gradient_sign":sign,
                           "body_impulse_Ns":body,"applied_MAC_impulse_Ns":fluid,
                           "body_velocity_m_s":velocity,"body_centre_m":centre})
    check("complete grid/axis/sign coverage",seen==expected)
    report={
        "phase":"P2","milestone":"moving_box_applied_mac_counterimpulse_partial",
        "partial_accepted":True,"phase_complete":False,
        "ctest_suites":79,"native_assertions":count,
        "checks":checks,"analytic_reference":references,
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "ctest_log_sha256":sha(LOG),
        "unproven":[
            "face momentum is a selected moving-dual-mass proxy, not a physical common momentum tensor",
            "no post-impulse cut-cell pressure projection; nonzero GCL residual explicitly tested",
            "no VOF transport, free-surface stresses or live CoupledMacWater3D integration",
            "no closed fluid+solid mechanical energy budget, no external 3D CFD reference",
            "no UE5.8 Native-E2E for this opt-in component",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_MOVING_MAC_COUNTERIMPULSE_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} cases={len(rows)}")
    print(f"REPORT={REPORT}")
if __name__=="__main__":
    main()
