"""P2 PARTIAL: independent analytic 3D free-surface projection pressure budget.
A green report does NOT imply physical fluid/solid impulse or energy closure.
"""
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
LOG=RUNS/"p2_projection_gradient_budget_ctest_20261030.txt"
OUT=RUNS/"p2_projection_gradient_budget_partial.json"
SOURCES=[
    "include/upc/p2_projection_gradient_budget3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_projection_gradient_budget3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_projection_gradient_budget_acceptance.py",
    "docs/P2_PROJECTION_GRADIENT_BUDGET_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
]
def sha(path: Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main()->None:
    proc=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                        text=True,timeout=120,check=False)
    log=LOG.read_text(encoding="utf-8",errors="replace")
    lines=proc.stdout.splitlines()
    table=list(csv.DictReader(line for line in lines if line and
        (line.startswith("mode,") or line.startswith("single,") or line.startswith("cube,"))))
    checks=[]
    def add(name:str,value:object,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",proc.returncode,proc.returncode==0)
    match=re.search(r"PASS: P2 projection-gradient budget assertions=(\d+)",proc.stdout)
    count=int(match.group(1)) if match else 0
    add("native_assertions",count,count>=100)
    add("full_ctest_88","100% tests passed, 0 tests failed out of 88" in log,
        "100% tests passed, 0 tests failed out of 88" in log)
    add("five_3d_scenarios",len(table),len(table)==5)
    reference=[]
    flux=0.20-(-0.06)+(-0.11)-0.03+0.08-0.05
    for i,row in enumerate(table):
        mode=row["mode"]
        n=int(row["n"])
        dx=float(row["dx"])
        rho=float(row["rho"])
        dt=float(row["dt"])
        div=float(row["div_after"])
        residual=float(row["gradient_residual"])
        energy=float(row["energy_observed_J"])
        work=float(row["work_J"])
        quad=float(row["quadratic_J"])
        suffix=f"{mode}_{n}_{i}"
        add(f"mode_{suffix}",mode,mode in ("single","cube"))
        add(f"divergence_{suffix}",div,math.isfinite(div) and 0<=div<1e-8)
        add(f"face_gradient_residual_{suffix}",residual,
            math.isfinite(residual) and 0<=residual<1e-12)
        add(f"pressure_work_identity_{suffix}",energy-work-quad,
            math.isfinite(energy) and abs(energy-work-quad)<1e-11)
        add(f"wetwet_{suffix}",int(row["wetwet"]),
            int(row["wetwet"])==(0 if mode=="single" else 12))
        add(f"wetair_{suffix}",int(row["wetair"]),
            int(row["wetair"])==(6 if mode=="single" else 24))
        if mode=="single":
            pressure_ref=-rho*dx*flux/(12*dt)
            energy_ref=-rho*dx**3*flux**2/12
            got_p=float(row["analytic_pressure_pa"])
            got_e=float(row["analytic_energy_J"])
            add(f"independent_poisson_{suffix}",got_p,
                math.isfinite(got_p) and abs(got_p-pressure_ref)<1e-10)
            add(f"independent_energy_{suffix}",energy,
                math.isfinite(energy) and abs(energy-energy_ref)<1e-12)
            add(f"analytic_energy_report_{suffix}",got_e,
                math.isfinite(got_e) and abs(got_e-energy_ref)<1e-12)
            reference.append({"n":n,"pressure_pa":pressure_ref,
                              "energy_J":energy_ref,"observed_energy_J":energy})
        else:
            add(f"nonzero_3d_cube_energy_{suffix}",energy,
                math.isfinite(energy) and energy < -1e-5)
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("eight_source_hashes",len(hashes),len(hashes)==8)
    accepted=bool(checks) and all(c["pass"] for c in checks)
    report={
        "phase":"P2","milestone":"post_projection_gradient_impulse_energy_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":count,"analytic_reference":reference,
        "scenarios":table,"source_sha256":hashes,
        "native_binary_sha256":sha(EXE),"ctest_log_sha256":sha(LOG),
        "limitations":"Staggered-face rho*dx^3 momentum/energy proxies only; no "
        "global FSI, no moving/rotating body geometry, no external CFD data "
        "and no Unreal 5.8 Native E2E for this diagnostic."
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(f"P2_PROJECTION_GRADIENT_PARTIAL={accepted} "
          f"CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={count} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]:print("FAIL",c)
        raise SystemExit(1)
if __name__=="__main__":
    main()
