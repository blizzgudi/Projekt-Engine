"""P4 evidence: validated Fourier 3D conduction and water enthalpy phase model.
A positive result is a PARTIAL milestone, NOT completion of P4.
"""
from __future__ import annotations
import csv,hashlib,io,json,math,re,subprocess
from datetime import datetime,timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
PROJECT=CORE.parents[1]
REPORT=PROJECT/"ENGINE TEST STUBE"/"runs"/"p4_partial_acceptance.json"
SOURCES=[
    "include/upc/thermal3d.hpp",
    "tests/thermal3d_tests.cpp",
    "benchmarks/p4_reference.cpp",
    "docs/P4_THERMAL_PARTIAL_ACCEPTANCE.md",
    "scripts/p4_partial_acceptance.py",
    "CMakeLists.txt",
]
def native(name):
    result=subprocess.run([str(CORE/"build"/name)],cwd=str(CORE),
                          capture_output=True,text=True,timeout=90)
    if result.returncode:
        raise RuntimeError(name+" failed: "+result.stderr[-1500:])
    return result
def main():
    test=native("upc_thermal3d_tests.exe")
    reference=native("upc_p4_reference.exe")
    match=re.search(r"PASS: (\d+) 3D heat conduction/latent-phase/snapshot assertions",
                    test.stdout)
    assertions=int(match.group(1)) if match else 0
    rows=list(csv.DictReader(io.StringIO(reference.stdout)))
    if len(rows)!=4:raise RuntimeError("Four Neumann 3D reference grids required")
    checks=[]
    def gate(name,ok,value=None,limit=None):
        checks.append({"name":name,"pass":bool(ok),"value":value,"limit":limit})
    gate("thermal_native_assertions",assertions>=800,assertions,800)
    for row in rows:
        n=int(row["nx"])
        gate("three_dimensional_grid_"+str(n),
             n in (8,12,16,24) and int(row["ny"])==n and int(row["nz"])==n,
             n,"8/12/16/24 cubed")
        def number(key):
            val=float(row[key])
            if not math.isfinite(val):raise RuntimeError("Non-finite "+key)
            return val
        err=number("max_profile_error_k")
        gate("neumann_fourier_profile_"+str(n),err<0.05,err,0.05)
        relative=number("relative_energy_residual")
        gate("insulated_energy_balance_"+str(n),relative<1e-10,relative,1e-10)
        amplitude_err=abs(number("expected_amplitude_k")-
                          number("observed_amplitude_k"))
        gate("thermal_mode_decay_"+str(n),amplitude_err<0.05,
             amplitude_err,0.05)
        elapsed=number("elapsed_ms")
        gate("thermal_cpu_timing_"+str(n),0<elapsed<10000,elapsed,10000)
    hashes={s:hashlib.sha256((CORE/s).read_bytes()).hexdigest() for s in SOURCES}
    gate("p4_partial_sources_present",len(hashes)==len(SOURCES),len(hashes),len(SOURCES))
    accepted=all(c["pass"] for c in checks)
    result={
        "phase":"P4",
        "subscope":"3D insulated Fourier conduction, water enthalpy phase plateaus, static MAC/VoxelGrid thermal snapshot",
        "partial_accepted":accepted,
        "phase_complete":False,
        "missing":"Live thermal advection, phase-specific MAC/solid/gas volume remap, full energy/mass coupling including P3 soil, thermal expansion",
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":assertions,
        "reference":rows,
        "checks":checks,
        "source_sha256":hashes,
        "native_stdout":test.stdout.strip(),
    }
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
    print("P4_PARTIAL_ACCEPTED="+str(accepted))
    print("P4_PHASE_COMPLETE=False")
    print("P4_CHECKS="+str(len(checks)))
    print("P4_NATIVE_ASSERTIONS="+str(assertions))
    print("P4_REPORT="+str(REPORT))
    if not accepted:raise SystemExit("P4 partial evidence validation failed")
if __name__=="__main__":main()
