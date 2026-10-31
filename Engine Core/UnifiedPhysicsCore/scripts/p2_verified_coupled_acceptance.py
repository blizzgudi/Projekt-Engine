"""P2 partial acceptance: transactional verified live MAC/VOF step, NOT FSI."""
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
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
EXE = CORE / "build" / "upc_p2_verified_coupled_water_tests.exe"
CTEST = RUNS / "p2_verified_coupled_ctest_20261031.txt"
OUT = RUNS / "p2_verified_coupled_partial.json"
SOURCES = [
    "include/upc/mac3d.hpp",
    "include/upc/coupled_water3d.hpp",
    "tests/p2_verified_coupled_water3d_tests.cpp",
    "scripts/p2_verified_coupled_full_build.cmd",
    "scripts/p2_verified_coupled_acceptance.py",
    "docs/P2_VERIFIED_COUPLED_MAC_VOF_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
    "CMakeLists.txt",
]

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> None:
    proc = subprocess.run([str(EXE)], cwd=CORE, text=True,
                          capture_output=True, timeout=90, check=False)
    log = CTEST.read_text(encoding="utf-8", errors="replace")
    checks: list[dict] = []
    def add(name: str, value: object, passed: bool) -> None:
        checks.append({"name": name, "value": value, "pass": bool(passed)})
    add("native_exit", proc.returncode, proc.returncode == 0)
    match = re.search(r"PASS: P2 verified coupled MAC VOF assertions=(\d+)", proc.stdout)
    assertions = int(match.group(1)) if match else 0
    add("native_full_state_rollback_and_callback_checks", assertions,
        assertions >= 18000)
    add("all_90_ctest", "100% tests passed, 0 tests failed out of 90" in log,
        "100% tests passed, 0 tests failed out of 90" in log)
    rows = list(csv.DictReader(io.StringIO("\n".join(
        line for line in proc.stdout.splitlines()
        if line.startswith("mode,") or line.startswith("case,")))))
    add("two_distinct_3d_grids", len(rows), len(rows) == 2)
    references = []
    for row in rows:
        n = int(row["n"])
        mass = float(row["mass_kg"])
        div = float(row["divergence_s_inv"])
        steps = int(row["substeps"])
        notifications = int(row["notifications"])
        add(f"{n}_grid", n, n in (8, 12))
        add(f"{n}_independent_voxel_mass", mass - 7.0,
            math.isfinite(mass) and abs(mass - 7.0) < 1e-10)
        add(f"{n}_verified_wet_divergence", div,
            math.isfinite(div) and 0 <= div <= 1e-8)
        add(f"{n}_live_substeps", steps, steps >= 1)
        add(f"{n}_real_transport_notifications", notifications,
            notifications > 0)
        references.append({"grid": n, "expected_mass_kg": 7.0,
                           "observed_mass_kg": mass,
                           "divergence_s_inv": div,
                           "adaptive_substeps": steps,
                           "deferred_transport_notifications": notifications})
    hashes = {name: sha(CORE / name) for name in SOURCES}
    add("nine_source_hashes", len(hashes), len(hashes) == 9)
    accepted = bool(checks) and all(x["pass"] for x in checks)
    report = {
        "phase": "P2",
        "milestone": "verified_transactional_live_mac_vof_partial",
        "partial_accepted": accepted,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "checks": checks,
        "native_assertions": assertions,
        "independent_reference": references,
        "source_sha256": hashes,
        "native_binary_sha256": sha(EXE),
        "ctest_log_sha256": sha(CTEST),
        "limitations": (
            "Full-step rollback and callback deferral for opt-in live MAC/VOF "
            "only. Callback exceptions after commit cannot be rolled back. "
            "No rotating FSI, physical global momentum/energy proof, "
            "external 3D CFD measurements or Unreal 5.8 Native E2E."
        ),
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(f"P2_VERIFIED_COUPLED_PARTIAL={accepted} "
          f"CHECKS={sum(x['pass'] for x in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for item in checks:
            if not item["pass"]:
                print("FAIL", item)
        raise SystemExit(1)

if __name__ == "__main__":
    main()
