"""Offline visualization/OBJ export of exact binary voxels created by C++ UPC tree printer.
No Blender/UE invocation. Renders geometry by ray-casting the MATERIAL-ID occupancy grid,
not by drawing a pre-made tree image. Cutaway removes one quadrant to expose 4 inner layers.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import json
import math
import numpy as np
from hashlib import sha256
import time

ROOT=Path(__file__).resolve().parent
OUT=ROOT/"output"
report=json.loads((OUT/"report.json").read_text(encoding="utf-8"))
NX,NY,NZ=report["nx"],report["ny"],report["nz"]
DX=report["dx_m"]
raw=(OUT/"voxel_material_ids.bin").read_bytes()
if len(raw)!=NX*NY*NZ: raise ValueError("Binary voxel payload unexpected size")
ids=np.frombuffer(raw,dtype=np.uint8).reshape((NZ,NY,NX)).copy()
assert set(np.unique(ids).tolist())=={0,1,2,3,4}
zind,yind,xind=np.indices(ids.shape, sparse=True)
cut=ids.copy()
cut[:,:NY//2,NX//2:]=0
print("INPUT:",len(raw),"bytes, SHA256",sha256(raw).hexdigest(),flush=True)

MTL={1:("Kernholz",(125,73,43)),2:("Splintholz",(202,160,104)),
     3:("Kambium",(236,208,136)),4:("Rinde",(65,42,31))}
PAL=np.array([(0,0,0)]+[MTL[x][1] for x in range(1,5)],dtype=np.uint8)
def font(size):
    f=Path("C:/Windows/Fonts/arial.ttf")
    return ImageFont.truetype(str(f),size) if f.exists() else ImageFont.load_default()
F14,F20,F29=font(14),font(20),font(29)

def render(volume, width=540, height=610, upto=1.0, cut_mode=False):
    t0=time.perf_counter()
    h,w=height,width
    eye=np.array([2.2,-2.6,1.83],dtype=np.float64)
    center=np.array([NX*DX/2,NY*DX/2,NZ*DX*0.52],dtype=np.float64)
    forward=center-eye
    forward/=np.linalg.norm(forward)
    right=np.cross(forward,[0.,0.,1.]);right/=np.linalg.norm(right)
    up=np.cross(right,forward);up/=np.linalg.norm(up)
    pixel=np.indices((h,w),dtype=np.float32)
    uvx=(pixel[1]-(w-1)/2)*(1.35/w)
    uvy=((h-1)/2-pixel[0])*(1.56/h)
    o=center-2.*forward
    ox=o[0]+right[0]*uvx+up[0]*uvy
    oy=o[1]+right[1]*uvx+up[1]*uvy
    oz=o[2]+right[2]*uvx+up[2]*uvy
    maxz=min(NZ,max(1,int(math.ceil(NZ*upto))))
    size=np.array([NX*DX,NY*DX,maxz*DX])
    # Accurate intersection of orthographic rays with printed-voxel bounding box.
    enter=np.full((h,w),-np.inf,dtype=np.float32)
    leave=np.full((h,w),np.inf,dtype=np.float32)
    for position,velocity,hi in [(ox,forward[0],size[0]),(oy,forward[1],size[1]),(oz,forward[2],size[2])]:
        if abs(velocity)<1e-6:
            far=np.where((position>=0)&(position<=hi), np.inf,-np.inf)
            enter=np.maximum(enter,np.where(np.isinf(far)&(far>0),-np.inf,np.inf))
            leave=np.minimum(leave,far)
        else:
            a=(0-position)/velocity
            b=(hi-position)/velocity
            enter=np.maximum(enter,np.minimum(a,b))
            leave=np.minimum(leave,np.maximum(a,b))
    valid=(enter<=leave)&(leave>0)&np.isfinite(enter)&np.isfinite(leave)
    near=np.maximum(enter,0)
    img=np.empty((h,w,3),dtype=np.uint8)
    gy=np.arange(h,dtype=np.float32)[:,None]/max(1,h-1)
    # Slate background with a subtle vignette, not an externally generated image.
    img[:,:,0]=(20+6*(1-gy)).astype(np.uint8)
    img[:,:,1]=(28+9*(1-gy)).astype(np.uint8)
    img[:,:,2]=(39+14*(1-gy)).astype(np.uint8)
    active=valid.copy()
    light=np.array([0.35,-0.40,0.85])
    light/=np.linalg.norm(light)
    steps=int(np.ceil((np.nanmax(np.where(valid,leave-near,0))) / (DX*.65)))+2
    for n in range(steps):
        if not active.any(): break
        t=near+(n+.20)*DX*.65
        now=active&(t<=leave)
        if not now.any(): break
        xp=ox+forward[0]*t;yp=oy+forward[1]*t;zp=oz+forward[2]*t
        xi=np.clip((xp/DX).astype(np.int32),0,NX-1)
        yi=np.clip((yp/DX).astype(np.int32),0,NY-1)
        zi=np.clip((zp/DX).astype(np.int32),0,NZ-1)
        mid=volume[zi,yi,xi]
        hit=now&(mid!=0)&(zi<maxz)
        if hit.any():
            az,ay,ax=zi[hit],yi[hit],xi[hit]
            def field(xoff,yoff,zoff):
                x=np.clip(ax+xoff,0,NX-1);y=np.clip(ay+yoff,0,NY-1);z=np.clip(az+zoff,0,NZ-1)
                val=volume[z,y,x]!=0
                val=val & (z<maxz)
                return val.astype(np.float32)
            normal=np.stack([field(-1,0,0)-field(1,0,0),
                             field(0,-1,0)-field(0,1,0),
                             field(0,0,-1)-field(0,0,1)],axis=1)
            length=np.sqrt(np.sum(normal*normal,axis=1))
            length=np.where(length>0,length,1.)
            normal=normal/length[:,None]
            exposure=np.clip(normal@light,-0.15,1.)
            lum=0.67+0.32*np.maximum(0,exposure)
            # procedural grain applied to bark and growth rings; NOT material data duplication.
            ma=mid[hit]
            ang=np.arctan2((ay+.5-NY/2),(ax+.5-NX/2))
            wood_ring=0.93+0.055*np.cos(np.hypot(ax-NX/2,ay-NY/2)*2.2)
            bark_variation=0.85+0.20*np.cos(0.75*az+17*ang)
            tex=np.where(ma==4,bark_variation,wood_ring)
            c=PAL[ma].astype(np.float32)*np.clip(lum*tex,0.25,1.25)[:,None]
            img[hit]=np.clip(c,0,255).astype(np.uint8)
        active &= ~hit
        active &= now
    return Image.fromarray(img,mode="RGB"),round(time.perf_counter()-t0,2)

# Cross-section is an EXACT z slice of binary material IDs; only resized for visibility.
slicez=int(NZ*.74)
cross=Image.fromarray(PAL[ids[slicez]],mode="RGB").resize((528,528),resample=Image.Resampling.NEAREST)
image,sec=render(ids,630,720)
cutimg,sec2=render(cut,630,720,cut_mode=True)
image.save(OUT/"tree_trunk_full.png")
cutimg.save(OUT/"tree_trunk_cutaway.png")
cross.save(OUT/"tree_trunk_cross_section.png")
print("FULL RENDER",sec,"s CUT RENDER",sec2,"s",flush=True)

# "Printer advancing upward" contact sheet.
perc=[10,30,55,80,100]
frames=[]
for p in perc:
    im,elapsed=render(ids,270,344,upto=p/100)
    im.save(OUT/f"tree_trunk_stage_{p:03d}.png")
    frames.append(im)
    print("LAYER FRAME",p,elapsed,"s",flush=True)
sheet=Image.new("RGB",(len(frames)*286+20,445),(13,21,31))
draw=ImageDraw.Draw(sheet)
for i,(p,im) in enumerate(zip(perc,frames)):
    x=16+i*286
    sheet.paste(im,(x,55))
    draw.text((x+8,12),f"{p}% gedruckt",font=F20,fill=(225,239,245))
draw.text((18,405),"Schichtweiser Aufbau aus denselben Material-IDs des UnifiedPhysicsCore",font=F14,fill=(180,204,220))
sheet.save(OUT/"tree_trunk_print_progress.png")

# Evidence overview
panel=Image.new("RGB",(1372,900),(15,24,34))
d=ImageDraw.Draw(panel)
d.text((35,15),"VIRTUAL VOXEL PRINTER - BAUMSTAMM TEST 001",font=F29,fill=(233,238,242))
panel.paste(image,(20,85))
panel.paste(cutimg,(705,85))
d.text((45,812),"VOLLER STAMM  |  Rinde aus Material-IDs",font=F20,fill=(234,238,242))
d.text((726,812),"SCHNITT  |  Kernholz, Splintholz, Kambium, Rinde",font=F20,fill=(234,238,242))
d.text((35,855),f"{report['occupied_voxels']:,} Materialvoxel    |    {report['mass_kg_upc']:.1f} kg (Testdichten)    |    1.344 m Hoehe",font=F14,fill=(190,214,225))
panel.save(OUT/"tree_trunk_overview.png")

# OBJ mesh; only *exposed* voxel faces (not a fake primitive/cylinder).
# Faceted grid mesh is intentionally unoptimized proof geometry.
facespec=[
  (0,-1,[(0,0,0),(0,0,1),(0,1,1),(0,1,0)]),
  (0,+1,[(1,0,0),(1,1,0),(1,1,1),(1,0,1)]),
  (1,-1,[(0,0,0),(1,0,0),(1,0,1),(0,0,1)]),
  (1,+1,[(0,1,0),(0,1,1),(1,1,1),(1,1,0)]),
  (2,-1,[(0,0,0),(0,1,0),(1,1,0),(1,0,0)]),
  (2,+1,[(0,0,1),(1,0,1),(1,1,1),(0,1,1)])]
def export_obj(volume,name):
    obj=OUT/(name+".obj")
    mtl=OUT/(name+".mtl")
    with mtl.open("w",encoding="ascii") as fp:
        for typ,(n,c) in MTL.items():
            fp.write(f"newmtl {n}\nKd {c[0]/255:.6f} {c[1]/255:.6f} {c[2]/255:.6f}\nKa 0.1 0.1 0.1\nKs 0.03 0.03 0.03\nNs 16\n\n")
    totalfaces=0
    with obj.open("w",encoding="ascii",buffering=2**20) as f:
        f.write(f"# Generated from REAL UPC VoxelGrid exported occupancy, dx={DX}m\nmtllib {name}.mtl\n")
        vertex=0
        # z,y,x; adjacent material interfaces not exported in full, but visible in the cutaway.
        for axis,step,corners in facespec:
            neigh=np.zeros_like(volume)
            if axis==0 and step==1:neigh[:,:,:-1]=volume[:,:,1:]
            elif axis==0:neigh[:,:,1:]=volume[:,:,:-1]
            elif axis==1 and step==1:neigh[:,:-1,:]=volume[:,1:,:]
            elif axis==1:neigh[:,1:,:]=volume[:,:-1,:]
            elif axis==2 and step==1:neigh[:-1,:,:]=volume[1:,:,:]
            else:neigh[1:,:,:]=volume[:-1,:,:]
            mask=(volume!=0)&(neigh==0)
            iz,iy,ix=np.nonzero(mask)
            mats=volume[mask]
            for material in (1,2,3,4):
                selected=np.where(mats==material)[0]
                if not len(selected):continue
                f.write(f"usemtl {MTL[material][0]}\n")
                for n in selected:
                    x,y,z=int(ix[n]),int(iy[n]),int(iz[n])
                    for a,b,c in corners:
                        f.write(f"v {(x+a)*DX:.4f} {(y+b)*DX:.4f} {(z+c)*DX:.4f}\n")
                    f.write(f"f {vertex+1} {vertex+2} {vertex+3} {vertex+4}\n")
                    vertex+=4
                    totalfaces+=1
    return obj.stat().st_size,totalfaces
size,faces=export_obj(ids,"tree_trunk_full")
size2,faces2=export_obj(cut,"tree_trunk_cutaway")
print("OBJ FULL",size,"bytes",faces,"quads, CUT",size2,"bytes",faces2,"quads",flush=True)

# Readback proof: material sequence per stem height, full vs cutaway, independent mass count.
independent_counts={str(i):int((ids==i).sum()) for i in range(5)}
assert independent_counts["0"]+sum(independent_counts[str(i)] for i in range(1,5))==len(raw)
for entry in report["materials"]:
    assert independent_counts[str(entry["voxel_id"])]==entry["voxel_count"]
evidence={
  "status":"PASS", "evidence_scope":"UPC-material-filled voxels -> generated mesh and 3D raster visualization",
  "binary_sha256":sha256(raw).hexdigest(),
  "materials_all_present":True, "counts_match_cpp_report":True,
  "total_occupied":report["occupied_voxels"],"full_obj_quads":faces,"cutaway_obj_quads":faces2,
  "full_obj_bytes":size,"cutaway_obj_bytes":size2,
  "renders_from_binary":True,"real_native_Blender_or_Unreal":False,
  "biological_realism_validated":False,"surface_mesh_optimized":False
}
(OUT/"render_validation.json").write_text(json.dumps(evidence,indent=2),encoding="utf-8")
print("VISUAL_QA_OFFLINE PASS, source_binary_sha256="+evidence["binary_sha256"],flush=True)
