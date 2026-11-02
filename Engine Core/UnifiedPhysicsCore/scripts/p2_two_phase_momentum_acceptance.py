"""P2 PARTIAL: independent numerical 3D two-phase momentum transport QA.

This is NOT a physical full-phase, stress, measured MARIN, moving FSI
or UE5.8 end-to-end acceptance. All source, logs and fingerprints stay
inside the existing D:\\Projekt Engine. Re-run native executables, compare
actual evidence, reject forged/omitted data and inspect the official gate.
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
PREFIX="p2_two_phase_momentum"
CONSERVATIVE=CORE/"build"/"upc_p2_two_phase_consistent_momentum_tests.exe"
LIVE=CORE/"build"/"upc_p2_two_phase_live_momentum_tests.exe"
BUILD=RUNS/(PREFIX+"_build.txt")
CTEST=RUNS/(PREFIX+"_ctest.txt")
CONSERVATIVE_LOG=RUNS/(PREFIX+"_conservative_native.txt")
LIVE_LOG=RUNS/(PREFIX+"_live_native.txt")
GATE_LOG=RUNS/(PREFIX+"_phase_gate.txt")
REPORT=RUNS/(PREFIX+"_partial.json")
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_two_phase_consistent_momentum3d.hpp",
    "include/upc/p2_two_phase_momentum_vof3d.hpp",
    "include/upc/p2_two_phase_mac_vof3d.hpp",
    "include/upc/p2_two_phase_mac_pressure3d.hpp",
    "include/upc/p2_conservative_vof_flux3d.hpp",
    "tests/p2_two_phase_consistent_momentum3d_tests.cpp",
    "tests/p2_two_phase_momentum_vof3d_tests.cpp",
    "scripts/p2_two_phase_momentum_build.cmd",
    "scripts/p2_two_phase_momentum_acceptance.py",
    "scripts/phase_gate.py",
    "docs/P2_TWO_PHASE_CONSISTENT_MOMENTUM_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
MOMENTUM=re.compile(
    r"^MOMENTUM grid=(\d+)x(\d+)x(\d+) constant=([01]) steps=(\d+)"
    r" max_reference_error=([-+0-9.eE]+)"
    r" impulse_error=([-+0-9.eE]+)"
    r" max_donor_kinetic_loss_j=([-+0-9.eE]+)"
    r" f0=([-+0-9.eE]+)$"
)
LIVE_MOMENTUM=re.compile(
    r"^LIVE_MOMENTUM grid=(\d+)x(\d+)x(\d+) steps=(\d+)"
    r" max_face_feedback_m_s=([-+0-9.eE]+)"
    r" cell_flux_impulse_error=([-+0-9.eE]+)"
    r" unclosed_mac_impulse_kg_m_s=([-+0-9.eE]+)"
    r" donor_kinetic_loss_j=([-+0-9.eE]+)"
    r" density_error_kg_m3=([-+0-9.eE]+)"
    r" full_phase=false$"
)
EXPECTED_MOMENTUM={
    ("4x4x4",1),("6x5x4",1),("8x6x5",0),("12x8x6",0)}
EXPECTED_LIVE={"4x4x4","6x5x4","8x6x5"}

def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def text(path:Path)->str:
    data=path.read_bytes()
    return data.decode("utf-16" if data.startswith((b"\xff\xfe",b"\xfe\xff"))
                       else "utf-8",errors="replace").replace("\r\n","\n").strip()
def parse(source:str,live:bool)->dict:
    rows={}
    regexp=LIVE_MOMENTUM if live else MOMENTUM
    for line in source.replace("\r\n","\n").splitlines():
        if not line.startswith("LIVE_MOMENTUM " if live else "MOMENTUM "):
            continue
        m=regexp.fullmatch(line)
        if not m:raise ValueError("Malformed native two-phase momentum row")
        values=[float(q) for q in (
            m.groups()[4:] if live else m.groups()[5:])]
        if not all(math.isfinite(q) for q in values):
            raise ValueError("Nonfinite native momentum metrics")
        key="x".join(m.groups()[:3])
        if live:
            steps=int(m.group(4))
            if key in rows or steps!=6:
                raise ValueError("Missing live six-step 3D test")
            feedback,cell_error,unclosed,loss,density=values
            if not(1e-6<feedback<2 and 0<=cell_error<1e-10 and
                   1e-3<unclosed<1e3 and 0<=loss<1 and
                   0<=density<1e-8):
                raise ValueError("Live momentum ledger passed despite missing closure/flux")
            rows[key]={"feedback_m_s":feedback,
                       "cell_flux_impulse_error":cell_error,
                       "unclosed_mac_impulse":unclosed,
                       "donor_kinetic_loss":loss,
                       "density_error":density}
        else:
            constant=int(m.group(4));steps=int(m.group(5))
            key=(key,constant)
            if key in rows or steps!=7:
                raise ValueError("Incomplete or duplicate 3D momentum grid")
            reference,impulse,loss,f0=values
            if not(0<=reference<1e-9 and 0<=impulse<1e-10 and
                   0<=f0<=1 and
                   (0<=loss<1e-9 if constant else 1e-9<loss<1)):
                raise ValueError("Conservative momentum reference/energy invalid")
            rows[key]={"reference_error":reference,"impulse_error":impulse,
                       "upwind_kinetic_loss":loss,"f0":f0}
    if set(rows)!=(EXPECTED_LIVE if live else EXPECTED_MOMENTUM):
        raise ValueError("Required 3D momentum experiment missing")
    footer=("PASS P2_TWO_PHASE_LIVE_MOMENTUM assertions="
            if live else "PASS P2_TWO_PHASE_CONSISTENT_MOMENTUM assertions=")
    matches=re.findall(r"^"+re.escape(footer)+r"(\d+) phase_complete=false$",
                       source,re.M)
    if len(matches)!=1 or int(matches[0])<(5337 if live else 39191):
        raise ValueError("Missing true native assertion footer/phase guard")
    return {"native_assertions":int(matches[0]),"cases":rows}

def main():
    for path in (CONSERVATIVE,LIVE,BUILD,CTEST,CONSERVATIVE_LOG,LIVE_LOG):
        if not path.is_file():
            raise RuntimeError(f"Missing native proof: {path}")
    native0=text(CONSERVATIVE_LOG)
    native1=text(LIVE_LOG)
    conservative=parse(native0,False)
    live=parse(native1,True)
    for binary,previous in ((CONSERVATIVE,native0),(LIVE,native1)):
        actual=subprocess.run([str(binary)],cwd=CORE,capture_output=True,
                              text=True,timeout=90)
        if actual.returncode!=0 or actual.stderr.strip() or (
           actual.stdout.replace("\r\n","\n").strip()!=previous):
            raise RuntimeError("Native binary rerun disagrees with physical evidence log")
    ctest=text(CTEST)
    if ("100% tests passed, 0 tests failed out of 140" not in ctest or
        "physics_p2_two_phase_consistent_momentum_partial" not in ctest or
        "physics_p2_two_phase_live_momentum_partial" not in ctest):
        raise RuntimeError("Full 140/140 existing native CTest regression not recorded")
    falsifications={
        "missing_grid":"\n".join(
            line for line in native0.splitlines() if "grid=12x8x6" not in line),
        "fake_closed_cell_impulse":re.sub(
            r"(MOMENTUM grid=8x6x5 .*?)impulse_error=[-+0-9.eE]+",
            r"\g<1>impulse_error=0.01",native0,count=1),
        "fake_negative_upwind_loss":re.sub(
            r"(MOMENTUM grid=12x8x6 .*?)max_donor_kinetic_loss_j=[-+0-9.eE]+",
            r"\g<1>max_donor_kinetic_loss_j=-0.2",native0,count=1),
        "fake_complete":native0.replace("phase_complete=false","phase_complete=true"),
        "truncated_steps":native0.replace("steps=7","steps=6",1),
        "truncated_assertions":re.sub(r"assertions=\d+","assertions=1",native0,count=1),
        "missing_live_grid":"\n".join(
            line for line in native1.splitlines() if "grid=6x5x4" not in line),
        "suppressed_physical_gap":re.sub(
            r"(LIVE_MOMENTUM grid=8x6x5 .*?)unclosed_mac_impulse_kg_m_s=[-+0-9.eE]+",
            r"\g<1>unclosed_mac_impulse_kg_m_s=0",native1,count=1),
        "fake_full_live":native1.replace("full_phase=false","full_phase=true"),
        "fake_nan_density":re.sub(
            r"(LIVE_MOMENTUM grid=4x4x4 .*?)density_error_kg_m3=[-+0-9.eE]+",
            r"\g<1>density_error_kg_m3=nan",native1,count=1),
    }
    rejected=[]
    for label,bad in falsifications.items():
        old=native1 if label in (
            "missing_live_grid","suppressed_physical_gap",
            "fake_full_live","fake_nan_density") else native0
        if bad==old:raise RuntimeError("Mutation did not change evidence: "+label)
        try:parse(bad,bad is not native0 and label in (
            "missing_live_grid","suppressed_physical_gap",
            "fake_full_live","fake_nan_density"))
        except (ValueError,TypeError):rejected.append(label)
        else:raise RuntimeError("Forged momentum acceptance passed: "+label)
    gate=subprocess.run(["py","-3",str(CORE/"scripts"/"phase_gate.py")],
                        cwd=CORE,capture_output=True,text=True,timeout=240)
    GATE_LOG.write_text(gate.stdout+gate.stderr,encoding="utf-8")
    if gate.returncode!=0 or "ALL_COMPLETE=False" not in gate.stdout or (
        "P2" not in gate.stdout.split("GATES_INCOMPLETE=")[-1]):
        raise RuntimeError("The official full P2 phase gate did not report INCOMPLETE")
    report={
        "phase":"P2",
        "milestone":"3D_density_consistent_finite_volume_momentum_with_live_MAC_feedback",
        "partial_accepted":True,"phase_complete":False,
        "global_physical_mac_momentum_conserved":False,
        "full_dynamic_two_way_fsi":False,
        "measured_marin_6s_validated":False,
        "native_conservative_assertions":conservative["native_assertions"],
        "native_live_assertions":live["native_assertions"],
        "full_native_ctest_suites":140,
        "independent_forged_evidence_rejections":rejected,
        "conservative_3d_cases":{
            f"{grid}_constant{constant}":data
            for (grid,constant),data in conservative["cases"].items()},
        "coupled_3d_live_cases":live["cases"],
        "known_unresolved_mac_feedback_impulse_kg_m_s":{
            grid:data["unclosed_mac_impulse"]
            for grid,data in live["cases"].items()},
        "source_sha256":{name:sha(CORE/name) for name in SOURCES},
        "artifact_sha256":{
            "conservative_binary":sha(CONSERVATIVE),
            "live_binary":sha(LIVE),
            "build":sha(BUILD),
            "ctest":sha(CTEST),
            "conservative_native":sha(CONSERVATIVE_LOG),
            "live_native":sha(LIVE_LOG),
            "phase_gate":sha(GATE_LOG),
        },
        "unmet":[
            "MAC dual-volume conservative momentum/kinetic flux; remap and second projection still produce physical impulse gap",
            "Moving two-phase stress, surface tension, and full 3D Navier-Stokes",
            "Dynamic two-way deformable/rigid-body FSI with physical wall impulse",
            "Independent MARIN/SPHERIC H1-H4 P1-P8 6s three-grid measured PASS",
            "New UE5.8 native E2E of the momentum-feedback path",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(f"P2_3D_MOMENTUM_PARTIAL=True CONSERVATIVE_ASSERTS="
          f"{conservative['native_assertions']} LIVE_ASSERTS="
          f"{live['native_assertions']} FORGERY_REJECTED="
          f"{len(rejected)} CTEST=140 P2_FULL_PHASE=False")
    print("REPORT="+str(REPORT))

if __name__=="__main__":
    main()
