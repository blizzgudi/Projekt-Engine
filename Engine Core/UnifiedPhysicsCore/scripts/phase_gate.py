"""Read-only evidence inspection for P0..P8. Conservative: never infer completion from file names alone."""
from __future__ import annotations
import json
import hashlib
import subprocess
from pathlib import Path
from datetime import datetime, timezone

HERE = Path(__file__).resolve().parent
CORE = HERE.parent
PROJECT = CORE.parents[1]
LAB = PROJECT / "ENGINE TEST STUBE"
BUILDS = LAB / "unreal" / "builds" / "EngineMaterialLab_ue58"
UPROJECT = LAB / "unreal" / "ENGINE_TEST_STUBE" / "ENGINE_TEST_STUBE.uproject"
LOG = LAB / "unreal" / "ENGINE_TEST_STUBE" / "Saved" / "Logs" / "ENGINE_TEST_STUBE.log"

def git_rev() -> str:
    cmd = subprocess.run(["git","-C",str(PROJECT),"rev-parse","--short","HEAD"],
                         capture_output=True,text=True,timeout=20)
    return cmd.stdout.strip() if cmd.returncode==0 else "unavailable"

def ctest() -> dict:
    ctest_exe = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe")
    if not ctest_exe.exists(): return {"ok":False,"message":"ctest missing"}
    p = subprocess.run([str(ctest_exe),"--test-dir",str(CORE/"build"),"--output-on-failure"],
                       capture_output=True,text=True,timeout=90)
    return {"ok":p.returncode==0 and "100% tests passed" in p.stdout,
            "tail":p.stdout[-1800:], "exit":p.returncode}

def p1_evidence() -> dict:
    report_path = LAB / "runs" / "p1_acceptance.json"
    if not report_path.exists():
        return {"ok":False,"why":"No P1 native acceptance result"}
    try:
        data=json.loads(report_path.read_text(encoding="utf-8"))
        if data.get("phase")!="P1" or data.get("accepted") is not True:
            raise ValueError("P1 quantitative acceptance is not PASS")
        checks=data.get("checks",[])
        if len(checks)<30 or not all(item.get("pass") is True for item in checks):
            raise ValueError("Insufficient/failed P1 quantitative checks")
        hashes=data.get("source_sha256",{})
        if len(hashes)<8:
            raise ValueError("Incomplete P1 source fingerprint list")
        for filename,expected in hashes.items():
            path=CORE/filename
            if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=expected:
                raise ValueError(f"Changed or missing P1 evidence source {filename}")
        dry=data.get("ritter_dry_bed",[])
        wet=data.get("stoker_wet_bed",[])
        if len(dry)!=3 or len(wet)!=3:
            raise ValueError("Independent analytical benchmark rows missing")
        return {"ok":True,"why":f"{len(checks)} quantitative checks, Ritter + Stoker and source hashes verified"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p7_sparse_partial_evidence() -> dict:
    path=LAB / "runs" / "p7_sparse_partial.json"
    if not path.is_file():
        return {"ok":False,"why":"No native P7 sparse 3D LOD partial report"}
    try:
        d=json.loads(path.read_text(encoding="utf-8"))
        if d.get("phase")!="P7" or d.get("partial_accepted") is not True or d.get("phase_complete") is not False:
            raise ValueError("P7 CPU LOD partial report flags invalid")
        checks=d.get("checks",[])
        if len(checks)<20 or not all(c.get("pass") is True for c in checks):
            raise ValueError("P7 sparse 3D LOD numeric checks missing")
        if d.get("native_assertions",0)<230 or len(d.get("reference",[]))!=3:
            raise ValueError("P7 native MAC/VOF LOD integration/CPU references missing")
        hashes=d.get("source_sha256",{})
        if len(hashes)<8:
            raise ValueError("P7 partial source fingerprint list incomplete")
        for name,sha in hashes.items():
            f=CORE/name
            if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P7 partial source changed: {name}")
        return {"ok":True,"why":f"{len(checks)} sparse 3D MAC/VOF LOD CPU checks with source hashes; GPU still open"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p6_full_cpu_evidence() -> dict:
    path=LAB / "runs" / "p6_acceptance.json"
    if not path.is_file():
        return {"ok":False,"why":"P6 3D stoichiometry/diffusion/thermal coupled acceptance report missing"}
    try:
        d=json.loads(path.read_text(encoding="utf-8"))
        if d.get("phase")!="P6" or d.get("accepted") is not True:
            raise ValueError("P6 quantitative model validation NOT PASS")
        c=d.get("checks",[])
        if len(c)<36 or not all(x.get("pass") is True for x in c):
            raise ValueError("P6 reaction/energy/Fick quantitative controls incomplete")
        rows=d.get("reference",[])
        if len(rows)!=6 or len([x for x in rows if x.get("scenario")=="arrhenius_kinetics"])!=3:
            raise ValueError("P6 kinetic numerical comparison absent")
        assert_count=d.get("native_assertions",{})
        if assert_count.get("mixture",0)<830 or assert_count.get("p4_contact",0)<300:
            raise ValueError("P6 3D elemental mass/heat or P4 heat feedback native test absent")
        source_hashes=d.get("source_sha256",{})
        if len(source_hashes)<11:
            raise ValueError("P6 source SHA-256 coverage incomplete")
        for name,expected in source_hashes.items():
            source=CORE/name
            if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest()!=expected:
                raise ValueError(f"P6 source changed: {name}")
        return {"ok":True,"why":f"{len(c)} reaction/3D Fick/Arrhenius/atomic mass/thermal contact checks and matching source hashes"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p5_full_cpu_evidence() -> dict:
    path=LAB / "runs" / "p5_full_acceptance.json"
    if not path.is_file():
        return {"ok":False,"why":"P5 deformable multi-body and constitutive verification missing"}
    try:
        data=json.loads(path.read_text(encoding="utf-8"))
        if data.get("phase")!="P5" or data.get("accepted") is not True:
            raise ValueError("P5 advanced numerical CPU gate is NOT PASS")
        checks=data.get("checks",[])
        if len(checks)<60 or not all(check.get("pass") is True for check in checks):
            raise ValueError("Not enough successful P5 self-contact and constitutive controls")
        if len(data.get("affine_3d_reference",[]))!=12 or len(data.get("original_reference",[]))!=11:
            raise ValueError("Constitutive or grain/fracture calibration benchmark missing")
        counts=data.get("native_checks",{})
        if (counts.get("plane",0)<10984 or counts.get("granular",0)<37000
             or counts.get("lattice",0)<22000 or counts.get("volume_self",0)<17):
            raise ValueError("Deformable/contact/grain native suite incomplete")
        source_hashes=data.get("source_sha256",{})
        if len(source_hashes)<16:
            raise ValueError("P5 source coverage incomplete")
        for filename,sha in source_hashes.items():
            f=CORE/filename
            if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P5 advanced code changed since test: {filename}")
        return {"ok":True,"why":f"{len(checks)} full P5 CPU controls plus "+
                "nonbonded 3D self/multibody/fracture-contacts and affine stress-strain references"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p5_bounded_partial_evidence() -> dict:
    path = LAB / "runs" / "p5_acceptance.json"
    if not path.is_file():
        return {"ok":False,"why":"P5 bounded 3D CPU solid/fracture/granular acceptance missing"}
    try:
        data=json.loads(path.read_text(encoding="utf-8"))
        if (data.get("phase")!="P5" or data.get("partial_accepted") is not True
            or data.get("phase_complete") is not False):
            raise ValueError("P5 bounded CPU-partial milestone NOT approved")
        checks=data.get("checks",[])
        if len(checks)<45 or not all(item.get("pass") is True for item in checks):
            raise ValueError("Incomplete P5 numeric benchmarks/invariants")
        native=data.get("native_checks",{})
        if native.get("plane",0)<10984 or native.get("lattice",0)<22000 or native.get("granular",0)<37000:
            raise ValueError("P5 contact, volumetric fracture or granular checks missing")
        rows=data.get("reference",[])
        if len(rows)!=11:
            raise ValueError("Missing independent Hooke/collision/fracture/CPU results")
        hashes=data.get("source_sha256",{})
        if len(hashes)<13:
            raise ValueError("P5 critical source fingerprints incomplete")
        for filename,sha in hashes.items():
            source=CORE / filename
            if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P5 source missing/changed since test: {filename}")
        return {"ok":True,"why":f"{len(checks)} quantitative CPU acceptance controls, "+
                "lattice fragments/voxel damage and DEM contact/energy/CPU; source hashes verified"}
    except (ValueError,TypeError,KeyError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p5_partial_evidence() -> dict:
    path=LAB / "runs" / "p5_contact_partial.json"
    if not path.is_file():
        return {"ok":False,"why":"P5 plane contact partial evidence missing"}
    try:
        data=json.loads(path.read_text(encoding="utf-8"))
        if (data.get("phase")!="P5" or
            data.get("partial_accepted") is not True or
            data.get("phase_complete") is not False):
            raise ValueError("P5 contact partial flags invalid")
        if data.get("native_checks",0)<10984:
            raise ValueError("Plane contact native assertions missing")
        for filename,sha in data.get("source_sha256",{}).items():
            source=CORE/filename
            if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P5 plane contact source mismatch: {filename}")
        return {"ok":True,"why":"Plane and restitution checks from prior P5 milestone verified"}
    except (ValueError,TypeError,KeyError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p4_complete_evidence() -> dict:
    report_path = LAB / "runs" / "p4_acceptance.json"
    if not report_path.exists():
        return {"ok":False,"why":"P4 dynamic thermodynamics CPU-MVP report missing"}
    try:
        data=json.loads(report_path.read_text(encoding="utf-8"))
        if data.get("phase")!="P4" or data.get("accepted") is not True or data.get("phase_complete") is not True:
            raise ValueError("P4 coupled CPU acceptance not approved")
        checks=data.get("checks",[])
        if len(checks)<25 or not all(c.get("pass") is True for c in checks):
            raise ValueError("Insufficient/failed P4 thermo-fluid quantitative checks")
        if data.get("native_thermal_assertions",0)<800 or data.get("native_phase_bridge_assertions",0)<25:
            raise ValueError("P4 thermal phase tests missing")
        if len(data.get("reference",[]))!=4:
            raise ValueError("P4 Neumann heat reference resolution missing")
        native=data.get("native_stdout",{})
        if not all("PASS:" in native.get(k,"") for k in ("thermal","phase_bridge","live_advection","soil_integration")):
            raise ValueError("P4 live MAC/P3 material bridge native proof missing")
        hashes=data.get("source_sha256",{})
        if len(hashes)<15:
            raise ValueError("P4 full source fingerprints incomplete")
        for path,sha in hashes.items():
            actual=CORE/path
            if not actual.is_file() or hashlib.sha256(actual.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P4 source changed after acceptance: {path}")
        return {"ok":True,"why":f"{len(checks)} verified coupled 3D heat/phase/advection/soil gates; 4 Neumann references, source hashes"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p4_partial_evidence() -> dict:
    report_path = LAB / "runs" / "p4_partial_acceptance.json"
    if not report_path.exists():
        return {"ok":False,"why":"P4 3D thermal partial report missing"}
    try:
        data=json.loads(report_path.read_text(encoding="utf-8"))
        if data.get("phase")!="P4" or data.get("partial_accepted") is not True or data.get("phase_complete") is not False:
            raise ValueError("P4 partial scope evidence invalid")
        checks=data.get("checks",[])
        if len(checks)<22 or not all(c.get("pass") is True for c in checks):
            raise ValueError("P4 3D Fourier quantitative checks missing")
        if int(data.get("native_assertions",0))<800:
            raise ValueError("P4 phase-change and thermal regression missing")
        if len(data.get("reference",[]))!=4:
            raise ValueError("P4 3D Neumann benchmark grid count missing")
        hashes=data.get("source_sha256",{})
        if len(hashes)<6:
            raise ValueError("P4 source hash coverage incomplete")
        for name,sha in hashes.items():
            p=CORE/name
            if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P4 source missing or changed: {name}")
        return {"ok":True,"why":f"{len(checks)} quantitative checks, water latent phase plateaus, 3D Fourier and static runtime snapshot; source hashes verified; no live phase/flow coupling"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p3_evidence() -> dict:
    report_path = LAB / "runs" / "p3_acceptance.json"
    if not report_path.exists():
        return {"ok":False,"why":"P3 CPU material-voxel acceptance report missing"}
    try:
        data=json.loads(report_path.read_text(encoding="utf-8"))
        if data.get("phase")!="P3" or data.get("accepted") is not True:
            raise ValueError("P3 quantitative acceptance not approved")
        checks=data.get("checks",[])
        if len(checks)<45 or not all(c.get("pass") is True for c in checks):
            raise ValueError("Insufficient or failed P3 quantitative checks")
        if int(data.get("body_native_assertions",0))<500:
            raise ValueError("3D body/porosity native tests missing")
        if float(data.get("buoyancy_relative_error",1))>=0.05:
            raise ValueError("Buoyancy check outside tolerance")
        rows=data.get("reference",[])
        if len(rows)!=12:
            raise ValueError("Missing Darcy/integrated 3D results")
        hashes=data.get("source_sha256",{})
        if len(hashes)<9:
            raise ValueError("Not all P3 source files fingerprinted")
        for filename,sha in hashes.items():
            source=CORE/filename
            if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"Missing or changed P3 source: {filename}")
        return {"ok":True,"why":f"{len(checks)} quantitative P3 gates, 3D moving body, Darcy and energy checks; source hashes verified. CPU MVP only"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p2_evidence() -> dict:
    report_path = LAB / "runs" / "p2_acceptance.json"
    if not report_path.exists():
        return {"ok": False, "why": "P2 native quantitative acceptance report missing"}
    try:
        data=json.loads(report_path.read_text(encoding="utf-8"))
        if data.get("phase")!="P2" or data.get("accepted") is not True:
            raise ValueError("P2 acceptance not approved")
        checks=data.get("checks",[])
        if len(checks)<30 or not all(entry.get("pass") is True for entry in checks):
            raise ValueError("Missing or failed quantitative acceptance checks")
        hashes=data.get("source_sha256",{})
        for filename,expected in hashes.items():
            path=CORE / filename
            if not path.is_file():
                raise ValueError(f"Missing P2 source: {filename}")
            actual=hashlib.sha256(path.read_bytes()).hexdigest()
            if actual != expected:
                raise ValueError(f"P2 source changed since verified: {filename}")
        if len(hashes)<7:
            raise ValueError("Insufficient P2 source fingerprint coverage")
        return {"ok":True,"why":f"{len(checks)} quantitative P2 checks and source hashes verified"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def main() -> None:
    tests = ctest()
    p1 = p1_evidence()
    p2 = p2_evidence()
    p3 = p3_evidence()
    p4_partial = p4_partial_evidence()
    p4 = p4_complete_evidence()
    p5 = p5_bounded_partial_evidence()
    p5_partial = p5_partial_evidence()
    p5_full = p5_full_cpu_evidence()
    p6 = p6_full_cpu_evidence()
    p7_partial = p7_sparse_partial_evidence()
    ue_bin = BUILDS / "Binaries" / "Win64" / "UnrealEditor-EngineMaterialLab.dll"
    log = LOG.read_text(encoding="utf-8",errors="replace") if LOG.exists() else ""
    plugin_built = ue_bin.exists() and ue_bin.stat().st_size>0
    native_module = "EngineMaterialLab UPC started" in log
    # Manual acceptance evidence is mandatory for remaining phase closures.
    # Prototypes and smoke tests cannot accidentally turn a phase green.
    phases = {
       "P0":{"status":"PASS" if tests["ok"] else "FAIL",
             "evidence":"MSVC/CTest C++ core regression"},
       "P1":{"status":"PASS" if tests["ok"] and p1["ok"] else "PARTIAL",
             "evidence":"2D shallow-water CPU baseline, "+p1["why"]+
             "; 3D/viscosity and subgrid interface physics remain out of P1 scope"},
       "P2":{"status":"PASS" if p2["ok"] and tests["ok"] else "PARTIAL",
              "evidence":"Selected MAC+VOF coupled 3D CPU prototype; "+p2["why"]+
              "; full 3D multi-material interaction is P3"},
       "P3":{"status":"PASS" if p3["ok"] and tests["ok"] else "PARTIAL",
             "evidence":"Porous 3D soil/free-water, pressure load, voxel-body displacement, isolated mass and work-energy ledgers; "+p3["why"]+
             "; high-fidelity moving-wall impulse/energy FSI remains out of scope"},
       "P4":{"status":"PASS" if tests["ok"] and p4["ok"] and p4_partial["ok"] else "PARTIAL",
             "evidence":"3D CPU thermodynamics MVP: "+p4["why"]+
             "; thermal, liquid advection and P3-gravity heat budgets conserved in their test domains; no gas hydrodynamics or fully unified mechanical-energy closure"},
       "P5":{"status":"PASS" if (tests["ok"] and p5["ok"] and p5_partial["ok"] and p5_full["ok"]) else "PARTIAL",
             "evidence":"Bounded 3D CPU mass-spring/fracture and spherical DEM contact MVP; "+
             p5_full["why"]+"; preliminary contact "+p5_partial["why"]+
             "; full continuum FEM/MPM, accurate arbitrary surface contact and global FSI/thermal energy remain unverified"},
       "P6":{"status":"PASS" if tests["ok"] and p6["ok"] else "PARTIAL",
             "evidence":"Restricted 3D reactive-material CPU-MVP: "+p6["why"]+
             "; generic registered synthetic species, not calibrated real chemistry, not globally coupled multiphase-CFD"},
       "P7":{"status":"PARTIAL","evidence":"P7 sparse 3D render LOD partial: "+p7_partial["why"]+
             "; GPU execution/CPU parity, LOD streaming and latency bounds remain unverified"},
       "P8":{"status":"PARTIAL" if plugin_built and native_module else "NOT_RUN",
             "evidence":"UE 5.8 plugin build="+str(plugin_built)+" native module load="+str(native_module)
             +"; PIE/rendering/native clean exit not verified"},
    }
    out={
      "generated_utc":datetime.now(timezone.utc).isoformat(),
      "project_root":str(PROJECT),"git_commit":git_rev(),"ctest":tests,
      "phases":phases,"all_phases_complete":all(p["status"]=="PASS" for p in phases.values())
    }
    dest=LAB/"runs"/"unified_physics_gate.json"
    dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_text(json.dumps(out,indent=2,ensure_ascii=False),encoding="utf-8")
    print("GATES_PASS="+",".join(k for k,v in phases.items() if v["status"]=="PASS"))
    print("GATES_INCOMPLETE="+",".join(k for k,v in phases.items() if v["status"]!="PASS"))
    print("ALL_COMPLETE="+str(out["all_phases_complete"]))
    print("EVIDENCE="+str(dest))
    if not tests["ok"]: raise SystemExit(1)
if __name__=="__main__":main()
