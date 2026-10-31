"""P2 partial: VOF gravitational potential transport ledger, NOT full energy/FSI."""
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
EXE=CORE/"build"/"upc_p2_vof_gravity_ledger_tests.exe"
LOG=RUNS/"p2_vof_gravity_ledger_ctest_20261031.txt"
OUT=RUNS/"p2_vof_gravity_ledger_partial.json"
SOURCES=[
    "include/upc/coupled_water3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_vof_gravity_ledger3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_vof_gravity_ledger_full_build.cmd",
    "scripts/p2_vof_gravity_ledger_acceptance.py",
    "docs/P2_VOF_GRAVITY_LEDGER_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
]
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main()->None:
    result=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,text=True,timeout=90,check=False)
    raw=LOG.read_bytes()
    log=raw.decode("utf-16" if raw.startswith((b"\xff\xfe",b"\xfe\xff")) else "utf-8",errors="replace")
    checks=[]
    def add(name:str,value:object,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",result.returncode,result.returncode==0)
    m=re.search(r"PASS: VOF gravitational transfer ledger assertions=(\d+)",result.stdout)
    assertions=int(m.group(1)) if m else 0
    add("native_assertions",assertions,assertions>=72)
    add("msvc_92_ctest","100% tests passed, 0 tests failed out of 92" in log,
        "100% tests passed, 0 tests failed out of 92" in log)
    lines=[x for x in result.stdout.splitlines() if x.startswith(("mode,","case,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("two_grid_cases",len(rows),len(rows)==2)
    refs=[]
    for row in rows:
        n=int(row["n"])
        mass_before=float(row["mass_before"])
        mass_after=float(row["mass_after"])
        before=float(row["potential_before"])
        after=float(row["potential_after"])
        ledger=float(row["ledger_delta"])
        events=float(row["event_delta"])
        residual=float(row["residual"])
        count=int(row["events"])
        # Seven unit-filled cells: four at c, three at c+1, each 1 kg.
        c=n//2
        analytic=9.81*0.1*(4*(c+0.5)+3*(c+1.5))
        add(f"{n}_grid",n,n in (8,12))
        add(f"{n}_mass_before",mass_before,abs(mass_before-7)<1e-10)
        add(f"{n}_mass_conserved",mass_after-mass_before,
            abs(mass_after-mass_before)<1e-10)
        add(f"{n}_independent_potential",before,
            math.isfinite(before) and abs(before-analytic)<1e-11)
        add(f"{n}_state_ledger",after-before-ledger,
            math.isfinite(after) and math.isfinite(ledger) and abs(after-before-ledger)<2e-12)
        add(f"{n}_independent_event_sum",events-ledger,
            math.isfinite(events) and abs(events-ledger)<2e-12)
        add(f"{n}_gravity_decreases_potential",ledger,ledger<0)
        add(f"{n}_post_projection_residual",residual,
            math.isfinite(residual) and 0<=residual<=1e-8)
        add(f"{n}_real_transfers",count,count>=1)
        refs.append({"n":n,"mass_before_kg":mass_before,"mass_after_kg":mass_after,
                     "potential_before_j":before,"potential_after_j":after,
                     "vof_transport_delta_j":ledger,"independent_event_delta_j":events,
                     "post_vof_divergence_s_inv":residual,"transfers":count})
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("nine_source_hashes",len(hashes),len(hashes)==9)
    accepted=bool(checks) and all(x["pass"] for x in checks)
    data={"phase":"P2","milestone":"vof_gravitational_potential_transport_ledger_partial",
          "partial_accepted":accepted,"phase_complete":False,
          "generated_utc":datetime.now(timezone.utc).isoformat(),
          "checks":checks,"native_assertions":assertions,"reference":refs,
          "source_sha256":hashes,"native_binary_sha256":sha(EXE),
          "ctest_log_sha256":sha(LOG),
          "limitations":"Discrete VOF gravitational potential transfer only. No kinetic energy closure, pressure work, dynamic FSI impulse/energy, independent measured CFD or Unreal Native-E2E."}
    OUT.write_text(json.dumps(data,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(f"P2_VOF_GRAVITY_PARTIAL={accepted} CHECKS={sum(x['pass'] for x in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for x in checks:
            if not x["pass"]:print("FAIL",x)
        raise SystemExit(1)
if __name__=="__main__":
    main()
