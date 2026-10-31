"""Strict P2 transverse-shear PLIC partial acceptance; NEVER a full-phase gate."""
from __future__ import annotations
import csv
import hashlib
import io
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
EXE=CORE/"build"/"upc_p2_plic_sheared_periodic_tests.exe"
BUILD=RUNS/"p2_plic_sheared_build_20261031.txt"
CTEST=RUNS/"p2_plic_sheared_ctest_20261031.txt"
OUT=RUNS/"p2_plic_sheared_partial.json"
SOURCES=[
    "include/upc/p2_plic_sheared_periodic3d.hpp",
    "include/upc/p2_plic_periodic_sweep3d.hpp",
    "include/upc/p2_vof_normal_reconstruction3d.hpp",
    "tests/p2_plic_sheared_periodic3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_plic_sheared_build.cmd",
    "scripts/p2_plic_sheared_acceptance.py",
    "docs/P2_PLIC_SHEARED_PERIODIC_PARTIAL.md",
    "docs/PHASENPLAN.md",
]
def sha(p:Path)->str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main()->None:
    run=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,
                       text=True,timeout=120)
    checks=[]
    def add(name,value,ok):
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",run.returncode,run.returncode==0)
    m=re.search(r"PASS: PLIC sheared periodic assertions=(\d+) phase_complete=false",
                run.stdout)
    assertions=int(m.group(1)) if m else 0
    add("native_assertions",assertions,assertions>=16000)
    ctest=CTEST.read_text(encoding="utf-8",errors="replace")
    add("all_102_ctests","100% tests passed" in ctest and
        "0 tests failed out of 102" in ctest,
        "100% tests passed" in ctest and
        "0 tests failed out of 102" in ctest)
    add("new_ctest_registered",
        "physics_p2_plic_sheared_periodic_partial" in ctest,
        "physics_p2_plic_sheared_periodic_partial" in ctest)
    add("build_log_nonempty",BUILD.stat().st_size,BUILD.stat().st_size>0)
    lines=[line for line in run.stdout.splitlines()
           if line.startswith(("case,","shear,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("six_3d_shear_cases",len(rows),len(rows)==6)
    references=[]
    for axis in range(3):
        for dims in ((8,8,8),(12,8,6)):
            match=[r for r in rows if r["case"]=="shear"
                   and int(r["axis"])==axis
                   and tuple(int(r[k]) for k in ("nx","ny","nz"))==dims]
            add(f"axis_{axis}_dims_{dims}_present",len(match),len(match)==1)
            if not match:continue
            row=match[0]
            initial=float(row["initial_volume"])
            final=float(row["final_volume"])
            expected=3*math.prod(dims)/dims[axis]*.001
            error=float(row["max_error"])
            recon=float(row["reconstruction_error"])
            courant=float(row["max_courant"])
            positive=int(row["positive_faces"])
            negative=int(row["negative_faces"])
            prefix=f"axis_{axis}_dims_{dims}"
            add(prefix+"_initial",initial,abs(initial-expected)<1e-12)
            add(prefix+"_final",final,abs(final-expected)<1e-12)
            add(prefix+"_analytic_error",error,error<4e-12)
            add(prefix+"_cfl",courant,abs(courant-.15)<1e-13)
            add(prefix+"_signed_fluxes",(positive,negative),
                positive>0 and negative>0)
            add(prefix+"_reconstruction_limit",recon,
                0<=recon<.06)
            references.append({"axis":axis,"dims":dims,
                               "initial_volume_m3":initial,
                               "final_volume_m3":final,
                               "one_step_analytic_error":error,
                               "reconstructed_two_step_error":recon,
                               "max_courant":courant,
                               "positive_faces":positive,
                               "negative_faces":negative})
    add("nonexact_reconstructed_normal_visible",
        max((r["reconstructed_two_step_error"] for r in references),
            default=0),
        any(r["reconstructed_two_step_error"]>.05 for r in references))
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("source_hashes",len(hashes),len(hashes)==9)
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2",
        "milestone":"divergence_free_transverse_shear_periodic_plic_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "checks":checks,"native_assertions":assertions,
        "reference":references,
        "source_sha256":hashes,
        "native_binary_sha256":sha(EXE),
        "build_log_sha256":sha(BUILD),
        "ctest_log_sha256":sha(CTEST),
        "limitations":"One-axis divergence-free periodic lanes only; reconstructed normals are approximate; not arbitrary 3D MAC/VOF, FSI, physical energy conservation, independent CFD data or Unreal Native E2E.",
    }
    OUT.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"P2_PLIC_SHEARED_PARTIAL={accepted} CHECKS={sum(x['pass'] for x in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]:print("FAIL",c)
        raise SystemExit(1)
if __name__=="__main__":
    main()
