"""Independent P2 MARIN Test-02 cut-cell geometry audit; never full P2 PASS.

Checks real MSVC native binary, exact rational geometry and negative tamper
controls. Does NOT rerun the expensive six-second three-grid CFD benchmark.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from fractions import Fraction as F
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
ROOT = CORE.parents[1]
RUNS = ROOT / "ENGINE TEST STUBE" / "runs"
EXE = CORE / "build" / "upc_p2_marin_obstacle_cut_geometry_tests.exe"
NATIVE = RUNS / "p2_marin_box_geom_native_20261102.log"
BUILD = RUNS / "p2_marin_box_geom_build_20261102.log"
CTEST = RUNS / "p2_marin_box_geom_ctest_20261102.log"
LEGACY = RUNS / "p2_marin_pcg_full6s_three_grid_partial.json"
GRIDS = {"coarse": (32, 10, 10, F(1, 10)),
         "medium": (48, 15, 15, F(1, 15)),
         "fine": (64, 20, 20, F(1, 20))}
LO = (F("0.6635"), F("0.295"), F(0))
HI = (F("0.8245"), F("0.698"), F("0.161"))
NUM = r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"
LINE = re.compile(
    rf"^MARIN_BOX_GEOM grid=(coarse|medium|fine) dx=({NUM}) "
    rf"max_face_area_residual_m2=({NUM}) analytic_m3=({NUM}) "
    rf"cut_m3=({NUM}) binary_m3=({NUM}) binary_error_m3=({NUM}) "
    r"cut_cells=(\d+) unresolved_sensors=(\d+)$"
)
END = re.compile(
    r"^PASS MARIN_BOX_GEOMETRY assertions=(\d+) "
    r"binary_obstacle_unresolved=true production_cut_cell_integrated=false "
    r"phase_complete=false$"
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def independent_geometry(nx: int, ny: int, nz: int, dx: F):
    # Rational arithmetic, independent of native C++ floating-point overlap.
    binary_count = cut_count = 0
    for z in range(nz):
        for y in range(ny):
            for x in range(nx):
                ijk = (x, y, z)
                overlaps = [max(F(0), min((v+1)*dx, HI[a]) -
                                max(v*dx, LO[a])) for a, v in enumerate(ijk)]
                solid = overlaps[0]*overlaps[1]*overlaps[2]
                if 0 < solid < dx**3:
                    cut_count += 1
                centres = [(v + F(1, 2))*dx for v in ijk]
                if all(LO[a] <= centres[a] < HI[a] for a in range(3)):
                    binary_count += 1
    y = F("0.471")
    offsets = (F(".021"), F(".061"), F(".101"), F(".141"))
    sensors = [(HI[0], y, h) for h in offsets]
    sensors += [(HI[0]-h, y, HI[2]) for h in offsets]
    unresolved = 0
    for xyz in sensors:
        ijk = tuple(int(v // dx) for v in xyz)
        if not all(0 <= ijk[a] < (nx, ny, nz)[a] for a in range(3)):
            raise AssertionError("Reference transducer outside domain")
        overlap = [max(F(0), min((ijk[a]+1)*dx, HI[a]) -
                       max(ijk[a]*dx, LO[a])) for a in range(3)]
        if math.prod(overlap) <= 0:
            raise AssertionError("Physical transducer does not intersect obstacle")
        centre = tuple((ijk[a] + F(1, 2))*dx for a in range(3))
        if not all(LO[a] <= centre[a] < HI[a] for a in range(3)):
            unresolved += 1
    return {
        "analytic_m3": math.prod(HI[a]-LO[a] for a in range(3)),
        "binary_m3": binary_count*dx**3,
        "cut_cells": cut_count,
        "unresolved_sensors": unresolved,
    }


def parse(raw: str):
    lines = raw.strip().splitlines()
    if len(lines) != 4:
        raise ValueError("Native geometry trace must have three grids and final marker")
    match = END.fullmatch(lines[-1])
    if not match or int(match.group(1)) < 40000:
        raise ValueError("Native assertions or conservative partial-only marker invalid")
    rows = {}
    for line in lines[:3]:
        m = LINE.fullmatch(line)
        if m is None:
            raise ValueError("Native geometry row malformed")
        name = m.group(1)
        if name in rows:
            raise ValueError("Duplicate native geometry grid")
        nx, ny, nz, dx = GRIDS[name]
        expected = independent_geometry(nx, ny, nz, dx)
        got = [float(m.group(i)) for i in range(2, 8)]
        if not all(math.isfinite(x) for x in got):
            raise ValueError("Nonfinite geometry measurement")
        got_dx, face_res, analytic, cut, binary, error = got
        if abs(got_dx-float(dx)) > 1e-14:
            raise ValueError("Grid resolution mismatch")
        if face_res < 0 or face_res > 2e-13:
            raise ValueError("MAC face analytical cross-section failed")
        if abs(analytic-float(expected["analytic_m3"])) > 1e-12:
            raise ValueError("Analytical physical obstacle volume mismatch")
        if abs(cut-analytic) > 2e-13:
            raise ValueError("Cut volume conservation failed")
        if abs(binary-float(expected["binary_m3"])) > 2e-13:
            raise ValueError("Binary centre-sampling reference mismatch")
        if abs(error-(binary-analytic)) > 2e-13:
            raise ValueError("Native binary volume error inconsistent")
        if int(m.group(8)) != expected["cut_cells"]:
            raise ValueError("Partial cut-cell count inconsistent")
        if int(m.group(9)) != expected["unresolved_sensors"]:
            raise ValueError("Unresolved sensor host count inconsistent")
        if abs(error) <= 1e-5 or int(m.group(9)) <= 0:
            raise ValueError("Expected binary wall geometry blocker missing")
        rows[name] = {
            "shape": [nx, ny, nz],
            "cell_size_m": float(dx),
            "analytic_obstacle_m3": analytic,
            "cut_cell_obstacle_m3": cut,
            "binary_obstacle_m3": binary,
            "binary_volume_error_m3": error,
            "binary_volume_error_percent": 100*error/analytic,
            "partial_cut_cells": int(m.group(8)),
            "unresolved_pressure_sensor_host_cells": int(m.group(9)),
            "max_analytic_mac_face_area_residual_m2": face_res,
        }
    if set(rows) != set(GRIDS):
        raise ValueError("Missing independent three-grid native geometry")
    return rows


def audit():
    for file in (EXE, NATIVE, BUILD, CTEST, LEGACY):
        if not file.is_file():
            raise RuntimeError(f"Required native evidence missing: {file}")
    ctest = CTEST.read_text(encoding="utf-8-sig", errors="replace")
    if not re.search(r"100% tests passed, 0 tests failed out of 151", ctest):
        raise RuntimeError("Full 151-suite MSVC CTest not verified")
    native_text = NATIVE.read_text(encoding="utf-8-sig")
    original = parse(native_text)
    p = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                       text=True, timeout=30, check=False)
    if p.returncode != 0 or p.stderr.strip():
        raise RuntimeError(f"Independent native rerun failed: {p.stderr[:200]}")
    rerun = parse(p.stdout)
    if rerun != original or p.stdout.strip() != native_text.strip():
        raise RuntimeError("Native rerun differs from hashed stored evidence")
    # Every tamper must be caught by the independent parser.
    lines = native_text.strip().splitlines()
    mutations = {
        "false_phase_pass": native_text.replace("phase_complete=false",
                                                "phase_complete=true"),
        "false_production_integration": native_text.replace(
            "production_cut_cell_integrated=false",
            "production_cut_cell_integrated=true"),
        "forged_binary_volume": native_text.replace("binary_m3=0.008",
                                                     "binary_m3=0.009", 1),
        "forged_cut_volume": native_text.replace(
            "cut_m3=0.010446163000000003", "cut_m3=0.012446163000000003", 1),
        "forged_face_area": native_text.replace(
            "max_face_area_residual_m2=1.3877787807814457e-17",
            "max_face_area_residual_m2=0.0001", 1),
        "forged_sensor_count": native_text.replace(
            "unresolved_sensors=6", "unresolved_sensors=0", 1),
        "forged_partial_count": native_text.replace(
            "cut_cells=27", "cut_cells=26", 1),
        "forged_grid_dx": native_text.replace(
            "dx=0.10000000000000001", "dx=0.2", 1),
        "forged_binary_error": native_text.replace(
            "binary_error_m3=-0.0024461629999999995",
            "binary_error_m3=0.0", 1),
        "missing_grid": "\n".join(lines[:1]+lines[2:])+"\n",
        "duplicate_grid": "\n".join([lines[0], lines[0], lines[2], lines[3]])+"\n",
        "forged_assertion_count": native_text.replace(
            "assertions=165073", "assertions=1"),
        "nan_geometry": native_text.replace(
            "analytic_m3=0.010446163000000001", "analytic_m3=nan", 1),
    }
    for name, forged in mutations.items():
        if forged == native_text:
            raise RuntimeError(f"Negative control did not mutate trace: {name}")
        try:
            parse(forged)
        except (ValueError, KeyError):
            continue
        raise RuntimeError(f"Forged native geometry accepted: {name}")
    legacy = json.loads(LEGACY.read_text(encoding="utf-8"))
    if legacy.get("experimental_three_grid_cfd_validated") is not False:
        raise RuntimeError("Historical physical measurement failure was overwritten")
    rmse = {f"H{i}": legacy["grids"]["fine"]["experimental_metrics"]
            [f"H{i}_m"]["rmse"] for i in range(1, 5)}
    if not all(value > .05 for value in rmse.values()):
        raise RuntimeError("Original failed H1-H4 threshold was silently changed")
    source_files = (
        "CMakeLists.txt",
        "include/upc/p2_moving_box_open_volume3d.hpp",
        "include/upc/p2_moving_box_mac_apertures3d.hpp",
        "include/upc/p2_marin_pressure_sensors3d.hpp",
        "tests/p2_marin_obstacle_cut_geometry3d_tests.cpp",
        "scripts/p2_marin_box_geometry_native.cmd",
        "scripts/p2_marin_box_geometry_audit.py",
        "docs/P2_MARIN_OBSTACLE_CUT_GEOMETRY_PARTIAL.md",
        "docs/PHASENPLAN.md",
    )
    report = {
        "phase": "P2",
        "milestone": "real_marin_test02_cut_cell_obstacle_volume_and_mac_face_area_diagnostic",
        "geometry_partial_accepted": True,
        "phase_complete": False,
        "full_six_second_experimental_cfd_validated": False,
        "production_marin_cut_cell_integrated": False,
        "native_ctest_suites": 151,
        "native_assertions": 165073,
        "independent_native_rerun": True,
        "forged_native_logs_rejected": len(mutations),
        "negative_case_names": list(mutations),
        "grids": original,
        "original_full6s_experiment_sha256": sha(LEGACY),
        "unchanged_fine_grid_H1_H4_rmse_m": rmse,
        "native_binary_sha256": sha(EXE),
        "native_build_sha256": sha(BUILD),
        "native_log_sha256": sha(NATIVE),
        "full_ctest_sha256": sha(CTEST),
        "source_sha256": {f: sha(CORE/f) for f in source_files},
        "known_negative_results": [
            "Binary production MARIN obstacle underestimates real box volume on all three grids",
            "6 to 8 pressure sensor host cells have no binary solid despite physical box intersection",
            "Exact isolated cut-cell and MAC face geometry not wired into live MARIN MAC/VOF pressure solver",
            "Existing full six-second H1-H4 laboratory RMSE exceeds unchanged 0.05m threshold on fine grid",
            "No measured P1-P8 pressure convergence, dynamic bidirectional FSI or full Unreal 5.8 native E2E",
        ],
        "generated_utc": datetime.now(timezone.utc).isoformat(),
    }
    out = RUNS / "p2_marin_box_geom_partial.json"
    out.write_text(json.dumps(report, indent=2, ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print("P2_MARIN_BOX_GEOMETRY_PARTIAL=True PHASE_COMPLETE=False")
    for name, data in original.items():
        print(name, "binary_error_percent",
              round(data["binary_volume_error_percent"], 4),
              "unresolved_sensors", data["unresolved_pressure_sensor_host_cells"])
    print("NATIVE_ASSERTIONS", report["native_assertions"],
          "CTEST", report["native_ctest_suites"],
          "FORGED_LOGS_REJECTED", len(mutations))
    return report


if __name__ == "__main__":
    audit()
