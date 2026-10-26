"""Reproducible P5 *partial* contact evidence. Never marks P5 complete."""
from __future__ import annotations

import hashlib
import json
import re
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parent.parent
PROJECT = CORE.parents[1]
REPORT = PROJECT / "ENGINE TEST STUBE" / "runs" / "p5_contact_partial.json"
CTEST = Path(
    r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
    r"\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
)
SOURCES = [
    "CMakeLists.txt",
    "include/upc/solid.hpp",
    "include/upc/solid_contact3d.hpp",
    "tests/solid_contact3d_tests.cpp",
    "scripts/p5_contact_partial.py",
    "docs/P5_PLANE_CONTACT_PARTIAL.md",
]


def run(args: list[str], timeout: int) -> tuple[str, float]:
    started = time.perf_counter()
    process = subprocess.run(
        args, cwd=CORE, capture_output=True, text=True, timeout=timeout, check=False
    )
    duration_ms = round((time.perf_counter() - started) * 1000, 3)
    if process.returncode != 0:
        raise RuntimeError(
            f"Native validation failed ({process.returncode}): "
            f"{process.stdout[-1500:]} {process.stderr[-1500:]}"
        )
    return process.stdout, duration_ms


def main() -> None:
    if not CTEST.is_file():
        raise RuntimeError("MSVC CTest executable not available")
    ctest, suite_ms = run(
        [str(CTEST), "--test-dir", str(CORE / "build"), "--output-on-failure"], 120
    )
    match = re.search(r"100% tests passed, 0 tests failed out of (\d+)", ctest)
    if not match or int(match.group(1)) < 19:
        raise RuntimeError("Complete regression suite did not pass")
    native, native_ms = run([str(CORE / "build" / "upc_solid_contact3d_tests.exe")], 30)
    count_match = re.search(r"PASS: P5 bounded unilateral plane contact.*checks=(\d+)", native)
    if not count_match or int(count_match.group(1)) < 10984:
        raise RuntimeError("P5 property/contact assertions missing")
    hashes = {p: hashlib.sha256((CORE / p).read_bytes()).hexdigest() for p in SOURCES}
    report = {
        "phase": "P5",
        "partial_accepted": True,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "Single immovable infinite plane; discrete point-mass projection, restitution and friction",
        "native_checks": int(count_match.group(1)),
        "ctest_suites": int(match.group(1)),
        "ctest_ms": suite_ms,
        "native_ms": native_ms,
        "native_stdout": native.strip(),
        "ctest_summary": match.group(0),
        "source_sha256": hashes,
        "unverified": [
            "volumetric solid contact",
            "multi-contact stacking",
            "self-collision",
            "granular constitutive physics",
            "fracture/contact coupled quantitative references",
            "continuum FEM/MPM and production stability",
        ],
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"P5_PARTIAL_ACCEPTED=True")
    print(f"P5_PHASE_COMPLETE=False")
    print(f"P5_NATIVE_CHECKS={report['native_checks']}")
    print(f"P5_CTEST_SUITES={report['ctest_suites']}")
    print(f"P5_REPORT={REPORT}")


if __name__ == "__main__":
    main()
