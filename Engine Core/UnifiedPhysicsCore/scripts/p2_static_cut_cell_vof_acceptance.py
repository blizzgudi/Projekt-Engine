#!/usr/bin/env python3
"""Bounded P2 static 3D donor-cell VOF evidence; NEVER a full P2 acceptance."""
import hashlib
import json
import math
import re
import subprocess
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
LOG=RUNS/"p2_static_cut_cell_vof_full_ctest_20261029.txt"
EXE=CORE/"build"/"upc_p2_static_cut_cell_vof_tests.exe"
REPORT=RUNS/"p2_static_cut_cell_vof_partial.json"
SOURCES=[
    "CMakeLists.txt","docs/PHASENPLAN.md",
    "docs/P2_STATIC_CUT_CELL_VOF_PARTIAL.md",
    "include/upc/p2_static_cut_cell_vof3d.hpp",
    "include/upc/p2_moving_box_mac_apertures3d.hpp",
    "include/upc/p2_moving_box_open_volume3d.hpp",
    "tests/p2_static_cut_cell_vof3d_tests.cpp",
    "scripts/p2_static_cut_cell_vof_acceptance.py",
    "scripts/p2_static_cut_cell_vof_full_build.cmd",
]
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    checks=[]
    def check(name,passed,detail=None):
        checks.append({"name":name,"pass":bool(passed),"detail":detail})
        if not passed:
            raise RuntimeError(f"{name}: {detail}")
    log=LOG.read_text(encoding="utf-8",errors="replace")
    check("87/87 full native CTest suites",
          "100% tests passed, 0 tests failed out of 87" in log)
    check("new CTest suite included",
          "physics_p2_static_cut_cell_vof_partial" in log)
    result=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                          text=True,check=True,timeout=120)
    output=result.stdout
    match=re.search(r"PASS: (\d+) native static 3D cut-cell VOF assertions",output)
    count=int(match.group(1)) if match else 0
    check("native assertions >= 90000",count>=90000,count)
    rows=re.findall(r"^VOF_CASE (\d+) (\d+) ([\d.eE+-]+) "
                    r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+)$",
                    output,re.MULTILINE)
    check("two grid sizes",len(rows)==2,sorted({x[0] for x in rows}))
    for row in rows:
        n,steps,mass,full,diff,cfl=(int(row[0]),int(row[1]),*map(float,row[2:]))
        check(f"{n} cubed, 40 dynamic steps",n in (8,12) and steps==40)
        check(f"{n} finite volume mass",math.isfinite(mass) and mass>0,mass)
        check(f"{n} full-water invariant",
              abs(full-(n**3-1))<1e-10,full)
        check(f"{n} nontrivial 3D advection",diff>1e-4,diff)
        check(f"{n} donor CFL",0<cfl<1,cfl)
    check("analytic donor-cell reference",
          "VOF_ANALYTIC 0.48 0.02 0.5" in output)
    partial=re.search(r"^VOF_PARTIAL (\d+) (\d+) "
                      r"([\d.eE+-]+) ([\d.eE+-]+)$",output,re.MULTILINE)
    check("fractional cut-cell scenario executed",partial is not None)
    if partial:
        cells,faces=int(partial.group(1)),int(partial.group(2))
        before,after=map(float,partial.groups()[2:])
        check("real partially occluded 3D cells and faces",
              cells>=8 and faces>0,(cells,faces))
        check("partial-cell water mass conserved",
              abs(after-before)<1e-10,(before,after))
    check("eight physical/numeric rejection tests",
          "VOF_NEGATIVE 8" in output)
    hashes={name:sha(CORE/name) for name in SOURCES}
    report={
        "phase":"P2","milestone":"static_cut_cell_3d_donor_vof_partial",
        "partial_accepted":True,"phase_complete":False,
        "ctest_suites":87,"native_assertions":count,
        "checks":checks,"source_sha256":hashes,
        "ctest_log_sha256":sha(LOG),
        "limitations":[
            "prescribed solenoidal MAC velocity and stationary cut-cell geometry",
            "first-order diffusive donor-cell advection, no interface reconstruction",
            "no pressure/impulse time integration or moving-solid counterreaction",
            "no global physical FSI energy/momentum conservation",
            "no external measured 3D CFD or new Unreal 5.8 Native-E2E",
        ],
    }
    RUNS.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"P2_STATIC_CUT_CELL_VOF_PARTIAL_PASS checks={len(checks)} "
          f"native_assertions={count} grids={len(rows)}")
    print(f"REPORT={REPORT}")
if __name__=="__main__":
    main()
