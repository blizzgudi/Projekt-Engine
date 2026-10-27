"""P2 PARTIAL: independent post-projection six-component face-wrench audit.

A passing diagnostic must NOT be interpreted as post-projection FSI conservation.
It explicitly measures the remaining body+face impulse and torque residuals.
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
REPORT = RUNS / "p2_post_projection_audit_partial.json"
LOG = RUNS / "p2_post_projection_audit_ctest_20261027.txt"
EXE = CORE / "build" / "upc_p2_post_projection_audit_tests.exe"
SOURCES = [
    "include/upc/mac3d.hpp",
    "include/upc/static_pressure_load3d.hpp",
    "include/upc/rigid_pressure_response3d.hpp",
    "include/upc/pressure_wrench_reaction3d.hpp",
    "include/upc/post_projection_wrench_audit3d.hpp",
    "tests/p2_post_projection_wrench_audit3d_tests.cpp",
    "scripts/p2_post_projection_audit_acceptance.py",
    "docs/P2_POST_PROJECTION_WRENCH_AUDIT_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def expected_counts(n: int, closed: bool) -> tuple[int, int]:
    nx, ny, nz = n, n - 1, n - 2
    ox, oy, oz = nx // 2 - 1, ny // 2 - 1, nz // 2 - 1

    def inside(x: int, y: int, z: int) -> bool:
        return 0 <= x < nx and 0 <= y < ny and 0 <= z < nz

    def body(x: int, y: int, z: int) -> bool:
        return ox <= x <= ox + 1 and oy <= y <= oy + 1 and oz <= z <= oz + 1

    def solid(x: int, y: int, z: int) -> bool:
        return (not inside(x, y, z) or x == 0 or y == 0
                or y == ny - 1 or z == 0 or z == nz - 1
                or (closed and x == nx - 1) or body(x, y, z))

    def wet(x: int, y: int, z: int) -> bool:
        return inside(x, y, z) and not solid(x, y, z) and (closed or x < nx - 2)

    counts = [0, 0]
    for a, (dx, dy, dz) in enumerate(((1, 0, 0), (0, 1, 0), (0, 0, 1))):
        for z in range(nz):
            for y in range(ny):
                for x in range(nx):
                    xx, yy, zz = x + dx, y + dy, z + dz
                    if not inside(xx, yy, zz) or solid(x, y, z) or solid(xx, yy, zz):
                        continue
                    if not (wet(x, y, z) or wet(xx, yy, zz)):
                        continue
                    counts[0 if wet(x, y, z) and wet(xx, yy, zz) else 1] += 1
    return tuple(counts)


def main() -> None:
    if not EXE.is_file() or not LOG.is_file():
        raise RuntimeError("Missing native binary or full CTest log")
    run = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                         text=True, timeout=120, check=False)
    if run.returncode:
        raise RuntimeError("Native audit failed: " + run.stderr[-1200:])
    lines = [line.strip() for line in run.stdout.splitlines() if line.strip()]
    header = ["mode", "n", "offset_pa", "divergence_post"]
    header += [f"{name}_{axis}_Ns"
               for name in ("reaction", "projection", "residual")
               for axis in "xyz"]
    header += [f"{name}_t{axis}_Nms"
               for name in ("reaction", "projection", "residual")
               for axis in "xyz"]
    header += ["energy_reaction_J", "energy_projection_J", "energy_total_J",
               "wetwet_faces", "wetair_faces"]
    if len(lines) != 14 or lines[0] != ",".join(header):
        raise RuntimeError("Native CSV header/scenario count mismatch")
    marker = re.fullmatch(
        r"PASS: P2 post-projection face-wrench audit assertions=(\d+) phase_complete=false",
        lines[-1])
    if not marker:
        raise RuntimeError("Native partial PASS marker missing")
    assertions = int(marker.group(1))
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    cases = [(mode, n, offset)
             for mode in ("air", "closed")
             for n in (8, 12, 16)
             for offset in (150.0, 1200.0)]
    if len(rows) != len(cases):
        raise RuntimeError("Wrong number of native scenarios")
    checks: list[dict] = []

    def check(name: str, ok: bool, value=None, expected=None) -> None:
        checks.append({"name": name, "pass": bool(ok),
                       "value": value, "expected": expected})

    check("native_assertions", assertions >= 700, assertions, ">=700")
    grouped = {}
    ref_force = tuple(-12.0 * slope * 0.08**2 for slope in (7.0, -11.0, 5.0))
    r = (-0.013, 0.017, -0.011)
    ref_torque = (
        r[1] * ref_force[2] - r[2] * ref_force[1],
        r[2] * ref_force[0] - r[0] * ref_force[2],
        r[0] * ref_force[1] - r[1] * ref_force[0],
    )
    for row, (mode, n, offset) in zip(rows, cases):
        tag = f"{mode}_{n}_{int(offset)}"
        try:
            numbers = {key: float(row[key]) for key in header[3:-2]}
            wetwet, wetair = int(row["wetwet_faces"]), int(row["wetair_faces"])
            got_n, got_offset = int(row["n"]), float(row["offset_pa"])
        except (ValueError, KeyError, TypeError) as exc:
            raise RuntimeError("Malformed native numeric CSV") from exc
        if not all(math.isfinite(v) for v in numbers.values()):
            raise RuntimeError("Nonfinite native audit output")
        check(tag + "_mode", row["mode"] == mode, row["mode"], mode)
        check(tag + "_n", got_n == n, got_n, n)
        check(tag + "_offset", got_offset == offset, got_offset, offset)
        div = numbers["divergence_post"]
        check(tag + "_divergence", 0 <= div < 2e-8, div, "<2e-8")
        counts = expected_counts(n, mode == "closed")
        check(tag + "_wetwet_count", wetwet == counts[0], wetwet, counts[0])
        check(tag + "_wetair_count", wetair == counts[1], wetair, counts[1])
        for i, axis in enumerate("xyz"):
            reaction = numbers[f"reaction_{axis}_Ns"]
            projection = numbers[f"projection_{axis}_Ns"]
            residual = numbers[f"residual_{axis}_Ns"]
            reaction_t = numbers[f"reaction_t{axis}_Nms"]
            projection_t = numbers[f"projection_t{axis}_Nms"]
            residual_t = numbers[f"residual_t{axis}_Nms"]
            check(tag + f"_reaction_linear_{axis}",
                  abs(reaction + ref_force[i] * 0.05) < 2e-6,
                  reaction, -ref_force[i] * 0.05)
            check(tag + f"_reaction_angular_{axis}",
                  abs(reaction_t + ref_torque[i] * 0.05) < 1e-7,
                  reaction_t, -ref_torque[i] * 0.05)
            check(tag + f"_projection_linear_residual_{axis}",
                  abs(residual - projection) < 1e-11,
                  residual - projection, 0.0)
            check(tag + f"_projection_angular_residual_{axis}",
                  abs(residual_t - projection_t) < 1e-11,
                  residual_t - projection_t, 0.0)
        er = numbers["energy_reaction_J"]
        ep = numbers["energy_projection_J"]
        et = numbers["energy_total_J"]
        check(tag + "_energy_telescope", abs(er + ep - et) < 1e-11,
              er + ep - et, 0.0)
        check(tag + "_projection_dissipation_proxy", ep < 0, ep, "<0")
        linear_residual = max(abs(numbers[f"residual_{a}_Ns"]) for a in "xyz")
        angular_residual = max(abs(numbers[f"residual_t{a}_Nms"]) for a in "xyz")
        if mode == "air":
            check(tag + "_open_linear_not_conserved",
                  linear_residual > 0.01, linear_residual, ">0.01")
            check(tag + "_open_angular_not_conserved",
                  angular_residual > 0.001, angular_residual, ">0.001")
        else:
            check(tag + "_closed_linear_reaction_cancelled_by_projection",
                  all(abs(numbers[f"residual_{a}_Ns"] - ref_force[i] * 0.05) < 1e-5
                      for i, a in enumerate("xyz")),
                  linear_residual, "body impulse remains after projection")
            check(tag + "_closed_angular_not_conserved",
                  angular_residual > 0.001, angular_residual, ">0.001")
        grouped[(mode, n, offset)] = (numbers, wetwet, wetair)
    for mode in ("air", "closed"):
        for n in (8, 12, 16):
            a, wa, aa = grouped[(mode, n, 150.0)]
            b, wb, ab = grouped[(mode, n, 1200.0)]
            delta = max(abs(a[key] - b[key]) for key in a)
            check(f"{mode}_{n}_gauge_invariance",
                  delta < 1e-5 and wa == wb and aa == ab,
                  delta, "<1e-5 and face counts equal")
    raw = LOG.read_bytes()
    log_text = raw.decode("utf-16" if raw.startswith((b"\xff\xfe", b"\xfe\xff"))
                          else "utf-8-sig")
    check("full_ctest_46",
          "100% tests passed, 0 tests failed out of 46" in log_text
          and "46/46 Test" in log_text,
          "46/46" if "46/46 Test" in log_text else "MISSING", "46/46")
    accepted = all(c["pass"] for c in checks)
    report = {
        "schema_version": 1,
        "phase": "P2",
        "milestone": "post_projection_six_component_face_wrench_audit_partial",
        "partial_accepted": accepted,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "12 real 3D MAC projections; read-only before/reaction/after projection face momentum, torque and energy; independent negative conservation diagnostics",
        "native_assertions": assertions,
        "native_stdout": run.stdout,
        "checks": checks,
        "source_sha256": {name: sha(CORE / name) for name in SOURCES},
        "binary_sha256": sha(EXE),
        "ctest_log_path": str(LOG),
        "ctest_log_sha256": sha(LOG),
        "limitation": "Post-projection body+face momentum and torque do NOT generally close; staggered-face proxies only, fixed body occupancy, no physical FSI conservation, external CFD measurement, moving free surface stress or UE5.8 Native E2E.",
    }
    RUNS.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2),
                      encoding="utf-8")
    print(f"P2_POST_PROJECTION_AUDIT_PARTIAL_ACCEPTED={accepted}")
    print(f"P2_POST_PROJECTION_AUDIT_CHECKS={len(checks)}")
    print(f"P2_POST_PROJECTION_AUDIT_NATIVE_ASSERTIONS={assertions}")
    print("P2_POST_PROJECTION_AUDIT_PHASE_COMPLETE=False")
    print(f"P2_POST_PROJECTION_AUDIT_REPORT={REPORT}")
    if not accepted:
        raise SystemExit("Failed: " + ", ".join(c["name"] for c in checks if not c["pass"]))


if __name__ == "__main__":
    main()
