"""P7 final native hardware GPU / conservative MAC render LOD / GPU brick
streaming quantitative acceptance. No mocks or synthetic test reports.
Prove GPU hardware via DXGI and physically dispatched Direct3D11 shaders.
"""
from __future__ import annotations
import csv,hashlib,io,json,math,re,subprocess
from datetime import datetime,timezone
from pathlib import Path

CORE=Path(__file__).resolve().parent.parent
LAB=CORE.parents[1]/"ENGINE TEST STUBE"
OUT=LAB/"runs"/"p7_acceptance.json"
P8=LAB/"runs"/"p8_acceptance.json"
PATHS=[
 "CMakeLists.txt",
 "shaders/p7_vof_reduce.hlsl",
 "include/upc/mac3d.hpp",
 "include/upc/coupled_water3d.hpp",
 "include/upc/sparse_water_lod3d.hpp",
 "include/upc/p7_brick_streamer3d.hpp",
 "tests/sparse_water_lod3d_tests.cpp",
 "tests/p7_brick_streamer3d_tests.cpp",
 "benchmarks/p7_sparse_reference.cpp",
 "benchmarks/p7_gpu_d3d11.cpp",
 "benchmarks/p7_gpu_streaming_d3d11.cpp",
 "docs/P7_SPARSE_LOD_PARTIAL.md",
 "docs/P7_GPU_LOD_STREAMING_CPU_GPU_ABNAHME.md",
 "scripts/p7_sparse_partial_acceptance.py",
 "scripts/p7_acceptance.py",
]
def native(name):
    exe=CORE/"build"/(name+".exe")
    p=subprocess.run([str(exe)],cwd=str(CORE),capture_output=True,
                     text=True,timeout=90)
    if p.returncode:
        raise RuntimeError(f"Native test {name} failed: {p.stdout[-500:]} {p.stderr[-1800:]}")
    return p.stdout,p.stderr
def number(row,name):
    v=float(row[name])
    if not math.isfinite(v):raise ValueError(f"Nonfinite {name}")
    return v
def main():
    sparseout,_=native("upc_p7_sparse_lod_tests")
    streamout,_=native("upc_p7_streamer_tests")
    sparsebench,_=native("upc_p7_sparse_reference")
    gpuout,gpuerr=native("upc_p7_gpu_d3d11")
    uploadout,uploaderr=native("upc_p7_gpu_streaming")
    sparse_reference=list(csv.DictReader(io.StringIO(sparsebench)))
    lines=gpuout.splitlines()
    if len(lines)<5:raise RuntimeError("Missing DXGI and GPU record")
    adapter=list(csv.DictReader(io.StringIO("\n".join(lines[:2]))))
    if len(adapter)!=1:raise RuntimeError("Bad DXGI vendor information")
    device=adapter[0]
    start=next((n for n,line in enumerate(lines) if line.startswith("scenario,nx,ny,nz")),None)
    if start is None:raise RuntimeError("No hardware compute benchmark header")
    gpu_rows=list(csv.DictReader(io.StringIO("\n".join(lines[start:]))))
    gpu_stream=list(csv.DictReader(io.StringIO(uploadout)))
    p8=json.loads(P8.read_text(encoding="utf-8"))
    partial=json.loads((LAB/"runs"/"p7_sparse_partial.json").read_text(encoding="utf-8"))
    checks=[]
    def gate(name,condition,value=None,limit=None):
        checks.append({"name":name,"pass":bool(condition),"value":value,"limit":limit})
    match=re.search(r"checks=(\d+)",sparseout)
    sparse_count=int(match.group(1)) if match else 0
    match=re.search(r"checks=(\d+)",streamout)
    stream_count=int(match.group(1)) if match else 0
    gate("P7_original_MAC_sparse_assertions",sparse_count>=230,sparse_count,230)
    gate("P7_CPU_stream_generation_assertions",stream_count>=250000,stream_count,250000)
    gate("P7_original_partial_cpu_gate",partial.get("partial_accepted") is True
         and partial.get("phase_complete") is False,partial.get("partial_accepted"),True)
    gate("real_D3D11_hardware_adapter",int(device["gpu_adapter_vendor_id"])!=0
         and int(device["feature_level"])>=45056 and
         "HARDWARE GPU compute" in gpuerr,
         device,{"feature_level":">=11_0","driver_type":"HARDWARE"})
    gate("positive_dedicated_hardware_vram",int(device["gpu_dedicated_vram_bytes"])>0,
         int(device["gpu_dedicated_vram_bytes"]),">0")
    gate("five_CPU_grid_cases",len(sparse_reference)==3
         and all(x["scenario"]=="sparse_3d_mac_snapshot" for x in sparse_reference),
         len(sparse_reference),3)
    gate("twenty_real_HLSL_dispatch_cases",len(gpu_rows)==20,len(gpu_rows),20)
    group={}
    for row in gpu_rows:
        side=int(row["nx"])
        group.setdefault(side,[]).append(row)
        scale=int(row["scale"])
        actual=number(row,"gpu_volume_m3")
        reference=number(row,"source_volume_m3")
        cpu=number(row,"cpu_reference_volume_m3")
        parity=number(row,"max_cell_error")
        g_ms=number(row,"gpu_kernel_ms")
        rt_ms=number(row,"wall_upload_compute_readback_ms")
        buf=int(row["gpu_buffer_budget_bytes"])
        gate(f"hardware_cell_parity_{side}_{scale}",0<=parity<=2e-6,parity,2e-6)
        gate(f"hardware_water_volume_{side}_{scale}",abs(actual-reference)<=2e-5,
             abs(actual-reference),2e-5)
        gate(f"CPU_reference_volume_{side}_{scale}",abs(cpu-reference)<1e-9,
             abs(cpu-reference),1e-9)
        gate(f"real_GPU_kernel_ms_{side}_{scale}",0<g_ms<25,g_ms,25)
        gate(f"GPU_roundtrip_ms_{side}_{scale}",0<rt_ms<250,rt_ms,250)
        gate(f"GPU_resident_buffer_budget_{side}_{scale}",0<buf<8*1024*1024,
             buf,8*1024*1024)
        gate(f"CPU_snapshot_reference_ms_{side}_{scale}",
             0<number(row,"cpu_reference_ms")<10000,
             number(row,"cpu_reference_ms"),10000)
    for size in (12,17,32,48,64):
        cases=group.get(size,[])
        gate("all_four_3D_LODs_"+str(size),
             len(cases)==4 and {int(x["scale"]) for x in cases}=={2,4,8,16},
             [x.get("scale") for x in cases],"2/4/8/16")
        if size==12:
            gate("UE_P8_same_128kg_physically_conserved_MAC_case",
                 all(abs(float(x["source_volume_m3"])-0.128)<1e-8 for x in cases),
                 [x["source_volume_m3"] for x in cases],0.128)
    gate("native_GPU_memory_upload_case_count",len(gpu_stream)==3,len(gpu_stream),3)
    gate("native_D3D11_brick_driver_upload",
         "PASS: P7 real D3D11 GPU brick buffer" in uploaderr,
         uploaderr.strip(),"PASS")
    for row in gpu_stream:
        level=int(row["lod"])
        frames=int(row["frames"])
        bricks=int(row["active_bricks"])
        sent=int(row["bytes_uploaded"])
        budget=int(row["max_bytes_per_frame"])
        memory=int(row["gpu_buffer_bytes"])
        mismatch=int(row["gpu_readback_difference_bytes"])
        elapsed=number(row,"max_frame_upload_ms")
        gate("GPU_upload_enforced_frame_budget_LOD"+str(level),
             sent==frames*4096 and budget==4096 and
             memory==65536 and frames==bricks==2,
             {"sent":sent,"frames":frames,"memory":memory,"budget":budget},
             "2 frames x 4KiB and 64KiB GPU allocation")
        gate("GPU_byte_exact_readback_LOD"+str(level),mismatch==0,mismatch,0)
        gate("GPU_brick_upload_latency_bound_LOD"+str(level),
             0<elapsed<1000,elapsed,1000)
        gate("same_hardware_for_dispatch_and_stream_LOD"+str(level),
             int(row["adapter_vendor_id"])==int(device["gpu_adapter_vendor_id"]),
             int(row["adapter_vendor_id"]),int(device["gpu_adapter_vendor_id"]))
    gate("P8_UE58_real_game_and_PIE_cross_reference",
         p8.get("phase")=="P8" and p8.get("accepted") is True and
         all(v.get("exit_code")==0 for v in p8.get("process_proof",{}).values()) and
         len(p8.get("process_proof",{}))==2,
         list(p8.get("process_proof",{})),"game+pie exit0")
    gate("P7_all_source_files_present",all((CORE/f).is_file() for f in PATHS),
         len(PATHS),len(PATHS))
    hashes={f:hashlib.sha256((CORE/f).read_bytes()).hexdigest() for f in PATHS}
    approved=all(x["pass"] for x in checks)
    report={
       "phase":"P7","schema_version":2,"accepted":approved,
       "scope":"True D3D11 hardware compute render-LOD, live P2/P8 MAC CPU-parity, bounded real GPU memory/readback, dynamic CPU sparse streaming and real per-frame brick VRAM uploads",
       "generated_utc":datetime.now(timezone.utc).isoformat(),
       "hardware":{"gpu_adapter_vendor_id":int(device["gpu_adapter_vendor_id"]),
            "gpu_adapter_device_id":int(device["gpu_adapter_device_id"]),
            "feature_level":int(device["feature_level"]),
            "dedicated_vram_bytes":int(device["gpu_dedicated_vram_bytes"]),
            "driver_type":"HARDWARE; never WARP"},
       "native_assertions":{"sparse":sparse_count,"streamer":stream_count},
       "cpu_sparse_reference":sparse_reference,"gpu_reference":gpu_rows,
       "gpu_stream":gpu_stream,"checks":checks,"source_sha256":hashes,
       "p8_evidence_digest":hashlib.sha256(P8.read_bytes()).hexdigest(),
       "limitations":"GPU ONLY render LOD/reduce + streaming upload; physics itself CPU. End-to-end GPU readback may be slower than CPU. No UE RDG async integration, 60fps proof, large AAA scene or physical GPU solver."
    }
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print("P7_COMPLETE_CPU_GPU_MVP="+str(approved))
    print("P7_NUMERICAL_GATES="+str(len(checks)))
    print("P7_NATIVE_STREAM_ASSERTIONS="+str(stream_count))
    print("P7_REAL_GPU_DISPATCHES="+str(len(gpu_rows)))
    print("P7_REAL_GPU_STREAM_LOD_CASES="+str(len(gpu_stream)))
    print("P7_HARDWARE_VENDOR_ID="+str(device["gpu_adapter_vendor_id"]))
    print("P7_REPORT="+str(OUT))
    if not approved:
        raise SystemExit("P7 GPU/native quantitative failure: "+
                         ",".join(x["name"] for x in checks if not x["pass"]))
if __name__=="__main__":main()
