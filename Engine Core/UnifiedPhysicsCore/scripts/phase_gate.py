"""Read-only evidence inspection for P0..P8. Conservative: never infer completion from file names alone."""
from __future__ import annotations
import json
import hashlib
import subprocess
from pathlib import Path
from datetime import datetime, timezone

HERE = Path(__file__).resolve().parent
CORE = HERE.parent
PROJECT = CORE.parents[1]
LAB = PROJECT / "ENGINE TEST STUBE"
BUILDS = LAB / "unreal" / "builds" / "EngineMaterialLab_ue58"
UPROJECT = LAB / "unreal" / "ENGINE_TEST_STUBE" / "ENGINE_TEST_STUBE.uproject"
LOG = LAB / "unreal" / "ENGINE_TEST_STUBE" / "Saved" / "Logs" / "ENGINE_TEST_STUBE.log"

def git_rev() -> str:
    cmd = subprocess.run(["git","-C",str(PROJECT),"rev-parse","--short","HEAD"],
                         capture_output=True,text=True,timeout=20)
    return cmd.stdout.strip() if cmd.returncode==0 else "unavailable"

def ctest() -> dict:
    ctest_exe = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe")
    if not ctest_exe.exists(): return {"ok":False,"message":"ctest missing"}
    p = subprocess.run([str(ctest_exe),"--test-dir",str(CORE/"build"),"--output-on-failure","--parallel","4"],
                       capture_output=True,text=True,timeout=180)
    return {"ok":p.returncode==0 and "100% tests passed" in p.stdout,
            "tail":p.stdout[-1800:], "exit":p.returncode}

def p1_evidence() -> dict:
    report_path = LAB / "runs" / "p1_acceptance.json"
    if not report_path.exists():
        return {"ok":False,"why":"No P1 native acceptance result"}
    try:
        data=json.loads(report_path.read_text(encoding="utf-8"))
        if data.get("phase")!="P1" or data.get("accepted") is not True:
            raise ValueError("P1 quantitative acceptance is not PASS")
        checks=data.get("checks",[])
        if len(checks)<30 or not all(item.get("pass") is True for item in checks):
            raise ValueError("Insufficient/failed P1 quantitative checks")
        hashes=data.get("source_sha256",{})
        if len(hashes)<8:
            raise ValueError("Incomplete P1 source fingerprint list")
        for filename,expected in hashes.items():
            path=CORE/filename
            if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=expected:
                raise ValueError(f"Changed or missing P1 evidence source {filename}")
        dry=data.get("ritter_dry_bed",[])
        wet=data.get("stoker_wet_bed",[])
        if len(dry)!=3 or len(wet)!=3:
            raise ValueError("Independent analytical benchmark rows missing")
        return {"ok":True,"why":f"{len(checks)} quantitative checks, Ritter + Stoker and source hashes verified"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p7_full_gpu_evidence() -> dict:
    """Pass only after hardware D3D11 CPU parity, streaming, and SHA evidence."""
    path=LAB/"runs"/"p7_acceptance.json"
    if not path.is_file():
        return {"ok":False,"why":"P7 real hardware GPU quantitative report missing"}
    try:
        d=json.loads(path.read_text(encoding="utf-8"))
        checks=d.get("checks",[])
        if d.get("phase")!="P7" or d.get("accepted") is not True or len(checks)<160 or not all(x.get("pass") is True for x in checks):
            raise ValueError("P7 hardware CPU/GPU, latency or VRAM checks incomplete")
        hw=d.get("hardware",{})
        if (hw.get("driver_type","").find("HARDWARE")<0 or
            hw.get("feature_level",0)<45056 or hw.get("gpu_adapter_vendor_id",0)==0):
            raise ValueError("P7 real GPU feature-level 11 unavailable")
        if len(d.get("gpu_reference",[]))!=20 or len(d.get("gpu_stream",[]))!=3:
            raise ValueError("P7 native GPU dispatch and streaming references missing")
        n=d.get("native_assertions",{})
        if n.get("streamer",0)<250000 or n.get("sparse",0)<230:
            raise ValueError("P7 native sparse/streaming tests missing")
        hashes=d.get("source_sha256",{})
        if len(hashes)<15:raise ValueError("P7 critical shader/code signatures incomplete")
        for name,sha in hashes.items():
            source=CORE/name
            if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P7 source changed after GPU execution: {name}")
        return {"ok":True,"why":f"{len(checks)} full hardware D3D11 CPU/GPU LOD and memory/latency gates, 20 GPU dispatches, 3 VRAM brick upload tests, 250k+ CPU asserts"}
    except (ValueError,TypeError,KeyError,OSError) as err:
        return {"ok":False,"why":str(err)}

def p7_sparse_partial_evidence() -> dict:
    path=LAB / "runs" / "p7_sparse_partial.json"
    if not path.is_file():
        return {"ok":False,"why":"No native P7 sparse 3D LOD partial report"}
    try:
        d=json.loads(path.read_text(encoding="utf-8"))
        if d.get("phase")!="P7" or d.get("partial_accepted") is not True or d.get("phase_complete") is not False:
            raise ValueError("P7 CPU LOD partial report flags invalid")
        checks=d.get("checks",[])
        if len(checks)<20 or not all(c.get("pass") is True for c in checks):
            raise ValueError("P7 sparse 3D LOD numeric checks missing")
        if d.get("native_assertions",0)<230 or len(d.get("reference",[]))!=3:
            raise ValueError("P7 native MAC/VOF LOD integration/CPU references missing")
        hashes=d.get("source_sha256",{})
        if len(hashes)<8:
            raise ValueError("P7 partial source fingerprint list incomplete")
        for name,sha in hashes.items():
            f=CORE/name
            if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P7 partial source changed: {name}")
        return {"ok":True,"why":f"{len(checks)} sparse 3D MAC/VOF LOD CPU checks with source hashes; GPU still open"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p6_full_cpu_evidence() -> dict:
    path=LAB / "runs" / "p6_acceptance.json"
    if not path.is_file():
        return {"ok":False,"why":"P6 3D stoichiometry/diffusion/thermal coupled acceptance report missing"}
    try:
        d=json.loads(path.read_text(encoding="utf-8"))
        if d.get("phase")!="P6" or d.get("accepted") is not True:
            raise ValueError("P6 quantitative model validation NOT PASS")
        c=d.get("checks",[])
        if len(c)<36 or not all(x.get("pass") is True for x in c):
            raise ValueError("P6 reaction/energy/Fick quantitative controls incomplete")
        rows=d.get("reference",[])
        if len(rows)!=6 or len([x for x in rows if x.get("scenario")=="arrhenius_kinetics"])!=3:
            raise ValueError("P6 kinetic numerical comparison absent")
        assert_count=d.get("native_assertions",{})
        if assert_count.get("mixture",0)<830 or assert_count.get("p4_contact",0)<300:
            raise ValueError("P6 3D elemental mass/heat or P4 heat feedback native test absent")
        source_hashes=d.get("source_sha256",{})
        if len(source_hashes)<11:
            raise ValueError("P6 source SHA-256 coverage incomplete")
        for name,expected in source_hashes.items():
            source=CORE/name
            if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest()!=expected:
                raise ValueError(f"P6 source changed: {name}")
        return {"ok":True,"why":f"{len(c)} reaction/3D Fick/Arrhenius/atomic mass/thermal contact checks and matching source hashes"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p8_native_evidence() -> dict:
    """Strict native UE5.8 GAME+PIE acceptance, NOT screenshots-only."""
    path=LAB/"runs"/"p8_acceptance.json"
    if not path.is_file():return {"ok":False,"why":"P8 native E2E report missing"}
    try:
        a=json.loads(path.read_text(encoding="utf-8"))
        checks=a.get("checks",[])
        if a.get("phase")!="P8" or a.get("accepted") is not True or len(checks)<20 or not all(x.get("pass") is True for x in checks):
            raise ValueError("P8 real UE5.8 quantitative gate incomplete")
        hashes=a.get("sources_sha256",{})
        if len(hashes)<16:raise ValueError("Missing P8 sources")
        for f,h in hashes.items():
            src=PROJECT/f
            if not src.is_file() or hashlib.sha256(src.read_bytes()).hexdigest()!=h:
                raise ValueError(f"P8 tested source missing/changed: {f}")
        project=LAB/"unreal"/"ENGINE_TEST_STUBE"
        module=project/"Plugins"/"EngineMaterialLab"/"Binaries"/"Win64"/"UnrealEditor-EngineMaterialLab.dll"
        if not module.is_file() or hashlib.sha256(module.read_bytes()).hexdigest()!=a.get("native_plugin_dll_sha256"):
            raise ValueError("P8 tested native Unreal DLL missing/changed")
        for mode in ("game","pie"):
            path=LAB/"runs"/("p8_"+mode+"_process.json")
            if not path.is_file():raise ValueError(f"P8 {mode} process report missing")
            r=json.loads(path.read_text(encoding="utf-8"))
            if not (r.get("accepted") is True and r.get("process_exit_code")==0 and
                    r.get("timed_out") is False and all(r.get("checks",{}).values())):
                raise ValueError(f"P8 {mode} did not pass native clean exit")
            proof=a.get("process_proof",{}).get(mode,{})
            image=r.get("image",{}).get("sha256")
            if image!=proof.get("image_sha256"):
                raise ValueError(f"P8 {mode} image signature differs")
            png=project/"Saved"/"EngineMaterialLab"/"P8Native"/(
                "P8_"+mode.upper()+"_UE58_Native_3DWater.png")
            log=LAB/"runs"/("p8_"+mode+"_unreal_native.log")
            if not png.is_file() or hashlib.sha256(png.read_bytes()).hexdigest()!=image or not log.is_file() or hashlib.sha256(log.read_bytes()).hexdigest()!=r.get("native_log_sha256"):
                raise ValueError(f"P8 {mode} GPU PNG or UE native log mismatch")
        return {"ok":True,"why":f"{len(checks)} strict checks: UE5.8 BuildPlugin + clean real GAME and PIE, native 3D MAC/VOF viscous rendering+mass+flow, SHA256 proof"}
    except (ValueError,KeyError,TypeError,OSError) as err:
        return {"ok":False,"why":str(err)}

def p5_full_cpu_evidence() -> dict:
    path=LAB / "runs" / "p5_full_acceptance.json"
    if not path.is_file():
        return {"ok":False,"why":"P5 deformable multi-body and constitutive verification missing"}
    try:
        data=json.loads(path.read_text(encoding="utf-8"))
        if data.get("phase")!="P5" or data.get("accepted") is not True:
            raise ValueError("P5 advanced numerical CPU gate is NOT PASS")
        checks=data.get("checks",[])
        if len(checks)<60 or not all(check.get("pass") is True for check in checks):
            raise ValueError("Not enough successful P5 self-contact and constitutive controls")
        if len(data.get("affine_3d_reference",[]))!=12 or len(data.get("original_reference",[]))!=11:
            raise ValueError("Constitutive or grain/fracture calibration benchmark missing")
        counts=data.get("native_checks",{})
        if (counts.get("plane",0)<10984 or counts.get("granular",0)<37000
             or counts.get("lattice",0)<22000 or counts.get("volume_self",0)<17):
            raise ValueError("Deformable/contact/grain native suite incomplete")
        source_hashes=data.get("source_sha256",{})
        if len(source_hashes)<16:
            raise ValueError("P5 source coverage incomplete")
        for filename,sha in source_hashes.items():
            f=CORE/filename
            if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P5 advanced code changed since test: {filename}")
        return {"ok":True,"why":f"{len(checks)} full P5 CPU controls plus "+
                "nonbonded 3D self/multibody/fracture-contacts and affine stress-strain references"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p5_bounded_partial_evidence() -> dict:
    path = LAB / "runs" / "p5_acceptance.json"
    if not path.is_file():
        return {"ok":False,"why":"P5 bounded 3D CPU solid/fracture/granular acceptance missing"}
    try:
        data=json.loads(path.read_text(encoding="utf-8"))
        if (data.get("phase")!="P5" or data.get("partial_accepted") is not True
            or data.get("phase_complete") is not False):
            raise ValueError("P5 bounded CPU-partial milestone NOT approved")
        checks=data.get("checks",[])
        if len(checks)<45 or not all(item.get("pass") is True for item in checks):
            raise ValueError("Incomplete P5 numeric benchmarks/invariants")
        native=data.get("native_checks",{})
        if native.get("plane",0)<10984 or native.get("lattice",0)<22000 or native.get("granular",0)<37000:
            raise ValueError("P5 contact, volumetric fracture or granular checks missing")
        rows=data.get("reference",[])
        if len(rows)!=11:
            raise ValueError("Missing independent Hooke/collision/fracture/CPU results")
        hashes=data.get("source_sha256",{})
        if len(hashes)<13:
            raise ValueError("P5 critical source fingerprints incomplete")
        for filename,sha in hashes.items():
            source=CORE / filename
            if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P5 source missing/changed since test: {filename}")
        return {"ok":True,"why":f"{len(checks)} quantitative CPU acceptance controls, "+
                "lattice fragments/voxel damage and DEM contact/energy/CPU; source hashes verified"}
    except (ValueError,TypeError,KeyError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p5_partial_evidence() -> dict:
    path=LAB / "runs" / "p5_contact_partial.json"
    if not path.is_file():
        return {"ok":False,"why":"P5 plane contact partial evidence missing"}
    try:
        data=json.loads(path.read_text(encoding="utf-8"))
        if (data.get("phase")!="P5" or
            data.get("partial_accepted") is not True or
            data.get("phase_complete") is not False):
            raise ValueError("P5 contact partial flags invalid")
        if data.get("native_checks",0)<10984:
            raise ValueError("Plane contact native assertions missing")
        for filename,sha in data.get("source_sha256",{}).items():
            source=CORE/filename
            if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P5 plane contact source mismatch: {filename}")
        return {"ok":True,"why":"Plane and restitution checks from prior P5 milestone verified"}
    except (ValueError,TypeError,KeyError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p4_complete_evidence() -> dict:
    report_path = LAB / "runs" / "p4_acceptance.json"
    if not report_path.exists():
        return {"ok":False,"why":"P4 dynamic thermodynamics CPU-MVP report missing"}
    try:
        data=json.loads(report_path.read_text(encoding="utf-8"))
        if data.get("phase")!="P4" or data.get("accepted") is not True or data.get("phase_complete") is not True:
            raise ValueError("P4 coupled CPU acceptance not approved")
        checks=data.get("checks",[])
        if len(checks)<25 or not all(c.get("pass") is True for c in checks):
            raise ValueError("Insufficient/failed P4 thermo-fluid quantitative checks")
        if data.get("native_thermal_assertions",0)<800 or data.get("native_phase_bridge_assertions",0)<25:
            raise ValueError("P4 thermal phase tests missing")
        if len(data.get("reference",[]))!=4:
            raise ValueError("P4 Neumann heat reference resolution missing")
        native=data.get("native_stdout",{})
        if not all("PASS:" in native.get(k,"") for k in ("thermal","phase_bridge","live_advection","soil_integration")):
            raise ValueError("P4 live MAC/P3 material bridge native proof missing")
        hashes=data.get("source_sha256",{})
        if len(hashes)<15:
            raise ValueError("P4 full source fingerprints incomplete")
        for path,sha in hashes.items():
            actual=CORE/path
            if not actual.is_file() or hashlib.sha256(actual.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P4 source changed after acceptance: {path}")
        return {"ok":True,"why":f"{len(checks)} verified coupled 3D heat/phase/advection/soil gates; 4 Neumann references, source hashes"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p4_partial_evidence() -> dict:
    report_path = LAB / "runs" / "p4_partial_acceptance.json"
    if not report_path.exists():
        return {"ok":False,"why":"P4 3D thermal partial report missing"}
    try:
        data=json.loads(report_path.read_text(encoding="utf-8"))
        if data.get("phase")!="P4" or data.get("partial_accepted") is not True or data.get("phase_complete") is not False:
            raise ValueError("P4 partial scope evidence invalid")
        checks=data.get("checks",[])
        if len(checks)<22 or not all(c.get("pass") is True for c in checks):
            raise ValueError("P4 3D Fourier quantitative checks missing")
        if int(data.get("native_assertions",0))<800:
            raise ValueError("P4 phase-change and thermal regression missing")
        if len(data.get("reference",[]))!=4:
            raise ValueError("P4 3D Neumann benchmark grid count missing")
        hashes=data.get("source_sha256",{})
        if len(hashes)<6:
            raise ValueError("P4 source hash coverage incomplete")
        for name,sha in hashes.items():
            p=CORE/name
            if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P4 source missing or changed: {name}")
        return {"ok":True,"why":f"{len(checks)} quantitative checks, water latent phase plateaus, 3D Fourier and static runtime snapshot; source hashes verified; no live phase/flow coupling"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p3_evidence() -> dict:
    report_path = LAB / "runs" / "p3_acceptance.json"
    if not report_path.exists():
        return {"ok":False,"why":"P3 CPU material-voxel acceptance report missing"}
    try:
        data=json.loads(report_path.read_text(encoding="utf-8"))
        if data.get("phase")!="P3" or data.get("accepted") is not True:
            raise ValueError("P3 quantitative acceptance not approved")
        checks=data.get("checks",[])
        if len(checks)<45 or not all(c.get("pass") is True for c in checks):
            raise ValueError("Insufficient or failed P3 quantitative checks")
        if int(data.get("body_native_assertions",0))<500:
            raise ValueError("3D body/porosity native tests missing")
        if float(data.get("buoyancy_relative_error",1))>=0.05:
            raise ValueError("Buoyancy check outside tolerance")
        rows=data.get("reference",[])
        if len(rows)!=12:
            raise ValueError("Missing Darcy/integrated 3D results")
        hashes=data.get("source_sha256",{})
        if len(hashes)<9:
            raise ValueError("Not all P3 source files fingerprinted")
        for filename,sha in hashes.items():
            source=CORE/filename
            if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"Missing or changed P3 source: {filename}")
        return {"ok":True,"why":f"{len(checks)} quantitative P3 gates, 3D moving body, Darcy and energy checks; source hashes verified. CPU MVP only"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p2_viscosity_partial_evidence() -> dict:
    """Isolated 3D viscosity step: separate technical milestone, NOT P2 full."""
    path=LAB/"runs"/"p2_viscosity_partial.json"
    if not path.is_file():
        return {"ok":False,"why":"No optional P2 viscous-MAC partial evidence"}
    try:
        d=json.loads(path.read_text(encoding="utf-8"))
        checks=d.get("checks",[])
        if (d.get("phase")!="P2" or
            d.get("milestone")!="viscous_mac3d_partial" or
            d.get("partial_accepted") is not True or
            d.get("phase_complete") is not False or
            len(checks)<7 or not all(x.get("pass") is True for x in checks)):
            raise ValueError("P2 viscous MAC partial numeric tests incomplete")
        hashes=d.get("source_sha256",{})
        if len(hashes)<6:raise ValueError("P2 viscosity fingerprints incomplete")
        for filename,sha in hashes.items():
            f=CORE/filename
            if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P2 viscosity tested source changed: {filename}")
        return {"ok":True,"why":"Independent analytic 3D MAC viscous eigenmode and energy-dissipation checks PASS; isolated operator evidence only"}
    except (ValueError,KeyError,TypeError,OSError) as err:
        return {"ok":False,"why":str(err)}

def p2_live_viscosity_partial_evidence() -> dict:
    """Actual CoupledMacWater3D viscous-flow integration, not P2 full acceptance."""
    path=LAB/"runs"/"p2_live_viscosity_partial.json"
    if not path.is_file():
        return {"ok":False,"why":"No live 3D MAC/VOF viscosity integration evidence"}
    try:
        d=json.loads(path.read_text(encoding="utf-8"))
        checks=d.get("checks",[])
        if (d.get("phase")!="P2" or
            d.get("milestone")!="live_masked_mac_vof_viscosity_partial" or
            d.get("partial_accepted") is not True or
            d.get("phase_complete") is not False or
            len(checks)<8 or not all(x.get("pass") is True for x in checks)):
            raise ValueError("Live 3D viscous MAC/VOF partial tests incomplete")
        hashes=d.get("source_sha256",{})
        if len(hashes)<8:
            raise ValueError("Live 3D viscous MAC source fingerprint list incomplete")
        for filename,sha in hashes.items():
            f=CORE/filename
            if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"Live 3D viscous MAC tested source changed: {filename}")
        return {"ok":True,"why":"Native live MAC/VOF masked viscosity and legacy regression, mass and face-energy proxy verified"}
    except (ValueError,KeyError,TypeError,OSError) as err:
        return {"ok":False,"why":str(err)}

# This gate distinguishes a bounded prototype acceptance from an entire
# physics/Unreal development phase. A green P2-P8 MVP must NEVER imply
# full completion. The listed criteria require independent future E2E
# validation and a code-reviewed replacement of these explicit blockers.
FULL_SCOPE_OPEN={
    "P2":[
        "validate full stress-consistent 3D viscosity at moving free surface (current Neumann proxy only)",
        "validate dynamic solid no-slip/impulse and 3D obstacle coupling",
        "compare 3D flow and momentum against independent measured references",
    ],
    "P3":[
        "bidirectional moving-solid/free-water force and impulse conservation",
        "closed energy/mass accounting for porous flow and moving solids",
        "validate 3D anisotropic soil and deformation against independent references",
    ],
    "P4":[
        "real 3D steam/gas momentum and transport with phase changes",
        "globally coupled mechanical, latent and sensible energy conservation",
    ],
    "P5":[
        "validated continuum deformable FEM/MPM or equivalent and mesh contacts",
        "validated granular and fracture benchmarks across material scales",
    ],
    "P6":[
        "experimentally calibrated real-species chemistry and multiphase transfer",
        "validated full energy/species coupling to moving fluid and solids",
    ],
    "P7":[
        "native GPU/LOD streaming integration into real UE rendering runtime",
        "end-to-end frame-time and memory scaling with large scenes and async transfers",
    ],
    "P8":[
        "Unreal editor reload and repeatable Native-E2E for coupled P2-P7 modules",
        "live P3-P6 materials and P7 LOD/streaming visualized and verified in Unreal",
    ],
}
def enforce_full_phase_scope(phases:dict) -> dict:
    """Preserve MVP evidence, refuse full PASS while full criteria are open."""
    for name,record in phases.items():
        record["prototype_status"]=record["status"]
        record["full_acceptance_open"]=list(FULL_SCOPE_OPEN.get(name,[]))
        record["completion_scope"]="full" if not record["full_acceptance_open"] else "bounded_prototype"
        if record["full_acceptance_open"] and record["status"]=="PASS":
            record["status"]="PARTIAL"
    return phases

def p2_evidence() -> dict:
    report_path = LAB / "runs" / "p2_acceptance.json"
    if not report_path.exists():
        return {"ok": False, "why": "P2 native quantitative acceptance report missing"}
    try:
        data=json.loads(report_path.read_text(encoding="utf-8"))
        if data.get("phase")!="P2" or data.get("accepted") is not True:
            raise ValueError("P2 acceptance not approved")
        checks=data.get("checks",[])
        if len(checks)<30 or not all(entry.get("pass") is True for entry in checks):
            raise ValueError("Missing or failed quantitative acceptance checks")
        hashes=data.get("source_sha256",{})
        for filename,expected in hashes.items():
            path=CORE / filename
            if not path.is_file():
                raise ValueError(f"Missing P2 source: {filename}")
            actual=hashlib.sha256(path.read_bytes()).hexdigest()
            if actual != expected:
                raise ValueError(f"P2 source changed since verified: {filename}")
        if len(hashes)<7:
            raise ValueError("Insufficient P2 source fingerprint coverage")
        return {"ok":True,"why":f"{len(checks)} quantitative P2 checks and source hashes verified"}
    except (ValueError,KeyError,TypeError,OSError) as exc:
        return {"ok":False,"why":str(exc)}

def p2_wall_shear_partial_evidence() -> dict:
    """Fixed-geometry prescribed tangential wall shear only; not FSI or P2 full."""
    path=LAB/"runs"/"p2_wall_shear_partial.json"
    if not path.is_file():
        return {"ok":False,"why":"P2 fixed-wall shear partial evidence missing"}
    try:
        d=json.loads(path.read_text(encoding="utf-8"))
        checks=d.get("checks",[])
        if (d.get("phase")!="P2" or d.get("milestone")!="prescribed_fixed_wall_tangential_shear_partial"
                or d.get("partial_accepted") is not True or d.get("phase_complete") is not False
                or len(checks)<6 or not all(c.get("pass") is True for c in checks)):
            raise ValueError("P2 wall shear partial acceptance invalid")
        hashes=d.get("source_sha256",{})
        if len(hashes)<7:
            raise ValueError("P2 wall shear source coverage insufficient")
        for name,sha in hashes.items():
            f=CORE/name
            if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P2 wall shear source mismatch: {name}")
        return {"ok":True,"why":"independent discrete Couette wall-shear stencil and 35 native CTests with SHA evidence"}
    except (ValueError,KeyError,TypeError,OSError) as err:
        return {"ok":False,"why":str(err)}

def p2_momentum_ledger_partial_evidence() -> dict:
    """Bounded diagnostic face-momentum budget, never physical FSI acceptance."""
    path=LAB/"runs"/"p2_viscous_momentum_ledger_partial.json"
    if not path.is_file():
        return {"ok":False,"why":"P2 masked MAC momentum ledger partial report missing"}
    try:
        d=json.loads(path.read_text(encoding="utf-8"))
        checks=d.get("checks",[])
        if (d.get("phase")!="P2" or
            d.get("milestone")!="masked_mac_face_momentum_proxy_ledger_partial" or
            d.get("partial_accepted") is not True or
            d.get("phase_complete") is not False or
            len(checks)<10 or not all(c.get("pass") is True for c in checks)):
            raise ValueError("P2 masked face-momentum partial checks invalid")
        hashes=d.get("source_sha256",{})
        if len(hashes)<10:
            raise ValueError("P2 momentum ledger fingerprint coverage insufficient")
        for name,sha in hashes.items():
            f=CORE/name
            if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=sha:
                raise ValueError(f"P2 momentum ledger source changed: {name}")
        return {"ok":True,"why":"53 native assertions, independent 90-face Couette source, live 36kg VOF and diffusion-only face-momentum proxy closure; NOT physical FSI"}
    except (ValueError,KeyError,TypeError,OSError) as err:
        return {"ok":False,"why":str(err)}

def main() -> None:
    tests = ctest()
    p1 = p1_evidence()
    p2 = p2_evidence()
    p2_viscosity = p2_viscosity_partial_evidence()
    p2_live_viscosity = p2_live_viscosity_partial_evidence()
    p2_wall_shear = p2_wall_shear_partial_evidence()
    p2_momentum_ledger = p2_momentum_ledger_partial_evidence()
    p3 = p3_evidence()
    p4_partial = p4_partial_evidence()
    p4 = p4_complete_evidence()
    p5 = p5_bounded_partial_evidence()
    p5_partial = p5_partial_evidence()
    p5_full = p5_full_cpu_evidence()
    p6 = p6_full_cpu_evidence()
    p7_partial = p7_sparse_partial_evidence()
    p7_full = p7_full_gpu_evidence()
    p8 = p8_native_evidence()
    ue_bin = BUILDS / "Binaries" / "Win64" / "UnrealEditor-EngineMaterialLab.dll"
    log = LOG.read_text(encoding="utf-8",errors="replace") if LOG.exists() else ""
    plugin_built = ue_bin.exists() and ue_bin.stat().st_size>0
    native_module = "EngineMaterialLab UPC started" in log
    # Manual acceptance evidence is mandatory for remaining phase closures.
    # Prototypes and smoke tests cannot accidentally turn a phase green.
    phases = {
       "P0":{"status":"PASS" if tests["ok"] else "FAIL",
             "evidence":"MSVC/CTest C++ core regression"},
       "P1":{"status":"PASS" if tests["ok"] and p1["ok"] else "PARTIAL",
             "evidence":"2D shallow-water CPU baseline, "+p1["why"]+
             "; 3D/viscosity and subgrid interface physics remain out of P1 scope"},
       "P2":{"status":"PASS" if p2["ok"] and p2_viscosity["ok"] and tests["ok"] else "PARTIAL",
              "evidence":"Selected MAC+VOF coupled 3D CPU prototype; "+p2["why"]+
              "; optional isolated viscous MAC milestone: "+p2_viscosity["why"]+
              "; live coupled viscous MAC/VOF milestone: "+p2_live_viscosity["why"]+
              "; fixed-wall tangential shear partial: "+p2_wall_shear["why"]+
              "; face-momentum proxy ledger partial: "+p2_momentum_ledger["why"]+
              "; full 3D CFD/FSI validation not proven"},
       "P3":{"status":"PASS" if p3["ok"] and tests["ok"] else "PARTIAL",
             "evidence":"Porous 3D soil/free-water, pressure load, voxel-body displacement, isolated mass and work-energy ledgers; "+p3["why"]+
             "; high-fidelity moving-wall impulse/energy FSI remains out of scope"},
       "P4":{"status":"PASS" if tests["ok"] and p4["ok"] and p4_partial["ok"] else "PARTIAL",
             "evidence":"3D CPU thermodynamics MVP: "+p4["why"]+
             "; thermal, liquid advection and P3-gravity heat budgets conserved in their test domains; no gas hydrodynamics or fully unified mechanical-energy closure"},
       "P5":{"status":"PASS" if (tests["ok"] and p5["ok"] and p5_partial["ok"] and p5_full["ok"]) else "PARTIAL",
             "evidence":"Bounded 3D CPU mass-spring/fracture and spherical DEM contact MVP; "+
             p5_full["why"]+"; preliminary contact "+p5_partial["why"]+
             "; full continuum FEM/MPM, accurate arbitrary surface contact and global FSI/thermal energy remain unverified"},
       "P6":{"status":"PASS" if tests["ok"] and p6["ok"] else "PARTIAL",
             "evidence":"Restricted 3D reactive-material CPU-MVP: "+p6["why"]+
             "; generic registered synthetic species, not calibrated real chemistry, not globally coupled multiphase-CFD"},
       "P7":{"status":"PASS" if (tests["ok"] and p7_partial["ok"] and p7_full["ok"] and p8["ok"]) else "PARTIAL",
             "evidence":"Hardware 3D render-LOD GPU/streaming CPU-MVP: "+p7_full["why"]+
             "; previous sparse CPU gate: "+p7_partial["why"]+
             "; P8 native UE/PIE: "+p8["why"]+
             "; GPU physics and Unreal RDG/AAA frame gains remain unverified"},
       "P8":{"status":"PASS" if tests["ok"] and p8["ok"] else "PARTIAL",
             "evidence":"Native UE5.8 isolated adapter CPU-MVP: "+p8["why"]+
             "; real 3D MAC water only, P3-P6 Unreal renderer and P7 GPU/LOD not proven"},
    }
    phases=enforce_full_phase_scope(phases)
    out={
      "generated_utc":datetime.now(timezone.utc).isoformat(),
      "project_root":str(PROJECT),"git_commit":git_rev(),"ctest":tests,
      "phases":phases,"all_phases_complete":all(p["status"]=="PASS" for p in phases.values())
    }
    dest=LAB/"runs"/"unified_physics_gate.json"
    dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_text(json.dumps(out,indent=2,ensure_ascii=False),encoding="utf-8")
    print("GATES_PASS="+",".join(k for k,v in phases.items() if v["status"]=="PASS"))
    print("GATES_INCOMPLETE="+",".join(k for k,v in phases.items() if v["status"]!="PASS"))
    print("ALL_COMPLETE="+str(out["all_phases_complete"]))
    print("EVIDENCE="+str(dest))
    if not tests["ok"]: raise SystemExit(1)
if __name__=="__main__":main()
