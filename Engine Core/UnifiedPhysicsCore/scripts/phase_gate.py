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
    p2 = p2_evidence()
    ue_bin = BUILDS / "Binaries" / "Win64" / "UnrealEditor-EngineMaterialLab.dll"
    log = LOG.read_text(encoding="utf-8",errors="replace") if LOG.exists() else ""
    plugin_built = ue_bin.exists() and ue_bin.stat().st_size>0
    native_module = "EngineMaterialLab UPC started" in log
    # Manual acceptance evidence is mandatory for remaining phase closures.
    # Prototypes and smoke tests cannot accidentally turn a phase green.
    phases = {
       "P0":{"status":"PASS" if tests["ok"] else "FAIL",
             "evidence":"MSVC/CTest C++ core regression"},
       "P1":{"status":"PARTIAL","evidence":"Ritter analytical dry-bed reference + three-grid depth/momentum convergence tested; front bias and additional references unresolved"},
       "P2":{"status":"PASS" if p2["ok"] and tests["ok"] else "PARTIAL",
              "evidence":"Selected MAC+VOF coupled 3D CPU prototype; "+p2["why"]+
              "; full 3D multi-material interaction is P3"},
       "P3":{"status":"PARTIAL","evidence":"single-cell porous infiltration conserves mass; integrated 3D FSI missing"},
       "P4":{"status":"PARTIAL","evidence":"enthalpy-based pure-water phases in a closed sample; multi-cell heat/energy validation missing"},
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
