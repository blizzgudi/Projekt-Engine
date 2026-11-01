"""Independent partial QA for axis-aligned VOF -> fractional MAC-PCG.
This does NOT validate a general moving interface or MARIN measurements.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
NATIVE=RUNS/"p2_axis_vof_pcg_native.txt"
CTEST=RUNS/"p2_axis_vof_pcg_ctest.txt"
BUILD=RUNS/"p2_axis_vof_pcg_full_build.txt"
GATE=RUNS/"p2_axis_vof_pcg_phase_gate.txt"
REPORT=RUNS/"p2_axis_vof_pcg_partial.json"
SOURCE_NAMES=[
    "CMakeLists.txt",
    "include/upc/mac3d.hpp",
    "include/upc/p2_axis_vof_pcg_geometry3d.hpp",
    "include/upc/p2_vof_normal_reconstruction3d.hpp",
    "tests/p2_axis_vof_pcg_geometry3d_tests.cpp",
    "scripts/p2_axis_vof_pcg_partial_acceptance.py",
]
PATTERN=re.compile(
    r"^VOF_PCG axis=(\d) n=(\d+) reverse=([01]) wet_partial=([01]) "
    r"theta=([0-9.]+) pressure_pa=(-?[0-9.eE+\-]+) iterations=(\d+)$"
)
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def read_log(path:Path)->str:
    data=path.read_bytes()
    return data.decode("utf-16" if data.startswith(b"\xff\xfe") else "utf-8-sig").replace("\r\n","\n")

def parse_native(raw:str)->tuple[list[dict],int]:
    if "phase_complete=true" in raw:
        raise ValueError("Full P2 completion marker forbidden")
    matches=[]
    for line in raw.splitlines():
        m=PATTERN.fullmatch(line)
        if not m:
            if line.startswith("VOF_PCG axis="):
                raise ValueError("Malformed or nonfinite native case")
            continue
        axis,n,rev,wp,theta,pressure,it=m.groups()
        axis,n,rev,wp,it=map(int,(axis,n,rev,wp,it))
        theta,pressure=float(theta),float(pressure)
        expected_theta=.25 if wp else .75
        expected_pressure=-1000.0/0.1*expected_theta
        if axis not in (0,1,2) or n not in (5,9) or rev not in (0,1):
            raise ValueError("Unexpected 3D grid or orientation")
        if not math.isfinite(pressure) or abs(pressure-expected_pressure)>1e-6:
            raise ValueError("Independent momentum pressure reference mismatch")
        if abs(theta-expected_theta)>1e-14 or it<1 or it>1000:
            raise ValueError("VOF geometric distance or PCG iteration invalid")
        matches.append(dict(axis=axis,n=n,reverse=bool(rev),
                            partial_wet=bool(wp),theta=theta,
                            pressure_pa=pressure,
                            expected_pressure_pa=expected_pressure,
                            pcg_iterations=it))
    expected={(a,n,r,w) for a in range(3) for n in (5,9)
              for r in (0,1) for w in (0,1)}
    observed={(m["axis"],m["n"],int(m["reverse"]),int(m["partial_wet"]))
              for m in matches}
    if len(matches)!=24 or observed!=expected:
        raise ValueError("Missing/duplicate native 3D VOF cases")
    if "VOF_PCG_RESET first_pa=-2500 second_pa=-5000" not in raw:
        raise ValueError("Stale geometry reset not verified")
    a=re.search(r"^PASS P2_AXIS_VOF_PCG assertions=(\d+) phase_complete=false$",
                raw,re.MULTILINE)
    if not a or int(a.group(1))<5100:
        raise ValueError("Native negative/positive assertions missing")
    return matches,int(a.group(1))

def audit()->dict:
    cases,assertions=parse_native(read_log(NATIVE))
    ctext=read_log(CTEST)
    c=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctext)
    if not c or int(c.group(1))<121:
        raise ValueError("Complete native CTest regression missing")
    build=read_log(BUILD)
    if "Linking CXX executable upc_p2_axis_vof_pcg_geometry_tests.exe" not in build:
        raise ValueError("Native MSVC link evidence missing")
    gate=read_log(GATE)
    if "P2" not in gate or "ALL_COMPLETE=False" not in gate:
        raise ValueError("P2 must remain incomplete")
    prior=RUNS/"p2_marin_wall_normal_partial.json"
    old=json.loads(prior.read_text(encoding="utf-8"))
    metrics=old["measured_metrics"]
    if len(metrics)!=8 or not all(metrics[f"P{i}_Pa"]["new_rmse_pa"]>500
                                    for i in range(1,9)):
        raise ValueError("Negative MARIN pressure evidence not preserved")
    hashes={name:sha(CORE/name) for name in SOURCE_NAMES}
    artifacts={
        str(p.relative_to(CORE.parents[1])).replace("\\","/"):sha(p)
        for p in (NATIVE,CTEST,BUILD,GATE,prior,
                  CORE/"build"/"upc_p2_axis_vof_pcg_geometry_tests.exe")
    }
    report=dict(
        phase="P2",milestone="axis_vof_to_fractional_pcg_partial",
        partial_accepted=True,phase_complete=False,
        stress_consistent_free_surface=False,
        general_oblique_plic=False,live_moving_vof_pcg=False,
        measured_marin_pressure_convergence=False,
        independent_three_grid_six_second_convergence=False,
        dynamic_bidirectional_fsi=False,
        global_physical_energy_impulse_closure=False,
        native_assertions=assertions,ctest_suites=int(c.group(1)),
        manufactured_3d_cases=cases,
        reset_and_invalid_geometry_tested=True,
        negative_marin_pressure_rmse_pa={
            f"P{i}":metrics[f"P{i}_Pa"]["new_rmse_pa"] for i in range(1,9)},
        previous_marin_report_sha256=sha(prior),
        source_sha256=hashes,artifact_sha256=artifacts,
        generated_utc=datetime.now(timezone.utc).isoformat())
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    return report

def mutations()->list[str]:
    raw=read_log(NATIVE)
    variations={
        "pressure":raw.replace("pressure_pa=-7500","pressure_pa=-7100",1),
        "missing_axis":raw.replace(
            next(line for line in raw.splitlines() if line.startswith("VOF_PCG axis=2 ")),
            "",1),
        "false_phase":raw.replace("phase_complete=false","phase_complete=true"),
        "nan":raw.replace("pressure_pa=-2500","pressure_pa=nan",1),
    }
    passed=[]
    for name,mutated in variations.items():
        try:parse_native(mutated)
        except ValueError:passed.append(name)
        else:raise ValueError(f"Mutation not detected: {name}")
    return passed

if __name__=="__main__":
    result=audit()
    passed=mutations()
    (RUNS/"p2_axis_vof_pcg_mutations.txt").write_text(
        "\n".join(f"REJECTED {x}" for x in passed)+"\n",encoding="utf-8")
    print(f"PARTIAL_ACCEPTED=True cases={len(result['manufactured_3d_cases'])} "
          f"assertions={result['native_assertions']} ctest={result['ctest_suites']}")
    print(f"MUTATIONS_REJECTED={','.join(passed)} P2_COMPLETE=False")
    print(f"REPORT={REPORT}")
