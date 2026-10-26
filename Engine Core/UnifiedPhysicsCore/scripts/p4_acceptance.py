"""Phase 4 full CPU-MVP acceptance: heat equation, latent phases,
live MAC enthalpy transport, thermal phase mask, P3 Darcy heat feedback.
Scopes are constrained; this is NOT full CFD/steam/thermoelasticity.
"""
from __future__ import annotations
import csv,hashlib,io,json,math,re,subprocess
from datetime import datetime,timezone
from pathlib import Path
CORE=Path(__file__).resolve().parent.parent
PROJECT=CORE.parents[1]
OUTPUT=PROJECT/"ENGINE TEST STUBE"/"runs"/"p4_acceptance.json"
FILES=[
 "include/upc/thermal3d.hpp",
 "include/upc/thermal_phase_bridge.hpp",
 "include/upc/thermal_advection3d.hpp",
 "include/upc/thermal_soil_bridge.hpp",
 "include/upc/thermal_volume_estimates.hpp",
 "include/upc/coupled_water3d.hpp",
 "include/upc/material_coupling3d.hpp",
 "tests/thermal3d_tests.cpp",
 "tests/thermal_phase_bridge_tests.cpp",
 "tests/thermal_advection3d_tests.cpp",
 "tests/thermal_soil_integration_tests.cpp",
 "benchmarks/p4_reference.cpp",
 "docs/P4_THERMAL_CPU_MVP_ABNAHME.md",
 "scripts/p4_acceptance.py",
 "CMakeLists.txt",
]
def run(name):
    p=subprocess.run([str(CORE/"build"/name)],cwd=CORE,
                     capture_output=True,text=True,timeout=100)
    if p.returncode: raise RuntimeError(name+" failed: "+p.stderr[-1400:])
    return p
def main():
    checks=[]
    def gate(label,passage,value=None,limit=None):
        checks.append({"name":label,"pass":bool(passage),"value":value,"limit":limit})
    thermal=run("upc_thermal3d_tests.exe")
    phase=run("upc_thermal_phase_bridge_tests.exe")
    advection=run("upc_thermal_advection3d_tests.exe")
    soil=run("upc_thermal_soil_tests.exe")
    reference=run("upc_p4_reference.exe")
    def assertions(label,output,pattern,minimum):
        m=re.search(pattern,output.stdout)
        n=int(m.group(1)) if m else 0
        gate(label,n>=minimum,n,minimum)
        return n
    original=assertions("thermal_native_3d",thermal,
                       r"PASS: (\d+) 3D heat conduction/latent-phase",800)
    phases=assertions("ice_water_steam_dynamic_mac_voxels",phase,
                      r"PASS: (\d+) thermal 3D water/ice/vapor",25)
    gate("water_enthalpy_advection_3d",
         "PASS: live 3D MAC transport carries water mass+enthalpy" in advection.stdout,
         advection.stdout.strip(),"PASS")
    gate("P2_P3_P4_soil_heat_gravity_mass",
         "PASS: P2 MAC + P3 Darcy + P4 enthalpy/conduction integrated" in soil.stdout,
         soil.stdout.strip(),"PASS")
    rows=list(csv.DictReader(io.StringIO(reference.stdout)))
    if len(rows)!=4: raise RuntimeError("Four Neumann heat grids required")
    for row in rows:
        n=int(row["nx"])
        gate("reference_grid_"+str(n),n in (8,12,16,24) and
             int(row["ny"])==n and int(row["nz"])==n,n,"8,12,16,24")
        def num(k):
            v=float(row[k])
            if not math.isfinite(v):raise RuntimeError("Nonfinite "+k)
            return v
        err=num("max_profile_error_k")
        gate("fourier_analytical_profile_"+str(n),err<0.05,err,0.05)
        rel=num("relative_energy_residual")
        gate("fourier_relative_energy_"+str(n),rel<1e-10,rel,1e-10)
        mode=abs(num("expected_amplitude_k")-num("observed_amplitude_k"))
        gate("fourier_neumann_mode_"+str(n),mode<0.05,mode,0.05)
        elapsed=num("elapsed_ms")
        gate("finite_cpu_time_"+str(n),0<elapsed<10000,elapsed,10000)
    gate("all_P4_modules_present",all((CORE/p).is_file() for p in FILES),
         len(FILES),len(FILES))
    # Test source files are fingerprinted; do not count filenames as a pass.
    hashes={name:hashlib.sha256((CORE/name).read_bytes()).hexdigest() for name in FILES}
    accepted=all(x["pass"] for x in checks)
    data={
     "phase":"P4","schema_version":1,"accepted":accepted,
     "phase_complete":accepted,"scope":"Thermodynamic coupled 3D CPU MVP only",
     "generated_utc":datetime.now(timezone.utc).isoformat(),
     "source_sha256":hashes,"checks":checks,
     "reference":rows,"native_thermal_assertions":original,
     "native_phase_bridge_assertions":phases,
     "native_stdout":{"thermal":thermal.stdout.strip(),
                      "phase_bridge":phase.stdout.strip(),
                      "live_advection":advection.stdout.strip(),
                      "soil_integration":soil.stdout.strip()},
     "physical_limitations":[
      "No real water-vapor gas solver or 3D gas expansion advection; ice/steam phase volumes are diagnostic",
      "No high-fidelity liquid-solid interface, freeze-expansion forces, or partially frozen advection",
      "Porewater and soil matrix are separate heat ledgers; not a calibrated Richards/thermal pore constitutive model",
      "Global hydraulic kinetic-energy dissipation is not thermally recovered; conserved energy tests isolate thermal and gravity-to-heat transfers",
      "Reference coefficients are constant 1-atm water approximations; no realistic pressure/salinity steam tables"
     ]
    }
    OUTPUT.parent.mkdir(parents=True,exist_ok=True)
    OUTPUT.write_text(json.dumps(data,indent=2,ensure_ascii=False),encoding="utf-8")
    print("P4_ACCEPTED="+str(accepted))
    print("P4_CHECKS="+str(len(checks)))
    print("P4_NATIVE_THERMAL_ASSERTIONS="+str(original))
    print("P4_DYNAMIC_PHASE_ASSERTIONS="+str(phases))
    print("P4_BENCHMARK_FINE_MS="+rows[-1]["elapsed_ms"])
    print("P4_REPORT="+str(OUTPUT))
    if not accepted:
        raise SystemExit("Failed P4 gates: "+",".join(x["name"] for x in checks if not x["pass"]))
if __name__=="__main__":main()
