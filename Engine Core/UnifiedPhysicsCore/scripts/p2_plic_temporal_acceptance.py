"""P2 partial: measured prescribed time-varying 3D MAC/PLIC schedule."""
from __future__ import annotations
import csv
import hashlib
import io
import json
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
EXE=CORE/"build"/"upc_p2_plic_temporal_schedule_tests.exe"
CONFIG=RUNS/"p2_plic_temporal_configure_20261031.txt"
BUILD=RUNS/"p2_plic_temporal_build_20261031.txt"
CTEST=RUNS/"p2_plic_temporal_ctest_20261031.txt"
OUT=RUNS/"p2_plic_temporal_partial.json"
SOURCES=[
    "include/upc/p2_plic_temporal_schedule3d.hpp",
    "include/upc/p2_plic_adaptive_subcycle3d.hpp",
    "include/upc/p2_plic_unsplit_mac3d.hpp",
    "tests/p2_plic_temporal_schedule3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_plic_temporal_build.cmd",
    "scripts/p2_plic_temporal_acceptance.py",
    "docs/P2_PLIC_TEMPORAL_SCHEDULE_PARTIAL.md",
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
    m=re.search(r"PASS: temporal MAC PLIC assertions=(\d+) phase_complete=false",
                native.stdout)
    assertions=int(m.group(1)) if m else 0
    add("native_assertions",assertions,assertions>=1500)
    ctest=CTEST.read_text(encoding="utf-8",errors="replace")
    add("all_106_ctests","0 tests failed out of 106" in ctest,
        "100% tests passed" in ctest and "0 tests failed out of 106" in ctest)
    add("new_test_registered","physics_p2_plic_temporal_schedule_partial" in ctest,
        "physics_p2_plic_temporal_schedule_partial" in ctest)
    add("configure_log",CONFIG.stat().st_size,CONFIG.stat().st_size>0)
    add("build_log",BUILD.stat().st_size,BUILD.stat().st_size>0)
    lines=[s for s in native.stdout.splitlines()
           if s.startswith(("case,","translation,","curl,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("two_cases",len(rows),len(rows)==2)
    reference=[]
    expected=[
        ("translation",2,3,.018,.0017304825534916637,.4),
        ("curl",2,4,.064,0,.22627416997969524),
    ]
    for kind,segments,steps,volume,error,courant in expected:
        matching=[r for r in rows if r["case"]==kind]
        add(kind+"_unique",len(matching),len(matching)==1)
        if not matching:continue
        r=matching[0]
        count=int(r["substeps"])
        initial=float(r["initial_volume"])
        final=float(r["final_volume"])
        l1=float(r["analytic_relative_l1"])
        cfl=float(r["max_step_courant"])
        add(kind+"_segments",int(r["segments"]),int(r["segments"])==segments)
        add(kind+"_steps",count,count==steps)
        add(kind+"_initial_mass",initial,abs(initial-volume)<1e-12)
        add(kind+"_final_mass",final,abs(final-volume)<1e-12)
        add(kind+"_mass_residual",final-initial,abs(final-initial)<1e-12)
        add(kind+"_reference_l1",l1,abs(l1-error)<1e-11)
        add(kind+"_cfl",cfl,abs(cfl-courant)<1e-11 and cfl<=.4+1e-12)
        reference.append({"case":kind,"segments":segments,"substeps":count,
                          "initial_volume_m3":initial,"final_volume_m3":final,
                          "analytic_relative_l1":l1,"max_step_courant":cfl})
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("source_sha256_count",len(hashes),len(hashes)==len(SOURCES))
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2",
        "milestone":"prescribed_piecewise_constant_temporal_mac_plic_partial",
        "partial_accepted":accepted,
        "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,
        "native_assertions":assertions,
        "reference":reference,
        "source_sha256":hashes,
        "native_binary_sha256":sha(EXE),
        "configure_log_sha256":sha(CONFIG),
        "build_log_sha256":sha(BUILD),
        "ctest_log_sha256":sha(CTEST),
        "limitations":"Time-varying prescribed periodic divergence-free MAC fields, not Navier-Stokes or two-way FSI; approximate geometric slabs, no measured CFD, global physical energy/momentum, GPU/LOD or Unreal 5.8 Native-E2E acceptance."
    }
    OUT.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"P2_PLIC_TEMPORAL_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]:print("FAIL",c)
        raise SystemExit(1)
if __name__=="__main__":
    main()
