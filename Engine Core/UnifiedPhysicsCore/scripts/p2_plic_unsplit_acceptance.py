"""P2 prescribed unsplit periodic 3D MAC/PLIC PARTIAL acceptance, not full CFD."""
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
EXE=CORE/"build"/"upc_p2_plic_unsplit_mac_tests.exe"
BUILD=RUNS/"p2_plic_unsplit_build_20261031.txt"
CONFIG=RUNS/"p2_plic_unsplit_configure_20261031.txt"
CTEST=RUNS/"p2_plic_unsplit_ctest_20261031.txt"
OUT=RUNS/"p2_plic_unsplit_partial.json"
SOURCES=[
    "include/upc/p2_plic_unsplit_mac3d.hpp",
    "include/upc/p2_plic_strang_periodic3d.hpp",
    "include/upc/p2_plic_sheared_periodic3d.hpp",
    "include/upc/p2_plic_periodic_sweep3d.hpp",
    "include/upc/p2_oblique_plic_geometry3d.hpp",
    "tests/p2_plic_unsplit_mac3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_plic_unsplit_build.cmd",
    "scripts/p2_plic_unsplit_acceptance.py",
    "docs/P2_PLIC_UNSPLIT_MAC_PARTIAL.md",
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
    m=re.search(r"PASS: PLIC unsplit MAC 3D assertions=(\d+) phase_complete=false",
                native.stdout)
    assertions=int(m.group(1)) if m else 0
    add("native_assertions",assertions,assertions>=9266)
    ctest=CTEST.read_text(encoding="utf-8",errors="replace")
    suite=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctest)
    add("at_least_104_ctests",int(suite.group(1)) if suite else 0,
        suite is not None and int(suite.group(1))>=104)
    add("new_test_registered","physics_p2_plic_unsplit_mac_partial" in ctest,
        "physics_p2_plic_unsplit_mac_partial" in ctest)
    add("configure_log",CONFIG.stat().st_size,CONFIG.stat().st_size>0)
    add("build_log",BUILD.stat().st_size,BUILD.stat().st_size>0)
    lines=[s for s in native.stdout.splitlines()
           if s.startswith(("case,","translation,","vortex,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("four_3d_cases",len(rows),len(rows)==4)
    expected=[
        ("translation",(12,10,8),.036,.00266888888888948,.3),
        ("translation",(20,16,12),.18,.00092933333333400,.3),
        ("vortex",(8,8,8),.064,0,.0486629509039023),
        ("vortex",(12,10,8),.192,0,.0450518763618511),
    ]
    reference=[]
    for kind,dims,volume,error,courant in expected:
        prefix=f"{kind}_{dims[0]}x{dims[1]}x{dims[2]}"
        matches=[r for r in rows if r["case"]==kind and
                 tuple(int(r[k]) for k in ("nx","ny","nz"))==dims]
        add(prefix+"_present",len(matches),len(matches)==1)
        if len(matches)!=1:continue
        row=matches[0]
        initial=float(row["initial_volume"])
        final=float(row["final_volume"])
        divergence=float(row["max_divergence"])
        relative=float(row["analytic_relative_l1"])
        six_face=float(row["max_six_face_courant"])
        faces=int(row["transported_faces"])
        add(prefix+"_initial_volume_m3",initial,abs(initial-volume)<1e-12)
        add(prefix+"_final_volume_m3",final,abs(final-volume)<1e-12)
        add(prefix+"_mass_residual_m3",final-initial,
            abs(final-initial)<1e-12)
        add(prefix+"_solenoidal_divergence_s_inv",divergence,
            abs(divergence)<2e-13)
        add(prefix+"_six_face_courant",six_face,
            abs(six_face-courant)<1e-12 and six_face<=1)
        add(prefix+"_geometric_transfers",faces,faces>100)
        add(prefix+"_analytic_relative_l1",relative,
            abs(relative-error)<1e-11 if kind=="translation"
            else relative==0)
        reference.append({
            "case":kind,"dims":dims,"initial_volume_m3":initial,
            "final_volume_m3":final,"max_divergence_s_inv":divergence,
            "analytic_relative_l1":relative,
            "max_six_face_courant":six_face,
            "transported_faces":faces})
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("source_sha256",len(hashes),len(hashes)==len(SOURCES))
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2",
        "milestone":"prescribed_unsplit_periodic_mac_plic_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":assertions,
        "reference":reference,"source_sha256":hashes,
        "native_binary_sha256":sha(EXE),
        "configure_log_sha256":sha(CONFIG),
        "build_log_sha256":sha(BUILD),
        "ctest_log_sha256":sha(CTEST),
        "limitations":"Frozen prescribed divergence-free MAC velocity; geometric slabs may overlap and then fail closed. Not a coupled Navier-Stokes or stress-consistent free-surface solver, two-way moving-body FSI, independently measured CFD reference, global physical energy/momentum proof, GPU integration or UE5.8 native E2E.",
    }
    OUT.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"P2_PLIC_UNSPLIT_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for item in checks:
            if not item["pass"]:print("FAIL",item)
        raise SystemExit(1)

if __name__=="__main__":
    main()
