"""P2 PARTIAL: frozen 3D pressure wrench -> rigid-body impulse response.

Independent analytic force/velocity/displacement/work reference, true native
3D MAC projection and full CTest required. Fluid reaction is bookkeeping only.
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
REPORT = RUNS / "p2_rigid_pressure_response_partial.json"
LOG = RUNS / "p2_rigid_pressure_response_ctest_20261027.txt"
EXE = CORE / "build" / "upc_p2_rigid_pressure_response_tests.exe"
SOURCES = [
    "include/upc/mac3d.hpp",
    "include/upc/static_pressure_load3d.hpp",
    "include/upc/rigid_pressure_response3d.hpp",
    "tests/p2_rigid_pressure_response3d_tests.cpp",
    "scripts/p2_rigid_pressure_response_acceptance.py",
    "docs/P2_FROZEN_PRESSURE_RIGID_RESPONSE_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def reference(n: int):
    """Closed-form 2x2x2 pressure integration, independent of native CSV."""
    dx, dt, mass = 0.08, 0.05, 2.5
    ox, oy, oz = n // 2 - 1, (n - 1) // 2 - 1, (n - 2) // 2 - 1
    centre = ((ox + 1) * dx, (oy + 1) * dx, (oz + 1) * dx)
    force = tuple(-12 * slope * dx * dx for slope in (7.0, -11.0, 5.0))
    velocity0 = (0.1, -0.2, 0.3)
    velocity = tuple(v + f * dt / mass for v, f in zip(velocity0, force))
    position = tuple(c + v * dt + 0.5 * f * dt * dt / mass
                     for c, v, f in zip(centre, velocity0, force))
    reaction = tuple(-f * dt for f in force)
    work = sum(f * (x - c) for f, x, c in zip(force, position, centre))
    energy = 0.5 * mass * sum(v1*v1-v0*v0
                              for v1, v0 in zip(velocity, velocity0))
    if abs(work - energy) > 1e-13:
        raise RuntimeError("Independent constant-force energy reference invalid")
    return force, velocity, position, reaction, work


def main() -> None:
    if not EXE.is_file() or not LOG.is_file():
        raise RuntimeError("Native binary or complete CTest log missing")
    proc = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                          text=True, timeout=120, check=False)
    if proc.returncode:
        raise RuntimeError("Native rigid pressure test failed: " + proc.stderr[-1200:])
    lines = [line.strip() for line in proc.stdout.splitlines() if line.strip()]
    header = ("mode,n,offset_pa,force_x_N,force_y_N,force_z_N,"
              "velocity_x_m_s,velocity_y_m_s,velocity_z_m_s,"
              "centre_x_m,centre_y_m,centre_z_m,"
              "omega_x_rad_s,omega_y_rad_s,omega_z_rad_s,"
              "reaction_x_Ns,reaction_y_Ns,reaction_z_Ns,"
              "pressure_work_J,kinetic_delta_J,energy_residual_J,divergence")
    if len(lines) != 14 or lines[0] != header:
        raise RuntimeError("Expected 12 native CSV scenarios and PASS marker")
    match = re.fullmatch(
        r"PASS: P2 frozen pressure response assertions=(\d+) phase_complete=false",
        lines[-1],
    )
    if not match:
        raise RuntimeError("Native PASS/incomplete-phase marker missing")
    assertions = int(match.group(1))
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    cases = [(mode, n, offset) for mode in ("air", "closed")
             for n in (8, 12, 16) for offset in (150.0, 1200.0)]
    checks: list[dict] = []

    def check(name: str, passed: bool, value=None, expected=None) -> None:
        checks.append({"name": name, "pass": bool(passed),
                       "value": value, "expected": expected})

    check("native_assertions", assertions >= 12*38, assertions, ">=456")
    grouped = {}
    for row, (mode, n, offset) in zip(rows, cases):
        tag = f"{mode}_{n}_{int(offset)}"
        try:
            observed_n = int(row["n"])
            observed_offset = float(row["offset_pa"])
            force = tuple(float(row[f"force_{axis}_N"]) for axis in "xyz")
            velocity = tuple(float(row[f"velocity_{axis}_m_s"]) for axis in "xyz")
            centre = tuple(float(row[f"centre_{axis}_m"]) for axis in "xyz")
            omega = tuple(float(row[f"omega_{axis}_rad_s"]) for axis in "xyz")
            reaction = tuple(float(row[f"reaction_{axis}_Ns"]) for axis in "xyz")
            work = float(row["pressure_work_J"])
            kinetic = float(row["kinetic_delta_J"])
            residual = float(row["energy_residual_J"])
            div = float(row["divergence"])
        except (KeyError, ValueError, TypeError) as exc:
            raise RuntimeError("Malformed native CSV") from exc
        if not all(math.isfinite(x) for x in
                   (*force, *velocity, *centre, *omega, *reaction,
                    work, kinetic, residual, div, observed_offset)):
            raise RuntimeError("Nonfinite native pressure response")
        ref_force, ref_velocity, ref_centre, ref_reaction, ref_work = reference(n)
        check(tag+"_mode", row["mode"] == mode, row["mode"], mode)
        check(tag+"_n", observed_n == n, observed_n, n)
        check(tag+"_offset", observed_offset == offset, observed_offset, offset)
        for axis in range(3):
            check(tag+f"_force_{axis}",
                  abs(force[axis]-ref_force[axis]) < 2e-5,
                  force[axis], ref_force[axis])
            check(tag+f"_velocity_{axis}",
                  abs(velocity[axis]-ref_velocity[axis]) < 5e-7,
                  velocity[axis], ref_velocity[axis])
            check(tag+f"_position_{axis}",
                  abs(centre[axis]-ref_centre[axis]) < 2e-8,
                  centre[axis], ref_centre[axis])
            check(tag+f"_reaction_{axis}",
                  abs(reaction[axis]-ref_reaction[axis]) < 1e-6,
                  reaction[axis], ref_reaction[axis])
            check(tag+f"_omega_{axis}",
                  abs(omega[axis]-(0.2,-0.1,0.15)[axis]) < 1e-4,
                  omega[axis], (0.2,-0.1,0.15)[axis])
        check(tag+"_work", abs(work-ref_work) < 1e-5, work, ref_work)
        check(tag+"_kinetic", abs(kinetic-ref_work) < 1e-5,
              kinetic, ref_work)
        check(tag+"_energy_residual", abs(residual) < 1e-12,
              residual, 0.0)
        check(tag+"_projection_divergence", 0 <= div < 2e-8,
              div, "<2e-8")
        grouped[(mode, n, offset)] = (force, velocity, centre, reaction, work)
    for mode in ("air", "closed"):
        for n in (8, 12, 16):
            a = grouped[(mode, n, 150.0)]
            b = grouped[(mode, n, 1200.0)]
            diff = max(abs(x-y) for xrow, yrow in zip(a, b)
                       for x, y in zip(
                           xrow if isinstance(xrow, tuple) else (xrow,),
                           yrow if isinstance(yrow, tuple) else (yrow,)))
            check(f"{mode}_{n}_gauge_invariance",
                  diff < 3e-5, diff, "<3e-5")
    raw = LOG.read_bytes()
    log_text = raw.decode("utf-16" if raw.startswith((b"\xff\xfe", b"\xfe\xff"))
                          else "utf-8")
    check("complete_ctest_43_of_43",
          "100% tests passed, 0 tests failed out of 43" in log_text
          and "43/43 Test" in log_text,
          "43/43" if "43/43 Test" in log_text else "MISSING", "43/43")
    accepted = all(x["pass"] for x in checks)
    report = {
        "schema_version": 1, "phase": "P2",
        "milestone": "frozen_3d_mac_pressure_rigid_impulse_partial",
        "partial_accepted": accepted, "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "12 real projected 3D MAC pressure fields; static body wrench -> frozen-force velocity/position/impulse and energy ledger",
        "native_assertions": assertions, "native_stdout": proc.stdout,
        "checks": checks,
        "source_sha256": {p: sha(CORE/p) for p in SOURCES},
        "binary_sha256": sha(EXE),
        "ctest_log_path": str(LOG),
        "ctest_log_sha256": sha(LOG),
        "limitation": "No fluid-side impulse applied, no moving solid occupancy, no body orientation, no physical FSI conservation, no measured 3D CFD or new UE5.8 native E2E; P2 remains partial",
    }
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2),
                      encoding="utf-8")
    print(f"P2_RIGID_PRESSURE_PARTIAL_ACCEPTED={accepted}")
    print(f"P2_RIGID_PRESSURE_CHECKS={len(checks)}")
    print(f"P2_RIGID_PRESSURE_NATIVE_ASSERTIONS={assertions}")
    print("P2_RIGID_PRESSURE_PHASE_COMPLETE=False")
    print(f"P2_RIGID_PRESSURE_REPORT={REPORT}")
    if not accepted:
        raise SystemExit("Failed checks: "+", ".join(
            x["name"] for x in checks if not x["pass"]))


if __name__ == "__main__":
    main()
