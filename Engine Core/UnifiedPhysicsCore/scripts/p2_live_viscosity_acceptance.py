"""P2 live masked-viscosity MAC/VOF partial acceptance; NOT full phase closure.

Runs the actual coupled C++ executable and the legacy inviscid regression,
validates quantitative invariants and fingerprints the exact sources.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parent.parent
LAB = CORE.parents[1] / "ENGINE TEST STUBE"
OUT = LAB / "runs" / "p2_live_viscosity_partial.json"
FILES = [
    "CMakeLists.txt",
    "include/upc/mac3d.hpp",
    "include/upc/viscous_mac3d.hpp",
    "include/upc/coupled_water3d.hpp",
    "tests/coupled_water3d_tests.cpp",
    "tests/coupled_viscous_water3d_tests.cpp",
    "scripts/p2_live_viscosity_acceptance.py",
    "docs/P2_LIVE_VISCOUS_MAC_PARTIAL.md",
]

def native(exe: str) -> str:
    binary = CORE / "build" / exe
    run = subprocess.run([str(binary)], cwd=CORE,
                         capture_output=True, text=True, timeout=75)
    if run.returncode:
        raise RuntimeError(f"{exe} failed with exit {run.returncode}: {run.stderr[-1800:]}")
    return run.stdout.strip()

def main() -> None:
    actual = native("upc_p2_live_viscous_water_tests.exe")
    legacy = native("upc_coupled_water3d_tests.exe")
    pattern = (r"PASS P2 live masked viscosity: checks=(\d+) "
               r"initial_mass_kg=([0-9.eE+-]+) "
               r"final_mass_kg=([0-9.eE+-]+) "
               r"total_visc_substeps=(\d+) "
               r"dissipated_face_energy_proxy=([0-9.eE+-]+) "
               r"max_vof_difference_vs_inviscid=([0-9.eE+-]+) "
               r"large_nu_substeps=(\d+) "
               r"wall_shear_vof_difference=([0-9.eE+-]+) "
               r"wall_shear_mass_kg=([0-9.eE+-]+)")
    match = re.fullmatch(pattern, actual)
    if not match or not legacy.startswith("PASS: "):
        raise RuntimeError("Structured native output missing")
    n, m0, m1, steps, dissipation, diff, adaptive, shear_diff, shear_mass = match.groups()
    n, steps, adaptive = int(n), int(steps), int(adaptive)
    m0, m1, dissipation, diff, shear_diff, shear_mass = map(
        float, (m0, m1, dissipation, diff, shear_diff, shear_mass))
    cases = [
        {"name": "native_assertions", "value": n, "threshold": ">=12000",
         "pass": n >= 12000},
        {"name": "live_mass_kg", "value": [m0, m1],
         "threshold": "abs(delta)<=1e-8",
         "pass": all(map(math.isfinite, (m0, m1))) and abs(m0-m1)<=1e-8},
        {"name": "masked_viscosity_called", "value": steps,
         "threshold": ">=16", "pass": steps >= 16},
        {"name": "diffusion_face_energy_dissipation", "value": dissipation,
         "threshold": ">1e-8",
         "pass": math.isfinite(dissipation) and dissipation > 1e-8},
        {"name": "actual_live_vof_changed_vs_inviscid", "value": diff,
         "threshold": ">1e-7",
         "pass": math.isfinite(diff) and diff > 1e-7},
        {"name": "adaptive_diffusion_substeps", "value": adaptive,
         "threshold": ">1", "pass": adaptive > 1},
        {"name": "live_prescribed_wall_shear_vof_effect",
         "value": shear_diff, "threshold": ">1e-9",
         "pass": math.isfinite(shear_diff) and shear_diff > 1e-9},
        {"name": "live_prescribed_wall_shear_mass_kg",
         "value": shear_mass, "threshold": "36 kg",
         "pass": math.isfinite(shear_mass) and abs(shear_mass-36.0)<=1e-8},
        {"name": "legacy_inviscid_native_regression", "value": legacy[:100],
         "threshold": "PASS", "pass": legacy.startswith("PASS: ")},
        {"name": "exact_source_coverage", "value": len(FILES),
         "threshold": ">=8", "pass": len(FILES)>=8 and all((CORE/f).is_file() for f in FILES)},
    ]
    ok = all(case["pass"] for case in cases)
    fingerprints = {p: hashlib.sha256((CORE / p).read_bytes()).hexdigest() for p in FILES}
    result = {
        "phase": "P2",
        "milestone": "live_masked_mac_vof_viscosity_partial",
        "partial_accepted": ok,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "native_stdout": actual,
        "legacy_native_stdout": legacy,
        "checks": cases,
        "source_sha256": fingerprints,
        "limitations": (
            "First-order semi-Lagrangian MAC+VOF with opt-in static-mask viscosity. "
            "Stationary solid no-slip and liquid-air Neumann are numerical proxies, "
            "not full stress-consistent interface physics. No moving-solid impulse "
            "closure, measured 3D CFD reference, global mechanical energy validation "
            "or Unreal Native-E2E for the viscous path."
        ),
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print("P2_LIVE_VISCOUS_PARTIAL=" + str(ok))
    print("P2_LIVE_VISCOUS_ASSERTIONS=" + str(n))
    print("P2_LIVE_VISCOUS_REPORT=" + str(OUT))
    if not ok:
        raise SystemExit("P2 live viscous integration partial acceptance failed")

if __name__ == "__main__":
    main()
