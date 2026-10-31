"""Bounded quantitative P2 acceptance: prescribed oblique planar cut cells."""
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
EXE=CORE/"build"/"upc_p2_oblique_plic_geometry_tests.exe"
BUILD=RUNS/"p2_oblique_plic_build_20261031.txt"
CTEST=RUNS/"p2_oblique_plic_ctest_20261031.txt"
OUT=RUNS/"p2_oblique_plic_partial.json"
SOURCES=[
    "include/upc/p2_axis_plic_geometry3d.hpp",
    "include/upc/p2_oblique_plic_geometry3d.hpp",
    "tests/p2_oblique_plic_geometry3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_oblique_plic_full_build.cmd",
    "scripts/p2_oblique_plic_acceptance.py",
    "docs/P2_OBLIQUE_PLIC_GEOMETRY_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()
def main()->None:
    native=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                          text=True,timeout=120)
    checks=[]
    def add(name:str,value:object,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",native.returncode,native.returncode==0)
    match=re.search(r"PASS: oblique PLIC assertions=(\d+) phase_complete=false",
                    native.stdout)
    count=int(match.group(1)) if match else 0
    add("native_assertions",count,count>=175)
    log=CTEST.read_text(encoding="utf-8",errors="replace")
    add("all_ctest_99","100% tests passed, 0 tests failed out of 99" in log,
        "100% tests passed, 0 tests failed out of 99" in log)
    add("new_ctest_entry","physics_p2_oblique_plic_geometry_partial" in log,
        "physics_p2_oblique_plic_geometry_partial" in log)
    add("build_log",BUILD.stat().st_size,BUILD.stat().st_size>0)
    lines=[line for line in native.stdout.splitlines()
           if line.startswith(("case,","tetra,","prism,","multi,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("four_native_scenarios",len(rows),len(rows)==4)
    add("exact_case_set",[(r["case"],r["grid"]) for r in rows],
        sorted((r["case"],int(r["grid"])) for r in rows)==
        [("multi",8),("multi",12),("prism",1),("tetra",1)])
    ref=[]
    for r in rows:
        kind=r["case"]
        n=int(r["grid"])
        mass=float(r["mass"])
        com=float(r["com_x"])
        ixx=float(r["Ixx"])
        ixy=float(r["Ixy"])
        key=f"{kind}_{n}"
        add(key+"_finite",[mass,com,ixx,ixy],
            all(math.isfinite(v) for v in (mass,com,ixx,ixy)))
        if kind=="tetra":
            add(key+"_mass",mass,abs(mass-8/6)<1e-12)
            add(key+"_centroid",com,abs(com-.05)<1e-12)
            add(key+"_inertia_diag",ixx,abs(ixx-.004)<1e-12)
            add(key+"_inertia_offdiag",ixy,abs(ixy-1/1500)<1e-12)
        elif kind=="prism":
            add(key+"_mass",mass,abs(mass-4)<1e-12)
            add(key+"_centroid",com,abs(com-.2/3)<1e-12)
            add(key+"_inertia_diag",ixx,abs(ixx-4*.04*5/36)<1e-12)
            add(key+"_inertia_offdiag",ixy,abs(ixy-4*.04/36)<1e-12)
        elif kind=="multi":
            add(key+"_grid",n,n in (8,12))
            add(key+"_mass_positive",mass,mass>0)
            add(key+"_inertia_positive",ixx,ixx>0)
        else:
            add(key+"_known",kind,False)
        ref.append({"case":kind,"grid":n,"mass_kg":mass,
                    "com_x_m":com,"Ixx_kg_m2":ixx,"Ixy_kg_m2":ixy})
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("source_sha_count",len(hashes),len(hashes)==8)
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2","milestone":"oblique_planar_vof_geometry_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":count,"reference":ref,
        "source_sha256":hashes,"native_binary_sha256":sha(EXE),
        "ctest_log_sha256":sha(CTEST),"build_log_sha256":sha(BUILD),
        "limitations":"Prescribed normals and one planar cut per cell; no PLIC normal reconstruction, conservative VOF advection, moving two-way FSI, independent measured 3D CFD or UE5.8 Native E2E."
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"P2_OBLIQUE_PLIC_PARTIAL={accepted} CHECKS={sum(x['pass'] for x in checks)}/{len(checks)} NATIVE_ASSERTIONS={count} PHASE_COMPLETE=False")
    if not accepted:
        for item in checks:
            if not item["pass"]:print("FAIL",item)
        raise SystemExit(1)
if __name__=="__main__":
    main()
