"""P2: LIVE static-solid Couette boundary in coupled 3D MAC/VOF.

Independent analytic masked stencil + native live-flow/VOF comparison,
full MSVC CTest suite, conservative phase gate. Never full P2 acceptance.
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
PROJECT = CORE.parents[1]
OUT = PROJECT / "ENGINE TEST STUBE" / "runs" / "p2_live_wall_shear_partial.json"
CTEST = Path(
    r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7"
    r"\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
)
FILES = [
    "CMakeLists.txt",
    "include/upc/mac3d.hpp",
    "include/upc/viscous_mac3d.hpp",
    "include/upc/coupled_water3d.hpp",
    "tests/viscous_mac3d_masked_tests.cpp",
    "tests/coupled_viscous_water3d_tests.cpp",
    "scripts/p2_live_viscosity_acceptance.py",
    "scripts/p2_live_wall_shear_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_LIVE_WALL_SHEAR_PARTIAL.md",
]


def run(argv: list[str], timeout: int) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        argv, cwd=CORE, text=True, capture_output=True,
        timeout=timeout, check=False,
    )


def main() -> None:
    if not CTEST.is_file():
        raise SystemExit("MSVC CTest missing")
    live_exe = CORE / "build" / "upc_p2_live_viscous_water_tests.exe"
    masked_exe = CORE / "build" / "upc_p2_viscosity_masked_tests.exe"
    if not live_exe.is_file() or not masked_exe.is_file():
        raise SystemExit("Native P2 test binaries missing")

    live = run([str(live_exe)], 45)
    masked = run([str(masked_exe)], 45)
    ctest = run([str(CTEST), "--test-dir", str(CORE / "build"),
                 "--output-on-failure"], 120)
    gate = run(["C:\\Python314\\python.exe", str(CORE / "scripts" / "phase_gate.py")], 130)

    live_match = re.search(
        r"checks=(\d+).*?wall_shear_vof_difference=([0-9.eE+\-]+)"
        r" wall_shear_mass_kg=([0-9.eE+\-]+)", live.stdout,
    )
    masked_match = re.search(
        r"wall_shear=([0-9.eE+\-]+) "
        r"wall_shear_expected=([0-9.eE+\-]+)", masked.stdout,
    )
    suites_match = re.search(
        r"100% tests passed, 0 tests failed out of (\d+)", ctest.stdout
    )
    native_checks = int(live_match.group(1)) if live_match else 0
    live_diff = float(live_match.group(2)) if live_match else float("nan")
    live_mass = float(live_match.group(3)) if live_match else float("nan")
    wall_value = float(masked_match.group(1)) if masked_match else float("nan")
    wall_expected = float(masked_match.group(2)) if masked_match else float("nan")
    suites = int(suites_match.group(1)) if suites_match else 0

    checks = [
        {"name": "live_native_exit", "value": live.returncode,
         "pass": live.returncode == 0},
        {"name": "live_native_assertions", "value": native_checks,
         "pass": native_checks >= 20000 and live.stdout.startswith("PASS P2 live")},
        {"name": "wall_driven_vof_difference", "value": live_diff,
         "pass": math.isfinite(live_diff) and live_diff > 1e-9},
        {"name": "wall_driven_mass_conserved_kg", "value": live_mass,
         "pass": math.isfinite(live_mass) and abs(live_mass - 36.0) <= 1e-8},
        {"name": "masked_native_exit", "value": masked.returncode,
         "pass": masked.returncode == 0},
        {"name": "independent_discrete_couette_stencil",
         "value": {"actual": wall_value, "expected": wall_expected},
         "pass": math.isfinite(wall_value) and math.isfinite(wall_expected)
         and abs(wall_value - wall_expected) <= 1e-12
         and abs(wall_value - 0.0144) <= 1e-12},
        {"name": "all_ctest_suites", "value": suites,
         "pass": ctest.returncode == 0 and suites == 35},
        {"name": "conservative_phase_gate", "value": gate.stdout[-400:],
         "pass": gate.returncode == 0
         and "ALL_COMPLETE=False" in gate.stdout
         and "GATES_INCOMPLETE=P2,P3,P4,P5,P6,P7,P8" in gate.stdout},
        {"name": "source_coverage", "value": len(FILES),
         "pass": len(FILES) >= 10 and all((CORE / f).is_file() for f in FILES)},
    ]
    ok = all(c["pass"] for c in checks)
    fingerprints = {
        p: hashlib.sha256((CORE / p).read_bytes()).hexdigest()
        for p in FILES if (CORE / p).is_file()
    }
    result = {
        "phase": "P2",
        "milestone": "live_static_wall_couette_mac_vof_partial",
        "partial_accepted": ok,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "native_stdout": live.stdout.strip(),
        "masked_stdout": masked.stdout.strip(),
        "ctest_exit": ctest.returncode,
        "ctest_tail": ctest.stdout[-2000:],
        "phase_gate_exit": gate.returncode,
        "phase_gate_stdout": gate.stdout.strip(),
        "checks": checks,
        "source_sha256": fingerprints,
        "limitations": (
            "Static geometry only. Prescribed tangential wall speed is not "
            "moving-body momentum/impulse coupling. Free-surface Neumann "
            "proxy, no full viscous stress, measured 3D reference, global "
            "energy validation or new UE5.8 native E2E. P2-P8 remain PARTIAL."
        ),
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, ensure_ascii=False, indent=2),
                   encoding="utf-8")
    print("P2_LIVE_WALL_SHEAR_PARTIAL=" + str(ok))
    print("P2_LIVE_WALL_SHEAR_NATIVE_CHECKS=" + str(native_checks))
    print("P2_LIVE_WALL_SHEAR_VOF_DIFFERENCE=" + str(live_diff))
    print("P2_LIVE_WALL_SHEAR_CTEST=" + str(suites))
    print("P2_LIVE_WALL_SHEAR_REPORT=" + str(OUT))
    if not ok:
        raise SystemExit("P2 live wall shear partial acceptance failed")


if __name__ == "__main__":
    main()
