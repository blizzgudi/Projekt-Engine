"""Independent P2 partial acceptance: solid/exterior MAC zero-velocity clamp."""
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
EXE=CORE/"build"/"upc_p2_projection_gradient_budget_tests.exe"
LOG=RUNS/"p2_projection_wall_clamp_ctest_20261030.txt"
OUT=RUNS/"p2_projection_wall_clamp_partial.json"
SOURCES=[
    "include/upc/p2_projection_gradient_budget3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_projection_gradient_budget3d_tests.cpp",
    "scripts/p2_projection_wall_clamp_full_build.cmd",
    "scripts/p2_projection_wall_clamp_acceptance.py",
    "docs/P2_PROJECTION_WALL_CLAMP_BUDGET_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
    "CMakeLists.txt",
]
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()
def main()->None:
    run=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,text=True,
                       timeout=120,check=False)
    log=LOG.read_text(encoding="utf-8",errors="replace")
    rows=list(csv.reader(line for line in run.stdout.splitlines()
                         if line.startswith("clamp,")))
    checks=[]
    def add(name:str,value:object,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",run.returncode,run.returncode==0)
    match=re.search(r"PASS: P2 projection-gradient budget assertions=(\d+)",run.stdout)
    assertions=int(match.group(1)) if match else 0
    add("native_assertions",assertions,assertions>=130)
    add("all_88_ctest_suites","100% tests passed, 0 tests failed out of 88" in log,
        "100% tests passed, 0 tests failed out of 88" in log)
    add("two_3d_grids",len(rows),len(rows)==2)
    references=[]
    for row in rows:
        if len(row)!=15:
            add("row_width",len(row),False)
            continue
        _,n_s,dx_s,rho_s,dt_s,*vals=row
        n=int(n_s); dx=float(dx_s); rho=float(rho_s); dt=float(dt_s)
        observed=[float(x) for x in vals]
        mass=rho*dx**3
        j=[-mass*0.25,mass*0.17,-mass*0.31]
        c=n//2
        lever=[
            [(c+1)*dx,(c+0.5)*dx,(c+0.5)*dx],
            [(c+1.5)*dx,(c+1)*dx,(c+0.5)*dx],
            [(c+0.5)*dx,(c+0.5)*dx,0],
        ]
        torque=[0.0]*3
        for axis in range(3):
            torque[(axis+1)%3]+=lever[axis][(axis+2)%3]*j[axis]
            torque[(axis+2)%3]-=lever[axis][(axis+1)%3]*j[axis]
        energy=-0.5*mass*(0.25**2+0.17**2+0.31**2)
        expected=j+torque+[energy,6,6*n*n,0]
        add(f"grid_{n}",n,n in (8,12))
        add(f"positive_parameters_{n}",[dx,rho,dt],
            all(math.isfinite(v) and v>0 for v in (dx,rho,dt)))
        for k,(got,ref) in enumerate(zip(observed,expected)):
            tol=1e-12 if k<7 else 0
            add(f"independent_component_{n}_{k}",got-ref,
                math.isfinite(got) and abs(got-ref)<=tol)
        references.append({"n":n,"dx_m":dx,"rho_kg_m3":rho,
                           "impulse_Ns":j,"angular_impulse_Nms":torque,
                           "energy_proxy_J":energy,"solid_faces":6,
                           "exterior_faces":6*n*n})
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("nine_source_hashes",len(hashes),len(hashes)==9)
    accepted=bool(checks) and all(c["pass"] for c in checks)
    report={
        "phase":"P2","milestone":"solid_exterior_mac_projection_clamp_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":assertions,
        "independent_reference":references,"source_sha256":hashes,
        "native_binary_sha256":sha(EXE),"ctest_log_sha256":sha(LOG),
        "limitations":"Numerical staggered-face proxy; not physical wall traction, "
                      "moving FSI, global energy conservation, CFD validation "
                      "or Unreal 5.8 Native E2E."
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"P2_WALL_CLAMP_PARTIAL={accepted} "
          f"CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]:print("FAIL",c)
        raise SystemExit(1)
if __name__=="__main__":
    main()
