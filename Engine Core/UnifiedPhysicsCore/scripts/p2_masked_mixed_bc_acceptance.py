"""P2 mixed-BC 3D MAC numerical reference (bounded partial, NOT full CFD).
Independently recomputes discrete eigenvalues/energy ratios from native CSV,
checks source and executable fingerprints, and fails closed on mismatches.
"""
from __future__ import annotations
import csv
import hashlib
import io
import json
import math
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
ROOT=CORE.parents[1]
REPORT=ROOT/"ENGINE TEST STUBE"/"runs"/"p2_masked_mixed_bc_partial.json"
SOURCES=(
    "include/upc/mac3d.hpp",
    "include/upc/viscous_mac3d.hpp",
    "tests/viscous_masked_eigenmode3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_masked_mixed_bc_acceptance.py",
    "docs/P2_MASKED_MIXED_BC_EIGENMODE_PARTIAL.md",
    "docs/PHASENPLAN.md",
)
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main()->None:
    exe=CORE/"build"/"upc_p2_masked_eigenmode_tests.exe"
    if not exe.is_file():
        raise RuntimeError("Native Release executable missing; build in VS developer environment")
    result=subprocess.run([str(exe)],cwd=CORE,capture_output=True,text=True,timeout=90)
    if result.returncode!=0:
        raise RuntimeError("Native mixed-BC eigenmode FAILED: "+result.stderr[-1400:])
    lines=[line.strip() for line in result.stdout.splitlines() if line.strip()]
    if len(lines)!=10 or lines[0]!="nx,ny,nz,wet_columns,substeps,alpha,lambda,expected_factor,max_error,energy_ratio,ledger_residual":
        raise RuntimeError("Native CSV format or row count mismatch")
    if not lines[-1].startswith("PASS: P2 masked mixed-BC eigenmode assertions=") or not lines[-1].endswith(" phase_complete=false"):
        raise RuntimeError("Native assertion/pass/partial marker missing")
    native_assertions=int(lines[-1].split("assertions=")[1].split()[0])
    if native_assertions<60000:
        raise RuntimeError("Insufficient discrete and continuum native assertions")
    rows=list(csv.DictReader(io.StringIO("\n".join(lines[:4]))))
    if len(rows)!=3:
        raise RuntimeError("Expected three distinct 3D reference grids")
    checks=[]
    def check(name,condition,value=None,limit=None):
        checks.append({"name":name,"pass":bool(condition),"value":value,"limit":limit})
    expected_grids=[(8,7,6,3),(10,9,8,5),(12,11,10,7)]
    errors=[]
    for index,(row,grid) in enumerate(zip(rows,expected_grids)):
        try:
            nx,ny,nz,L=(int(row[name]) for name in ("nx","ny","nz","wet_columns"))
            substeps=int(row["substeps"])
            alpha=float(row["alpha"])
            lam=float(row["lambda"])
            factor=float(row["expected_factor"])
            error=float(row["max_error"])
            energy=float(row["energy_ratio"])
            residual=float(row["ledger_residual"])
        except (ValueError,KeyError,TypeError) as exc:
            raise RuntimeError("Invalid numeric native CSV") from exc
        if not all(map(math.isfinite,(alpha,lam,factor,error,energy,residual))):
            raise RuntimeError("Nonfinite native reference value")
        suffix=f"grid_{index}"
        check(suffix+"_dimensions",(nx,ny,nz,L)==grid,[nx,ny,nz,L],grid)
        check(suffix+"_adaptive_steps",substeps==3,substeps,3)
        independent_alpha=0.05*0.08/(0.1**2*substeps)
        independent_lambda=(
            2*(math.cos(math.pi/(2*L+1))-1)
            +2*(math.cos(math.pi/ny)-1)
            +2*(math.cos(math.pi/(nz+1))-1)
        )
        independent_factor=(1+independent_alpha*independent_lambda)**substeps
        check(suffix+"_alpha",abs(alpha-independent_alpha)<2e-14,alpha,independent_alpha)
        check(suffix+"_lambda",abs(lam-independent_lambda)<2e-13,lam,independent_lambda)
        check(suffix+"_factor",abs(factor-independent_factor)<2e-13,factor,independent_factor)
        check(suffix+"_eigenmode_max_error",0<=error<2e-12,error,2e-12)
        check(suffix+"_energy_ratio",abs(energy-independent_factor**2)<3e-12,energy,independent_factor**2)
        check(suffix+"_energy_ledger_residual",0<=residual<2e-11,residual,2e-11)
        errors.append(error)
    check("max_error_finite",max(errors)<2e-12,max(errors),2e-12)
    check("native_assertions",native_assertions>=60000,native_assertions,60000)
    # A second, physically distinct reference: continuum 3D heat-equation
    # eigen-decay on a fixed 1m mixed Dirichlet/Neumann slab, four refinements.
    # It is analytic PDE validation, not experimental Navier-Stokes data.
    expected_header=("continuum_nx,ny,nz,wet_columns,timesteps,dx,"
                     "continuum_factor,numerical_factor,max_error,ledger_residual")
    if lines[4]!=expected_header:
        raise RuntimeError("Continuum manufactured-reference header missing")
    continuum_rows=list(csv.DictReader(io.StringIO("\n".join(lines[4:9]))))
    if len(continuum_rows)!=4:
        raise RuntimeError("Four continuum grid refinements required")
    continuum_results=[]
    previous_error=None
    for i,(row,L) in enumerate(zip(continuum_rows,(5,9,17,33))):
        try:
            nx,ny,nz,columns=(int(row[k]) for k in
                ("continuum_nx","ny","nz","wet_columns"))
            nsteps=int(row["timesteps"])
            dx=float(row["dx"])
            continuum=float(row["continuum_factor"])
            numerical=float(row["numerical_factor"])
            error=float(row["max_error"])
            residual=float(row["ledger_residual"])
        except (ValueError,TypeError,KeyError) as exc:
            raise RuntimeError("Invalid continuum benchmark CSV") from exc
        if not all(map(math.isfinite,(dx,continuum,numerical,error,residual))):
            raise RuntimeError("Nonfinite continuum benchmark")
        expected_grid=(L+5,L+1,L,L)
        check(f"continuum_{i}_grid",(nx,ny,nz,columns)==expected_grid,
              [nx,ny,nz,columns],list(expected_grid))
        check(f"continuum_{i}_time",nsteps==32,nsteps,32)
        ref_dx=1/(L+0.5)
        tx,ty,tz=(math.pi/(2*L+1),math.pi/ny,math.pi/(nz+1))
        ref_continuum=math.exp(-0.01*0.04*(tx*tx+ty*ty+tz*tz)/(ref_dx**2))
        discrete_lambda=sum(2*(math.cos(t)-1) for t in (tx,ty,tz))
        ref_numerical=(1+0.01*(0.04/32)/(ref_dx**2)*discrete_lambda)**32
        check(f"continuum_{i}_dx",abs(dx-ref_dx)<1e-14,dx,ref_dx)
        check(f"continuum_{i}_analytic_factor",abs(continuum-ref_continuum)<2e-13,
              continuum,ref_continuum)
        check(f"continuum_{i}_discrete_factor",abs(numerical-ref_numerical)<2e-13,
              numerical,ref_numerical)
        check(f"continuum_{i}_actual_error",
              0<error<1e-3 and error<=abs(ref_numerical-ref_continuum)+1e-11
              and error>0.5*abs(ref_numerical-ref_continuum),
              error,abs(ref_numerical-ref_continuum))
        check(f"continuum_{i}_ledger",0<=residual<1e-8,residual,1e-8)
        if previous_error is not None:
            check(f"continuum_{i}_refinement",error<previous_error*0.65,
                  error,previous_error*0.65)
        previous_error=error
        continuum_results.append({"grid":[nx,ny,nz],"dx_m":dx,
            "analytic_continuum_factor":continuum,
            "discrete_factor":numerical,"absolute_max_error":error,
            "ledger_residual":residual})
    check("continuum_finest_vs_coarsest",
          continuum_results[-1]["absolute_max_error"]
          <continuum_results[0]["absolute_max_error"]*0.05,
          continuum_results[-1]["absolute_max_error"],
          continuum_results[0]["absolute_max_error"]*0.05)
    fingerprints={}
    for name in SOURCES:
        path=CORE/name
        if not path.is_file():
            raise RuntimeError("Missing tested source "+name)
        fingerprints[name]=sha(path)
    ctest_log=ROOT/"ENGINE TEST STUBE"/"runs"/"p2_continuum_refinement_ctest_20261026.txt"
    if not ctest_log.is_file():
        raise RuntimeError("Full 39-test CTest evidence log missing")
    log_text=ctest_log.read_text(encoding="utf-8")
    check("full_ctest_39_of_39",
          "100% tests passed, 0 tests failed out of 39" in log_text
          and "39/39 Test" in log_text,
          "39/39" if "39/39 Test" in log_text else "MISSING","39/39")
    passed=all(x["pass"] for x in checks)
    report={
        "schema_version":1,
        "phase":"P2",
        "milestone":"masked_mixed_dirichlet_neumann_eigenmode_partial",
        "partial_accepted":passed,
        "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "scope":"Stationary 3D MAC y-face diffusion; solid Dirichlet left, air Neumann right, exterior no-slip; three discrete analytic eigenmodes plus four grid-refined continuum PDE manufactured modes; not physical FSI.",
        "continuum_reference":continuum_results,
        "native_assertions":native_assertions,
        "native_stdout":result.stdout,
        "checks":checks,
        "source_sha256":fingerprints,
        "executable_sha256":sha(exe),
        "ctest_log_sha256":sha(ctest_log),
        "ctest_log_path":str(ctest_log),
        "limitation":"Independent discrete and continuum analytic diffusion eigenmodes, NOT independent laboratory 3D Navier-Stokes measurements. No moving body, free-surface stress tensor, two-way impulse, or complete energy budget.",
    }
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print("P2_MIXED_BC_PARTIAL_ACCEPTED="+str(passed))
    print("P2_MIXED_BC_CHECKS="+str(len(checks)))
    print("P2_MIXED_BC_NATIVE_ASSERTIONS="+str(native_assertions))
    print("P2_MIXED_BC_MAX_ERROR="+repr(max(errors)))
    print("P2_CONTINUUM_ERRORS="+repr([x["absolute_max_error"] for x in continuum_results]))
    print("P2_MIXED_BC_PHASE_COMPLETE=False")
    print("P2_MIXED_BC_REPORT="+str(REPORT))
    if not passed:
        raise SystemExit("P2 partial gate failed: "+",".join(x["name"] for x in checks if not x["pass"]))

if __name__=="__main__":
    main()
