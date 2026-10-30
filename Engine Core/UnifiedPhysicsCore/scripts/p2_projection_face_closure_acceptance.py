"""Independent P2 partial gate: all MAC faces, pressure/clamp/dry partition."""
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
EXE=CORE/"build"/"upc_p2_projection_face_closure_tests.exe"
LOG=RUNS/"p2_projection_face_closure_ctest_20261031.txt"
OUT=RUNS/"p2_projection_face_closure_partial.json"
SOURCES=[
    "include/upc/p2_projection_face_closure3d.hpp",
    "include/upc/p2_projection_gradient_budget3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_projection_face_closure3d_tests.cpp",
    "scripts/p2_projection_face_closure_full_build.cmd",
    "scripts/p2_projection_face_closure_acceptance.py",
    "docs/P2_PROJECTION_FACE_CLOSURE_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
]
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main()->None:
    result=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                          text=True,timeout=90,check=False)
    log=LOG.read_text(encoding="utf-8",errors="replace")
    checks=[]
    def add(name:str,value:object,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",result.returncode,result.returncode==0)
    m=re.search(r"PASS: P2 all-face projection closure assertions=(\d+)",result.stdout)
    count=int(m.group(1)) if m else 0
    add("native_assertions",count,count>=80)
    add("88_ctest_regression",
        "100% tests passed, 0 tests failed out of 88" in log,
        "100% tests passed, 0 tests failed out of 88" in log)
    rows=list(csv.DictReader(io.StringIO("\n".join(
        line for line in result.stdout.splitlines()
        if line.startswith("mode,") or line.startswith("case,")))))
    add("two_3d_grids",len(rows),len(rows)==2)
    references=[]
    for row in rows:
        n=int(row["n"]); dx=float(row["dx"]); rho=float(row["rho"])
        dt=float(row["dt"]); mass=rho*dx**3; c=n//2
        add(f"grid_{n}",n,n in (8,12))
        add(f"valid_parameters_{n}",[dx,rho,dt],
            all(math.isfinite(x) and x>0 for x in (dx,rho,dt)))
        pressure=0.005*rho*dx/dt
        expected_linear=[-0.26*mass,0.17*mass,-0.31*mass]
        expected_energy=-0.094*mass
        centre=[(c+0.2)*dx,(c-0.1)*dx,(c+0.15)*dx]
        # Analytic eight-face delta stencil, independent of C++ implementation.
        changes=[
            (0,c,c,c,-0.01),(1,c,c,c,-0.01),
            (1,c,c+1,c,0.01),(2,c,c,c,-0.01),
            (2,c,c,c+1,0.01),(0,c+1,c,c,-0.25),
            (1,c+1,c+1,c,0.17),(2,c,c,0,-0.31)
        ]
        angular=[0.0,0.0,0.0]
        for axis,x,y,z,dv in changes:
            lever=[
                (x+(0.5 if axis!=0 else 0))*dx-centre[0],
                (y+(0.5 if axis!=1 else 0))*dx-centre[1],
                (z+(0.5 if axis!=2 else 0))*dx-centre[2]
            ]
            j=mass*dv
            angular[(axis+1)%3]+=lever[(axis+2)%3]*j
            angular[(axis+2)%3]-=lever[(axis+1)%3]*j
        expected={
            "pressure":pressure,
            "px":expected_linear[0],"py":expected_linear[1],
            "pz":expected_linear[2],
            "Lx":angular[0],"Ly":angular[1],"Lz":angular[2],
            "energy":expected_energy,
            "pressure_faces":5,
            "clamped_faces":6*n*n+6,
            "dry_faces":3*n*n*n-3*n*n-11,
            "dry_changed":0,"clamp_nonzero":0,
            "max_residual":0,"gradient_error":0,
        }
        for name,reference in expected.items():
            actual=float(row[name])
            tol=1e-12 if name not in (
                "pressure_faces","clamped_faces","dry_faces",
                "dry_changed","clamp_nonzero") else 0
            add(f"{n}_{name}",actual-reference,
                math.isfinite(actual) and abs(actual-reference)<=tol)
        face_count=int(row["pressure_faces"])+int(row["clamped_faces"])+int(row["dry_faces"])
        add(f"{n}_all_faces_partition",face_count,
            face_count==3*(n+1)*n*n)
        references.append({"n":n,"dx_m":dx,"rho_kg_m3":rho,
                           "pressure_pa":pressure,
                           "linear_face_proxy_Ns":expected_linear,
                           "angular_face_proxy_Nms":angular,
                           "energy_face_proxy_J":expected_energy,
                           "partition_faces":face_count})
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("nine_source_hashes",len(hashes),len(hashes)==9)
    accepted=bool(checks) and all(c["pass"] for c in checks)
    report={
        "phase":"P2",
        "milestone":"all_face_projection_partition_fault_detection_partial",
        "partial_accepted":accepted,
        "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,
        "native_assertions":count,
        "independent_reference":references,
        "source_sha256":hashes,
        "native_binary_sha256":sha(EXE),
        "ctest_log_sha256":sha(LOG),
        "limitations":"Staggered face mass proxies only; no dynamic FSI physical impulse or energy conservation, moving collision geometry, external 3D CFD measurement or Unreal 5.8 Native E2E."
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"P2_FACE_CLOSURE_PARTIAL={accepted} "
          f"CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={count} PHASE_COMPLETE=False")
    if not accepted:
        for item in checks:
            if not item["pass"]:
                print("FAIL",item)
        raise SystemExit(1)

if __name__=="__main__":
    main()
