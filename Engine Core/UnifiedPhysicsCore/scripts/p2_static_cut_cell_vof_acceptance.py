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
    "include/upc/p2_cut_cell_tracer_diffusion3d.hpp",
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
    subcycles=re.findall(r"^VOF_SUBCYCLE (\d+) (\d+) ([\d.eE+-]+) "
                         r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+)$",
                         output,re.MULTILINE)
    check("subcycling on both 3D grids",len(subcycles)==2,
          [r[0] for r in subcycles])
    for r in subcycles:
        n,k=int(r[0]),int(r[1])
        before,after,error,cfl=map(float,r[2:])
        check(f"{n} cubed four temporal substeps",n in (8,12) and k==4)
        check(f"{n} subcycle volume conservation",abs(before-after)<1e-10)
        check(f"{n} subcycle matches manual four-step reference",error<1e-14)
        check(f"{n} subcycle donor CFL",0<cfl<1)
    auto=re.findall(r"^VOF_AUTO (\d+) (\d+) ([\d.eE+-]+) "
                    r"([\d.eE+-]+) ([\d.eE+-]+)$",
                    output,re.MULTILINE)
    check("automatic CFL mode on both 3D grids",len(auto)==2,
          [r[0] for r in auto])
    for r in auto:
        n,k=int(r[0]),int(r[1])
        before,after,cfl=map(float,r[2:])
        check(f"{n} cubed minimum automatic substeps",
              n in (8,12) and k==2,(n,k))
        check(f"{n} auto volume conserved",
              math.isfinite(before) and before>0 and
              abs(before-after)<1e-10,(before,after))
        check(f"{n} auto donor CFL target",
              0<cfl<=0.8+1e-12,cfl)
    sequences=re.findall(r"^VOF_SEQUENCE (\d+) (\d+) ([\d.eE+-]+) "
                         r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+) "
                         r"([\d.eE+-]+)$",output,re.MULTILINE)
    check("time-varying MAC sequence on two 3D grids",
          len(sequences)==2,[r[0] for r in sequences])
    for row in sequences:
        n,k=int(row[0]),int(row[1])
        before,after,error,diff,cfl=map(float,row[2:])
        check(f"{n} cubed four distinct MAC time slices",n in (8,12) and k==4)
        check(f"{n} time-varying water conserved",
              math.isfinite(before) and abs(before-after)<1e-10)
        check(f"{n} time-varying equals independent step calls",
              error<1e-14 and diff>1e-5)
        check(f"{n} time-varying donor CFL",0<cfl<=1)
    adaptive=re.findall(r"^VOF_ADAPTIVE (\d+) (\d+) ([\d.eE+-]+) "
                        r"([\d.eE+-]+) ([\d.eE+-]+)$",
                        output,re.MULTILINE)
    check("unequal adaptive MAC slices on both 3D grids",
          len(adaptive)==2,[r[0] for r in adaptive])
    for row in adaptive:
        n,k=int(row[0]),int(row[1])
        before,after,cfl=map(float,row[2:])
        check(f"{n} cubed nonuniform adaptive slice substeps",
              n in (8,12) and 4<k<=1024,(n,k))
        check(f"{n} adaptive sequence water conserved",
              math.isfinite(before) and before>0 and
              abs(before-after)<1e-10,(before,after))
        check(f"{n} adaptive sequence geometric CFL",
              0<cfl<=0.8+1e-12,cfl)
    ledger=re.findall(r"^VOF_FACE_LEDGER (\d+) (\d+) ([\d.eE+-]+) "
                      r"([\d.eE+-]+) (\d+) (\d+)$",
                      output,re.MULTILINE)
    check("signed finite-volume face ledgers on both 3D grids",
          len(ledger)==2,ledger)
    for row in ledger:
        n,cases=int(row[0]),int(row[1])
        error,boundary=float(row[2]),float(row[3])
        positive,negative=int(row[4]),int(row[5])
        check(f"{n} cubed direct/subcycle/sequence/adaptive and partial cases",
              n in (8,12) and cases==(5 if n==8 else 4),cases)
        check(f"{n} cubed independent cellwise face-flux water balance",
              math.isfinite(error) and error<1e-12,error)
        check(f"{n} cubed sealed outer face water flux",
              boundary==0.0,boundary)
        check(f"{n} cubed bidirectional signed transported volumes",
              positive>0 and negative>0,(positive,negative))
    tracer=re.findall(r"^VOF_TRACER (\d+) (\d+) ([\d.eE+-]+) "
                      r"([\d.eE+-]+) ([\d.eE+-]+)$",
                      output,re.MULTILINE)
    check("passive dissolved tracer on two 3D grids",
          len(tracer)==2,[r[0] for r in tracer])
    for row in tracer:
        n,steps=int(row[0]),int(row[1])
        before,after,error=map(float,row[2:])
        check(f"{n} cubed 40 tracer/VOF coupled steps",
              n in (8,12) and steps==40,(n,steps))
        check(f"{n} cubed conserved dissolved tracer mass",
              math.isfinite(before) and before>0 and
              abs(before-after)<1e-10,(before,after))
        check(f"{n} cubed independent face solute balance",
              math.isfinite(error) and error<1e-12,error)
    check("analytic water-to-tracer donor mass 0.06 kg",
          "VOF_TRACER_ANALYTIC 1.5 0.06 3" in output)
    check("four invalid tracer inputs rejected",
          "VOF_TRACER_NEGATIVE 4" in output)
    adaptive_tracer=re.findall(
        r"^VOF_TRACER_ADAPTIVE (\d+) (\d+) ([\d.eE+-]+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+) "
        r"([\d.eE+-]+)$",output,re.MULTILINE)
    check("adaptive dissolved tracer on two 3D grids",
          len(adaptive_tracer)==2,[r[0] for r in adaptive_tracer])
    for row in adaptive_tracer:
        n,steps=int(row[0]),int(row[1])
        tracer_before,tracer_after,water_before,water_after,error=map(float,row[2:])
        check(f"{n} cubed six adaptive tracer substeps",
              n in (8,12) and steps==6,(n,steps))
        check(f"{n} cubed adaptive tracer mass conserved",
              math.isfinite(tracer_before) and tracer_before>0 and
              abs(tracer_after-tracer_before)<1e-10)
        check(f"{n} cubed adaptive water volume conserved",
              math.isfinite(water_before) and water_before>0 and
              abs(water_after-water_before)<1e-10)
        check(f"{n} cubed manual per-substep tracer/water/face-ledger parity",
              math.isfinite(error) and error<1e-12,error)
    solute_ledger=re.findall(
        r"^VOF_TRACER_FACE_LEDGER (\d+) (\d+) ([\d.eE+-]+) "
        r"([\d.eE+-]+)$",output,re.MULTILINE)
    check("signed cumulative solute ledger on both 3D grids",
          len(solute_ledger)==2,solute_ledger)
    for row in solute_ledger:
        n,steps=int(row[0]),int(row[1])
        residual,boundary=float(row[2]),float(row[3])
        check(f"{n} cubed tracer ledger spans six adaptive substeps",
              n in (8,12) and steps==6,(n,steps))
        check(f"{n} cubed independently closed local tracer inventory",
              math.isfinite(residual) and residual<1e-12,residual)
        check(f"{n} cubed no tracer escapes sealed boundaries",
              boundary==0.0,boundary)
    check("six invalid adaptive tracer inputs rejected",
          "VOF_TRACER_ADAPTIVE_NEGATIVE 6" in output)
    diffusion=re.findall(
        r"^VOF_TRACER_DIFFUSION (\d+) (\d+) ([\d.eE+-]+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+)$",
        output,re.MULTILINE)
    check("diffusive tracer runs on both 3D grids",len(diffusion)==2,diffusion)
    for row in diffusion:
        n,steps=int(row[0]),int(row[1])
        before,after,residual,cfl=map(float,row[2:])
        check(f"{n} cubed 40 coupled advection/diffusion steps",
              n in (8,12) and steps==40,(n,steps))
        check(f"{n} cubed conservative diffusive tracer",
              math.isfinite(before) and before>0 and abs(after-before)<1e-10)
        check(f"{n} cubed independent advection+diffusion face balance",
              math.isfinite(residual) and residual<1e-12,residual)
        check(f"{n} cubed stable explicit Fick CFL",0<cfl<1,cfl)
    analytic=re.search(
        r"^VOF_TRACER_DIFFUSION_ANALYTIC ([\d.eE+-]+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+)$",
        output,re.MULTILINE)
    check("six-face analytic 3D Fick reference",analytic is not None)
    if analytic:
        center,neighbor,mass,cfl=map(float,analytic.groups())
        check("analytic 3D center loss",abs(center-0.97)<1e-13)
        check("analytic 3D neighbor gain",abs(neighbor-0.005)<1e-13)
        check("analytic six-face mass balance",abs(mass-1)<1e-13)
        check("analytic diffusion CFL",abs(cfl-0.03)<1e-13)
    partial_diff=re.search(
        r"^VOF_TRACER_DIFFUSION_PARTIAL (\d+) ([\d.eE+-]+) "
        r"([\d.eE+-]+)$",output,re.MULTILINE)
    check("fractional 3D cut-cell diffusion",partial_diff is not None)
    if partial_diff:
        faces,before,after=partial_diff.groups()
        check("fractional aperture diffusion conserves solute",
              int(faces)>0 and abs(float(before)-float(after))<1e-10)
    check("five invalid diffusion inputs rejected",
          "VOF_TRACER_DIFFUSION_NEGATIVE 5" in output)
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
            "bounded geometric CFL subcycling of water and passive tracer on prescribed fields, not adaptive CFD",
            "explicit Fick-type tracer diffusion uses approximate wetted cut-cell apertures, not measured interfacial physics",
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
