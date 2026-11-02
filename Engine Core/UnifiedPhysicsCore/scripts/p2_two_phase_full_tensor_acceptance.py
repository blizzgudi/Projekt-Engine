r"""Independent native proof for the opt-in P2 moving 3D full-symmetric
Newtonian MAC viscous tensor + 3D water/air VOF + PCG coupling.
Only a numerical PARTIAL QA, never P2 full CFD/FSI/MARIN acceptance.
All files and any temporary outputs remain in D:\Projekt Engine.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime,timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
ROOT=CORE.parents[1]
RUNS=ROOT/"ENGINE TEST STUBE"/"runs"
BINARY_TENSOR=CORE/"build"/"upc_p2_two_phase_full_tensor_mac_tests.exe"
BINARY_LIVE=CORE/"build"/"upc_p2_two_phase_full_tensor_live_tests.exe"
NATIVE_TENSOR=RUNS/"p2_two_phase_full_tensor_native.txt"
NATIVE_LIVE=RUNS/"p2_two_phase_full_tensor_live_native.txt"
BUILD=RUNS/"p2_two_phase_full_tensor_build.txt"
CTEST=RUNS/"p2_two_phase_full_tensor_ctest.txt"
GATE=RUNS/"p2_two_phase_full_tensor_phase_gate.txt"
REPORT=RUNS/"p2_two_phase_full_tensor_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_two_phase_full_tensor_viscous_mac3d.hpp",
    "include/upc/p2_two_phase_full_tensor_mac_vof3d.hpp",
    "include/upc/p2_two_phase_mac_vof3d.hpp",
    "include/upc/p2_two_phase_mac_pressure3d.hpp",
    "tests/p2_two_phase_full_tensor_viscous_mac3d_tests.cpp",
    "tests/p2_two_phase_full_tensor_mac_vof3d_tests.cpp",
    "scripts/p2_two_phase_full_tensor_build.cmd",
    "scripts/p2_two_phase_full_tensor_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_TWO_PHASE_FULL_TENSOR_MAC_VOF_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
FOURIER=re.compile(
    r"^SYMMETRIC_MAC Fourier grid=(\d+)x(\d+)x(\d+)"
    r" error_m_s=([-+0-9.eE]+)"
    r" dissipation_w=([-+0-9.eE]+)"
    r" energy_residual_j=([-+0-9.eE]+) phase_complete=false$"
)
HETERO=re.compile(
    r"^SYMMETRIC_MAC hetero_grid=(\d+)x(\d+)x(\d+) steps=(\d+)"
    r" max_velocity_delta=([-+0-9.eE]+)"
    r" max_energy_residual_j=([-+0-9.eE]+)"
    r" max_wall_impulse_residual=([-+0-9.eE]+) phase_complete=false$"
)
LIVE=re.compile(
    r"^LIVE_TENSOR grid=(\d+)x(\d+)x(\d+) oblique=([01]) steps=(\d+)"
    r" max_moving_vof_cfl=([-+0-9.eE]+)"
    r" viscous_delta_m_s=([-+0-9.eE]+)"
    r" viscous_loss_j=([-+0-9.eE]+)"
    r" vs_inviscid_face_delta_m_s=([-+0-9.eE]+)"
    r" pressure_divergence_s_inv=([-+0-9.eE]+)"
    r" stress_pressure_energy_residual_j=([-+0-9.eE]+)"
    r" stress_pressure_impulse_residual_kg_m_s=([-+0-9.eE]+)"
    r" phase_mass_residual_kg=([-+0-9.eE]+)"
    r" full_phase=false$"
)
EXPECTED={
    "fourier":{"4x4x4","6x5x4","8x6x5"},
    "hetero":{"4x4x4","6x5x4","8x6x5"},
    "live":{("4x4x4",0),("6x5x4",0),("6x5x4",1),("8x6x5",1)},
}
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def read(path:Path)->str:
    raw=path.read_bytes()
    return raw.decode("utf-16" if raw.startswith((b"\xff\xfe",b"\xfe\xff"))
                      else "utf-8",errors="replace").replace("\r\n","\n").strip()

def parse(data:str,live:bool):
    raw=data.replace("\r\n","\n")
    records={"fourier":{},"hetero":{},"live":{}}
    patterns=[("live",LIVE)] if live else [
        ("fourier",FOURIER),("hetero",HETERO)]
    for line in raw.splitlines():
        for group,pattern in patterns:
            if line.startswith({"fourier":"SYMMETRIC_MAC Fourier ",
                                "hetero":"SYMMETRIC_MAC hetero_grid=",
                                "live":"LIVE_TENSOR "}[group]):
                match=pattern.fullmatch(line)
                if not match:
                    raise ValueError("Malformed native symmetric stress/VOF record")
                gs=match.groups()
                key="x".join(gs[:3])
                if group=="live":
                    key=(key,int(gs[3]))
                    steps=int(gs[4]);metrics=list(map(float,gs[5:]))
                    if steps!=8 or len(metrics)!=8:
                        raise ValueError("Incomplete 3D moving MAC/VOF history")
                    (cfl,delta,loss,diff,div,e,i,mass)=metrics
                    if not(all(math.isfinite(v) for v in metrics) and
                        0<cfl<1 and delta>1e-11 and loss>=0 and
                        diff>1e-10 and div<1e-9 and
                        abs(e)<1e-10 and abs(i)<1e-10 and abs(mass)<1e-9):
                        raise ValueError("3D live 2-phase viscous stress invariant failed")
                    metrics={"vof_cfl":cfl,"viscous_delta_m_s":delta,
                             "stress_loss_j":loss,
                             "face_vs_inviscid_delta_m_s":diff,
                             "pressure_divergence":div,
                             "energy_residual_j":e,
                             "wall_impulse_residual_kg_m_s":i,
                             "phase_mass_residual_kg":mass}
                elif group=="fourier":
                    error,diss,energy=map(float,gs[3:])
                    if not (math.isfinite(error) and 0<=error<1e-12 and
                            math.isfinite(diss) and diss>0 and
                            math.isfinite(energy) and abs(energy)<1e-12):
                        raise ValueError("Analytic 3D full-tensor reference failed")
                    metrics={"fourier_error_m_s":error,"dissipation_w":diss,
                             "energy_residual_j":energy}
                else:
                    steps=int(gs[3])
                    vel,energy,impulse=map(float,gs[4:])
                    if not (steps==9 and math.isfinite(vel) and vel>1e-10 and
                            math.isfinite(energy) and 0<=energy<1e-11 and
                            math.isfinite(impulse) and 0<=impulse<1e-10):
                        raise ValueError("Heterogeneous full 3D tensor failed")
                    metrics={"velocity_change_m_s":vel,
                             "energy_residual_j":energy,
                             "wall_impulse_residual_kg_m_s":impulse}
                if key in records[group]:
                    raise ValueError("Duplicate 3D native case")
                records[group][key]=metrics
    for label in (["live"] if live else ["fourier","hetero"]):
        if set(records[label])!=EXPECTED[label]:
            raise ValueError("Missing required real 3D stress/VOF grid")
    footer=("PASS P2_TWO_PHASE_MOVING_FULL_TENSOR assertions=" if live
            else "PASS P2_TWO_PHASE_FULL_TENSOR_VISCOUS_MAC assertions=")
    matches=re.findall(r"^"+re.escape(footer)+r"(\d+) phase_complete=false$",
                       raw,re.M)
    if len(matches)!=1 or int(matches[0])<(4844 if live else 574):
        raise ValueError("Native PASS assertion count/phase marker missing")
    return {"assertions":int(matches[0]),"records":records}

def main():
    for file in (BINARY_TENSOR,BINARY_LIVE,NATIVE_TENSOR,NATIVE_LIVE,
                 BUILD,CTEST):
        if not file.is_file():
            raise RuntimeError("Required real 3D native build/test artifact absent: "+str(file))
    logs=(read(NATIVE_TENSOR),read(NATIVE_LIVE))
    p0,p1=parse(logs[0],False),parse(logs[1],True)
    for binary,stored in ((BINARY_TENSOR,logs[0]),(BINARY_LIVE,logs[1])):
        actual=subprocess.run([str(binary)],cwd=CORE,capture_output=True,
                              text=True,timeout=75)
        if actual.returncode!=0 or actual.stderr.strip() or (
            actual.stdout.replace("\r\n","\n").strip()!=stored):
            raise RuntimeError("Independent native 3D tensor executable replay mismatched")
    ctest=read(CTEST)
    count=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctest)
    count=int(count.group(1)) if count else 0
    if count<143 or "physics_p2_two_phase_full_tensor_mac_partial" not in ctest or (
        "physics_p2_two_phase_full_tensor_live_partial" not in ctest):
        raise RuntimeError("Missing full native CTest 143+ evidence")
    fake={
        "Fourier_missing_grid":("\n".join(
            x for x in logs[0].splitlines() if
            "Fourier grid=8x6x5" not in x),False),
        "Fourier_nan_error":(logs[0].replace(
            "error_m_s=6.9388939039072284e-18","error_m_s=nan",1),False),
        "Fourier_fake_energy":(logs[0].replace(
            "energy_residual_j=3.0919821045691287e-18",
            "energy_residual_j=5",1),False),
        "hetero_missing_steps":(logs[0].replace(
            "hetero_grid=6x5x4 steps=9","hetero_grid=6x5x4 steps=8",1),False),
        "tensor_false_full_pass":(logs[0].replace(
            "phase_complete=false","phase_complete=true"),False),
        "tensor_low_assertions":(re.sub(
            r"assertions=574","assertions=10",logs[0]),False),
        "live_missing_grid":("\n".join(
            x for x in logs[1].splitlines()
            if "LIVE_TENSOR grid=8x6x5" not in x),True),
        "live_forged_phase_pass":(logs[1].replace(
            "full_phase=false","full_phase=true"),True),
        "live_bad_pressure_divergence":(re.sub(
            r"pressure_divergence_s_inv=[-+0-9.eE]+",
            "pressure_divergence_s_inv=0.1",logs[1],count=1),True),
        "live_fake_mass_loss":(re.sub(
            r"phase_mass_residual_kg=[-+0-9.eE]+",
            "phase_mass_residual_kg=3",logs[1],count=1),True),
        "live_no_viscous_effect":(re.sub(
            r"viscous_delta_m_s=[-+0-9.eE]+",
            "viscous_delta_m_s=0",logs[1],count=1),True),
        "live_energy_mismatch":(re.sub(
            r"stress_pressure_energy_residual_j=[-+0-9.eE]+",
            "stress_pressure_energy_residual_j=9",logs[1],count=1),True),
    }
    rejected=[]
    for label,(bad,live) in fake.items():
        orig=logs[1] if live else logs[0]
        if bad==orig:
            raise RuntimeError("Forgery mutation not applied: "+label)
        try:parse(bad,live)
        except (ValueError,TypeError):rejected.append(label)
        else:raise RuntimeError("Forged native physical trace passed: "+label)
    gate=subprocess.run(["py","-3",str(CORE/"scripts"/"phase_gate.py")],
                        cwd=CORE,capture_output=True,text=True,timeout=210)
    GATE.write_text(gate.stdout+gate.stderr,encoding="utf-8")
    if gate.returncode!=0 or "ALL_COMPLETE=False" not in gate.stdout or (
        "P2" not in gate.stdout.split("GATES_INCOMPLETE=")[-1]):
        raise RuntimeError("Official P2 full phase gate did not remain INCOMPLETE")
    evidence={
        "phase":"P2",
        "milestone":"real_moving_3D_two_phase_full_tensor_viscosity_PCg",
        "partial_accepted":True,
        "phase_complete":False,
        "physically_sharp_two_phase_stress_validated":False,
        "global_3D_two_way_FSI_validated":False,
        "experimental_MARIN_full_6s_three_grid_pass":False,
        "unreal58_native_E2E_current_full_chain":False,
        "native_tensor_assertions":p0["assertions"],
        "native_moving_two_phase_assertions":p1["assertions"],
        "full_ctest_suites":count,
        "synthetic_native_trace_forgery_rejections":rejected,
        "manufactured_and_heterogeneous_cases":p0["records"],
        "moving_two_phase_cases":{
            f"{key[0]}_oblique{key[1]}":value
            for key,value in p1["records"]["live"].items()},
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "artifact_sha256":{
            "tensor_native_binary":sha(BINARY_TENSOR),
            "live_native_binary":sha(BINARY_LIVE),
            "tensor_native_log":sha(NATIVE_TENSOR),
            "live_native_log":sha(NATIVE_LIVE),
            "build_log":sha(BUILD),
            "ctest_log":sha(CTEST),
            "phase_gate_log":sha(GATE),
        },
        "open_full_phase_acceptance":[
            "stress-continuous moving sharp water/air PLIC surface and capillary flux",
            "conservative combined MAC dual-volume momentum advection/viscosity/body force",
            "actual rotating/deformable rigid 3D dynamic two-way fluid-solid impulse and energy closure",
            "full >=6-second SPHERIC/MARIN H1-H4 P1-P8 experimental 3-grid PASS",
            "repeatable UE5.8 native-E2E for the coupled full 3D path",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(evidence,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_3D_FULL_TENSOR_PARTIAL=PASS "
          f"TENSOR_ASSERTIONS={p0['assertions']} "
          f"MOVING_ASSERTIONS={p1['assertions']} "
          f"CTEST={count} FORGED_TRACES_REJECTED={len(rejected)}")
    print("FULL_MOVING_SURFACE_STRESS=NOT_VALIDATED "
          "FULL_DYNAMIC_FSI=NOT_VALIDATED FULL_P2=INCOMPLETE")
    print("REPORT="+str(REPORT))
if __name__=="__main__":
    main()
