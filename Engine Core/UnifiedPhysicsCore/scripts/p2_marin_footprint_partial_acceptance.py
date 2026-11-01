"""P2 PARTIAL: audit native manufactured finite-area MARIN pressure probes.

This is an independent log-integrity/sha audit of a sampling primitive, not
SPHERIC/MARIN pressure agreement, calibrated sensor diameter, or full P2.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
PROJECT = CORE.parents[1]
RUNS = PROJECT / "ENGINE TEST STUBE" / "runs"
NATIVE = RUNS / "p2_marin_footprint_native.txt"
BUILD = RUNS / "p2_marin_footprint_build.txt"
FULL_BUILD = RUNS / "p2_marin_footprint_full_build.txt"
CTEST = RUNS / "p2_marin_footprint_ctest.txt"
GATE = RUNS / "p2_marin_footprint_phase_gate.txt"
BINARY = CORE / "build" / "upc_p2_marin_footprint_tests.exe"
OUTPUT = RUNS / "p2_marin_footprint_partial.json"
SOURCE = (
    "include/upc/p2_marin_footprint_pressure3d.hpp",
    "include/upc/p2_marin_pressure_sensors3d.hpp",
    "include/upc/coupled_water3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_marin_footprint_pressure3d_tests.cpp",
    "scripts/p2_marin_footprint_partial_acceptance.py",
)
LINE = re.compile(
    r"NATIVE_MAC_FOOTPRINT dx=(0\.1|0\.05) wet=(\d+) "
    r"p1_pa=([+-]?\d+(?:\.\d+)?) p5_pa=([+-]?\d+(?:\.\d+)?)"
)
PASS = re.compile(
    r"PASS P2_MARIN_FOOTPRINT assertions=(\d+) "
    r"radius_m_is_unverified=true phase_complete=false"
)

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def parse_native(log: str) -> dict:
    lines = log.strip().splitlines()
    if len(lines) != 3 or "FAIL" in log:
        raise ValueError("Native log length or failure invalid")
    grids = {}
    for line in lines[:2]:
        m = LINE.fullmatch(line.strip())
        if not m:
            raise ValueError("Missing native MAC footprint grid")
        dx, wet, p1, p5 = m.groups()
        if dx in grids or int(wet) != 8:
            raise ValueError("Duplicate grid or missing wet pressure probe")
        p1, p5 = float(p1), float(p5)
        if not all(map(math.isfinite, (p1, p5))):
            raise ValueError("Nonfinite pressure")
        grids[dx] = {"wet": int(wet), "p1_pa": p1, "p5_pa": p5}
    if set(grids) != {"0.1", "0.05"}:
        raise ValueError("Both independent native grid sizes required")
    m = PASS.fullmatch(lines[2].strip())
    if not m or int(m.group(1)) < 140:
        raise ValueError("Missing native assertions or false P2 claim")
    return {"grids": grids, "native_assertions": int(m.group(1))}

def audit() -> dict:
    if not all(p.is_file() and p.stat().st_size > 0
               for p in (NATIVE, BUILD, FULL_BUILD, CTEST, GATE, BINARY)):
        raise RuntimeError("Native MSVC build/binary/trace missing")
    gate_bytes = GATE.read_bytes()
    gate_log = gate_bytes.decode("utf-16" if gate_bytes.startswith(bytes((255,254)))
                                 else "utf-8-sig")
    if "P2" not in gate_log or "ALL_COMPLETE=False" not in gate_log or \
       "GATES_INCOMPLETE=" not in gate_log:
        raise RuntimeError("Phase gate did not confirm P2 incomplete")
    ctest_log = CTEST.read_text(encoding="utf-8-sig")
    match = re.search(r"100% tests passed, 0 tests failed out of (\d+)", ctest_log)
    if not match or int(match.group(1)) < 123 or not re.search(
        r"physics_p2_marin_footprint_partial\s+\.+\s+Passed", ctest_log):
        raise RuntimeError("Full native CTest or footprint registration missing")
    native = NATIVE.read_text(encoding="utf-8-sig")
    result = parse_native(native)
    mutants = (
        native.replace("phase_complete=false", "phase_complete=true"),
        native.replace("radius_m_is_unverified=true", "radius_m_is_unverified=false"),
        native.replace("dx=0.05", "dx=0.1"),
        native.replace("wet=8", "wet=0", 1),
        native.replace("assertions=145", "assertions=3"),
        native.replace("p1_pa=4424.33", "p1_pa=nan"),
    )
    checks = [
        {"name": "real_native_3d_mac_two_grid_pressure", "pass": True},
        {"name": "full_native_ctest_all_suites", "pass": int(match.group(1)) >= 123},
        {"name": "official_phase_gate_remains_incomplete", "pass": True},
        {"name": "manufactured_disk_affine_quadratic_masks_and_negative_tests",
         "pass": result["native_assertions"] >= 140},
        {"name": "native_output_disclaims_unknown_sensor_radius", "pass": True},
    ]
    for i, bad in enumerate(mutants):
        rejected = False
        try:
            parse_native(bad)
        except ValueError:
            rejected = True
        checks.append({"name": f"reject_corrupted_native_trace_{i+1}",
                       "pass": rejected})
    if not all(c["pass"] for c in checks):
        raise RuntimeError("Footprint independent QA mutation rejected insufficiently")
    evidence = {
        "phase": "P2",
        "milestone": "finite_circular_marin_pressure_footprint_sampling_partial",
        "partial_accepted": True,
        "phase_complete": False,
        "real_sensor_radius_known": False,
        "experimental_pressure_validated": False,
        "full_6s_measured_e2e": False,
        "source_sha256": {name: sha(CORE/name) for name in SOURCE},
        "native_binary_sha256": sha(BINARY),
        "build_log_sha256": sha(BUILD),
        "full_build_log_sha256": sha(FULL_BUILD),
        "ctest_log_sha256": sha(CTEST),
        "phase_gate_log_sha256": sha(GATE),
        "ctest_passed_suites": int(match.group(1)),
        "native_log_sha256": sha(NATIVE),
        "native_result": result,
        "checks": checks,
        "known_open": [
            "Determine original SPHERIC Test 02 P1-P8 sensing area from a verified source",
            "Resolve wall geometry/traction and stress-consistent water-air boundary",
            "Run native full >=6s three-grid MARIN H1-H4/P1-P8 validation",
            "Prove dynamic two-way FSI and physical mass/momentum/energy closure",
        ],
        "generated_utc": datetime.now(timezone.utc).isoformat(),
    }
    OUTPUT.write_text(json.dumps(evidence, indent=2, ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print("P2_FOOTPRINT_PARTIAL_PASS=True MUTATION_NEGATIVE_PASS=6 "
          "EXPERIMENTAL_VALIDATION=False PHASE_COMPLETE=False")
    return evidence

if __name__ == "__main__":
    audit()
