"""P2 strict partial acceptance: algebraic masked-MAC face-square energy ledger.

Re-runs native reference and full CTest, verifies independent numeric
constants, hashes and conservative phase gate. NOT P2 full completion.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parent.parent
PROJECT = CORE.parents[1]
RUNS = PROJECT / "ENGINE TEST STUBE" / "runs"
OUT = RUNS / "p2_viscous_energy_partial.json"
SOURCE_FILES = (
    "include/upc/viscous_mac3d.hpp",
    "tests/viscous_energy_ledger3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_viscous_energy_acceptance.py",
    "docs/P2_VISCOUS_ENERGY_LEDGER_PARTIAL.md",
    "docs/PHASENPLAN.md",
)
BIN = CORE / "build" / "upc_p2_viscous_energy_ledger_tests.exe"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(argv: list[str], timeout: int) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        argv, cwd=CORE, text=True, capture_output=True,
        timeout=timeout, check=False, encoding="utf-8", errors="replace",
    )
    if result.returncode:
        raise RuntimeError(
            f"command failed (exit={result.returncode}): {argv!r}\n"
            + result.stdout[-4000:] + "\n" + result.stderr[-4000:]
        )
    return result


def main() -> None:
    if not BIN.is_file():
        raise SystemExit("P2 native energy-ledger executable not built")
    native = run([str(BIN)], 45)
    match = re.search(
        r"PASS P2 discrete masked MAC energy ledger checks=(\d+)"
        r" internal_loss=([-+0-9.eE]+)"
        r" solid_loss=([-+0-9.eE]+)"
        r" wall_work=([-+0-9.eE]+)"
        r" euler_correction=([-+0-9.eE]+)"
        r" energy_delta=([-+0-9.eE]+)"
        r" adaptive_substeps=(\d+) phase_complete=false",
        native.stdout,
    )
    if not match:
        raise SystemExit("P2 native output missing exact discrete reference")
    n_checks = int(match.group(1))
    internal, solid, work, euler, delta = map(float, match.groups()[1:6])
    substeps = int(match.group(7))
    alpha = 0.012 * 0.03 / 0.1**2
    ref_internal = 5 * alpha * 0.25**2
    ref_solid = alpha * 0.25**2
    ref_work = alpha * 0.25 * 0.4
    ref_euler = 0.5 * (
        (alpha * (0.4 - 6 * 0.25))**2
        + 4 * (alpha * (0.4 + 0.25))**2
        + 85 * (alpha * 0.4)**2
        + (alpha * 0.25)**2
    )
    ref_delta = -ref_internal - ref_solid + ref_work + ref_euler
    values = (internal, solid, work, euler, delta)
    refs = (ref_internal, ref_solid, ref_work, ref_euler, ref_delta)
    names = ("internal_loss", "solid_loss", "wall_work", "euler_correction", "energy_delta")
    checks = [
        {"name": "native_63_assertions", "pass": n_checks >= 63, "value": n_checks},
        {"name": "adaptive_substeps", "pass": substeps >= 8, "value": substeps},
        *[
            {"name": f"independent_{name}", "pass": math.isfinite(v)
             and abs(v - ref) < 1e-12, "value": v, "reference": ref}
            for name, v, ref in zip(names, values, refs)
        ],
    ]
    ctest = shutil.which("ctest")
    if not ctest:
        ctest = (
            r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
            r"\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
        )
    if not Path(ctest).is_file():
        raise SystemExit("CMake CTest not found; cannot claim acceptance")
    all_tests = run(
        [ctest, "--test-dir", "build", "--output-on-failure", "--parallel", "5"], 210
    )
    combined_ctest = all_tests.stdout + "\n" + all_tests.stderr
    suites = re.search(
        r"100% tests passed, 0 tests failed out of (\d+)", combined_ctest
    )
    checks.append({
        "name": "full_37_native_ctest_suites",
        "pass": suites is not None and int(suites.group(1)) >= 37
        and "physics_p2_masked_viscous_face_energy_ledger" in combined_ctest,
        "value": int(suites.group(1)) if suites else 0,
    })
    gate = run([sys.executable, str(CORE / "scripts/phase_gate.py")], 100)
    checks.append({
        "name": "strict_gate_not_full",
        "pass": "ALL_COMPLETE=False" in gate.stdout
        and re.search(r"GATES_INCOMPLETE=.*\bP2\b", gate.stdout) is not None,
    })
    hashes = {}
    for name in SOURCE_FILES:
        path = CORE / name
        if not path.is_file():
            raise SystemExit(f"Required source missing: {name}")
        hashes[name] = sha(path)
    checks.append({
        "name": "source_sha256_coverage",
        "pass": len(hashes) == len(SOURCE_FILES)
        and all(len(v) == 64 for v in hashes.values()),
    })
    accepted = all(check["pass"] for check in checks)
    report = {
        "phase": "P2",
        "milestone": "explicit_masked_mac_discrete_face_energy_ledger_partial",
        "partial_accepted": accepted,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "checks": checks,
        "native_stdout": native.stdout.strip(),
        "native_binary_sha256": sha(BIN),
        "ctest_exit_code": all_tests.returncode,
        "ctest_stdout_tail": combined_ctest[-10000:],
        "phase_gate_stdout": gate.stdout.strip(),
        "source_sha256": hashes,
        "limitations": (
            "Exact algebraic staggered-face-square stencil budget only. "
            "No physical continuum energy, no moving-body FSI, no "
            "stress-consistent free surface, no measured 3D reference, "
            "no new Unreal 5.8 native E2E. P2-P8 full phases remain open."
        ),
    }
    RUNS.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"P2_DISCRETE_ENERGY_PARTIAL={accepted}")
    print(f"P2_NATIVE_ASSERTIONS={n_checks}")
    print(f"P2_CTEST_SUITES={int(suites.group(1)) if suites else 0}")
    print(f"P2_ENERGY_DELTA={delta:.17g}")
    print(f"P2_REPORT={OUT}")
    if not accepted:
        raise SystemExit("P2 discrete face-energy ledger partial acceptance FAILED")


if __name__ == "__main__":
    main()
