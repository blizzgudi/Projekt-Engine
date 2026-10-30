"""P2 PARTIAL: independent nonlinear pressure-wrench to frozen rigid-body ODE.

The prescribed midpoint pressure box and integrated body trajectory are distinct.
Neither the body motion nor the diagnostic reaction is fed back to MAC/VOF.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

from p2_pressure_wrench_reference_acceptance import expected

CORE = Path(__file__).resolve().parent.parent
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
LOG = RUNS / "p2_frozen_wrench_rigid_full_ctest_20261030.txt"
EXE = CORE / "build" / "upc_p2_frozen_wrench_rigid_tests.exe"
OUT = RUNS / "p2_frozen_wrench_rigid_partial.json"
SOURCES = [
    "include/upc/p2_frozen_wrench_rigid_step3d.hpp",
    "include/upc/p2_pressure_wrench_reference3d.hpp",
    "include/upc/p2_moving_box_pressure_traction3d.hpp",
    "include/upc/rigid_pressure_response3d.hpp",
    "tests/p2_frozen_wrench_rigid_step3d_tests.cpp",
    "scripts/p2_frozen_wrench_rigid_full_build.cmd",
    "scripts/p2_frozen_wrench_rigid_acceptance.py",
    "scripts/p2_pressure_wrench_reference_acceptance.py",
    "docs/P2_FROZEN_WRENCH_RIGID_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
]

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def reference(n: int, dx: float, sign: int) -> dict:
    w = expected(n, dx, sign)  # independently integrate all fractional pressure tiles
    f = w["force"]
    ref = [v * dx for v in (3.4, 4.9, 4.1)]
    centre = [v * dx for v in (4.45, 4.35, 4.55)]
    arm = [ref[i] - centre[i] for i in range(3)]
    cross = [
        arm[1]*f[2] - arm[2]*f[1],
        arm[2]*f[0] - arm[0]*f[2],
        arm[0]*f[1] - arm[1]*f[0],
    ]
    t = [w["reference_torque"][i] + cross[i] for i in range(3)]
    dt, mass = 0.05, 12.0
    inertia = (0.45, 0.63, 0.78)
    v0 = (0.15, -0.07, 0.08)
    omega0 = (0.2, -0.1, 0.15)
    v = [v0[i] + dt*f[i]/mass for i in range(3)]
    omega = [omega0[i] + dt*t[i]/inertia[i] for i in range(3)]
    d = [dt*(v0[i]+v[i])/2 for i in range(3)]
    theta = [dt*(omega0[i]+omega[i])/2 for i in range(3)]
    c = [centre[i]+d[i] for i in range(3)]
    dE = sum(
        mass*(v[i]**2-v0[i]**2)/2+
        inertia[i]*(omega[i]**2-omega0[i]**2)/2
        for i in range(3)
    )
    work = sum(f[i]*d[i]+t[i]*theta[i] for i in range(3))
    new_c = [centre[i] + sign*shift*dx
             for i,shift in enumerate((0.11,-0.08,0.09))]
    mismatch = math.sqrt(sum((c[i]-new_c[i])**2 for i in range(3)))
    return {
        "f": f, "t": t, "v": v, "w": omega, "c": c,
        "deltaE": dE, "work": work, "residual": dE-work,
        "mismatch": mismatch,
    }

def main() -> None:
    if not EXE.is_file() or not LOG.is_file():
        raise SystemExit("Native executable or CTest log missing")
    native = subprocess.run([str(EXE)],cwd=CORE,text=True,capture_output=True,
                            timeout=60,check=False)
    log = LOG.read_text(encoding="utf-8",errors="replace")
    checks = []
    def add(name: str, value: object, ok: bool) -> None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",native.returncode,native.returncode==0)
    match = re.search(r"PASS: (\d+) native frozen-wrench rigid-body assertions",
                      native.stdout)
    assertions = int(match.group(1)) if match else 0
    add("native_assertions",assertions,assertions>=176)
    add("full_ctest_87",bool(re.search(
        r"100% tests passed, 0 tests failed out of 87",log)),
        bool(re.search(r"100% tests passed, 0 tests failed out of 87",log)))
    pattern = (r"CASE n=(\d+) dx=([-+\d.eE]+) sign=(-?\d+)"
               + "".join(fr" {key}=([-+\d.eE]+)" for key in (
                   "f0","f1","f2","t0","t1","t2","v0","v1","v2",
                   "w0","w1","w2","c0","c1","c2","deltaE","work",
                   "residual","mismatch")))
    rows = re.findall(pattern,native.stdout)
    add("four_3d_grids_and_directions",len(rows),len(rows)==4)
    refs = []
    for row in rows:
        n,dx,sign=int(row[0]),float(row[1]),int(row[2])
        vals=list(map(float,row[3:]))
        actual = {
            "f": vals[0:3], "t": vals[3:6], "v": vals[6:9],
            "w": vals[9:12], "c": vals[12:15], "deltaE": vals[15],
            "work": vals[16], "residual": vals[17], "mismatch": vals[18],
        }
        target=reference(n,dx,sign)
        for key in ("f","t","v","w","c"):
            for i,(got,want) in enumerate(zip(actual[key],target[key])):
                add(f"{key}_{n}_{sign}_{i}",got,
                    math.isfinite(got) and abs(got-want)<1e-10)
        for key in ("deltaE","work","residual","mismatch"):
            got,want=actual[key],target[key]
            add(f"{key}_{n}_{sign}",got,
                math.isfinite(got) and abs(got-want)<1e-10)
        add(f"nonzero_spin_{n}_{sign}",actual["t"][2],
            abs(actual["t"][2])>1e-8)
        add(f"prescribed_path_not_integrated_{n}_{sign}",
            actual["mismatch"],actual["mismatch"]>1e-6)
        refs.append({"n":n,"dx":dx,"sign":sign,
                     "native":actual,"independent_reference":target})
    add("unique_cases",len({(r["n"],r["sign"]) for r in refs}),
        {(r["n"],r["sign"]) for r in refs}==
        {(8,-1),(8,1),(12,-1),(12,1)})
    hashes={name:sha(CORE/name) for name in SOURCES if (CORE/name).is_file()}
    add("source_hashes",len(hashes),len(hashes)==len(SOURCES))
    accepted=all(x["pass"] for x in checks)
    report={
        "phase":"P2","milestone":"nonlinear_pressure_to_frozen_rigid_ode_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"reference":refs,"native_assertions":assertions,
        "native_stdout":native.stdout,"native_stderr":native.stderr[-1000:],
        "source_sha256":hashes,"native_binary_sha256":sha(EXE),
        "ctest_log_sha256":sha(LOG),
        "limitations":"Prescribed axis-aligned box and frozen diagonal world inertia; "
        "no orientation integration, body geometry remap, MAC impulse feedback, "
        "global fluid energy closure, measured 3D CFD or Unreal Native-E2E.",
    }
    RUNS.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False),
                   encoding="utf-8")
    print(f"P2_FROZEN_RIGID_PARTIAL={accepted} "
          f"CHECKS={sum(x['pass'] for x in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={assertions} SOURCE_HASHES={len(hashes)}")
    print(f"EVIDENCE={OUT}")
    if not accepted:
        raise SystemExit(1)

if __name__=="__main__":
    main()
