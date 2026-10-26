"""P3 native CPU acceptance: porous coupling, rigid force, local energy ledger.
No false claim of globally conservative 3D Navier-Stokes/FSI.
"""
from __future__ import annotations
import csv,hashlib,io,json,math,re,subprocess
from datetime import datetime,timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
ROOT=CORE.parents[1]
LAB=ROOT/"ENGINE TEST STUBE"
EVIDENCE=LAB/"runs"/"p3_acceptance.json"
FINGERPRINT=[
    "include/upc/core.hpp",
    "include/upc/fluid3d_module.hpp",
    "include/upc/coupled_water3d.hpp",
    "include/upc/material_coupling3d.hpp",
    "tests/material_coupling3d_tests.cpp",
    "benchmarks/p3_reference.cpp",
    "docs/P3_MATERIAL_VOLUME_ABNAHME.md",
    "scripts/p3_acceptance.py",
    "CMakeLists.txt",
]
def native(filename):
    p=subprocess.run([str(CORE/"build"/filename)],cwd=str(CORE),
                     capture_output=True,text=True,timeout=100)
    if p.returncode:
        raise RuntimeError(filename+" error: "+p.stderr[-1200:])
    return p
def main():
    test=native("upc_material_coupling3d_tests.exe")
    benchmark=native("upc_p3_reference.exe")
    items=list(csv.DictReader(io.StringIO(benchmark.stdout)))
    if len(items)!=12:raise RuntimeError("P3 expected 12 CSV reference rows")
    darcy=[x for x in items if x["scenario"]=="darcy_analytic"]
    integrated=[x for x in items if x["scenario"]=="mac_vof_soil_3d"]
    checks=[]
    def gate(label,passed,value=None,threshold=None):
        checks.append({"name":label,"pass":bool(passed),"value":value,"threshold":threshold})
    def number(row,key):
        value=float(row[key])
        if not math.isfinite(value):raise RuntimeError("Non-finite "+key)
        return value
    if len(darcy)!=9 or len(integrated)!=3:
        raise RuntimeError("Missing 3-grid/3-permeability P3 data")
    for n,entry in enumerate(darcy):
        key=str(n)
        expected=number(entry,"expected_flux_m3")
        observed=number(entry,"observed_flux_m3")
        flux_error=abs(observed-expected)
        gate("darcy_analytic_flux_"+key,flux_error<1e-12,flux_error,1e-12)
        water_error=abs(number(entry,"volume_initial_m3")-
                        number(entry,"volume_final_m3"))
        gate("darcy_single_cell_volume_"+key,water_error<1e-12,water_error,1e-12)
        energy_error=abs(number(entry,"energy_balance_error_j"))
        gate("darcy_single_cell_energy_"+key,energy_error<1e-9,energy_error,1e-9)
    for n,entry in enumerate(integrated):
        key=str(n)
        transferred=number(entry,"observed_flux_m3")
        gate("coupled_3d_transfer_"+key,transferred>1e-6,transferred,1e-6)
        volume_error=abs(number(entry,"volume_initial_m3")-
                         number(entry,"volume_final_m3"))
        gate("coupled_3d_mass_"+key,volume_error<1e-9,volume_error,1e-9)
        energy_error=abs(number(entry,"energy_balance_error_j"))
        gate("coupled_3d_porous_energy_"+key,energy_error<1e-7,energy_error,1e-7)
        millis=number(entry,"elapsed_ms")
        gate("coupled_3d_cpu_ms_"+key,0<millis<10000,millis,10000)
    # Compare Darcy response to permeability at unchanged geometry.
    for n in range(3):
        group=darcy[3*n:3*n+3]
        values=[number(row,"observed_flux_m3") for row in group]
        gate("material_conductivity_order_"+str(n),values[0]==0 and
             values[1]>0 and values[2]>values[1],values,"0 < low < high")
    # 3D solid buoyancy analytical rho*g*V, moving voxel work-energy+MAC resumption
    # are checked in the native C++ test; parse independent pressure quantity.
    m=re.search(r"PRESSURE_FORCE_N=([0-9.eE+-]+) BUOYANCY_EXPECTED_N=([0-9.eE+-]+)",
                test.stderr)
    if not m:raise RuntimeError("P3 pressure traction evidence missing")
    force,reference=map(float,m.groups())
    relative_force_error=abs(force-reference)/reference
    gate("rigid_3d_hydrostatic_buoyancy",relative_force_error<0.05,
         relative_force_error,0.05)
    m=re.search(r"PASS: (\d+) 3D porous-material/voxel/pressure/moving-body tests",
                test.stdout)
    native_checks=int(m.group(1)) if m else 0
    gate("material_voxel_infiltration_body_checks",native_checks>=500,native_checks,500)
    gate("tracked_material_runtime",all((CORE/f).is_file() for f in FINGERPRINT),
         len(FINGERPRINT),len(FINGERPRINT))
    hashes={p:hashlib.sha256((CORE/p).read_bytes()).hexdigest() for p in FINGERPRINT}
    report={
        "phase":"P3",
        "schema_version":1,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "accepted":all(c["pass"] for c in checks),
        "scope":"3D CPU MVP porous material/volume exchange, pressure force and grid-quantized rigid body",
        "limitation":"No fully conservative fluid momentum-energy FSI, no Richards-unsaturated/capillary physics, no thermal dissipation return; only isolated Darcy gravitational and body work-energy ledgers are conservative",
        "source_sha256":hashes,
        "reference":items,
        "checks":checks,
        "body_native_assertions":native_checks,
        "buoyancy_measured_n":force,
        "buoyancy_expected_n":reference,
        "buoyancy_relative_error":relative_force_error,
        "native_stdout":test.stdout.strip(),
        "native_stderr":test.stderr.strip(),
    }
    EVIDENCE.parent.mkdir(parents=True,exist_ok=True)
    EVIDENCE.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print("P3_ACCEPTED="+str(report["accepted"]))
    print("P3_CHECKS="+str(len(checks)))
    print("P3_NATIVE_ASSERTIONS="+str(native_checks))
    print("P3_BUOYANCY_RELATIVE_ERROR="+str(relative_force_error))
    print("P3_INTEGRATED_16_CUBED_MS="+integrated[-1]["elapsed_ms"])
    print("P3_REPORT="+str(EVIDENCE))
    if not report["accepted"]:
        raise SystemExit("P3 gates failed: "+
                         ", ".join(c["name"] for c in checks if not c["pass"]))
if __name__=="__main__":main()
