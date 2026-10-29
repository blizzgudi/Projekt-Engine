#!/usr/bin/env python3
"""P2 PARTIAL: projected dynamic-box MAC flux plus passive conservative VOF."""
from __future__ import annotations
import hashlib
import itertools
import json
import math
import re
import subprocess
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
LOG = RUNS / "p2_moving_box_projected_vof_full_ctest_20261029.txt"
EXE = CORE / "build" / "upc_p2_moving_box_projected_vof_tests.exe"
REPORT = RUNS / "p2_moving_box_projected_vof_partial.json"
SOURCES = [
    "CMakeLists.txt",
    "docs/PHASENPLAN.md",
    "docs/P2_MOVING_BOX_PROJECTED_VOF_PARTIAL.md",
    "include/upc/p2_moving_box_projected_vof3d.hpp",
    "include/upc/p2_moving_box_projected_reaction3d.hpp",
    "include/upc/p2_moving_box_mac_counterimpulse3d.hpp",
    "include/upc/p2_moving_box_dynamic_pressure3d.hpp",
    "include/upc/p2_moving_box_cut_cell_projection3d.hpp",
    "include/upc/p2_moving_cut_cell_scalar_transport3d.hpp",
    "tests/p2_moving_box_projected_vof3d_tests.cpp",
    "scripts/p2_moving_box_projected_vof_full_build.cmd",
    "scripts/p2_moving_box_projected_vof_acceptance.py",
]

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def independent_mass(n: int, uniform: bool) -> float:
    total = 0.0
    for z in range(n):
        for y in range(n):
            for x in range(n):
                overlap = 1.0
                for i in (x, y, z):
                    overlap *= max(0.0, min(i+1.0, 4.8)-max(float(i), 3.2))
                alpha = 1.0 - overlap
                fill = 1.0 if uniform else 0.2 + 0.01*x + 0.012*y + 0.008*z
                total += 1000.0*alpha*fill
    return total

def main() -> None:
    checks = []
    def check(name: str, passed: bool, detail=None) -> None:
        checks.append({"name":name,"pass":bool(passed),"detail":detail})
        if not passed:
            raise RuntimeError(f"FAIL {name}: {detail}")
    log = LOG.read_text(encoding="utf-8", errors="replace")
    check("complete 81/81 CTest",
          "100% tests passed, 0 tests failed out of 81" in log)
    check("new VOF native test in CTest",
          "physics_p2_moving_box_projected_vof_partial" in log)
    run = subprocess.run([str(EXE)],capture_output=True,text=True,
                         timeout=180,check=True)
    match = re.search(r"PASS:\s+(\d+) native projected moving-box VOF assertions",
                      run.stdout)
    native = int(match.group(1)) if match else 0
    check("native assertions >= 67000",native>=67000,native)
    pattern = (r"^VOF\s+(\d+)\s+(\d+)\s+(-?\d+)\s+([01])\s+"
               r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
               r"([\d.eE+-]+)\s+([\d.eE+-]+)$")
    rows = re.findall(pattern,run.stdout,re.MULTILINE)
    check("24 analytic water configurations",len(rows)==24,len(rows))
    seen = set()
    references = []
    for row in rows:
        n,axis,sign,uniform=map(int,row[:4])
        mass,residual,courant,impulse=map(float,row[4:])
        key=(n,axis,sign,uniform)
        check(f"unique {key}",key not in seen)
        seen.add(key)
        check(f"finite {key}",all(map(math.isfinite,
              (mass,residual,courant,impulse))))
        expected=independent_mass(n,bool(uniform))
        check(f"independent old-water mass {key}",
              abs(mass-expected)<2e-7,(mass,expected))
        check(f"conservative water mass {key}",abs(residual)<2e-7,residual)
        check(f"bounded donor CFL {key}",0.0<courant<1.0,courant)
        check(f"FSI proxy explicitly unclosed {key}",
              abs(impulse)>100.0,impulse)
        references.append({"grid":n,"axis":axis,"gradient_sign":sign,
                           "uniform":bool(uniform),"water_mass_kg":mass,
                           "water_mass_residual_kg":residual,
                           "max_donor_courant":courant,
                           "unclosed_impulse_proxy_Ns":impulse})
    check("full grid/axis/sign/fill coverage",
          seen==set(itertools.product((8,12),range(3),(-1,1),(0,1))))
    sequential = re.findall(
        r"^SEQUENTIAL\s+(\d+)\s+(\d+)\s+(-?\d+)\s+([\d.eE+-]+)$",
        run.stdout,re.MULTILINE)
    check("12 two-step moving-water cases",len(sequential)==12,len(sequential))
    seen_seq=set()
    for row in sequential:
        n,axis,sign=map(int,row[:3])
        mass=float(row[3])
        key=(n,axis,sign)
        check(f"unique sequential {key}",key not in seen_seq)
        seen_seq.add(key)
        check(f"sequential initial mass {key}",
              math.isfinite(mass) and
              abs(mass-independent_mass(n,False))<2e-7,mass)
    check("sequential coverage",
          seen_seq==set(itertools.product((8,12),range(3),(-1,1))))
    report = {
        "phase":"P2",
        "milestone":"moving_box_projected_passive_vof_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "ctest_suites":81,
        "native_assertions":native,
        "checks":checks,
        "analytic_reference":references,
        "sequential_reference":sequential,
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "ctest_log_sha256":sha(LOG),
        "unproven":[
            "passive water remap does not feed free-surface stresses back into pressure or solid force",
            "projection changes fluid momentum proxy by about 350 Ns; physical FSI momentum/energy not closed",
            "no validated water-air stress boundary, soil, heat, fracture or reaction coupling in this path",
            "no independent measured 3D CFD or Unreal 5.8 Native-E2E of this path",
        ],
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_PROJECTED_VOF_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={native} cases={len(rows)} "
          f"sequential={len(sequential)}")
    print(f"REPORT={REPORT}")

if __name__=="__main__":
    main()
