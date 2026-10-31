"""Bounded P2 3D MAC/PLIC adaptive-CFL partial acceptance, not full CFD."""
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
EXE=CORE/"build"/"upc_p2_plic_adaptive_subcycle_tests.exe"
BUILD=RUNS/"p2_plic_adaptive_build_20261031.txt"
CONFIG=RUNS/"p2_plic_adaptive_configure_20261031.txt"
CTEST=RUNS/"p2_plic_adaptive_ctest_20261031.txt"
OUT=RUNS/"p2_plic_adaptive_partial.json"
SOURCES=[
    "include/upc/p2_plic_adaptive_subcycle3d.hpp",
    "include/upc/p2_plic_unsplit_mac3d.hpp",
    "include/upc/p2_plic_strang_periodic3d.hpp",
    "include/upc/p2_oblique_plic_geometry3d.hpp",
    "tests/p2_plic_adaptive_subcycle3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_plic_adaptive_build.cmd",
    "scripts/p2_plic_adaptive_acceptance.py",
    "docs/P2_PLIC_ADAPTIVE_SUBCYCLE_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main()->None:
    native=subprocess.run([str(EXE)],cwd=CORE,text=True,
                          capture_output=True,timeout=120)
    checks=[]
    def add(name:str,value,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",native.returncode,native.returncode==0)
    m=re.search(r"PASS: adaptive PLIC subcycling assertions=(\d+) phase_complete=false",
                native.stdout)
    assertions=int(m.group(1)) if m else 0
    add("native_assertions",assertions,assertions>=2400)
    ctest=CTEST.read_text(encoding="utf-8",errors="replace")
    add("105_ctests",ctest.count("0 tests failed out of 105"),
        "0 tests failed out of 105" in ctest and "100% tests passed" in ctest)
    add("new_test_registered","physics_p2_plic_adaptive_subcycle_partial" in ctest,
        "physics_p2_plic_adaptive_subcycle_partial" in ctest)
    add("configure_log",CONFIG.stat().st_size,CONFIG.stat().st_size>0)
    add("build_log",BUILD.stat().st_size,BUILD.stat().st_size>0)
    lines=[s for s in native.stdout.splitlines()
           if s.startswith(("case,","translation,","seam,","curl,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("three_cases",len(rows),len(rows)==3)
    expected=[
        ("translation",(12,10,8),.036,6,.11663344154143769,.4),
        ("seam",(12,10,8),.018,3,.00044786202841333,.32),
        ("curl",(8,8,8),.064,6,0,.22627416997969524),
    ]
    reference=[]
    for kind,dims,volume,steps,error,courant in expected:
        match=[r for r in rows if r["case"]==kind and
               tuple(int(r[k]) for k in ("nx","ny","nz"))==dims]
        add(kind+"_present",len(match),len(match)==1)
        if len(match)!=1:continue
        row=match[0]
        initial=float(row["initial_volume"])
        final=float(row["final_volume"])
        relative=float(row["analytic_relative_l1"])
        max_cfl=float(row["max_step_courant"])
        count=int(row["substeps"])
        faces=int(row["transported_faces"])
        add(kind+"_initial_volume",initial,abs(initial-volume)<1e-12)
        add(kind+"_final_volume",final,abs(final-volume)<1e-12)
        add(kind+"_mass_residual",final-initial,abs(final-initial)<1e-12)
        add(kind+"_substeps",count,count==steps and count>1)
        add(kind+"_courant",max_cfl,abs(max_cfl-courant)<1e-11 and max_cfl<=.4+1e-12)
        add(kind+"_reference_l1",relative,abs(relative-error)<1e-10)
        add(kind+"_transfers",faces,faces>0)
        reference.append({"case":kind,"dims":dims,"substeps":count,
                          "initial_volume_m3":initial,"final_volume_m3":final,
                          "analytic_relative_l1":relative,
                          "max_step_courant":max_cfl,
                          "transported_faces":faces})
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("source_sha256",len(hashes),len(hashes)==len(SOURCES))
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2",
        "milestone":"prescribed_adaptive_cfl_mac_plic_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":assertions,
        "reference":reference,"source_sha256":hashes,
        "native_binary_sha256":sha(EXE),
        "configure_log_sha256":sha(CONFIG),
        "build_log_sha256":sha(BUILD),
        "ctest_log_sha256":sha(CTEST),
        "limitations":"Prescribed frozen periodic divergence-free MAC velocity, not a Navier-Stokes time step. Approximate simultaneous geometric slabs, not exact spacetime transport. No moving-body FSI, independent measured 3D CFD reference, global physical energy/momentum proof or Unreal 5.8 Native-E2E.",
    }
    OUT.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"P2_PLIC_ADAPTIVE_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for item in checks:
            if not item["pass"]:print("FAIL",item)
        raise SystemExit(1)

if __name__=="__main__":
    main()
