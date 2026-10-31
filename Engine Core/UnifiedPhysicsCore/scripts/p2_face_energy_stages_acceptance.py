"""P2 partial: live 3D MAC face-square stage audit, not physical energy closure."""
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
EXE=CORE/"build"/"upc_p2_face_energy_stages_tests.exe"
LOG=RUNS/"p2_face_energy_stages_ctest_20261031.txt"
OUT=RUNS/"p2_face_energy_stages_partial.json"
SOURCES=[
    "include/upc/coupled_water3d.hpp",
    "include/upc/mac3d.hpp",
    "tests/p2_face_energy_stages3d_tests.cpp",
    "CMakeLists.txt",
    "scripts/p2_face_energy_stages_full_build.cmd",
    "scripts/p2_face_energy_stages_acceptance.py",
    "docs/P2_FACE_ENERGY_STAGES_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
]
def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main()->None:
    result=subprocess.run([str(EXE)],cwd=CORE,capture_output=True,text=True,timeout=90,check=False)
    raw=LOG.read_bytes()
    log=raw.decode("utf-16" if raw.startswith((b"\xff\xfe",b"\xfe\xff")) else "utf-8",errors="replace")
    checks=[]
    def add(name:str,value:object,ok:bool)->None:
        checks.append({"name":name,"value":value,"pass":bool(ok)})
    add("native_exit",result.returncode,result.returncode==0)
    m=re.search(r"PASS: face stage energy assertions=(\d+)",result.stdout)
    assertions=int(m.group(1)) if m else 0
    add("native_assertions",assertions,assertions>=4500)
    add("msvc_93_ctest","100% tests passed, 0 tests failed out of 93" in log,
        "100% tests passed, 0 tests failed out of 93" in log)
    lines=[line for line in result.stdout.splitlines() if line.startswith(("mode,","case,"))]
    rows=list(csv.DictReader(io.StringIO("\n".join(lines))))
    add("two_3d_grids",len(rows),len(rows)==2)
    refs=[]
    for row in rows:
        n=int(row["n"])
        analytic=float(row["analytic_gravity"])
        measured=float(row["measured_gravity"])
        projection=float(row["projection"])
        post=float(row["post_projection"])
        final=float(row["final"])
        closure=float(row["closure"])
        divergence=float(row["divergence"])
        substeps=int(row["substeps"])
        independent=0.5*1000*(0.1**3)*3*(9.81*0.01)**2
        add(f"{n}_grid",n,n in (8,12))
        add(f"{n}_independent_three_vertical_faces",measured,
            math.isfinite(measured) and abs(measured-independent)<1e-12)
        add(f"{n}_reported_reference",analytic,abs(analytic-independent)<1e-12)
        add(f"{n}_projection_stage",projection,abs(projection)<1e-12)
        add(f"{n}_post_projection_energy_loss",post,post<0)
        add(f"{n}_independent_final",final,
            math.isfinite(final) and abs(final-(independent+post+projection))<1e-11)
        add(f"{n}_closure",closure,abs(closure)<1e-11)
        add(f"{n}_divergence",divergence,0<=divergence<=1e-8)
        add(f"{n}_single_substep",substeps,substeps==1)
        refs.append({"grid":n,"independent_gravity_face_energy_j":independent,
                     "measured_gravity_face_energy_j":measured,
                     "pressure_projection_delta_j":projection,
                     "post_transport_projection_delta_j":post,
                     "final_face_energy_j":final,"closure_j":closure,
                     "wet_divergence_s_inv":divergence})
    hashes={name:sha(CORE/name) for name in SOURCES}
    add("nine_source_hashes",len(hashes),len(hashes)==9)
    accepted=bool(checks) and all(c["pass"] for c in checks)
    report={"phase":"P2","milestone":"live_mac_vof_face_energy_stage_budget_partial",
            "partial_accepted":accepted,"phase_complete":False,
            "generated_utc":datetime.now(timezone.utc).isoformat(),
            "checks":checks,"native_assertions":assertions,"reference":refs,
            "source_sha256":hashes,"native_binary_sha256":sha(EXE),
            "ctest_log_sha256":sha(LOG),
            "limitations":"All-face square proxy with nominal cell mass, not VOF-weighted physical kinetic energy. Stage closure is telescoping bookkeeping, not physical conservation. No solid-body reaction, heat, independent CFD measurements, or new Unreal E2E."}
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(f"P2_FACE_STAGES_PARTIAL={accepted} CHECKS={sum(c['pass'] for c in checks)}/{len(checks)} NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        for c in checks:
            if not c["pass"]: print("FAIL",c)
        raise SystemExit(1)
if __name__=="__main__":
    main()
