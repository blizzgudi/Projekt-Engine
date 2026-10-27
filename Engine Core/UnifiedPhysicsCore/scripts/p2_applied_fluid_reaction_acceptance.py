"""P2 PARTIAL: independently verify actual opposite linear impulse on MAC faces.

This tests a discrete face-momentum proxy BEFORE re-projection, NOT global FSI.
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
REPORT = RUNS / "p2_applied_fluid_reaction_partial.json"
LOG = RUNS / "p2_applied_fluid_reaction_ctest_20261027.txt"
EXE = CORE / "build" / "upc_p2_pressure_reaction_transfer_tests.exe"
SOURCES = [
    "include/upc/mac3d.hpp",
    "include/upc/static_pressure_load3d.hpp",
    "include/upc/rigid_pressure_response3d.hpp",
    "include/upc/pressure_reaction_transfer3d.hpp",
    "tests/p2_pressure_reaction_transfer3d_tests.cpp",
    "scripts/p2_applied_fluid_reaction_acceptance.py",
    "docs/P2_APPLIED_FLUID_REACTION_PARTIAL.md",
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
            for dx, dy, dz in ((-1,0,0),(1,0,0),(0,-1,0),
                               (0,1,0),(0,0,-1),(0,0,1)))

    counts = [0, 0, 0]
    for z in range(nz):
        for y in range(ny):
            for x in range(nx):
                if not wet(x, y, z):
                    continue
                for a, (dx, dy, dz) in enumerate(((1,0,0),(0,1,0),(0,0,1))):
                    if wet(x + dx, y + dy, z + dz) and (
                            adjacent(x, y, z) or adjacent(x + dx, y + dy, z + dz)):
                        counts[a] += 1
    return tuple(counts)


def main() -> None:
    if not EXE.is_file() or not LOG.is_file():
        raise RuntimeError("Native binary or full CTest log is missing")
    run = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                         text=True, timeout=120, check=False)
    if run.returncode:
        raise RuntimeError("Native reaction transfer failed: " + run.stderr[-1200:])
    lines = [line.strip() for line in run.stdout.splitlines() if line.strip()]
    header = ("mode,n,offset_pa,force_x_N,force_y_N,force_z_N,"
              "body_x_Ns,body_y_Ns,body_z_Ns,"
              "fluid_x_Ns,fluid_y_Ns,fluid_z_Ns,"
              "faces_x,faces_y,faces_z,face_energy_proxy_J,"
              "divergence_pre_projection,divergence_post_projection")
    if len(lines) != 14 or lines[0] != header:
        raise RuntimeError("Expected 12 native CSV scenarios and PASS marker")
    match = re.fullmatch(
        r"PASS: P2 applied fluid face reaction assertions=(\d+) phase_complete=false",
        lines[-1])
    if not match:
        raise RuntimeError("Native PASS/incomplete-phase marker missing")
    assertions = int(match.group(1))
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    cases = [(mode, n, offset) for mode in ("air", "closed")
             for n in (8, 12, 16) for offset in (150.0, 1200.0)]
    if len(rows) != len(cases):
        raise RuntimeError("Native scenario count mismatch")
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
            native_n = int(row["n"])
            native_offset = float(row["offset_pa"])
            force = tuple(float(row[f"force_{a}_N"]) for a in "xyz")
            body = tuple(float(row[f"body_{a}_Ns"]) for a in "xyz")
            fluid = tuple(float(row[f"fluid_{a}_Ns"]) for a in "xyz")
            counts = tuple(int(row[f"faces_{a}"]) for a in "xyz")
            energy = float(row["face_energy_proxy_J"])
            before = float(row["divergence_pre_projection"])
            after = float(row["divergence_post_projection"])
        except (ValueError, TypeError, KeyError) as exc:
            raise RuntimeError("Malformed native CSV") from exc
        if not all(math.isfinite(x) for x in
                   (*force, *body, *fluid, energy, before, after, native_offset)):
            raise RuntimeError("Nonfinite native scenario")
        ref_force = tuple(-12 * s * 0.08**2 for s in (7.0, -11.0, 5.0))
        ref_body = tuple(f * 0.05 for f in ref_force)
        ref_fluid = tuple(-v for v in ref_body)
        ref_counts = expected_faces(n, closed)
        face_mass = 1000 * 0.08**3
        ref_energy = sum(j*j/(2*face_mass*c) for j, c in zip(ref_fluid, ref_counts))
        check(tag+"_mode", row["mode"] == mode, row["mode"], mode)
        check(tag+"_grid", native_n == n, native_n, n)
        check(tag+"_pressure_offset", native_offset == offset, native_offset, offset)
        for axis in range(3):
            check(tag+f"_force_{axis}", abs(force[axis]-ref_force[axis]) < 2e-5,
                  force[axis], ref_force[axis])
            check(tag+f"_body_{axis}", abs(body[axis]-ref_body[axis]) < 1e-6,
                  body[axis], ref_body[axis])
            check(tag+f"_applied_fluid_{axis}",
                  abs(fluid[axis]-ref_fluid[axis]) < 1e-6,
                  fluid[axis], ref_fluid[axis])
            check(tag+f"_net_impulse_{axis}",
                  abs(fluid[axis]+body[axis]) < 1e-12,
                  fluid[axis]+body[axis], 0.0)
            check(tag+f"_independent_face_count_{axis}",
                  counts[axis] == ref_counts[axis], counts[axis], ref_counts[axis])
        check(tag+"_face_energy_proxy",
              abs(energy-ref_energy) < 1e-8, energy, ref_energy)
        check(tag+"_reaction_creates_divergence", before > 1e-5, before, ">1e-5")
        check(tag+"_post_projection_divergence", 0 <= after < 2e-8,
              after, "<2e-8")
        grouped[(mode, n, offset)] = (force, body, fluid, counts, energy)
    for mode in ("air", "closed"):
        for n in (8, 12, 16):
            a = grouped[(mode, n, 150.0)]
            b = grouped[(mode, n, 1200.0)]
            delta = max(abs(x-y)
                        for left, right in zip(a, b)
                        for x, y in zip(left if isinstance(left, tuple) else (left,),
                                        right if isinstance(right, tuple) else (right,)))
            check(f"{mode}_{n}_gauge_invariance", delta < 3e-5, delta, "<3e-5")
    log_raw = LOG.read_bytes()
    log_text = log_raw.decode("utf-16" if log_raw.startswith((b"\xff\xfe", b"\xfe\xff"))
                              else "utf-8-sig")
    check("complete_ctest_44_of_44",
          "100% tests passed, 0 tests failed out of 44" in log_text
          and "44/44 Test" in log_text, "44/44" if "44/44 Test" in log_text else "MISSING",
          "44/44")
    accepted = all(item["pass"] for item in checks)
    report = {
        "schema_version": 1, "phase": "P2",
        "milestone": "applied_frozen_pressure_fluid_face_reaction_partial",
        "partial_accepted": accepted, "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "12 projected 3D MAC fields, frozen 2x2x2 body pressure wrench, applied opposite linear impulse to internal wet-wet faces; independent discrete face-momentum closure BEFORE pressure reprojection",
        "native_assertions": assertions,
        "native_stdout": run.stdout,
        "checks": checks,
        "source_sha256": {p: sha(CORE/p) for p in SOURCES},
        "binary_sha256": sha(EXE),
        "ctest_log_path": str(LOG),
        "ctest_log_sha256": sha(LOG),
        "limitation": "Fixed body, no remeshing or VOF body motion, no angular reaction, face-momentum proxy only, pressure reprojection changes budget; no complete 3D FSI/energy, independent CFD measurements or new UE5.8 native E2E. P2 remains PARTIAL."
    }
    RUNS.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2),
                      encoding="utf-8")
    print(f"P2_APPLIED_REACTION_PARTIAL_ACCEPTED={accepted}")
    print(f"P2_APPLIED_REACTION_CHECKS={len(checks)}")
    print(f"P2_APPLIED_REACTION_NATIVE_ASSERTIONS={assertions}")
    print("P2_APPLIED_REACTION_PHASE_COMPLETE=False")
    print(f"P2_APPLIED_REACTION_REPORT={REPORT}")
    if not accepted:
        raise SystemExit("Failed: "+", ".join(x["name"] for x in checks if not x["pass"]))


if __name__ == "__main__":
    main()
