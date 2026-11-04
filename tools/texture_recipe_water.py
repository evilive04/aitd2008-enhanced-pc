"""Deterministic authored pond normals generated from locally owned originals."""
import io
import struct
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from inspect_scene_ui import Xmbf


def decode(raw,w,h,fmt):
    return Image.frombytes('RGBA',(w,h),raw,'bcn',(3 if fmt==15 else 1,'DXT5' if fmt==15 else 'DXT1'))


def levels(raw):
    w,h=struct.unpack_from('<II',raw,524);fmt=struct.unpack_from('<I',raw,540)[0];count=struct.unpack_from('<I',raw,560)[0]
    if raw[512:516]!=b'TEXT' or fmt not in (11,15) or not 1<=count<=12:
        raise ValueError('Unsupported original pond texture')
    out=[]
    for i in range(count):
        words,off,_,_=struct.unpack_from('<4I',raw,568+16*i);payload=raw[512+off:512+off+2*words]
        size=(max(4,w>>i),max(4,h>>i))
        if len(payload)!=size[0]*size[1]//(1 if fmt==15 else 2):raise ValueError('Pond mip extent differs')
        out.append((decode(payload,*size,fmt),payload))
    return raw,out


def vec(im):
    a=np.asarray(im).astype(np.float32)/127.5-1
    return a[:,:,[3,1,2]]


def unit(n):return n/np.maximum(np.linalg.norm(n,axis=2,keepdims=True),1e-8)


def build(original,gloss_raw,config):
    old,orig=levels(original);_,gloss=levels(gloss_raw)
    if orig[0][0].size!=(256,256) or gloss[0][0].size!=(256,256):
        raise ValueError('Original pond atlas dimensions differ')
    region=Image.new('L',(256,256));ImageDraw.Draw(region).polygon([tuple(p) for p in config['polygon']],fill=255)
    g=np.asarray(gloss[0][0])[:,:,0].astype(np.float32)
    m=np.clip((g-50)/50,0,1)*np.asarray(region)/255
    mask=Image.fromarray(np.uint8(np.rint(m*255)))
    metric=config['metric']
    N=2048
    baseold=np.array(orig[0][0]);ups=np.stack([np.asarray(Image.fromarray(baseold[:,:,k]).resize((N,N),Image.Resampling.BILINEAR)) for k in range(4)],axis=2)
    maskhi=mask.resize((N,N),Image.Resampling.BILINEAR).filter(ImageFilter.MinFilter(3));mh=np.asarray(maskhi).astype(np.float32)/255
    vv,uu=np.mgrid[:N,:N].astype(np.float32);uu=(uu+.5)/N;vv=(vv+.5)/N
    # Seeded real Gaussian height spectrum: broad wind lobes and all wavelengths
    # 2..16m. Fourier derivatives give an integrable physical vector field; no
    # repeated handful of parallel crests and no unrelated image/noise artwork.
    rng=np.random.default_rng(config['seed']);white=rng.standard_normal((N,N),dtype=np.float32);spectrum=np.fft.rfft2(white)
    ku=2*np.pi*np.fft.rfftfreq(N,d=metric[0]/N)[None,:];kv=2*np.pi*np.fft.fftfreq(N,d=metric[1]/N)[:,None];k=np.sqrt(ku*ku+kv*kv);safe=np.maximum(k,1e-8);wavelength=2*np.pi/safe
    # Smoothly band-limit the Phillips-style k^-4 height power spectrum. A broad
    # angular lobe retains substantial crosswind energy instead of comb-like waves.
    wind=.48;alignment=(ku*np.cos(wind)+kv*np.sin(wind))/safe;power_direction=.32+.68*alignment*alignment
    band=np.clip((wavelength-2)/.8,0,1)*np.clip((16-wavelength)/3,0,1);band=band*band*(3-2*band)
    height=spectrum*np.sqrt(power_direction)*band/(safe*safe);height[0,0]=0
    su=np.fft.irfft2(height*(1j*ku),s=(N,N)).real;sv=np.fft.irfft2(height*(1j*kv),s=(N,N)).real
    slope=np.sqrt(su*su+sv*sv);scale=np.tan(np.deg2rad(2.65))/np.percentile(slope[mh>.8],95);su*=scale;sv*=scale;height*=scale
    water=unit(np.dstack([-su,-sv,np.ones((N,N),np.float32)]));physical_height=np.fft.irfft2(height,s=(N,N)).real

    del white,spectrum,height,su,sv,slope,physical_height,ku,kv,k,safe,wavelength,alignment,power_direction,band
    # Each level blends newly averaged unit water vectors into the corresponding
    # original level, while copying compressed BC3 blocks outside the mask exactly.
    encoded=[];stats=[];waterlevel=water;masklevel=mh;outside_max=0
    for level in range(10):
     size=N>>level
     if level<3:
      baseline=np.stack([np.asarray(Image.fromarray(baseold[:,:,k]).resize((size,size),Image.Resampling.BILINEAR)) for k in range(4)],2);oldraw=None
     else:baseline=np.array(orig[level-3][0]);oldraw=orig[level-3][1]
     oldvec=vec(Image.fromarray(baseline));blended=unit(oldvec*(1-masklevel[:,:,None])+waterlevel*masklevel[:,:,None]);rgba=baseline.copy();active=masklevel>0
     for k,ch in enumerate([3,1,2]):rgba[:,:,ch][active]=np.uint8(np.rint(np.clip(blended[:,:,k][active]*.5+.5,0,1)*255))
     im=Image.fromarray(rgba);f=io.BytesIO();im.save(f,format='DDS',pixel_format='DXT5');dds=f.getvalue();assert dds[84:88]==b'DXT5';comp=bytearray(dds[128:]);blocks=active.reshape(size//4,4,size//4,4).any(axis=(1,3));outsideblocks=~blocks
     if oldraw is not None:
      a=np.frombuffer(comp,dtype=np.uint8).reshape(size//4,size//4,16);ob=np.frombuffer(oldraw,dtype=np.uint8).reshape(a.shape);a[outsideblocks]=ob[outsideblocks];assert np.array_equal(a[outsideblocks],ob[outsideblocks])
     decoded=np.array(decode(bytes(comp),size,size,15));diff=np.abs(decoded.astype(np.int16)-baseline.astype(np.int16));out=~active
     bound=int(diff[out].max()) if out.any() else 0;outside_max=max(outside_max,bound);nd=unit(vec(Image.fromarray(decoded)));ang=np.degrees(np.arccos(np.clip(nd[:,:,2],-1,1)));full=masklevel>.8
     stats.append({'level':level,'size':size,'bytes':len(comp),'modifiedBC3Blocks':int(blocks.sum()),'outsideMaskMaxRGBAError':bound,'outsideBC3BlocksBitExact':oldraw is not None,'waterAngleP95Degrees':float(np.percentile(ang[full],95)) if full.any() else None,'maxUnitLengthEncodingError':float(np.abs(np.linalg.norm(vec(Image.fromarray(decoded)),axis=2)[active]-1).max()) if active.any() else 0})
     encoded.append(bytes(comp))
     if size>4:
      waterlevel=unit(waterlevel.reshape(size//2,2,size//2,2,3).mean(axis=(1,3)));masklevel=masklevel.reshape(size//2,2,size//2,2).mean(axis=(1,3))
    # Original schema/resourceName/type/pool unchanged; dimensions and mip array are
    # the only header changes. Native convention ends at4x4, same as source.
    body=bytearray(old[512:568]);struct.pack_into('<II',body,12,N,N);struct.pack_into('<I',body,24,len(encoded));struct.pack_into('<II',body,48,len(encoded),56);body.extend(bytes(len(encoded)*16))
    for i,data in enumerate(encoded):
     body.extend(bytes((-len(body))%16));off=len(body);struct.pack_into('<4I',body,56+16*i,len(data)//2,off,0,0);body.extend(data)
    new=bytearray(old[:512])+body;new.extend(bytes((-len(new))%4096));texture=bytes(new);assert texture[:512]==old[:512] and texture[512:524]==old[512:524]

    x=Xmbf(texture);mips=x.array(x.fields(x.root,0)['Data'])
    if len(mips)!=10:raise ValueError('Generated pond mip count differs')
    if outside_max>8 or max(a['maxUnitLengthEncodingError'] for a in stats)>=.015:
        raise ValueError('Generated pond boundary or normal encoding exceeds approved limits')
    if not 2<stats[0]['waterAngleP95Degrees']<4.5 or not all(a['outsideBC3BlocksBitExact'] for a in stats[3:]):
        raise ValueError('Generated pond detail or preserved terrain differs')
    return texture
