"""P2 PARTIAL: independently verify nonlinear fractional 3D pressure wrench.

Reference uses explicit cut-face area integration and exact swept-volume overlap.
Counterimpulses remain diagnostics, NOT coupled FSI.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parent.parent
LAB = CORE.parents[1] / "ENGINE TEST STUBE"
RUNS = LAB / "runs"
LOG = RUNS / "p2_pressure_wrench_reference_full_ctest_20261030.txt"
OUT = RUNS / "p2_pressure_wrench_reference_partial.json"
EXE = CORE / "build" / "upc_p2_pressure_wrench_reference_tests.exe"
SOURCES = [
    "include/upc/p2_pressure_wrench_reference3d.hpp",
    "include/upc/p2_moving_box_pressure_traction3d.hpp",
    "include/upc/p2_moving_box_open_volume3d.hpp",
    "include/upc/p2_moving_box_dynamic_pressure3d.hpp",
    "tests/p2_pressure_wrench_reference3d_tests.cpp",
    "scripts/p2_pressure_wrench_reference_full_build.cmd",
    "scripts/p2_pressure_wrench_reference_acceptance.py",
    "docs/P2_PRESSURE_WRENCH_REFERENCE_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
]

def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def pressure(q: tuple[int,int,int]) -> float:
    x,y,z = (v+0.5 for v in q)
    return 100.0+3*x-2*y+4*z+0.25*(x-4.5)**3*(y-4.5)

def overlap(a: float, b: float, c: float, d: float) -> float:
    return max(0.0,min(b,d)-max(a,c))

def expected(n: int, dx: float, sign: int) -> dict:
    lo = [v*dx for v in (3.2,3.1,3.3)]
    hi = [v*dx for v in (5.7,5.6,5.8)]
    d = [sign*v*dx for v in (0.11,-0.08,0.09)]
    midlo = [lo[a]+0.5*d[a] for a in range(3)]
    midhi = [hi[a]+0.5*d[a] for a in range(3)]
    center = [(midlo[a]+midhi[a])/2 for a in range(3)]
    reference = [v*dx for v in (3.4,4.9,4.1)]
    f = [0.0]*3
    torque = [0.0]*3
    for axis in range(3):
        b,c = [a for a in range(3) if a != axis]
        for side in (-1,1):
            face = midlo[axis] if side<0 else midhi[axis]
            normal_cell = math.floor(math.nextafter(
                face/dx, -math.inf if side<0 else math.inf))
            for j in range(n):
                lb = overlap(j*dx,(j+1)*dx,midlo[b],midhi[b])
                if not lb: continue
                cb = (max(j*dx,midlo[b])+min((j+1)*dx,midhi[b]))/2
                for k in range(n):
                    lc = overlap(k*dx,(k+1)*dx,midlo[c],midhi[c])
                    if not lc: continue
                    cc = (max(k*dx,midlo[c])+min((k+1)*dx,midhi[c]))/2
                    q = [0,0,0]
                    q[axis],q[b],q[c] = normal_cell,j,k
                    traction = -side*pressure(tuple(q))*lb*lc
                    f[axis] += traction
                    arm = [0.0]*3
                    arm[axis] = face-center[axis]
                    arm[b] = cb-center[b]
                    arm[c] = cc-center[c]
                    unit_force = [0.0]*3
                    unit_force[axis] = traction
                    torque[0] += arm[1]*unit_force[2]-arm[2]*unit_force[1]
                    torque[1] += arm[2]*unit_force[0]-arm[0]*unit_force[2]
                    torque[2] += arm[0]*unit_force[1]-arm[1]*unit_force[0]
    # Exact piecewise-constant cell pressure integral over swept solid volume.
    work = 0.0
    for z in range(n):
        for y in range(n):
            for x in range(n):
                q=(x,y,z)
                def solid_volume(shift: float) -> float:
                    lengths = [overlap(q[a]*dx,(q[a]+1)*dx,
                                       lo[a]+shift*d[a],hi[a]+shift*d[a])
                               for a in range(3)]
                    return math.prod(lengths)
                work += pressure(q)*(solid_volume(0)-solid_volume(1))
    arm = [center[a]-reference[a] for a in range(3)]
    cross = [arm[1]*f[2]-arm[2]*f[1],
             arm[2]*f[0]-arm[0]*f[2],
             arm[0]*f[1]-arm[1]*f[0]]
    return {"force":f,"torque":torque,
            "reference_torque":[torque[a]+cross[a] for a in range(3)],
            "work":work}

def main() -> None:
    if not EXE.is_file() or not LOG.is_file():
        raise SystemExit("Missing native executable or full CTest log")
    native = subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                            text=True,timeout=60,check=False)
    log = LOG.read_text(encoding="utf-8",errors="replace")
    checks=[]
    def add(name: str,value: object,ok: bool) -> None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",native.returncode,native.returncode==0)
    match=re.search(r"PASS: (\d+) native nonlinear 3D pressure-wrench assertions",
                    native.stdout)
    count=int(match.group(1)) if match else 0
    add("native_assertions",count,count>=108)
    add("full_ctest_87",bool(re.search(
        r"100% tests passed, 0 tests failed out of 87",log)),
        bool(re.search(r"100% tests passed, 0 tests failed out of 87",log)))
    pattern=(r"CASE n=(\d+) dx=([-+\d.eE]+) sign=(-?\d+) "
             r"fx=([-+\d.eE]+) fy=([-+\d.eE]+) fz=([-+\d.eE]+) "
             r"tx=([-+\d.eE]+) ty=([-+\d.eE]+) tz=([-+\d.eE]+) "
             r"rx=([-+\d.eE]+) ry=([-+\d.eE]+) rz=([-+\d.eE]+) "
             r"work=([-+\d.eE]+) mismatch=([-+\d.eE]+)")
    rows=re.findall(pattern,native.stdout)
    add("four_3d_grids_and_directions",len(rows),len(rows)==4)
    refs=[]
    for row in rows:
        n,dx,sign=int(row[0]),float(row[1]),int(row[2])
        values=list(map(float,row[3:]))
        expected_values=expected(n,dx,sign)
        names=["force","torque","reference_torque"]
        actual={"force":values[0:3],"torque":values[3:6],
                "reference_torque":values[6:9],"work":values[9],
                "work_mismatch":values[10]}
        for name in names:
            for axis in range(3):
                measured=actual[name][axis]
                target=expected_values[name][axis]
                add(f"{name}_{n}_{sign}_{axis}",measured,
                    math.isfinite(measured) and abs(measured-target)<1e-10)
        add(f"geometric_work_{n}_{sign}",actual["work"],
            abs(actual["work"]-expected_values["work"])<1e-10)
        add(f"nonzero_intrinsic_torque_{n}_{sign}",actual["torque"][2],
            abs(actual["torque"][2])>1e-8)
        add(f"midpoint_work_residual_{n}_{sign}",actual["work_mismatch"],
            abs(actual["work_mismatch"])<1e-10)
        refs.append({"n":n,"dx":dx,"direction":sign,"native":actual,
                     "independent_reference":expected_values})
    add("cases_unique",len({(r["n"],r["direction"]) for r in refs}),
        {(r["n"],r["direction"]) for r in refs}==
        {(8,-1),(8,1),(12,-1),(12,1)})
    hashes={name:sha(CORE/name) for name in SOURCES if (CORE/name).is_file()}
    add("all_source_hashes",len(hashes),len(hashes)==len(SOURCES))
    accepted=all(c["pass"] for c in checks)
    result={"phase":"P2","milestone":"nonlinear_3d_pressure_wrench_reference_partial",
            "partial_accepted":accepted,"phase_complete":False,
            "generated_utc":datetime.now(timezone.utc).isoformat(),
            "checks":checks,"reference":refs,"native_assertions":count,
            "native_stdout":native.stdout,"native_stderr":native.stderr[-1500:],
            "source_sha256":hashes,"native_binary_sha256":sha(EXE),
            "ctest_log_sha256":sha(LOG),
            "limitations":"Diagnostic angular reaction only; no rotating body, "
            "fluid momentum feedback, stress-correct free surface or Native UE E2E."}
    RUNS.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(result,indent=2,ensure_ascii=False),
                   encoding="utf-8")
    print(f"P2_WRENCH_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={count} SOURCE_HASHES={len(hashes)}")
    print(f"EVIDENCE={OUT}")
    if not accepted: raise SystemExit(1)

if __name__=="__main__":
    main()
