"""Quantitative P6 native acceptance: balanced 3D mixture, Arrhenius/Fick
reference, P4 heat coupling, conservative energy and element inventories.
Only verifies the explicitly documented CPU-MVP model, not real chemistry.
"""
from __future__ import annotations
import csv,hashlib,io,json,math,re,subprocess
from datetime import datetime,timezone
from pathlib import Path
CORE=Path(__file__).resolve().parent.parent
PROJECT=CORE.parents[1]
REPORT=PROJECT/"ENGINE TEST STUBE"/"runs"/"p6_acceptance.json"
SOURCES=[
 "CMakeLists.txt","include/upc/core.hpp",
 "include/upc/materials_ext.hpp",
 "include/upc/reactive_mixture3d.hpp",
 "include/upc/reactive_thermal_bridge.hpp",
 "include/upc/thermal3d.hpp",
 "tests/reactive_mixture3d_tests.cpp",
 "tests/reactive_thermal_bridge_tests.cpp",
 "benchmarks/p6_reference.cpp",
 "docs/P6_REACTIVE_MATERIALS_CPU_MVP_ABNAHME.md",
 "scripts/p6_acceptance.py",
]
def native(name):
    p=subprocess.run([str(CORE/"build"/name)],cwd=str(CORE),
                     capture_output=True,text=True,timeout=100)
    if p.returncode:
        raise RuntimeError(name+": "+p.stderr[-1800:])
    return p.stdout.strip()
def numerical(entry,key):
    value=float(entry[key])
    if not math.isfinite(value):raise ValueError("Not finite: "+key)
    return value
def main():
    core=native("upc_p6_reactive3d_tests.exe")
    coupled=native("upc_p6_thermal_bridge_tests.exe")
    benchmark=native("upc_p6_reference.exe")
    rows=list(csv.DictReader(io.StringIO(benchmark)))
    if len(rows)!=6:raise RuntimeError("Missing 6 P6 analytic/Fick CPU references")
    checks=[]
    def gate(name,ok,value=None,target=None):
        checks.append({"name":name,"pass":bool(ok),"value":value,"target":target})
    def assertions(name,out,label,minimum):
        match=re.search(label+r".*?checks=(\d+)",out)
        count=int(match.group(1)) if match else 0
        gate(name,count>=minimum,count,minimum)
        return count
    basic=assertions("3D_synthetic_stoich_Fick_native",core,
                     r"PASS: P6 synthetic 3D mixtures",830)
    heat=assertions("P4_thermal_feedback_native",coupled,
                    r"PASS: P6 balanced reaction chemical energy",300)
    reaction=[x for x in rows if x["scenario"]=="arrhenius_kinetics"]
    diffusion=[x for x in rows if x["scenario"]=="fick_3d"]
    gate("3_kinetic_references",len(reaction)==3,len(reaction),3)
    gate("3_3D_diffusion_references",len(diffusion)==3,len(diffusion),3)
    for i,item in enumerate(reaction):
        temp=numerical(item,"temp_k")
        expected=1-math.exp(-2.0*math.exp(-14000/(8.31446261815324*temp))*0.3)
        observed=numerical(item,"observed")
        err=abs(observed-expected)
        gate("arrhenius_exact_"+str(i),err<1e-11,err,1e-11)
        gate("kinetic_bounded_"+str(i),0<observed<1,observed,"(0,1)")
        mass_err=abs(numerical(item,"mass_before_kg")-
                     numerical(item,"mass_after_kg"))
        gate("reactive_mass_"+str(i),mass_err<1e-10,mass_err,1e-10)
        energy_err=abs(numerical(item,"energy_residual_j"))
        gate("reaction_heat_plus_chemical_"+str(i),
             energy_err<1e-8,energy_err,1e-8)
        gate("reaction_cpu_"+str(i),
             0<numerical(item,"elapsed_ms")<10000,
             numerical(item,"elapsed_ms"),10000)
    gate("temperature_activated_rate_monotonic",
         all(numerical(reaction[i],"observed")<
             numerical(reaction[i+1],"observed") for i in range(2)),
         [numerical(x,"observed") for x in reaction],"ascending")
    for i,item in enumerate(diffusion):
        n=int(item["nx"])
        gate("3D_Fick_grid_"+str(i),n in (8,12,16) and
             int(numerical(item,"observed"))==n*n,n,n*n)
        mass_err=abs(numerical(item,"mass_before_kg")-
                     numerical(item,"mass_after_kg"))
        gate("3D_Fick_mass_"+str(i),mass_err<1e-9,mass_err,1e-9)
        energy_err=abs(numerical(item,"energy_residual_j"))
        gate("3D_Fick_sensible_chemical_energy_"+str(i),
             energy_err<1e-7,energy_err,1e-7)
        gate("Fick_amount_per_grid_"+str(i),
             abs(numerical(item,"observed")-numerical(item,"expected"))<1e-9,
             numerical(item,"observed"),numerical(item,"expected"))
        gate("Fick_CPU_time_"+str(i),0<numerical(item,"elapsed_ms")<10000,
             numerical(item,"elapsed_ms"),10000)
    gate("source_files_exist",all((CORE/p).is_file() for p in SOURCES),
         len(SOURCES),len(SOURCES))
    hashes={name:hashlib.sha256((CORE/name).read_bytes()).hexdigest()
            for name in SOURCES}
    approved=all(c["pass"] for c in checks)
    data={
        "phase":"P6","schema_version":1,
        "accepted":approved,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "scope":"Synthetic-element stoichiometric 3D mixture, finite volume Fick diffusion, Arrhenius limiting-species pseudo-first-order, chemical/sensible energy and P4 solid contact CPU-MVP",
        "checks":checks,"reference":rows,
        "source_sha256":hashes,
        "native_assertions":{"mixture":basic,"p4_contact":heat},
        "native_output":{"mixture":core,"p4_contact":coupled},
        "excluded":"No calibrated real reaction enthalpy database, actual real-substance chemistry, turbulent MAC advection, full reactive 3D CFD/FSI or global energy closure across fluid/solids"
    }
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(data,indent=2,ensure_ascii=False),encoding="utf-8")
    print("P6_ACCEPTED="+str(approved))
    print("P6_CHECKS="+str(len(checks)))
    print("P6_NATIVE_MIXTURE_ASSERTIONS="+str(basic))
    print("P6_NATIVE_P4_CONTACT_ASSERTIONS="+str(heat))
    print("P6_REFERENCE_GRIDS="+",".join(str(int(x["nx"])) for x in diffusion))
    print("P6_REPORT="+str(REPORT))
    if not approved:
        raise SystemExit("P6 quantitative gates failed: "+
                         ",".join(c["name"] for c in checks if not c["pass"]))
if __name__=="__main__":main()
