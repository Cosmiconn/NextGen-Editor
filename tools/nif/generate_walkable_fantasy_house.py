#!/usr/bin/env python3
"""Generate an original production-quality walkable fantasy house NIF.

The asset is deliberately original.  It follows the colourful, rounded visual language of
classic anime MMORPG villages without copying Fiesta geometry or textures.

Output:
  NG_WalkableHouse_A.nif
  textures/*.dds
  manifest.json / README.txt
  NextGen_WalkableHouse_A_Production.zip

The NIF targets Gamebryo 20.0.0.4, Z-up coordinates and the subset consumed by
src/core/NifModel.cpp.  The scene is split into a visible #M node and a simplified #CD
collision node.  Visible and collision children carry matching #M/#CD suffixes so the
intent survives tools that flatten the hierarchy.
"""
from __future__ import annotations

import argparse
import json
import math
import shutil
import struct
import zipfile
from pathlib import Path


# ---------------------------------------------------------------------------
# Geometry
# ---------------------------------------------------------------------------

class Mesh:
    def __init__(self) -> None:
        self.vertices: list[tuple[float, float, float]] = []
        self.normals: list[tuple[float, float, float]] = []
        self.uvs: list[tuple[float, float]] = []
        self.triangles: list[tuple[int, int, int]] = []

    @staticmethod
    def _normal(a, b, c):
        ux, uy, uz = b[0]-a[0], b[1]-a[1], b[2]-a[2]
        vx, vy, vz = c[0]-a[0], c[1]-a[1], c[2]-a[2]
        nx, ny, nz = uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx
        n = math.sqrt(nx*nx + ny*ny + nz*nz) or 1.0
        return nx/n, ny/n, nz/n

    def tri(self, a, b, c, uv=((0, 0), (1, 0), (0.5, 1)), normal=None) -> None:
        normal = normal or self._normal(a, b, c)
        i = len(self.vertices)
        self.vertices.extend((a, b, c))
        self.normals.extend((normal, normal, normal))
        self.uvs.extend(uv)
        self.triangles.append((i, i+1, i+2))

    def quad(self, a, b, c, d, uv=((0, 0), (1, 0), (1, 1), (0, 1))) -> None:
        normal = self._normal(a, b, c)
        i = len(self.vertices)
        self.vertices.extend((a, b, c, d))
        self.normals.extend((normal, normal, normal, normal))
        self.uvs.extend(uv)
        self.triangles.extend(((i, i+1, i+2), (i, i+2, i+3)))


def part(parts: dict[str, Mesh], name: str) -> Mesh:
    return parts.setdefault(name, Mesh())


def box(mesh: Mesh, cx, cy, cz, sx, sy, sz, tile=1.0) -> None:
    x0, x1 = cx-sx/2, cx+sx/2
    y0, y1 = cy-sy/2, cy+sy/2
    z0, z1 = cz-sz/2, cz+sz/2
    uv=((0,0),(tile,0),(tile,tile),(0,tile))
    mesh.quad((x0,y0,z0),(x0,y1,z0),(x1,y1,z0),(x1,y0,z0),uv)
    mesh.quad((x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1),uv)
    mesh.quad((x0,y0,z0),(x1,y0,z0),(x1,y0,z1),(x0,y0,z1),uv)
    mesh.quad((x0,y1,z0),(x0,y1,z1),(x1,y1,z1),(x1,y1,z0),uv)
    mesh.quad((x0,y0,z0),(x0,y0,z1),(x0,y1,z1),(x0,y1,z0),uv)
    mesh.quad((x1,y0,z0),(x1,y1,z0),(x1,y1,z1),(x1,y0,z1),uv)


def box_rot_z(mesh: Mesh, cx, cy, cz, sx, sy, sz, angle_deg, tile=1.0) -> None:
    tmp = Mesh(); box(tmp, 0, 0, 0, sx, sy, sz, tile)
    a=math.radians(angle_deg); ca,sa=math.cos(a),math.sin(a)
    base=len(mesh.vertices)
    for x,y,z in tmp.vertices:
        mesh.vertices.append((cx+x*ca-y*sa, cy+x*sa+y*ca, cz+z))
    for x,y,z in tmp.normals:
        mesh.normals.append((x*ca-y*sa, x*sa+y*ca, z))
    mesh.uvs.extend(tmp.uvs)
    mesh.triangles.extend((base+a0,base+b0,base+c0) for a0,b0,c0 in tmp.triangles)


def beam_xz(mesh: Mesh, x1,z1,x2,z2,yc,depth,width,tile=1.0) -> None:
    dx,dz=x2-x1,z2-z1; length=math.hypot(dx,dz) or 1.0
    px,pz=-dz/length*width/2,dx/length*width/2
    f,b=yc-depth/2,yc+depth/2
    p1=(x1+px,f,z1+pz); p2=(x2+px,f,z2+pz); p3=(x2-px,f,z2-pz); p4=(x1-px,f,z1-pz)
    q1=(p1[0],b,p1[2]); q2=(p2[0],b,p2[2]); q3=(p3[0],b,p3[2]); q4=(p4[0],b,p4[2])
    uv=((0,0),(tile,0),(tile,1),(0,1))
    mesh.quad(p1,p2,p3,p4,uv); mesh.quad(q4,q3,q2,q1,uv)
    mesh.quad(p1,q1,q2,p2,uv); mesh.quad(p2,q2,q3,p3,uv)
    mesh.quad(p3,q3,q4,p4,uv); mesh.quad(p4,q4,q1,p1,uv)


def cylinder(mesh: Mesh, cx,cy,z0,radius,height,sides=12,radius_top=None) -> None:
    rt=radius if radius_top is None else radius_top; z1=z0+height
    for i in range(sides):
        a=2*math.pi*i/sides; b=2*math.pi*(i+1)/sides
        p0=(cx+radius*math.cos(a),cy+radius*math.sin(a),z0)
        p1=(cx+radius*math.cos(b),cy+radius*math.sin(b),z0)
        q0=(cx+rt*math.cos(a),cy+rt*math.sin(a),z1)
        q1=(cx+rt*math.cos(b),cy+rt*math.sin(b),z1)
        mesh.quad(p0,p1,q1,q0,((i/sides,0),((i+1)/sides,0),((i+1)/sides,1),(i/sides,1)))
        mesh.tri((cx,cy,z0),p1,p0,normal=(0,0,-1)); mesh.tri((cx,cy,z1),q0,q1,normal=(0,0,1))


def sphere(mesh: Mesh,cx,cy,cz,radius,segments=10,rings=5,flatten=1.0) -> None:
    pts=[]
    for j in range(rings+1):
        phi=math.pi*j/rings; z=cz+radius*math.cos(phi)*flatten; rr=radius*math.sin(phi)
        row=[]
        for i in range(segments):
            a=2*math.pi*i/segments; row.append((cx+rr*math.cos(a),cy+rr*math.sin(a),z))
        pts.append(row)
    for j in range(rings):
        for i in range(segments):
            k=(i+1)%segments; a,b=pts[j][i],pts[j][k]; c,d=pts[j+1][k],pts[j+1][i]
            if j==0: mesh.tri(a,d,c)
            elif j==rings-1: mesh.tri(a,d,b)
            else: mesh.quad(a,d,c,b)


def curved_roof(mesh: Mesh,cx,cy,eave_z,width,depth,height,segments=12) -> None:
    half=width/2; y0,y1=cy-depth/2,cy+depth/2
    for side in (-1,1):
        pts=[]
        for i in range(segments+1):
            t=i/segments
            x=cx+side*half*t
            # high rounded crown with a small kicked-up eave
            z=eave_z+height*(1.0-t**0.72)+22.0*t**8
            pts.append((x,z))
        for i in range(segments):
            (xa,za),(xb,zb)=pts[i],pts[i+1]
            mesh.quad((xa,y0,za),(xb,y0,zb),(xb,y1,zb),(xa,y1,za),
                      ((0,i/3),(1,i/3),(1,(i+1)/3),(0,(i+1)/3)))


def gable(mesh: Mesh, y, width, eave_z, height, front=True) -> None:
    d=-1 if front else 1
    a=(-width/2,y,eave_z); b=(width/2,y,eave_z); c=(0,y,eave_z+height)
    if d<0: mesh.tri(a,c,b,((0,0),(0.5,1),(1,0)))
    else: mesh.tri(a,b,c,((0,0),(1,0),(0.5,1)))


def stairs(mesh: Mesh, x0,y0,z0,width,run,rise,count,axis='y') -> None:
    for i in range(count):
        depth=run/count; h=rise*(i+1)/count
        if axis=='y': box(mesh,x0,y0+i*depth+depth/2,z0+h/2,width,depth,h,1)
        else: box(mesh,x0+i*depth+depth/2,y0,z0+h/2,depth,width,h,1)


# ---------------------------------------------------------------------------
# Procedural original textures (uncompressed BGRA DDS, no third-party libs)
# ---------------------------------------------------------------------------

TEXTURES={
    'plaster':(224,196,148),'timber':(101,48,18),'roof_red':(176,47,28),'stone':(133,139,139),
    'floor':(137,76,31),'door':(121,59,22),'metal':(48,50,54),'glass':(255,170,45),
    'cloth':(193,49,38),'foliage':(70,137,31),'flower':(230,91,142),'interior':(165,94,39),
}


def clamp(v): return max(0,min(255,int(v)))
def noise(x,y,seed): return (((x*73856093)^(y*19349663)^(seed*83492791)) & 255)-128


def texel(kind,x,y):
    base=TEXTURES[kind]; n=noise(x,y,list(TEXTURES).index(kind)+1)
    r,g,b=base; delta=n*0.055
    r+=delta; g+=delta; b+=delta
    if kind in ('timber','door','floor','interior'):
        grain=(math.sin((x if kind!='floor' else y)*0.17)+math.sin((x+y)*0.045))*8
        r+=grain; g+=grain*.55; b+=grain*.25
        if kind=='floor' and y%42<3: r*=.55; g*=.55; b*=.55
        if kind!='floor' and x%64<3: r*=.68; g*=.68; b*=.68
    elif kind=='roof_red':
        row=y//32; xx=(x+(row%2)*16)%64
        if y%32<3 or xx<3: r*=.55; g*=.55; b*=.55
        elif y%32>25: r*=.83; g*=.83; b*=.83
    elif kind=='stone':
        row=y//48; xx=(x+(row%2)*32)%64
        if y%48<4 or xx<4: r*=.58; g*=.58; b*=.58
        else:
            shade=((x//64+y//48)*13)%17-8; r+=shade; g+=shade; b+=shade
    elif kind=='plaster':
        if ((x*7+y*11)%251)<2: r-=22; g-=20; b-=18
    elif kind=='metal':
        h=18*math.sin(x*.08); r+=h; g+=h; b+=h
    elif kind=='glass':
        glow=28*(1-abs(x-128)/128); r+=glow; g+=glow*.55
        if abs(x-y)<4 or abs((255-x)-y)<4: r+=30; g+=30; b+=30
    elif kind=='cloth':
        if (x//32)%2: r+=35; g+=35; b+=28
        if y%32<2: r*=.8; g*=.8; b*=.8
    elif kind=='foliage':
        if ((x//16+y//16)&1): g+=18; r-=8
    elif kind=='flower':
        if ((x-128)**2+(y-128)**2)<55**2: r+=18; g+=12; b+=12
    return clamp(r),clamp(g),clamp(b),255


def write_dds(path: Path, kind: str, size=256) -> None:
    flags=0x0000100F
    header=bytearray()
    header+=struct.pack('<I',124)+struct.pack('<I',flags)
    header+=struct.pack('<I',size)+struct.pack('<I',size)+struct.pack('<I',size*4)
    header+=struct.pack('<I',0)+struct.pack('<I',0)+bytes(44)
    # DDS_PIXELFORMAT: BGRA8888
    header+=struct.pack('<IIIIIIII',32,0x41,0,32,0x00FF0000,0x0000FF00,0x000000FF,0xFF000000)
    header+=struct.pack('<IIIII',0x1000,0,0,0,0)
    assert len(header)==124
    pixels=bytearray()
    for y in range(size):
        for x in range(size):
            r,g,b,a=texel(kind,x,y); pixels+=bytes((b,g,r,a))
    path.write_bytes(b'DDS '+header+pixels)


# ---------------------------------------------------------------------------
# House construction
# ---------------------------------------------------------------------------

def build_house():
    m: dict[str,Mesh]={}; cd: dict[str,Mesh]={}
    P=lambda n:part(m,n)
    C=lambda n:part(cd,n)

    # Foundation, walkable ground floor and shell with a real doorway opening.
    box(P('stone'),0,0,28,820,680,56,3)
    box(P('floor'),0,0,62,742,602,24,5)
    wall_z=286; wall_h=430
    box(P('plaster'),-380,0,wall_z,40,600,wall_h,3); box(P('plaster'),380,0,wall_z,40,600,wall_h,3)
    box(P('plaster'),0,290,wall_z,720,40,wall_h,4)
    box(P('plaster'),-220,-290,wall_z,280,40,wall_h,3); box(P('plaster'),220,-290,wall_z,280,40,wall_h,3)
    box(P('plaster'),0,-290,454,160,40,94,1)
    gable(P('plaster'),-311,720,501,178,True); gable(P('plaster'),311,720,501,178,False)

    # Exterior timber frame and irregular story line.
    for x in (-360,-225,225,360): box(P('timber'),x,-314,292,28,30,420,3)
    for x in (-360,360): box(P('timber'),x,0,292,32,625,430,4)
    for z in (105,318,492): box(P('timber'),0,-316,z,730,28,24,5)
    for z in (105,318,492): box(P('timber'),0,312,z,730,28,24,5)
    for y in (-190,0,190):
        box(P('timber'),-382,y,310,28,185,24,2); box(P('timber'),382,y,310,28,185,24,2)
    for y in (-316,312):
        beam_xz(P('timber'),-352,330,-90,492,y,30,24,3); beam_xz(P('timber'),90,492,352,330,y,30,24,3)
        beam_xz(P('timber'),-352,120,-210,305,y,30,22,2); beam_xz(P('timber'),352,120,210,305,y,30,22,2)

    # Swept roof, ridge and carved-looking edge trims.
    curved_roof(P('roof_red'),0,0,490,860,700,215,14)
    box(P('timber'),0,0,699,34,720,32,6)
    for y in (-351,351):
        beam_xz(P('timber'),0,696,-430,512,y,34,28,4); beam_xz(P('timber'),0,696,430,512,y,34,28,4)
    for x in (-430,430): box(P('timber'),x,0,512,24,720,28,5)

    # Chimney with cap courses.
    box(P('stone'),-245,115,613,92,100,250,2); box(P('stone'),-245,115,744,116,124,24,1)
    box(P('stone'),-245,115,770,102,110,28,1)

    # Open arched-ish front doorway: frame + door leaf rotated inward.
    for x in (-92,92): box(P('timber'),x,-331,191,25,34,265,2)
    box(P('timber'),0,-331,324,210,34,28,2)
    cylinder(P('timber'),0,-331,306,105,26,14,105)
    box_rot_z(P('door'),-78,-252,190,150,24,255,72,3)
    cylinder(P('metal'),-72,-321,198,7,8,10); cylinder(P('metal'),-72,-321,263,7,8,10)

    # Windows: warm glass, substantial wooden surrounds and sills.
    for x in (-235,235):
        box(P('glass'),x,-317,278,108,10,122,1)
        box(P('timber'),x-62,-326,278,16,18,148,1); box(P('timber'),x+62,-326,278,16,18,148,1)
        box(P('timber'),x,-326,209,140,18,16,1); box(P('timber'),x,-326,347,140,18,16,1)
        box(P('timber'),x,-327,278,12,19,135,1); box(P('timber'),x,-327,278,124,19,12,1)
        box(P('interior'),x,-350,194,155,45,34,1)
        for dx in (-45,-15,18,46):
            sphere(P('foliage'),x+dx,-360,224,25,8,4,.7); sphere(P('flower'),x+dx+6,-369,240+(dx%2)*5,11,8,4,.55)

    # Side windows.
    for sx in (-1,1):
        x=391*sx
        for y in (-120,130):
            box(P('glass'),x,y,282,10,112,118,1)
            box(P('timber'),x+5*sx,y-64,282,18,16,142,1); box(P('timber'),x+5*sx,y+64,282,18,16,142,1)
            box(P('timber'),x+5*sx,y,216,18,140,16,1); box(P('timber'),x+5*sx,y,348,18,140,16,1)

    # Small dormer on the front roof.
    box(P('plaster'),120,-266,570,158,112,128,1); box(P('glass'),120,-326,582,72,8,82,1)
    for x in (76,164): box(P('timber'),x,-331,582,14,15,104,1)
    box(P('timber'),120,-331,535,104,15,14,1); box(P('timber'),120,-331,629,104,15,14,1)
    curved_roof(P('roof_red'),120,-272,637,205,160,88,7)

    # Striped shop-like awning over right front window.
    for i in range(7):
        mat='cloth' if i%2==0 else 'plaster'
        box(P(mat),170+i*22,-374,385,22,112,14,1)
    box(P('timber'),238,-327,398,188,18,16,1)

    # Exterior lantern, barrels and a small signboard.
    box(P('metal'),-142,-357,372,12,70,12,1); box(P('metal'),-142,-389,337,64,54,10,1)
    box(P('glass'),-142,-389,305,48,42,64,1); box(P('metal'),-142,-389,270,58,50,10,1)
    for bx in (-318,315):
        cylinder(P('interior'),bx,-340,60,52,118,12,46)
        for z in (72,118,164): cylinder(P('metal'),bx,-340,z,55,7,12,53)
    box(P('timber'),315,-392,270,18,18,205,1); box(P('door'),315,-399,340,128,18,78,1)

    # Front steps (visual) and stone edging.
    box(P('stone'),0,-353,47,220,78,30,2); box(P('stone'),0,-390,30,270,74,24,2); box(P('stone'),0,-423,16,315,58,16,2)

    # Interior loft, stairs and railing.  The front half stays double-height.
    box(P('floor'),0,142,342,720,286,22,4)
    stairs(P('interior'),265,-155,74,118,365,268,11,'y')
    for y in (85,145,205,265): box(P('timber'),206,y,430,18,18,168,1)
    box(P('timber'),206,175,510,18,250,18,2); box(P('timber'),206,175,390,18,250,18,2)

    # Fireplace and chimney breast.
    box(P('stone'),-270,260,176,190,70,225,2); box(P('stone'),-270,218,155,120,20,128,1)
    box(P('metal'),-270,205,150,88,10,88,1)

    # Interior table + four chairs.
    box(P('interior'),0,15,142,250,125,22,2)
    for x in (-102,102):
        for y in (-43,43): box(P('interior'),x,y,100,22,22,84,1)
    for x,y in ((-175,10),(175,10),(0,-110),(0,135)):
        box(P('interior'),x,y,105,72,72,18,1); box(P('interior'),x,y+25,158,72,18,108,1)

    # Shelves and pantry props along rear wall.
    for x in (-70,80,230):
        box(P('timber'),x,250,210,16,28,250,1)
    for z in (105,180,255,325): box(P('interior'),80,250,z,330,42,18,2)
    for x in (-45,30,110,190):
        cylinder(P('interior'),x,225,117,18,42,10,15)

    # Loft bed/chest and rug.
    box(P('interior'),-165,165,392,250,120,34,2); box(P('cloth'),-165,145,420,232,95,20,1)
    box(P('interior'),-165,220,475,250,22,145,1); box(P('interior'),80,190,390,105,70,60,1)
    box(P('cloth'),0,20,78,310,205,8,3)

    # Interior hanging lamp.
    cylinder(P('metal'),0,0,485,8,80,8); cylinder(P('metal'),0,0,458,64,10,12,42)
    for a in range(0,360,90):
        rad=math.radians(a); x=math.cos(rad)*58; y=math.sin(rad)*58
        box(P('glass'),x,y,445,24,24,48,1)

    # Foliage around foundation gives the stylized silhouette from the concept.
    for x,y in ((-340,-350),(-285,-365),(280,-360),(345,-340),(-390,180),(390,205)):
        sphere(P('foliage'),x,y,105,38,9,5,.65)
        sphere(P('flower'),x+10,y-8,130,15,8,4,.5)

    # Simplified collision.  Door gap is intentionally kept clear.
    box(C('Floor'),0,0,50,740,600,20,1)
    box(C('Walls'),-378,0,285,36,600,430,1); box(C('Walls'),378,0,285,36,600,430,1)
    box(C('Walls'),0,288,285,720,36,430,1)
    box(C('Walls'),-220,-288,285,280,36,430,1); box(C('Walls'),220,-288,285,280,36,430,1)
    box(C('Walls'),0,-288,454,160,36,92,1)
    box(C('UpperFloor'),0,142,338,720,286,18,1)
    stairs(C('Stairs'),265,-155,62,118,365,276,11,'y')
    box(C('Fireplace'),-270,250,170,195,86,225,1)
    box(C('ExteriorSteps'),0,-353,47,220,78,30,1); box(C('ExteriorSteps'),0,-390,30,270,74,24,1); box(C('ExteriorSteps'),0,-423,16,315,58,16,1)
    return m,cd


# ---------------------------------------------------------------------------
# NIF 20.0.0.4 writer
# ---------------------------------------------------------------------------

def u8(v): return struct.pack('<B',v)
def u16(v): return struct.pack('<H',v)
def u32(v): return struct.pack('<I',v)
def i32(v): return struct.pack('<i',v)
def f32(v): return struct.pack('<f',float(v))
def sstr(value):
    data=value.encode('ascii','replace'); return u32(len(data))+data

def objectnet(name): return sstr(name)+u32(0)+i32(-1)

def avobject(name, properties=(), flags=0x000E):
    d=bytearray(objectnet(name)); d+=u16(flags)
    d+=f32(0)+f32(0)+f32(0)
    for v in (1,0,0,0,1,0,0,0,1): d+=f32(v)
    d+=f32(1)+u32(len(properties))
    for r in properties: d+=i32(r)
    d+=i32(-1)
    return bytes(d)

def node_bytes(name,children,flags=0x000E):
    d=bytearray(avobject(name,(),flags))+u32(len(children))
    for r in children: d+=i32(r)
    d+=u32(0)
    return bytes(d)

def shape_bytes(name,data_ref,properties,flags=0x000E):
    return avobject(name,properties,flags)+i32(data_ref)+i32(-1)+u8(0)

def material_bytes(name):
    d=bytearray(objectnet('Mat_'+name))
    # Textures carry colour; neutral material preserves authored DDS values.
    for vec in ((.55,.55,.55),(1,1,1),(.14,.14,.14),(0,0,0)):
        for v in vec: d+=f32(v)
    d+=f32(12)+f32(1)
    return bytes(d)

def texturing_bytes(name,source_ref):
    d=bytearray(objectnet('Tex_'+name)); d+=u32(2)+u32(1)  # MODULATE, one classic slot
    d+=u8(1)+i32(source_ref)+u32(3)+u32(2)+u32(0)+u8(0)  # WRAP/WRAP, trilinear, UV0, no transform
    d+=u32(0)  # num_shader_textures (mandatory since 10.0.1.0)
    return bytes(d)

def source_bytes(name,filename):
    d=bytearray(objectnet('Src_'+name)); d+=u8(1)+sstr(filename)+i32(-1)
    d+=u32(0)+u32(0)+u32(0)+u8(1)+u8(1)
    return bytes(d)

def data_bytes(mesh: Mesh):
    nv,nt=len(mesh.vertices),len(mesh.triangles)
    if nv>65535 or nt>65535: raise ValueError(f'geometry exceeds uint16 limits: {nv=} {nt=}')
    d=bytearray(u32(0)+u16(nv)+u8(0)+u8(0)+u8(1))
    for x,y,z in mesh.vertices: d+=f32(x)+f32(y)+f32(z)
    d+=u16(1)+u8(1)  # one UV set + normals
    for x,y,z in mesh.normals: d+=f32(x)+f32(y)+f32(z)
    xs=[v[0] for v in mesh.vertices]; ys=[v[1] for v in mesh.vertices]; zs=[v[2] for v in mesh.vertices]
    cx=(min(xs)+max(xs))/2; cy=(min(ys)+max(ys))/2; cz=(min(zs)+max(zs))/2
    radius=max(math.dist((cx,cy,cz),v) for v in mesh.vertices)
    d+=f32(cx)+f32(cy)+f32(cz)+f32(radius)+u8(0)
    for u,v in mesh.uvs: d+=f32(u)+f32(v)
    d+=u16(0)+i32(-1)+u16(nt)+u32(nt*3)+u8(1)
    for a,b,c in mesh.triangles: d+=u16(a)+u16(b)+u16(c)
    d+=u16(0)
    return bytes(d)


def write_nif(path: Path, visible: dict[str,Mesh], collision: dict[str,Mesh]):
    blocks=[]
    def add(kind,fn): blocks.append((kind,fn)); return len(blocks)-1
    visible_children=[]; collision_children=[]
    root_children=[]
    root=add('NiNode',lambda:node_bytes('NG_WalkableHouse_A_Root',root_children))
    mnode=add('NiNode',lambda:node_bytes('#M',visible_children,0x000E)); root_children.append(mnode)
    cdnode=add('NiNode',lambda:node_bytes('#CD',collision_children,0x000F)); root_children.append(cdnode)

    for name,mesh in visible.items():
        if not mesh.triangles: continue
        mat_ref=add('NiMaterialProperty',lambda n=name:material_bytes(n))
        src_ref=add('NiSourceTexture',lambda n=name:source_bytes(n,f'textures/NG_WalkableHouse_A_{n}.dds'))
        tex_ref=add('NiTexturingProperty',lambda n=name,s=src_ref:texturing_bytes(n,s))
        data_ref=add('NiTriShapeData',lambda me=mesh:data_bytes(me))
        shape_ref=add('NiTriShape',lambda n=name,d=data_ref,m=mat_ref,t=tex_ref:shape_bytes(f'{n}#M',d,(m,t),0x000E))
        visible_children.append(shape_ref)

    for name,mesh in collision.items():
        if not mesh.triangles: continue
        data_ref=add('NiTriShapeData',lambda me=mesh:data_bytes(me))
        # APP_CULLED bit on collision geometry keeps it non-rendering in Gamebryo while #CD
        # remains available to Fiesta collision discovery.
        shape_ref=add('NiTriShape',lambda n=name,d=data_ref:shape_bytes(f'{n}#CD',d,(),0x000F))
        collision_children.append(shape_ref)

    types=[]
    for kind,_ in blocks:
        if kind not in types: types.append(kind)
    out=bytearray(b'Gamebryo File Format, Version 20.0.0.4\n')
    out+=u32(0x14000004)+u8(1)+u32(0)+u32(len(blocks))+u16(len(types))
    for kind in types: out+=sstr(kind)
    for kind,_ in blocks: out+=u16(types.index(kind))
    out+=u32(0)
    for _,fn in blocks: out+=fn()
    out+=u32(1)+i32(root)
    path.write_bytes(out)
    return {
        'file':path.name,'bytes':len(out),'blocks':len(blocks),
        'visible_parts':len(visible_children),'collision_parts':len(collision_children),
        'visible_vertices':sum(len(x.vertices) for x in visible.values()),
        'visible_triangles':sum(len(x.triangles) for x in visible.values()),
        'collision_vertices':sum(len(x.vertices) for x in collision.values()),
        'collision_triangles':sum(len(x.triangles) for x in collision.values()),
        'visible_names':[f'{x}#M' for x in visible if visible[x].triangles],
        'collision_names':[f'{x}#CD' for x in collision if collision[x].triangles],
    }


def validate_nif(path: Path, manifest) -> None:
    data=path.read_bytes(); sig=b'Gamebryo File Format, Version 20.0.0.4\n'
    assert data.startswith(sig)
    assert struct.unpack_from('<I',data,len(sig))[0]==0x14000004
    assert b'#M' in data and b'#CD' in data
    assert b'Floor#CD' in data and b'Walls#CD' in data and b'Stairs#CD' in data
    assert manifest['visible_triangles']>=2500, manifest
    assert manifest['collision_triangles']>=100, manifest
    assert manifest['visible_parts']>=10 and manifest['collision_parts']>=5


def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--output',type=Path,default=Path('build/walkable-house-a'))
    args=ap.parse_args(); out=args.output
    if out.exists(): shutil.rmtree(out)
    tex=out/'textures'; tex.mkdir(parents=True)
    for kind in TEXTURES: write_dds(tex/f'NG_WalkableHouse_A_{kind}.dds',kind)
    visible,collision=build_house()
    entry=write_nif(out/'NG_WalkableHouse_A.nif',visible,collision)
    validate_nif(out/'NG_WalkableHouse_A.nif',entry)
    entry['textures']=[f'textures/NG_WalkableHouse_A_{k}.dds' for k in TEXTURES]
    (out/'manifest.json').write_text(json.dumps(entry,indent=2),encoding='utf-8')
    (out/'README.txt').write_text(
        'NextGen Walkable House A - production asset\n'
        'Original stylized fantasy-village geometry and procedural original textures.\n\n'
        'NIF: Gamebryo 20.0.0.4, Z-up, NiTriShape + UV0 + normals + NiTexturingProperty.\n'
        'Visible hierarchy: #M -> material meshes named <part>#M.\n'
        'Collision hierarchy: #CD -> simplified meshes named <part>#CD and APP_CULLED.\n'
        'Walkability: clear front doorway, ground floor, loft floor, staircase and exterior steps.\n'
        'Collision deliberately excludes roof, windows, flowers, lamps and other decoration.\n',encoding='utf-8')
    archive=out.parent/'NextGen_WalkableHouse_A_Production.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(out.rglob('*')):
            if p.is_file(): z.write(p,p.relative_to(out))
    print(json.dumps(entry,indent=2)); print('archive',archive)

if __name__=='__main__': main()
