"""Read-only evidence inspection for P0..P8. Conservative: never infer completion from file names alone."""
from __future__ import annotations
import json
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

def main() -> None:
    tests = ctest()
    ue_bin = BUILDS / "Binaries" / "Win64" / "UnrealEditor-EngineMaterialLab.dll"
    log = LOG.read_text(encoding="utf-8",errors="replace") if LOG.exists() else ""
    plugin_built = ue_bin.exists() and ue_bin.stat().st_size>0
    native_module = "EngineMaterialLab UPC started" in log
    # Manual acceptance evidence is mandatory for remaining phase closures.
    # Prototypes and smoke tests cannot accidentally turn a phase green.
    phases = {
       "P0":{"status":"PASS" if tests["ok"] else "FAIL",
             "evidence":"MSVC/CTest C++ core regression"},
       "P1":{"status":"PARTIAL","evidence":"2D dam-break + obstacle/mass unit tests; independent reference data missing"},
       "P2":{"status":"PARTIAL","evidence":"3D conservative volume transport and separate MAC projection validated; coupled 3D fluid missing"},
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
