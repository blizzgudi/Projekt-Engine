"""P2 PARTIAL: static 3D MAC multi-voxel pressure force and torque.

Independent Python integration of all 24 wetted faces, not reading any
C++ pressure array. Requires native 3D projection, complete CTest and
exact source/binary/log SHA256. This is NOT moving-solid FSI.
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
REPORT = RUNS / "p2_static_body_wrench_partial.json"
LOG = RUNS / "p2_static_body_wrench_ctest_20261027.txt"
EXE = CORE / "build" / "upc_p2_static_body_wrench_tests.exe"
SOURCES = [
    "include/upc/mac3d.hpp",
    "include/upc/static_pressure_load3d.hpp",
    "tests/p2_static_body_wrench3d_tests.cpp",
    "scripts/p2_static_body_wrench_acceptance.py",
    "docs/P2_STATIC_BODY_WRENCH_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def reference(n: int, offset: float) -> tuple[tuple[float, ...], tuple[float, ...]]:
    """Integrate all wetted face pressures independently in Python."""
    dx = 0.08
    ox, oy, oz = n // 2 - 1, (n - 1) // 2 - 1, (n - 2) // 2 - 1
    slopes = (7.0, -11.0, 5.0)
    directions = ((-1, 0, 0), (1, 0, 0), (0, -1, 0),
                  (0, 1, 0), (0, 0, -1), (0, 0, 1))
    force = [0.0] * 3
    torque = [0.0] * 3
    faces = 0
    for z in (oz, oz + 1):
        for y in (oy, oy + 1):
            for x in (ox, ox + 1):
                for d in directions:
                    neighbor = (x + d[0], y + d[1], z + d[2])
                    if (ox <= neighbor[0] <= ox + 1
                            and oy <= neighbor[1] <= oy + 1
                            and oz <= neighbor[2] <= oz + 1):
                        continue
                    pressure = offset + sum(s * v for s, v in zip(slopes, neighbor))
                    df = [-v * pressure * dx * dx for v in d]
                    arm = [(v + 0.5 + 0.5 * delta) * dx
                           for v, delta in zip((x, y, z), d)]
                    dm = (arm[1] * df[2] - arm[2] * df[1],
                          arm[2] * df[0] - arm[0] * df[2],
                          arm[0] * df[1] - arm[1] * df[0])
                    for axis in range(3):
                        force[axis] += df[axis]
                        torque[axis] += dm[axis]
                    faces += 1
    if faces != 24:
        raise RuntimeError("Independent 2x2x2 boundary has wrong face count")
    analytic_force = [-12 * s * dx * dx for s in slopes]
    centre = [(v + 1) * dx for v in (ox, oy, oz)]
    analytic_torque = (
        centre[1] * analytic_force[2] - centre[2] * analytic_force[1],
        centre[2] * analytic_force[0] - centre[0] * analytic_force[2],
        centre[0] * analytic_force[1] - centre[1] * analytic_force[0],
    )
    if max(abs(a - b) for a, b in zip(force, analytic_force)) > 1e-11:
        raise RuntimeError("Python face integration differs from closed-form force")
    if max(abs(a - b) for a, b in zip(torque, analytic_torque)) > 1e-11:
        raise RuntimeError("Python face integration differs from closed-form torque")
    return tuple(force), tuple(torque)


def main() -> None:
    if not EXE.is_file() or not LOG.is_file():
        raise RuntimeError("Native binary or complete CTest log missing")
    result = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                            text=True, timeout=90, check=False)
    if result.returncode:
        raise RuntimeError("Native 3D body wrench failed: " + result.stderr[-1200:])
    lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    header = (
        "mode,nx,ny,nz,offset_pa,solid_voxels,wetted_faces,"
        "force_x_N,force_y_N,force_z_N,torque_x_Nm,"
        "torque_y_Nm,torque_z_Nm,max_force_error_N,"
        "max_torque_error_Nm,max_pressure_error_pa,divergence"
    )
    if len(lines) != 14 or lines[0] != header:
        raise RuntimeError("Expected 12 native CSV cases and PASS marker")
    marker = re.fullmatch(
        r"PASS: P2 static body wrench assertions=(\d+) phase_complete=false",
        lines[-1],
    )
    if not marker:
        raise RuntimeError("Native PASS or incomplete phase marker missing")
    assertions = int(marker.group(1))
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    expected_order = [(mode, n, offset)
                      for mode in ("air", "closed")
                      for n in (8, 12, 16)
                      for offset in (150.0, 1200.0)]
    checks: list[dict] = []

    def check(name: str, passed: bool, value=None, expected=None) -> None:
        checks.append({"name": name, "pass": bool(passed),
                       "value": value, "expected": expected})

    check("native_assertions", assertions >= 11768, assertions, ">=11768")
    by_case: dict[tuple[str, int, float], tuple[tuple[float, ...], tuple[float, ...]]] = {}
    for row, (mode, n, offset) in zip(rows, expected_order):
        tag = f"{mode}_{n}_{int(offset)}"
        try:
            dims = [int(row[key]) for key in ("nx", "ny", "nz")]
            body_cells = int(row["solid_voxels"])
            wet_faces = int(row["wetted_faces"])
            observed_offset = float(row["offset_pa"])
            force = tuple(float(row[key]) for key in
                          ("force_x_N", "force_y_N", "force_z_N"))
            torque = tuple(float(row[key]) for key in
                           ("torque_x_Nm", "torque_y_Nm", "torque_z_Nm"))
            ferr = float(row["max_force_error_N"])
            terr = float(row["max_torque_error_Nm"])
            perr = float(row["max_pressure_error_pa"])
            div = float(row["divergence"])
        except (ValueError, KeyError, TypeError) as exc:
            raise RuntimeError("Malformed native body wrench CSV") from exc
        if not all(math.isfinite(x) for x in
                   (*force, *torque, ferr, terr, perr, div, observed_offset)):
            raise RuntimeError("Nonfinite native body wrench values")
        ref_force, ref_torque = reference(n, offset)
        check(tag + "_mode", row["mode"] == mode, row["mode"], mode)
        check(tag + "_dimensions", dims == [n, n-1, n-2], dims, [n, n-1, n-2])
        check(tag + "_offset", observed_offset == offset, observed_offset, offset)
        check(tag + "_body_voxels", body_cells == 8, body_cells, 8)
        check(tag + "_wetted_faces", wet_faces == 24, wet_faces, 24)
        for axis in range(3):
            check(tag + f"_force_{axis}", abs(force[axis] - ref_force[axis]) < 2e-5,
                  force[axis], ref_force[axis])
            check(tag + f"_torque_{axis}", abs(torque[axis] - ref_torque[axis]) < 3e-5,
                  torque[axis], ref_torque[axis])
        check(tag + "_max_force_error", 0 <= ferr < 2e-5, ferr, 2e-5)
        check(tag + "_max_torque_error", 0 <= terr < 3e-5, terr, 3e-5)
        check(tag + "_pressure_reconstruction", 0 <= perr < 3e-4, perr, 3e-4)
        check(tag + "_projection_divergence", 0 <= div < 2e-8, div, 2e-8)
        by_case[(mode, n, offset)] = (force, torque)
    for mode in ("air", "closed"):
        for n in (8, 12, 16):
            a, b = by_case[(mode, n, 150.0)], by_case[(mode, n, 1200.0)]
            delta = max(abs(x - y) for left, right in zip(a, b)
                        for x, y in zip(left, right))
            check(f"{mode}_{n}_pressure_gauge_invariance", delta < 3e-5,
                  delta, 3e-5)

    raw = LOG.read_bytes()
    log_text = raw.decode("utf-16" if raw.startswith((b"\xff\xfe", b"\xfe\xff"))
                          else "utf-8")
    check("complete_ctest_42_of_42",
          "100% tests passed, 0 tests failed out of 42" in log_text
          and "42/42 Test" in log_text,
          "42/42" if "42/42 Test" in log_text else "MISSING", "42/42")
    accepted = all(c["pass"] for c in checks)
    report = {
        "schema_version": 1, "phase": "P2",
        "milestone": "static_3d_body_pressure_force_torque_partial",
        "partial_accepted": accepted, "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "Static 2x2x2 body pressure force/torque from real 3D MAC projection; 12 native cases, independent Python face integration",
        "native_assertions": assertions,
        "native_stdout": result.stdout,
        "checks": checks,
        "source_sha256": {name: sha(CORE / name) for name in SOURCES},
        "binary_sha256": sha(EXE),
        "ctest_log_path": str(LOG),
        "ctest_log_sha256": sha(LOG),
        "limitation": "No moving-body FSI, no impulse reaction, no physical total-energy closure, no measured CFD, no UE5.8 native E2E, no full P2 acceptance",
    }
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2),
                      encoding="utf-8")
    print(f"P2_STATIC_BODY_WRENCH_PARTIAL_ACCEPTED={accepted}")
    print(f"P2_STATIC_BODY_WRENCH_CHECKS={len(checks)}")
    print(f"P2_STATIC_BODY_WRENCH_NATIVE_ASSERTIONS={assertions}")
    print("P2_STATIC_BODY_WRENCH_PHASE_COMPLETE=False")
    print(f"P2_STATIC_BODY_WRENCH_REPORT={REPORT}")
    if not accepted:
        raise SystemExit("Failed: " + ", ".join(c["name"] for c in checks if not c["pass"]))


if __name__ == "__main__":
    main()
