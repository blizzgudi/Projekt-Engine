"""P2 partial native gate: live coupled MAC/VOF viscous energy ledger.

Only algebraic discrete staggered-face diagnostics, NOT continuum energy.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
PROJECT=CORE.parents[1]
OUT=PROJECT/"ENGINE TEST STUBE"/"runs"/"p2_live_energy_partial.json"
BIN=CORE/"build"/"upc_p2_live_viscous_energy_ledger_tests.exe"
SOURCES=(
    "include/upc/coupled_water3d.hpp",
    "include/upc/viscous_mac3d.hpp",
    "tests/coupled_viscous_energy_ledger3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_live_energy_acceptance.py",
    "docs/P2_LIVE_ENERGY_LEDGER_PARTIAL.md",
    "docs/PHASENPLAN.md",
)
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args:list[str],timeout:int)->subprocess.CompletedProcess[str]:
    p=subprocess.run(args,cwd=CORE,capture_output=True,text=True,
                     encoding="utf-8",errors="replace",timeout=timeout)
    if p.returncode:
        raise RuntimeError(f"exit {p.returncode}: {args!r}\n{p.stdout[-2500:]}\n{p.stderr[-2500:]}")
    return p

def main()->None:
    if not BIN.is_file():
        raise SystemExit("Missing native Release executable")
    native=run([str(BIN)],90)
    m=re.search(
        r"PASS P2 live viscous energy ledger checks=(\d+)"
        r" mass_kg=([-+0-9.eE]+) wall_work=([-+0-9.eE]+)"
        r" gradient_loss=([-+0-9.eE]+)"
        r" viscosity_substeps=(\d+) phase_complete=false",
        native.stdout,
    )
    if m is None:
        raise SystemExit("Native live ledger output did not match proof contract")
    n=int(m[1]);mass=float(m[2]);work=float(m[3])
    loss=float(m[4]);substeps=int(m[5])
    checks=[
        {"name":"native_assertions","pass":n>=697,"value":n},
        {"name":"3d_vof_mass_kg","pass":math.isfinite(mass) and abs(mass-36)<1e-8,"value":mass},
        {"name":"wall_work_proxy","pass":math.isfinite(work) and work>1e-8,"value":work},
        {"name":"gradient_loss_proxy","pass":math.isfinite(loss) and loss>1e-8,"value":loss},
        {"name":"adaptive_live_substeps","pass":substeps>=10,"value":substeps},
    ]
    ctest=shutil.which("ctest")
    if not ctest:
        ctest=(r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
               r"\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe")
    if not Path(ctest).is_file():
        raise SystemExit("CTest unavailable")
    full=run([ctest,"--test-dir","build","--output-on-failure","--parallel","5"],240)
    combined=full.stdout+"\n"+full.stderr
    suites=re.search(r"100% tests passed, 0 tests failed out of (\d+)",combined)
    count=int(suites[1]) if suites else 0
    checks.append({"name":"all_native_ctest_suites",
                   "pass":count>=38 and
                     "physics_p2_live_viscous_energy_ledger_partial" in combined,
                   "value":count})
    gate=run([sys.executable,str(CORE/"scripts"/"phase_gate.py")],120)
    checks.append({"name":"full_phases_not_claimed",
                   "pass":"ALL_COMPLETE=False" in gate.stdout
                   and re.search(r"GATES_INCOMPLETE=.*\bP2\b",gate.stdout) is not None})
    hashes={}
    for name in SOURCES:
        f=CORE/name
        if not f.is_file():
            raise SystemExit(f"Missing source: {name}")
        hashes[name]=sha(f)
    checks.append({"name":"source_hash_coverage","pass":len(hashes)==len(SOURCES)})
    passed=all(c["pass"] for c in checks)
    report={
        "phase":"P2",
        "milestone":"live_coupled_viscous_face_energy_ledger_partial",
        "partial_accepted":passed,
        "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,
        "native_stdout":native.stdout.strip(),
        "native_binary_sha256":sha(BIN),
        "ctest_exit_code":full.returncode,
        "ctest_stdout_tail":combined[-10000:],
        "phase_gate_stdout":gate.stdout.strip(),
        "source_sha256":hashes,
        "limitations":"Discrete face-square budget only; no continuum physical energy, "
                      "moving-solid FSI, stress-consistent air/water surface, "
                      "measured 3D CFD reference, or fresh UE5.8 native E2E. "
                      "P2-P8 full phases remain open.",
    }
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    print(f"P2_LIVE_ENERGY_PARTIAL={passed}")
    print(f"P2_NATIVE_ASSERTIONS={n}")
    print(f"P2_CTEST_SUITES={count}")
    print(f"P2_REPORT={OUT}")
    if not passed:
        raise SystemExit("P2 live energy partial gate FAILED")
if __name__=="__main__":
    main()
