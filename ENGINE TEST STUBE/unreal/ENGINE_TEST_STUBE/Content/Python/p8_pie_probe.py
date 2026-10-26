"""UE 5.8 editor PIE E2E: native plugin spawns MAC/VOF 3D world,
captures render and native state, PIE session ends before clean Editor exit.
This file is invoked only with -ExecCmds="py import p8_pie_probe".
"""
import unreal
unreal.log("P8_PIE_PYTHON_LOADED")
s={"ticks":0,"began":False,"end_requested":False,"exit_requested":False}
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.log("P8_PIE_CALLBACK_REGISTERED")
def _tick(delta):
    s["ticks"]+=1
    t=s["ticks"]
    if t==35 and not s["began"]:
        s["began"]=True
        try:
            level.editor_play_simulate()
            unreal.log("P8_PIE_REQUESTED tick="+str(t))
        except Exception as ex:
            unreal.log_error("P8_PIE_REQUEST_FAILED "+repr(ex))
    if t>=720 and s["began"] and not s["end_requested"]:
        s["end_requested"]=True
        try:
            level.editor_request_end_play()
            unreal.log("P8_PIE_END_REQUESTED tick="+str(t))
        except Exception as ex:
            unreal.log_error("P8_PIE_END_FAILED "+repr(ex))
    if t>=930 and s["end_requested"] and not s["exit_requested"]:
        if not level.is_in_play_in_editor():
            s["exit_requested"]=True
            unreal.log("P8_PIE_ENDED_CLEANLY; exit requested tick="+str(t))
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        elif t==930:
            unreal.log_error("P8_PIE_WORLD_STILL_RUNNING_AFTER_END")
handle=unreal.register_slate_post_tick_callback(_tick)
