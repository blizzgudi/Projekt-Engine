"""P5 CPU solid/fracture/granular quantitative PARTIAL acceptance.
Requires local MSVC release binaries; never stamps a phase PASS from filenames.
"""
from __future__ import annotations
import csv,hashlib,io,json,math,re,subprocess
from datetime import datetime,timezone
from pathlib import Path
CORE=Path(__file__).resolve().parent.parent
PROJECT=CORE.parents[1]
REPORT=PROJECT/"ENGINE TEST STUBE"/"runs"/"p5_acceptance.json"
SOURCES=[
    "CMakeLists.txt",
    "include/upc/core.hpp",
    "include/upc/solid.hpp",
    "include/upc/solid_contact3d.hpp",
    "include/upc/solid_lattice3d.hpp",
    "include/upc/granular3d.hpp",
    "tests/solid_tests.cpp",
    "tests/solid_contact3d_tests.cpp",
    "tests/solid_lattice3d_tests.cpp",
    "tests/granular3d_tests.cpp",
    "benchmarks/p5_reference.cpp",
    "docs/P5_SOLID_GRANULAR_CPU_MVP_ABNAHME.md",
    "scripts/p5_acceptance.py",
]
def native(exe):
    cmd=[str(CORE/"build"/exe)]
    r=subprocess.run(cmd,cwd=str(CORE),capture_output=True,text=True,
                     timeout=100,check=False)
    if r.returncode:
        raise RuntimeError(f"{exe} failed ({r.returncode}): {r.stderr[-2500:]}")
    return r.stdout
def main():
    contact=native("upc_solid_contact3d_tests.exe")
    granular=native("upc_granular3d_tests.exe")
    lattice=native("upc_solid_lattice3d_tests.exe")
    original=native("upc_solid_tests.exe")
    benchmark=native("upc_p5_reference.exe")
    rows=list(csv.DictReader(io.StringIO(benchmark)))
    if len(rows)!=11:
        raise RuntimeError(f"Missing P5 quantitative case (expected 11, received {len(rows)})")
    checks=[]
    def gate(label,predicate,value=None,limit=None):
        checks.append({"name":label,"pass":bool(predicate),"value":value,"limit":limit})
    def value(row,key):
        a=float(row[key])
        if not math.isfinite(a):raise RuntimeError(f"Nonfinite {key}")
        return a
    def native_count(name,log,prefix,minimum):
        match=re.search(prefix+r".*?checks=(\d+)",log)
        n=int(match.group(1)) if match else 0
        gate("native_"+name,n>=minimum,n,minimum)
        return n
    native_checks={
      "plane":native_count("plane",contact,r"PASS: P5 bounded unilateral plane contact",10984),
      "granular":native_count("granular",granular,r"PASS: P5 granular 3D pair collision",37000),
      "lattice":native_count("lattice",lattice,r"PASS: P5 volumetric spring lattice",22000),
    }
    gate("old_solid_regression","PASS: spring forces" in original,original.strip(),"PASS")
    scenarios={}
    for row in rows:scenarios.setdefault(row["scenario"],[]).append(row)
    for name,count in [
      ("elastic_collision",3),("hooke_energy",1),("granular_pile",3),
      ("lattice_3d",3),("fracture_topology",1),
    ]:
        gate("benchmark_"+name,len(scenarios.get(name,[]))==count,
             len(scenarios.get(name,[])),count)
    elastic=scenarios["elastic_collision"]
    for row in elastic:
        e=value(row,"parameter")
        expected=value(row,"expected")
        observed=value(row,"observed")
        gate("restitution_energy_e_"+str(e),abs(observed-expected)<1e-10,
             observed,expected)
        gate("pair_mass_e_"+str(e),
             abs(value(row,"mass_initial_kg")-value(row,"mass_final_kg"))<1e-11,
             value(row,"mass_final_kg"),value(row,"mass_initial_kg"))
        gate("pair_zero_overlap_e_"+str(e),
             value(row,"max_overlap_m")<1e-10,
             value(row,"max_overlap_m"),1e-10)
    hooke=scenarios["hooke_energy"][0]
    gate("Hooke_independent_reference_0p05J",
         abs(value(hooke,"observed")-value(hooke,"expected"))<1e-12 and
         abs(value(hooke,"expected")-0.05)<1e-12,
         value(hooke,"observed"),0.05)
    for row in scenarios["granular_pile"]:
        n=int(row["grid_n"])
        gate("pile_count_"+str(n),int(row["particle_count"])==n**3,
             int(row["particle_count"]),n**3)
        gate("pile_mass_"+str(n),
             abs(value(row,"mass_final_kg")-value(row,"mass_initial_kg"))<1e-10,
             abs(value(row,"mass_final_kg")-value(row,"mass_initial_kg")),1e-10)
        gate("pile_interpenetration_"+str(n),
             0<=value(row,"max_overlap_m")<0.025,
             value(row,"max_overlap_m"),0.025)
        gate("pile_cpu_time_"+str(n),
             0<value(row,"elapsed_ms")<10000,value(row,"elapsed_ms"),10000)
    for row in scenarios["lattice_3d"]:
        n=int(row["grid_n"])
        gate("lattice_count_"+str(n),int(row["particle_count"])==n**3,
             int(row["particle_count"]),n**3)
        gate("lattice_intact_"+str(n),
             abs(value(row,"observed")-value(row,"expected"))<1e-12
             and value(row,"observed")>n**3,
             value(row,"observed"),value(row,"expected"))
        gate("lattice_mass_"+str(n),
             abs(value(row,"mass_final_kg")-value(row,"mass_initial_kg"))<1e-9,
             abs(value(row,"mass_final_kg")-value(row,"mass_initial_kg")),1e-9)
        gate("lattice_cpu_time_"+str(n),
             0<value(row,"elapsed_ms")<10000,value(row,"elapsed_ms"),10000)
    fracture=scenarios["fracture_topology"][0]
    gate("3d_fracture_two_pieces",
         int(fracture["observed"])==2 and int(fracture["particle_count"])==64,
         int(fracture["observed"]),2)
    gate("fracture_conserves_solid_mass",
         abs(value(fracture,"mass_initial_kg")-value(fracture,"mass_final_kg"))<1e-10,
         abs(value(fracture,"mass_initial_kg")-value(fracture,"mass_final_kg")),1e-10)
    # A physically timed 3D test run is obligatory; short source signatures
    # and static code inspection are not sufficient for acceptance.
    gate("all_material_source_files",all((CORE/s).is_file() for s in SOURCES),
         len(SOURCES),len(SOURCES))
    hashes={path:hashlib.sha256((CORE/path).read_bytes()).hexdigest()
            for path in SOURCES}
    ok=all(item["pass"] for item in checks)
    payload={
       "phase":"P5","schema_version":1,"partial_accepted":ok,
       "phase_complete":False,
       "scope":"3D CPU lattice fracture, bounded plane/multi-grain contacts and voxel damage feedback",
       "generated_utc":datetime.now(timezone.utc).isoformat(),
       "native_checks":native_checks,
       "reference":rows,"checks":checks,"source_sha256":hashes,
       "native_outputs":{"plane":contact.strip(),"granular":granular.strip(),
                         "lattice":lattice.strip(),"old_solid":original.strip()},
       "limitations":"No continuous collision detection, full continuum FEM/MPM, rotating rigid bodies, realistic granular soil rheology, globally conservative thermo-fluid-solid FSI or GPU production"
    }
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(payload,indent=2,ensure_ascii=False),encoding="utf-8")
    print("P5_PARTIAL_ACCEPTED="+str(ok))
    print("P5_PHASE_COMPLETE=False")
    print("P5_CHECKS="+str(len(checks)))
    print("P5_NATIVE_PLANE_ASSERTIONS="+str(native_checks["plane"]))
    print("P5_NATIVE_LATTICE_ASSERTIONS="+str(native_checks["lattice"]))
    print("P5_NATIVE_GRANULAR_ASSERTIONS="+str(native_checks["granular"]))
    print("P5_GRANULAR_64_CPU_MS="+str(scenarios["granular_pile"][-1]["elapsed_ms"]))
    print("P5_LATTICE_512_CPU_MS="+str(scenarios["lattice_3d"][-1]["elapsed_ms"]))
    print("P5_REPORT="+str(REPORT))
    if not ok:
        raise SystemExit("P5 PARTIAL FAIL: "+",".join(x["name"] for x in checks if not x["pass"]))
if __name__=="__main__":main()
