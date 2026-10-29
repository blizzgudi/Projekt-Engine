#!/usr/bin/env python3
"""P2 PARTIAL: independent audit of closed-basin 3D kinematic MAC fluxes.
NOT a dynamic Navier-Stokes, VOF-advection or FSI acceptance.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
LOG = RUNS / "p2_closed_basin_kinematic_flux_full_ctest_20261029.txt"
EXE = CORE / "build" / "upc_p2_closed_basin_kinematic_flux_tests.exe"
REPORT = RUNS / "p2_closed_basin_kinematic_flux_partial.json"
SOURCES = [
    "CMakeLists.txt", "docs/PHASENPLAN.md",
    "docs/P2_CLOSED_BASIN_KINEMATIC_FLUX_PARTIAL.md",
    "include/upc/p2_closed_basin_kinematic_flux3d.hpp",
    "include/upc/p2_closed_basin_hydrostatic3d.hpp",
    "include/upc/p2_moving_box_mac_apertures3d.hpp",
    "include/upc/p2_moving_box_open_volume3d.hpp",
    "tests/p2_closed_basin_kinematic_flux3d_tests.cpp",
    "scripts/p2_closed_basin_kinematic_flux_acceptance.py",
    "scripts/p2_closed_basin_kinematic_flux_full_build.cmd",
]
def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main() -> None:
    checks = []
    def check(name: str, ok: bool, value=None) -> None:
        checks.append({"name": name, "pass": bool(ok), "detail": value})
        if not ok:
            raise RuntimeError(f"{name}: {value}")
    log = LOG.read_text(encoding="utf-8", errors="replace")
    check("full 86/86 CTest", "100% tests passed, 0 tests failed out of 86" in log)
    check("new CTest suite", "physics_p2_closed_basin_kinematic_flux_partial" in log)
    result = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                            text=True, check=True, timeout=120)
    output = result.stdout
    match = re.search(
        r"PASS:\s+(\d+) native closed-basin 3D kinematic flux assertions",
        output)
    count = int(match.group(1)) if match else 0
    check("native independent 3D assertions >= 150000", count >= 150000, count)
    pattern = (r"^FLUX\s+(\d+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
               r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
               r"(\d+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)$")
    rows = re.findall(pattern, output, re.MULTILINE)
    check("32 reference configurations", len(rows) == 32, len(rows))
    seen=set()
    references=[]
    for row in rows:
        n=int(row[0])
        level,g,v,continuity,cg,iterations,maxflux,netchange = (
            float(row[1]),float(row[2]),float(row[3]),float(row[4]),
            float(row[5]),int(row[6]),float(row[7]),float(row[8]))
        key=(n,round(level,9),round(g,9),round(v,9))
        check(f"unique configuration {key}", key not in seen)
        seen.add(key)
        check(f"finite numerical results {key}",
              all(math.isfinite(x) for x in (
                  continuity,cg,maxflux,netchange)))
        check(f"3D finite-volume continuity {key}", 0<=continuity<1e-8,
              continuity)
        check(f"conjugate-gradient convergence {key}", 0<=cg<1e-8, cg)
        check(f"CG bounded iterations {key}", 0<=iterations<20000, iterations)
        check(f"global water conservation {key}", abs(netchange)<1e-9,
              netchange)
        if level==0:
            check(f"dry basin zero face volume {key}", maxflux==0,maxflux)
        if level in (4.0,5.75):
            check(f"wet basin actual water flux {key}", maxflux>1e-10,maxflux)
        references.append({
            "grid":n,"initial_water_level_m":level,
            "gravity_m_s2":g,"body_velocity_m_s":v,
            "max_cell_continuity_residual_m3":continuity,
            "cg_residual_l2_m3":cg,"cg_iterations":iterations,
            "max_integrated_face_flux_m3":maxflux,
            "total_water_change_m3":netchange,
        })
    expected={(n,round(h,9),round(g,9),round(v,9))
              for n in (8,12) for h in (0.0,3.2,4.0,5.75)
              for g in (0.0,9.81) for v in (-0.4,0.4)}
    check("complete independent parameter matrix", seen==expected)
    hashes={name:sha(CORE/name) for name in SOURCES}
    report={
        "phase":"P2",
        "milestone":"closed_basin_3d_kinematic_face_flux_partial",
        "partial_accepted":True,"phase_complete":False,
        "ctest_suites":86,"native_assertions":count,
        "checks":checks,"reference":references,
        "source_sha256":hashes,"ctest_log_sha256":sha(LOG),
        "limitations":[
            "flux reconstruction between prescribed hydrostatic states",
            "not a time-advanced MAC/VOF Navier-Stokes solution",
            "no applied rigid-fluid counterimpulse or fluid inertia",
            "no global physical FSI energy/momentum conservation",
            "no external 3D CFD measurements or UE5.8 Native-E2E",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_CLOSED_BASIN_KINEMATIC_FLUX_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} cases={len(rows)}")
    print(f"REPORT={REPORT}")

if __name__=="__main__":
    main()
