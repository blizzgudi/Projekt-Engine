"""Independent P2 partial gate: transactional, residual-verified 3D MAC projection."""
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
EXE = CORE / "build" / "upc_p2_verified_mac_projection_tests.exe"
CTEST = RUNS / "p2_verified_mac_ctest_20261031.txt"
OUT = RUNS / "p2_verified_mac_projection_partial.json"
SOURCES = [
    "include/upc/mac3d.hpp",
    "tests/p2_verified_mac_projection3d_tests.cpp",
    "scripts/p2_verified_mac_projection_full_build.cmd",
    "scripts/p2_verified_mac_projection_acceptance.py",
    "docs/P2_VERIFIED_MAC_PROJECTION_PARTIAL.md",
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
    match = re.search(r"PASS: P2 verified MAC projection assertions=(\d+)", proc.stdout)
    assertions = int(match.group(1)) if match else 0
    add("rollback_all_faces_native_assertions", assertions, assertions >= 38000)
    add("all_89_ctest", "100% tests passed, 0 tests failed out of 89" in log,
        "100% tests passed, 0 tests failed out of 89" in log)
    rows = list(csv.DictReader(io.StringIO("\n".join(
        line for line in proc.stdout.splitlines()
        if line.startswith("mode,") or line.startswith("case,")))))
    add("two_distinct_3d_grids", len(rows), len(rows) == 2)
    references = []
    for row in rows:
        n = int(row["n"])
        dx = float(row["dx"])
        rho = float(row["rho"])
        dt = float(row["dt"])
        add(f"{n}_grid", n, n in (8, 12))
        add(f"{n}_parameters", [dx, rho, dt],
            all(math.isfinite(v) and v > 0 for v in (dx, rho, dt)))
        R = rho * dx * 0.12 / dt
        expected = {
            "p_left": -R / 12.0,
            "p_right": R / 12.0,
            "u_internal": 0.1,
        }
        for name, reference in expected.items():
            observed = float(row[name])
            add(f"{n}_{name}_independent_poisson", observed - reference,
                math.isfinite(observed) and abs(observed - reference) <=
                (1e-7 if name.startswith("p_") else 1e-10))
        div = float(row["divergence"])
        one = float(row["one_cell_divergence"])
        add(f"{n}_actual_wet_divergence", div,
            math.isfinite(div) and 0 <= div < 1e-8)
        add(f"{n}_one_iteration_one_cell_converges", one,
            math.isfinite(one) and 0 <= one < 1e-8)
        references.append({
            "n": n, "dx_m": dx, "rho_kg_m3": rho, "dt_s": dt,
            "p_left_pa": -R/12, "p_right_pa": R/12,
            "internal_face_velocity_m_s": 0.1,
            "observed_divergence_s_inv": div,
            "one_cell_divergence_s_inv": one,
        })
    add("distinct_pressure_references", [r["p_right_pa"] for r in references],
        len(references) == 2 and
        abs(references[0]["p_right_pa"] - references[1]["p_right_pa"]) > 1)
    hashes = {name: sha(CORE / name) for name in SOURCES}
    add("eight_source_hashes", len(hashes), len(hashes) == 8)
    accepted = bool(checks) and all(c["pass"] for c in checks)
    report = {
        "phase": "P2",
        "milestone": "verified_transactional_mac_projection_partial",
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
            "Opt-in rollback and divergence check only. Legacy project() unchanged. "
            "No dynamic rotating FSI, physical global momentum/energy proof, "
            "external 3D CFD measurements, or new Unreal 5.8 Native E2E."
        ),
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(f"P2_VERIFIED_MAC_PARTIAL={accepted} "
          f"CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for check in checks:
            if not check["pass"]:
                print("FAIL", check)
        raise SystemExit(1)

if __name__ == "__main__":
    main()
