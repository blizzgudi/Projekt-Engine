"""P2 bounded partial: masked MAC face-momentum proxy ledger.

No physical FSI closure or complete P2 acceptance is implied.
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
OUT = PROJECT / "ENGINE TEST STUBE" / "runs" / "p2_viscous_momentum_ledger_partial.json"
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
    "tests/viscous_momentum_ledger3d_tests.cpp",
    "scripts/p2_momentum_ledger_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_VISCOUS_MOMENTUM_LEDGER_PARTIAL.md",
]


def run(argv: list[str], timeout: int) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        argv, cwd=CORE, text=True, capture_output=True,
        timeout=timeout, check=False,
    )


def main() -> None:
    exe = CORE / "build" / "upc_p2_viscous_momentum_ledger_tests.exe"
    if not exe.is_file() or not CTEST.is_file():
        raise SystemExit("MSVC native binary or CTest missing")

    native = run([str(exe)], 40)
    ctest = run(
        [str(CTEST), "--test-dir", str(CORE / "build"),
         "--output-on-failure"], 100,
    )
    gate = run(
        [r"C:\Python314\python.exe", str(CORE / "scripts" / "phase_gate.py")],
        130,
    )
    pattern = (
        r"checks=(\d+) discrete_wall_source_m_s=([0-9.eE+\-]+)"
        r" adaptive_substeps=(\d+) live_mass_kg=([0-9.eE+\-]+)"
        r" live_wall_y_source_proxy_kg_m_s=([0-9.eE+\-]+)"
        r" live_y_closure_residual_proxy_kg_m_s=([0-9.eE+\-]+)"
        r" phase_complete=false"
    )
    m = re.search(pattern, native.stdout)
    n = int(m.group(1)) if m else 0
    wall = float(m.group(2)) if m else float("nan")
    substeps = int(m.group(3)) if m else 0
    mass = float(m.group(4)) if m else float("nan")
    live_source = float(m.group(5)) if m else float("nan")
    live_residual = float(m.group(6)) if m else float("nan")
    suite = re.search(
        r"100% tests passed, 0 tests failed out of (\d+)", ctest.stdout
    )
    suites = int(suite.group(1)) if suite else 0

    checks = [
        {"name": "native_exit", "value": native.returncode,
         "pass": native.returncode == 0 and native.stdout.startswith("PASS P2")},
        {"name": "native_assertions", "value": n, "pass": n >= 50},
        {"name": "independent_couette_90_faces", "value": wall,
         "pass": math.isfinite(wall) and abs(wall - 1.296) < 1e-12},
        {"name": "adaptive_substeps", "value": substeps,
         "pass": substeps >= 8},
        {"name": "live_mass_conservation_kg", "value": mass,
         "pass": math.isfinite(mass) and abs(mass - 36.0) < 1e-8},
        {"name": "live_solid_wall_source_proxy", "value": live_source,
         "pass": math.isfinite(live_source) and live_source > 0},
        {"name": "live_momentum_proxy_closure_residual", "value": live_residual,
         "pass": math.isfinite(live_residual) and abs(live_residual) < 1e-8},
        {"name": "all_ctest_suites", "value": suites,
         "pass": ctest.returncode == 0 and suites == 36},
        {"name": "phase_gate_is_conservative", "value": gate.stdout[-500:],
         "pass": gate.returncode == 0
         and "ALL_COMPLETE=False" in gate.stdout
         and "GATES_INCOMPLETE=P2,P3,P4,P5,P6,P7,P8" in gate.stdout},
        {"name": "source_fingerprint_coverage", "value": len(FILES),
         "pass": len(FILES) >= 10 and all((CORE / p).is_file() for p in FILES)},
    ]
    ok = all(item["pass"] for item in checks)
    result = {
        "phase": "P2",
        "milestone": "masked_mac_face_momentum_proxy_ledger_partial",
        "partial_accepted": ok,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "native_stdout": native.stdout.strip(),
        "native_stderr": native.stderr.strip(),
        "native_binary_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
        "ctest_exit": ctest.returncode,
        "ctest_tail": ctest.stdout[-2800:],
        "phase_gate_exit": gate.returncode,
        "phase_gate_stdout": gate.stdout.strip(),
        "checks": checks,
        "source_sha256": {
            p: hashlib.sha256((CORE / p).read_bytes()).hexdigest()
            for p in FILES if (CORE / p).is_file()
        },
        "limitations": (
            "Face-velocity-sum budget scaled by rho*dx^3 is NOT continuum "
            "momentum or FSI impulse. Static prescribed shear only; no "
            "moving body, surface-stress consistency, measured 3D reference, "
            "global energy closure, or UE5.8 E2E for this change. P2-P8 PARTIAL."
        ),
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"P2_MOMENTUM_LEDGER_PARTIAL={ok}")
    print(f"P2_MOMENTUM_LEDGER_NATIVE_CHECKS={n}")
    print(f"P2_MOMENTUM_LEDGER_DISCRETE_SOURCE={wall}")
    print(f"P2_MOMENTUM_LEDGER_CTEST={suites}")
    print(f"P2_MOMENTUM_LEDGER_REPORT={OUT}")
    if not ok:
        raise SystemExit("P2 momentum ledger partial acceptance failed")


if __name__ == "__main__":
    main()
