"""Independent, deliberately PARTIAL MARIN live-column wall diagnostic QA.

Analytic box area/force references are computed without importing UPC code.
The historical 6s three-grid experiment is read only; never rerun here.
"""
from __future__ import annotations
import copy
import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
ROOT = CORE.parents[1]
RUNS = ROOT / "ENGINE TEST STUBE" / "runs"
OUT = RUNS / "p2_marin_live_column_wall_partial.json"
SOURCES = (
    "CMakeLists.txt",
    "include/upc/p2_marin_live_column_wall3d.hpp",
    "include/upc/p2_marin_box_wall_traction3d.hpp",
    "tests/p2_marin_live_column_wall3d_tests.cpp",
    "scripts/p2_marin_live_column_wall_audit.py",
    "docs/P2_MARIN_LIVE_COLUMN_WALL_PARTIAL.md",
)
LOGS = (
    "p2_live_column_red_build_20261102.log",
    "p2_live_column_build_20261102.log",
    "p2_live_column_native_20261102.log",
    "p2_live_column_full_ctest_20261102.log",
    "p2_marin_pcg_full6s_three_grid_partial.json",
)

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def log_text(name: str) -> str:
    raw=(RUNS/name).read_bytes()
    return raw.decode("utf-16" if raw.startswith(b"\xff\xfe") else "utf-8",errors="replace")

def close(a: float, b: float, tol: float = 2e-6) -> bool:
    return math.isfinite(a) and abs(a-b) <= tol

def validate(report: dict) -> bool:
    try:
        if (report["phase"] != "P2" or report["phase_complete"] is not False
                or report["partial_accepted"] is not True
                or report["measured_physics_pass"] is not False):
            return False
        if report["ctest"]["count"] != 155 or not report["ctest"]["passed"]:
            return False
        if report["native"]["assertions"] < 62:
            return False
        if not close(report["native"]["divergence_s_inv"], 0, 1e-6):
            return False
        if len(report["grids"]) != 3:
            return False
        width, depth, height, p = .161, .403, .161, 3000.
        low_area = 2*(width+depth)*.10
        high_area = 2*(width+depth)*height+width*depth
        high_fz = -p*width*depth
        for item, nx in zip(report["grids"], (32,48,64)):
            if item["nx"] != nx or not all((
                close(item["low_area_m2"], low_area, 1e-9),
                close(item["high_area_m2"], high_area, 1e-9),
                close(item["high_force_z_n"], high_fz, 1e-6))):
                return False
        h=report["historical_six_second"]
        if h["all_12_channels_monotonic"] is not False:
            return False
        if set(h["fine_height_rmse_m"]) != {"H1_m","H2_m","H3_m","H4_m"}:
            return False
        if not all(value > .05 for value in h["fine_height_rmse_m"].values()):
            return False
        if set(h["fine_pressure_rmse_pa"]) != {f"P{i}_Pa" for i in range(1,9)}:
            return False
        if not all(value > 5000 for value in h["fine_pressure_rmse_pa"].values()):
            return False
        if not report["red_missing_header_reproduced"]:
            return False
        for name,expected in report["source_sha256"].items():
            if name not in SOURCES or sha(CORE/name) != expected:
                return False
        if set(report["source_sha256"]) != set(SOURCES):
            return False
        for name,expected in report["evidence_sha256"].items():
            if name not in LOGS or sha(RUNS/name) != expected:
                return False
        if set(report["evidence_sha256"]) != set(LOGS):
            return False
        return True
    except (KeyError,ValueError,TypeError,OSError):
        return False

def main() -> None:
    native=log_text(LOGS[2])
    ctest=log_text(LOGS[3])
    red=log_text(LOGS[0])
    build=log_text(LOGS[1])
    assert "fatal error C1083" in red and "p2_marin_live_column_wall3d.hpp" in red
    assert "Linking CXX executable upc_p2_marin_live_column_wall_tests.exe" in build
    assert "100% tests passed, 0 tests failed out of 155" in ctest
    rows=re.findall(r"COLUMN_MANUFACTURED nx=(\d+) low_area=([\d.eE+-]+) high_area=([\d.eE+-]+) high_fz=([\d.eE+-]+)",native)
    assert len(rows)==3,rows
    match=re.search(r"COLUMN_NATIVE_MAC area=([\d.eE+-]+) fz=([\d.eE+-]+) divergence=([\d.eE+-]+)",native)
    assert match, "No actual native MAC PCG result"
    passes=re.search(r"PASS P2_MARIN_LIVE_COLUMN assertions=(\d+) phase_complete=false measured_validation=false",native)
    assert passes, "Native test not PASS or incorrectly claims P2 acceptance"
    old=json.loads((RUNS/LOGS[4]).read_text(encoding="utf-8"))
    report={
        "phase":"P2",
        "scope":"live MAC pressure plus strictly graph-shaped VOF column diagnostic",
        "phase_complete":False,
        "partial_accepted":True,
        "measured_physics_pass":False,
        "red_missing_header_reproduced":True,
        "ctest":{"count":155,"passed":True},
        "native":{
            "assertions":int(passes.group(1)),
            "wall_area_m2":float(match.group(1)),
            "force_z_n":float(match.group(2)),
            "divergence_s_inv":float(match.group(3)),
        },
        "grids":[{
            "nx":int(row[0]),"low_area_m2":float(row[1]),
            "high_area_m2":float(row[2]),"high_force_z_n":float(row[3])
        } for row in rows],
        "historical_six_second":{
            "all_12_channels_monotonic":old["all_12_channels_improve_monotonically"],
            "fine_height_rmse_m":old["fine_height_rmse_m"],
            "fine_pressure_rmse_pa":old["fine_pressure_rmse_pa"],
            "unmodified_rmse_limit_m":old["full_six_second_height_rmse_target_m"],
        },
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "evidence_sha256":{name:sha(RUNS/name) for name in LOGS},
        "unmet":[
            "moving non-graph water-air interface stress and pressure",
            "true geometric cut-cell MAC pressure solver at MARIN obstacle",
            "resolved calibrated real P1-P8 transducer response",
            "independent 6s three-grid height and pressure convergence",
            "bidirectional dynamic FSI and global energy/momentum",
            "full coupled Unreal 5.8 native E2E",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    assert validate(report),"Independent analytic/hash/native QA failed"
    mutations={
        "fake_full_phase":lambda r:r.update(phase_complete=True),
        "fake_measurement_pass":lambda r:r.update(measured_physics_pass=True),
        "lost_grid":lambda r:r["grids"].pop(),
        "forged_analytic_force":lambda r:r["grids"][2].update(high_force_z_n=-100),
        "forged_mac_divergence":lambda r:r["native"].update(divergence_s_inv=.1),
        "forged_source_hash":lambda r:r["source_sha256"].update({"CMakeLists.txt":"0"*64}),
        "forged_ctest":lambda r:r["ctest"].update(count=1),
        "forged_marin_convergence":lambda r:r["historical_six_second"].update(all_12_channels_monotonic=True),
    }
    rejected=[]
    for name,mutate in mutations.items():
        corrupted=copy.deepcopy(report)
        mutate(corrupted)
        assert not validate(corrupted),f"QA accepted tampered report: {name}"
        rejected.append(name)
    report["negative_mutations_rejected"]=rejected
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(f"PASS P2_MARIN_LIVE_COLUMN_AUDIT grids=3 native_assertions={passes.group(1)} "
          f"ctest=155/155 negative_qa={len(rejected)}/{len(mutations)} "
          "lab_physics=FAIL phase_complete=false")
    print(f"REPORT={OUT}")
if __name__=="__main__":
    main()
