"""P2 axis-aligned planar cut-cell geometry, bounded quantitative acceptance."""
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
EXE=CORE/"build"/"upc_p2_axis_plic_geometry_tests.exe"
CTEST=RUNS/"p2_axis_plic_ctest_20261031.txt"
BUILD=RUNS/"p2_axis_plic_build_20261031.txt"
OUT=RUNS/"p2_axis_plic_partial.json"
SOURCES=[
    "include/upc/p2_axis_plic_geometry3d.hpp",
    "tests/p2_axis_plic_geometry3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_axis_plic_full_build.cmd",
    "scripts/p2_axis_plic_acceptance.py",
    "docs/P2_AXIS_PLIC_GEOMETRY_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main()->None:
    native=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                          text=True,timeout=90)
    checks=[]
    def add(name:str,value:object,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",native.returncode,native.returncode==0)
    m=re.search(r"PASS: axis PLIC geometric assertions=(\d+) phase_complete=false",
                native.stdout)
    assertions=int(m.group(1)) if m else 0
    add("native_assertions",assertions,assertions>=80)
    log=CTEST.read_text(encoding="utf-8",errors="replace")
    add("full_ctest_98","100% tests passed, 0 tests failed out of 98" in log,
        "100% tests passed, 0 tests failed out of 98" in log)
    add("native_ctest_entry","physics_p2_axis_plic_geometry_partial" in log,
        "physics_p2_axis_plic_geometry_partial" in log)
    add("build_log_nonempty",BUILD.stat().st_size,BUILD.stat().st_size>0)
    lines=[x for x in native.stdout.splitlines()
           if x.startswith(("case,","half,","cube,","partial,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("five_geometry_cases",len(rows),len(rows)==5)
    reference=[]
    for row in rows:
        mode=row["case"]
        n=int(row["grid"])
        mass=float(row["mass"])
        cx=float(row["com_x"])
        ixx=float(row["Ixx"])
        ixy=float(row["Ixy"])
        label=f"{mode}_{n}"
        add(label+"_finite",(mass,cx,ixx,ixy),
            all(math.isfinite(v) for v in (mass,cx,ixx,ixy)))
        if mode=="half":
            add(label+"_mass",mass,abs(mass-4)<1e-12)
            add(label+"_centroid",cx,abs(cx-.05)<1e-13)
            add(label+"_Ixx",ixx,abs(ixx-4*.08/12)<1e-13)
            add(label+"_Ixy",ixy,abs(ixy)<1e-13)
        elif mode=="cube":
            add(label+"_grid",n,n in (8,12))
            add(label+"_mass",mass,abs(mass-64)<2e-11)
            add(label+"_centroid",cx,abs(cx-.2)<2e-13)
            add(label+"_Ixx",ixx,abs(ixx-64*.16/6)<2e-11)
            add(label+"_Ixy",ixy,abs(ixy)<2e-11)
        elif mode=="partial":
            add(label+"_grid",n,n in (8,12))
            add(label+"_mass_positive",mass,mass>0)
            add(label+"_Ixx_positive",ixx,ixx>0)
        else:
            add(label+"_known_case",mode,False)
        reference.append({"mode":mode,"grid":n,"mass_kg":mass,
                          "com_x_m":cx,"Ixx_kg_m2":ixx,"Ixy_kg_m2":ixy})
    add("exact_case_set",[(r["mode"],r["grid"]) for r in reference],
        sorted((r["mode"],r["grid"]) for r in reference)==
        [("cube",8),("cube",12),("half",1),("partial",8),("partial",12)])
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("source_hash_count",len(hashes),len(hashes)==7)
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2","milestone":"axis_aligned_planar_vof_geometry_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":assertions,
        "reference":reference,"source_sha256":hashes,
        "native_binary_sha256":sha(EXE),
        "ctest_log_sha256":sha(CTEST),"build_log_sha256":sha(BUILD),
        "limitations":"Exact axis-aligned planar geometry only; no arbitrary PLIC normal, live MAC/VOF transport, moving solid FSI, independent measured 3D CFD, or UE5.8 Native E2E."
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"P2_AXIS_PLIC_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]:print("FAIL",c)
        raise SystemExit(1)
if __name__=="__main__":
    main()
