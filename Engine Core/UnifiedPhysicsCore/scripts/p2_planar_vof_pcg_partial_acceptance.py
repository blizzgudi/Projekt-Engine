r"""P2 PARTIAL: native 3D oblique PLIC-VOF-to-MAC-PCG geometry acceptance.

This checks reproducibility, actual analytic/native/live tests, negative QA,
and SHA256; it never pretends to be full 3D two-phase/FSI CFD validation.
All outputs remain in D:\Projekt Engine.
"""
from __future__ import annotations

import hashlib
import json
import math
import os
import re
import subprocess
import sys
from datetime import datetime,timezone
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
BINARY=CORE/"build"/"upc_p2_planar_vof_pcg_tests.exe"
CTEST=RUNS/"p2_planar_vof_pcg_ctest.txt"
BUILD=RUNS/"p2_planar_vof_pcg_build.txt"
LOG=RUNS/"p2_planar_vof_pcg_native.txt"
SOURCE=[
    "CMakeLists.txt",
    "include/upc/p2_planar_vof_pcg_geometry3d.hpp",
    "include/upc/p2_axis_vof_pcg_geometry3d.hpp",
    "include/upc/p2_oblique_plic_geometry3d.hpp",
    "include/upc/coupled_water3d.hpp",
    "tests/p2_planar_vof_pcg_geometry3d_tests.cpp",
    "scripts/p2_planar_vof_pcg_partial_acceptance.py",
    "scripts/p2_planar_vof_pcg_build.cmd",
    "docs/P2_PLANAR_VOF_PCG_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def valid_log(s:str)->tuple[int,int]:
    rows=s.splitlines()
    ending=re.fullmatch(
        r"PASS P2_PLANAR_VOF_PCG assertions=(\d+) phase_complete=false",
        rows[-1] if rows else "")
    if not ending:
        raise ValueError("Native PASS/assertions/full-scope marker invalid")
    assertions=int(ending.group(1))
    if assertions<1099:
        raise ValueError("Missing independent 3D geometrical/rollback tests")
    manufactured=[x for x in rows if x.startswith("OBLIQUE_3D ")]
    if len(manufactured)!=4:
        raise ValueError("All 4 independent manufactured 3D pressure cuts required")
    verified=[]
    for x in manufactured:
        matches=re.fullmatch(
            r"OBLIQUE_3D n=(8|12) cut=([0-9.]+) faces=(\d+) "
            r"two_partials=(\d+) ghost_div=([0-9.eE+\-]+)",x)
        if not matches:
            raise ValueError("Malformed 3D cut reference")
        n=int(matches.group(1))
        faces=int(matches.group(3))
        two=int(matches.group(4))
        divergence=float(matches.group(5))
        if faces<63 or two!=faces or not math.isfinite(divergence) or divergence>1e-7:
            raise ValueError("Oblique 3D pressure geometry/PCG invalid")
        verified.append(n)
    if sorted(verified)!=[8,8,12,12]:
        raise ValueError("Missing grid variants")
    live=[row for row in rows if row.startswith("OBLIQUE_LIVE ")]
    if len(live)!=1:
        raise ValueError("Real opt-in coupled solver not executed")
    m=re.fullmatch(
        r"OBLIQUE_LIVE water_mass_kg=([0-9.]+) resolved=(\d+) "
        r"oblique=(\d+) divergence=([0-9.eE+\-]+) rollback=PASS",
        live[0])
    if not m:
        raise ValueError("Real oblique 3D MAC/VOF live regression incomplete")
    mass,resolved,oblique,div=(float(m.group(1)),int(m.group(2)),
                               int(m.group(3)),float(m.group(4)))
    if not(150<mass<155) or resolved!=96 or oblique!=96 or (
       not math.isfinite(div) or div>1e-7):
        raise ValueError("Native live oblique VOF continuity test mismatched")
    return assertions,len(manufactured)

def main():
    RUNS.mkdir(exist_ok=True)
    if sys.argv[1:] not in ([],["--rebuild-and-rerun"]):
        raise SystemExit("Only --rebuild-and-rerun is supported")
    if sys.argv[1:]:
        result=subprocess.run(
            ["cmd.exe","/d","/c",str(CORE/"scripts"/"p2_planar_vof_pcg_build.cmd")],
            cwd=CORE,text=True,capture_output=True,timeout=240)
        if result.returncode:
            raise RuntimeError("MSVC Native CTest build failed: "+
                               (result.stdout+result.stderr)[-2000:])
    if not BINARY.is_file() or not CTEST.is_file() or not BUILD.is_file():
        raise RuntimeError("Missing real native binary, build or CTest evidence")
    if BINARY.stat().st_mtime_ns<max(
        (CORE/x).stat().st_mtime_ns for x in (
            "include/upc/p2_planar_vof_pcg_geometry3d.hpp",
            "include/upc/coupled_water3d.hpp",
            "tests/p2_planar_vof_pcg_geometry3d_tests.cpp")):
        raise RuntimeError("Stale compiled binary; rebuild required")
    env=os.environ.copy()
    tmp=CORE/"build"/"p2_planar_temp"
    tmp.mkdir(exist_ok=True)
    env.update({"TMP":str(tmp),"TEMP":str(tmp)})
    exe=subprocess.run([str(BINARY)],cwd=CORE,env=env,text=True,
                       capture_output=True,timeout=90)
    if exe.returncode or exe.stderr.strip():
        raise RuntimeError("Native 3D planar pressure implementation failed")
    output=exe.stdout
    assertions,references=valid_log(output)
    LOG.write_text(output,encoding="utf-8",newline="\n")
    logtext=CTEST.read_bytes()
    ctest=logtext.decode("utf-16" if logtext.startswith(bytes((255,254)))
                         else "utf-8-sig",errors="replace")
    match=re.search(r"100% tests passed, 0 tests failed out of (\d+)",ctest)
    suite=int(match.group(1)) if match else 0
    mutations={
        "false_full_phase_pass":output.replace(
            "phase_complete=false","phase_complete=true"),
        "missing_native_success":output.replace(
            "PASS P2_PLANAR_VOF_PCG","FAIL P2_PLANAR_VOF_PCG"),
        "missing_one_3d_reference":"\n".join(
            row for row in output.splitlines()
            if not row.startswith("OBLIQUE_3D n=8 "))+"\n",
        "destroyed_3d_pressure_convergence":output.replace(
            "ghost_div=3.45003e-12","ghost_div=nan"),
        "missing_live_transactional_rollback":output.replace(
            "rollback=PASS","rollback=FAIL"),
        "deleted_oblique_live_geometry":output.replace(
            "oblique=96","oblique=0"),
    }
    rejected={}
    for label,changed in mutations.items():
        if changed==output:raise RuntimeError("Mutation not applied: "+label)
        try:valid_log(changed)
        except (ValueError,RuntimeError):rejected[label]=True
        else:raise RuntimeError("Dangerous forged native evidence accepted: "+label)
    checks={
        "real_native_executable_verified":assertions>=1099,
        "four_analytical_oblique_3D_cases":references==4,
        "real_optin_coupled_mac_pcg_live_geometry_and_rollback":(
            "OBLIQUE_LIVE" in output and "rollback=PASS" in output),
        "full_native_ctest_at_least_124":(
            suite>=124 and "physics_p2_planar_vof_pcg_partial" in ctest),
        "six_independent_corruptions_rejected":len(rejected)==6,
        "original_axis_geometry_still_in_regression":(
            "physics_p2_axis_vof_pcg_geometry_partial" in ctest and
            "physics_p2_live_axis_vof_pcg_partial" in ctest),
        "source_files_present_and_fingerprintable":all(
            (CORE/x).is_file() for x in SOURCE),
    }
    accepted=all(checks.values())
    evidence={
        "phase":"P2",
        "milestone":"volume_matched_oblique_planar_vof_pcg_3D_live_and_transactional",
        "partial_accepted":accepted,
        "phase_complete":False,
        "physical_stress_consistency_validated":False,
        "external_experimental_cfd_validated":False,
        "automatically_reconstructed_normals_validated":False,
        "full_bidirectional_dynamic_fsi_validated":False,
        "native_assertions":assertions,
        "native_ctest_suites":suite,
        "checks":checks,
        "negative_mutations":rejected,
        "source_sha256":{x:sha(CORE/x) for x in SOURCE},
        "artifact_sha256":{
            "native_executable":sha(BINARY),
            "native_test_log":sha(LOG),
            "ctest":sha(CTEST),
            "msvc_build":sha(BUILD),
        },
        "open_full_phase_acceptance":[
            "Reconstruct physically accurate oblique normals from dynamic VOF, not prescribed field",
            "Validated stress-consistent moving water/air traction and surface tension",
            "Real independent 6-second MARIN H1-H4 and P1-P8 thresholds plus 3-grid convergence",
            "Moving-solid true two-way FSI with physical global impulse and energy closure",
        ],
        "generated_utc":datetime.now(timezone.utc).isoformat(),
    }
    out=RUNS/"p2_planar_vof_pcg_partial.json"
    out.write_text(json.dumps(evidence,ensure_ascii=False,indent=2)+"\n",
                   encoding="utf-8")
    print(f"P2_PLANAR_VOF_PCG_PARTIAL={accepted} ASSERTIONS={assertions} "
          f"CTEST={suite} MUTATIONS_REJECTED={len(rejected)} "
          "P2_FULL_PHASE=False")
    if not accepted:raise SystemExit(1)

if __name__=="__main__":main()
