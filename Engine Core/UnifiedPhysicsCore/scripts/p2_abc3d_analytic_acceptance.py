"""P2 PARTIAL: independent 3D ABC exact Navier-Stokes benchmark.
A green analytical single-step result is NOT a full P2 or FSI acceptance.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
EXE = CORE / "build" / "upc_p2_abc3d_analytic_tests.exe"
NATIVE = RUNS / "p2_abc3d_native.txt"
COMPILE = RUNS / "p2_abc3d_compile.txt"
CTEST = RUNS / "p2_abc3d_ctest.txt"
OUT = RUNS / "p2_abc3d_analytic_partial.json"
SOURCES = [
    "include/upc/p2_viscous_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_periodic_mac_projection3d.hpp",
    "include/upc/p2_variable_mac_transport3d.hpp",
    "include/upc/p2_plic_adaptive_subcycle3d.hpp",
    "include/upc/p2_plic_unsplit_mac3d.hpp",
    "tests/p2_abc3d_analytic_tests.cpp",
    "scripts/p2_abc3d_analytic_build.cmd",
    "scripts/p2_abc3d_analytic_acceptance.py",
    "docs/P2_ABC3D_ANALYTIC_PARTIAL.md",
]
def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> None:
    native=NATIVE.read_text(encoding="utf-8")
    compiled=COMPILE.read_text(encoding="utf-8")
    ctest=CTEST.read_text(encoding="utf-8")
    checks=[]
    def check(name, value, condition):
        checks.append({"name":name,"value":value,"pass":bool(condition)})
    match=re.search(r"PASS ABC3D assertions=(\d+) phase_complete=false",native)
    assertions=int(match.group(1)) if match else 0
    check("native_assertions",assertions,assertions>=25000)
    check("native_no_failure","FAIL ABC3D" not in native,"FAIL ABC3D" not in native)
    check("compiled","error C" not in compiled and "fatal error" not in compiled.lower(),
          "error C" not in compiled and "fatal error" not in compiled.lower())
    check("ctest_106",ctest.splitlines()[-1] if ctest else "",
          "100% tests passed" in ctest and "0 tests failed out of 106" in ctest)
    rows=[]
    for line in native.splitlines():
        parts=line.split()
        if parts and parts[0]=="ABC":
            if len(parts)!=8: raise ValueError("Malformed ABC row")
            n=int(parts[1])
            rows.append(dict(n=n,rel_l2=float(parts[2]),
                max_abs_m_s=float(parts[3]),divergence_s_inv=float(parts[4]),
                mass_residual_m3=float(parts[5]),energy_ratio=float(parts[6]),
                analytic_energy_ratio=float(parts[7])))
    check("three_resolutions",[r["n"] for r in rows],
          [r["n"] for r in rows]==[8,12,16])
    analytic_ratio=math.exp(-2*.003*(2*math.pi)**2*.02)
    for row in rows:
        n=row["n"]
        check(f"abc_{n}_relative_l2",row["rel_l2"],
              math.isfinite(row["rel_l2"]) and row["rel_l2"]<.01)
        check(f"abc_{n}_max_abs",row["max_abs_m_s"],
              math.isfinite(row["max_abs_m_s"]) and row["max_abs_m_s"]<.001)
        check(f"abc_{n}_projected_divergence",row["divergence_s_inv"],
              math.isfinite(row["divergence_s_inv"]) and row["divergence_s_inv"]<1e-9)
        check(f"abc_{n}_water_mass",row["mass_residual_m3"],
              math.isfinite(row["mass_residual_m3"]) and abs(row["mass_residual_m3"])<1e-10)
        check(f"abc_{n}_energy_decay",row["energy_ratio"],
              0<row["energy_ratio"]<1)
        check(f"abc_{n}_independent_analytic_decay",row["analytic_energy_ratio"],
              math.isclose(row["analytic_energy_ratio"],analytic_ratio,rel_tol=1e-12))
        check(f"abc_{n}_energy_error_documented",
              row["energy_ratio"]-row["analytic_energy_ratio"],
              math.isfinite(row["energy_ratio"]))
    refinement=[line.split() for line in native.splitlines()
                if line.startswith("REFINEMENT ")]
    check("refinement_row",len(refinement),len(refinement)==1)
    if len(rows)==3 and len(refinement)==1 and len(refinement[0])==3:
        for idx in (0,1):
            actual=float(refinement[0][idx+1])
            expected=rows[idx]["rel_l2"]/rows[idx+1]["rel_l2"]
            check(f"refinement_{idx}",actual,
                  math.isclose(actual,expected,rel_tol=1e-12) and actual>1.2)
        p01=math.log(rows[0]["rel_l2"]/rows[1]["rel_l2"])/math.log(12/8)
        p12=math.log(rows[1]["rel_l2"]/rows[2]["rel_l2"])/math.log(16/12)
        check("observed_order_8_12",p01,p01>.5)
        check("observed_order_12_16",p12,p12>.5)
    hashes={name:sha(CORE/name) for name in SOURCES}
    check("ten_source_fingerprints",len(hashes),len(hashes)==10)
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2","milestone":"exact_3d_abc_viscous_navier_stokes_single_step",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":assertions,"checks":checks,"reference":rows,
        "source_sha256":hashes,"native_binary_sha256":sha(EXE),
        "native_log_sha256":sha(NATIVE),"compile_log_sha256":sha(COMPILE),
        "ctest_log_sha256":sha(CTEST),
        "limitations":[
            "ABC is a continuous exact solution, not independent measured CFD data",
            "One 0.02s split step only; first-order upwind causes excess numerical energy dissipation",
            "5-step 0.10s ABC/VOF slab trial failed with unresolved periodic PLIC interface on 12^3 grid; investigate separately",
            "Periodic constant-density single velocity, passive VOF; no stress-consistent two-phase free surface or dynamic FSI",
            "No UE5.8 Native E2E for this path; P2-P8 remain incomplete",
        ],
    }
    # Preserve the timestamp when the verified evidence is byte-for-byte
    # unchanged; repeated read-only gate checks must not dirty Git.
    previous=None
    if OUT.is_file():
        try:
            previous=json.loads(OUT.read_text(encoding="utf-8"))
        except (OSError,ValueError):
            previous=None
    if isinstance(previous,dict) and {
        k:v for k,v in previous.items() if k!="generated_utc"
    }=={k:v for k,v in report.items() if k!="generated_utc"}:
        report["generated_utc"]=previous["generated_utc"]
    else:
        OUT.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"P2_ABC3D_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} "
          f"ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:raise SystemExit(1)

if __name__=="__main__":
    main()
