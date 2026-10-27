"""P2 partial: independently audit component-wise 3D Neumann compatibility.

A global zero wall flux does not imply solvability of disconnected closed
water chambers. This checks native rejection and exact open-cell references,
plus full CTest, hashes and conservative phase status. NOT full P2/FSI.
"""
from __future__ import annotations

import csv
import hashlib
import io
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parent.parent
ROOT = CORE.parents[1]
RUNS = ROOT / "ENGINE TEST STUBE" / "runs"
EXE = CORE / "build" / "upc_p2_kinematic_component_tests.exe"
CTEST_LOG = RUNS / "p2_component_compatibility_ctest_20261027.txt"
REPORT = RUNS / "p2_kinematic_component_partial.json"
SOURCES = (
    "include/upc/mac3d.hpp",
    "include/upc/kinematic_wall_projection3d.hpp",
    "tests/p2_kinematic_component_compatibility3d_tests.cpp",
    "tests/p2_kinematic_wall_projection3d_tests.cpp",
    "scripts/p2_kinematic_component_acceptance.py",
    "docs/P2_KINEMATIC_COMPONENT_COMPATIBILITY_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    if not EXE.is_file() or not CTEST_LOG.is_file():
        raise RuntimeError("Missing native component test or complete CTest log")
    log = CTEST_LOG.read_text(encoding="utf-8-sig")
    m = re.search(r"100% tests passed, 0 tests failed out of (\d+)", log)
    if not m or int(m.group(1)) < 48 or (
        "physics_p2_kinematic_disconnected_wet_component_partial" not in log
    ):
        raise RuntimeError("Full 48-test CTest proof missing")
    p = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                       text=True, timeout=60, check=False)
    if p.returncode:
        raise RuntimeError("Native test failed: " + p.stderr[-1200:])
    if "REJECTED: two_closed_moving" not in p.stderr or (
        "REJECTED: one_open_one_closed_moving" not in p.stderr
    ):
        raise RuntimeError("Independent sealed-chamber negative tests missing")
    lines = [x.strip() for x in p.stdout.splitlines() if x.strip()]
    if len(lines) != 6 or not lines[0].startswith("case,closed_components,"):
        raise RuntimeError("Unexpected native CSV or missing cases")
    marker = re.fullmatch(
        r"PASS: P2 connected-component kinematic compatibility assertions=(\d+) phase_complete=false",
        lines[-1],
    )
    if not marker or int(marker.group(1)) < 62:
        raise RuntimeError("Native assertions or partial marker missing")
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    names = ("both_open_positive", "both_open_negative",
             "one_open_one_closed_stationary", "both_closed_stationary")
    if tuple(r["case"] for r in rows) != names:
        raise RuntimeError("Unexpected scenario order")
    checks: list[dict] = []

    def check(name: str, ok: bool, value, reference) -> None:
        checks.append({"name": name, "pass": bool(ok),
                       "value": value, "reference": reference})

    check("native_assertions", int(marker.group(1)) >= 62,
          int(marker.group(1)), ">=62")
    check("sealed_components_rejected", "REJECTED: two_closed_moving" in p.stderr,
          True, True)
    check("mixed_components_rejected",
          "REJECTED: one_open_one_closed_moving" in p.stderr, True, True)
    # Independent one-cell Poisson reference: p = +/-rho*dx*vx/(2*dt).
    # For one wet cell with one moving wall and one air face,
    # incompressibility requires air-face speed equal wall speed.
    rho, dx, dt, v = 1000.0, 0.08, 0.025, 0.12
    pressure = rho * dx * v / (2 * dt)
    expected = (
        (0, 2, -pressure, pressure, v, v),
        (0, 2, pressure, -pressure, -v, -v),
        (1, 1, 0.0, 0.0, 0.0, 0.0),
        (2, 0, 0.0, 0.0, 0.0, 0.0),
    )
    for row, (closed, opened, pl, pr, vl, vr) in zip(rows, expected):
        name = row["case"]
        check(name+"_closed", int(row["closed_components"]) == closed,
              int(row["closed_components"]), closed)
        check(name+"_open", int(row["open_components"]) == opened,
              int(row["open_components"]), opened)
        check(name+"_body_faces", int(row["body_faces"]) == 2,
              int(row["body_faces"]), 2)
        for field, reference, tolerance in (
            ("net_body_flux_m3_s", 0.0, 1e-15),
            ("max_divergence_s_inv", 0.0, 1e-10),
            ("pressure_left_pa", pl, 1e-10),
            ("pressure_right_pa", pr, 1e-10),
            ("air_face_left_m_s", vl, 1e-12),
            ("air_face_right_m_s", vr, 1e-12),
        ):
            value = float(row[field])
            check(name+"_"+field, math.isfinite(value)
                  and abs(value-reference) <= tolerance,
                  value, {"value": reference, "tolerance": tolerance})
    if not all(x["pass"] for x in checks):
        raise RuntimeError("Independent component checks failed: "
                           + ", ".join(x["name"] for x in checks if not x["pass"]))
    gate = subprocess.run(["python", str(CORE / "scripts" / "phase_gate.py")],
                          cwd=CORE, capture_output=True, text=True, timeout=120)
    if gate.returncode or "GATES_PASS=P0,P1" not in gate.stdout or (
        "GATES_INCOMPLETE=P2,P3,P4,P5,P6,P7,P8" not in gate.stdout
    ) or "ALL_COMPLETE=False" not in gate.stdout:
        raise RuntimeError("Conservative phase gate unexpectedly changed")
    report = {
        "phase": "P2",
        "milestone": "per_connected_wet_component_neumann_compatibility_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "frozen-mask component-wise closed Neumann solvability; not geometric FSI",
        "native_assertions": int(marker.group(1)),
        "checks": checks,
        "native_csv": lines[:-1],
        "native_rejections": p.stderr.strip().splitlines(),
        "ctest_count": int(m.group(1)),
        "phase_gate_output": gate.stdout.strip(),
        "source_sha256": {name: sha(CORE / name) for name in SOURCES},
        "native_exe_sha256": sha(EXE),
        "ctest_log_sha256": sha(CTEST_LOG),
    }
    RUNS.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                      encoding="utf-8")
    print(f"PASS: P2 component compatibility {len(checks)}/{len(checks)} "
          f"independent checks, native={marker.group(1)}, "
          f"ctest={m.group(1)}, phase_complete=false")


if __name__ == "__main__":
    main()
