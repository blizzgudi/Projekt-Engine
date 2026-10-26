"""P2 partial gate: separate static-wall drive and drag in MAC face-sum ledger.

Runs the previous momentum-ledger acceptance before validating the new
independent Couette stencil. This is NOT physical FSI momentum conservation.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parent.parent
PROJECT = CORE.parents[1]
RUNS = PROJECT / "ENGINE TEST STUBE" / "runs"
PARENT_REPORT = RUNS / "p2_viscous_momentum_ledger_partial.json"
OUT = RUNS / "p2_wall_drive_drag_partial.json"
FILES = (
    "include/upc/viscous_mac3d.hpp",
    "include/upc/coupled_water3d.hpp",
    "tests/viscous_momentum_ledger3d_tests.cpp",
    "scripts/p2_momentum_ledger_acceptance.py",
    "scripts/p2_wall_drive_drag_acceptance.py",
    "docs/P2_WALL_DRIVE_DRAG_PARTIAL.md",
    "docs/PHASENPLAN.md",
)

def main() -> None:
    parent = subprocess.run(
        [sys.executable, str(CORE / "scripts/p2_momentum_ledger_acceptance.py")],
        cwd=CORE, text=True, capture_output=True, timeout=220, check=False,
    )
    if parent.returncode != 0:
        raise SystemExit(
            "P2 parent acceptance failed (not writing PASS):\n"
            + parent.stdout[-2000:] + "\n" + parent.stderr[-2000:]
        )
    previous = json.loads(PARENT_REPORT.read_text(encoding="utf-8"))
    stdout = previous.get("native_stdout", "")
    match = re.search(
        r"phase_complete=false wall_drive_sum_m_s=([-+0-9.eE]+)"
        r" preloaded_wall_drag_m_s=([-+0-9.eE]+)"
        r" live_wall_drive_proxy_kg_m_s=([-+0-9.eE]+)"
        r" live_wall_drag_proxy_kg_m_s=([-+0-9.eE]+)",
        stdout,
    )
    if match is None:
        raise SystemExit("New native drive/drag evidence missing")
    drive, drag, live_drive, live_drag = map(float, match.groups())
    alpha = 0.012 * 0.03 / (0.1 * 0.1)
    checks = [
        {"name": "parent_gate_pass", "pass": previous.get("partial_accepted") is True
         and previous.get("phase_complete") is False},
        {"name": "native_92_assertions", "pass": "checks=92 " in stdout},
        {"name": "independent_90_face_drive", "value": drive,
         "pass": math.isfinite(drive) and abs(drive - 90 * alpha * 0.4) < 1e-12},
        {"name": "independent_preloaded_wall_drag", "value": drag,
         "pass": math.isfinite(drag) and abs(drag + alpha * 0.25) < 1e-12},
        {"name": "live_wall_drive_positive", "value": live_drive,
         "pass": math.isfinite(live_drive) and live_drive > 0},
        {"name": "live_wall_drag_nonpositive", "value": live_drag,
         "pass": math.isfinite(live_drag) and live_drag <= 0},
        {"name": "native_inviscid_zero_and_ledger_closure", "pass":
         "phase_complete=false" in stdout and "live_y_closure_residual_proxy_kg_m_s=0" in stdout},
        {"name": "all_36_ctest_suites", "pass":
         any(c.get("name") == "all_ctest_suites" and c.get("pass") is True
             for c in previous.get("checks", []))},
        {"name": "phase_gate_remains_partial", "pass":
         "ALL_COMPLETE=False" in previous.get("phase_gate_stdout", "")
         and "GATES_INCOMPLETE=P2,P3,P4,P5,P6,P7,P8" in previous.get("phase_gate_stdout", "")},
        {"name": "source_hash_coverage", "pass": all((CORE / p).is_file() for p in FILES)},
    ]
    accepted = all(c["pass"] for c in checks)
    report = {
        "phase": "P2",
        "milestone": "static_wall_drive_drag_decomposition_partial",
        "partial_accepted": accepted,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "checks": checks,
        "native_stdout": stdout,
        "parent_acceptance_stdout": parent.stdout.strip(),
        "parent_report_sha256": hashlib.sha256(PARENT_REPORT.read_bytes()).hexdigest(),
        "native_binary_sha256": previous["native_binary_sha256"],
        "source_sha256": {
            p: hashlib.sha256((CORE / p).read_bytes()).hexdigest()
            for p in FILES if (CORE / p).is_file()
        },
        "limitations": (
            "Prescribed tangential velocity on fixed voxel geometry only; "
            "numerical staggered-face source decomposition, not physical "
            "solid impulse, stress-consistent free-surface viscosity, "
            "moving-body FSI, measured 3D reference, or UE5.8 native E2E. "
            "P2 through P8 remain incomplete."
        ),
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"P2_WALL_DRIVE_DRAG_PARTIAL={accepted}")
    print(f"P2_WALL_DRIVE_DRAG_NATIVE_CHECKS=92")
    print(f"P2_WALL_DRIVE_DRAG_REFERENCE={drive:.17g},{drag:.17g}")
    print(f"P2_WALL_DRIVE_DRAG_REPORT={OUT}")
    if not accepted:
        raise SystemExit("P2 wall drive/drag bounded acceptance FAILED")

if __name__ == "__main__":
    main()
