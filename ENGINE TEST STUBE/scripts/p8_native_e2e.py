"""Unreal Engine 5.8 real executable P8 E2E test runner.
Launches ONLY the isolated ENGINE TEST STUBE .uproject; never AI Game Studio.
One invocation tests exactly one mode: game or pie. No fake logs/screenshots.
Bounded execution with process-specific timeout and clean-zero-exit requirement.
"""
from __future__ import annotations
import argparse,hashlib,json,subprocess,time,shutil,sys
from datetime import datetime,timezone
from pathlib import Path
from PIL import Image,ImageStat
PROJECT_ROOT=Path(__file__).resolve().parents[2]
LAB=PROJECT_ROOT/"ENGINE TEST STUBE"
UNREAL=LAB/"unreal"/"ENGINE_TEST_STUBE"
PROJECT=UNREAL/"ENGINE_TEST_STUBE.uproject"
EDITOR=Path(r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe")
SAVE=UNREAL/"Saved"
RESULTS=LAB/"runs"
P8=SAVE/"EngineMaterialLab"/"P8Native"
SOURCE=UNREAL/"Content"/"Python"/"p8_pie_probe.py"
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("mode",choices=["game","pie"])
    parser.add_argument("--timeout",type=int,default=100)
    a=parser.parse_args()
    if not EDITOR.is_file() or not PROJECT.is_file():
        raise RuntimeError("Native UE5.8 editor or isolated project absent")
    if a.mode=="pie" and not SOURCE.is_file():
        raise RuntimeError("Missing dedicated Python PIE controller")
    RESULTS.mkdir(parents=True,exist_ok=True)
    P8.mkdir(parents=True,exist_ok=True)
    name=f"P8_{a.mode.upper()}_UE58_Native_3DWater"
    native_json=P8/(name+".json")
    native_image=P8/(name+".png")
    command=[str(EDITOR),str(PROJECT),"/Engine/Maps/Entry",
             "-unattended","-NoSplash","-NoSound","-RenderOffscreen",
             "-ResX=960","-ResY=540","-log"]
    if a.mode=="game":
        command+=["-game","-EngineMaterialLabAutoQuit"]
    else:
        command+=['-ExecCmds=py import p8_pie_probe']
    started=time.time()
    t0=time.monotonic()
    process=subprocess.Popen(command,stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL,cwd=str(UNREAL))
    timeout=False
    try:
        exit_code=process.wait(timeout=a.timeout)
    except subprocess.TimeoutExpired:
        timeout=True
        process.kill()
        process.wait(timeout=12)
        exit_code=process.returncode
    elapsed=time.monotonic()-t0
    log_path=SAVE/"Logs"/"ENGINE_TEST_STUBE.log"
    if not log_path.is_file():
        raise RuntimeError("Native UE log absent")
    archived_log=RESULTS/f"p8_{a.mode}_unreal_native.log"
    shutil.copy2(log_path,archived_log)
    log=archived_log.read_text(encoding="utf-8",errors="replace")
    native={}
    if native_json.exists() and native_json.stat().st_mtime>=started-2:
        native=json.loads(native_json.read_text(encoding="utf-8-sig"))
    image={}
    if native_image.exists() and native_image.stat().st_mtime>=started-2:
        with Image.open(native_image) as raw:
            raw.load()
            rgb=raw.convert("RGB")
            small=rgb.resize((120,68))
            colors=len(small.getcolors(120*68) or [])
            st=ImageStat.Stat(rgb)
            image={
                "width":rgb.width,"height":rgb.height,
                "distinct_sample_colors":colors,
                "rgb_stddev":st.stddev,"bytes":native_image.stat().st_size,
                "sha256":sha(native_image),
            }
    markers=[
      f"P8 WORLD START kind={a.mode.upper()}",
      f"P8 NATIVE E2E kind={a.mode.upper()} mass=1 flow=1 mesh=1 render=1",
      "EngineMaterialLab UPC stopped",
    ]
    if a.mode=="pie":
        markers.extend([
            "P8_PIE_PYTHON_LOADED",
            "PIE: Created PIE world",
            "P8_PIE_END_REQUESTED",
            "P8_PIE_ENDED_CLEANLY",
        ])
    checks={
        "native_exit_zero":exit_code==0 and not timeout,
        "real_editor_opened":"LogInit: Display: Engine is initialized" in log
                         or "LogInit: Display: Engine is initialized." in log,
        "native_UE58_module_loaded":"EngineMaterialLab UPC started" in log,
        "native_log_proof":all(m in log for m in markers),
        "no_critical_error":not any(m in log for m in
                       ["=== Critical error: ===","LogWindows: Error: appError",
                        "Fatal error:","Assertion failed:"]),
        "render_png_native_nontrivial":image.get("bytes",0)>4096
              and image.get("width")==960 and image.get("height")==540
              and image.get("distinct_sample_colors",0)>=40
              and min(image.get("rgb_stddev",[0]))>5,
        "native_json_matches":native.get("kind")==a.mode.upper() and
              native.get("native_mac_3d") is True and
              native.get("mass_ok") is True and native.get("flow_ok") is True and
              native.get("mesh_ok") is True and native.get("render_ok") is True and
              native.get("frames",0)>=85 and native.get("mesh_instances",0)>25 and
              native.get("max_mass_residual_kg",1)>-1 and
              native.get("max_mass_residual_kg",1)<1e-6 and
              native.get("max_outside_vof",0)>0.04 and
              native.get("ue_version","").startswith("5.8"),
    }
    record={
       "mode":a.mode,"accepted":all(checks.values()),
       "process_exit_code":exit_code,"timed_out":timeout,
       "elapsed_s":elapsed,"started_utc":datetime.fromtimestamp(
           started,timezone.utc).isoformat(),
       "native_project":str(PROJECT),"editor_executable_sha256":sha(EDITOR),
       "native_json":native,"image":image,"checks":checks,
       "native_json_sha256":sha(native_json) if native else None,
       "native_log_sha256":sha(archived_log),
       "native_log_file":str(archived_log),
    }
    target=RESULTS/f"p8_{a.mode}_process.json"
    target.write_text(json.dumps(record,indent=2,ensure_ascii=False),
                      encoding="utf-8")
    print("P8_"+a.mode.upper()+"_ACCEPTED="+str(record["accepted"]))
    print("P8_"+a.mode.upper()+"_EXIT="+str(exit_code))
    print("P8_"+a.mode.upper()+"_TIME_S="+str(round(elapsed,2)))
    print("P8_"+a.mode.upper()+"_RENDER_SHA256="+image.get("sha256",""))
    print("P8_"+a.mode.upper()+"_RESULT="+str(target))
    if not record["accepted"]:
        raise SystemExit("Native "+a.mode+" failed: "+
                         ",".join(k for k,v in checks.items() if not v))
if __name__=="__main__":
    main()
