"""P2 PARTIAL: independent 6-DOF frozen-pressure body/MAC reaction reference.

Uses independently integrated nonlinear pressure tiles from the earlier P2
reference, and independently reconstructs/solves the MAC minimum-norm
6-constraint face impulse transfer. Does NOT validate physical FSI.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from p2_frozen_wrench_rigid_acceptance import reference

CORE=Path(__file__).resolve().parent.parent
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
LOG=RUNS/"p2_frozen_wrench_mac_reaction_full_ctest_20261030.txt"
EXE=CORE/"build"/"upc_p2_frozen_wrench_mac_reaction_tests.exe"
OUT=RUNS/"p2_frozen_wrench_mac_reaction_partial.json"
SOURCES=[
    "include/upc/p2_frozen_wrench_mac_reaction3d.hpp",
    "include/upc/p2_frozen_wrench_rigid_step3d.hpp",
    "include/upc/p2_pressure_wrench_reference3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_frozen_wrench_mac_reaction3d_tests.cpp",
    "scripts/p2_frozen_wrench_mac_reaction_full_build.cmd",
    "scripts/p2_frozen_wrench_mac_reaction_acceptance.py",
    "scripts/p2_frozen_wrench_rigid_acceptance.py",
    "docs/P2_FROZEN_WRENCH_MAC_REACTION_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
]

def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def solve(matrix: list[list[float]], rhs: list[float]) -> list[float]:
    # Independent unscaled Gaussian elimination (C++ uses diagonal scaling).
    a=[r[:] for r in matrix]
    b=rhs[:]
    for k in range(6):
        pivot=max(range(k,6),key=lambda i:abs(a[i][k]))
        if abs(a[pivot][k])<1e-13:
            raise ValueError("Independent reference rank deficiency")
        a[k],a[pivot]=a[pivot],a[k]
        b[k],b[pivot]=b[pivot],b[k]
        for i in range(k+1,6):
            factor=a[i][k]/a[k][k]
            for j in range(k,6):
                a[i][j]-=factor*a[k][j]
            b[i]-=factor*b[k]
    x=[0.0]*6
    for i in range(5,-1,-1):
        x[i]=(b[i]-sum(a[i][j]*x[j] for j in range(i+1,6)))/a[i][i]
    return x

def expected(n: int, dx: float, sign: int, seeded: int) -> dict:
    dt=0.05
    mass=1000.0*dx**3
    body_impulse=reference(n,dx,sign)
    target=[-dt*v for v in body_impulse["f"]+body_impulse["t"]]
    centre=[4.45*dx,4.35*dx,4.55*dx]
    def is_body(x: int,y: int,z: int) -> bool:
        return 4<=x<=5 and 4<=y<=5 and 4<=z<=5
    def wet(x: int,y: int,z: int) -> bool:
        return 0<=x<n and 0<=y<n and 0<=z<n and not is_body(x,y,z)
    def adjacent(x: int,y: int,z: int) -> bool:
        return any(is_body(x+u,y+v,z+w) for u,v,w in (
            (-1,0,0),(1,0,0),(0,-1,0),(0,1,0),(0,0,-1),(0,0,1)))
    faces=[]
    count=[0,0,0]
    for z in range(n):
        for y in range(n):
            for x in range(n):
                if not wet(x,y,z):
                    continue
                for axis in range(3):
                    xx=x+(axis==0)
                    yy=y+(axis==1)
                    zz=z+(axis==2)
                    if not wet(xx,yy,zz) or not (adjacent(x,y,z) or adjacent(xx,yy,zz)):
                        continue
                    r=[(xx+(0.5 if axis!=0 else 0))*dx-centre[0],
                       (yy+(0.5 if axis!=1 else 0))*dx-centre[1],
                       (zz+(0.5 if axis!=2 else 0))*dx-centre[2]]
                    col=[0.0]*6
                    col[axis]=1.0
                    col[3+(axis+1)%3]=r[(axis+2)%3]
                    col[3+(axis+2)%3]=-r[(axis+1)%3]
                    baseline=(0.01*(axis+1)+0.0005*(xx-2*yy+zz)) if seeded else 0.0
                    faces.append((axis,xx,yy,zz,col,baseline))
                    count[axis]+=1
    gram=[[sum(mass*f[4][i]*f[4][j] for f in faces)
           for j in range(6)] for i in range(6)]
    lagrange=solve(gram,target)
    applied=[0.0]*6
    energy=0.0
    maxdv=0.0
    velocities={}
    for axis,x,y,z,col,baseline in faces:
        dv=sum(a*b for a,b in zip(col,lagrange))
        j=mass*dv
        for i in range(6):
            applied[i]+=col[i]*j
        energy+=mass*dv*(baseline+0.5*dv)
        maxdv=max(maxdv,abs(dv))
        velocities[(axis,x,y,z)]=dv
    def baseline(axis: int,x: int,y: int,z: int) -> float:
        if (axis==0 and (x==0 or x==n)) or (
            axis==1 and (y==0 or y==n)) or (
            axis==2 and (z==0 or z==n)):
            return 0.0
        a=(x-(axis==0),y-(axis==1),z-(axis==2))
        if not wet(*a) or not wet(x,y,z):
            return 0.0
        return (0.01*(axis+1)+0.0005*(x-2*y+z)) if seeded else 0.0
    def v(axis: int,x: int,y: int,z: int) -> float:
        return baseline(axis,x,y,z)+velocities.get((axis,x,y,z),0.0)
    divergence=0.0
    for z in range(n):
        for y in range(n):
            for x in range(n):
                if wet(x,y,z):
                    d=(v(0,x+1,y,z)-v(0,x,y,z)+
                       v(1,x,y+1,z)-v(1,x,y,z)+
                       v(2,x,y,z+1)-v(2,x,y,z))/dx
                    divergence=max(divergence,abs(d))
    return {"count":count,"lin":applied[:3],"ang":applied[3:],
            "energy":energy,"maxdv":maxdv,"divergence":divergence,
            "target":target,"faces":len(faces)}

def main() -> None:
    if not EXE.is_file() or not LOG.is_file():
        raise SystemExit("Native binary or full CTest log missing")
    proc=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                        text=True,timeout=90,check=False)
    log=LOG.read_text(encoding="utf-8",errors="replace")
    checks=[]
    def add(name: str,value: object,ok: bool) -> None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",proc.returncode,proc.returncode==0)
    m=re.search(r"PASS: (\d+) native frozen-wrench MAC reaction assertions",
                proc.stdout)
    assertions=int(m.group(1)) if m else 0
    add("native_assertions",assertions,assertions>=100)
    full="100% tests passed, 0 tests failed out of 87" in log
    add("full_ctest_87",full,full)
    pattern=(r"CASE n=(\d+) dx=([-+\d.eE]+) sign=(-?\d+) seeded=(\d+)"
             + "".join(fr" {key}=([-+\d.eE]+)" for key in (
                 "count0","count1","count2",
                 "lin0","lin1","lin2","ang0","ang1","ang2",
                 "energy","maxdv","divergence")))
    rows=re.findall(pattern,proc.stdout)
    add("eight_cases",len(rows),len(rows)==8)
    results=[]
    for row in rows:
        n,dx,sign,seeded=int(row[0]),float(row[1]),int(row[2]),int(row[3])
        values=list(map(float,row[4:]))
        actual={"count":values[:3],"lin":values[3:6],
                "ang":values[6:9],"energy":values[9],
                "maxdv":values[10],"divergence":values[11]}
        want=expected(n,dx,sign,seeded)
        suffix=f"{n}_{sign}_{seeded}"
        for key in ("count","lin","ang"):
            for i,(got,ref) in enumerate(zip(actual[key],want[key])):
                tol=0.0 if key=="count" else 1e-9
                add(f"{key}_{suffix}_{i}",got,
                    math.isfinite(got) and abs(got-ref)<=(tol if tol else 0))
        for key in ("energy","maxdv","divergence"):
            got,ref=actual[key],want[key]
            add(f"{key}_{suffix}",got,
                math.isfinite(got) and abs(got-ref)<1e-9)
        for i in range(3):
            add(f"linear_body_fluid_{suffix}_{i}",
                actual["lin"][i]+want["target"][i]*-1,
                abs(actual["lin"][i]-want["target"][i])<1e-9)
            add(f"angular_body_fluid_{suffix}_{i}",
                actual["ang"][i]+want["target"][i+3]*-1,
                abs(actual["ang"][i]-want["target"][i+3])<1e-9)
        add(f"nonzero_unprojected_divergence_{suffix}",
            actual["divergence"],actual["divergence"]>1e-6)
        results.append({"n":n,"dx":dx,"sign":sign,"seeded":seeded,
                        "native":actual,"independent_reference":want})
    add("unique_cases",len({(r["n"],r["sign"],r["seeded"]) for r in results}),
        len({(r["n"],r["sign"],r["seeded"]) for r in results})==8)
    hashes={name:sha(CORE/name) for name in SOURCES if (CORE/name).is_file()}
    add("source_hashes",len(hashes),len(hashes)==len(SOURCES))
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2",
        "milestone":"frozen_nonlinear_pressure_wrench_to_mac_6dof_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"reference":results,"native_assertions":assertions,
        "native_stdout":proc.stdout,"native_stderr":proc.stderr[-1500:],
        "source_sha256":hashes,"native_binary_sha256":sha(EXE),
        "ctest_log_sha256":sha(LOG),
        "limitations":"Frozen pressure and static voxel mask; MAC face mass is a "
        "discrete proxy, no rotating geometry, no post-reaction pressure "
        "projection, no physical FSI momentum/energy closure, no measured "
        "3D CFD and no Unreal 5.8 Native-E2E for this adapter.",
    }
    RUNS.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False),
                   encoding="utf-8")
    print(f"P2_FROZEN_WRENCH_MAC_PARTIAL={accepted} "
          f"CHECKS={sum(x['pass'] for x in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={assertions} SOURCE_HASHES={len(hashes)}")
    print(f"EVIDENCE={OUT}")
    if not accepted:
        raise SystemExit(1)

if __name__=="__main__":
    main()
