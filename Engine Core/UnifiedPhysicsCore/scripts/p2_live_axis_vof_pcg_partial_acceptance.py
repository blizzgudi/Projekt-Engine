"""Independent partial QA for LIVE axis-aligned VOF -> fractional 3D MAC-PCG.
Manufactured hydrostatics and negative MARIN data; never a full P2 gate.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
NATIVE=RUNS/"p2_live_axis_vof_pcg_native.txt"
CTEST=RUNS/"p2_live_axis_vof_pcg_ctest.txt"
BUILD=RUNS/"p2_live_axis_vof_pcg_full_build.txt"
GATE=RUNS/"p2_live_axis_vof_pcg_phase_gate.txt"
MEASURED=RUNS/"p2_marin_pcg_6s_partial.json"
BINARY=CORE/"build"/"upc_p2_live_axis_vof_pcg_tests.exe"
REPORT=RUNS/"p2_live_axis_vof_pcg_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "include/upc/coupled_water3d.hpp",
    "include/upc/mac3d.hpp",
    "include/upc/p2_axis_vof_pcg_geometry3d.hpp",
    "include/upc/p2_vof_normal_reconstruction3d.hpp",
    "tests/p2_live_axis_vof_pcg3d_tests.cpp",
    "scripts/p2_live_axis_vof_pcg_partial_acceptance.py",
    "docs/P2_LIVE_AXIS_VOF_PCG_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
PATTERN=re.compile(
    r"^LIVE_AXIS n=(5|9) mass_kg=([0-9.eE+\-]+) "
    r"pressure_fractional_pa=([0-9.eE+\-]+) "
    r"pressure_legacy_pa=([0-9.eE+\-]+) "
    r"faces=(\d+) divergence=([0-9.eE+\-]+)$"
)
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path:Path)->str:
    b=path.read_bytes()
    return b.decode("utf-16" if b.startswith(b"\xff\xfe") else "utf-8-sig").replace("\r\n","\n")
def verify_native(s:str)->list[dict]:
    rows=[]
    for line in s.splitlines():
        if line.startswith("LIVE_AXIS"):
            m=PATTERN.fullmatch(line)
            if not m:raise ValueError("Malformed native live case")
            n=int(m[1])
            mass,frac,legacy,div=map(float,(m[2],m[3],m[4],m[6]))
            faces=int(m[5])
            if not all(map(math.isfinite,(mass,frac,legacy,div))):
                raise ValueError("Nonfinite native metric")
            if (abs(mass-1000.0*n*n*1.75)>1e-7 or
                abs(frac-2452.5)>1e-5 or abs(legacy-4905.0)>1e-5 or
                faces!=n*n or div<0 or div>1e-8):
                raise ValueError("Manufactured hydrostatic/face/divergence mismatch")
            rows.append(dict(grid=[n,n,n],water_mass_kg=mass,
                             fractional_pressure_pa=frac,
                             legacy_pressure_pa=legacy,
                             reconstructed_faces=faces,divergence_s_inv=div))
    if len(rows)!=2 or {r["grid"][0] for r in rows}!={5,9}:
        raise ValueError("Both independent live grids required")
    if not re.search(r"^PASS P2_LIVE_AXIS_VOF_PCG assertions=(\d+) phase_complete=false$",s,re.M):
        raise ValueError("Native fail-closed/rollback assertion marker missing")
    count=int(re.search(r"^PASS P2_LIVE_AXIS_VOF_PCG assertions=(\d+)",s,re.M)[1])
    if count<43:raise ValueError("Native rollback/negative coverage incomplete")
    return rows
def main()->None:
    native=read(NATIVE)
    rows=verify_native(native)
    ctest=read(CTEST)
    if not re.search(r"100% tests passed, 0 tests failed out of 122",ctest):
        raise ValueError("Full native CTest not green")
    if "physics_p2_live_axis_vof_pcg_partial" not in ctest:
        raise ValueError("New native suite not included in full CTest")
    build=read(BUILD)
    if "Linking CXX executable upc_p2_live_axis_vof_pcg_tests.exe" not in build:
        raise ValueError("Full native build evidence missing")
    gate=read(GATE)
    if not ("ALL_COMPLETE=False" in gate and "P2" in gate.split("GATES_INCOMPLETE=")[1].splitlines()[0]):
        raise ValueError("P2 must remain INCOMPLETE in official phase gate")
    historic=json.loads(MEASURED.read_text(encoding="utf-8"))
    if (historic["phase"]!="P2" or historic["grid"]["duration_s"]<6 or
        historic["height_rmse_threshold_met"] is not False or
        historic["phase_complete"] is not False):
        raise ValueError("Historical 6s MARIN negative evidence missing/altered")
    heights={k:historic["metrics"][k]["rmse"] for k in ("H1_m","H2_m","H3_m","H4_m")}
    if any(not math.isfinite(v) or v<=.05 for v in heights.values()):
        raise ValueError("Negative measured height evidence must remain explicit")
    mutations=[
        native.replace("pressure_fractional_pa=2452.5","pressure_fractional_pa=9999",1),
        native.replace("faces=25","faces=24",1),
        native.replace("phase_complete=false","phase_complete=true",1),
        native.replace("LIVE_AXIS n=9","LIVE_AXIS n=7",1),
    ]
    for idx,bad in enumerate(mutations):
        try:verify_native(bad)
        except ValueError:continue
        raise ValueError(f"Mutation {idx} incorrectly accepted")
    report={
        "phase":"P2",
        "milestone":"live_axis_vof_fractional_mac_pcg_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "measured_3d_marin_validated":False,
        "stress_consistent_air_water":False,
        "bidirectional_moving_solid_fsi":False,
        "global_physical_impulse_energy_closed":False,
        "native_ctest_suites":122,
        "native_assertions":43,
        "independent_negative_mutations_rejected":len(mutations),
        "manufactured_live_hydrostatic":rows,
        "historical_6s_marin_height_rmse_m":heights,
        "historical_6s_marin_grid":historic["grid"],
        "historical_6s_marin_not_rerun":True,
        "source_sha256":{p:sha(CORE/p) for p in SOURCES},
        "artifact_sha256":{p.name:sha(p) for p in (NATIVE,CTEST,BUILD,GATE,MEASURED,BINARY)},
        "limitations":[
            "Only planar axis-aligned VOF geometry with f>0.5 wet mask",
            "Ambiguous/oblique/two-partial interface is rejected transactionally",
            "No full 6s three-grid MARIN convergence or resolved pressure sensor footprints",
            "No stress-consistent two-phase air, dynamic FSI or physical energy/impulse closure",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(f"PARTIAL_QA_PASS grids={len(rows)} ctest=122 native=43 mutations=4")
    print("P2_FULL_ACCEPTANCE=INCOMPLETE")
if __name__=="__main__":
    main()
