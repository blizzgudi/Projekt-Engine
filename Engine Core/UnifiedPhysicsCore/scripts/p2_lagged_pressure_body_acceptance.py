"""P2 PARTIAL: independent reference for lagged MAC pressure -> rigid predictor."""
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
EXE = CORE / "build" / "upc_p2_lagged_pressure_body_tests.exe"
LOG = RUNS / "p2_lagged_pressure_body_ctest_20261027.txt"
REPORT = RUNS / "p2_lagged_pressure_body_partial.json"
SOURCES = [
    "include/upc/lagged_kinematic_pressure_body3d.hpp",
    "include/upc/kinematic_wall_projection3d.hpp",
    "include/upc/kinematic_wall_pressure_work3d.hpp",
    "include/upc/rigid_pressure_response3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_lagged_kinematic_pressure_body3d_tests.cpp",
    "scripts/p2_lagged_pressure_body_acceptance.py",
    "docs/P2_LAGGED_KINEMATIC_PRESSURE_BODY_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    raw=LOG.read_bytes()
    log=raw.decode("utf-16" if raw.startswith(bytes((255,254))) else "utf-8-sig")
    m=re.search(r"100% tests passed, 0 tests failed out of (\d+)",log)
    if not m or int(m.group(1))<51 or (
        "physics_p2_lagged_kinematic_pressure_body_partial" not in log
    ):
        raise RuntimeError("Full CTest evidence missing")
    run=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                       text=True,timeout=120,check=False)
    if run.returncode:
        raise RuntimeError("Native predictor failed: "+run.stderr)
    lines=[s.strip() for s in run.stdout.splitlines() if s.strip()]
    marker=re.fullmatch(
        r"PASS: P2 lagged pressure rigid predictor assertions=(\d+) phase_complete=false",
        lines[-1])
    if not marker or int(marker.group(1))<190:
        raise RuntimeError("Native predictor assertion marker missing")
    rows=list(csv.DictReader(io.StringIO("\n".join(lines[:-1]))))
    if len(rows)!=9:
        raise RuntimeError("Expected nine 3D reference cases")
    checks=[]
    def near(name,value,reference,tol=1e-11):
        ok=math.isfinite(value) and abs(value-reference)<=tol
        checks.append({"name":name,"value":value,"reference":reference,
                       "tolerance":tol,"pass":bool(ok)})
    for row,(n,speed) in zip(rows,((n,v) for n in (8,12,16)
                                  for v in (0.12,-0.12,0.0))):
        key=f"{n}_{speed:+.2f}"
        near(key+"_grid",float(row["grid"]),n,0)
        near(key+"_speed",float(row["speed_m_s"]),speed,1e-15)
        dx=0.08;rho=1000.0;dt=0.025;mass=4.0;inertia=0.2
        force=-rho*dx**3*speed/dt
        torque=0.04*force
        impulse=force*dt
        angular_impulse=torque*dt
        vx=speed+impulse/mass
        omega=angular_impulse/inertia
        old_work=force*speed*dt
        mid_work=force*dt*(speed+vx)/2+torque*dt*omega/2
        gap=0.5*(impulse**2/mass+angular_impulse**2/inertia)
        for name,reference in (
            ("force_x_N",force),("torque_z_Nm",torque),
            ("impulse_x_Ns",impulse),("angular_impulse_z_Nms",angular_impulse),
            ("body_vx_after_m_s",vx),("body_omega_z_after_rad_s",omega),
            ("frozen_work_J",old_work),("kinetic_work_J",mid_work),
            ("lagged_gap_J",gap)):
            near(key+"_"+name,float(row[name]),reference)
        div=float(row["divergence_s_inv"])
        checks.append({"name":key+"_divergence","value":div,
                       "reference":"<=1e-8 s^-1",
                       "pass":math.isfinite(div) and 0<=div<=1e-8})
    if not all(c["pass"] for c in checks):
        raise RuntimeError("Independent reference mismatch")
    gate=subprocess.run(["python",str(CORE/"scripts"/"phase_gate.py")],
                        cwd=CORE,capture_output=True,text=True,timeout=120)
    if gate.returncode or "GATES_PASS=P0,P1" not in gate.stdout or (
        "GATES_INCOMPLETE=P2,P3,P4,P5,P6,P7,P8" not in gate.stdout
    ) or "ALL_COMPLETE=False" not in gate.stdout:
        raise RuntimeError("Unexpected phase gate: "+gate.stdout+" "+gate.stderr)
    report={
        "phase":"P2","milestone":"lagged_kinematic_pressure_rigid_predictor_partial",
        "partial_accepted":True,"phase_complete":False,
        "timestamp_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":int(marker.group(1)),"checks":checks,
        "ctest_count":int(m.group(1)),"native_csv":lines[:-1],
        "phase_gate_output":gate.stdout.strip(),
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "native_exe_sha256":sha(EXE),"ctest_log_sha256":sha(LOG),
        "scope":"real projected kinematic wall pressure -> rigid predictor; frozen mask and no applied fluid reaction; not FSI"
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"PASS: P2 lagged rigid predictor {len(checks)}/{len(checks)} "
          f"independent checks, native={marker.group(1)}, "
          f"ctest={m.group(1)}, phase_complete=false")

if __name__=="__main__":
    main()
