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
    p4 = p4_partial_evidence()
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
       "P4":{"status":"PARTIAL",
             "evidence":"3D thermal partial validation: "+p4["why"]+
             "; phase-changing 3D MAC transport, solid/gas volume remap, P3 thermal feedback and global energy closure still missing"},
       "P5":{"status":"PARTIAL","evidence":"3D mass-spring + fracture; stable volumetric solids/granular contact missing"},
       "P6":{"status":"PARTIAL","evidence":"composition mass bookkeeping; calibrated thermochemical laws missing"},
       "P7":{"status":"PARTIAL","evidence":"sparse CPU chunks and benchmarks; GPU/LOD integration missing"},
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
