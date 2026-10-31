"""P2 partial quantitative acceptance: neighbor-derived VOF normals + PLIC moments."""
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
EXE=CORE/"build"/"upc_p2_vof_normal_reconstruction_tests.exe"
BUILD=RUNS/"p2_vof_normal_build_20261031.txt"
CTEST=RUNS/"p2_vof_normal_ctest_20261031.txt"
OUT=RUNS/"p2_vof_normal_reconstruction_partial.json"
SOURCES=[
    "include/upc/p2_vof_normal_reconstruction3d.hpp",
    "include/upc/p2_oblique_plic_geometry3d.hpp",
    "tests/p2_vof_normal_reconstruction3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_vof_normal_reconstruction_full_build.cmd",
    "scripts/p2_vof_normal_reconstruction_acceptance.py",
    "docs/P2_VOF_NORMAL_RECONSTRUCTION_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main()->None:
    native=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                          text=True,timeout=120)
    checks=[]
    def add(name:str,value:object,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",native.returncode,native.returncode==0)
    match=re.search(r"PASS: reconstructed VOF normals assertions=(\d+) phase_complete=false",
                    native.stdout)
    count=int(match.group(1)) if match else 0
    add("native_assertions",count,count>=13065)
    log=CTEST.read_text(encoding="utf-8",errors="replace")
    add("all_ctest_100","100% tests passed, 0 tests failed out of 100" in log,
        "100% tests passed, 0 tests failed out of 100" in log)
    add("new_ctest_entry","physics_p2_vof_normal_reconstruction_partial" in log,
        "physics_p2_vof_normal_reconstruction_partial" in log)
    add("build_log_nonempty",BUILD.stat().st_size,BUILD.stat().st_size>0)
    lines=[line for line in native.stdout.splitlines()
           if line.startswith(("case,","affine,","plane,","masked,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("four_native_cases",len(rows),len(rows)==4)
    if len(rows)!=4:
        raise RuntimeError("Missing native scenario rows")
    actual=[(r["case"],int(r["grid"])) for r in rows]
    add("exact_scenario_set",actual,
        actual==[("affine",8),("affine",12),("plane",12),("masked",3)])
    refs={
        ("affine",8):(4.0,(.018,-.014,.011),512,384),
        ("affine",12):(13.5,(.018,-.014,.011),1728,864),
        ("plane",12):(13.5,(.8,.5,-.3),268,96),
        ("masked",3):(104.16,(.02,.03,-.01),26,50),
    }
    parsed=[]
    for row in rows:
        key=(row["case"],int(row["grid"]))
        mass=float(row["mass"])
        vec=[float(row[f"normal_{a}"]) for a in ("x","y","z")]
        partial=int(row["partial"])
        one_sided=int(row["one_sided"])
        expected_mass,v,p,o=refs[key]
        norm=math.sqrt(sum(x*x for x in v))
        expected=[x/norm for x in v]
        name=f"{key[0]}_{key[1]}"
        add(name+"_mass",mass,abs(mass-expected_mass)<2e-10)
        add(name+"_partial",partial,partial==p)
        add(name+"_one_sided",one_sided,one_sided==o)
        add(name+"_normal_finite",vec,all(math.isfinite(x) for x in vec))
        for a in range(3):
            add(name+"_normal_"+str(a),vec[a],
                abs(vec[a]-expected[a])<1e-12)
        parsed.append({"case":key[0],"grid":key[1],"mass_kg":mass,
                       "normal":vec,"partial_cells":partial,
                       "one_sided_axes":one_sided})
    alignment=re.search(r"plane_alignment,([0-9.eE+-]+),([0-9.eE+-]+)",
                        native.stdout)
    min_dot=float(alignment.group(1)) if alignment else float("nan")
    mean_dot=float(alignment.group(2)) if alignment else float("nan")
    add("sharp_interface_min_alignment",min_dot,
        math.isfinite(min_dot) and min_dot>0.80)
    add("sharp_interface_mean_alignment",mean_dot,
        math.isfinite(mean_dot) and mean_dot>0.96)
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("source_fingerprints",len(hashes),len(hashes)==8)
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2","milestone":"local_vof_normal_reconstruction_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":count,
        "reference":parsed,
        "sharp_plane_alignment":{"min":min_dot,"mean":mean_dot},
        "source_sha256":hashes,"native_binary_sha256":sha(EXE),
        "build_log_sha256":sha(BUILD),"ctest_log_sha256":sha(CTEST),
        "limitations":"Finite-difference neighbor gradient only, not high-order PLIC, exact interface normals, conservative geometric advection, physical FSI, measured 3D CFD validation or Unreal 5.8 Native E2E."
    }
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                   encoding="utf-8")
    print(f"P2_VOF_NORMAL_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} NATIVE_ASSERTIONS={count} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]:print("FAIL",c)
        raise SystemExit(1)
if __name__=="__main__":
    main()
