"""P2 bounded: finite-volume uniform-cell inertia, not moving-solid FSI."""
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
EXE=CORE/"build"/"upc_p2_vof_uniform_cell_inertia_tests.exe"
LOG=RUNS/"p2_vof_uniform_cell_ctest_20261031.txt"
BUILD=RUNS/"p2_vof_uniform_cell_build_20261031.txt"
OUT=RUNS/"p2_vof_uniform_cell_partial.json"
SOURCES=[
    "include/upc/mac3d.hpp",
    "include/upc/p2_vof_angular_inventory3d.hpp",
    "include/upc/p2_vof_uniform_cell_inertia3d.hpp",
    "tests/p2_vof_uniform_cell_inertia3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_vof_uniform_cell_full_build.cmd",
    "scripts/p2_vof_uniform_cell_acceptance.py",
    "docs/P2_VOF_UNIFORM_CELL_INERTIA_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()
def main()->None:
    native=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,text=True,timeout=90)
    checks=[]
    def add(name,value,ok):
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",native.returncode,native.returncode==0)
    match=re.search(r"PASS: uniform-cell VOF inertia assertions=(\d+) phase_complete=false",native.stdout)
    assertions=int(match.group(1)) if match else 0
    add("native_assertions",assertions,assertions>=150)
    log=LOG.read_text(encoding="utf-8",errors="replace")
    add("all_96_ctest","100% tests passed, 0 tests failed out of 96" in log,
        "100% tests passed, 0 tests failed out of 96" in log)
    add("build_log",BUILD.stat().st_size,BUILD.stat().st_size>0)
    lines=[s for s in native.stdout.splitlines()
           if s.startswith(("case,","asymmetric,","cube,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("five_cases",len(rows),len(rows)==5)
    refs=[]
    for row in rows:
        mode=row["case"];n=int(row["grid"])
        nums={key:float(row[key]) for key in ("mass","Ixx","Ixy","point_Ixx","reference")}
        tag=f"{mode}_{n}"
        add(tag+"_finite",nums,all(math.isfinite(v) for v in nums.values()))
        if mode=="asymmetric":
            add(tag+"_grid",n,n in (8,12))
            add(tag+"_mass",nums["mass"],abs(nums["mass"]-1)<1e-12)
            add(tag+"_inertia",nums["Ixx"],abs(nums["Ixx"]-(.03375+.01/6))<1e-12)
            add(tag+"_offdiag",nums["Ixy"],abs(nums["Ixy"]+.016875)<1e-12)
            add(tag+"_point",nums["point_Ixx"],abs(nums["point_Ixx"]-.03375)<1e-12)
        elif mode=="cube":
            add(tag+"_grid",n,n in (2,4,8))
            add(tag+"_mass",nums["mass"],abs(nums["mass"]-8)<1e-11)
            add(tag+"_inertia",nums["Ixx"],abs(nums["Ixx"]-8*.2*.2/6)<1e-12)
            add(tag+"_reference",nums["reference"],abs(nums["Ixx"]-nums["reference"])<1e-12)
            add(tag+"_offdiag",nums["Ixy"],abs(nums["Ixy"])<1e-12)
            add(tag+"_point_lower",nums["point_Ixx"],nums["point_Ixx"]<nums["Ixx"])
        else:
            add(tag+"_mode",mode,False)
        refs.append({"case":mode,"grid":n,**nums})
    add("exact_grid_set",[(r["case"],r["grid"]) for r in refs],
        sorted((r["case"],r["grid"]) for r in refs)==
        [("asymmetric",8),("asymmetric",12),("cube",2),("cube",4),("cube",8)])
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("source_hash_count",len(hashes),len(hashes)==9)
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2","milestone":"uniform_cell_finite_volume_inertia_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":assertions,"reference":refs,
        "source_sha256":hashes,
        "native_binary_sha256":sha(EXE),"ctest_log_sha256":sha(LOG),
        "build_log_sha256":sha(BUILD),
        "limitations":"Uniform fractional occupancy within each voxel, not resolved subcell free surface. Read-only inertia diagnostic; no rotational fluid-solid impulse or energy coupling, external 3D CFD data or new UE5.8 Native-E2E."
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(f"P2_UNIFORM_CELL_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]:print("FAIL",c)
        raise SystemExit(1)
if __name__=="__main__":
    main()
