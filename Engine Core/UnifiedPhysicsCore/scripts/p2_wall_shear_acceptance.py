"""P2 fixed-geometry tangential wall shear: reproducible partial acceptance.

Native analytic discrete stencil + all 35 CTests; never mark P2 fully accepted.
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
OUT = LAB / "runs" / "p2_wall_shear_partial.json"
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
    "scripts/phase_gate.py",
    "scripts/p2_wall_shear_acceptance.py",
    "docs/P2_WALL_SHEAR_PARTIAL.md",
]


def run(args: list[str], timeout: int) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        args, cwd=CORE, text=True, capture_output=True, timeout=timeout,
        check=False,
    )


def main() -> None:
    binary = CORE / "build" / "upc_p2_viscosity_masked_tests.exe"
    if not binary.is_file() or not CTEST.is_file():
        raise SystemExit("Missing built native test or MSVC CTest")
    native = run([str(binary)], 35)
    suite = run([str(CTEST), "--test-dir", str(CORE / "build"),
                 "--output-on-failure"], 115)
    match = re.search(
        r"checks=(\d+).*?wall_shear=([0-9.eE+\-]+) "
        r"wall_shear_expected=([0-9.eE+\-]+)", native.stdout,
        re.DOTALL,
    )
    count, shear, expected = (0, float("nan"), float("nan"))
    if match:
        count = int(match.group(1))
        shear = float(match.group(2))
        expected = float(match.group(3))
    suite_match = re.search(
        r"100% tests passed, 0 tests failed out of (\d+)", suite.stdout
    )
    suites = int(suite_match.group(1)) if suite_match else 0
    checks = [
        {"name": "native_exit_zero", "value": native.returncode,
         "pass": native.returncode == 0},
        {"name": "native_discrete_shear_assertions", "value": count,
         "pass": count >= 40 and native.stdout.startswith("PASS P2 masked")},
        {"name": "independent_discrete_couette_reference",
         "value": {"actual": shear, "expected": expected},
         "pass": math.isfinite(shear) and math.isfinite(expected)
         and abs(shear - 0.0144) <= 1e-12
         and abs(shear - expected) <= 1e-12},
        {"name": "all_native_ctests_exit_zero", "value": suite.returncode,
         "pass": suite.returncode == 0},
        {"name": "native_ctest_count", "value": suites,
         "pass": suites == 35},
        {"name": "complete_source_coverage", "value": len(FILES),
         "pass": len(FILES) >= 9 and all((CORE / f).is_file() for f in FILES)},
    ]
    fingerprints = {
        name: hashlib.sha256((CORE / name).read_bytes()).hexdigest()
        for name in FILES if (CORE / name).is_file()
    }
    accepted = all(check["pass"] for check in checks)
    result = {
        "phase": "P2",
        "milestone": "prescribed_fixed_wall_tangential_shear_partial",
        "partial_accepted": accepted,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "native_stdout": native.stdout.strip(),
        "native_stderr": native.stderr[-1200:],
        "ctest_exit": suite.returncode,
        "ctest_tail": suite.stdout[-3500:],
        "checks": checks,
        "source_sha256": fingerprints,
        "limitations": (
            "Fixed geometry with prescribed tangential wall ghost velocity only; "
            "no normal wall motion, moving-body impulse, full stress-free surface, "
            "measured 3D CFD benchmark or Unreal native E2E for this option."
        ),
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, ensure_ascii=False, indent=2),
                   encoding="utf-8")
    print("P2_WALL_SHEAR_PARTIAL=" + str(accepted))
    print("P2_WALL_SHEAR_NATIVE_CHECKS=" + str(count))
    print("P2_WALL_SHEAR_CTEST=" + str(suites))
    print("P2_WALL_SHEAR_REPORT=" + str(OUT))
    if not accepted:
        raise SystemExit("P2 wall shear partial acceptance failed")


if __name__ == "__main__":
    main()
