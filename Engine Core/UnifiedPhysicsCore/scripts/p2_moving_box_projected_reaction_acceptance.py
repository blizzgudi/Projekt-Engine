#!/usr/bin/env python3
"""P2 PARTIAL: native post-counterimpulse cut-cell projection gate."""
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
LOG=RUNS/"p2_moving_box_projected_reaction_full_ctest_20261029.txt"
EXE=CORE/"build"/"upc_p2_moving_box_projected_reaction_tests.exe"
REPORT=RUNS/"p2_moving_box_projected_reaction_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "docs/PHASENPLAN.md",
    "docs/P2_MOVING_BOX_PROJECTED_REACTION_PARTIAL.md",
    "include/upc/p2_moving_box_projected_reaction3d.hpp",
    "include/upc/p2_moving_box_mac_counterimpulse3d.hpp",
    "include/upc/p2_moving_box_dynamic_pressure3d.hpp",
    "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
    "include/upc/p2_moving_mac_dual_volumes3d.hpp",
    "tests/p2_moving_box_projected_reaction3d_tests.cpp",
    "scripts/p2_moving_box_projected_reaction_full_build.cmd",
    "scripts/p2_moving_box_projected_reaction_acceptance.py",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main()->None:
    checks=[]
    def check(name:str,passed:bool,detail=None)->None:
        checks.append({"name":name,"pass":bool(passed),"detail":detail})
        if not passed:
            raise RuntimeError(f"FAIL {name}: {detail}")
    log=LOG.read_text(encoding="utf-8",errors="replace")
    check("complete 80/80 CTest",
          "100% tests passed, 0 tests failed out of 80" in log)
    check("new native test in CTest",
          "physics_p2_moving_box_projected_reaction_partial" in log)
    run=subprocess.run([str(EXE)],capture_output=True,text=True,
                       timeout=180,check=True)
    m=re.search(r"PASS:\s+(\d+) native projected reaction assertions",
                run.stdout)
    native=int(m.group(1)) if m else 0
    check("native assertions >= 2000",native>=2000,native)
    pattern=(r"^PROJECTED\s+(\d+)\s+(\d+)\s+(-?\d+)\s+"
             r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
             r"([\d.eE+-]+)\s+([\d.eE+-]+)$")
    rows=re.findall(pattern,run.stdout,re.MULTILINE)
    check("12 analytic configurations",len(rows)==12,len(rows))
    seen=set()
    references=[]
    for row in rows:
        n,axis,sign=map(int,row[:3])
        pre,post,correction,residual=map(float,row[3:])
        key=(n,axis,sign)
        check(f"unique {key}",key not in seen)
        seen.add(key)
        check(f"finite {key}",all(map(math.isfinite,
              (pre,post,correction,residual))))
        check(f"pre-projection violation {key}",
              0.008<pre<0.009,pre)
        check(f"post-projection continuity {key}",post<1e-10,post)
        check(f"reduction >= 4 orders {key}",
              post<pre*1e-4,post/pre)
        check(f"correction exposed {key}",
              abs(correction)>1e-4,correction)
        check(f"correction sign {key}",
              correction*sign>0,correction)
        check(f"impulse residual ledger {key}",
              abs(residual-correction)<1e-9,residual-correction)
        references.append({"grid":n,"axis":axis,"gradient_sign":sign,
            "unprojected_gcl_residual_m3":pre,
            "projected_gcl_residual_m3":post,
            "projection_momentum_change_proxy_Ns":correction,
            "combined_impulse_residual_proxy_Ns":residual})
    check("full grid/axis/sign coverage",
          seen==set(itertools.product((8,12),range(3),(-1,1))))
    report={
        "phase":"P2",
        "milestone":"moving_box_projected_mac_reaction_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "ctest_suites":80,
        "native_assertions":native,
        "checks":checks,
        "analytic_reference":references,
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "ctest_log_sha256":sha(LOG),
        "unproven":[
            "projected MAC continuity does not establish physical FSI impulse conservation",
            "pressure projection changes the dual-mass momentum proxy substantially",
            "no coupled common momentum tensor, free-surface VOF or global mechanical energy closure",
            "no independent measured 3D CFD reference or Unreal 5.8 Native-E2E for this path",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_PROJECTED_REACTION_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={native} cases={len(rows)}")
    print(f"REPORT={REPORT}")
if __name__=="__main__":
    main()
