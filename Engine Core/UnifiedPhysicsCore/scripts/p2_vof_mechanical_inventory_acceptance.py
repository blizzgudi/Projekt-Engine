"""P2 bounded: independent VOF-cell-centred mechanical inventory reference."""
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
EXE=CORE/"build"/"upc_p2_vof_mechanical_inventory_tests.exe"
LOG=RUNS/"p2_vof_mechanical_inventory_ctest_20261031.txt"
OUT=RUNS/"p2_vof_mechanical_inventory_partial.json"
SOURCES=[
    "include/upc/mac3d.hpp",
    "include/upc/coupled_water3d.hpp",
    "include/upc/p2_vof_mechanical_inventory3d.hpp",
    "tests/p2_vof_mechanical_inventory3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_vof_mechanical_inventory_full_build.cmd",
    "scripts/p2_vof_mechanical_inventory_acceptance.py",
    "docs/P2_VOF_MECHANICAL_INVENTORY_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main()->None:
    native=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,text=True,
                          timeout=90,check=False)
    raw=LOG.read_bytes()
    log=raw.decode("utf-16" if raw.startswith((b"\xff\xfe",b"\xfe\xff"))
                   else "utf-8",errors="replace")
    checks=[]
    def add(name:str,value:object,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",native.returncode,native.returncode==0)
    m=re.search(r"PASS: vof mechanical inventory assertions=(\d+)",native.stdout)
    assertions=int(m.group(1)) if m else 0
    add("native_assertions",assertions,assertions>=62)
    add("msvc_94_ctest","100% tests passed, 0 tests failed out of 94" in log,
        "100% tests passed, 0 tests failed out of 94" in log)
    lines=[line for line in native.stdout.splitlines()
           if line.startswith(("mode,","synthetic,","live,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("two_synthetic_and_two_live_grids",len(rows),len(rows)==4)
    references=[]
    expected={"mass":1.0,"px":-0.25,"py":-0.25,"pz":-1.375,
              "kinetic":2.53125,"potential":4.65975,"total":7.191}
    for row in rows:
        n=int(row["n"])
        mode=row["mode"]
        add(f"{mode}_{n}_grid",n,n in (8,12))
        nums={k:float(row[k]) for k in
              ("mass","px","py","pz","kinetic","potential","total")}
        add(f"{mode}_{n}_finite",nums,all(math.isfinite(x) for x in nums.values()))
        add(f"{mode}_{n}_energy_sum",nums["total"],
            abs(nums["total"]-(nums["kinetic"]+nums["potential"]))<1e-11)
        add(f"{mode}_{n}_occupied",row["occupied"],
            int(row["occupied"])==(2 if mode=="synthetic" else 3))
        if mode=="synthetic":
            for key,expected_value in expected.items():
                add(f"{mode}_{n}_analytic_{key}",nums[key],
                    abs(nums[key]-expected_value)<1e-12)
        elif mode=="live":
            add(f"{mode}_{n}_mass",nums["mass"],abs(nums["mass"]-2)<1e-10)
            add(f"{mode}_{n}_positive_energy",nums["kinetic"],
                nums["kinetic"]>0 and nums["potential"]>0)
        else:
            add("unknown_mode",mode,False)
        references.append({"grid":n,"mode":mode,**nums,
                           "occupied_cells":int(row["occupied"])})
    add("exact_grid_set",[(r["mode"],r["grid"]) for r in references],
        sorted((r["mode"],r["grid"]) for r in references)==
        [("live",8),("live",12),("synthetic",8),("synthetic",12)])
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("ten_source_hashes",len(hashes),len(hashes)==10)
    accepted=bool(checks) and all(c["pass"] for c in checks)
    report={
        "phase":"P2",
        "milestone":"vof_mass_weighted_cell_centre_mechanical_inventory_partial",
        "partial_accepted":accepted,
        "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":assertions,
        "reference":references,
        "source_sha256":hashes,
        "native_binary_sha256":sha(EXE),
        "ctest_log_sha256":sha(LOG),
        "build_log_sha256":sha(RUNS/"p2_vof_mechanical_inventory_build_20261031.txt"),
        "limitations":"Cell-centred velocity sampling is a VOF-mass-weighted diagnostic, not exact MAC/VOF kinetic energy or conservation. No dynamic FSI, pressure work, viscous heating, external CFD data or UE5.8 E2E."
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"P2_VOF_MECHANICAL_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]:print("FAIL",c)
        raise SystemExit(1)
if __name__=="__main__":
    main()
