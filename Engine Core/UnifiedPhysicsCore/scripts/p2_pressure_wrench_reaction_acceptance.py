"""P2 PARTIAL: independent six-component MAC face reaction acceptance.

The frozen-body, pre-projection face impulse proxy is NOT global FSI.
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
REPORT = RUNS / "p2_pressure_wrench_reaction_partial.json"
LOG = RUNS / "p2_pressure_wrench_reaction_ctest_20261027.txt"
EXE = CORE / "build" / "upc_p2_pressure_wrench_reaction_tests.exe"
SOURCES = [
    "include/upc/mac3d.hpp",
    "include/upc/static_pressure_load3d.hpp",
    "include/upc/rigid_pressure_response3d.hpp",
    "include/upc/pressure_wrench_reaction3d.hpp",
    "tests/p2_pressure_wrench_reaction3d_tests.cpp",
    "scripts/p2_pressure_wrench_reaction_acceptance.py",
    "docs/P2_PRESSURE_WRENCH_REACTION_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def expected_faces(n: int, closed: bool) -> tuple[int, int, int]:
    nx, ny, nz = n, n - 1, n - 2
    ox, oy, oz = nx // 2 - 1, ny // 2 - 1, nz // 2 - 1

    def inside(x: int, y: int, z: int) -> bool:
        return 0 <= x < nx and 0 <= y < ny and 0 <= z < nz

    def body(x: int, y: int, z: int) -> bool:
        return ox <= x <= ox + 1 and oy <= y <= oy + 1 and oz <= z <= oz + 1

    def wet(x: int, y: int, z: int) -> bool:
        return (inside(x, y, z) and x > 0 and y > 0 and z > 0
                and y < ny - 1 and z < nz - 1
                and (not closed or x < nx - 1)
                and (closed or x < nx - 2)
                and not body(x, y, z))

    def adjacent(x: int, y: int, z: int) -> bool:
        return wet(x, y, z) and any(body(x + dx, y + dy, z + dz)
            for dx, dy, dz in ((-1, 0, 0), (1, 0, 0),
                               (0, -1, 0), (0, 1, 0),
                               (0, 0, -1), (0, 0, 1)))

    counts = [0, 0, 0]
    for z in range(nz):
        for y in range(ny):
            for x in range(nx):
                if not wet(x, y, z):
                    continue
                for a, (dx, dy, dz) in enumerate(((1, 0, 0), (0, 1, 0), (0, 0, 1))):
                    if wet(x + dx, y + dy, z + dz) and (
                            adjacent(x, y, z) or adjacent(x + dx, y + dy, z + dz)):
                        counts[a] += 1
    return tuple(counts)


def main() -> None:
    if not EXE.is_file() or not LOG.is_file():
        raise RuntimeError("Native binary or full CTest log missing")
    run = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                         text=True, timeout=120, check=False)
    if run.returncode:
        raise RuntimeError("Native wrench reaction failed: " + run.stderr[-1200:])
    lines = [line.strip() for line in run.stdout.splitlines() if line.strip()]
    header = ("mode,n,offset_pa,force_x_N,force_y_N,force_z_N,"
              "body_x_Ns,body_y_Ns,body_z_Ns,"
              "fluid_x_Ns,fluid_y_Ns,fluid_z_Ns,"
              "body_tx_Nms,body_ty_Nms,body_tz_Nms,"
              "fluid_tx_Nms,fluid_ty_Nms,fluid_tz_Nms,"
              "faces_x,faces_y,faces_z,face_energy_proxy_J,"
              "divergence_pre_projection,divergence_post_projection")
    if len(lines) != 14 or lines[0] != header:
        raise RuntimeError("Expected 12 native scenarios plus PASS")
    match = re.fullmatch(
        r"PASS: P2 six-component face wrench reaction assertions=(\d+) phase_complete=false",
        lines[-1])
    if not match:
        raise RuntimeError("Native PASS/partial marker missing")
    assertions = int(match.group(1))
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    cases = [(mode, n, offset) for mode in ("air", "closed")
             for n in (8, 12, 16) for offset in (150.0, 1200.0)]
    if len(rows) != len(cases):
        raise RuntimeError("Scenario count mismatch")
    checks: list[dict] = []

    def check(name: str, ok: bool, value=None, expected=None) -> None:
        checks.append({"name": name, "pass": bool(ok),
                       "value": value, "expected": expected})

    check("native_assertions", assertions >= 65000, assertions, ">=65000")
    grouped: dict[tuple, tuple] = {}
    for row, (mode, n, offset) in zip(rows, cases):
        tag = f"{mode}_{n}_{int(offset)}"
        closed = mode == "closed"
        try:
            force = tuple(float(row[f"force_{a}_N"]) for a in "xyz")
            body = tuple(float(row[f"body_{a}_Ns"]) for a in "xyz")
            fluid = tuple(float(row[f"fluid_{a}_Ns"]) for a in "xyz")
            body_t = tuple(float(row[f"body_t{a}_Nms"]) for a in "xyz")
            fluid_t = tuple(float(row[f"fluid_t{a}_Nms"]) for a in "xyz")
            counts = tuple(int(row[f"faces_{a}"]) for a in "xyz")
            energy = float(row["face_energy_proxy_J"])
            before = float(row["divergence_pre_projection"])
            after = float(row["divergence_post_projection"])
            got_n = int(row["n"])
            got_offset = float(row["offset_pa"])
        except (KeyError, ValueError, TypeError) as exc:
            raise RuntimeError("Malformed native CSV") from exc
        if not all(math.isfinite(v) for v in
                   (*force, *body, *fluid, *body_t, *fluid_t, energy, before, after)):
            raise RuntimeError("Nonfinite native scenario")
        ref_force = tuple(-12 * s * 0.08**2 for s in (7.0, -11.0, 5.0))
        ref_body = tuple(v * 0.05 for v in ref_force)
        r = (-0.013, 0.017, -0.011)
        ref_torque = (
            r[1] * ref_force[2] - r[2] * ref_force[1],
            r[2] * ref_force[0] - r[0] * ref_force[2],
            r[0] * ref_force[1] - r[1] * ref_force[0],
        )
        ref_angular = tuple(v * 0.05 for v in ref_torque)
        check(tag + "_mode", row["mode"] == mode, row["mode"], mode)
        check(tag + "_n", got_n == n, got_n, n)
        check(tag + "_offset", got_offset == offset, got_offset, offset)
        for axis in range(3):
            check(tag + f"_force_{axis}", abs(force[axis] - ref_force[axis]) < 2e-5,
                  force[axis], ref_force[axis])
            check(tag + f"_body_linear_{axis}", abs(body[axis] - ref_body[axis]) < 1e-6,
                  body[axis], ref_body[axis])
            check(tag + f"_fluid_linear_{axis}", abs(fluid[axis] + ref_body[axis]) < 1e-6,
                  fluid[axis], -ref_body[axis])
            check(tag + f"_linear_closure_{axis}", abs(fluid[axis] + body[axis]) < 1e-11,
                  fluid[axis] + body[axis], 0.0)
            check(tag + f"_body_torque_{axis}", abs(body_t[axis] - ref_angular[axis]) < 1e-8,
                  body_t[axis], ref_angular[axis])
            check(tag + f"_fluid_torque_{axis}", abs(fluid_t[axis] + ref_angular[axis]) < 1e-8,
                  fluid_t[axis], -ref_angular[axis])
            check(tag + f"_angular_closure_{axis}", abs(fluid_t[axis] + body_t[axis]) < 1e-11,
                  fluid_t[axis] + body_t[axis], 0.0)
            check(tag + f"_face_count_{axis}", counts[axis] == expected_faces(n, closed)[axis],
                  counts[axis], expected_faces(n, closed)[axis])
        check(tag + "_torque_nonzero", max(map(abs, body_t)) > 1e-5,
              max(map(abs, body_t)), ">1e-5")
        check(tag + "_energy_finite", energy >= 0 and energy < 1,
              energy, "[0,1)")
        check(tag + "_pre_projection_divergence", before > 1e-5,
              before, ">1e-5")
        check(tag + "_post_projection_divergence", 0 <= after < 2e-8,
              after, "<2e-8")
        grouped[(mode, n, offset)] = (force, body, fluid, body_t, fluid_t, counts, energy)
    for mode in ("air", "closed"):
        for n in (8, 12, 16):
            a = grouped[(mode, n, 150.0)]
            b = grouped[(mode, n, 1200.0)]
            delta = max(abs(x - y)
                        for left, right in zip(a, b)
                        for x, y in zip(left if isinstance(left, tuple) else (left,),
                                        right if isinstance(right, tuple) else (right,)))
            check(f"{mode}_{n}_pressure_gauge_invariance", delta < 3e-5, delta, "<3e-5")
    raw = LOG.read_bytes()
    log_text = raw.decode("utf-16" if raw.startswith((b"\xff\xfe", b"\xfe\xff"))
                          else "utf-8-sig")
    check("full_ctest_45", "100% tests passed, 0 tests failed out of 45" in log_text
          and "45/45 Test" in log_text, "45/45" if "45/45 Test" in log_text else "MISSING", "45/45")
    accepted = all(item["pass"] for item in checks)
    report = {
        "schema_version": 1,
        "phase": "P2",
        "milestone": "applied_six_component_frozen_pressure_wrench_partial",
        "partial_accepted": accepted,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "12 real projected 3D MAC fields, frozen 2x2x2 body, applied opposite linear and angular impulses to wet-wet faces; independent 6-DOF face impulse sums before re-projection",
        "native_assertions": assertions,
        "native_stdout": run.stdout,
        "checks": checks,
        "source_sha256": {name: sha(CORE / name) for name in SOURCES},
        "binary_sha256": sha(EXE),
        "ctest_log_path": str(LOG),
        "ctest_log_sha256": sha(LOG),
        "limitation": "Frozen body and fixed occupancy; discrete face momentum proxy only before projection, not physical full FSI, no full post-projection conservation, no 3D measured CFD references or UE5.8 Native E2E. P2-P8 remain partial."
    }
    RUNS.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2),
                      encoding="utf-8")
    print(f"P2_SIX_COMPONENT_REACTION_PARTIAL_ACCEPTED={accepted}")
    print(f"P2_SIX_COMPONENT_REACTION_CHECKS={len(checks)}")
    print(f"P2_SIX_COMPONENT_REACTION_NATIVE_ASSERTIONS={assertions}")
    print("P2_SIX_COMPONENT_REACTION_PHASE_COMPLETE=False")
    print(f"P2_SIX_COMPONENT_REACTION_REPORT={REPORT}")
    if not accepted:
        raise SystemExit("Failed: " + ", ".join(x["name"] for x in checks if not x["pass"]))


if __name__ == "__main__":
    main()
