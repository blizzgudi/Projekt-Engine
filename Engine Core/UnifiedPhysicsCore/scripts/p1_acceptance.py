"""Native, deterministic P1 2D shallow-water acceptance against independent theory.
Reads executable CSV results, verifies numerical invariants, and stamps exact
source hashes. This gate does NOT certify 3D physics or realistic turbulent fluids.
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

CORE = Path(__file__).resolve().parent.parent
ROOT = CORE.parents[1]
LAB = ROOT / "ENGINE TEST STUBE"
ARTIFACT = LAB / "runs" / "p1_acceptance.json"
FINGERPRINTS = [
    "include/upc/core.hpp",
    "tests/ritter_reference_tests.cpp",
    "tests/stoker_wet_dam_tests.cpp",
    "tests/shallow2d_extended_tests.cpp",
    "tests/core_tests.cpp",
    "docs/P1_2D_FLUID_ABNAHME.md",
    "CMakeLists.txt",
    "scripts/p1_acceptance.py",
]
def run(executable: str) -> str:
    binary = CORE / "build" / executable
    if not binary.is_file():
        raise RuntimeError(f"Missing native binary {binary}")
    p=subprocess.run([str(binary)],cwd=str(CORE),capture_output=True,
                     text=True,timeout=90,check=False)
    if p.returncode:
        raise RuntimeError(f"{executable} failed ({p.returncode}): {p.stderr[-1400:]}")
    if "PASS:" not in p.stdout:
        raise RuntimeError(f"{executable} missing PASS signal")
    return p.stdout.strip()

def rows_from(log: str, scenario: str) -> list[dict[str, float]]:
    lines=[line.strip() for line in log.splitlines()]
    head=next((i for i,line in enumerate(lines)
               if line.startswith("dx_m,depth_mean_abs_error_m,")),None)
    if head is None or head+3>=len(lines):
        raise RuntimeError(f"No 3-row reference CSV found for {scenario}")
    lines=lines[head:head+4]
    raw=list(csv.DictReader(io.StringIO("\n".join(lines))))
    if len(raw)!=3:
        raise RuntimeError(f"Expected three reference rows for {scenario}")
    result=[]
    for row in raw:
        cast={}
        for name,value in row.items():
            try:
                number=float(value)
            except (TypeError,ValueError) as exc:
                raise RuntimeError(f"Bad {scenario} CSV value {name}") from exc
            if not math.isfinite(number):
                raise RuntimeError(f"Non-finite {scenario} {name}")
            cast[name]=number
        result.append(cast)
    if any(abs(actual["dx_m"]-dx)>1e-12
           for actual,dx in zip(result,[0.1,0.05,0.025])):
        raise RuntimeError(f"Unexpected {scenario} grid resolution")
    return result

def main() -> None:
    checks=[]
    def check(name: str, condition: bool, value: object=None, tolerance: object=None) -> None:
        checks.append({"name":name,"pass":bool(condition),"value":value,"tolerance":tolerance})

    dry_stdout=run("upc_ritter_reference_tests.exe")
    wet_stdout=run("upc_stoker_wet_dam_tests.exe")
    ext_stdout=run("upc_shallow2d_extended_tests.exe")
    core_stdout=run("upc_tests.exe")
    dry=rows_from(dry_stdout,"ritter")
    wet=rows_from(wet_stdout,"stoker")
    for label,series in [("ritter",dry),("stoker",wet)]:
        for row in series:
            suffix=str(row["dx_m"])
            mass=row["volume_error_m3"]
            check(label+"_mass_"+suffix,mass<1e-9,mass,1e-9)
            check(label+"_depth_L1_"+suffix,row["depth_mean_abs_error_m"]<0.01,
                  row["depth_mean_abs_error_m"],0.01)
            check(label+"_momentum_L1_"+suffix,row["momentum_mean_abs_error_m2_s"]<0.025,
                  row["momentum_mean_abs_error_m2_s"],0.025)
        for column in ["depth_mean_abs_error_m","momentum_mean_abs_error_m2_s"]:
            values=[row[column] for row in series]
            check(label+"_convergence_"+column,
                  values[2]<values[1]<values[0],values,"strict decline")
    front=[r["front_abs_error_m"] for r in dry]
    isoline=[r["strong_front_iso_0p002_error_m"] for r in dry]
    check("ritter_fine_dry_front_under_8cm",front[2]<0.08,front[2],0.08)
    check("ritter_front_refinement_monotonic",front[2]<front[1]<front[0],front,"strict decline")
    check("ritter_isoline_error_disclosed",all(0<=x<0.36 for x in isoline),
          isoline,0.36)
    shock=[r["shock_position_error_m"] for r in wet]
    check("stoker_wet_shock_under_5cm",all(x<0.05 for x in shock),shock,0.05)
    check("stoker_shock_converges",shock[2]<shock[1]<shock[0],shock,"strict decline")

    try:
        extended_assertions=int(re.search(r"PASS: (\d+) assertions",ext_stdout).group(1))
        baseline_assertions=int(re.search(r"PASS: (\d+) physics assertions",core_stdout).group(1))
    except (AttributeError,ValueError) as exc:
        raise RuntimeError("Missing invariant assertion counts") from exc
    check("2d_extended_obstacles_pressure_cfl_symmetry",extended_assertions>=2100,
          extended_assertions,2100)
    check("baseline_material_dam_obstacle_regressions",baseline_assertions>=400,
          baseline_assertions,400)

    # P1 CPU scaling for current release build, measured in the native benchmark.
    process=subprocess.run([str(CORE/"build"/"upc_bench.exe")],
                           cwd=str(CORE),capture_output=True,text=True,timeout=90)
    if process.returncode:
        raise RuntimeError("CPU benchmark failed: "+process.stderr[-1000:])
    benchmark=list(csv.DictReader(io.StringIO(process.stdout)))
    shallow=[item for item in benchmark if item.get("experiment")=="shallow_water_2d"]
    check("p1_cpu_benchmark_available",len(shallow)==3,[r.get("cells") for r in shallow],3)
    for entry in shallow:
        t=float(entry["time_ms"])
        check("p1_cpu_benchmark_finite_"+entry["cells"],math.isfinite(t) and t>0,
              t,"positive finite milliseconds")

    hashes={}
    for name in FINGERPRINTS:
        path=CORE/name
        if not path.is_file():
            raise RuntimeError("Missing P1 source file "+name)
        hashes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    accepted=all(item["pass"] for item in checks)
    report={
        "schema_version":1,"phase":"P1","accepted":accepted,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "scope":"2D depth-averaged CPU shallow-water solver only, not volumetric CFD",
        "source_sha256":hashes,
        "ritter_dry_bed":dry,"stoker_wet_bed":wet,
        "invariants_stdout":ext_stdout,
        "core_stdout":core_stdout,
        "benchmark_2d":shallow,
        "checks":checks,
        "limitation":"1e-6m front threshold is numerical and detects tiny diffuse water; 0.002m contour has much larger deviation, documented. No subgrid interface model, viscosity or 3D dynamics.",
    }
    ARTIFACT.parent.mkdir(parents=True,exist_ok=True)
    ARTIFACT.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print("P1_ACCEPTED="+str(accepted))
    print("P1_CHECKS="+str(len(checks)))
    print("P1_RITTER_FINE_FRONT_ERROR_M="+str(front[-1]))
    print("P1_RITTER_FINE_L1_M="+str(dry[-1]["depth_mean_abs_error_m"]))
    print("P1_STOKER_FINE_SHOCK_ERROR_M="+str(shock[-1]))
    print("P1_STOKER_FINE_L1_M="+str(wet[-1]["depth_mean_abs_error_m"]))
    print("P1_REPORT="+str(ARTIFACT))
    if not accepted:
        failures=[item["name"] for item in checks if not item["pass"]]
        raise SystemExit("Failed P1 gates: "+",".join(failures))

if __name__=="__main__":
    main()
