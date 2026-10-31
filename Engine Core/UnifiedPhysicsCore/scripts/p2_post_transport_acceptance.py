"""P2 bounded acceptance: post-VOF wet-mask MAC projection; never full P2."""
from __future__ import annotations
import csv
import hashlib
import io
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
EXE=CORE/"build"/"upc_p2_post_transport_projection_tests.exe"
LOG=RUNS/"p2_post_transport_build_ctest_20261031.txt"
OUT=RUNS/"p2_post_transport_partial.json"
SOURCES=[
    "include/upc/coupled_water3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_post_transport_projection3d_tests.cpp",
    "CMakeLists.txt",
    "docs/P2_POST_TRANSPORT_PROJECTION_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/p2_post_transport_acceptance.py",
    "scripts/phase_gate.py",
]
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main()->None:
    result=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                          text=True,timeout=90,check=False)
    raw_log=LOG.read_bytes()
    log=raw_log.decode("utf-16" if raw_log.startswith((b"\xff\xfe",b"\xfe\xff")) else "utf-8",errors="replace")
    checks=[]
    def add(name:str,value:object,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",result.returncode,result.returncode==0)
    match=re.search(r"PASS: post-transport verified projection assertions=(\d+)",result.stdout)
    assertions=int(match.group(1)) if match else 0
    add("native_rollback_mass_occupancy_and_listener_assertions",assertions,
        assertions>=18000)
    add("msvc_91_ctest",
        "100% tests passed, 0 tests failed out of 91" in log,
        "100% tests passed, 0 tests failed out of 91" in log)
    lines=[line for line in result.stdout.splitlines()
           if line.startswith("mode,") or line.startswith("case,")]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("two_3d_grids",len(rows),len(rows)==2)
    refs=[]
    for row in rows:
        n=int(row["n"])
        mass=float(row["mass_kg"])
        post=float(row["post_divergence"])
        legacy=float(row["legacy_post_divergence"])
        wet=int(row["wet_cells"])
        callbacks=int(row["callbacks"])
        add(f"{n}_grid",n,n in (8,12))
        add(f"{n}_independent_seven_cell_mass",mass,
            math.isfinite(mass) and abs(mass-7.0)<1e-10)
        add(f"{n}_post_vof_divergence",post,
            math.isfinite(post) and 0<=post<=1e-8)
        add(f"{n}_original_old_mask_gap",legacy,
            math.isfinite(legacy) and legacy>1.0)
        add(f"{n}_divergence_reduction_factor",legacy/max(post,1e-300),
            post>=0 and legacy>post*1e6)
        add(f"{n}_wet_cells",wet,wet>7)
        add(f"{n}_committed_transport_events",callbacks,callbacks>0)
        refs.append({"grid":n,"mass_kg":mass,"final_wet_divergence_s_inv":post,
                     "old_mask_final_divergence_s_inv":legacy,
                     "wet_cells":wet,"notifications":callbacks})
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("eight_source_hashes",len(hashes),len(hashes)==8)
    accepted=bool(checks) and all(c["pass"] for c in checks)
    data={"phase":"P2","milestone":"post_vof_wet_mask_projection_partial",
          "partial_accepted":accepted,"phase_complete":False,
          "generated_utc":datetime.now(timezone.utc).isoformat(),
          "checks":checks,"native_assertions":assertions,
          "reference":refs,"source_sha256":hashes,
          "native_binary_sha256":sha(EXE),
          "ctest_log_sha256":sha(LOG),
          "limitations":"Discrete post-VOF MAC divergence only; no physical FSI impulse/energy conservation, independent CFD measurement or Unreal 5.8 Native-E2E."}
    OUT.write_text(json.dumps(data,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(f"P2_POST_TRANSPORT_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]:print("FAIL",c)
        raise SystemExit(1)
if __name__=="__main__":
    main()
