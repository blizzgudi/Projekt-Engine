#!/usr/bin/env python3
"""P2 PARTIAL: independently audit a closed-basin quasistatic hydrostatic model.
This does NOT validate dynamic MAC/VOF, fluid momentum or full FSI.
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
LOG = RUNS / "p2_closed_basin_hydrostatic_full_ctest_20261029.txt"
EXE = CORE / "build" / "upc_p2_closed_basin_hydrostatic_tests.exe"
REPORT = RUNS / "p2_closed_basin_hydrostatic_partial.json"
SOURCES = [
    "CMakeLists.txt", "docs/PHASENPLAN.md",
    "docs/P2_CLOSED_BASIN_HYDROSTATIC_PARTIAL.md",
    "include/upc/p2_closed_basin_hydrostatic3d.hpp",
    "include/upc/p2_moving_box_open_volume3d.hpp",
    "include/upc/rigid_pressure_response3d.hpp",
    "tests/p2_closed_basin_hydrostatic3d_tests.cpp",
    "scripts/p2_closed_basin_hydrostatic_acceptance.py",
    "scripts/p2_closed_basin_hydrostatic_full_build.cmd",
]
def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def potential(z: float, h: float, n: int, g: float) -> float:
    bottom = z - 0.8
    upper = min(h, z + 0.8)
    excluded = 2.56 * (upper*upper-bottom*bottom) if h > bottom else 0.0
    return 500.0 * g * (n*n*h*h-excluded)

def force(z: float, volume: float, n: int, g: float) -> float:
    area = n*n
    wet = min(1.6, max(0.0, (volume-area*(z-0.8))/(area-2.56)))
    return 1000.0*g*2.56*wet

def main() -> None:
    checks = []
    def check(name: str, passed: bool, detail=None) -> None:
        checks.append({"name": name, "pass": bool(passed), "detail": detail})
        if not passed:
            raise RuntimeError(f"{name} FAILED: {detail}")

    log = LOG.read_text(encoding="utf-8", errors="replace")
    check("full 85/85 CTest", "100% tests passed, 0 tests failed out of 85" in log)
    check("new suite present", "physics_p2_closed_basin_hydrostatic_partial" in log)
    proc = subprocess.run([str(EXE)], capture_output=True, text=True,
                          timeout=120, check=True)
    stdout = proc.stdout
    match = re.search(
        r"PASS:\s+(\d+) native closed-basin hydrostatic assertions", stdout)
    count = int(match.group(1)) if match else 0
    check("native assertions >= 50000", count >= 50000, count)
    pattern = (r"^BASIN\s+(\d+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
               r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)\s+"
               r"([\d.eE+-]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)$")
    rows = re.findall(pattern, stdout, re.MULTILINE)
    check("48 reference configurations", len(rows) == 48, len(rows))
    references = []
    seen = set()
    for raw in rows:
        n = int(raw[0])
        level, g, v0, dz, after_level, mean_force, defect, volume = map(float, raw[1:])
        key = (n, round(level, 9), round(g, 9), round(v0, 9))
        check(f"unique {key}", key not in seen)
        seen.add(key)
        initial_wet = min(1.6, max(0.0, level-3.2))
        initial_volume = n*n*level-2.56*initial_wet
        final_wet = min(1.6, max(0.0, after_level-(3.2+dz)))
        final_volume = n*n*after_level-2.56*final_wet
        check(f"initial inventory {key}",
              abs(initial_volume-volume) < 1e-9, [initial_volume, volume])
        check(f"final displaced inventory {key}",
              abs(final_volume-volume) < 1e-9, [final_volume, volume])
        # Independently integrate the force on the actual moving box.
        reference_force = sum(
            force(4+dz*(k+0.5)/8192,volume,n,g)
            for k in range(8192))/8192
        check(f"independent force quadrature {key}",
              abs(mean_force-reference_force) < 2e-4,
              [mean_force, reference_force])
        v1 = 2*dz/0.01-v0
        check(f"body momentum {key}",
              abs(10000*(v1-v0)-(mean_force-10000*g)*0.01) < 3e-6)
        energy = (5000*(v1*v1-v0*v0)+10000*g*dz+
                  potential(4+dz,after_level,n,g)-potential(4,level,n,g))
        check(f"independent water+body energy {key}",
              abs(energy) < 1e-7, energy)
        check(f"reported energy defect {key}", abs(defect) < 1e-7, defect)
        references.append({
            "grid": n, "initial_level_m": level, "final_level_m": after_level,
            "gravity_m_s2": g, "initial_velocity_m_s": v0,
            "dz_m": dz, "mean_buoyancy_N": mean_force,
            "water_volume_m3": volume,
            "equilibrium_energy_defect_J": defect,
        })
    expected = {
        (n, round(level,9), round(g,9), round(v,9))
        for n in (8,12)
        for level in (0.0,3.125,3.2,4.0,5.75,float(n))
        for g in (0.0,9.81)
        for v in (-0.4,0.4)
    }
    check("complete grid/level/gravity/velocity coverage", seen == expected)
    check("maximum equilibrium energy defect < 1e-7 J",
          max(abs(r["equilibrium_energy_defect_J"]) for r in references) < 1e-7)
    hashes = {name: sha(CORE / name) for name in SOURCES}
    report = {
        "phase": "P2",
        "milestone": "closed_basin_quasistatic_water_displacement_partial",
        "partial_accepted": True, "phase_complete": False,
        "ctest_suites": 85, "native_assertions": count,
        "checks": checks, "reference": references,
        "source_sha256": hashes, "ctest_log_sha256": sha(LOG),
        "limitations": [
            "quasistatic hydrostatic equilibrium; no resolved fluid inertia",
            "water fractions reconstructed, NOT advected by MAC/VOF",
            "buoyancy counterimpulse diagnostic only, NOT applied",
            "no dynamic 3D FSI momentum/energy validation or external CFD data",
            "no UE5.8 native E2E for this optional path",
        ],
    }
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_CLOSED_BASIN_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} cases={len(rows)}")
    print(f"REPORT={REPORT}")

if __name__ == "__main__":
    main()
