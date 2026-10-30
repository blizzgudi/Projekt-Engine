"""P2 partial: independent fractional 3D pressure-force/work verification."""
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
RUNS = LAB / "runs"
LOG = RUNS / "p2_fractional_pressure_work_full_ctest_20261030.txt"
OUT = RUNS / "p2_fractional_pressure_work_partial.json"
CTEST = Path(
    r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7"
    r"\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
)
NATIVE = CORE / "build" / "upc_p2_fractional_pressure_work_tests.exe"
FILES = [
    "include/upc/p2_moving_box_pressure_traction3d.hpp",
    "include/upc/p2_moving_box_open_volume3d.hpp",
    "tests/p2_fractional_pressure_work3d_tests.cpp",
    "scripts/p2_fractional_pressure_work_full_build.cmd",
    "scripts/p2_fractional_pressure_work_acceptance.py",
    "docs/P2_FRACTIONAL_PRESSURE_WORK_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
]

def main() -> None:
    if not NATIVE.is_file() or not CTEST.is_file():
        raise SystemExit("Native executable or CTest missing; build first")
    native = subprocess.run([str(NATIVE)], cwd=CORE, capture_output=True,
                            text=True, timeout=60, check=False)
    ctest = subprocess.run(
        [str(CTEST), "--test-dir", str(CORE / "build"), "-C", "Release",
         "--parallel", "4", "--output-on-failure"],
        cwd=CORE, capture_output=True, text=True, timeout=180, check=False
    )
    RUNS.mkdir(parents=True, exist_ok=True)
    LOG.write_text(ctest.stdout + ctest.stderr, encoding="utf-8")
    checks = []
    def add(name: str, actual: object, passed: bool) -> None:
        checks.append({"name": name, "value": actual, "pass": bool(passed)})
    add("native_exit", native.returncode, native.returncode == 0)
    count_match = re.search(r"PASS: (\d+) native fractional 3D", native.stdout)
    count = int(count_match.group(1)) if count_match else 0
    add("native_assertions", count, count >= 355)
    rows = re.findall(
        r"CASE n=(\d+) dx=([0-9.eE+\-]+) sign=(-?\d+) "
        r"fx=([0-9.eE+\-]+) fy=([0-9.eE+\-]+) "
        r"fz=([0-9.eE+\-]+) work=([0-9.eE+\-]+) "
        r"residual=([0-9.eE+\-]+)", native.stdout
    )
    add("four_diagonal_3d_cases", len(rows), len(rows) == 4)
    seen = set()
    references = []
    for row in rows:
        n, dx, sign = int(row[0]), float(row[1]), int(row[2])
        force = tuple(map(float, row[3:6]))
        work, residual = float(row[6]), float(row[7])
        seen.add((n, sign))
        expected_force = tuple(-2 * g * (2.5 * dx)**2
                               for g in (12.0, -7.0, 5.0))
        expected_displacement = (sign * 0.11 * dx,
                                 -sign * 0.08 * dx,
                                 sign * 0.09 * dx)
        expected_work = sum(f * d for f, d in
                            zip(expected_force, expected_displacement))
        references.append({
            "grid": n, "dx_m": dx, "direction": sign,
            "force_N": force, "expected_force_N": expected_force,
            "geometric_work_J": work, "expected_work_J": expected_work,
            "traction_work_residual_J": residual
        })
        for axis, measured in enumerate(force):
            add(f"force_{n}_{sign}_{axis}", measured,
                math.isfinite(measured) and
                abs(measured - expected_force[axis]) < 3e-11)
        add(f"work_{n}_{sign}", work,
            math.isfinite(work) and abs(work - expected_work) < 3e-11)
        add(f"work_residual_{n}_{sign}", residual,
            math.isfinite(residual) and abs(residual) < 3e-11)
    add("all_grids_and_directions", sorted(seen),
        seen == {(8, -1), (8, 1), (12, -1), (12, 1)})
    suite = re.search(r"100% tests passed, 0 tests failed out of (\d+)",
                      ctest.stdout)
    suite_count = int(suite.group(1)) if suite else 0
    add("full_ctest_exit", ctest.returncode, ctest.returncode == 0)
    add("existing_ctest_suites", suite_count, suite_count == 87)
    source_hashes = {}
    for name in FILES:
        file = CORE / name
        if file.is_file():
            source_hashes[name] = hashlib.sha256(file.read_bytes()).hexdigest()
    add("source_hash_count", len(source_hashes),
        len(source_hashes) == len(FILES))
    accepted = all(c["pass"] for c in checks)
    report = {
        "phase": "P2",
        "milestone": "fractional_3d_pressure_work_and_gauge_partial",
        "partial_accepted": accepted,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "checks": checks,
        "reference": references,
        "native_stdout": native.stdout,
        "native_stderr": native.stderr[-2000:],
        "ctest_count": suite_count,
        "ctest_sha256": hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "native_binary_sha256": hashlib.sha256(NATIVE.read_bytes()).hexdigest(),
        "source_sha256": source_hashes,
        "limitations": (
            "Prescribed subcell box motion and piecewise constant pressure only; "
            "not dynamic post-projection two-way FSI, physical global momentum/"
            "energy closure, measured CFD validation or UE5.8 native E2E."
        ),
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(json.dumps({
        "partial_accepted": accepted,
        "checks_passed": sum(c["pass"] for c in checks),
        "checks_total": len(checks),
        "native_assertions": count,
        "ctest_suites": suite_count,
        "report": str(OUT)
    }, ensure_ascii=False))
    if not accepted:
        raise SystemExit(1)

if __name__ == "__main__":
    main()
