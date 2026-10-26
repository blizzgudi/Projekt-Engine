"""P2 acceptance gate: execute native reference cases, save exact sources + measurements.
Requires built Release upc_p2_reference and upc_coupled_water3d_tests binaries.
This is a CPU research/MVP gate, not a validated full Navier-Stokes/AAA/GPU fluid engine.
"""
from __future__ import annotations
import csv
import hashlib
import io
import json
import math
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
ROOT=CORE.parents[1]
ART=ROOT / "ENGINE TEST STUBE" / "runs" / "p2_acceptance.json"
BINARY=CORE/"build"/"upc_p2_reference.exe"
TEST=CORE/"build"/"upc_coupled_water3d_tests.exe"
RUNTIME=CORE/"build"/"upc_unified_runtime_tests.exe"
WATCH=[
    "include/upc/core.hpp",
    "include/upc/coupled_water3d.hpp",
    "include/upc/fluid3d_module.hpp",
    "include/upc/mac3d.hpp",
    "tests/coupled_water3d_tests.cpp",
    "tests/unified_runtime_tests.cpp",
    "docs/P2_3D_FLUID_ABNAHME.md",
    "benchmarks/p2_reference.cpp",
    "CMakeLists.txt",
]
def hashes():
    return {name:hashlib.sha256((CORE/name).read_bytes()).hexdigest() for name in WATCH}
def execute(binpath):
    r=subprocess.run([str(binpath)],capture_output=True,text=True,timeout=100,cwd=str(CORE))
    if r.returncode != 0:
        raise RuntimeError(f"{binpath.name} exited {r.returncode}: "+r.stderr[-2000:])
    return r
def validate():
    ref=execute(BINARY)
    test=execute(TEST)
    runtime=execute(RUNTIME)
    rows=list(csv.DictReader(io.StringIO(ref.stdout)))
    if len(rows)!=6:
        raise RuntimeError(f"Expected 6 reference rows, got {len(rows)}")
    def num(row,key):
        v=float(row[key])
        if not math.isfinite(v):raise RuntimeError(f"Nonfinite {key}")
        return v
    dams=[r for r in rows if r["scenario"]=="dam_break"]
    ponds=[r for r in rows if r["scenario"]=="hydrostatic"]
    if [int(r["nx"]) for r in dams]!=[8,16,32] or \
       [int(r["nx"]) for r in ponds]!=[8,16,24]:
        raise RuntimeError("Unexpected resolution grid sequence")
    tests=[]
    def confirm(name,passed,value=None,limit=None):
        tests.append({"name":name,"pass":bool(passed),"value":value,"limit":limit})
    for row in rows:
        volume_error=abs(num(row,"initial_m3")-num(row,"final_m3"))
        confirm("mass_"+row["scenario"]+"_"+row["nx"],volume_error<1e-8,volume_error,1e-8)
        confirm("resident_budget_"+row["scenario"]+"_"+row["nx"],
                0<int(row["resident_bytes"])<2_000_000,int(row["resident_bytes"]),2_000_000)
        confirm("finite_runtime_"+row["scenario"]+"_"+row["nx"],
                num(row,"elapsed_ms")>0,num(row,"elapsed_ms"),None)
    for row in ponds:
        relative=abs(num(row,"pressure_pa")-num(row,"reference_pa"))/num(row,"reference_pa")
        confirm("hydrostatic_reference_"+row["nx"],relative<0.01,relative,0.01)
        confirm("hydrostatic_divergence_"+row["nx"],
                num(row,"max_div_1_s")<0.05,num(row,"max_div_1_s"),0.05)
    centres=[num(x,"centroid_x_m") for x in dams]
    coarse_diff=abs(centres[0]-centres[1])
    fine_diff=abs(centres[1]-centres[2])
    confirm("3d_dam_break_moves",[x>0.2 for x in centres]==[True]*3,centres,[0.2]*3)
    confirm("grid_centroid_coarse_to_medium",coarse_diff<0.02,coarse_diff,0.02)
    confirm("grid_centroid_medium_to_fine",fine_diff<0.015,fine_diff,0.015)
    confirm("centroid_resolution_difference_decreases",fine_diff<coarse_diff,
            fine_diff/coarse_diff if coarse_diff else None,1.0)
    confirm("3d_solid_collision_test",test.stdout.startswith("PASS:"),test.stdout.strip(),None)
    confirm("unified_material_runtime",runtime.stdout.startswith("PASS:"),runtime.stdout.strip(),None)
    ok=all(entry["pass"] for entry in tests)
    report={
       "schema_version":1,
       "generated_utc":datetime.now(timezone.utc).isoformat(),
       "phase":"P2",
       "scope":"CPU MAC pressure + bounded VOF 3D free-surface transport prototype; no FLIP/APIC/MPM, no 3D FSI",
       "accepted":ok,
       "source_sha256":hashes(),
       "reference":rows,
       "checks":tests,
       "native_3d_tests_stdout":test.stdout.strip(),
       "native_unified_runtime_stdout":runtime.stdout.strip(),
       "comparison_note":"Centroid differences across grids are a numerical self-convergence sanity check, not validation against measured dam-break data",
       "reference_stderr":ref.stderr[-1000:],
       "last_runtime_note":"CPU timings are single-run hardware-specific observations, not interactive frame-rate evidence"
    }
    ART.parent.mkdir(exist_ok=True,parents=True)
    ART.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print("P2_ACCEPTED="+str(ok))
    print("P2_CHECKS="+str(len(tests)))
    print("P2_CENTROID="+",".join(f"{x:.6f}" for x in centres))
    print("P2_FINE_GRID_MS="+dams[-1]["elapsed_ms"])
    print("P2_FINE_GRID_RESIDENT_BYTES="+dams[-1]["resident_bytes"])
    print("P2_REPORT="+str(ART))
    if not ok:raise SystemExit(1)
if __name__=="__main__":validate()
