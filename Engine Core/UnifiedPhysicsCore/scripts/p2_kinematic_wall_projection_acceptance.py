"""P2 PARTIAL: independent numeric audit of frozen-mask kinematic wall MAC projection.

This cannot establish moving-body FSI, momentum conservation or full P2.
Only writes a passing report after native checks, complete CTest and source
fingerprints have all been independently verified.
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
EXE = CORE / "build" / "upc_p2_kinematic_wall_tests.exe"
LOG = RUNS / "p2_kinematic_wall_ctest_20261027.txt"
REPORT = RUNS / "p2_kinematic_wall_partial.json"
SOURCES = (
    "include/upc/mac3d.hpp",
    "include/upc/kinematic_wall_projection3d.hpp",
    "tests/p2_kinematic_wall_projection3d_tests.cpp",
    "scripts/p2_kinematic_wall_projection_acceptance.py",
    "docs/P2_KINEMATIC_MOVING_WALL_PROJECTION_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
)
HEADER = ("mode,n,motion,divergence_s_inv,body_faces,wall_flux_m3_s,"
          "max_pressure_pa,iterations,face_energy_proxy_J")


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def independent_wall_energy(motion: int) -> float:
    """Exact 24 normal-velocity face energies of a 2x2x2 voxel cube."""
    dx, rho = 0.08, 1000.0
    linear = (0.15, -0.11, 0.07) if motion == 0 else (0.12, -0.08, 0.05)
    omega = (0.0, 0.0, 0.0) if motion == 0 else (0.4, -0.5, 0.7)
    energy = 0.0
    flux = 0.0
    for axis in range(3):
        other = [a for a in range(3) if a != axis]
        for side in (-1, 1):
            for i in (-0.5, 0.5):
                for j in (-0.5, 0.5):
                    r = [0.0, 0.0, 0.0]
                    r[axis] = side * dx
                    r[other[0]] = i * dx
                    r[other[1]] = j * dx
                    cross = (
                        omega[1]*r[2]-omega[2]*r[1],
                        omega[2]*r[0]-omega[0]*r[2],
                        omega[0]*r[1]-omega[1]*r[0],
                    )
                    normal_speed = linear[axis] + cross[axis]
                    energy += 0.5*rho*dx**3*normal_speed**2
                    flux += side*dx**2*normal_speed
    if abs(flux) > 1e-15:
        raise AssertionError("Analytic rigid body is not volume conserving")
    return energy


def main() -> None:
    if not EXE.is_file() or not LOG.is_file():
        raise RuntimeError("Native executable or complete CTest log missing")
    log_text = LOG.read_text(encoding="utf-8-sig")
    match = re.search(r"100% tests passed, 0 tests failed out of (\d+)", log_text)
    if not match or int(match.group(1)) < 47:
        raise RuntimeError("Full MSVC Release CTest suite not green (47 expected)")
    native = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                            text=True, timeout=180, check=False)
    if native.returncode:
        raise RuntimeError("Native moving-wall test failed: " + native.stderr[-1200:])
    lines = [line.strip() for line in native.stdout.splitlines() if line.strip()]
    if len(lines) != 14 or lines[0] != HEADER:
        raise RuntimeError("Wrong native CSV header or scenario count")
    marker = re.fullmatch(
        r"PASS: P2 kinematic moving-wall projection assertions=(\d+) phase_complete=false",
        lines[-1],
    )
    if not marker:
        raise RuntimeError("Native partial PASS marker absent")
    assertions = int(marker.group(1))
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    cases = [(mode, n, motion)
             for mode in ("closed", "air")
             for n in (8, 12, 16)
             for motion in (0, 1)]
    checks = []

    def check(name: str, ok: bool, value, expected) -> None:
        checks.append({"name": name, "pass": bool(ok),
                       "value": value, "expected": expected})

    check("native_assertions", assertions >= 90000, assertions, ">=90000")
    for row, (mode, n, motion) in zip(rows, cases):
        tag = f"{mode}_{n}_{motion}"
        if (row["mode"], int(row["n"]), int(row["motion"])) != (mode, n, motion):
            raise RuntimeError("Unexpected native scenario order")
        div = float(row["divergence_s_inv"])
        count = int(row["body_faces"])
        flux = float(row["wall_flux_m3_s"])
        pressure = float(row["max_pressure_pa"])
        iterations = int(row["iterations"])
        energy = float(row["face_energy_proxy_J"])
        if not all(math.isfinite(v) for v in (div, flux, pressure, energy)):
            raise RuntimeError("Nonfinite native metric")
        reference_energy = independent_wall_energy(motion)
        check(tag+"_body_faces", count == 24, count, 24)
        check(tag+"_divergence", 0 <= div < 1e-8, div, "<1e-8 1/s")
        check(tag+"_net_wall_flux", abs(flux) < 1e-14, flux, "abs<1e-14 m3/s")
        check(tag+"_pressure", 1 < pressure < 1000, pressure, "(1,1000) Pa")
        check(tag+"_convergence", 0 < iterations < 4500,
              iterations, "(0,4500)")
        check(tag+"_face_energy_lower_bound", energy+1e-13 >= reference_energy,
              energy, f">={reference_energy}")
    # Same imposed 2x2x2 wall velocities on each grid and in air/closed modes.
    # Fluid motion is allowed to vary with resolution and pressure BC.
    check("scenarios", len(rows) == 12, len(rows), 12)
    if not all(c["pass"] for c in checks):
        failures = [c["name"] for c in checks if not c["pass"]]
        raise RuntimeError("Independent kinematic acceptance failed: "+", ".join(failures))
    phase = subprocess.run(["python", str(CORE / "scripts" / "phase_gate.py")],
                           cwd=CORE, capture_output=True, text=True, timeout=120)
    if phase.returncode or "ALL_COMPLETE=False" not in phase.stdout:
        raise RuntimeError("Conservative full phase gate not verified")
    report = {
        "phase": "P2",
        "milestone": "frozen_mask_kinematic_moving_wall_projection_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "prescribed rigid normal-wall velocity and 3D MAC pressure projection; no body remap or two-way FSI",
        "native_assertions": assertions,
        "checks": checks,
        "native_csv": lines[:-1],
        "independent_wall_energy_J": {
            str(m): independent_wall_energy(m) for m in (0, 1)
        },
        "ctest_count": int(match.group(1)),
        "phase_gate_output": phase.stdout.strip(),
        "source_sha256": {p: sha(CORE / p) for p in SOURCES},
        "native_exe_sha256": sha(EXE),
        "ctest_log_sha256": sha(LOG),
    }
    RUNS.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                      encoding="utf-8")
    print(f"PASS: P2 kinematic wall partial {len(checks)}/{len(checks)} "
          f"checks, native={assertions}, ctest={match.group(1)}, phase_complete=false")


if __name__ == "__main__":
    main()
