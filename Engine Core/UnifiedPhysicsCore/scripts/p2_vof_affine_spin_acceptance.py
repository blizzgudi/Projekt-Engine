"""P2 bounded MAC/VOF affine subcell spin validation (not full FSI)."""
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
EXE=CORE/"build"/"upc_p2_vof_affine_spin_tests.exe"
CTEST=RUNS/"p2_vof_affine_spin_ctest_20261031.txt"
BUILD=RUNS/"p2_vof_affine_spin_build_20261031.txt"
OUT=RUNS/"p2_vof_affine_spin_partial.json"
SOURCES=[
    "include/upc/mac3d.hpp",
    "include/upc/p2_vof_angular_inventory3d.hpp",
    "include/upc/p2_vof_uniform_cell_inertia3d.hpp",
    "include/upc/p2_vof_affine_spin3d.hpp",
    "tests/p2_vof_affine_spin3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_vof_affine_spin_full_build.cmd",
    "scripts/p2_vof_affine_spin_acceptance.py",
    "docs/P2_VOF_AFFINE_SPIN_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main()->None:
    p=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,text=True,timeout=60)
    checks=[]
    def add(name:str,value:object,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",p.returncode,p.returncode==0)
    m=re.search(r"PASS: affine MAC subcell spin assertions=(\d+) phase_complete=false",p.stdout)
    assertions=int(m.group(1)) if m else 0
    add("native_assertions",assertions,assertions>=112)
    log=CTEST.read_text(encoding="utf-8",errors="replace")
    add("full_ctest_97",log.count("100% tests passed, 0 tests failed out of 97"),"100% tests passed, 0 tests failed out of 97" in log)
    add("native_ctest_entry","physics_p2_vof_affine_spin_partial" in log,
        "physics_p2_vof_affine_spin_partial" in log)
    add("build_nonempty",BUILD.stat().st_size,BUILD.stat().st_size>0)
    lines=[s for s in p.stdout.splitlines() if s.startswith(("case,","cube,","partial,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("five_native_cases",len(rows),len(rows)==5)
    reference=[]
    expected_mass={8:1.2890625,12:5.85546875}
    for row in rows:
        mode=row["case"]
        n=int(row["grid"])
        mass=float(row["mass"])
        inertia=float(row["Icom"])
        Lz=float(row["Lcom_z"])
        label=f"{mode}_{n}"
        add(label+"_finite",(mass,inertia,Lz),all(math.isfinite(x) for x in (mass,inertia,Lz)))
        if mode=="cube":
            add(label+"_grid",n,n in (8,12,16))
            add(label+"_mass",mass,abs(mass-64)<2e-11)
            add(label+"_inertia",inertia,abs(inertia-64*.4*.4/6)<3e-12)
            add(label+"_spin_corrected_Lz",Lz,abs(Lz-64*.4*.4/6*1.3)<2e-11)
        elif mode=="partial":
            add(label+"_grid",n,n in (8,12))
            add(label+"_mass",mass,abs(mass-expected_mass.get(n,-1))<2e-11)
            add(label+"_Lz_finite",Lz,math.isfinite(Lz) and Lz!=0)
        else:
            add(label+"_known_mode",mode,False)
        reference.append({"mode":mode,"grid":n,"mass_kg":mass,
                          "inertia_com_kg_m2":inertia,"angular_com_z_kg_m2_s":Lz})
    add("exact_case_set",[(r["mode"],r["grid"]) for r in reference],
        sorted((r["mode"],r["grid"]) for r in reference)==
        [("cube",8),("cube",12),("cube",16),("partial",8),("partial",12)])
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("source_hash_count",len(hashes),len(hashes)==10)
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2","milestone":"affine_mac_vof_subcell_spin_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":assertions,
        "reference":reference,"source_sha256":hashes,
        "native_binary_sha256":sha(EXE),"ctest_log_sha256":sha(CTEST),
        "build_log_sha256":sha(BUILD),
        "limitations":"Read-only affine subcell angular-momentum diagnostic with uniform partial-cell density; neighbouring dry MAC velocities can bias curl; not reconstructed VOF interface, not conservative time evolution, not measured 3D CFD, not moving-body FSI or UE5.8 Native-E2E."
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(f"P2_AFFINE_SPIN_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]:print("FAIL",c)
        raise SystemExit(1)
if __name__=="__main__":
    main()
