"""P2 PARTIAL: 3D manufactured MAC pressure-gradient cancellation.
Independent Python reconstruction of initial divergence (air/solid boundaries),
native 3D obstacle tests, CTest and source/binary/log fingerprints.
No measured CFD, moving FSI, or full P2 acceptance is claimed.
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
REPORT = RUNS / "p2_manufactured_projection_partial.json"
LOG = RUNS / "p2_pressure_projection_ctest_20261027.txt"
EXE = CORE / "build" / "upc_p2_manufactured_projection_tests.exe"
SOURCES = [
    "include/upc/mac3d.hpp",
    "tests/p2_pressure_projection_manufactured3d_tests.cpp",
    "scripts/p2_pressure_projection_acceptance.py",
    "docs/P2_MANUFACTURED_PRESSURE_PROJECTION_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def reference_divergence(n: int, closed: bool) -> tuple[float, int]:
    """Independent finite-volume divergence from an analytic pressure field.

    The velocity is the manufactured pressure gradient; the expected result
    is NOT obtained by reading any C++ solver output or pressure arrays.
    """
    nx, ny, nz = n, n - 1, n - 2
    dx, dt, rho = 0.08, 0.02, 1000.0
    obstacle = (nx // 2, ny // 2, nz // 2)

    def inside(x: int, y: int, z: int) -> bool:
        return 0 <= x < nx and 0 <= y < ny and 0 <= z < nz

    def wall(x: int, y: int, z: int) -> bool:
        return (
            not inside(x, y, z)
            or x == 0 or y == 0 or y == ny - 1 or z == 0 or z == nz - 1
            or (closed and x == nx - 1)
            or (x, y, z) == obstacle
        )

    def wet(x: int, y: int, z: int) -> bool:
        return inside(x, y, z) and not wall(x, y, z) and (closed or x < nx - 2)

    def p(x: int, y: int, z: int) -> float:
        if not wet(x, y, z):
            return 0.0
        return (
            250.0 + 17.0 * math.sin(0.27 * x)
            + 13.0 * math.cos(0.41 * y)
            + 11.0 * math.sin(0.23 * z)
            + 0.75 * x * y / nx
        )

    def face(a: tuple[int, int, int], b: tuple[int, int, int]) -> float:
        if wall(*a) or wall(*b):
            return 0.0
        aw, bw = wet(*a), wet(*b)
        if not (aw or bw):
            return 0.0
        return dt / (rho * dx) * (2.0 if aw != bw else 1.0) * (p(*b) - p(*a))

    m, count = 0.0, 0
    for z in range(nz):
        for y in range(ny):
            for x in range(nx):
                if not wet(x, y, z):
                    continue
                count += 1
                div = (
                    face((x, y, z), (x + 1, y, z))
                    - face((x - 1, y, z), (x, y, z))
                    + face((x, y, z), (x, y + 1, z))
                    - face((x, y - 1, z), (x, y, z))
                    + face((x, y, z), (x, y, z + 1))
                    - face((x, y, z - 1), (x, y, z))
                ) / dx
                m = max(m, abs(div))
    return m, count


def main() -> None:
    if not EXE.is_file() or not LOG.is_file():
        raise RuntimeError("Native binary or full CTest log missing")
    run = subprocess.run([str(EXE)], cwd=CORE, capture_output=True, text=True, timeout=90)
    if run.returncode != 0:
        raise RuntimeError("Native manufactured projection FAILED: " + run.stderr[-1500:])
    lines = [line.strip() for line in run.stdout.splitlines() if line.strip()]
    header = (
        "mode,nx,ny,nz,fluid_cells,initial_divergence,final_divergence,"
        "max_pressure_error_pa,max_face_speed_m_s,max_solid_flux_m_s"
    )
    if len(lines) != 8 or lines[0] != header:
        raise RuntimeError("Expected six native reference rows and PASS marker")
    match = re.fullmatch(
        r"PASS: P2 3D manufactured projection assertions=(\d+) phase_complete=false",
        lines[-1],
    )
    if not match:
        raise RuntimeError("Missing native assertions / explicit incomplete marker")
    assertions = int(match.group(1))
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    checks: list[dict] = []

    def check(name: str, condition: bool, value=None, limit=None) -> None:
        checks.append({"name": name, "pass": bool(condition), "value": value, "limit": limit})

    check("native_assertions", assertions >= 38000, assertions, 38000)
    expected = [(mode, n) for mode in ("air", "closed") for n in (8, 12, 16)]
    for i, (row, (mode, n)) in enumerate(zip(rows, expected)):
        try:
            dims = [int(row[k]) for k in ("nx", "ny", "nz")]
            fluid_cells = int(row["fluid_cells"])
            initial = float(row["initial_divergence"])
            final = float(row["final_divergence"])
            pressure = float(row["max_pressure_error_pa"])
            velocity = float(row["max_face_speed_m_s"])
            wall_flux = float(row["max_solid_flux_m_s"])
        except (ValueError, KeyError, TypeError) as exc:
            raise RuntimeError("Malformed native projection CSV") from exc
        if not all(map(math.isfinite, (initial, final, pressure, velocity, wall_flux))):
            raise RuntimeError("Nonfinite manufactured projection result")
        ref_div, ref_count = reference_divergence(n, mode == "closed")
        tag = f"{mode}_{n}"
        check(tag + "_mode", row["mode"] == mode, row["mode"], mode)
        check(tag + "_dimensions", dims == [n, n - 1, n - 2], dims, [n, n - 1, n - 2])
        check(tag + "_fluid_count", fluid_cells == ref_count, fluid_cells, ref_count)
        check(tag + "_independent_initial_divergence",
              abs(initial - ref_div) < 2e-12, initial, ref_div)
        check(tag + "_projection_divergence", 0 <= final < 2e-8, final, 2e-8)
        check(tag + "_pressure_gauge_aware", 0 <= pressure < 3e-4, pressure, 3e-4)
        check(tag + "_velocity_cancellation", 0 <= velocity < 2e-7, velocity, 2e-7)
        check(tag + "_solid_impermeability", wall_flux == 0.0, wall_flux, 0.0)
        check(tag + "_divergence_reduction", initial / max(final, 1e-30) > 1e6,
              initial / max(final, 1e-30), 1e6)
    log_bytes = LOG.read_bytes()
    log_text = log_bytes.decode("utf-16" if log_bytes.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8")
    check("complete_ctest_42_of_42",
          "100% tests passed, 0 tests failed out of 42" in log_text
          and "42/42 Test" in log_text,
          "42/42" if "42/42 Test" in log_text else "MISSING", "42/42")
    source_hashes = {name: sha(CORE / name) for name in SOURCES}
    accepted = all(c["pass"] for c in checks)
    report = {
        "schema_version": 1,
        "phase": "P2",
        "milestone": "3d_manufactured_pressure_projection_partial",
        "partial_accepted": accepted,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "scope": (
            "3D staggered MAC pressure projection; manufactured gradient "
            "cancellation with internal solid obstacle, exterior no-flux, "
            "liquid/air half-cell Dirichlet and closed pure-Neumann pressure "
            "gauge on 8x7x6, 12x11x10 and 16x15x14 grids."
        ),
        "native_assertions": assertions,
        "native_stdout": run.stdout,
        "checks": checks,
        "source_sha256": source_hashes,
        "binary_sha256": sha(EXE),
        "ctest_log_path": str(LOG),
        "ctest_log_sha256": sha(LOG),
        "limitation": (
            "Manufactured discrete pressure-gradient identity, not external "
            "measured 3D CFD. No moving-solid two-way FSI, physical "
            "stress-consistent free-surface viscosity, turbulence validation "
            "or new Unreal 5.8 Native-E2E."
        ),
    }
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"P2_MANUFACTURED_PROJECTION_PARTIAL_ACCEPTED={accepted}")
    print(f"P2_MANUFACTURED_PROJECTION_CHECKS={len(checks)}")
    print(f"P2_MANUFACTURED_PROJECTION_NATIVE_ASSERTIONS={assertions}")
    print("P2_MANUFACTURED_PROJECTION_PHASE_COMPLETE=False")
    print(f"P2_MANUFACTURED_PROJECTION_REPORT={REPORT}")
    if not accepted:
        raise SystemExit("Failed: " + ", ".join(c["name"] for c in checks if not c["pass"]))


if __name__ == "__main__":
    main()
