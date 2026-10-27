"""P2 PARTIAL: independently reconstruct integer voxel-sweep water volume."""
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
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
EXE = CORE / "build" / "upc_p2_voxel_sweep_remap_tests.exe"
LOG = RUNS / "p2_voxel_sweep_ctest_20261027.txt"
REPORT = RUNS / "p2_voxel_sweep_partial.json"
SOURCES = [
    "include/upc/p2_voxel_sweep_remap3d.hpp",
    "tests/p2_voxel_sweep_remap3d_tests.cpp",
    "scripts/p2_voxel_sweep_acceptance.py",
    "docs/P2_VOXEL_SWEEP_MASS_REMAP_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    log = LOG.read_text(encoding="utf-8-sig")
    count = re.search(r"100% tests passed, 0 tests failed out of (\d+)", log)
    if not count or int(count.group(1)) != 52 or (
        "physics_p2_voxel_sweep_mass_remap_partial" not in log
    ):
        raise RuntimeError("Full 52-suite CTest evidence missing")
    native = subprocess.run(
        [str(EXE)], cwd=CORE, capture_output=True, text=True, timeout=120
    )
    if native.returncode:
        raise RuntimeError("Native sweep failure: " + native.stderr)
    lines = [s.strip() for s in native.stdout.splitlines() if s.strip()]
    marker = re.fullmatch(
        r"PASS: P2 voxel sweep conservative remap assertions=(\d+) phase_complete=false",
        lines[-1],
    )
    if not marker or int(marker.group(1)) < 76000:
        raise RuntimeError("Native assertion marker absent")
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    cases = [(n, axis, sign) for n in (8, 12, 16)
             for axis in range(3) for sign in (-1, 1)]
    if len(rows) != len(cases):
        raise RuntimeError("Missing 3D axis/direction cases")
    checks = []

    def near(name, actual, expected, tol=1e-13):
        good = math.isfinite(actual) and abs(actual - expected) <= tol
        checks.append({"name": name, "actual": actual, "expected": expected,
                       "tolerance": tol, "pass": bool(good)})

    for row, (n, axis, sign) in zip(rows, cases):
        key = f"{n}_{axis}_{sign}"
        near(key + "_grid", float(row["grid"]), n, 0)
        near(key + "_axis", float(row["axis"]), axis, 0)
        near(key + "_sign", float(row["sign"]), sign, 0)
        # Reconstruct the test's old shape and scalar field independently.
        centre = [n // 2] * 3
        old = set()
        for k in (0, 1):
            c = centre.copy()
            c[axis] += k
            old.add(tuple(c))
        c = centre.copy()
        c[(axis + 1) % 3] += 2
        old.add(tuple(c))
        new = set()
        for c in old:
            moved = list(c)
            moved[axis] += sign
            new.add(tuple(moved))
        entering = new - old
        vacated = old - new
        displaced = sum(
            ((x * 7 + y * 11 + z * 13) % 9) / 10
            for (x, y, z) in entering
        ) * (0.125 ** 3)
        near(key + "_entering", float(row["entering"]), len(entering), 0)
        near(key + "_displaced_m3", float(row["displaced_m3"]), displaced)
        near(key + "_residual_m3", float(row["residual_m3"]), 0.0)
        if len(entering) != 2 or len(vacated) != 2:
            raise RuntimeError("Incorrect independent shape reconstruction")
    if not all(c["pass"] for c in checks):
        raise RuntimeError("Independent 3D swept-volume reference mismatch")
    gate = subprocess.run(
        ["python", str(CORE / "scripts" / "phase_gate.py")],
        cwd=CORE, capture_output=True, text=True, timeout=120,
    )
    expected = ("GATES_PASS=P0,P1",
                "GATES_INCOMPLETE=P2,P3,P4,P5,P6,P7,P8",
                "ALL_COMPLETE=False")
    if gate.returncode or not all(e in gate.stdout for e in expected):
        raise RuntimeError("Unexpected full-phase gate: " + gate.stdout + gate.stderr)
    report = {
        "phase": "P2",
        "milestone": "one_cell_solid_sweep_conservative_vof_remap_partial",
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
        "scope": "exact integer voxel mask shift + conservative scalar VOF reclassification; nonphysical through-body transfer, no MAC/FSI",
    }
    REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: P2 voxel sweep {len(checks)}/{len(checks)} independent checks; "
          f"native={marker.group(1)}, ctest={count.group(1)}, phase_complete=false")


if __name__ == "__main__":
    main()
