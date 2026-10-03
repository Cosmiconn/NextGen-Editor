#!/usr/bin/env python3
"""Generate original colorful fantasy-village NIF assets for NextGen-Editor.

The generated assets are original geometry, not copies of Fiesta meshes/textures.
They target the Fiesta/NextGen Gamebryo 20.0.0.4 layout already consumed by
src/core/NifModel.cpp.  The first pass is intentionally texture-independent:
static NiTriShape geometry + normals + per-part NiMaterialProperty colors.

Usage:
    python tools/nif/generate_original_fantasy_assets.py
    python tools/nif/generate_original_fantasy_assets.py --output build/fantasy_nifs
"""
from __future__ import annotations

import argparse
import json
import math
import shutil
import struct
import zipfile
from pathlib import Path


class Mesh:
    def __init__(self) -> None:
        self.vertices: list[tuple[float, float, float]] = []
        self.normals: list[tuple[float, float, float]] = []
        self.triangles: list[tuple[int, int, int]] = []

    def tri(self, a, b, c, normal=None) -> None:
        if normal is None:
            ux, uy, uz = b[0]-a[0], b[1]-a[1], b[2]-a[2]
            vx, vy, vz = c[0]-a[0], c[1]-a[1], c[2]-a[2]
            nx, ny, nz = uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx
            length = math.sqrt(nx*nx + ny*ny + nz*nz) or 1.0
            normal = (nx/length, ny/length, nz/length)
        start = len(self.vertices)
        self.vertices.extend((a, b, c))
        self.normals.extend((normal, normal, normal))
        self.triangles.append((start, start+1, start+2))

    def quad(self, a, b, c, d) -> None:
        ux, uy, uz = b[0]-a[0], b[1]-a[1], b[2]-a[2]
        vx, vy, vz = c[0]-a[0], c[1]-a[1], c[2]-a[2]
        nx, ny, nz = uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx
        length = math.sqrt(nx*nx + ny*ny + nz*nz) or 1.0
        normal = (nx/length, ny/length, nz/length)
        start = len(self.vertices)
        self.vertices.extend((a, b, c, d))
        self.normals.extend((normal, normal, normal, normal))
        self.triangles.extend(((start, start+1, start+2), (start, start+2, start+3)))


def part(parts, material):
    return parts.setdefault(material, Mesh())


def box(mesh, cx, cy, cz, sx, sy, sz) -> None:
    x0, x1 = cx-sx/2, cx+sx/2
    y0, y1 = cy-sy/2, cy+sy/2
    z0, z1 = cz-sz/2, cz+sz/2
    mesh.quad((x0,y0,z0),(x0,y1,z0),(x1,y1,z0),(x1,y0,z0))
    mesh.quad((x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1))
    mesh.quad((x0,y0,z0),(x1,y0,z0),(x1,y0,z1),(x0,y0,z1))
    mesh.quad((x0,y1,z0),(x0,y1,z1),(x1,y1,z1),(x1,y1,z0))
    mesh.quad((x0,y0,z0),(x0,y0,z1),(x0,y1,z1),(x0,y1,z0))
    mesh.quad((x1,y0,z0),(x1,y1,z0),(x1,y1,z1),(x1,y0,z1))


def roof(mesh, cx, cy, eave_z, width, depth, height) -> None:
    x0, x1 = cx-width/2, cx+width/2
    y0, y1 = cy-depth/2, cy+depth/2
    apex_front = (cx,y0,eave_z+height)
    apex_back = (cx,y1,eave_z+height)
    lf, rf = (x0,y0,eave_z), (x1,y0,eave_z)
    lb, rb = (x0,y1,eave_z), (x1,y1,eave_z)
    mesh.tri(lf, rf, apex_front)
    mesh.tri(lb, apex_back, rb)
    mesh.quad(lf, apex_front, apex_back, lb)
    mesh.quad(rf, rb, apex_back, apex_front)
    mesh.quad(lf, lb, rb, rf)


def cylinder(mesh, cx, cy, z0, radius, height, sides=12, radius_top=None) -> None:
    radius_top = radius if radius_top is None else radius_top
    z1 = z0 + height
    for i in range(sides):
        a = 2*math.pi*i/sides
        b = 2*math.pi*(i+1)/sides
        p0=(cx+radius*math.cos(a), cy+radius*math.sin(a), z0)
        p1=(cx+radius*math.cos(b), cy+radius*math.sin(b), z0)
        q0=(cx+radius_top*math.cos(a), cy+radius_top*math.sin(a), z1)
        q1=(cx+radius_top*math.cos(b), cy+radius_top*math.sin(b), z1)
        mesh.quad(p0,p1,q1,q0)
        mesh.tri((cx,cy,z0),p1,p0,(0,0,-1))
        mesh.tri((cx,cy,z1),q0,q1,(0,0,1))


def sphere(mesh, cx, cy, cz, radius, segments=10, rings=5) -> None:
    points=[]
    for j in range(rings+1):
        phi=math.pi*j/rings
        zz=cz+radius*math.cos(phi)
        rr=radius*math.sin(phi)
        row=[]
        for i in range(segments):
            angle=2*math.pi*i/segments
            row.append((cx+rr*math.cos(angle),cy+rr*math.sin(angle),zz))
        points.append(row)
    for j in range(rings):
        for i in range(segments):
            k=(i+1)%segments
            a,b=points[j][i],points[j][k]
            c,d=points[j+1][k],points[j+1][i]
            if j==0:
                mesh.tri(a,d,c)
            elif j==rings-1:
                mesh.tri(a,d,b)
            else:
                mesh.quad(a,d,c,b)


def beam_xz(mesh, x1,z1,x2,z2,yc,depth,width) -> None:
    dx,dz=x2-x1,z2-z1
    length=math.sqrt(dx*dx+dz*dz) or 1.0
    px,pz=-dz/length*width/2,dx/length*width/2
    front,back=yc-depth/2,yc+depth/2
    p1=(x1+px,front,z1+pz); p2=(x2+px,front,z2+pz)
    p3=(x2-px,front,z2-pz); p4=(x1-px,front,z1-pz)
    q1=(p1[0],back,p1[2]); q2=(p2[0],back,p2[2])
    q3=(p3[0],back,p3[2]); q4=(p4[0],back,p4[2])
    mesh.quad(p1,p2,p3,p4); mesh.quad(q4,q3,q2,q1)
    mesh.quad(p1,q1,q2,p2); mesh.quad(p2,q2,q3,p3)
    mesh.quad(p3,q3,q4,p4); mesh.quad(p4,q4,q1,p1)


def pyramid(mesh,cx,cy,z0,sx,sy,height) -> None:
    x0,x1=cx-sx/2,cx+sx/2; y0,y1=cy-sy/2,cy+sy/2
    apex=(cx,cy,z0+height)
    p=[(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0)]
    mesh.quad(p[0],p[3],p[2],p[1])
    mesh.tri(p[0],p[1],apex); mesh.tri(p[1],p[2],apex)
    mesh.tri(p[2],p[3],apex); mesh.tri(p[3],p[0],apex)


COLORS={
    'plaster':(0.94,0.82,0.64),'wood':(0.36,0.16,0.055),'wood_light':(0.62,0.31,0.10),
    'roof_red':(0.72,0.10,0.07),'roof_blue':(0.12,0.25,0.72),'stone':(0.48,0.51,0.53),
    'stone_light':(0.67,0.68,0.65),'window':(1.00,0.62,0.08),'green':(0.16,0.52,0.10),
    'green_light':(0.43,0.72,0.15),'flower_pink':(0.95,0.22,0.46),
    'flower_white':(0.98,0.95,0.84),'flower_purple':(0.52,0.22,0.78),
    'metal':(0.12,0.13,0.16),'cloth_red':(0.82,0.14,0.10),'cloth_blue':(0.12,0.30,0.82),
    'cloth_gold':(0.95,0.58,0.08),'cloth_cream':(0.98,0.84,0.57),
    'produce_red':(0.85,0.08,0.04),'produce_green':(0.28,0.62,0.08),
    'produce_orange':(0.95,0.38,0.03),
}


def house_base(roof_material='roof_red', porch=False):
    p={}
    box(part(p,'stone'),0,0,25,540,410,50)
    box(part(p,'plaster'),0,0,215,500,380,350)
    roof(part(p,roof_material),0,0,365,580,445,205)
    roof(part(p,roof_material),0,0,350,600,455,34)
    for x in (-235,235): box(part(p,'wood'),x,-196,225,28,24,340)
    box(part(p,'wood'),0,-198,348,500,24,26)
    box(part(p,'wood'),0,-198,125,500,24,24)
    beam_xz(part(p,'wood'),-245,360,0,550,-198,25,24)
    beam_xz(part(p,'wood'),0,550,245,360,-198,25,24)
    box(part(p,'wood_light'),0,-203,120,118,28,220)
    box(part(p,'wood'),-67,-218,122,18,18,238); box(part(p,'wood'),67,-218,122,18,18,238)
    box(part(p,'wood'),0,-218,236,152,18,18)
    for x in (-155,155):
        box(part(p,'window'),x,-204,245,76,16,92)
        box(part(p,'wood'),x,-214,245,9,18,98); box(part(p,'wood'),x,-214,245,82,18,9)
        box(part(p,'wood_light'),x,-226,187,105,34,34)
        for dx,mat in ((-30,'flower_white'),(0,'flower_pink'),(30,'flower_purple')):
            sphere(part(p,mat),x+dx,-232,217,15,8,4)
        sphere(part(p,'green'),x-18,-225,211,18,8,4); sphere(part(p,'green'),x+20,-225,211,18,8,4)
    box(part(p,'stone_light'),-160,85,505,78,80,205); box(part(p,'stone'),-160,85,615,92,94,28)
    box(part(p,'stone_light'),0,-255,32,190,90,32); box(part(p,'stone_light'),0,-226,57,155,68,26)
    if porch:
        box(part(p,'wood'),185,-255,105,18,160,210); box(part(p,'wood'),330,-255,105,18,160,210)
        for i in range(5):
            box(part(p,'cloth_blue' if i%2==0 else 'cloth_cream'),257+(i-2)*34,-278,220,34,130,16)
        box(part(p,'wood_light'),255,-275,82,185,90,20)
    return p


def general_store():
    p={}
    box(part(p,'stone'),0,0,30,780,500,60); box(part(p,'plaster'),0,0,285,740,470,500)
    roof(part(p,'roof_red'),0,0,525,850,560,250); roof(part(p,'roof_red'),200,-205,465,360,170,165)
    for x in (-350,-180,0,180,350): box(part(p,'wood'),x,-245,285,24,28,485)
    for z in (120,330,500): box(part(p,'wood'),0,-248,z,725,25,24)
    for x in (-220,0,220):
        box(part(p,'window'),x,-255,400,90,16,115); box(part(p,'wood'),x,-266,400,9,18,120); box(part(p,'wood'),x,-266,400,96,18,9)
    box(part(p,'wood_light'),0,-315,125,650,120,150)
    for i in range(10): box(part(p,'cloth_red' if i%2==0 else 'cloth_cream'),-315+i*70,-350,285,70,210,18)
    for x in (-325,325): box(part(p,'wood'),x,-350,180,26,28,280)
    for x,mat in ((-220,'produce_red'),(-70,'produce_green'),(80,'produce_orange'),(225,'produce_green')):
        box(part(p,'wood_light'),x,-395,150,120,70,45)
        for dx in (-35,0,35): sphere(part(p,mat),x+dx,-405,185,18,8,4)
    box(part(p,'wood'),0,-280,342,620,28,22)
    for x in (-240,-120,0,120,240):
        sphere(part(p,'green'),x,-292,365,26,8,4)
        sphere(part(p,'flower_pink' if (x//120)%2==0 else 'flower_white'),x+8,-300,385,12,8,4)
    box(part(p,'stone_light'),-280,120,620,90,95,210)
    return p


def market_stall():
    p={}
    for x in (-220,220):
        for y in (-90,90): box(part(p,'wood'),x,y,170,28,28,340)
    box(part(p,'wood_light'),0,0,115,470,210,45)
    for i in range(8): box(part(p,'cloth_gold' if i%2==0 else 'cloth_cream'),-210+i*60,0,355,60,250,24)
    for x,mat in ((-150,'produce_red'),(-50,'produce_green'),(50,'produce_orange'),(150,'produce_green')):
        box(part(p,'wood_light'),x,-45,155,85,95,35)
        for dx,dy in ((-18,-12),(15,-10),(-5,14),(20,18)): sphere(part(p,mat),x+dx,-45+dy,185,12,7,3)
    return p


def signpost():
    p={}; cylinder(part(p,'wood'),0,0,0,24,350,10); pyramid(part(p,'wood_light'),0,0,350,70,70,75)
    for z,mat,xoff in ((285,'cloth_blue',55),(220,'cloth_red',-50),(155,'green',45)):
        box(part(p,mat),xoff,-8,z,220,28,52); box(part(p,'wood'),xoff,-5,z,230,18,10)
    return p


def lantern():
    p={}; cylinder(part(p,'wood'),0,0,0,22,330,10); beam_xz(part(p,'metal'),0,310,105,350,0,24,18)
    box(part(p,'metal'),120,0,300,80,65,16); box(part(p,'window'),120,0,255,62,54,78)
    pyramid(part(p,'metal'),120,0,294,82,72,48); pyramid(part(p,'metal'),120,0,205,70,60,35)
    return p


def stone_well():
    p={}; cylinder(part(p,'stone'),0,0,0,150,120,16); cylinder(part(p,'stone_light'),0,0,105,165,35,16)
    cylinder(part(p,'metal'),0,0,138,118,6,16)
    for x in (-125,125): box(part(p,'wood'),x,0,260,30,34,300)
    roof(part(p,'roof_blue'),0,0,385,390,270,130); box(part(p,'wood_light'),0,0,275,285,24,24)
    cylinder(part(p,'wood_light'),70,-15,145,35,55,10,28)
    return p


def barrel():
    p={}; cylinder(part(p,'wood_light'),0,0,0,92,210,14,80)
    for z in (22,100,188): cylinder(part(p,'metal'),0,0,z,96,10,14,94)
    return p


def crate_stack():
    p={}
    for cx,cy,cz,size in ((-80,0,70,140),(75,20,60,120),(35,0,175,115)):
        box(part(p,'wood_light'),cx,cy,cz,size,size,size)
        beam_xz(part(p,'wood'),cx-size*.38,cz-size*.38,cx+size*.38,cz+size*.38,cy-size/2-4,10,10)
        beam_xz(part(p,'wood'),cx+size*.38,cz-size*.38,cx-size*.38,cz+size*.38,cy-size/2-4,10,10)
    return p


def bench():
    p={}
    for z in (105,145,185): box(part(p,'wood_light'),0,0,z,390,36,28)
    box(part(p,'wood_light'),0,65,65,390,115,28)
    for x in (-165,165):
        box(part(p,'metal'),x,35,65,28,32,125); beam_xz(part(p,'metal'),x,40,x+(-35 if x<0 else 35),5,35,28,22)
    return p


def fence():
    p={}
    for x in (-190,0,190): box(part(p,'wood'),x,0,120,42,48,240); pyramid(part(p,'wood_light'),x,0,240,55,60,45)
    for z in (85,165): box(part(p,'wood_light'),0,0,z,410,34,34)
    return p


def flower_planter():
    p={}; box(part(p,'wood_light'),0,0,55,330,145,110); box(part(p,'wood'),0,-76,55,345,15,22)
    xs=(-120,-80,-40,0,40,80,120); mats=('flower_white','flower_pink','flower_purple')
    for i,x in enumerate(xs):
        sphere(part(p,'green'),x,0,125,35,8,4); sphere(part(p,mats[i%3]),x,-25 if i%2 else 20,155+(i%2)*10,16,8,4)
    return p


def tree():
    p={}; cylinder(part(p,'wood'),0,0,0,85,410,12,48)
    for x2,z2 in ((-150,10),(150,10),(-100,25),(110,20)): beam_xz(part(p,'wood'),0,35,x2,z2,0,70,38)
    for cx,cy,cz,r,mat in ((-125,0,430,140,'green'),(0,0,485,165,'green_light'),(130,0,430,145,'green'),(-70,70,545,120,'green_light'),(85,65,545,120,'green'),(0,-65,555,120,'green_light')):
        sphere(part(p,mat),cx,cy,cz,r,10,5)
    return p


ASSETS={
    'NG_SmallHouse_A':lambda:house_base('roof_red',False),
    'NG_SmallHouse_B':lambda:house_base('roof_blue',True),
    'NG_GeneralStore':general_store,
    'NG_MarketStall':market_stall,
    'NG_Signpost':signpost,
    'NG_LanternStreetlamp':lantern,
    'NG_StoneWell':stone_well,
    'NG_Barrel':barrel,
    'NG_CrateStack':crate_stack,
    'NG_Bench':bench,
    'NG_FenceSegment':fence,
    'NG_FlowerPlanter':flower_planter,
    'NG_StylizedTree':tree,
}


def u8(v): return struct.pack('<B',v)
def u16(v): return struct.pack('<H',v)
def u32(v): return struct.pack('<I',v)
def i32(v): return struct.pack('<i',v)
def f32(v): return struct.pack('<f',float(v))
def sstr(value):
    data=value.encode('ascii','replace')
    return u32(len(data))+data


def objectnet(name):
    return sstr(name)+u32(0)+i32(-1)


def avobject(name, properties):
    data=bytearray(objectnet(name)); data+=u16(0x000E)
    data+=f32(0)+f32(0)+f32(0)
    for value in (1,0,0,0,1,0,0,0,1): data+=f32(value)
    data+=f32(1)+u32(len(properties))
    for ref in properties: data+=i32(ref)
    data+=i32(-1)
    return bytes(data)


def write_nif(path, asset_name, parts):
    groups=[(name,mesh) for name,mesh in parts.items() if mesh.vertices and mesh.triangles]
    block_count=1+3*len(groups)
    types=('NiNode','NiTriShape','NiMaterialProperty','NiTriShapeData')
    block_types=[0]
    for _ in groups: block_types.extend((1,2,3))
    shape_refs=[1+3*i for i in range(len(groups))]

    out=bytearray(b'Gamebryo File Format, Version 20.0.0.4\n')
    out+=u32(0x14000004)+u8(1)+u32(0)+u32(block_count)+u16(len(types))
    for type_name in types: out+=sstr(type_name)
    for type_index in block_types: out+=u16(type_index)
    out+=u32(0)

    root=bytearray(avobject(asset_name+'_Root',[]))+u32(len(shape_refs))
    for ref in shape_refs: root+=i32(ref)
    root+=u32(0); out+=root

    for index,(material,mesh) in enumerate(groups):
        shape_ref=1+3*index; material_ref=shape_ref+1; data_ref=shape_ref+2
        shape=bytearray(avobject(asset_name+'_'+material,[material_ref]))
        shape+=i32(data_ref)+i32(-1)+u8(0)
        out+=shape

        rgb=COLORS.get(material,(0.7,0.7,0.7)); ambient=tuple(v*.62 for v in rgb); emissive=tuple(v*.035 for v in rgb)
        mat=bytearray(objectnet('Mat_'+material))
        for vector in (ambient,rgb,(.10,.10,.10),emissive):
            for value in vector: mat+=f32(value)
        mat+=f32(8.0)+f32(1.0); out+=mat

        nv=len(mesh.vertices); nt=len(mesh.triangles)
        if nv>65535 or nt>65535: raise ValueError(f'{asset_name}/{material} exceeds uint16 geometry limits')
        geom=bytearray(u32(0)+u16(nv)+u8(0)+u8(0)+u8(1))
        for x,y,z in mesh.vertices: geom+=f32(x)+f32(y)+f32(z)
        geom+=u16(0)+u8(1)
        for x,y,z in mesh.normals: geom+=f32(x)+f32(y)+f32(z)
        xs=[v[0] for v in mesh.vertices]; ys=[v[1] for v in mesh.vertices]; zs=[v[2] for v in mesh.vertices]
        cx=(min(xs)+max(xs))/2; cy=(min(ys)+max(ys))/2; cz=(min(zs)+max(zs))/2
        radius=max(math.dist((cx,cy,cz),v) for v in mesh.vertices)
        geom+=f32(cx)+f32(cy)+f32(cz)+f32(radius)+u8(0)
        geom+=u16(0)+i32(-1)+u16(nt)+u32(nt*3)+u8(1)
        for a,b,c in mesh.triangles: geom+=u16(a)+u16(b)+u16(c)
        geom+=u16(0); out+=geom

    out+=u32(1)+i32(0)
    path.write_bytes(out)
    return {'file':path.name,'bytes':len(out),'parts':len(groups),'vertices':sum(len(m.vertices) for _,m in groups),'triangles':sum(len(m.triangles) for _,m in groups),'materials':[name for name,_ in groups]}


def validate_header(path):
    data=path.read_bytes(); expected=b'Gamebryo File Format, Version 20.0.0.4\n'
    if not data.startswith(expected): raise ValueError(f'{path.name}: bad NIF signature')
    version=struct.unpack_from('<I',data,len(expected))[0]
    if version!=0x14000004: raise ValueError(f'{path.name}: wrong version {version:#x}')
    if len(data)<128: raise ValueError(f'{path.name}: unexpectedly short')


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,default=Path('build/original_fantasy_nifs'))
    args=parser.parse_args()
    out_dir=args.output
    if out_dir.exists(): shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)
    manifest=[]
    for name,builder in ASSETS.items():
        entry=write_nif(out_dir/(name+'.nif'),name,builder()); validate_header(out_dir/entry['file']); manifest.append(entry)
    (out_dir/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    (out_dir/'README.txt').write_text(
        'Original NextGen fantasy-village assets\n'
        'Gamebryo NIF 20.0.0.4 / static NiTriShape / Z-up / no external textures required.\n'
        'Generated geometry is original and intended as a clean renderer/game compatibility baseline.\n',
        encoding='utf-8')
    archive=out_dir.parent/'NextGen_FiestaStyle_NIF_AssetPack_v1.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as zf:
        for item in sorted(out_dir.iterdir()): zf.write(item,item.name)
    print(f'Generated {len(manifest)} NIF assets in {out_dir}')
    print(f'Archive: {archive}')
    for entry in manifest:
        print(f"{entry['file']}: {entry['parts']} parts, {entry['vertices']} vertices, {entry['triangles']} triangles")


if __name__=='__main__':
    main()
