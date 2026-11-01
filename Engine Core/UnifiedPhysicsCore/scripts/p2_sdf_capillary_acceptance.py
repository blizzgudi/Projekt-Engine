"""Independent native QA for a bounded 3D SDF/Young-Laplace P2 milestone.
No P2 full-phase acceptance is inferred from this test.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
ROOT = CORE.parents[1]
RUNS = ROOT / "ENGINE TEST STUBE" / "runs"
NATIVE = RUNS / "p2_sdf_capillary_native.txt"
CTEST = RUNS / "p2_sdf_capillary_ctest.txt"
BUILD = RUNS / "p2_sdf_capillary_build.txt"
BINARY = CORE / "build" / "upc_p2_sdf_capillary_tests.exe"
REPORT = RUNS / "p2_sdf_capillary_partial.json"
SOURCES = [
    "CMakeLists.txt",
    "include/upc/mac3d.hpp",
    "include/upc/p2_sdf_capillary3d.hpp",
    "tests/p2_sdf_capillary3d_tests.cpp",
    "scripts/p2_sdf_capillary_build.cmd",
    "scripts/p2_sdf_capillary_acceptance.py",
    "docs/P2_SDF_CAPILLARY_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
]
GEOM = re.compile(
    r"CAPILLARY_GEOMETRY shape=(sphere|cylinder) dx=([0-9.eE+-]+) "
    r"faces=(\d+) mean_curvature=([0-9.eE+-]+) "
    r"expected=([0-9.eE+-]+) max_rel_error=([0-9.eE+-]+)"
)
PCG = re.compile(
    r"CAPILLARY_PCG divergence_s_inv=([0-9.eE+-]+) "
    r"max_x_face_speed_m_s=([0-9.eE+-]+) iterations=(\d+)"
)
PLANE = re.compile(r"CAPILLARY_PLANE faces=(\d+) divergence_s_inv=([0-9.eE+-]+)")
PASS = re.compile(r"PASS P2_SDF_CAPILLARY assertions=(\d+) phase_complete=false")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_native(log: str) -> dict:
    lines = log.strip().splitlines()
    if len(lines) != 7:
        raise ValueError("Expected exactly four geometry, one PCG, one plane and one PASS row")
    matches = [GEOM.fullmatch(lines[i]) for i in (0, 2, 3, 4)]
    if any(m is None for m in matches):
        raise ValueError("Malformed manufactured curvature row")
    rows = []
    for m, (shape, dx) in zip(matches, (
        ("sphere", .08), ("sphere", .04), ("cylinder", .08), ("cylinder", .04)
    )):
        kind, grid, faces, mean, expected, maxerr = m.groups()
        grid, faces, mean, expected, maxerr = float(grid), int(faces), float(mean), float(expected), float(maxerr)
        reference = (2 if shape == "sphere" else 1) / .287
        if kind != shape or abs(grid - dx) > 1e-12 or faces < 100:
            raise ValueError("Missing independent sphere/cylinder grid")
        if not all(math.isfinite(v) for v in (mean, expected, maxerr)):
            raise ValueError("Nonfinite curvature evidence")
        if abs(expected - reference) > 1e-5 or abs(mean - reference) / reference > (.01 if dx == .08 else .004):
            raise ValueError("Mean curvature does not agree with analytic 3D surface")
        if maxerr > (.025 if dx == .08 else .006) or maxerr < 0:
            raise ValueError("Maximum discrete curvature error too large")
        rows.append(dict(shape=shape, dx_m=dx, faces=faces, mean_inv_m=mean,
                         analytic_inv_m=reference, max_relative_error=maxerr))
    if rows[0]["max_relative_error"] <= 3 * rows[1]["max_relative_error"]:
        raise ValueError("Sphere grid refinement did not improve by factor >=3")
    if rows[2]["max_relative_error"] <= 3 * rows[3]["max_relative_error"]:
        raise ValueError("Cylinder grid refinement did not improve by factor >=3")
    p, plane, passed = PCG.fullmatch(lines[1]), PLANE.fullmatch(lines[5]), PASS.fullmatch(lines[6])
    if not p or not plane or not passed:
        raise ValueError("Missing native PCG, planar or explicit PARTIAL marker")
    divergence, spurious, iterations = float(p[1]), float(p[2]), int(p[3])
    if not math.isfinite(divergence) or not 0 <= divergence < 1e-7:
        raise ValueError("Capillary PCG divergence failed")
    if not math.isfinite(spurious) or not 0 <= spurious < 1e-5:
        raise ValueError("Capillary spurious velocity exceeds manufactured-case bound")
    if not 0 < iterations < 3000:
        raise ValueError("No converged capillary PCG iterations")
    if int(plane[1]) != 144 or abs(float(plane[2])) > 1e-9:
        raise ValueError("Planar zero-curvature pressure projection invalid")
    if int(passed[1]) < 9000:
        raise ValueError("Missing native negative/geometry assertions")
    return dict(manufactured=rows, capillary_pcg_divergence_s_inv=divergence,
                capillary_pcg_max_x_velocity_m_s=spurious,
                capillary_pcg_iterations=iterations,
                planar_interface_faces=int(plane[1]),
                native_assertions=int(passed[1]))


def main() -> None:
    log = NATIVE.read_text(encoding="utf-8")
    native = validate_native(log)
    ctest = CTEST.read_text(encoding="utf-8")
    if "100% tests passed, 0 tests failed out of 128" not in ctest:
        raise ValueError("Native 128-suite regression not proven")
    build = BUILD.read_text(encoding="utf-8")
    if "Linking CXX executable upc_p2_sdf_capillary_tests.exe" not in build:
        raise ValueError("New native MSVC target was not built")
    repeat = subprocess.run([str(BINARY)], capture_output=True, text=True, timeout=40)
    if repeat.returncode != 0 or repeat.stdout.strip() != log.strip():
        raise ValueError("Independent rerun differs from native build evidence")
    rejected = 0
    for before, after in (
        ("phase_complete=false", "phase_complete=true"),
        ("assertions=9142", "assertions=100"),
        ("shape=sphere", "shape=cube"),
        ("max_rel_error=0.0146074", "max_rel_error=0.9"),
        ("faces=144", "faces=0"),
        ("max_x_face_speed_m_s=1.94287e-07", "max_x_face_speed_m_s=1"),
    ):
        if before not in log:
            raise ValueError(f"Expected corruption anchor missing: {before}")
        try:
            validate_native(log.replace(before, after, 1))
        except ValueError:
            rejected += 1
        else:
            raise ValueError(f"Corrupted native evidence accepted: {before}")
    marin_path = RUNS / "p2_marin_pcg_6s_partial.json"
    marin = json.loads(marin_path.read_text(encoding="utf-8"))
    if marin.get("height_rmse_threshold_met") is not False or marin.get("phase_complete") is not False:
        raise ValueError("Previously failed measured 6-second MARIN evidence unexpectedly changed")
    report = {
        "phase": "P2", "milestone": "sdf_young_laplace_mac_pcg_partial",
        "partial_accepted": True, "phase_complete": False,
        "native": native, "ctest_passed": 128, "tampered_native_logs_rejected": rejected,
        "historical_marin_6s_height_validation": "FAIL",
        "historical_marin_report_sha256": sha(marin_path),
        "missing": [
            "No VOF-to-SDF curvature reconstruction or live evolving capillary interface",
            "No tangential/normal two-phase viscous stress or resolved air dynamics",
            "No verified 6-second MARIN H1-H4/P1-P8 three-grid convergence",
            "No bidirectional dynamic solid/air coupling or global physical energy balance",
            "No UE5.8 Native-E2E for this capillary path"
        ],
        "source_sha256": {name: sha(CORE / name) for name in SOURCES},
        "binary_sha256": sha(BINARY),
        "native_log_sha256": sha(NATIVE),
        "build_log_sha256": sha(BUILD),
        "ctest_log_sha256": sha(CTEST),
        "generated_utc": datetime.now(timezone.utc).isoformat()
    }
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"P2 SDF CAPILLARY PARTIAL PASS: {native['native_assertions']} native assertions; "
          f"128/128 CTest; {rejected} forged logs rejected; P2_FULL=false")


if __name__ == "__main__":
    main()
