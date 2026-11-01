"""P2 PARTIAL: reproducible native 16^3 PLIC-substep endpoint fix audit."""
from __future__ import annotations
import hashlib
import json
import re
from datetime import datetime,timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
FILES={
    "compile":RUNS/"p2_plic_substep_fix_compile.txt",
    "native":RUNS/"p2_plic_substep_fix_native.txt",
    "ctest":RUNS/"p2_plic_substep_fix_ctest.txt",
    "binary":CORE/"build"/"upc_p2_plic_substep_trace3d_tests.exe",
}
SOURCES=[
    "include/upc/p2_plic_substep_trace3d.hpp",
    "include/upc/p2_plic_endpoint_subcycle3d.hpp",
    "include/upc/p2_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_viscous_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_plic_adaptive_subcycle3d.hpp",
    "tests/p2_plic_substep_trace3d_tests.cpp",
    "tests/p2_plic_unsplit_mac3d_tests.cpp",
    "scripts/p2_plic_substep_fix_build.cmd",
    "scripts/p2_plic_substep_fix_acceptance.py",
    "docs/P2_PLIC_SUBSTEP_ENDPOINT_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "CMakeLists.txt",
]
def sha(file:Path)->str:
    return hashlib.sha256(file.read_bytes()).hexdigest()

def main():
    compile_log=FILES["compile"].read_text(encoding="utf-8",errors="replace")
    native=FILES["native"].read_text(encoding="utf-8",errors="replace")
    ctest=FILES["ctest"].read_text(encoding="utf-8",errors="replace")
    checks=[]
    def require(name,ok,value=None):
        checks.append({"name":name,"pass":bool(ok),"value":value})
    require("msvc_build",
            "-- Configuring done" in compile_log and
            ("Linking CXX executable" in compile_log or
             "ninja: no work to do." in compile_log or
             "p2_plic_substep_trace3d_tests.cpp" in compile_log)
            and "fatal error" not in compile_log.lower())
    a=re.search(r"PASS P2_PLIC_SUBSTEP_TRACE assertions=(\d+) phase_complete=false",native)
    assertions=int(a.group(1)) if a else 0
    require("native_assertions",assertions>=46,assertions)
    require("all_3d_grids",all(f"TRACE_ABC n={n} " in native for n in (8,12,16)))
    require("original_second_substep_failure",
            "TRACE_ABC n=16 completed=0 planned=2 passed=1 failed_substep=2 "
            "reason=Unresolved periodic PLIC interface" in native)
    require("failure_precursor_is_endpoint_not_geometric",
            "TRACE_STAGE n=16 completed=1 partial=515 geometric=0 endpoint=3" in native)
    snap=re.search(r"TRACE_ROUNDOFF n=16 completed=1 passed=2 snapped=(\d+) "
                   r"absolute_snap_m3=([0-9.eE+-]+) reason=none",native)
    snapped=int(snap.group(1)) if snap else 0
    volume=float(snap.group(2)) if snap else float("inf")
    require("real_solver_optin_correction_tiny",
            snapped==7 and 0<volume<1e-12,
            {"snapped":snapped,"absolute_volume_m3":volume})
    require("synthetic_ambiguity_fail_closed",
            "TRACE_SYNTHETIC geometric=1 endpoint=1 pure_water=PASS" in native)
    m=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctest)
    count=int(m.group(1)) if m else 0
    require("complete_registered_ctest",count>=112 and
            "physics_p2_plic_endpoint_substep_partial" in ctest,count)
    require("actual_native_binary",FILES["binary"].is_file())
    require("sources_exist",all((CORE/f).is_file() for f in SOURCES))
    success=all(c["pass"] for c in checks)
    result={
        "phase":"P2","milestone":"periodic_plic_16cubed_substep_endpoint_fix_partial",
        "partial_accepted":success,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":assertions,"existing_ctest_suites":count,
        "checks":checks,
        "source_sha256":{f:sha(CORE/f) for f in SOURCES},
        "artifact_sha256":{key:sha(file) for key,file in FILES.items()},
        "limits":"Opt-in only: one 16^3 viscous ABC frame; original error reproduced. "
                 "No full free-surface, global FSI, external CFD or UE5.8 E2E acceptance.",
    }
    output=RUNS/"p2_plic_substep_fix_partial.json"
    output.write_text(json.dumps(result,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print(json.dumps({"partial_accepted":success,"checks":checks,
                      "evidence":str(output)},ensure_ascii=False))
    if not success:
        raise SystemExit(1)

if __name__=="__main__":
    main()
