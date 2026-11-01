"""Independent native QA for P2 SDF tangential traction boundary reconstruction.
Only a synthetic PARTIAL milestone; never upgrades P2 full phase.
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

CORE=Path(__file__).resolve().parents[1]
ROOT=CORE.parents[1]
RUNS=ROOT/"ENGINE TEST STUBE"/"runs"
PREFIX="p2_sdf_tangential_stress"
BINARY=CORE/"build"/"upc_p2_sdf_tangential_stress_tests.exe"
NATIVE=RUNS/(PREFIX+"_native.txt")
BUILD=RUNS/(PREFIX+"_build.txt")
CTEST=RUNS/(PREFIX+"_ctest.txt")
GATE=RUNS/(PREFIX+"_phase_gate.txt")
REPORT=RUNS/(PREFIX+"_partial.json")
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_sdf_capillary3d.hpp",
    "include/upc/p2_sdf_normal_stress3d.hpp",
    "include/upc/p2_sdf_tangential_stress3d.hpp",
    "tests/p2_sdf_tangential_stress3d_tests.cpp",
    "scripts/p2_sdf_tangential_stress_build.cmd",
    "scripts/p2_sdf_tangential_stress_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_SDF_TANGENTIAL_STRESS_PARTIAL.md",
]
PLANE=re.compile(r"TANGENTIAL_PLANE faces=(\d+) before_pa=([0-9.eE+-]+) after_pa=([0-9.eE+-]+) normal_delta_pa=([0-9.eE+-]+)")
SPHERE=re.compile(r"TANGENTIAL_SPHERE dx=([0-9.eE+-]+) faces=(\d+) max_after_pa=([0-9.eE+-]+) max_normal_delta_pa=([0-9.eE+-]+) pressure_delta_pa=([0-9.eE+-]+)")
NEG=re.compile(r"TANGENTIAL_NEGATIVE rejected=(\d+) zero_shear_faces=(\d+)")
PASS=re.compile(r"PASS P2_SDF_TANGENTIAL_STRESS assertions=(\d+) phase_complete=false")

def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def parse(log:str)->dict:
    lines=log.strip().splitlines()
    if len(lines)!=5:
        raise ValueError("Native log must contain exactly five trace rows")
    plane=PLANE.fullmatch(lines[0])
    spheres=[SPHERE.fullmatch(x) for x in lines[1:3]]
    neg=NEG.fullmatch(lines[3])
    passed=PASS.fullmatch(lines[4])
    if not plane or not all(spheres) or not neg or not passed:
        raise ValueError("Native evidence missing or malformed")
    count=int(plane[1])
    before,after,normal_delta=map(float,plane.groups()[1:])
    # Independent planar x-normal manufactured traction:
    # t_y=2*(G_yx+G_xy)-0.5*(A_yx+A_xy)=0.555 Pa
    # t_z=2*(G_zx+G_xz)-0.5*(A_zx+A_xz)=0.89 Pa.
    expected=math.hypot(.6-.555,-.25-.89)
    if count!=144 or not all(map(math.isfinite,(before,after,normal_delta))):
        raise ValueError("Invalid planar evidence")
    if abs(before-expected)>1e-5 or not (0<=after<1e-11) or not (0<=normal_delta<1e-11):
        raise ValueError("Manufactured planar tangential/normal stress failed")
    rows=[]
    for m,grid,min_faces in zip(spheres,(.08,.04),(250,950)):
        dx,faces,res,normal,pressure=map(float,m.groups())
        if not all(map(math.isfinite,(dx,faces,res,normal,pressure))):
            raise ValueError("Nonfinite sphere evidence")
        if abs(dx-grid)>1e-12 or faces<min_faces or not all(
            0<=v<1e-10 for v in (res,normal,pressure)
        ):
            raise ValueError("Spherical 3D traction or normal-pressure invariance failed")
        rows.append(dict(dx_m=dx,faces=int(faces),residual_pa=res,
                         normal_jump_change_pa=normal,pressure_change_pa=pressure))
    if int(neg[1])!=5 or int(neg[2])!=144 or int(passed[1])<5000:
        raise ValueError("Negative cases or native assertions missing")
    return dict(plane_faces=count,plane_initial_residual_pa=before,
                plane_corrected_residual_pa=after,
                plane_normal_stress_change_pa=normal_delta,
                spheres=rows,rejected_invalid_inputs=5,
                native_assertions=int(passed[1]))

def main()->None:
    native=NATIVE.read_text(encoding="utf-8")
    evidence=parse(native)
    build=BUILD.read_text(encoding="utf-8")
    ctest=CTEST.read_text(encoding="utf-8")
    if "upc_p2_sdf_tangential_stress_tests.exe" not in build:
        raise ValueError("Missing real MSVC native target build")
    if "100% tests passed, 0 tests failed out of 130" not in ctest:
        raise ValueError("Missing full 130-suite native CTest PASS")
    rerun=subprocess.run([str(BINARY)],capture_output=True,text=True,timeout=90)
    if rerun.returncode!=0 or rerun.stdout.strip()!=native.strip():
        raise ValueError("Independent native rerun disagrees")
    forged=0
    forged_traces=[]
    for old,new in (
        ("phase_complete=false","phase_complete=true"),
        ("faces=144","faces=0"),
        ("rejected=5","rejected=0"),
    ):
        if old not in native:
            raise ValueError("Tamper test anchor missing: "+old)
        forged_traces.append(native.replace(old,new,1))
    for pattern in (
        r"(?<= after_pa=)[0-9.eE+-]+",
        r"(?<= max_after_pa=)[0-9.eE+-]+",
        r"(?<= assertions=)[0-9]+",
    ):
        changed,count=re.subn(pattern,"9",native,count=1)
        if count!=1:
            raise ValueError("Tamper numeric anchor missing: "+pattern)
        forged_traces.append(changed)
    for tampered in forged_traces:
        try:
            parse(tampered)
        except ValueError:
            forged+=1
        else:
            raise ValueError("Tampered native trace was accepted")
    marin=RUNS/"p2_marin_pcg_6s_partial.json"
    old=json.loads(marin.read_text(encoding="utf-8"))
    if old.get("height_rmse_threshold_met") is not False or old.get("phase_complete") is not False:
        raise ValueError("Real MARIN 6s validation failure not preserved")
    gate=subprocess.run([sys.executable,str(CORE/"scripts"/"phase_gate.py")],
                        capture_output=True,text=True,timeout=240)
    GATE.write_text(gate.stdout+gate.stderr,encoding="utf-8")
    if gate.returncode!=0 or "ALL_COMPLETE=False" not in gate.stdout or (
        "P2" not in gate.stdout.split("GATES_INCOMPLETE=",1)[-1].splitlines()[0]):
        raise ValueError("Official phase gate incorrectly promoted P2")
    report=dict(
        phase="P2",milestone="prescribed_sdf_tangential_stress_gradient_partial",
        partial_accepted=True,phase_complete=False,
        native=evidence,ctest_passed=130,tampered_native_logs_rejected=forged,
        historical_marin_6s_height_validation="FAIL",
        historical_marin_report_sha256=sha(marin),
        official_phase_gate="P2 INCOMPLETE / ALL_COMPLETE=False",
        limitations=[
            "Reconstructed local prescribed liquid gradient only; no MAC/VOF ghost-velocity update",
            "Air gradient and surface tension gradient externally prescribed, no dynamic air/Marangoni solver",
            "No independent full 6s MARIN H1-H4/P1-P8 measurement/grid convergence",
            "No dynamic two-way rigid FSI/global momentum and energy closure",
            "No Unreal 5.8 Native E2E for this new path",
        ],
        source_sha256={name:sha(CORE/name) for name in SOURCES},
        binary_sha256=sha(BINARY),
        build_log_sha256=sha(BUILD),
        native_log_sha256=sha(NATIVE),
        ctest_log_sha256=sha(CTEST),
        phase_gate_log_sha256=sha(GATE),
        generated_utc=datetime.now(timezone.utc).isoformat(),
    )
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print(f"P2 TANGENTIAL STRESS PARTIAL PASS: {evidence['native_assertions']} assertions; "
          f"130/130 CTest; {forged} forged traces rejected; P2_FULL=false")

if __name__=="__main__":
    main()
