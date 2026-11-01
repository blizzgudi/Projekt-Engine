"""P2 partial: native 16^3 periodic PLIC preflight and CTest evidence."""
from __future__ import annotations
import hashlib
import json
import re
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
FILES={
    "native": RUNS/"p2_plic_preflight_native.txt",
    "compile": RUNS/"p2_plic_preflight_compile.txt",
    "ctest": RUNS/"p2_plic_preflight_ctest.txt",
    "binary": CORE/"build"/"upc_p2_plic_preflight3d_tests.exe",
}
SOURCES=[
    "include/upc/p2_plic_preflight3d.hpp",
    "include/upc/p2_plic_strang_periodic3d.hpp",
    "tests/p2_plic_preflight3d_tests.cpp",
    "scripts/p2_plic_preflight_build.cmd",
    "scripts/p2_plic_preflight_acceptance.py",
    "docs/P2_PLIC_PREFLIGHT_PARTIAL.md",
    "CMakeLists.txt",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main()->None:
    texts={key:path.read_text(encoding="utf-8",errors="replace")
           for key,path in FILES.items() if key!="binary"}
    checks=[]
    def check(name:str,ok:bool,value=None):
        checks.append({"name":name,"pass":bool(ok),"value":value})
    compile_log=texts["compile"]
    check("real_msvc_release_build",
          "-- Configuring done" in compile_log
          and "-- Generating done" in compile_log
          and ("Linking CXX executable" in compile_log
               or "ninja: no work to do." in compile_log)
          and "fatal error" not in compile_log.lower())
    match=re.search(r"PASS P2_PLIC_PREFLIGHT assertions=(\d+) phase_complete=false",
                    texts["native"])
    assertions=int(match.group(1)) if match else 0
    check("native_assertions",assertions>=260,assertions)
    check("16cubed_mixed_and_endpoint_classification",
          "resolved_planar=256 unresolved_geometric=1 unresolved_endpoint=1" in texts["native"])
    check("periodic_seam",
          "periodic_seam_resolved=256" in texts["native"])
    ctest=texts["ctest"]
    count_match=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctest)
    check("full_ctest_at_least_111",
          count_match is not None and int(count_match.group(1))>=111
          and "physics_p2_plic_preflight_partial" in ctest,
          ctest.splitlines()[-5:])
    check("native_binary_exists",FILES["binary"].is_file())
    check("all_sources_exist",all((CORE/f).is_file() for f in SOURCES))
    accepted=all(c["pass"] for c in checks)
    result={
        "phase":"P2","milestone":"periodic_plic_16cubed_preflight_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":assertions,"checks":checks,
        "source_sha256":{f:sha(CORE/f) for f in SOURCES},
        "artifact_sha256":{k:sha(v) for k,v in FILES.items()},
        "limits":"Read-only classification only; 16^3 moving mixed-interface "
                 "Galilean ABC/PLIC failure remains unresolved. No full P2 acceptance.",
    }
    out=RUNS/"p2_plic_preflight_partial.json"
    out.write_text(json.dumps(result,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(json.dumps({"partial_accepted":accepted,"checks":checks,
                      "evidence":str(out)},ensure_ascii=False))
    if not accepted:raise SystemExit(1)
if __name__=="__main__":main()
