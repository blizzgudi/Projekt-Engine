"""Reproducible native evidence for P2 opt-in MAC PCG pressure partial.

Does not claim external MARIN measurement convergence, two-phase stress
consistency, or global moving-body FSI. Runs native CTest and independent
binary outputs, records SHA256 of tested code/binaries and negative cases.
"""
from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
ROOT = CORE.parents[1]
RUNS = ROOT / "ENGINE TEST STUBE" / "runs"
BUILD = CORE / "build"
CTEST = Path(
    r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
    r"\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
)
SOURCE_FILES = [
    "CMakeLists.txt",
    "include/upc/mac3d.hpp",
    "include/upc/coupled_water3d.hpp",
    "tests/p2_mac_pcg_pressure_tests.cpp",
    "tests/p2_live_pcg_coupled_tests.cpp",
    "scripts/p2_pcg_partial_acceptance.py",
]
NATIVE_TESTS = [
    ("upc_p2_mac_pcg_pressure_tests.exe",
     "PASS P2_MAC_PCG_PRESSURE",
     ["PCG_3D_HYDRO n=8", "PCG_3D_HYDRO n=12",
      "PCG_3D_HYDRO n=16", "PCG_3D_MIXED",
      "PCG_3D_NEGATIVE underconverged=REJECTED",
      "nonfinite_rhs=REJECTED", "rollback=PASS",
      "PCG_3D_LIVE"]),
    ("upc_p2_live_pcg_coupled_tests.exe",
     "PASS P2_LIVE_PCG_COUPLED",
     ["PCG_LIVE_POND", "PCG_LIVE_DAM",
      "PCG_LIVE_ROLLBACK verified=1 callbacks=0"]),
]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args: list[str], timeout: int = 300) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        args, cwd=CORE, text=True, capture_output=True,
        timeout=timeout, check=False
    )


def main() -> int:
    RUNS.mkdir(parents=True, exist_ok=True)
    sources = {rel: sha256(CORE / rel) for rel in SOURCE_FILES}
    binary_results: list[dict] = []
    ok = True
    for name, marker, required in NATIVE_TESTS:
        binary = BUILD / name
        if not binary.is_file():
            raise RuntimeError(f"Native binary missing: {binary}")
        own_source = (
            "tests/p2_mac_pcg_pressure_tests.cpp"
            if name == "upc_p2_mac_pcg_pressure_tests.exe"
            else "tests/p2_live_pcg_coupled_tests.cpp"
        )
        if binary.stat().st_mtime_ns < max(
            (CORE / rel).stat().st_mtime_ns
            for rel in ("include/upc/mac3d.hpp",
                        "include/upc/coupled_water3d.hpp", own_source)
        ):
            raise RuntimeError(f"Stale binary: {binary}; rebuild with MSVC first")
        result = run([str(binary)], 180)
        assertions = re.search(r"assertions=(\d+)", result.stdout)
        passed = (
            result.returncode == 0 and marker in result.stdout
            and "full_phase_complete=false" in result.stdout
            and all(item in result.stdout for item in required)
            and assertions is not None
        )
        ok = ok and passed
        binary_results.append({
            "name": name, "sha256": sha256(binary),
            "exit_code": result.returncode,
            "assertions": int(assertions.group(1)) if assertions else None,
            "required_markers": required,
            "stdout": result.stdout, "stderr": result.stderr,
            "passed": passed,
        })

    ctest = run(
        [str(CTEST), "--test-dir", str(BUILD),
         "--output-on-failure", "--parallel", "4"], 300
    )
    log_path = RUNS / "p2_pcg_ctest_full.log"
    log_path.write_text(ctest.stdout + "\nSTDERR\n" + ctest.stderr,
                        encoding="utf-8")
    count_match = re.search(
        r"100% tests passed, 0 tests failed out of (\d+)", ctest.stdout
    )
    ctest_passed = (
        ctest.returncode == 0 and count_match is not None
        and int(count_match.group(1)) >= 117
        and "physics_p2_mac_pcg_pressure_partial" in ctest.stdout
        and "physics_p2_live_pcg_coupled_partial" in ctest.stdout
    )
    ok = ok and ctest_passed
    report = {
        "phase": "P2",
        "milestone": "opt_in_mac_pcg_poisson_pressure_partial",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "partial_accepted": ok,
        "full_phase_complete": False,
        "external_marin_measurement_pass": False,
        "external_marin_measurement_retested": False,
        "source_sha256": sources,
        "native": binary_results,
        "ctest": {
            "exit_code": ctest.returncode,
            "count": int(count_match.group(1)) if count_match else None,
            "passed": ctest_passed,
            "log_path": str(log_path),
            "log_sha256": sha256(log_path),
        },
        "negative_qa": {
            "insufficient_iterations": "rejected natively",
            "invalid_parameters": "rejected natively",
            "nonfinite_rhs": "rejected natively",
            "coupled_state_rollback": "rejected without transport callbacks",
        },
        "not_verified": [
            "measured MARIN H1-H4 and P1-P8 pressure RMSE thresholds",
            "full >=6 second measured transient",
            "grid-independent wall transducer footprints",
            "stress-consistent two-phase liquid/air interface",
            "dynamic bidirectional fluid/rigid-body impulse and work",
            "global physical mass, momentum and energy closure",
        ],
    }
    out = RUNS / "p2_pcg_partial_acceptance.json"
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2),
                   encoding="utf-8")
    print("P2_PCG_PARTIAL_ACCEPTED=" + str(ok))
    print("FULL_PHASE_COMPLETE=False")
    print("CTEST_COUNT=" + str(report["ctest"]["count"]))
    print("EVIDENCE=" + str(out))
    return 0 if ok else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print("P2_PCG_PARTIAL_ACCEPTED=False")
        print("ERROR=" + str(exc))
        sys.exit(1)
