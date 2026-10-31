"""Strict bounded P2 geometric PLIC periodic sweep partial gate (not full P2)."""
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
EXE=CORE/"build"/"upc_p2_plic_periodic_sweep_tests.exe"
BUILD=RUNS/"p2_plic_periodic_build_20261031.txt"
CTEST=RUNS/"p2_plic_periodic_ctest_20261031.txt"
OUT=RUNS/"p2_plic_periodic_sweep_partial.json"
SOURCES=[
    "include/upc/p2_plic_periodic_sweep3d.hpp",
    "include/upc/p2_oblique_plic_geometry3d.hpp",
    "include/upc/p2_vof_normal_reconstruction3d.hpp",
    "tests/p2_plic_periodic_sweep3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_plic_periodic_sweep_build.cmd",
    "scripts/p2_plic_periodic_sweep_acceptance.py",
    "docs/P2_PLIC_PERIODIC_SWEEP_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main()->None:
    run=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                       text=True,timeout=120)
    checks=[]
    def add(name,value,ok):
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",run.returncode,run.returncode==0)
    m=re.search(r"PASS: PLIC periodic sweep assertions=(\d+) phase_complete=false",
                run.stdout)
    count=int(m.group(1)) if m else 0
    add("native_assertions",count,count>=6000)
    log=CTEST.read_text(encoding="utf-8",errors="replace")
    add("full_ctest_pass","100% tests passed" in log and
        "0 tests failed out of 101" in log,
        "100% tests passed" in log and "0 tests failed out of 101" in log)
    add("new_test_present","physics_p2_plic_periodic_sweep_partial" in log,
        "physics_p2_plic_periodic_sweep_partial" in log)
    add("build_nonempty",BUILD.stat().st_size,BUILD.stat().st_size>0)
    slab=re.search(r"slab,oblique_right,([0-9.eE+-]+),oblique_left,([0-9.eE+-]+)",
                   run.stdout)
    right=float(slab.group(1)) if slab else math.nan
    left=float(slab.group(2)) if slab else math.nan
    add("analytic_right_triangle",right,abs(right-.03125)<1e-13)
    add("analytic_left_trapezoid",left,abs(left-.21875)<1e-13)
    lines=[line for line in run.stdout.splitlines()
           if line.startswith(("case,","periodic,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("six_axis_direction_cases",len(rows),len(rows)==6)
    parsed=[]
    for axis in range(3):
        for direction in (-1,1):
            matching=[r for r in rows
                      if r["case"]=="periodic" and
                         int(r["axis"])==axis and
                         int(r["direction"])==direction]
            add(f"axis_{axis}_direction_{direction}_present",
                len(matching),len(matching)==1)
            if not matching:continue
            r=matching[0]
            initial=float(r["initial_volume"])
            final=float(r["final_volume"])
            error=float(r["max_error"])
            courant=float(r["courant"])
            faces=int(r["faces"])
            add(f"{axis}_{direction}_initial",initial,abs(initial-.192)<1e-12)
            add(f"{axis}_{direction}_final",final,abs(final-.192)<1e-12)
            add(f"{axis}_{direction}_analytic_error",error,error<3e-12)
            add(f"{axis}_{direction}_courant",courant,abs(courant-.25)<1e-14)
            add(f"{axis}_{direction}_faces",faces,faces>0)
            parsed.append({"axis":axis,"direction":direction,
                           "initial_m3":initial,"final_m3":final,
                           "max_error":error,"active_faces":faces})
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("source_hashes",len(hashes),len(hashes)==9)
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2","milestone":"one_axis_periodic_plic_geometric_transport",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":count,
        "reference":parsed,"analytic_slab":{"right":right,"left":left},
        "source_sha256":hashes,"native_binary_sha256":sha(EXE),
        "build_log_sha256":sha(BUILD),"ctest_log_sha256":sha(CTEST),
        "limitations":"Single uniform periodic Cartesian sweep; not general 3D MAC/VOF Navier-Stokes, coupled momentum/energy, moving-solid FSI, independent CFD experiment or Unreal 5.8 E2E."
    }
    OUT.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"P2_PLIC_PERIODIC_PARTIAL={accepted} CHECKS={sum(x['pass'] for x in checks)}/{len(checks)} NATIVE_ASSERTIONS={count} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]:print("FAIL",c)
        raise SystemExit(1)
if __name__=="__main__":
    main()
