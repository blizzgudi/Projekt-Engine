"""Independent P2 PARTIAL QA for manufactured two-phase normal stress on native MAC-PCG.
Never promotes a bounded synthetic test to full P2 acceptance.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
ROOT = CORE.parents[1]
RUNS = ROOT / "ENGINE TEST STUBE" / "runs"
PREFIX = "p2_sdf_normal_stress"
NATIVE = RUNS / (PREFIX + "_native.txt")
BUILD = RUNS / (PREFIX + "_build.txt")
CTEST = RUNS / (PREFIX + "_ctest.txt")
GATE_LOG = RUNS / (PREFIX + "_phase_gate.txt")
BINARY = CORE / "build" / "upc_p2_sdf_normal_stress_tests.exe"
REPORT = RUNS / (PREFIX + "_partial.json")
SOURCES = [
    "CMakeLists.txt", "include/upc/mac3d.hpp",
    "include/upc/p2_sdf_capillary3d.hpp",
    "include/upc/p2_sdf_normal_stress3d.hpp",
    "tests/p2_sdf_normal_stress3d_tests.cpp",
    "scripts/p2_sdf_normal_stress_build.cmd",
    "scripts/p2_sdf_normal_stress_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_SDF_NORMAL_STRESS_PARTIAL.md", "docs/PHASENPLAN.md",
]
PLANE = re.compile(
    r"NORMAL_STRESS_PLANE faces=(\d+) viscous_jump_pa=([0-9.eE+-]+) "
    r"max_speed_m_s=([0-9.eE+-]+) max_pressure_error_pa=([0-9.eE+-]+) "
    r"divergence_s_inv=([0-9.eE+-]+)"
)
SPHERE = re.compile(
    r"NORMAL_STRESS_SPHERE dx=([0-9.eE+-]+) faces=(\d+) "
    r"max_radial_jump_error_pa=([0-9.eE+-]+) max_skew_error_pa=([0-9.eE+-]+)"
)
NEG = re.compile(r"NORMAL_STRESS_NEGATIVE rejected=(\d+) zero_viscosity_faces=(\d+)")
PASS = re.compile(r"PASS P2_SDF_NORMAL_STRESS assertions=(\d+) phase_complete=false")

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def parse_native(log: str) -> dict:
    lines = log.strip().splitlines()
    if len(lines) != 5:
        raise ValueError("Exactly five native trace rows required")
    plane = PLANE.fullmatch(lines[0])
    spheres = [SPHERE.fullmatch(line) for line in lines[1:3]]
    negative = NEG.fullmatch(lines[3])
    passed = PASS.fullmatch(lines[4])
    if not plane or not all(spheres) or not negative or not passed:
        raise ValueError("Malformed or missing native evidence")
    faces = int(plane[1])
    jump, speed, pressure_err, divergence = map(float, plane.groups()[1:])
    expected = 2 * (2 * .3 - .5 * (-.1))
    if faces != 144 or not all(math.isfinite(v) for v in
                                (jump, speed, pressure_err, divergence)):
        raise ValueError("Invalid planar normal-stress result")
    if abs(jump - expected) > 1e-12 or not (0 <= speed < 1e-9) or not (
        0 <= pressure_err < 1e-7) or not (0 <= divergence < 1e-9):
        raise ValueError("Planar stress jump or native MAC-PCG equilibrium failed")
    rows = []
    for match, grid, min_faces, max_err in zip(
        spheres, (.08, .04), (250, 950), (.04, .015)
    ):
        dx, count, err, skew = map(float, match.groups())
        if not all(math.isfinite(v) for v in (dx, count, err, skew)):
            raise ValueError("Nonfinite spherical stress evidence")
        if abs(dx - grid) > 1e-12 or count < min_faces or (
            not 0 <= err < max_err) or not (0 <= skew < 1e-11):
            raise ValueError("3D sphere stress reference failed")
        rows.append(dict(dx_m=dx, faces=int(count), max_radial_jump_error_pa=err,
                         max_skew_error_pa=skew))
    if rows[0]["max_radial_jump_error_pa"] / rows[1]["max_radial_jump_error_pa"] < 2:
        raise ValueError("Grid refinement did not improve radial normal stress")
    if int(negative[1]) != 5 or int(negative[2]) != 144:
        raise ValueError("Native negative/zero-viscosity tests missing")
    if int(passed[1]) < 5000:
        raise ValueError("Native assertions incomplete")
    return dict(plane_faces=faces, plane_viscous_jump_pa=jump,
                plane_max_speed_m_s=speed, plane_pressure_error_pa=pressure_err,
                plane_divergence_s_inv=divergence, spheres=rows,
                native_assertions=int(passed[1]), rejected_inputs=5)

def main() -> None:
    native_text = NATIVE.read_text(encoding="utf-8")
    evidence = parse_native(native_text)
    build = BUILD.read_text(encoding="utf-8")
    ctest = CTEST.read_text(encoding="utf-8")
    if "Linking CXX executable upc_p2_sdf_normal_stress_tests.exe" not in build:
        raise ValueError("No native MSVC target build proof")
    if "100% tests passed, 0 tests failed out of 129" not in ctest:
        raise ValueError("Full native 129-suite CTest regression not proven")
    repeat = subprocess.run([str(BINARY)], capture_output=True, text=True, timeout=60)
    if repeat.returncode != 0 or repeat.stdout.strip() != native_text.strip():
        raise ValueError("Independent native binary rerun differs")
    corrupted = 0
    for before, after in (
        ("phase_complete=false", "phase_complete=true"),
        ("viscous_jump_pa=1.3", "viscous_jump_pa=2.0"),
        ("faces=144", "faces=0"),
        ("max_radial_jump_error_pa=0.0311568", "max_radial_jump_error_pa=2"),
        ("rejected=5", "rejected=0"),
        ("assertions=5871", "assertions=4"),
    ):
        if before not in native_text:
            raise ValueError("Missing expected native corruption anchor: " + before)
        try:
            parse_native(native_text.replace(before, after, 1))
        except ValueError:
            corrupted += 1
        else:
            raise ValueError("Tampered native trace incorrectly accepted")
    historical = RUNS / "p2_marin_pcg_6s_partial.json"
    marin = json.loads(historical.read_text(encoding="utf-8"))
    if marin.get("height_rmse_threshold_met") is not False or (
        marin.get("phase_complete") is not False):
        raise ValueError("Historical 6-second MARIN height FAIL not preserved")
    gate = subprocess.run([sys.executable, str(CORE / "scripts" / "phase_gate.py")],
                          capture_output=True, text=True, timeout=240)
    GATE_LOG.write_text(gate.stdout + gate.stderr, encoding="utf-8")
    if gate.returncode != 0 or "ALL_COMPLETE=False" not in gate.stdout or (
        "P2" not in gate.stdout.split("GATES_INCOMPLETE=", 1)[-1].splitlines()[0]):
        raise ValueError("Official phase gate did not preserve P2 incomplete")
    report = {
        "phase": "P2", "milestone": "prescribed_two_phase_viscous_normal_stress_partial",
        "partial_accepted": True, "phase_complete": False,
        "native": evidence, "ctest_passed": 129,
        "tampered_native_logs_rejected": corrupted,
        "official_phase_gate": "P2 INCOMPLETE / ALL_COMPLETE=False",
        "historical_marin_6s_height_validation": "FAIL",
        "historical_marin_report_sha256": sha(historical),
        "limitations": [
            "Prescribed liquid/air gradients; no resolved dynamic two-phase stress field",
            "No tangential traction/Marangoni or VOF-to-SDF curvature transport",
            "Historical 6-second MARIN H1-H4 FAIL; P1-P8 pressure/three-grid convergence absent",
            "No dynamic bidirectional FSI or global physical momentum/energy closure",
            "No Unreal 5.8 native E2E for this new partial operator",
        ],
        "source_sha256": {name: sha(CORE / name) for name in SOURCES},
        "binary_sha256": sha(BINARY),
        "native_log_sha256": sha(NATIVE),
        "build_log_sha256": sha(BUILD),
        "ctest_log_sha256": sha(CTEST),
        "phase_gate_log_sha256": sha(GATE_LOG),
        "generated_utc": datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"P2 NORMAL STRESS PARTIAL PASS: {evidence['native_assertions']} native assertions; "
          f"129/129 CTest; {corrupted} forged logs rejected; P2_FULL=false")

if __name__ == "__main__":
    main()
