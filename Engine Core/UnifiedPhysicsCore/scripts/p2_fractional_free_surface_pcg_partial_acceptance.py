"""Independent partial QA for geometric liquid-air MAC-PCG distance.

Verifies native manufactured 3D pressure, not a physical free-surface
reconstruction or a measured 3D CFD/FSI acceptance.
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
NATIVE=RUNS/"p2_fractional_surface_native.txt"
CTEST=RUNS/"p2_fractional_surface_ctest.txt"
BUILD=RUNS/"p2_fractional_surface_build.txt"
GATE=RUNS/"p2_fractional_surface_phase_gate.txt"
REPORT=RUNS/"p2_fractional_surface_partial.json"
PATTERN=re.compile(
    r"^FRACTIONAL_INTERFACE axis=(\d) n=(\d+) theta=([0-9.]+)"
    r" reverse=([01]) p_Pa=(-?[0-9.eE+-]+) iterations=(\d+)$"
)
SOURCES=[
    "CMakeLists.txt",
    "include/upc/mac3d.hpp",
    "tests/p2_fractional_free_surface_pcg_tests.cpp",
    "scripts/p2_fractional_free_surface_pcg_build.cmd",
    "scripts/p2_fractional_free_surface_pcg_partial_acceptance.py",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def audit()->dict:
    raw=NATIVE.read_text(encoding="utf-8")
    records=[]
    for line in raw.splitlines():
        match=PATTERN.fullmatch(line)
        if not match:
            continue
        axis,n,theta,reverse,pressure,iterations=match.groups()
        axis,n,reverse,iterations=int(axis),int(n),int(reverse),int(iterations)
        theta,pressure=float(theta),float(pressure)
        if not math.isfinite(pressure):
            raise ValueError("Nonfinite manufactured pressure")
        # Independent 1D momentum balance at p_air=0 and zero final
        # normal velocity: p_liquid=-rho*dx/dt * theta * 1 m/s.
        expected=-1000.0*1.0/0.1*theta
        if abs(pressure-expected)>1e-6:
            raise ValueError("Manufactured interface pressure mismatch")
        if iterations<1 or iterations>1000:
            raise ValueError("No valid native PCG iterations")
        records.append({
            "axis":axis,"grid_n":n,"theta":theta,"reverse":bool(reverse),
            "measured_pressure_pa":pressure,"analytic_pressure_pa":expected,
            "abs_error_pa":abs(pressure-expected),"pcg_iterations":iterations,
        })
    expected={(a,n,t,0) for a in range(3) for n in (5,9)
              for t in (.25,.5,.75)}
    expected|={(a,5,.25,1) for a in range(3)}
    observed={(r["axis"],r["grid_n"],r["theta"],int(r["reverse"]))
              for r in records}
    if len(records)!=21 or observed!=expected:
        raise ValueError("3D/two-grid/three-theta/mirrored coverage incomplete")
    match=re.search(
        r"^PASS P2_FRACTIONAL_FREE_SURFACE_PCG assertions=(\d+)"
        r" phase_complete=false$",raw,re.MULTILINE
    )
    if not match or int(match.group(1))<3000:
        raise ValueError("Native assertions or partial-only marker missing")
    ctext=CTEST.read_text(encoding="utf-8")
    ctest=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctext)
    if not ctest or int(ctest.group(1))<120:
        raise ValueError("Full native CTest regression failed or absent")
    build_bytes=BUILD.read_bytes()
    build_text=build_bytes.decode("utf-16" if build_bytes.startswith(b"\xff\xfe") else "utf-8")
    if "error" in build_text.lower():
        raise ValueError("Build error detected")
    gate=GATE.read_text(encoding="utf-8")
    if "P2" not in gate or "ALL_COMPLETE=False" not in gate:
        raise ValueError("Missing negative full phase-gate evidence")
    previous=RUNS/"p2_marin_wall_normal_partial.json"
    marin=json.loads(previous.read_text(encoding="utf-8"))
    metrics=marin["measured_metrics"]
    if len(metrics)!=8 or not all(
        metrics[f"P{i}_Pa"]["new_rmse_pa"]>500 for i in range(1,9)
    ):
        raise ValueError("Measured pressure nonconvergence not preserved")
    source_hashes={name:sha(CORE/name) for name in SOURCES}
    report={
        "phase":"P2",
        "milestone":"geometric_fractional_liquid_air_pcg_partial",
        "partial_accepted":True,"phase_complete":False,
        "stress_consistent_free_surface":False,
        "live_vof_interface_reconstruction":False,
        "two_phase_air_stress":False,
        "measured_marin_pressure_convergence":False,
        "independent_three_grid_6s_convergence":False,
        "dynamic_bidirectional_fsi":False,
        "physical_global_energy_impulse_closure":False,
        "native_assertions":int(match.group(1)),
        "ctest_suites":int(ctest.group(1)),
        "manufactured_3d_cases":records,
        "max_manufactured_pressure_abs_error_pa":max(
            r["abs_error_pa"] for r in records),
        "negative_marin_pressure_rmse_pa":{
            name:metrics[name]["new_rmse_pa"] for name in sorted(metrics)
        },
        "source_sha256":source_hashes,
        "binary_sha256":sha(CORE/"build/upc_p2_fractional_free_surface_pcg_tests.exe"),
        "native_log_sha256":sha(NATIVE),
        "ctest_log_sha256":sha(CTEST),
        "build_log_sha256":sha(BUILD),
        "phase_gate_log_sha256":sha(GATE),
        "previous_marin_pressure_report_sha256":sha(previous),
        "limitations":[
            "Interface distances must be supplied by external geometry; VOF fractions are not distances",
            "No automatic coupling of reconstructed interface to the live MARIN solver",
            "p_air=0 Dirichlet is not a stress-consistent two-phase boundary",
            "Original measured MARIN P1-P8 pressure remains outside acceptance",
            "No new 6-second three-grid run or native UE5.8 E2E",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print("P2_FRACTIONAL_FREE_SURFACE_PARTIAL=True P2_FULL_ACCEPTANCE=False")
    print("manufactured_cases",len(records),"native_assertions",int(match.group(1)),
          "ctest",int(ctest.group(1)))
    return report

if __name__=="__main__":
    audit()
