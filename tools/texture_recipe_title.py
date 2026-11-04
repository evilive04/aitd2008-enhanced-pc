"""Apply authored replacement logo quads to locally owned intro resources.

Only new project geometry/UV/basis values are recipe metadata. Original scene
headers, hierarchy and material data are read from the owner's installation.
"""
import io
import math
import struct
from PIL import Image
from inspect_scene_ui import Xmbf


def check(ok):
    if not ok:raise ValueError('Original intro layout or authored recipe contract differs')


def variant(x,f):
 k,a,arr=f;check(not arr and x.types[k]['kind']==13);t,p=struct.unpack_from('<II',x.data,x.span(a,8));check(t in x.types);return t,p

def collection(x):
 return [(x.types[t]['name'],t,p) for k,a in x.array(x.fields(x.root,0)['Collection']) for t,p in [variant(x,x.fields(k,a)['Value'])]]
def scalar(x,b,f,value,fmt='<f'):
 check(not f[2]);struct.pack_into(fmt,b,x.span(f[1],struct.calcsize(fmt)),value)
def vector(x,b,f,vals):
 fs=x.fields(f[0],f[1]);arr=x.array(fs['Value']);check(len(arr)==len(vals))
 for (_,a),v in zip(arr,vals):struct.pack_into('<f',b,x.span(a,4),v)
def sub(a,b):return [x-y for x,y in zip(a,b)]
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def patch_bounds(x,b,k,p,pts):
 fs=x.fields(k,p);lo=[min(v[i] for v in pts) for i in range(3)];hi=[max(v[i] for v in pts) for i in range(3)];center=[(a+c)/2 for a,c in zip(lo,hi)];radius=max(math.sqrt(dot(sub(v,center),sub(v,center))) for v in pts)+1e-5
 scalar(x,b,fs['SphereRadius'],radius);vector(x,b,fs['SphereCenter'],center)
 for v in pts:check(dot(sub(v,center),sub(v,center))<=radius*radius+1e-6)

def vertex_data(x,fs):
 arr=x.array(fs['Data']);check(len(arr)==160 and all(x.types[k]['bytes']==1 for k,a in arr));a=x.span(arr[0][1],160);return a,x.data[a:a+160]


def build(config,sources,logo):
    im=Image.open(io.BytesIO(logo)).convert('RGBA');check(im.size==(1990,485))
    atlas=Image.new('RGBA',(2048,512));atlas.paste(im,(29,13))
    for y in range(504,512):
        for xx in range(1020,1028):atlas.putpixel((xx,y),(255,255,255,255))
    mips=[];cur=atlas
    while cur.width>=4 or cur.height>=4:
        w,h=cur.size;stream=io.BytesIO();cur.save(stream,format='DDS',pixel_format='DXT5');dds=stream.getvalue()
        check(dds[:4]==b'DDS ' and dds[84:88]==b'DXT5')
        raw=dds[128:];check(len(raw)==max(1,(w+3)//4)*max(1,(h+3)//4)*16);mips.append(raw)
        if w==4 and h<=4:break
        cur=cur.resize((max(1,w//2),max(1,h//2)),Image.Resampling.LANCZOS)
    texture_slot=next(r['storage_index'] for r in config['replacements'] if r['asset'].lower().endswith('.xmt'))
    old=sources[texture_slot];x=Xmbf(old);base=x.base
    new=bytearray(old[:base]);body=bytearray(old[base:base+56])
    struct.pack_into('<II',body,12,2048,512);struct.pack_into('<I',body,24,len(mips));struct.pack_into('<I',body,28,15)
    struct.pack_into('<II',body,48,len(mips),56);body.extend(bytes(len(mips)*16))
    for i,raw in enumerate(mips):
        body.extend(bytes((-len(body))%16));ptr=len(body);struct.pack_into('<4I',body,56+i*16,len(raw)//2,ptr,0,0);body.extend(raw)
    new.extend(body);new.extend(bytes((-len(new))%4096));Xmbf(new)
    output={texture_slot:bytes(new)}
    for model in config['strips']:
        old=sources[model['slot']];x=Xmbf(old);b=bytearray(old);parts=collection(x)
        check([a[0] for a in parts]==['edGfxVertexBuffer','edGfxGeometry'])
        _,vk,vp=parts[0];off,_=vertex_data(x,x.fields(vk,vp));local=model['local'];uv=model['uv']
        check(len(local)==len(uv)==4)
        for v,(point,texture_uv) in enumerate(zip(local,uv)):
            struct.pack_into('>3f2e',b,off+v*40,*point,*texture_uv)
            for at,name in ((16,'normal'),(24,'tangent'),(32,'bitangent')):
                struct.pack_into('>3e',b,off+v*40+at,*model['basis'][name])
        _,gk,gp=parts[1];gf=x.fields(gk,gp);patch_bounds(x,b,gk,gp,local)
        for sk,sp in x.array(gf['SubGeometries']):
            prim=x.fields(sk,sp)['Primitive'];patch_bounds(x,b,prim[0],prim[1],local)
            pf=x.fields(prim[0],prim[1]);check(x.value(pf['PrimitiveCount'])==2 and x.value(pf['Type'])==4)
        Xmbf(b);output[model['slot']]=bytes(b)
    for slot in config['remove_static_child_slots']:
        old=sources[slot];x=Xmbf(old);parts=collection(x)
        check([a[0] for a in parts]==['edGfxHierarchy','edGfxInstance','edGfxInstance'])
        b=bytearray(old);arr=x.fields(x.root,0)['Collection'];struct.pack_into('<I',b,x.span(arr[1],4),2)
        check(len(collection(Xmbf(b)))==2 and sum(a!=c for a,c in zip(old,b))==1);output[slot]=bytes(b)
    for slot in config['streak_slots']:
        old=sources[slot];x=Xmbf(old);parts=collection(x)
        check([a[0] for a in parts]==['edGfxVertexBuffer','edGfxGeometry','edGfxVertexBuffer','edGfxGeometry'])
        b=bytearray(old);_,k,a=parts[0];off,_=vertex_data(x,x.fields(k,a))
        for v in range(4):struct.pack_into('>2e',b,off+v*40+12,.5,508/512)
        output[slot]=bytes(b)
    check(len(output)==43)
    return output
