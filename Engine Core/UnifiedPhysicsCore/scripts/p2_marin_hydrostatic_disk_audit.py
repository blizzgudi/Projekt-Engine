"""Independent P2 PARTIAL QA: exact MARIN hydrostatic sensor-disk geometry.

No full P2 gate, no fabricated laboratory validation, no new 6s CFD run.
"""
from __future__ import annotations
import copy
import hashlib
import json
import math
import re
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
ROOT = CORE.parents[1]
RUNS = ROOT / "ENGINE TEST STUBE" / "runs"
NATIVE = RUNS / "p2_hydrostatic_disk_native_20261103.log"
BUILD = RUNS / "p2_hydrostatic_disk_build_20261103.log"
CTEST = RUNS / "p2_hydrostatic_disk_ctest_20261103.log"
OLD = RUNS / "p2_marin_pcg_full6s_three_grid_partial.json"
REPORT = RUNS / "p2_marin_hydrostatic_disk_partial.json"
EXE = CORE / "build" / "upc_p2_marin_hydrostatic_disk_tests.exe"
SOURCES = [
    "CMakeLists.txt",
    "include/upc/p2_marin_hydrostatic_disk3d.hpp",
    "include/upc/p2_marin_footprint_pressure3d.hpp",
    "include/upc/p2_marin_pressure_sensors3d.hpp",
    "tests/p2_marin_hydrostatic_disk3d_tests.cpp",
    "scripts/p2_marin_hydrostatic_disk_native.cmd",
    "scripts/p2_marin_hydrostatic_disk_audit.py",
]
ARTIFACTS = {
    "native_log": NATIVE,
    "build_log": BUILD,
    "ctest_log": CTEST,
    "native_binary": EXE,
    "historical_lab_report": OLD,
    "official_gate_readonly": RUNS / "p2_hydrostatic_disk_gate_readonly_20261103.log",
}
GRID = {32: (32, 10, 10), 48: (48, 15, 15), 64: (64, 20, 20)}
PATTERN = re.compile(
    r"DISK_NATIVE nx=(\d+) dx=([\d.e+-]+) roof_exact_pa=([\d.e+-]+) "
    r"roof_current_pa=([\d.e+-]+) roof_error_pa=([-\d.e+]+) "
    r"roof_current_wet=([\d.e+-]+)"
)
def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def near(a: float, b: float, tol: float, what: str) -> None:
    if not math.isfinite(a) or abs(a - b) > tol:
        raise AssertionError(f"{what}: {a} vs {b} tol={tol}")

def validate(report: dict) -> None:
    if report.get("phase") != "P2" or report.get("phase_complete") is not False:
        raise AssertionError("forged full phase")
    if report.get("measured_validation") is not False or report.get("historical_lab_fail") is not True:
        raise AssertionError("forged laboratory pass")
    if report.get("native_assertions") != 299 or report.get("ctest_suites") != 156:
        raise AssertionError("native test counts changed")
    if set(report.get("source_sha256", {})) != set(SOURCES):
        raise AssertionError("source coverage changed")
    for name, expected in report["source_sha256"].items():
        if sha(CORE / name) != expected:
            raise AssertionError("source SHA mismatch: " + name)
    if set(report.get("artifact_sha256", {})) != set(ARTIFACTS):
        raise AssertionError("artifact coverage changed")
    for name, expected in report["artifact_sha256"].items():
        if sha(ARTIFACTS[name]) != expected:
            raise AssertionError("artifact SHA mismatch: " + name)
    gate = ARTIFACTS["official_gate_readonly"].read_text(encoding="utf-8-sig")
    if "P2_OFFICIAL_FULL_SCOPE_STATUS=PARTIAL" not in gate or "GATE_REPORT_NOT_WRITTEN=True" not in gate:
        raise AssertionError("official full-scope gate was misrepresented")
    ctest = CTEST.read_text(encoding="utf-8", errors="replace")
    if "100% tests passed, 0 tests failed out of 156" not in ctest:
        raise AssertionError("not a complete 156-test native CTest")
    native = NATIVE.read_text(encoding="utf-8", errors="replace")
    if "PASS P2_MARIN_HYDROSTATIC_DISK assertions=299 measured_validation=false phase_complete=false" not in native:
        raise AssertionError("native PASS is missing or incorrectly promoted")
    if "DISK_ANALYTIC assertions=290 half_wet=0.5 half_pressure_pa=20.8175" not in native:
        raise AssertionError("native independent numerical integration absent")
    old = json.loads(OLD.read_text(encoding="utf-8"))
    if old.get("complete_phase") is not False or old.get("experimental_three_grid_cfd_validated") is not False:
        raise AssertionError("historical MARIN gate unexpectedly promoted")
    if old.get("full_six_second_height_rmse_target_m") != 0.05:
        raise AssertionError("MARIN RMSE target was changed")
    for key in ("H1", "H2", "H3", "H4"):
        if old["fine_height_rmse_m"][key + "_m"] <= 0.05:
            raise AssertionError("historical height FAIL was concealed")
    rows = report.get("manufactured_native", [])
    if len(rows) != 3 or [x["nx"] for x in rows] != [32, 48, 64]:
        raise AssertionError("missing or duplicated independent grids")
    matches = list(PATTERN.finditer(native))
    if len(matches) != 3:
        raise AssertionError("native grid rows missing")
    expected = 1000. * 9.81 * (.19 - .161)
    near(report.get("roof_exact_pa", float("nan")), expected, 1e-8, "exact roof")
    for row, match in zip(rows, matches):
        nx = int(match.group(1))
        if row["nx"] != nx or tuple(row["shape"]) != GRID[nx]:
            raise AssertionError("grid identity changed")
        dx = 3.2 / nx
        z_idx = math.floor((.161 + .5 * dx) / dx)
        zc = (z_idx + .5) * dx
        # Independent manufactured pressure/VOF cell-centre reference:
        # p_cell=rho*g*max(H-zc,0); VOF=clamp((H-z_low)/dx,0,1).
        # No fitting to observed native values.
        f = min(1., max(0., (.19 - z_idx * dx) / dx))
        p = 1000. * 9.81 * max(0., .19 - zc)
        predicted = f * p
        for i, key in enumerate(("dx", "roof_exact_pa", "roof_current_pa",
                                 "roof_error_pa", "roof_current_wet"), start=1):
            near(float(row[key]), float(match.group(i + 1)),
                 6e-5 if key != "dx" else 5e-7, "report vs native " + key)
        near(row["dx"], dx, 5e-7, "independent grid spacing")
        near(row["roof_exact_pa"], expected, 1e-8, "independent roof reference")
        near(row["roof_current_pa"], predicted, .001, "independent native VOF bias")
        near(row["roof_error_pa"], predicted - expected, .001, "independent pressure discrepancy")
        near(row["roof_current_wet"], f, 1e-8, "independent cell VOF")
        if abs(row["roof_error_pa"]) < 10.:
            raise AssertionError("unresolved sensor bias concealed")
    if not (abs(rows[1]["roof_error_pa"]) < abs(rows[2]["roof_error_pa"])):
        raise AssertionError("observed medium-to-fine error deterioration concealed")

def main() -> None:
    native = NATIVE.read_text(encoding="utf-8", errors="replace")
    matches = list(PATTERN.finditer(native))
    if len(matches) != 3:
        raise RuntimeError("Missing native grid records")
    rows = []
    for match in matches:
        nx = int(match.group(1))
        if nx not in GRID:
            raise RuntimeError("Unexpected native grid")
        rows.append({
            "nx": nx,
            "shape": list(GRID[nx]),
            "dx": float(match.group(2)),
            "roof_exact_pa": float(match.group(3)),
            "roof_current_pa": float(match.group(4)),
            "roof_error_pa": float(match.group(5)),
            "roof_current_wet": float(match.group(6)),
        })
    report = {
        "phase": "P2",
        "milestone": "marin_test02_exact_hydrostatic_disk_reference_partial",
        "phase_complete": False,
        "measured_validation": False,
        "historical_lab_fail": True,
        "prescribed_waterline_m": .19,
        "assumed_sensor_radius_m": .01,
        "sensor_radius_experimentally_verified": False,
        "roof_exact_pa": 1000. * 9.81 * (.19 - .161),
        "native_assertions": 299,
        "ctest_suites": 156,
        "manufactured_native": rows,
        "source_sha256": {name: sha(CORE / name) for name in SOURCES},
        "artifact_sha256": {name: sha(path) for name, path in ARTIFACTS.items()},
        "unmet": [
            "Actual transducer disk diameter and response are unverified",
            "Native MAC/VOF footprint pressure has large, nonmonotonic hydrostatic bias",
            "No live stress-consistent cut-cell pressure correction",
            "No new six-second MARIN/SPHERIC H1-H4/P1-P8 convergence",
            "No dynamic bidirectional FSI, global balances or UE5.8 native E2E",
        ],
    }
    validate(report)
    negative = []
    mutations = {
        "forged_phase_pass": lambda d: d.update(phase_complete=True),
        "forged_lab_pass": lambda d: d.update(measured_validation=True),
        "concealed_historical_fail": lambda d: d.update(historical_lab_fail=False),
        "fake_native_assertions": lambda d: d.update(native_assertions=300),
        "fake_ctest": lambda d: d.update(ctest_suites=157),
        "fake_exact_roof": lambda d: d.update(roof_exact_pa=0),
        "changed_native_roof": lambda d: d["manufactured_native"][0].update(roof_current_pa=284.49),
        "changed_wet_area": lambda d: d["manufactured_native"][1].update(roof_current_wet=1.),
        "missing_fine_grid": lambda d: d["manufactured_native"].pop(),
        "changed_grid": lambda d: d["manufactured_native"][0].update(nx=48),
        "forged_source_hash": lambda d: d["source_sha256"].update({"CMakeLists.txt": "0" * 64}),
        "forged_native_hash": lambda d: d["artifact_sha256"].update(native_log="0" * 64),
    }
    for name, mutate in mutations.items():
        forged = copy.deepcopy(report)
        mutate(forged)
        try:
            validate(forged)
        except (AssertionError, KeyError, TypeError, ValueError):
            negative.append(name)
        else:
            raise AssertionError("Failed to reject forged QA: " + name)
    report["negative_qa_rejected"] = negative
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print("P2_MARIN_DISK_QA PASS: 3 manufactured grids, 299 native assertions, 156 CTests")
    print(f"NEGATIVE_QA {len(negative)}/{len(mutations)} rejected")
    for row in rows:
        print(f"nx={row['nx']} roof exact={row['roof_exact_pa']:.3f} Pa "
              f"native={row['roof_current_pa']:.3f} Pa error={row['roof_error_pa']:.3f} Pa")
    print("P2 PARTIAL: real six-second laboratory convergence still FAIL")

if __name__ == "__main__":
    main()
