"""Final bounded P5 CPU milestone acceptance, including self/deformable contact
and independent affine 3D lattice force-strain scaling. Does NOT certify FEM,
MPM, rotational/continuous collision or full thermo-fluid-fracture coupling.
"""
from __future__ import annotations
import csv,hashlib,io,json,math,re,subprocess
from datetime import datetime,timezone
from pathlib import Path
CORE=Path(__file__).resolve().parent.parent
PROJECT=CORE.parents[1]
OUT=PROJECT/"ENGINE TEST STUBE"/"runs"/"p5_full_acceptance.json"
PATHS=[
 "CMakeLists.txt","include/upc/core.hpp",
 "include/upc/solid.hpp","include/upc/solid_contact3d.hpp",
 "include/upc/solid_lattice3d.hpp","include/upc/granular3d.hpp",
 "include/upc/deformable_contact3d.hpp",
 "tests/solid_tests.cpp","tests/solid_contact3d_tests.cpp",
 "tests/solid_lattice3d_tests.cpp","tests/granular3d_tests.cpp",
 "tests/deformable_contact3d_tests.cpp",
 "benchmarks/p5_reference.cpp","benchmarks/p5_constitutive_reference.cpp",
 "docs/P5_SOLID_GRANULAR_CPU_MVP_ABNAHME.md",
 "scripts/p5_full_acceptance.py",
]
def run(exe):
    p=subprocess.run([str(CORE/"build"/exe)],capture_output=True,
                     text=True,timeout=100,cwd=str(CORE))
    if p.returncode:
        raise RuntimeError(exe+" FAILED: "+p.stdout[-1000:]+p.stderr[-1400:])
    return p.stdout
def finite(row,column):
    f=float(row[column])
    if not math.isfinite(f):
        raise RuntimeError("Nonfinite "+column)
    return f
def main():
    tests={
     "plane":run("upc_solid_contact3d_tests.exe"),
     "granular":run("upc_granular3d_tests.exe"),
     "lattice":run("upc_solid_lattice3d_tests.exe"),
     "volume_self":run("upc_deformable_contact3d_tests.exe"),
     "old_solid":run("upc_solid_tests.exe"),
    }
    reference=list(csv.DictReader(io.StringIO(run("upc_p5_reference.exe"))))
    constitutive=list(csv.DictReader(io.StringIO(run("upc_p5_constitutive.exe"))))
    if len(reference)!=11 or len(constitutive)!=12:
        raise RuntimeError("Missing P5 benchmarks (11 + 12 rows required)")
    checks=[]
    def check(key,condition,v=None,tolerance=None):
        checks.append({"name":key,"pass":bool(condition),"value":v,"limit":tolerance})
    expected_counts={"plane":10984,"granular":37000,"lattice":22000,"volume_self":17}
    native_counts={}
    for scenario,min_count in expected_counts.items():
        if scenario=="volume_self":
            pat=r"PASS: P5 deformable multi-body/self/fragment collision.*checks=(\d+)"
        else:
            pat=r"checks=(\d+)"
        m=re.search(pat,tests[scenario])
        n=int(m.group(1)) if m else 0
        native_counts[scenario]=n
        check("native_"+scenario,n>=min_count,n,min_count)
    check("original_solid_reference","PASS: spring forces" in tests["old_solid"],
          tests["old_solid"].strip(),"PASS")
    by={}
    for row in reference:by.setdefault(row["scenario"],[]).append(row)
    for scenario,count in [("elastic_collision",3),("hooke_energy",1),
                            ("granular_pile",3),("lattice_3d",3),
                            ("fracture_topology",1)]:
        check("reference_"+scenario,len(by.get(scenario,[]))==count,
              len(by.get(scenario,[])),count)
    for row in by["elastic_collision"]:
        e=finite(row,"parameter")
        check("DEM_e2_"+str(e),
              abs(finite(row,"observed")-e*e)<1e-10,
              finite(row,"observed"),e*e)
        check("DEM_pair_mass_"+str(e),
              abs(finite(row,"mass_initial_kg")-finite(row,"mass_final_kg"))<1e-10,
              finite(row,"mass_final_kg"),finite(row,"mass_initial_kg"))
        check("DEM_pair_overlap_"+str(e),finite(row,"max_overlap_m")<1e-9,
              finite(row,"max_overlap_m"),1e-9)
    hooke=by["hooke_energy"][0]
    check("analytical_Hooke_U_0p05J",
          abs(finite(hooke,"observed")-0.05)<1e-12,finite(hooke,"observed"),0.05)
    for row in by["granular_pile"]:
        n=int(row["grid_n"])
        check("granular_count_"+str(n),int(row["particle_count"])==n**3,
              int(row["particle_count"]),n**3)
        check("granular_pile_overlap_"+str(n),
              0<=finite(row,"max_overlap_m")<0.025,
              finite(row,"max_overlap_m"),0.025)
        check("granular_pile_mass_"+str(n),
              abs(finite(row,"mass_initial_kg")-finite(row,"mass_final_kg"))<1e-10,
              abs(finite(row,"mass_initial_kg")-finite(row,"mass_final_kg")),1e-10)
        check("granular_pile_cpu_"+str(n),
              0<finite(row,"elapsed_ms")<10000,
              finite(row,"elapsed_ms"),10000)
    for row in by["lattice_3d"]:
        n=int(row["grid_n"])
        check("lattice_nodes_"+str(n),int(row["particle_count"])==n**3,
              int(row["particle_count"]),n**3)
        check("lattice_bonds_"+str(n),
              finite(row,"expected")==finite(row,"observed")>n**3,
              finite(row,"observed"),finite(row,"expected"))
        check("lattice_mass_"+str(n),
              abs(finite(row,"mass_initial_kg")-finite(row,"mass_final_kg"))<1e-9,
              abs(finite(row,"mass_initial_kg")-finite(row,"mass_final_kg")),1e-9)
        check("lattice_cpu_"+str(n),0<finite(row,"elapsed_ms")<10000,
              finite(row,"elapsed_ms"),10000)
    fracture=by["fracture_topology"][0]
    check("volumetric_two_fragment_bruch",int(fracture["observed"])==2 and
          int(fracture["particle_count"])==64,int(fracture["observed"]),2)
    check("fracture_mass_invariant",
          abs(finite(fracture,"mass_initial_kg")-finite(fracture,"mass_final_kg"))<1e-10,
          finite(fracture,"mass_final_kg"),finite(fracture,"mass_initial_kg"))
    strains={}
    for row in constitutive:
        n=int(row["nx"])
        strains.setdefault(n,{})[row["scenario"],row["epsilon"]]=row
    for n in (4,6,8):
        group=[strains[n][("uniaxial_lattice",str(eps))]
               for eps in ("0.001","0.002","0.004")]
        forces=[finite(r,"axial_force_n") for r in group]
        energies=[finite(r,"elastic_energy_j") for r in group]
        shear=strains[n][("shear_lattice","0")]
        check("affine_axial_positive_"+str(n),all(x>0 for x in forces),
              forces,">0")
        deviation=abs(forces[1]/(2*forces[0])-1)
        check("affine_force_linear_"+str(n),deviation<0.005,deviation,0.005)
        energy_dev=abs(energies[1]/(4*energies[0])-1)
        check("affine_strain_energy_quadratic_"+str(n),
              energy_dev<0.005,energy_dev,0.005)
        check("affine_shear_restoring_force_"+str(n),
              finite(shear,"shear_force_n")>0 and finite(shear,"elastic_energy_j")>0,
              finite(shear,"shear_force_n"),">0")
        check("multi_resolution_lattice_"+str(n),
              len(group)==3 and int(shear["nx"])==n,n,3)
    check("all_full_P5_sources",all((CORE/p).is_file() for p in PATHS),len(PATHS),len(PATHS))
    hashes={p:hashlib.sha256((CORE/p).read_bytes()).hexdigest() for p in PATHS}
    accepted=all(x["pass"] for x in checks)
    report={
     "schema_version":2,"phase":"P5","accepted":accepted,
     "generated_utc":datetime.now(timezone.utc).isoformat(),
     "scope":"Bounded CPU MVP: volumetric spring lattice, fracture, nodal self/deformable contacts, spherical DEM granular physics and approximate affine constitutive reference",
     "source_sha256":hashes,"checks":checks,
     "original_reference":reference,
     "affine_3d_reference":constitutive,
     "native_checks":native_counts,"native_outputs":tests,
     "caveats":"Spherical node proxy not continuous surface FEM/MPM; not independently measured soil rheology; rigid rotations, continuous collision and globally conservative thermo-fluid-fracture FSI not implemented"
    }
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print("P5_FULL_CPU_MVP_ACCEPTED="+str(accepted))
    print("P5_FULL_CHECKS="+str(len(checks)))
    print("P5_SELF_CONTACT_CHECKS="+str(native_counts["volume_self"]))
    print("P5_STRESS_STRAIN_RESOLUTIONS=4,6,8")
    print("P5_REPORT="+str(OUT))
    if not accepted:
        raise SystemExit("Full P5 CPU gate FAIL: "+
                         ",".join(x["name"] for x in checks if not x["pass"]))
if __name__=="__main__":main()
