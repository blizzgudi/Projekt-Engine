"""P2 PARTIAL: independently manufactured 3D pressure projection -> MAC/PLIC.

Run only after p2_projected_plic_build.cmd completed successfully.
Never promotes P2 or any other full phase.
"""
from __future__ import annotations

import csv
import hashlib
import io
import json
import re
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parent.parent
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
PREFIX = "p2_projected_plic"
EXE = CORE / "build" / "upc_p2_projected_plic_schedule_tests.exe"
NATIVE = RUNS / (PREFIX + "_native_20261031.txt")
COMPILE = RUNS / (PREFIX + "_compile_20261031.txt")
CTEST = RUNS / (PREFIX + "_ctest_20261031.txt")
OUT = RUNS / (PREFIX + "_partial.json")
SOURCES = [
    "include/upc/p2_projected_plic_schedule3d.hpp",
    "include/upc/p2_periodic_mac_projection3d.hpp",
    "include/upc/p2_plic_temporal_schedule3d.hpp",
    "include/upc/p2_plic_adaptive_subcycle3d.hpp",
    "include/upc/p2_plic_unsplit_mac3d.hpp",
    "tests/p2_projected_plic_schedule3d_tests.cpp",
    "scripts/p2_projected_plic_build.cmd",
    "scripts/p2_projected_plic_acceptance.py",
    "docs/P2_PROJECTED_PLIC_SCHEDULE_PARTIAL.md",
]

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> None:
    native = NATIVE.read_text(encoding="utf-8", errors="replace")
    ctest = CTEST.read_text(encoding="utf-8", errors="replace")
    compile_log = COMPILE.read_text(encoding="utf-8", errors="replace")
    checks: list[dict] = []
    def check(name: str, value, ok: bool) -> None:
        checks.append({"name": name, "value": value, "pass": bool(ok)})

    match = re.search(
        r"PASS: projected MAC PLIC assertions=(\d+) phase_complete=false",
        native,
    )
    assertions = int(match.group(1)) if match else 0
    check("native_assertions", assertions, assertions >= 4000)
    check("native_no_failure", "FAIL:" not in native, "FAIL:" not in native)
    check("ctest_106_of_106", "0 tests failed out of 106" in ctest,
          "100% tests passed" in ctest and
          "0 tests failed out of 106" in ctest)
    check("msvc_compiled", "error C" not in compile_log and
          "fatal error" not in compile_log.lower(),
          "error C" not in compile_log and
          "fatal error" not in compile_log.lower())
    lines = [line for line in native.splitlines()
             if line.startswith(("case,", "gradient,", "shear,"))]
    rows = list(csv.DictReader(io.StringIO("\n".join(lines))))
    check("two_distinct_cases", len(rows), len(rows) == 2)
    reference = []
    for kind, before_min, before_max, steps in [
        ("gradient", 0.2, 0.3, 2),
        ("shear", 0.0, 1e-12, 1),
    ]:
        selected = [r for r in rows if r["case"] == kind]
        check(kind + "_unique", len(selected), len(selected) == 1)
        if not selected:
            continue
        row = selected[0]
        before = float(row["divergence_before"])
        after = float(row["divergence_after"])
        mass = float(row["mass_residual"])
        count = int(row["substeps"])
        pressure_error = float(row["pressure_max_error"])
        check(kind + "_initial_divergence", before,
              before_min <= before <= before_max)
        check(kind + "_projected_divergence", after,
              0 <= after < 1e-10)
        check(kind + "_water_volume_residual", mass, abs(mass) < 1e-12)
        check(kind + "_substeps", count, count == steps)
        check(kind + "_manufactured_pressure_error", pressure_error,
              0 <= pressure_error < 1e-8)
        reference.append({
            "case": kind, "initial_divergence_s_inv": before,
            "projected_divergence_s_inv": after,
            "mass_residual_m3": mass, "substeps": count,
            "manufactured_pressure_max_error_pa": pressure_error,
        })
    hashes = {name: sha(CORE / name) for name in SOURCES}
    check("source_fingerprint_count", len(hashes), len(hashes) == 9)
    accepted = all(item["pass"] for item in checks)
    report = {
        "phase": "P2",
        "milestone": "prescribed_periodic_projection_to_plic_partial",
        "partial_accepted": accepted,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "checks": checks,
        "native_assertions": assertions,
        "reference": reference,
        "source_sha256": hashes,
        "native_binary_sha256": sha(EXE),
        "native_log_sha256": sha(NATIVE),
        "compile_log_sha256": sha(COMPILE),
        "ctest_log_sha256": sha(CTEST),
        "limitations": (
            "Independent periodic constant-density projection of prescribed "
            "staggered MAC fields before approximate PLIC transport. "
            "No Navier-Stokes predictor, moving FSI, free-surface stress, "
            "measured 3D CFD, global energy/momentum, GPU or UE5.8 E2E."
        ),
    }
    OUT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        f"P2_PROJECTED_PLIC_PARTIAL={accepted} "
        f"CHECKS={sum(item['pass'] for item in checks)}/{len(checks)} "
        f"NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False"
    )
    if not accepted:
        for item in checks:
            if not item["pass"]:
                print("FAIL", item)
        raise SystemExit(1)

if __name__ == "__main__":
    main()
