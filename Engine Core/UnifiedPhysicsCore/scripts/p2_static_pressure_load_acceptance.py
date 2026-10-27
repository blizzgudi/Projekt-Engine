"""P2 PARTIAL: static voxel pressure traction from a real 3D MAC solve.

Independent Python face-pressure force reference; 3D air/closed modes,
three resolutions, pressure-gauge/offset invariance, invalid occupancy tests,
full native CTest log and source/binary SHA256. NOT moving-solid FSI.
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
REPORT = RUNS / "p2_static_pressure_load_partial.json"
LOG = RUNS / "p2_static_pressure_load_ctest_20261027.txt"
EXE = CORE / "build" / "upc_p2_static_pressure_load_tests.exe"
SOURCES = [
    "include/upc/mac3d.hpp",
    "include/upc/static_pressure_load3d.hpp",
    "tests/p2_static_pressure_load3d_tests.cpp",
    "scripts/p2_static_pressure_load_acceptance.py",
    "docs/P2_STATIC_SOLID_PRESSURE_TRACTION_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def independent_reference(n: int, closed: bool) -> tuple[int, tuple[float, float, float]]:
    nx, ny, nz = n, n - 1, n - 2
    obstacle = (nx // 2, ny // 2, nz // 2)
    count = 0
    for z in range(nz):
        for y in range(ny):
            for x in range(nx):
                wall = (
                    x == 0 or y == 0 or y == ny - 1
                    or z == 0 or z == nz - 1
                    or (closed and x == nx - 1)
                    or (x, y, z) == obstacle
                )
                count += int(not wall and (closed or x < nx - 2))
    dx = 0.08
    # Net force ON the solid = sum of p_fluid * normal_fluid_to_solid * A.
    # Opposing pressure samples are separated by TWO cell centres.
    slopes = (7.0, -11.0, 5.0)
    return count, tuple(-2.0 * slope * dx * dx for slope in slopes)


def main() -> None:
    if not EXE.is_file() or not LOG.is_file():
        raise RuntimeError("Native binary or complete CTest log missing")
    run = subprocess.run([str(EXE)], cwd=CORE, capture_output=True, text=True, timeout=90)
    if run.returncode:
        raise RuntimeError("Native static pressure traction FAILED: " + run.stderr[-1600:])
    lines = [line.strip() for line in run.stdout.splitlines() if line.strip()]
    expected_header = (
        "mode,nx,ny,nz,offset_pa,fluid_cells,wetted_faces,"
        "force_x_N,force_y_N,force_z_N,max_force_error_N,"
        "max_pressure_error_pa,final_divergence"
    )
    if len(lines) != 14 or lines[0] != expected_header:
        raise RuntimeError("Expected twelve native rows and PASS marker")
    marker = re.fullmatch(
        r"PASS: P2 static pressure traction assertions=(\d+) phase_complete=false",
        lines[-1],
    )
    if not marker:
        raise RuntimeError("Native PASS and explicit incomplete phase marker missing")
    assertions = int(marker.group(1))
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    checks: list[dict] = []

    def check(name: str, passed: bool, value=None, expected=None) -> None:
        checks.append({
            "name": name, "pass": bool(passed),
            "value": value, "expected": expected,
        })

    check("native_assertions", assertions >= 11751, assertions, ">=11751")
    expected_order = [
        (mode, n, offset)
        for mode in ("air", "closed")
        for n in (8, 12, 16)
        for offset in (150.0, 1200.0)
    ]
    by_case = {}
    for row, (mode, n, offset) in zip(rows, expected_order):
        tag = f"{mode}_{n}_{int(offset)}"
        try:
            dims = [int(row[key]) for key in ("nx", "ny", "nz")]
            liquid_cells = int(row["fluid_cells"])
            wetted = int(row["wetted_faces"])
            observed_offset = float(row["offset_pa"])
            forces = tuple(float(row[key]) for key in ("force_x_N", "force_y_N", "force_z_N"))
            force_error = float(row["max_force_error_N"])
            pressure_error = float(row["max_pressure_error_pa"])
            divergence = float(row["final_divergence"])
        except (ValueError, KeyError, TypeError) as exc:
            raise RuntimeError("Malformed native traction CSV") from exc
        if not all(map(math.isfinite, (*forces, observed_offset, force_error,
                                    pressure_error, divergence))):
            raise RuntimeError("Nonfinite traction data")
        count, reference = independent_reference(n, mode == "closed")
        check(tag + "_mode", row["mode"] == mode, row["mode"], mode)
        check(tag + "_dimensions", dims == [n, n - 1, n - 2], dims,
              [n, n - 1, n - 2])
        check(tag + "_offset", observed_offset == offset, observed_offset, offset)
        check(tag + "_fluid_cells", liquid_cells == count, liquid_cells, count)
        check(tag + "_six_wetted_faces", wetted == 6, wetted, 6)
        for axis, (actual, expected) in enumerate(zip(forces, reference)):
            check(tag + f"_force_{axis}", abs(actual - expected) < 3e-6,
                  actual, expected)
        check(tag + "_native_max_error", 0 <= force_error < 3e-6,
              force_error, 3e-6)
        check(tag + "_pressure_reconstruction", 0 <= pressure_error < 3e-4,
              pressure_error, 3e-4)
        check(tag + "_incompressibility", 0 <= divergence < 2e-8,
              divergence, 2e-8)
        by_case[(mode, n, offset)] = forces
    for mode in ("air", "closed"):
        for n in (8, 12, 16):
            a, b = by_case[(mode, n, 150.0)], by_case[(mode, n, 1200.0)]
            delta = max(abs(x - y) for x, y in zip(a, b))
            check(f"{mode}_{n}_pressure_offset_invariance", delta < 3e-6,
                  delta, 3e-6)

    log_bytes = LOG.read_bytes()
    log_text = log_bytes.decode(
        "utf-16" if log_bytes.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8"
    )
    check("complete_ctest_41_of_41",
          "100% tests passed, 0 tests failed out of 41" in log_text
          and "41/41 Test" in log_text,
          "41/41" if "41/41 Test" in log_text else "MISSING", "41/41")
    source_hashes = {name: sha(CORE / name) for name in SOURCES}
    accepted = all(c["pass"] for c in checks)
    report = {
        "schema_version": 1,
        "phase": "P2",
        "milestone": "static_solid_pressure_traction_3d_partial",
        "partial_accepted": accepted,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "scope": (
            "Static axis-aligned solid voxel pressure-face load from a genuine "
            "3D MAC projection; six wetted faces; 3 resolutions, closed-Neumann "
            "and free-surface-Dirichlet, pressure offset/gauge invariance."
        ),
        "native_assertions": assertions,
        "native_stdout": run.stdout,
        "checks": checks,
        "source_sha256": source_hashes,
        "binary_sha256": sha(EXE),
        "ctest_log_path": str(LOG),
        "ctest_log_sha256": sha(LOG),
        "limitation": (
            "First-order cell-centred pressure-face sampling on static voxels, "
            "not moving-solid FSI, shear stress, surface tension, real CFD "
            "measurements, gauge-independent partial wetting, physical total "
            "energy/impulse conservation or UE5.8 Native-E2E."
        ),
    }
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"P2_STATIC_PRESSURE_PARTIAL_ACCEPTED={accepted}")
    print(f"P2_STATIC_PRESSURE_CHECKS={len(checks)}")
    print(f"P2_STATIC_PRESSURE_NATIVE_ASSERTIONS={assertions}")
    print("P2_STATIC_PRESSURE_PHASE_COMPLETE=False")
    print(f"P2_STATIC_PRESSURE_REPORT={REPORT}")
    if not accepted:
        raise SystemExit("Failed: " + ", ".join(c["name"] for c in checks if not c["pass"]))


if __name__ == "__main__":
    main()
