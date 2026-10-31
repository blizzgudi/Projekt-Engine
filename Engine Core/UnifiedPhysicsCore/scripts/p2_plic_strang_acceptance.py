"""P2 three-axis periodic Strang PLIC quantitative PARTIAL gate, not full CFD."""
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
EXE=CORE/"build"/"upc_p2_plic_strang_periodic_tests.exe"
BUILD=RUNS/"p2_plic_strang_build_20261031.txt"
CTEST=RUNS/"p2_plic_strang_ctest_20261031.txt"
OUT=RUNS/"p2_plic_strang_partial.json"
SOURCES=[
    "include/upc/p2_plic_strang_periodic3d.hpp",
    "include/upc/p2_plic_sheared_periodic3d.hpp",
    "include/upc/p2_plic_periodic_sweep3d.hpp",
    "include/upc/p2_oblique_plic_geometry3d.hpp",
    "include/upc/p2_axis_plic_geometry3d.hpp",
    "tests/p2_plic_strang_periodic3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_plic_strang_build.cmd",
    "scripts/p2_plic_strang_acceptance.py",
    "docs/P2_PLIC_STRANG_PERIODIC_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main()->None:
    native=subprocess.run([str(EXE)],cwd=CORE,text=True,
                          capture_output=True,timeout=120)
    checks=[]
    def add(name:str,value,ok:bool):
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",native.returncode,native.returncode==0)
    m=re.search(r"PASS: PLIC Strang 3D assertions=(\d+) phase_complete=false",
                native.stdout)
    assertions=int(m.group(1)) if m else 0
    add("native_assertions",assertions,assertions>=8300)
    ctest=CTEST.read_text(encoding="utf-8",errors="replace")
    add("103_ctests","0 tests failed out of 103" in ctest and
        "100% tests passed" in ctest,
        "0 tests failed out of 103" in ctest and "100% tests passed" in ctest)
    add("new_test_registered",
        "physics_p2_plic_strang_periodic_partial" in ctest,
        "physics_p2_plic_strang_periodic_partial" in ctest)
    add("build_log",BUILD.stat().st_size,BUILD.stat().st_size>0)
    lines=[s for s in native.stdout.splitlines()
           if s.startswith(("case,","box,","shear,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("four_3d_cases",len(rows),len(rows)==4)
    reference=[]
    cases=[
        ("box",(12,10,8),.036,.075),
        ("box",(20,16,12),.18,.075),
        ("shear",(12,10,8),.192,.1),
        ("shear",(16,12,10),.576,.1),
    ]
    for kind,dims,expected,courant in cases:
        match=[r for r in rows if r["case"]==kind and
               tuple(int(r[k]) for k in ("nx","ny","nz"))==dims]
        prefix=f"{kind}_{dims[0]}x{dims[1]}x{dims[2]}"
        add(prefix+"_present",len(match),len(match)==1)
        if not match:continue
        row=match[0]
        initial=float(row["initial_volume"])
        final=float(row["final_volume"])
        error=float(row["analytic_l1_fraction_error"])
        stages=int(row["active_stages"])
        faces=int(row["transported_faces"])
        c=float(row["max_stage_courant"])
        add(prefix+"_initial_m3",initial,abs(initial-expected)<1e-12)
        add(prefix+"_final_m3",final,abs(final-expected)<1e-12)
        add(prefix+"_mass_residual_m3",final-initial,
            abs(final-initial)<2e-12)
        add(prefix+"_five_stages",stages,stages==5)
        add(prefix+"_geometric_faces",faces,faces>100)
        add(prefix+"_stage_courant",c,abs(c-courant)<1e-13)
        if kind=="box":
            box_cells=36 if dims[0]==12 else 180
            relative=error/box_cells
            add(prefix+"_exact_cuboid_l1_relative",relative,
                0<relative<.02)
        reference.append({
            "case":kind,"dims":dims,
            "initial_volume_m3":initial,"final_volume_m3":final,
            "l1_fraction_error":error,"max_stage_courant":c,
            "active_stages":stages,"transported_faces":faces})
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("source_sha256",len(hashes),len(hashes)==len(SOURCES))
    accepted=all(x["pass"] for x in checks)
    report={
        "phase":"P2",
        "milestone":"three_axis_periodic_strang_geometric_plic_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":assertions,
        "reference":reference,"source_sha256":hashes,
        "native_binary_sha256":sha(EXE),
        "build_log_sha256":sha(BUILD),
        "ctest_log_sha256":sha(CTEST),
        "limitations":"Prescribed periodic lane-wise solenoidal face velocities, direction splitting and approximate normals; no unsplit MAC pressure coupling, moving two-way FSI, global physical energy/impulse closure, independent CFD data, or UE5.8 native E2E.",
    }
    OUT.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"P2_PLIC_STRANG_PARTIAL={accepted} CHECKS={sum(x['pass'] for x in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for item in checks:
            if not item["pass"]:print("FAIL",item)
        raise SystemExit(1)

if __name__=="__main__":
    main()
