"""P2 PARTIAL: verify native ABC5 endpoint-roundoff regression and hashes.
This must never mark P2 or subsequent phases complete.
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
EXE = CORE / "build" / "upc_p2_abc3d_multistep_probe.exe"
NATIVE = RUNS / "p2_abc3d_multistep_probe_native.txt"
COMPILE = RUNS / "p2_abc3d_multistep_probe_compile.txt"
CTEST = RUNS / "p2_abc3d_multistep_probe_ctest.txt"
OUT = RUNS / "p2_abc3d_multistep_roundoff_partial.json"
SOURCES = [
    "include/upc/p2_viscous_mac_plic_roundoff_sequence3d.hpp",
    "tests/p2_abc3d_multistep_probe.cpp",
    "scripts/p2_abc3d_multistep_probe_build.cmd",
    "scripts/p2_abc3d_multistep_roundoff_acceptance.py",
    "docs/P2_ABC3D_MULTISTEP_ROUNDOFF_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> None:
    native=NATIVE.read_text(encoding="utf-8")
    compile_log=COMPILE.read_text(encoding="utf-8")
    ctest=CTEST.read_text(encoding="utf-8")
    checks=[]
    def check(name: str, value, ok: bool) -> None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    check("compile_msvc",compile_log.strip()[-200:],
          "error C" not in compile_log and "fatal error" not in compile_log.lower()
          and "Microsoft (R)" not in compile_log)
    check("ctest_106",ctest.splitlines()[-1] if ctest else "",
          "100% tests passed" in ctest and "0 tests failed out of 106" in ctest)
    check("native_pass","PASS ABC5_ROUNDOFF" in native,
          "PASS ABC5_ROUNDOFF" in native and "FAIL ABC5_ROUNDOFF" not in native)
    match=re.search(r"PASS ABC5_ROUNDOFF assertions=(\d+) phase_complete=false",native)
    assertions=int(match.group(1)) if match else 0
    check("native_assertions",assertions,assertions>=6900)
    match=re.search(r"^ABC5 (.+)$",native,re.M)
    if not match:raise ValueError("Missing ABC5 native numeric record")
    metrics={}
    for field in match.group(1).split():
        k,v=field.split("=",1)
        metrics[k]=float(v)
    expected={"rel_l2","energy_ratio","analytic_energy_ratio","mass_m3",
              "mass_residual_m3","snap_cells","snap_abs_m3",
              "divergence","nearfull","flat"}
    check("complete_native_metrics",sorted(metrics),set(metrics)==expected)
    for key,value in metrics.items():
        check(f"finite_{key}",value,math.isfinite(value))
    check("reproduced_one_ulp",metrics["nearfull"],metrics["nearfull"]>=1)
    check("reproduced_flat_normals",metrics["flat"],metrics["flat"]>=1)
    check("five_step_abc_l2",metrics["rel_l2"],
          0<metrics["rel_l2"]<.01)
    check("water_volume",metrics["mass_m3"],
          abs(metrics["mass_m3"]-.25)<1e-10)
    check("mass_residual",metrics["mass_residual_m3"],
          abs(metrics["mass_residual_m3"])<1e-10)
    check("snapped_cells",metrics["snap_cells"],metrics["snap_cells"]>=1)
    check("bounded_absolute_snap_m3",metrics["snap_abs_m3"],
          0<metrics["snap_abs_m3"]<1e-12)
    check("pressure_divergence",metrics["divergence"],
          0<=metrics["divergence"]<1e-9)
    analytic=math.exp(-2*.003*(2*math.pi)**2*.10)
    check("independent_analytic_energy_ratio",metrics["analytic_energy_ratio"],
          math.isclose(metrics["analytic_energy_ratio"],analytic,rel_tol=1e-12))
    check("energy_dissipation_documented",metrics["energy_ratio"],
          0<metrics["energy_ratio"]<metrics["analytic_energy_ratio"])
    hashes={name:sha(CORE/name) for name in SOURCES}
    check("six_source_hashes",len(hashes),len(hashes)==6)
    report={
        "phase":"P2",
        "milestone":"abc3d_five_step_endpoint_roundoff_sanitation",
        "partial_accepted":all(x["pass"] for x in checks),
        "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":assertions,
        "metrics":metrics,
        "checks":checks,
        "source_sha256":hashes,
        "native_binary_sha256":sha(EXE),
        "native_log_sha256":sha(NATIVE),
        "compile_log_sha256":sha(COMPILE),
        "ctest_log_sha256":sha(CTEST),
        "limitations":[
            "Optional endpoint-only roundoff sanitation; unmodified core still rejects near-full degenerate normals",
            "No arbitrary PLIC normal fallback or generic geometric overlap repair",
            "Continuous analytic ABC is not measured external 3D CFD validation",
            "Extra numerical energy dissipation; no global physical two-phase energy conservation",
            "No moving two-way FSI, stress-consistent free surface, or UE5.8 Native-E2E of this adapter",
            "P2-P8 remain incomplete",
        ],
    }
    previous=None
    if OUT.is_file():
        try:previous=json.loads(OUT.read_text(encoding="utf-8"))
        except (OSError,ValueError):pass
    if isinstance(previous,dict) and {
        k:v for k,v in previous.items() if k!="generated_utc"
    }=={k:v for k,v in report.items() if k!="generated_utc"}:
        report["generated_utc"]=previous["generated_utc"]
    else:
        OUT.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    passed=sum(x["pass"] for x in checks)
    print(f"P2_ABC5_ROUNDOFF_PARTIAL={report['partial_accepted']} "
          f"CHECKS={passed}/{len(checks)} ASSERTIONS={assertions} "
          "PHASE_COMPLETE=False")
    if not report["partial_accepted"]:raise SystemExit(1)

if __name__=="__main__":
    main()
