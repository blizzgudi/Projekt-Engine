"""P2 bounded: VOF cell-centre angular momentum and inertia audit."""
from __future__ import annotations
import csv
import hashlib
import io
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
EXE=CORE/"build"/"upc_p2_vof_angular_inventory_tests.exe"
LOG=RUNS/"p2_vof_angular_ctest_20261031.txt"
BUILD=RUNS/"p2_vof_angular_build_20261031.txt"
OUT=RUNS/"p2_vof_angular_partial.json"
SOURCES=[
    "include/upc/mac3d.hpp",
    "include/upc/p2_vof_angular_inventory3d.hpp",
    "tests/p2_vof_angular_inventory3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_vof_angular_full_build.cmd",
    "scripts/p2_vof_angular_acceptance.py",
    "docs/P2_VOF_ANGULAR_INVENTORY_PARTIAL.md",
    "docs/PHASENPLAN.md",
]

def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main()->None:
    native=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,text=True,
                          timeout=90,check=False)
    log=LOG.read_text(encoding="utf-8",errors="replace")
    checks=[]
    def add(name:str,value:object,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",native.returncode,native.returncode==0)
    m=re.search(r"PASS: vof angular inventory assertions=(\d+) phase_complete=false",
                native.stdout)
    assertions=int(m.group(1)) if m else 0
    add("native_assertions",assertions,assertions>=126)
    add("msvc_95_ctest",log.count("100% tests passed, 0 tests failed out of 95"),
        "100% tests passed, 0 tests failed out of 95" in log)
    add("build_log_present",BUILD.stat().st_size,BUILD.stat().st_size>0)
    lines=[s for s in native.stdout.splitlines()
           if s.startswith(("mode,","analytic,","projected,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("four_grid_rows",len(rows),len(rows)==4)
    refs=[]
    expected_analytic={"mass":1,"Lx":-.73125,"Ly":.50625,"Lz":.225,
                       "Ixx":.03375,"Ixy":-.016875,
                       "dLx":-.184375,"dLy":.021875,"dLz":.1625}
    expected_projected={"mass":1.5,"Lx":-.73125,"Ly":.65625,"Lz":.075,
                        "Ixx":.0675,"Ixy":-.02625,
                        "dLx":0,"dLy":.00625,"dLz":-.00625}
    for row in rows:
        mode=row["mode"]
        n=int(row["n"])
        tag=f"{mode}_{n}"
        add(tag+"_grid",n,n in (8,12))
        nums={key:float(row[key]) for key in
              ("mass","Lx","Ly","Lz","Ixx","Ixy","dLx","dLy","dLz")}
        add(tag+"_finite",nums,all(math.isfinite(v) for v in nums.values()))
        expected={"analytic":expected_analytic,
                  "projected":expected_projected}.get(mode,{})
        add(tag+"_mode",mode,bool(expected))
        for key,value in expected.items():
            add(tag+"_reference_"+key,nums[key],abs(nums[key]-value)<1e-10)
        if mode=="projected":
            add(tag+"_nontrivial_projection",nums["dLy"],
                abs(nums["dLy"])>1e-10)
        refs.append({"mode":mode,"grid":n,**nums})
    add("exact_mode_grid_set",[(r["mode"],r["grid"]) for r in refs],
        sorted((r["mode"],r["grid"]) for r in refs)==
        [("analytic",8),("analytic",12),("projected",8),("projected",12)])
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("eight_source_hashes",len(hashes),len(hashes)==8)
    accepted=bool(checks) and all(c["pass"] for c in checks)
    report={
        "phase":"P2",
        "milestone":"vof_angular_momentum_point_mass_inertia_partial",
        "partial_accepted":accepted,
        "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,
        "native_assertions":assertions,
        "reference":refs,
        "source_sha256":hashes,
        "native_binary_sha256":sha(EXE),
        "ctest_log_sha256":sha(LOG),
        "build_log_sha256":sha(BUILD),
        "limitations":"Cell-centre point-mass angular/inertia inventory only; no moving-body FSI torque, no global momentum/energy conservation, no external CFD measurement, no UE5.8 Native-E2E."
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"P2_VOF_ANGULAR_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]:print("FAIL",c)
        raise SystemExit(1)

if __name__=="__main__":
    main()
