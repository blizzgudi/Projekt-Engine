"""P2 partial: native 3D MAC convergence/atomicity regression, not full FSI."""
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
EXE = CORE / "build" / "upc_p2_kinematic_convergence_tests.exe"
LOG = RUNS / "p2_kinematic_convergence_ctest_20261027.txt"
REPORT = RUNS / "p2_kinematic_convergence_partial.json"
SOURCES = [
    "include/upc/kinematic_wall_projection3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_kinematic_projection_convergence3d_tests.cpp",
    "scripts/p2_kinematic_convergence_acceptance.py",
    "docs/P2_KINEMATIC_PROJECTION_CONVERGENCE_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    raw = LOG.read_bytes()
    log = raw.decode("utf-16" if raw.startswith(bytes((255,254))) else "utf-8-sig")
    m = re.search(r"100% tests passed, 0 tests failed out of (\d+)", log)
    if not m or int(m.group(1)) < 50 or (
        "physics_p2_kinematic_projection_atomic_convergence_partial" not in log
    ):
        raise RuntimeError("Full native CTest proof absent")
    native = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                            text=True, timeout=90, check=False)
    if native.returncode:
        raise RuntimeError("Native regression failed: " + native.stderr)
    lines = [line.strip() for line in native.stdout.splitlines() if line.strip()]
    if len(lines) != 8 or not lines[0].startswith("grid,boundary,rejected_budgets,"):
        raise RuntimeError("Unexpected native CSV")
    marker = re.fullmatch(
        r"PASS: P2 atomic convergence checks=(\d+) phase_complete=false",
        lines[-1])
    if not marker or int(marker.group(1)) < 81644:
        raise RuntimeError("Insufficient native atomicity assertions")
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    checks = []
    def check(name, value, expected, tolerance=0.0):
        ok = (math.isfinite(value) and abs(value-expected) <= tolerance
              if isinstance(value,(int,float)) else value == expected)
        checks.append({"name":name, "pass":bool(ok), "value":value,
                       "reference":expected, "tolerance":tolerance})
    for row, (n, boundary) in zip(rows,
                                   ((n, b) for n in (8,12,16)
                                    for b in ("closed","open"))):
        key = f"{n}_{boundary}"
        check(key+"_grid",int(row["grid"]),n)
        check(key+"_boundary",row["boundary"],boundary)
        check(key+"_rejected_budgets",int(row["rejected_budgets"]),2)
        check(key+"_mac_face_count",int(row["mac_faces"]),3*(n+1)*n*n)
        iterations = int(row["accepted_iterations"])
        checks.append({"name":key+"_iterations_in_bounds",
                       "pass":2<iterations<=5000,"value":iterations,
                       "reference":"3..5000"})
        divergence = float(row["max_divergence_s_inv"])
        checks.append({"name":key+"_divergence_below_1e-8",
                       "pass":math.isfinite(divergence) and 0<=divergence<=1e-8,
                       "value":divergence,"reference":"<=1e-8 s^-1"})
    check("native_assertions",int(marker.group(1)),81644)
    if len(rows)!=6 or not all(c["pass"] for c in checks):
        raise RuntimeError("Native reference mismatch")
    gate = subprocess.run(["python",str(CORE/"scripts"/"phase_gate.py")],
                          cwd=CORE,capture_output=True,text=True,timeout=120)
    if gate.returncode or "GATES_PASS=P0,P1" not in gate.stdout or (
        "GATES_INCOMPLETE=P2,P3,P4,P5,P6,P7,P8" not in gate.stdout
    ) or "ALL_COMPLETE=False" not in gate.stdout:
        raise RuntimeError("Full phase status changed: "+gate.stdout)
    report = {
        "phase":"P2", "milestone":"kinematic_projection_atomic_convergence_partial",
        "partial_accepted":True, "phase_complete":False,
        "timestamp_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":int(marker.group(1)), "checks":checks,
        "native_csv":lines[:-1], "ctest_count":int(m.group(1)),
        "phase_gate_output":gate.stdout.strip(),
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "native_exe_sha256":sha(EXE), "ctest_log_sha256":sha(LOG),
        "scope":"frozen-mask kinematic MAC solver atomic rejection on underconvergence; not dynamic FSI"
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"PASS: P2 atomic convergence {len(checks)}/{len(checks)} checks, "
          f"native={marker.group(1)}, ctest={m.group(1)}, phase_complete=false")

if __name__=="__main__":
    main()
