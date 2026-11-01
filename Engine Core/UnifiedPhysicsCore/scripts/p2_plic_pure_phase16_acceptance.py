"""P2 bounded native acceptance: 16^3 uniform pure-phase MAC/PLIC invariant.
Do not infer two-phase interface validity or full P2 acceptance.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
PREFIX="p2_plic_pure_phase16"
NATIVE=RUNS/(PREFIX+"_native.txt")
COMPILE=RUNS/(PREFIX+"_compile.txt")
CTEST=RUNS/(PREFIX+"_ctest.txt")
EXE=CORE/"build"/"upc_p2_plic_pure_phase16_tests.exe"
OUT=RUNS/(PREFIX+"_partial.json")
SOURCES=[
    "include/upc/p2_plic_unsplit_mac3d.hpp",
    "include/upc/p2_plic_adaptive_subcycle3d.hpp",
    "include/upc/p2_viscous_mac_plic_roundoff_schedule3d.hpp",
    "tests/p2_plic_pure_phase16_tests.cpp",
    "scripts/p2_plic_pure_phase16_build.cmd",
    "scripts/p2_plic_pure_phase16_acceptance.py",
    "docs/P2_PLIC_PURE_PHASE16_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main()->None:
    native=NATIVE.read_text(encoding="utf-8")
    compile_log=COMPILE.read_text(encoding="utf-8")
    ctest=CTEST.read_text(encoding="utf-8")
    checks=[]
    def check(name:str,value,passed:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(passed)})
    check("msvc_cmake_release_build",compile_log.strip().splitlines()[-6:],
          "-- Configuring done" in compile_log
          and "-- Generating done" in compile_log
          and ("Linking CXX executable" in compile_log
               or "ninja: no work to do." in compile_log)
          and "fatal error" not in compile_log.lower())
    check("complete_ctest_110",ctest.strip().splitlines()[-4:],
          "100% tests passed, 0 tests failed out of 110" in ctest
          and "physics_p2_plic_pure_phase16_partial" in ctest)
    match=re.search(r"PASS PURE16 assertions=(\d+) phase_complete=false",native)
    assertions=int(match.group(1)) if match else 0
    check("native_assertions_and_fail_closed_cases",assertions,
          assertions>=37 and "FAIL PURE16" not in native)
    rows=[]
    for m in re.finditer(r"^PURE16 fill=(\S+) (.+)$",native,re.M):
        row={"fill":float(m.group(1))}
        for token in m.group(2).split():
            k,v=token.split("=",1)
            row[k]=float(v)
        rows.append(row)
    check("both_16cubed_phases",[r["fill"] for r in rows],
          [r["fill"] for r in rows]==[1.0,0.0])
    if len(rows)==2:
        check("exact_uniform_phase_volume",[r["volume"] for r in rows],
              all(r["fill"]==r["volume"] for r in rows))
        check("no_artificial_endpoint_roundoff",[r["snap"] for r in rows],
              all(r["snap"]==0 for r in rows))
        check("exact_mass_ledger",[r["ledger"] for r in rows],
              all(r["ledger"]==0 for r in rows))
        check("actual_viscous_coupled_steps",[r["viscous"] for r in rows],
              all(r["viscous"]>=2 for r in rows))
        check("actual_plic_coupled_steps",[r["plic"] for r in rows],
              all(r["plic"]>=2 for r in rows))
        check("projected_divergence",[r["divergence"] for r in rows],
              all(math.isfinite(r["divergence"])
                  and 0<=r["divergence"]<1e-9 for r in rows))
    sources={name:sha(CORE/name) for name in SOURCES}
    check("source_sha256",len(sources),len(sources)==len(SOURCES))
    artifacts={str(p.relative_to(CORE.parents[1])):sha(p)
               for p in (NATIVE,COMPILE,CTEST,EXE)}
    check("binary_and_log_sha256",len(artifacts),len(artifacts)==4)
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2", "milestone":"pure_phase_16cubed_invariance_partial",
        "partial_accepted":accepted, "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":assertions,"rows":rows,"checks":checks,
        "source_sha256":sources,"artifact_sha256":artifacts,
        "limitations":[
            "16^3 water-air slab with a real interface still unvalidated",
            "pure-phase shortcut only, after full divergence/CFL checks",
            "no measured CFD reference, stress-consistent interface or dynamic FSI",
            "no global physical energy/momentum closure or new UE5.8 Native-E2E",
        ],
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"P2_PURE_PHASE16_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} ASSERTIONS={assertions}")
    print(f"EVIDENCE={OUT}")
    if not accepted:
        raise SystemExit(1)
if __name__=="__main__":
    main()