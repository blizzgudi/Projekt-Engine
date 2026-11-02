"""Independent P2 partial QA for 3D VOF height-function geometry.

Manufactured column-wise alpha is NOT moving two-phase CFD or experimental
MARIN validation. Preserve P2 PARTIAL and explicit negative boundaries.
"""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
EXE=CORE/"build"/"upc_p2_vof_height_curvature_tests.exe"
NATIVE=RUNS/"p2_height_curvature_native_20261102.txt"
BUILD=RUNS/"p2_height_curvature_build_20261102.txt"
FAILED_BUILD=RUNS/"p2_height_curvature_initial_build_failures_20261102.txt"
FULL=RUNS/"p2_height_curvature_full_ctest_20261102.txt"
GATE=RUNS/"unified_physics_gate.json"
REPORT=RUNS/"p2_vof_height_curvature_partial.json"
SOURCES=[
    "CMakeLists.txt",
    "include/upc/p2_vof_height_curvature3d.hpp",
    "tests/p2_vof_height_curvature3d_tests.cpp",
    "docs/P2_VOF_HEIGHT_CURVATURE_PARTIAL.md",
    "scripts/p2_vof_height_curvature_acceptance.py",
    "scripts/phase_gate.py",
]
SPHERE=re.compile(
    r"HEIGHT_CURVATURE axis=(\d) grid=(\d+) dx_m=(\S+) "
    r"curvature_inv_m=(\S+) reference_inv_m=(\S+) "
    r"error_inv_m=(\S+) phase_complete=false"
)
PLANE=re.compile(
    r"HEIGHT_PLANE axis=(\d) grid=(\d+) curvature_inv_m=(\S+) "
    r"phase_complete=false"
)
PASS=re.compile(
    r"PASS P2_VOF_HEIGHT_CURVATURE assertions=(\d+) phase_complete=false"
)
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def read_log(p:Path)->str:
    raw=p.read_bytes()
    return raw.decode("utf-16" if raw.startswith((b"\xff\xfe",b"\xfe\xff")) else "utf-8-sig")

def number(v:str)->float:
    x=float(v)
    if not math.isfinite(x):
        raise ValueError("Nonfinite native curvature evidence")
    return x

def reference_curvature(dx:float)->float:
    # Independent centered finite-difference curvature of a sphere cap
    # h(u,v)=0.05+sqrt(1-u²-v²), at u=v=-dx/2.
    u=v=-dx/2
    def h(a:float,b:float)->float:
        return .05+math.sqrt(1-a*a-b*b)
    center=h(u,v)
    hu=(h(u+dx,v)-h(u-dx,v))/(2*dx)
    hv=(h(u,v+dx)-h(u,v-dx))/(2*dx)
    huu=(h(u+dx,v)-2*center+h(u-dx,v))/(dx*dx)
    hvv=(h(u,v+dx)-2*center+h(u,v-dx))/(dx*dx)
    huv=(h(u+dx,v+dx)-h(u+dx,v-dx)-
         h(u-dx,v+dx)+h(u-dx,v-dx))/(4*dx*dx)
    return -(huu*(1+hv*hv)+hvv*(1+hu*hu)-2*hu*hv*huv)/(
        (1+hu*hu+hv*hv)**1.5)

def parse(raw:str)->dict:
    lines=raw.strip().splitlines()
    if len(lines)!=13:
        raise ValueError("Missing manufactured 3D grids/planes or footer")
    rows={}
    for line in lines[:9]:
        m=SPHERE.fullmatch(line)
        if not m:
            raise ValueError("Malformed sphere row or forged phase")
        axis,n=int(m[1]),int(m[2])
        if axis not in (0,1,2) or n not in (12,24,48) or (axis,n) in rows:
            raise ValueError("Wrong or duplicate 3D sphere axis/grid")
        dx,k,analytic,error=map(number,m.group(3,4,5,6))
        expected=reference_curvature(1.2/n)
        if (abs(dx-1.2/n)>1e-12 or abs(analytic-2)>1e-12 or
            abs(k-expected)>1e-9 or
            abs(error-abs(k-2))>1e-11 or
            not 0<error<.007):
            raise ValueError("3D sphere curvature not independently reproduced")
        rows[axis,n]={"dx_m":dx,"curvature_inv_m":k,
                      "reference_inv_m":analytic,"abs_error_inv_m":error}
    if set(rows)!={(a,n) for a in range(3) for n in (12,24,48)}:
        raise ValueError("Missing 3D axis or resolution")
    planes={}
    for line in lines[9:12]:
        m=PLANE.fullmatch(line)
        if not m:
            raise ValueError("Malformed oblique planar reference")
        axis,n=int(m[1]),int(m[2])
        if axis not in (0,1,2) or axis in planes or n!=24:
            raise ValueError("Missing or duplicate oblique plane")
        k=number(m[3])
        if abs(k)>1e-10:
            raise ValueError("Planar free surface curvature not zero")
        planes[axis]=k
    if set(planes)!={0,1,2}:
        raise ValueError("Incomplete planar orientations")
    m=PASS.fullmatch(lines[12])
    if not m or int(m[1])<71:
        raise ValueError("Native assertion count or partial status invalid")
    for axis in range(3):
        if not (rows[axis,48]["abs_error_inv_m"]<
                rows[axis,24]["abs_error_inv_m"]/3<
                rows[axis,12]["abs_error_inv_m"]/9):
            raise ValueError("No 3-grid curvature convergence")
    return {"grids":{f"axis{a}_n{n}":rows[a,n]
                      for a in range(3) for n in (12,24,48)},
            "planes":{f"axis{a}":planes[a] for a in range(3)},
            "native_assertions":int(m[1]),
            "manufactured_curvature_converged":True}

def corrupt(raw:str)->dict[str,bool]:
    lines=raw.strip().splitlines()
    cases={
        "missing_axis": "\n".join(lines[1:])+"\n",
        "duplicate_grid": raw.replace("axis=1 grid=12","axis=0 grid=12",1),
        "fake_phase_pass": raw.replace("phase_complete=false","phase_complete=true",1),
        "wrong_analytic": re.sub(r"reference_inv_m=\S+",
                                 "reference_inv_m=1",raw,count=1),
        "false_curvature": re.sub(r"curvature_inv_m=\S+",
                                 "curvature_inv_m=2",raw,count=1),
        "nonfinite_error": re.sub(r"error_inv_m=\S+",
                                 "error_inv_m=nan",raw,count=1),
        "wrong_dx": re.sub(r"dx_m=\S+","dx_m=0.2",raw,count=1),
        "wrong_plane": raw.replace("HEIGHT_PLANE axis=1 grid=24",
                                   "HEIGHT_PLANE axis=1 grid=12",1),
        "fake_assertions": raw.replace("assertions=71","assertions=0",1),
        "negative_error": re.sub(r"error_inv_m=\S+",
                                 "error_inv_m=-1",raw,count=1),
    }
    result={}
    for name,mutant in cases.items():
        if mutant==raw:
            raise ValueError("Mutation did not change real native evidence: "+name)
        try:parse(mutant)
        except (ValueError,TypeError,KeyError):result[name]=True
        else:result[name]=False
    return result

def main()->None:
    for p in [EXE,NATIVE,BUILD,FAILED_BUILD,FULL,GATE]+[CORE/x for x in SOURCES]:
        if not p.is_file():
            raise FileNotFoundError("Missing native QA evidence "+str(p))
    raw=read_log(NATIVE)
    data=parse(raw)
    replay=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,text=True,
                          encoding="utf-8",errors="replace",timeout=90)
    if replay.returncode!=0 or replay.stdout.strip().splitlines()!=raw.strip().splitlines() or replay.stderr.strip():
        raise RuntimeError("Independent native rerun changed or failed")
    full=read_log(FULL)
    if "100% tests passed, 0 tests failed out of 148" not in full:
        raise RuntimeError("Full native 148/148 CTest not verified")
    gate=json.loads(GATE.read_text(encoding="utf-8-sig"))
    p2=gate["phases"]["P2"]
    if (p2["status"]!="PARTIAL" or
        not p2["full_acceptance_open"] or
        gate["all_phases_complete"] is not False or
        gate["ctest"]["ok"] is not True):
        raise RuntimeError("Official full P2 phase gate is not correctly incomplete")
    mutations=corrupt(raw)
    if not all(mutations.values()):
        raise RuntimeError("Independent QA accepted forged native evidence")
    report={
        "phase":"P2",
        "milestone":"native_3d_vof_height_function_geometry_partial",
        "partial_accepted":True,
        "phase_complete":False,
        "full_P2_accepted":False,
        "manufactured_geometry_verified":True,
        "moving_mac_vof_coupled":False,
        "dynamic_two_way_fsi_validated":False,
        "global_physical_energy_momentum_closed":False,
        "marin_spheric_6s_3grid_validated":False,
        "full_native_ctest_count":148,
        "native_rerun_identical":True,
        "negative_qa_mutations_rejected":mutations,
        "metrics":data,
        "source_sha256":{x:sha(CORE/x) for x in SOURCES},
        "binary_sha256":sha(EXE),
        "artifact_sha256":{p.name:sha(p) for p in
                           [NATIVE,BUILD,FAILED_BUILD,FULL,GATE]},
        "known_limitations":[
            "column-wise manufactured alpha, not true curved-cell VOF volume integration",
            "single-valued monotone height field only; no overhang or detached droplet",
            "not integrated into moving two-phase capillary stress or pressure projection",
            "no measured MARIN H1-H4/P1-P8 six-second three-grid validation",
            "no moving-solid FSI or closed global physical energy/impulse"],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    REPORT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",
                      encoding="utf-8")
    print("P2_HEIGHT_CURVATURE_QA=PASS_PARTIAL")
    print("NATIVE_ASSERTIONS="+str(data["native_assertions"]))
    print("TAMPER_REJECTED="+str(sum(mutations.values()))+"/"+str(len(mutations)))
    print("P2_FULL_ACCEPTED=False")
    print("REPORT_SHA256="+sha(REPORT))

if __name__=="__main__":
    main()
