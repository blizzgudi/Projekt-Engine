"""P2 partial: verify Galilean-translated 3D ABC Navier-Stokes reference.
Uses actual MSVC, native executable and complete CTest logs; no full-phase PASS.
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
NATIVE=RUNS/"p2_abc3d_galilean_native.txt"
COMPILE=RUNS/"p2_abc3d_galilean_compile.txt"
CTEST=RUNS/"p2_abc3d_galilean_ctest.txt"
EXE=CORE/"build"/"upc_p2_abc3d_galilean_tests.exe"
OUT=RUNS/"p2_abc3d_galilean_partial.json"
SOURCES=[
    "include/upc/p2_viscous_mac_plic_roundoff_schedule3d.hpp",
    "include/upc/p2_viscous_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_plic_strang_periodic3d.hpp",
    "tests/p2_abc3d_galilean_tests.cpp",
    "scripts/p2_abc3d_galilean_build.cmd",
    "scripts/p2_abc3d_galilean_acceptance.py",
    "docs/P2_ABC3D_GALILEAN_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]

def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main()->None:
    native=NATIVE.read_text(encoding="utf-8")
    build=COMPILE.read_text(encoding="utf-8")
    ctest=CTEST.read_text(encoding="utf-8")
    checks=[]
    def check(name:str,value,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    check("real_msvc_build",build.strip().splitlines()[-6:],
          "-- Configuring done" in build and "-- Generating done" in build
          and ("Linking CXX executable" in build or "ninja: no work to do." in build)
          and "error C" not in build and "fatal error" not in build.lower())
    check("full_ctest_109",ctest.strip().splitlines()[-4:],
          "100% tests passed, 0 tests failed out of 109" in ctest
          and "physics_p2_abc3d_galilean_partial" in ctest)
    match=re.search(r"PASS ABC_GALILEAN assertions=(\d+) phase_complete=false",native)
    assertions=int(match.group(1)) if match else 0
    check("native_success_and_assertions",assertions,
          assertions>=12000 and "FAIL ABC_GALILEAN" not in native)
    rows=[]
    for match in re.finditer(r"^ABC_GALILEAN n=(\d+) (.+)$",native,re.M):
        d={"n":int(match.group(1))}
        for token in match.group(2).split():
            k,v=token.split("=",1)
            d[k]=float(v)
        rows.append(d)
    expected_keys={"n","rel_l2","unshifted_l2","mean_error",
                   "energy_ratio","analytic_energy_ratio","volume",
                   "ledger","snap","divergence","viscous","plic"}
    check("three_3d_grids",[r["n"] for r in rows],
          [r["n"] for r in rows]==[8,10,12]
          and all(set(r)==expected_keys for r in rows))
    if len(rows)==3:
        check("finite_metrics",None,all(
              math.isfinite(v) for r in rows for v in r.values()))
        check("spatial_error_reduction",[r["rel_l2"] for r in rows],
              0<rows[2]["rel_l2"]<rows[1]["rel_l2"]<rows[0]["rel_l2"]<.02)
        check("actual_advected_analytic_reference",None,all(
              r["rel_l2"]<.45*r["unshifted_l2"] for r in rows))
        check("periodic_mean_momentum",None,all(
              r["mean_error"]<1e-12 for r in rows))
        exact_energy=math.exp(-2*.003*4*math.pi*math.pi*.02)
        check("independent_exact_energy",exact_energy,all(
              math.isclose(r["analytic_energy_ratio"],exact_energy,
                           rel_tol=1e-12) for r in rows))
        check("measured_numerical_dissipation",None,all(
              0<r["energy_ratio"]<exact_energy for r in rows)
              and rows[0]["energy_ratio"]<rows[1]["energy_ratio"]<
                  rows[2]["energy_ratio"])
        check("passive_vof_volume",None,all(
              abs(r["volume"]-.5)<1e-10 for r in rows))
        check("water_ledger_and_roundoff",None,all(
              abs(r["ledger"])<1e-10 and 0<=r["snap"]<1e-12
              for r in rows))
        check("pressure_projection",None,all(
              0<=r["divergence"]<1e-9 for r in rows))
        check("coupled_native_steps",None,all(
              r["viscous"]>=1 and r["plic"]>=1 for r in rows))
        ratio=re.search(r"^ABC_GALILEAN_REFINEMENT (.+)$",native,re.M)
        check("native_refinement_record",bool(ratio),ratio is not None)
    sources={name:sha(CORE/name) for name in SOURCES}
    check("source_sha256",len(sources),len(sources)==10)
    artifacts={str(p.relative_to(CORE.parents[1])):sha(p)
               for p in (NATIVE,COMPILE,CTEST,EXE)}
    check("native_artifact_sha256",len(artifacts),len(artifacts)==4)
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2","milestone":"galilean_abc3d_advection_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":assertions,"rows":rows,"checks":checks,
        "source_sha256":sources,"artifact_sha256":artifacts,
        "known_failed_probe":{
            "grid":"16^3","observed_error":"Unresolved periodic PLIC interface",
            "accepted":False,
            "note":"Observed in exploratory native runs with water slabs and all-water fill; excluded from passing grid set."
        },
        "limitations":[
            "constant-density periodic flow with passive VOF",
            "16^3 PLIC reconstruction probe failed and remains open",
            "no stress-consistent free surface or dynamic two-way FSI",
            "no independent measured CFD benchmark or full energy/momentum closure",
            "no new Unreal 5.8 native E2E"
        ],
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"P2_ABC_GALILEAN_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} ASSERTIONS={assertions}")
    print(f"EVIDENCE={OUT}")
    if not accepted:
        raise SystemExit(1)

if __name__=="__main__":
    main()
