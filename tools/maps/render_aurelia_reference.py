#!/usr/bin/env python3
"""Deterministic evidence renderer for the actual generated Aurelia NIF geometry.

This is deliberately not an AI/image-generation step.  It imports the exact
asset builders used to write the Gamebryo NIF files and the exact placement
list used to write Aurelia75.shmd, projects their triangles with a perspective
camera and rasterizes them to PNG.  The evidence images therefore show the
real authored map geometry/layout, with simple material-color lighting rather
than hypothetical concept art.
"""
from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE=Path(__file__).resolve().parent
REPO=HERE.parents[1]
for item in (str(HERE),str(REPO/"tools"/"nif")):
    if item not in sys.path: sys.path.insert(0,item)

import generate_aurelia_reference_city as city
import generate_aurelia_landmarks as landmarks
import generate_original_fantasy_assets as ng


def vsub(a,b): return (a[0]-b[0],a[1]-b[1],a[2]-b[2])
def vadd(a,b): return (a[0]+b[0],a[1]+b[1],a[2]+b[2])
def vmul(a,s): return (a[0]*s,a[1]*s,a[2]*s)
def dot(a,b): return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def length(a): return math.sqrt(max(1e-20,dot(a,a)))
def norm(a):
    l=length(a); return (a[0]/l,a[1]/l,a[2]/l)


@dataclass(frozen=True)
class Camera:
    eye: tuple[float,float,float]
    target: tuple[float,float,float]
    fov: float


CAMERAS={
    "aerial":Camera((5750,-1550,5000),(9300,3300,760),43),
    "harbor":Camera((6500,-900,1900),(9300,2250,720),48),
    "plaza":Camera((7850,1250,1300),(9300,3020,720),48),
    "citadel":Camera((7600,2850,1900),(9300,4540,1160),46),
    "east":Camera((12450,1550,2600),(9450,3450,820),44),
}


def model_name(model_path: str) -> str:
    return model_path.replace("\\","/").rsplit("/",1)[-1].rsplit(".",1)[0]


def transform(v, placement):
    a=math.radians(placement.yaw_deg); c,s=math.cos(a),math.sin(a)
    x,y,z=v
    x*=placement.scale; y*=placement.scale; z*=placement.scale
    return (placement.x + x*c-y*s,
            placement.y + x*s+y*c,
            placement.elevation + z)


def material_rgb(name: str) -> tuple[int,int,int]:
    rgb=ng.COLORS.get(name,(0.72,0.72,0.72))
    return tuple(max(0,min(255,int(round(x*255)))) for x in rgb)


def sky_image(width,height):
    image=Image.new("RGB",(width,height))
    px=image.load()
    top=(89,167,232); bottom=(224,239,248)
    for y in range(height):
        t=y/max(1,height-1)
        col=tuple(int(top[i]*(1-t)+bottom[i]*t) for i in range(3))
        for x in range(width): px[x,y]=col
    return image


def render(placements, camera: Camera, output: Path, width=1600, height=900):
    fwd=norm(vsub(camera.target,camera.eye))
    right=norm(cross(fwd,(0,0,1)))
    up=norm(cross(right,fwd))
    focal=(height/2)/math.tan(math.radians(camera.fov)/2)
    sun=norm((-0.35,-0.55,0.82))
    tris=[]
    cache={}

    def cam_coords(v):
        r=vsub(v,camera.eye)
        return dot(r,right),dot(r,up),dot(r,fwd)

    for placement in placements:
        name=model_name(placement.model)
        builder=landmarks.ALL_ASSETS.get(name)
        if builder is None:
            continue
        if name not in cache: cache[name]=builder()
        parts=cache[name]
        for material,mesh in parts.items():
            base_rgb=material_rgb(material)
            for ia,ib,ic in mesh.triangles:
                world=(transform(mesh.vertices[ia],placement),
                       transform(mesh.vertices[ib],placement),
                       transform(mesh.vertices[ic],placement))
                cam=tuple(cam_coords(v) for v in world)
                if min(v[2] for v in cam) <= 8:
                    continue
                pts=[]
                for x,y,z in cam:
                    pts.append((width/2+focal*x/z,height/2-focal*y/z))
                if max(x for x,y in pts)<-200 or min(x for x,y in pts)>width+200 or max(y for x,y in pts)<-200 or min(y for x,y in pts)>height+200:
                    continue
                e1=vsub(world[1],world[0]); e2=vsub(world[2],world[0])
                n=norm(cross(e1,e2))
                light=0.52+0.48*max(0.0,dot(n,sun))
                depth=sum(v[2] for v in cam)/3
                haze=max(0.0,min(0.62,(depth-3300)/6500))
                lit=tuple(int(max(0,min(255,c*light))) for c in base_rgb)
                haze_col=(190,218,238)
                color=tuple(int(lit[i]*(1-haze)+haze_col[i]*haze) for i in range(3))
                tris.append((depth,pts,color))

    tris.sort(key=lambda item:item[0],reverse=True)
    image=sky_image(width,height)
    draw=ImageDraw.Draw(image)
    for _,pts,color in tris:
        draw.polygon(pts,fill=color)

    # Evidence mark: exact source provenance, not a visual embellishment.
    label="Aurelia75 v2 | exact generated NIF geometry + SHMD placements | NextGen technical render"
    font=ImageFont.load_default()
    box=draw.textbbox((0,0),label,font=font)
    tw,th=box[2]-box[0],box[3]-box[1]
    draw.rounded_rectangle((14,height-th-31,24+tw,height-12),radius=6,fill=(8,18,31))
    draw.text((19,height-th-26),label,font=font,fill=(236,243,250))
    output.parent.mkdir(parents=True,exist_ok=True)
    image.save(output,optimize=True)
    return len(tris)


def contact_sheet(files: list[tuple[str,Path]], output: Path):
    thumbs=[]
    for label,path in files:
        im=Image.open(path).convert("RGB"); im.thumbnail((780,440))
        thumbs.append((label,im.copy()))
    canvas=Image.new("RGB",(1600,960),(18,23,31)); draw=ImageDraw.Draw(canvas); font=ImageFont.load_default()
    for idx,(label,im) in enumerate(thumbs[:4]):
        x=10+(idx%2)*795; y=28+(idx//2)*465
        canvas.paste(im,(x,y+18)); draw.text((x,y),label.upper(),font=font,fill=(235,240,247))
    output.parent.mkdir(parents=True,exist_ok=True); canvas.save(output,optimize=True)


def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--output",type=Path,default=Path("build/aurelia-renders"))
    ap.add_argument("--width",type=int,default=1600); ap.add_argument("--height",type=int,default=900)
    args=ap.parse_args()
    placements=city.build_reference_city()
    args.output.mkdir(parents=True,exist_ok=True)
    rendered=[]
    for name,camera in CAMERAS.items():
        path=args.output/f"aurelia_{name}.png"
        count=render(placements,camera,path,args.width,args.height)
        print(f"{name}: {count} visible triangles -> {path}")
        rendered.append((name,path))
    contact_sheet(rendered,args.output/"aurelia_contact_sheet.png")
    (args.output/"README.txt").write_text(
        f"Aurelia75 v2 technical render evidence\nPlacements: {len(placements)}\n"
        "Images were rasterized from the exact procedural mesh geometry written to the NIF assets and the exact SHMD placement list.\n"
        "No generative-image model is used in this rendering step.\n",encoding="utf-8")
    print(f"placements={len(placements)} output={args.output}")
    return 0


if __name__=="__main__": raise SystemExit(main())
