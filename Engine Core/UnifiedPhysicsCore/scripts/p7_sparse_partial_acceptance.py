"""P7 partial evidence: native CPU sparse LOD of real MAC+VOF, not GPU acceptance."""
from __future__ import annotations
import csv,hashlib,io,json,math,re,subprocess
from datetime import datetime,timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
LAB=CORE.parents[1]/"ENGINE TEST STUBE"
OUT=LAB/"runs"/"p7_sparse_partial.json"
FILES=[
    "CMakeLists.txt",
    "include/upc/coupled_water3d.hpp",
    "include/upc/mac3d.hpp",
    "include/upc/sparse_water_lod3d.hpp",
    "tests/sparse_water_lod3d_tests.cpp",
    "benchmarks/p7_sparse_reference.cpp",
    "docs/P7_SPARSE_LOD_PARTIAL.md",
    "scripts/p7_sparse_partial_acceptance.py",
]
def execute(name):
    p=subprocess.run([str(CORE/"build"/name)],cwd=str(CORE),
                     capture_output=True,text=True,timeout=90)
    if p.returncode:raise RuntimeError(name+": "+p.stderr[-2000:])
    return p.stdout.strip()
def main():
    native=execute("upc_p7_sparse_lod_tests.exe")
    m=re.search(r"PASS: P7 conservative 3D MAC sparse render LOD; checks=(\d+)",native)
    assertions=int(m.group(1)) if m else 0
    benchmark=execute("upc_p7_sparse_reference.exe")
    rows=list(csv.DictReader(io.StringIO(benchmark)))
    checks=[]
    def gate(name,ok,value=None,target=None):
        checks.append({"name":name,"pass":bool(ok),"value":value,"target":target})
    gate("native_MAC_LOD_assertions",assertions>=230,assertions,230)
    gate("three_sparse_3D_reference_grids",len(rows)==3,len(rows),3)
    for row in rows:
        n=int(row["side"])
        gate(f"grid_{n}",row["scenario"]=="sparse_3d_mac_snapshot" and n in (32,48,64),n,"32,48,64")
        volume=float(row["source_volume_m3"])
        for key in ("finest_volume_m3","coarsest_volume_m3"):
            observed=float(row[key])
            gate(f"volume_{n}_{key}",math.isfinite(observed) and
                 abs(observed-volume)<1e-12,observed,volume)
        payload=int(row["brick_payload_bytes"])
        dense=int(row["dense_payload_bytes"])
        gate(f"sparse_payload_less_half_dense_{n}",0<payload<dense/2,
             payload,dense/2)
        gate(f"nonempty_bricks_{n}",int(row["active_bricks"])>0,
             int(row["active_bricks"]),">0")
        elapsed=float(row["build_ms"])
        gate(f"finite_cpu_build_ms_{n}",math.isfinite(elapsed) and
             0<elapsed<10000,elapsed,"0<ms<10000")
        gate(f"multi_resolution_{n}",int(row["levels"])>=6,
             int(row["levels"]),">=6")
    hashes={name:hashlib.sha256((CORE/name).read_bytes()).hexdigest() for name in FILES}
    accepted=all(c["pass"] for c in checks)
    data={
        "phase":"P7","partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "scope":"CPU sparse 8x8x8 brick snapshots of native P2 3D MAC+VOF water, conservative edge-weighted LOD and memory/latency reference",
        "excluded":"GPU kernel, GPU/CPU parity, GPU transfer and memory budgets, live LOD streaming and full P7 acceptance",
        "native_assertions":assertions,"native_output":native,
        "checks":checks,"reference":rows,"source_sha256":hashes,
    }
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding="utf-8")
    print("P7_SPARSE_PARTIAL_ACCEPTED="+str(accepted))
    print("P7_PHASE_COMPLETE=False")
    print("P7_CHECKS="+str(len(checks)))
    print("P7_NATIVE_ASSERTIONS="+str(assertions))
    print("P7_REPORT="+str(OUT))
    if not accepted:raise SystemExit("P7 sparse partial gates failed")
if __name__=="__main__":main()
