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
    check("native assertions >= 288000",count>=288000,count)
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
    anisotropic=re.findall(
        r"^VOF_TRACER_ANISOTROPIC (\d+) ([\d.eE+-]+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+)$",
        output,re.MULTILINE)
    check("diagonal anisotropic Fick reference on two 3D grids",
          len(anisotropic)==2,[r[0] for r in anisotropic])
    for row in anisotropic:
        n=int(row[0])
        center,x,y,z,mass,cfl,residual=map(float,row[1:])
        check(f"{n} cubed anisotropic six-neighbor analytic pulse",
              n in (8,12) and abs(center-0.988)<1e-13 and
              abs(x-0.001)<1e-13 and abs(y-0.002)<1e-13 and
              abs(z-0.003)<1e-13,(center,x,y,z))
        check(f"{n} cubed anisotropic solute conservation",
              abs(mass-1.0)<1e-12,mass)
        check(f"{n} cubed anisotropic local CFL",
              abs(cfl-0.012)<1e-13,cfl)
        check(f"{n} cubed independent cellwise anisotropic face balance",
              math.isfinite(residual) and residual<1e-12,residual)
    check("anisotropic zero axes, scalar parity, and four negative cases",
          "VOF_TRACER_ANISOTROPIC 8 " in output and
          "VOF_TRACER_ANISOTROPIC 12 " in output)
    subcycled_diffusion=re.findall(
        r"^VOF_TRACER_DIFFUSION_SUBCYCLE (\d+) (\d+) ([\d.eE+-]+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+)$",
        output,re.MULTILINE)
    check("bounded anisotropic diffusion subcycles on both 3D grids",
          len(subcycled_diffusion)==2,[r[0] for r in subcycled_diffusion])
    for row in subcycled_diffusion:
        n,k=int(row[0]),int(row[1])
        center,mass,cfl,residual=map(float,row[2:])
        check(f"{n} cubed automatic two-step diffusion",n in (8,12) and k==2)
        check(f"{n} cubed independent two-step 3D pulse center",
              abs(center-0.15625)<1e-13,center)
        check(f"{n} cubed subcycled tracer mass conservation",
              abs(mass-1.0)<1e-12,mass)
        check(f"{n} cubed actual substep diffusion CFL",
              0<cfl<=0.8+1e-12 and abs(cfl-0.75)<1e-12,cfl)
        check(f"{n} cubed independent cumulative diffusion face balance",
              math.isfinite(residual) and residual<1e-12,residual)
    subcycled_partial=re.findall(
        r"^VOF_TRACER_DIFFUSION_SUBCYCLE_PARTIAL (\d+) (\d+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+)$",
        output,re.MULTILINE)
    check("fractional cut-cell diffusion subcycles on both 3D grids",
          len(subcycled_partial)==2,[r[0] for r in subcycled_partial])
    for row in subcycled_partial:
        n,k=int(row[0]),int(row[1])
        before,after,cfl,residual=map(float,row[2:])
        check(f"{n} cubed fractional diffusion subcycle count",
              n in (8,12) and k==3,k)
        check(f"{n} cubed fractional initial solute volume",
              abs(before-0.784)<1e-12,before)
        check(f"{n} cubed fractional subcycled mass conservation",
              abs(after-before)<1e-12,(before,after))
        check(f"{n} cubed fractional per-substep CFL",
              0<cfl<=0.8+1e-12,cfl)
        check(f"{n} cubed fractional independent cellwise ledger",
              math.isfinite(residual) and residual<1e-12,residual)

    heterogeneous=re.findall(
        r"^VOF_TRACER_HETEROGENEOUS (\d+) ([\d.eE+-]+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+) "
        r"([\d.eE+-]+) ([\d.eE+-]+)$",output,re.MULTILINE)
    check("harmonic material-interface 3D diffusion on two grids",
          len(heterogeneous)==2,[r[0] for r in heterogeneous])
    for row in heterogeneous:
        n=int(row[0])
        center,left,right,mass,cfl,residual=map(float,row[1:])
        check(f"{n} cubed independent harmonic face reference",
              n in (8,12) and abs(center-0.6)<1e-12 and
              abs(left-2/15)<1e-12 and abs(right-4/15)<1e-12,
              (center,left,right))
        check(f"{n} cubed heterogeneous total mass",
              abs(mass-1.0)<1e-12,mass)
        check(f"{n} cubed heterogeneous explicit CFL",
              abs(cfl-0.4)<1e-12,cfl)
        check(f"{n} cubed independent signed U/V/W cell balance",
              math.isfinite(residual) and residual<1e-12,residual)

    hetero_subcycle=re.findall(
        r"^VOF_TRACER_HET_SUBCYCLE (\d+) (\d+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+)$",
        output,re.MULTILINE)
    check("heterogeneous harmonic CFL subcycling on two 3D grids",
          len(hetero_subcycle)==2,[r[0] for r in hetero_subcycle])
    for row in hetero_subcycle:
        n,k=int(row[0]),int(row[1])
        mass,cfl,residual=map(float,row[2:])
        check(f"{n} cubed harmonic interface exact 3 substeps",
              n in (8,12) and k==3,k)
        check(f"{n} cubed subcycled heterogeneous solute mass",
              abs(mass-1.0)<1e-12,mass)
        check(f"{n} cubed exact per-cell harmonic CFL",
              abs(cfl-2/3)<1e-12,cfl)
        check(f"{n} cubed independent cumulative U/V/W mass ledger",
              math.isfinite(residual) and residual<1e-12,residual)
    hetero_fractional=re.findall(
        r"^VOF_TRACER_HET_SUBCYCLE_PARTIAL (\d+) (\d+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+) (\d+)$",
        output,re.MULTILINE)
    check("heterogeneous fractional cut-cell subcycling on two 3D grids",
          len(hetero_fractional)==2,[r[0] for r in hetero_fractional])
    for row in hetero_fractional:
        n,k=int(row[0]),int(row[1])
        mass,cfl,residual=map(float,row[2:5])
        partial=int(row[5])
        expected={8:113.00864,12:504.52864}.get(n,0)
        check(f"{n} cubed fractional cut-cell CFL substeps",
              k==2 and partial>=8,(k,partial))
        check(f"{n} cubed fractional heterogeneous mass reference",
              abs(mass-expected)<1e-9,mass)
        check(f"{n} cubed fractional heterogeneous local CFL",
              0<cfl<=0.7+1e-12,cfl)
        check(f"{n} cubed fractional independent cellwise mass ledger",
              math.isfinite(residual) and residual<1e-12,residual)
    harmonic=re.findall(
        r"^VOF_TRACER_HARMONIC_EXTREME (\d+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+) "
        r"(\d+) ([\d.eE+-]+)$",output,re.MULTILINE)
    check("extreme harmonic material-interface runs on both 3D grids",
          len(harmonic)==2,[r[0] for r in harmonic])
    for row in harmonic:
        n=int(row[0])
        contrast,equal,large=map(float,row[1:4])
        substeps=int(row[4]);mass=float(row[5])
        check(f"{n} cubed subnormal/huge D harmonic flux",
              n in (8,12) and abs(contrast-2e-10)<1e-21,contrast)
        check(f"{n} cubed equal subnormal D remains conductive",
              abs(equal-1e-10)<1e-21,equal)
        check(f"{n} cubed equal large D avoids overflow",
              abs(large-0.01)<1e-13,large)
        check(f"{n} cubed extreme heterogeneous subcycle and mass",
              substeps==1 and abs(mass-1)<1e-13,(substeps,mass))
    quadratic=re.findall(
        r"^VOF_TRACER_HET_QUADRATIC (\d+) ([\d.eE+-]+) "
        r"([\d.eE+-]+) ([\d.eE+-]+)$",output,re.MULTILINE)
    check("3D heterogeneous numerical mixing audit on two grids",
          len(quadratic)==2,[row[0] for row in quadratic])
    for row in quadratic:
        n=int(row[0]);before,after,loss=map(float,row[1:])
        check(f"{n} cubed exact initial mixing functional",
              n in (8,12) and abs(before-1.0)<1e-13,before)
        check(f"{n} cubed independent three-step mixing reference",
              abs(after-20045/59049)<1e-13,after)
        check(f"{n} cubed nonnegative conservative quadratic dissipation",
              abs(loss-39004/59049)<1e-13 and abs(before-after-loss)<1e-13,
              loss)
    fractional_quadratic=re.findall(
        r"^VOF_TRACER_HET_QUADRATIC_PARTIAL (\d+) ([\d.eE+-]+) "
        r"([\d.eE+-]+) ([\d.eE+-]+)$",output,re.MULTILINE)
    check("3D fractional cut-cell mixing audit on two grids",
          len(fractional_quadratic)==2,[row[0] for row in fractional_quadratic])
    for row in fractional_quadratic:
        n=int(row[0]);before,after,loss=map(float,row[1:])
        check(f"{n} cubed partial wet-volume Q decreases",
              n in (8,12) and 0<after<before and loss>0,
              (before,after,loss))
        check(f"{n} cubed partial Q difference closes",
              abs(before-after-loss)<1e-10,(before,after,loss))
    # Independent exact three-cell forward-Euler dissipation reference.
    ref_left,ref_center,ref_right=0.0,1.0,0.0
    ref_drive=ref_correction=0.0
    for _ in range(3):
        jl=(2/9)*(ref_center-ref_left)
        jr=(4/9)*(ref_center-ref_right)
        ref_drive+=2*(jl*(ref_center-ref_left)+jr*(ref_center-ref_right))
        ref_correction+=jl*jl+jr*jr+(jl+jr)**2
        ref_left+=jl
        ref_right+=jr
        ref_center-=jl+jr
    identity=re.findall(
        r"^VOF_TRACER_HET_QUADRATIC_IDENTITY (\d+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+)$",
        output,re.MULTILINE)
    check("3D face-gradient Euler identity on both grids",
          len(identity)==2,[row[0] for row in identity])
    for row in identity:
        n=int(row[0]);drive,correction,residual=map(float,row[1:])
        check(f"{n} cubed independent face-gradient dissipation",
              n in (8,12) and abs(drive-ref_drive)<1e-12,drive)
        check(f"{n} cubed independent explicit Euler correction",
              abs(correction-ref_correction)<1e-12,correction)
        check(f"{n} cubed exact three-cell quadratic balance",
              abs(drive-correction-39004/59049)<1e-12 and
              math.isfinite(residual) and abs(residual)<1e-12,residual)
    fractional_identity=re.findall(
        r"^VOF_TRACER_HET_QUADRATIC_IDENTITY_PARTIAL (\d+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+)$",
        output,re.MULTILINE)
    check("fractional 3D Euler identity on both grids",
          len(fractional_identity)==2,[row[0] for row in fractional_identity])
    for row in fractional_identity:
        n=int(row[0]);drive,correction,residual=map(float,row[1:])
        check(f"{n} cubed fractional face dissipation exceeds correction",
              n in (8,12) and drive>correction>0.0,(drive,correction))
        check(f"{n} cubed fractional discrete quadratic balance",
              math.isfinite(residual) and abs(residual)<1e-10,residual)
    # Independent continuum Neumann cosine eigenmode on three 3D grids.
    # The reference is recomputed analytically here, not read from C++.
    neumann=re.findall(
        r"^VOF_TRACER_NEUMANN_3D (\d+) (\d+) ([\d.eE+-]+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+) "
        r"([\d.eE+-]+) ([\d.eE+-]+)$",output,re.MULTILINE)
    check("independent 3D Neumann eigenmode on three grids",
          len(neumann)==3,[r[0] for r in neumann])
    errors={}
    for row in neumann:
        n,steps=int(row[0]),int(row[1])
        amp,discrete,continuum,mass,error,cfl=map(float,row[2:])
        expected_steps={8:4,12:9,16:16}.get(n)
        check(f"{n} cubed eigenmode time refinement",
              expected_steps==steps,(n,steps))
        dx=1.0/n
        dt=0.2/steps
        eigenvalue=12*0.03/(dx*dx)*math.sin(math.pi/(2*n))**2
        ref_discrete=(1-eigenvalue*dt)**steps
        ref_continuum=math.exp(-3*math.pi**2*0.03*0.2)
        check(f"{n} cubed exact 3D discrete Neumann eigenvalue",
              abs(amp-ref_discrete)<3e-12 and
              abs(discrete-ref_discrete)<3e-12,(amp,ref_discrete))
        check(f"{n} cubed independent continuum Neumann reference",
              abs(continuum-ref_continuum)<1e-13 and
              abs(error-abs(amp-ref_continuum))<1e-13,error)
        check(f"{n} cubed eigenmode conservative stable CFL",
              abs(mass-2.0)<1e-11 and 0<cfl<0.8,(mass,cfl))
        errors[n]=error
    check("3D Neumann continuum error decreases under refinement",
          errors[8]>errors[12]>errors[16],errors)
    check("3D Neumann continuum second-order refinement trend",
          2.0<errors[8]/errors[12]<2.6 and
          1.6<errors[12]/errors[16]<2.0,errors)
    compartments=re.findall(
        r"^VOF_TRACER_HET_COMPARTMENTS (\d+) (\d+) "
        r"([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+) "
        r"([\d.eE+-]+) (\d+)$",output,re.MULTILINE)
    check("two 3D disconnected diffusion compartment cases",
          len(compartments)==2,[row[0] for row in compartments])
    for row in compartments:
        n,number=int(row[0]),int(row[1])
        residual=float(row[2])
        masses=list(map(float,row[3:6]))
        steps=int(row[6])
        expected=[0.0,0.0,0.0]
        for z in range(n):
            for y in range(n):
                for x in range(n):
                    if (x,y,z)==(3,3,3):
                        continue
                    group=0 if x<4 else (1 if x==4 else 2)
                    value=(1+0.02*y+0.01*z if group==0 else
                           (3+0.03*y+0.04*z if group==1 else
                            5+0.01*x+0.02*y+0.01*z))
                    expected[group]+=value
        check(f"{n} cubed three independent wet compartments",
              n in (8,12) and number==3,(n,number))
        check(f"{n} cubed diffusion CFL requires three substeps",
              steps==3,steps)
        check(f"{n} cubed independent per-compartment solute masses",
              all(abs(a-b)<1e-9 for a,b in zip(masses,expected)),
              (masses,expected))
        check(f"{n} cubed compartment ledger closes",
              math.isfinite(residual) and 0<=residual<1e-10,residual)
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
            "diagonal grid-aligned anisotropic diffusivity only; no cross-tensor terms or measured soil calibration",
            "heterogeneous cell-wise diffusivity uses harmonic face mixing, not calibrated porous media or thermodynamic transport",
            "heterogeneous diffusion-only CFL subcycling uses stationary wetted cut cells, not a coupled Navier-Stokes or reactive solver",
            "extreme harmonic mean is numerical robustness, not measured physical material calibration",
            "face-gradient and Euler-correction ledger audits numerical quadratic dissipation, NOT physical energy",
            "3D Neumann eigenmode validates passive solute diffusion, not measured Navier-Stokes CFD",
            "diffusion-connected compartment mass audit covers fixed zero-D barriers, not moving porous or FSI boundaries",
            "opt-in conservative diffusion-only CFL subcycling after one prescribed-MAC advection, not fully coupled time integration",
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
