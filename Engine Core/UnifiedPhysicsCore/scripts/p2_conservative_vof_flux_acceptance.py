"""P2 partial only: independent numeric gate for fixed-mask 3D MAC face VOF flux."""
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
LAB = CORE.parents[1] / "ENGINE TEST STUBE"
LOG = LAB / "runs" / "p2_conservative_vof_flux_full_ctest_20261027.txt"
OUT = LAB / "runs" / "p2_conservative_vof_flux_partial.json"
EXE = CORE / "build" / "upc_p2_conservative_vof_flux_tests.exe"


def gate(name, actual, expected, tol=0.0):
    if not math.isfinite(actual) or abs(actual - expected) > tol:
        raise ValueError(f"{name}: {actual} != {expected} (tol {tol})")
    return {"name": name, "actual": actual, "expected": expected,
            "tolerance": tol, "pass": True}


def main():
    log = LOG.read_text(encoding="utf-8-sig")
    if "100% tests passed, 0 tests failed out of 54" not in log:
        raise ValueError("Full 54-suite CTest result missing")
    if "physics_p2_conservative_3d_mac_vof_face_flux_partial" not in log:
        raise ValueError("New native CTest absent from full suite")
    p = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                       text=True, timeout=120, check=True)
    rows = [line.strip().split(",") for line in p.stdout.splitlines()
            if line.startswith(("ring,", "curl,"))]
    ring = [r for r in rows if r[0] == "ring"]
    curl = [r for r in rows if r[0] == "curl"]
    if len(ring) != 18 or len(curl) != 3:
        raise ValueError("Expected 18 ring and 3 3D curl cases")
    checks = []
    seen = set()
    for r in ring:
        if len(r) != 11:
            raise ValueError(f"Malformed ring row: {r}")
        n, a, b, reverse, active = map(int, r[1:6])
        key = (n, a, b, reverse)
        if n not in (8, 12, 16) or not 0 <= a < b <= 2 or reverse not in (0, 1):
            raise ValueError(f"Unexpected ring case: {key}")
        seen.add(key)
        for name, value, expected, tol in (
            ("four_MAC_faces", float(active), 4, 0),
            ("mass_residual_m3", float(r[6]), 0, 1e-11),
            ("one_step_source", float(r[7]), 1 - 0.25, 1e-13),
            ("one_step_downstream", float(r[8]), 0.25, 1e-13),
            ("two_step_source", float(r[9]), (1 - 0.25)**2, 1e-13),
            ("two_step_downstream", float(r[10]), 2 * 0.25 * (1 - 0.25), 1e-13),
        ):
            c = gate(f"ring_{n}_{a}_{b}_{reverse}_{name}", value, expected, tol)
            checks.append(c)
    if len(seen) != 18:
        raise ValueError("Missing or duplicated ring scenario")
    for r in curl:
        if len(r) != 5:
            raise ValueError("Malformed curl row")
        n = int(r[1])
        if n not in (8, 12, 16):
            raise ValueError("Unexpected curl grid")
        active, cfl, residual = int(r[2]), float(r[3]), float(r[4])
        if active <= n*n or not (0 <= cfl < 1):
            raise ValueError("Curl advection inactive or CFL invalid")
        checks.append(gate(f"curl_{n}_mass_residual", residual, 0, 1e-9))
    m = re.search(r"assertions=(\d+) phase_complete=false", p.stdout)
    if not m or int(m.group(1)) < 44000:
        raise ValueError("Native boundedness/negative-test assertions missing")
    source_names = [
        "include/upc/p2_conservative_vof_flux3d.hpp",
        "tests/p2_conservative_vof_flux3d_tests.cpp",
        "scripts/p2_conservative_vof_flux_acceptance.py",
        "docs/P2_CONSERVATIVE_MAC_VOF_FLUX_PARTIAL.md",
        "CMakeLists.txt",
    ]
    hashes = {name: hashlib.sha256((CORE / name).read_bytes()).hexdigest()
              for name in source_names}
    report = {
        "phase": "P2",
        "milestone": "fixed_mask_conservative_mac_vof_flux_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "accepted": False,
        "native_assertions": int(m.group(1)),
        "full_ctest_suites": 54,
        "checks": checks,
        "native_stdout": p.stdout,
        "full_ctest_log_sha256": hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256": hashes,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "limitations": [
            "Frozen solid mask, no swept boundary flux or fluid-body impulse",
            "Requires a divergence-free MAC face field; no pressure solve here",
            "First-order donor-cell advection; no geometric high-order VOF",
            "No independent experimental 3D CFD measurements",
            "No new Unreal 5.8 native rendering E2E",
        ],
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                   encoding="utf-8")
    print(f"PASS: {len(checks)} independent numeric checks, "
          f"{report['native_assertions']} native assertions, "
          "54/54 full CTest; phase_complete=false")
    print(f"EVIDENCE={OUT}")


if __name__ == "__main__":
    main()
