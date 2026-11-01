"""P2 PARTIAL: independent native planar VOF normal fit evidence checker.

No physical stress, MARIN measured validation or P2 full acceptance is inferred.
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
PROJECT = CORE.parents[1]
RUNS = PROJECT / "ENGINE TEST STUBE" / "runs"
BINARY = CORE / "build" / "upc_p2_local_planar_normal_fit_tests.exe"
BUILD = RUNS / "p2_local_planar_normal_fit_build.txt"
CTEST = RUNS / "p2_local_planar_normal_fit_ctest.txt"
NATIVE = RUNS / "p2_local_planar_normal_fit_native.txt"
REPORT = RUNS / "p2_local_planar_normal_fit_partial.json"
SOURCES = [
    "CMakeLists.txt",
    "include/upc/p2_local_planar_normal_fit3d.hpp",
    "include/upc/p2_vof_normal_reconstruction3d.hpp",
    "include/upc/p2_planar_vof_pcg_geometry3d.hpp",
    "include/upc/p2_oblique_plic_geometry3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_local_planar_normal_fit3d_tests.cpp",
    "scripts/p2_local_planar_normal_fit_build.cmd",
    "scripts/p2_local_planar_normal_fit_acceptance.py",
    "docs/P2_LOCAL_PLANAR_NORMAL_FIT_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
EXPECTED = [(8,104,96),(8,85,63),(8,127,119),
            (12,228,216),(12,166,165),(12,287,275)]

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def verify_native(log: str) -> list[dict]:
    lines = log.splitlines()
    if not lines or lines[-1] != (
        "PASS P2_LOCAL_PLANAR_NORMAL_FIT assertions=1977 phase_complete=false"
    ):
        raise ValueError("Native assertion count or partial marker invalid")
    rows = [line for line in lines if line.startswith("PLANE ")]
    if len(rows) != 6 or len(lines) != 7:
        raise ValueError("Six manufactured planar native results required")
    parsed = []
    for line, (n, cells, faces) in zip(rows, EXPECTED):
        m = re.fullmatch(
            r"PLANE n=(\d+) cells=(\d+) faces=(\d+) "
            r"min_alignment=([0-9.eE+\-]+) "
            r"max_fraction_error=([0-9.eE+\-]+) iterations=(\d+)",
            line,
        )
        if not m:
            raise ValueError("Malformed native 3D plane row")
        nn, cc, ff = map(int, m.group(1, 2, 3))
        alignment, error = float(m.group(4)), float(m.group(5))
        iters = int(m.group(6))
        if (nn,cc,ff)!=(n,cells,faces):
            raise ValueError("Missing independent grid/orientation variant")
        if (not math.isfinite(alignment) or alignment < 1-2e-7 or
            not math.isfinite(error) or error >= 1e-9 or iters <= 0):
            raise ValueError("Planar volume fit or normal alignment failed")
        parsed.append({"n":nn,"partial_cells":cc,"pressure_faces":ff,
                       "minimum_alignment":alignment,
                       "maximum_fraction_residual":error,
                       "solver_iterations":iters})
    return parsed

def mutation_checks(log: str) -> int:
    variants = [
        log.replace("assertions=1977","assertions=1651"),
        log.replace("phase_complete=false","phase_complete=true"),
        log.replace("faces=96","faces=0",1),
        log.replace("max_fraction_error=4.1137093731435925e-12",
                    "max_fraction_error=0.01",1),
        "\n".join(log.splitlines()[:-2])+"\n"+log.splitlines()[-1]+"\n",
        log.replace("min_alignment=0.99999999999999978",
                    "min_alignment=0.9",1),
    ]
    for bad in variants:
        try:
            verify_native(bad)
        except ValueError:
            continue
        raise RuntimeError("Independent QA accepted corrupted native evidence")
    return len(variants)

def main() -> None:
    if not all(p.is_file() for p in (BINARY,BUILD,CTEST,NATIVE)):
        raise RuntimeError("Native binary/build/CTest/trace missing")
    if BINARY.stat().st_mtime_ns < max(
        (CORE / p).stat().st_mtime_ns for p in SOURCES if p.endswith((".hpp",".cpp"))
    ):
        raise RuntimeError("Native executable predates source")
    native = NATIVE.read_text(encoding="utf-8")
    rows = verify_native(native)
    mutations = mutation_checks(native)
    if not re.search(r"100% tests passed, 0 tests failed out of 125",
                     CTEST.read_text(encoding="utf-8")):
        raise RuntimeError("Full 125 CTest suite not passed")
    build_log = BUILD.read_text(encoding="utf-8", errors="replace")
    if ("upc_p2_local_planar_normal_fit_tests" not in build_log and
        "ninja: no work to do." not in build_log):
        raise RuntimeError("Neither native build nor up-to-date Ninja build evidenced")
    result = subprocess.run([str(BINARY)],cwd=CORE,capture_output=True,
                            text=True,timeout=90)
    if result.returncode or result.stderr.strip() or result.stdout != native:
        raise RuntimeError("Fresh native re-run does not match recorded trace")
    report = {
        "phase":"P2",
        "milestone":"local_volume_matched_planar_vof_normal_fit_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "marin_measured_validation_pass":False,
        "native_assertions":1977,
        "full_ctest_suites":125,
        "corrupted_native_traces_rejected":mutations,
        "manufactured_3d_planes":rows,
        "source_sha256":{p:sha(CORE/p) for p in SOURCES},
        "artifact_sha256":{str(p.relative_to(PROJECT)).replace("\\","/"):sha(p)
                           for p in (BINARY,BUILD,CTEST,NATIVE)},
        "unresolved":[
            "Local plane fits are not dynamic stress-consistent air-water CFD",
            "No validated MARIN H1-H4/P1-P8 full >=6s three-grid convergence",
            "No dynamic bidirectional solid impulse and global energy closure",
            "No new Unreal 5.8 native E2E for this optional geometry",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"PARTIAL_ACCEPTED=True PLANES={len(rows)} "
          f"NATIVE_ASSERTIONS=1977 CTEST=125/125 NEGATIVE_QA={mutations}")
    print("P2_FULL_ACCEPTANCE=False")
    print(f"EVIDENCE={REPORT}")

if __name__=="__main__":
    main()
