"""P8 true-native UE5.8 adapter acceptance.

Verifies independently:
  * UAT BuildPlugin Win64 Editor/Game Dev and Game Shipping build logs
  * loaded native Editor-module DLL fingerprint
  * independently relaunched UE Game AND real UE Editor PIE
  * process exit 0, no critical error or timeout, real 3D MAC/VOF markers
  * nontrivial UE render target PNG with dimensions+pixel variance
  * mass conservation and changed 3D water distribution
  * matching source fingerprints, unchanged original UPC headers

Nothing from source comments or synthetic/mock evidence is sufficient.
"""
from __future__ import annotations
import hashlib,json,os
from datetime import datetime,timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
PROJECT=CORE.parents[1]
LAB=PROJECT/"ENGINE TEST STUBE"
UE=LAB/"unreal"/"ENGINE_TEST_STUBE"
PLUGIN=UE/"Plugins"/"EngineMaterialLab"
PACKAGED=LAB/"unreal"/"builds"/"EngineMaterialLab_P8"
RUNS=LAB/"runs"
OUT=RUNS/"p8_acceptance.json"
UAT=Path(os.environ.get("APPDATA",str(Path.home()/"AppData"/"Roaming"))) / "Unreal Engine" / "AutomationTool" / "Logs" / "C+Program+Files+Epic+Games+UE_5.8" / "Log.txt"
CRITICAL_FILES=[
  "Engine Core/UnifiedPhysicsCore/include/upc/core.hpp",
  "Engine Core/UnifiedPhysicsCore/include/upc/mac3d.hpp",
  "Engine Core/UnifiedPhysicsCore/include/upc/coupled_water3d.hpp",
  "Engine Core/UnifiedPhysicsCore/include/upc/viscous_mac3d.hpp",
  "Engine Core/UnifiedPhysicsCore/include/upc/fluid3d_module.hpp",
  "ENGINE TEST STUBE/unreal/ENGINE_TEST_STUBE/ENGINE_TEST_STUBE.uproject",
  "ENGINE TEST STUBE/unreal/ENGINE_TEST_STUBE/Plugins/EngineMaterialLab/EngineMaterialLab.uplugin",
  "ENGINE TEST STUBE/unreal/ENGINE_TEST_STUBE/Plugins/EngineMaterialLab/Source/EngineMaterialLab/EngineMaterialLab.Build.cs",
  "ENGINE TEST STUBE/unreal/ENGINE_TEST_STUBE/Plugins/EngineMaterialLab/Source/EngineMaterialLab/Private/EngineMaterialLabModule.cpp",
  "ENGINE TEST STUBE/unreal/ENGINE_TEST_STUBE/Plugins/EngineMaterialLab/Source/EngineMaterialLab/ThirdParty/include/upc/core.hpp",
  "ENGINE TEST STUBE/unreal/ENGINE_TEST_STUBE/Plugins/EngineMaterialLab/Source/EngineMaterialLab/ThirdParty/include/upc/mac3d.hpp",
  "ENGINE TEST STUBE/unreal/ENGINE_TEST_STUBE/Plugins/EngineMaterialLab/Source/EngineMaterialLab/ThirdParty/include/upc/coupled_water3d.hpp",
  "ENGINE TEST STUBE/unreal/ENGINE_TEST_STUBE/Plugins/EngineMaterialLab/Source/EngineMaterialLab/ThirdParty/include/upc/viscous_mac3d.hpp",
  "ENGINE TEST STUBE/unreal/ENGINE_TEST_STUBE/Plugins/EngineMaterialLab/Source/EngineMaterialLab/ThirdParty/include/upc/fluid3d_module.hpp",
  "ENGINE TEST STUBE/unreal/ENGINE_TEST_STUBE/Content/Python/p8_pie_probe.py",
  "ENGINE TEST STUBE/scripts/p8_native_e2e.py",
  "Engine Core/UnifiedPhysicsCore/docs/P8_UNREAL58_NATIVE_E2E_ABNAHME.md",
  "Engine Core/UnifiedPhysicsCore/scripts/p8_acceptance.py",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def parse(path:Path)->dict:
    return json.loads(path.read_text(encoding="utf-8"))
def main():
    checks=[]
    def gate(name,okay,details=None,threshold=None):
        checks.append({"name":name,"pass":bool(okay),"details":details,"threshold":threshold})
    missing=[name for name in CRITICAL_FILES if not (PROJECT/name).is_file()]
    gate("critical_unreal_and_upc_source_files",not missing,missing,[])
    if missing:
        raise RuntimeError(f"P8 critical project files missing: {missing}")
    fp={name:sha(PROJECT/name) for name in CRITICAL_FILES}
    third=PLUGIN/"Source"/"EngineMaterialLab"/"ThirdParty"/"include"/"upc"
    origin=CORE/"include"/"upc"
    for header in ("core.hpp","mac3d.hpp","coupled_water3d.hpp","viscous_mac3d.hpp","fluid3d_module.hpp"):
        ok=sha(third/header)==sha(origin/header)
        gate("independent_upc_header_matches_unreal_adapter_"+header,ok,ok,True)
    source=PLUGIN/"Source"/"EngineMaterialLab"/"Private"/"EngineMaterialLabModule.cpp"
    editor_dll=PLUGIN/"Binaries"/"Win64"/"UnrealEditor-EngineMaterialLab.dll"
    package_dll=PACKAGED/"Binaries"/"Win64"/"UnrealEditor-EngineMaterialLab.dll"
    gate("UE58_compiled_plugin_binary_exists",
         editor_dll.is_file() and editor_dll.stat().st_size>50000,
         editor_dll.stat().st_size if editor_dll.is_file() else 0,50000)
    if not editor_dll.is_file() or not package_dll.is_file():
        raise RuntimeError("UE native module or packaged binary is absent")
    same=sha(editor_dll)==sha(package_dll)
    gate("latest_native_plugin_matches_packaged_DLL",same,same,True)
    age=editor_dll.stat().st_mtime-source.stat().st_mtime
    gate("native_binary_not_older_than_cpp",age>=0,round(age,3),0)
    plugin_text=(PLUGIN/"EngineMaterialLab.uplugin").read_text(encoding="utf-8")
    plugin_json=json.loads(plugin_text)
    gate("single_UE5_runtime_adapter",
         len(plugin_json.get("Modules",[]))==1 and
         plugin_json["Modules"][0].get("Name")=="EngineMaterialLab" and
         plugin_json["Modules"][0].get("Type")=="Runtime",
         plugin_json.get("Modules",[]),"1 runtime module")
    gate("plugin_compiler_UE_version",
         '"EngineAssociation": "5.8"' in (UE/"ENGINE_TEST_STUBE.uproject").read_text(encoding="utf-8"),
         "5.8","5.8")
    buildlog=""
    if UAT.is_file():
        buildlog=UAT.read_text(encoding="utf-8",errors="replace")
    marker=[
      "EngineMaterialLab_P8",
      "UnrealEditor Win64 Development",
      "UnrealGame Win64 Development",
      "UnrealGame Win64 Shipping",
      "BUILD SUCCESSFUL",
      "AutomationTool exiting with ExitCode=0",
    ]
    gate("native_UAT_BuildPlugin_Dev_Shipping_PASS",
         all(m in buildlog for m in marker) and
         UAT.stat().st_mtime>=source.stat().st_mtime if UAT.is_file() else False,
         [m for m in marker if m not in buildlog],[])
    # Copy log to project evidence so future unrelated UAT runs cannot erase it.
    if buildlog and all(m in buildlog for m in marker):
        (RUNS/"p8_uat_build_native.log").write_text(buildlog,encoding="utf-8")
    result={}
    for mode in ("game","pie"):
        p=RUNS/("p8_"+mode+"_process.json")
        if not p.is_file():raise RuntimeError("P8 native "+mode+" process report missing")
        run=parse(p)
        result[mode]=run
        case=run.get("checks",{})
        gate(mode+"_native_process_exit_clean",
             run.get("accepted") is True and
             run.get("process_exit_code")==0 and
             run.get("timed_out") is False and
             case and all(v is True for v in case.values()),
             run.get("process_exit_code"),0)
        native=run.get("native_json",{})
        gate(mode+"_actual_3D_Mac_VOF_conservation",
             native.get("native_mac_3d") is True and
             native.get("mass_ok") is True and native.get("flow_ok") is True and
             abs(native.get("initial_mass_kg",0)-128.0)<1e-8 and
             abs(native.get("final_mass_kg",0)-128.0)<1e-6 and
             native.get("max_mass_residual_kg",1)<1e-6 and
             native.get("max_outside_vof",0)>0.04,
             native,{"mass":128.0,"flow":">0.04"})
        gate(mode+"_live_masked_viscosity_executed",
             native.get("kinematic_viscosity_m2_s",0)>0 and
             native.get("viscous_substeps",0)>=85,
             {"nu_m2_s":native.get("kinematic_viscosity_m2_s"),
              "viscous_substeps":native.get("viscous_substeps")},
             "positive nu and >=85 live viscosity steps")
        raw=UE/"Saved"/"EngineMaterialLab"/"P8Native"/(
            f"P8_{mode.upper()}_UE58_Native_3DWater.png")
        gate(mode+"_actual_native_render_png_hash",
             raw.is_file() and sha(raw)==run.get("image",{}).get("sha256") and
             run["image"].get("distinct_sample_colors",0)>=40 and
             min(run["image"].get("rgb_stddev",[0]))>5 and
             run["image"].get("width")==960 and
             run["image"].get("height")==540,
             {"sha256":sha(raw) if raw.is_file() else None,
              "colors":run.get("image",{}).get("distinct_sample_colors")},
             "native 960x540, colorful/nonuniform")
        log=RUNS/("p8_"+mode+"_unreal_native.log")
        gate(mode+"_real_process_log_fingerprint",
             log.is_file() and sha(log)==run.get("native_log_sha256") and
             f"P8 NATIVE E2E kind={mode.upper()}" in log.read_text(encoding="utf-8",
                    errors="replace"),
             sha(log) if log.is_file() else None,
             run.get("native_log_sha256"))
    gate("independent_native_GAME_and_real_PIE_sessions",
         all(result[m].get("accepted") is True for m in ("game","pie")),
         [result[m].get("elapsed_s") for m in ("game","pie")],"both PASS")
    passed=all(item["pass"] for item in checks)
    OUT.parent.mkdir(parents=True,exist_ok=True)
    report={
      "phase":"P8","schema_version":1,"accepted":passed,
      "scope":"Native UE5.8 plugin build and real clean Game + Editor PIE + SceneCapture GPU-rendered 3D MAC/VOF with opt-in masked viscosity E2E; excluding full P3-P6 multi-material Unreal runtime and P7 GPU LOD",
      "generated_utc":datetime.now(timezone.utc).isoformat(),
      "checks":checks,"sources_sha256":fp,"native_plugin_dll_sha256":sha(editor_dll),
      "process_proof":{
        m:{"report":str(RUNS/("p8_"+m+"_process.json")),
           "image_sha256":result[m].get("image",{}).get("sha256"),
           "native_log_sha256":result[m].get("native_log_sha256"),
           "exit_code":result[m].get("process_exit_code"),
           "elapsed_s":result[m].get("elapsed_s")} for m in ("game","pie")
      },
      "limitations":[
        "The adapter currently visualizes P2 3D MAC water only; full P3-P6 world/asset integration is not proven",
        "Instanced cubes are debug renderer rather than photorealistic transparent water",
        "Screenshot is from real UE SceneCapture2D, not user viewport",
        "UE packaged BuildPlugin success does not imply complete cooked standalone Shipping game",
        "P7 GPU/LOD/performance game-production readiness remains open",
      ],
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print("P8_ACCEPTED="+str(passed))
    print("P8_CHECKS="+str(len(checks)))
    print("P8_PLUGIN_BINARY_SHA256="+sha(editor_dll))
    print("P8_GAME_EXIT="+str(result["game"].get("process_exit_code")))
    print("P8_PIE_EXIT="+str(result["pie"].get("process_exit_code")))
    print("P8_NATIVE_EVIDENCE="+str(OUT))
    if not passed:
        raise SystemExit("P8 native UE5.8 E2E gate failed: "+
                         ",".join(x["name"] for x in checks if not x["pass"]))
if __name__=="__main__":main()
