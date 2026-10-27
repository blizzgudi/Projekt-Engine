"""P2 partial: independently check analytic moving-wall pressure work, not FSI."""
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
EXE = CORE / "build" / "upc_p2_kinematic_pressure_work_tests.exe"
LOG = RUNS / "p2_kinematic_pressure_work_ctest_20261027.txt"
REPORT = RUNS / "p2_kinematic_pressure_work_partial.json"
SOURCES = [
    "include/upc/kinematic_wall_projection3d.hpp",
    "include/upc/kinematic_wall_pressure_work3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_kinematic_wall_pressure_work3d_tests.cpp",
    "scripts/p2_kinematic_pressure_work_acceptance.py",
    "docs/P2_KINEMATIC_WALL_PRESSURE_WORK_PARTIAL.md",
    "docs/PHASENPLAN.md", "CMakeLists.txt",
]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    log = LOG.read_text(encoding="utf-16" if LOG.read_bytes().startswith(bytes((255,254))) else "utf-8-sig")
    m = re.search(r"100% tests passed, 0 tests failed out of (\d+)", log)
    if not m or int(m.group(1)) < 49 or (
        "physics_p2_kinematic_wall_pressure_work_partial" not in log
    ):
        raise RuntimeError("Full CTest evidence missing")
    native = subprocess.run([str(EXE)], cwd=CORE, capture_output=True,
                            text=True, timeout=60, check=False)
    if native.returncode:
        raise RuntimeError("Native failed: " + native.stderr)
    if "CLOSED_GAUGE: 24 wetted faces, +73 Pa gauge invariant" not in native.stderr:
        raise RuntimeError("Gauge invariant native test missing")
    lines = [s.strip() for s in native.stdout.splitlines() if s.strip()]
    if len(lines) != 5 or not lines[0].startswith("case,wet_faces,"):
        raise RuntimeError("Unexpected native CSV")
    marker = re.fullmatch(
        r"PASS: P2 kinematic pressure-work assertions=(\d+) phase_complete=false",
        lines[-1])
    if not marker or int(marker.group(1)) < 69:
        raise RuntimeError("Native assertion count or partial status wrong")
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    if [r["case"] for r in rows] != [
        "open_positive", "open_negative", "open_stationary"
    ]:
        raise RuntimeError("Native cases missing")
    checks = []
    def check(name, value, expected, tol=0):
        ok = (math.isfinite(value) and abs(value-expected)<=tol
              if isinstance(value,(int,float)) else value==expected)
        checks.append(dict(name=name, **{"pass":bool(ok)},
                           value=value, reference=expected, tolerance=tol))
    for row, v in zip(rows, (0.12,-0.12,0.0)):
        p = 1000*0.08*v/(2*0.025)
        f = -2*p*0.08**2
        power = f*v
        check(row["case"]+"_faces",int(row["wet_faces"]),2)
        for field,ref,tol in [
            ("force_x_N",f,1e-12),("torque_x_Nm",0,1e-14),
            ("power_W",power,1e-13),("work_J",power*0.025,1e-14),
            ("impulse_x_Ns",f*0.025,1e-14),
            ("divergence_s_inv",0,1e-10),
        ]:
            check(row["case"]+"_"+field,float(row[field]),ref,tol)
    check("native_assertions",int(marker.group(1)),69)
    if not all(c["pass"] for c in checks):
        raise RuntimeError("Independent analytic reference mismatch")
    gate = subprocess.run(["python",str(CORE/"scripts"/"phase_gate.py")],
                          cwd=CORE,capture_output=True,text=True,timeout=120)
    if gate.returncode or "GATES_PASS=P0,P1" not in gate.stdout or (
        "GATES_INCOMPLETE=P2,P3,P4,P5,P6,P7,P8" not in gate.stdout
    ) or "ALL_COMPLETE=False" not in gate.stdout:
        raise RuntimeError("Full-phase gate changed: "+gate.stdout)
    report = dict(
        phase="P2", milestone="kinematic_wall_pressure_work_partial",
        partial_accepted=True, phase_complete=False,
        timestamp_utc=datetime.now(timezone.utc).isoformat(),
        native_assertions=int(marker.group(1)), checks=checks,
        native_csv=lines[:-1], native_stderr=native.stderr.strip(),
        ctest_count=int(m.group(1)), phase_gate_output=gate.stdout.strip(),
        source_sha256={name:sha(CORE/name) for name in SOURCES},
        native_exe_sha256=sha(EXE), ctest_log_sha256=sha(LOG),
        scope="frozen geometry, read-only pressure traction/work; no dynamic FSI")
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"PASS: P2 pressure work {len(checks)}/{len(checks)} independent checks, "
          f"native={marker.group(1)}, ctest={m.group(1)}, phase_complete=false")

if __name__=="__main__":
    main()
