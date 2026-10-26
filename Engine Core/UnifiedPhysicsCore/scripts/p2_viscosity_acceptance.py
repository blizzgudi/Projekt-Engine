"""Reproducible P2 optional 3D MAC viscosity partial evidence.

This does NOT close P2 and cannot be used as full-phase acceptance.
"""
from __future__ import annotations
import hashlib,json,math,re,subprocess
from datetime import datetime,timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
LAB=CORE.parents[1]/"ENGINE TEST STUBE"
OUT=LAB/"runs"/"p2_viscosity_partial.json"
FILES=[
 "CMakeLists.txt",
 "include/upc/mac3d.hpp",
 "include/upc/viscous_mac3d.hpp",
 "tests/viscous_mac3d_tests.cpp",
 "docs/P2_VISCOUS_MAC_PARTIAL.md",
 "scripts/p2_viscosity_acceptance.py",
]
def main():
    exe=CORE/"build"/"upc_p2_viscosity_tests.exe"
    r=subprocess.run([str(exe)],cwd=str(CORE),capture_output=True,
                     text=True,timeout=45)
    if r.returncode:
        raise SystemExit("Native viscosity test failed: "+r.stderr[-1000:])
    match=re.search(
      r"PASS: P2 optional 3D MAC viscosity; checks=(\d+) "
      r"analytic_eigenmode_max_error=([0-9.eE+-]+) "
      r"energy_before=([0-9.eE+-]+) "
      r"energy_after=([0-9.eE+-]+) "
      r"energy_61_steps=([0-9.eE+-]+) "
      r"adaptive_substeps=(\d+) "
      r"isotropic_face_value=([0-9.eE+-]+)",r.stdout)
    if not match:raise SystemExit("Native output missing structured P2 viscosity evidence")
    checks,err,e0,e1,e61,steps,axis=match.groups()
    checks,steps=int(checks),int(steps)
    err,e0,e1,e61,axis=map(float,(err,e0,e1,e61,axis))
    cases=[
      {"name":"native_assertions","value":checks,"limit":">=1174","pass":checks>=1174},
      {"name":"independent_discrete_3d_eigenmode","value":err,"limit":"<=1e-12",
       "pass":math.isfinite(err) and 0<=err<=1e-12},
      {"name":"first_step_energy_proxy_dissipation","value":[e0,e1],"limit":"0<after<before",
       "pass":0<e1<e0},
      {"name":"61_step_energy_proxy_dissipation","value":e61,"limit":"0<end<first",
       "pass":0<e61<e1},
      {"name":"adaptive_viscosity_CFL","value":steps,"limit":"100..200",
       "pass":100<=steps<=200},
      {"name":"isotropic_x_y_z_impulse","value":axis,"limit":"0<axis<1",
       "pass":0<axis<1},
      {"name":"full_source_coverage","value":len(FILES),"limit":">=6",
       "pass":all((CORE/p).is_file() for p in FILES)},
    ]
    fingerprints={p:hashlib.sha256((CORE/p).read_bytes()).hexdigest() for p in FILES}
    ok=all(x["pass"] for x in cases)
    payload={"phase":"P2","milestone":"viscous_mac3d_partial",
       "partial_accepted":ok,"phase_complete":False,
       "generated_utc":datetime.now(timezone.utc).isoformat(),
       "native_stdout":r.stdout.strip(),"checks":cases,"source_sha256":fingerprints,
       "limitations":"standalone 3D homogeneous MAC viscous diffusion, no CoupledMacWater3D integration, no multiphase/solid boundaries, no external CFD validation"}
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(payload,indent=2,ensure_ascii=False),encoding="utf-8")
    print("P2_VISCOUS_MAC_PARTIAL="+str(ok))
    print("P2_VISCOUS_MAC_ASSERTIONS="+str(checks))
    print("P2_VISCOUS_MAC_EIGENMODE_ERROR="+str(err))
    print("P2_VISCOUS_MAC_REPORT="+str(OUT))
    if not ok:raise SystemExit("P2 optional viscous MAC partial evidence FAIL")
if __name__=="__main__":main()
