"""P2 PARTIAL: independent BFS reference for exterior-only voxel VOF routes."""
import csv
import hashlib
import io
import json
import math
import re
import subprocess
from collections import deque
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parent.parent
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
EXE = CORE / "build" / "upc_p2_exterior_vof_route_tests.exe"
LOG = RUNS / "p2_exterior_vof_route_ctest_20261027.txt"
REPORT = RUNS / "p2_exterior_vof_route_partial.json"
SOURCES = [
    "include/upc/p2_voxel_sweep_remap3d.hpp",
    "include/upc/p2_exterior_vof_route3d.hpp",
    "tests/p2_exterior_vof_route3d_tests.cpp",
    "scripts/p2_exterior_vof_route_acceptance.py",
    "docs/P2_EXTERIOR_VOF_ROUTE_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def exterior_distance(n, axis, sign):
    """Independent six-neighbor BFS; old/new solid excluded except endpoints."""
    centre = (n // 2,) * 3
    lead = list(centre)
    lead[axis] += sign
    source = tuple(lead)
    old = {centre}
    new = {source}
    q = deque([(source, 0)])
    visited = {source}
    while q:
        pos, distance = q.popleft()
        for a in range(3):
            for direction in (-1, 1):
                nxt = list(pos)
                nxt[a] += direction
                if not 0 <= nxt[a] < n:
                    continue
                nxt = tuple(nxt)
                if nxt in visited:
                    continue
                if nxt == centre:
                    if pos == source:  # forbid direct through-body transfer
                        continue
                    return distance + 1
                if nxt in old or nxt in new:
                    continue
                visited.add(nxt)
                q.append((nxt, distance + 1))
    raise RuntimeError("No exterior path in independent reference")


def main():
    log = LOG.read_text(encoding="utf-8-sig")
    count = re.search(r"100% tests passed, 0 tests failed out of (\d+)", log)
    if not count or int(count.group(1)) != 53 or (
        "physics_p2_exterior_vof_face_route_partial" not in log
    ):
        raise RuntimeError("Full 53-suite CTest evidence missing")
    native = subprocess.run(
        [str(EXE)], cwd=CORE, capture_output=True, text=True, timeout=120
    )
    if native.returncode:
        raise RuntimeError("Native exterior VOF failure: " + native.stderr)
    lines = [line.strip() for line in native.stdout.splitlines() if line.strip()]
    marker = re.fullmatch(
        r"PASS: P2 exterior VOF routes assertions=(\d+) phase_complete=false",
        lines[-1],
    )
    if not marker or int(marker.group(1)) < 114000:
        raise RuntimeError("Native assertion evidence missing")
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    cases = [(n, axis, sign) for n in (8, 12, 16)
             for axis in range(3) for sign in (-1, 1)]
    if len(rows) != len(cases):
        raise RuntimeError("Missing exterior-route 3D cases")
    checks = []

    def near(name, actual, expected, tol=1e-13):
        good = math.isfinite(actual) and abs(actual - expected) <= tol
        checks.append({"name": name, "actual": actual, "expected": expected,
                       "tolerance": tol, "pass": bool(good)})

    for row, (n, axis, sign) in zip(rows, cases):
        label = f"{n}_{axis}_{sign}"
        centre = [n // 2] * 3
        centre[axis] += sign
        x, y, z = centre
        fraction = 0.25 + 0.01 * ((x + 2*y + 3*z) % 20)
        expected_steps = exterior_distance(n, axis, sign)
        near(label + "_grid", float(row["grid"]), n, 0)
        near(label + "_axis", float(row["axis"]), axis, 0)
        near(label + "_sign", float(row["sign"]), sign, 0)
        near(label + "_face_steps", float(row["face_steps"]), expected_steps, 0)
        near(label + "_leading_m3", float(row["leading_m3"]),
             fraction * (0.125 ** 3))
        near(label + "_residual_m3", float(row["residual_m3"]), 0)
    if not all(check["pass"] for check in checks):
        raise RuntimeError("Independent exterior BFS or mass reference mismatch")
    gate = subprocess.run(
        ["python", str(CORE / "scripts" / "phase_gate.py")],
        cwd=CORE, capture_output=True, text=True, timeout=240,
    )
    expected = ("GATES_PASS=P0,P1",
                "GATES_INCOMPLETE=P2,P3,P4,P5,P6,P7,P8",
                "ALL_COMPLETE=False")
    if gate.returncode or not all(e in gate.stdout for e in expected):
        raise RuntimeError("Unexpected phase gate: " + gate.stdout + gate.stderr)
    report = {
        "phase": "P2",
        "milestone": "exterior_face_connected_vof_route_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "ctest_count": int(count.group(1)),
        "native_assertions": int(marker.group(1)),
        "checks": checks,
        "native_csv": lines[:-1],
        "phase_gate_output": gate.stdout.strip(),
        "source_sha256": {name: sha(CORE / name) for name in SOURCES},
        "native_exe_sha256": sha(EXE),
        "ctest_log_sha256": sha(LOG),
        "scope": "six-neighbor exterior-only scalar VOF permutation; NOT physical flux, MAC momentum, energy or FSI",
    }
    REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: P2 exterior routes {len(checks)}/{len(checks)} independent "
          f"checks; native={marker.group(1)}, ctest={count.group(1)}, "
          "phase_complete=false")


if __name__ == "__main__":
    main()
