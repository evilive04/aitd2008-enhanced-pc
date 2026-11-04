"""Reconstruct the original lens contour at higher resolution, locally only."""
import io
import struct
from PIL import Image, ImageFilter


def build(original):
    if (original[512:516]!=b'TEXT' or struct.unpack_from('<II',original,524)!=(256,256)
            or struct.unpack_from('<I',original,540)[0]!=11):
        raise ValueError('Unsupported original lens symbol texture')
    im=Image.frombytes('RGBA',(256,256),original[4096:4096+32768],'bcn',(1,)).convert('L')
    width=2048
    soft=im.filter(ImageFilter.GaussianBlur(.45)).resize((width,width),Image.Resampling.LANCZOS)
    # Same native radiance and source iso-contour, no newly generated design.
    large=soft.point(lambda n:round(220*max(0,min(1,(n-108)/24))))
    header=bytearray(original[:4096]);struct.pack_into('<II',header,524,width,width)
    struct.pack_into('<I',header,536,10);struct.pack_into('<I',header,560,10);payload=bytearray()
    for n in range(10):
        size=width>>n;level=large if not n else large.resize((size,size),Image.Resampling.LANCZOS)
        rgba=Image.merge('RGBA',(level,level,level,Image.new('L',level.size,255)))
        stream=io.BytesIO();rgba.save(stream,format='DDS',pixel_format='DXT1');dds=stream.getvalue()
        if dds[84:88]!=b'DXT1' or len(dds[128:])!=size*size//2:
            raise ValueError('Unexpected lens BC1 encoder extent')
        data=dds[128:];begin=4096+len(payload);payload.extend(data)
        payload.extend(bytes((-len(payload))%4096));end=4096+len(payload)
        struct.pack_into('<4I',header,568+16*n,len(data)//2,begin-512,0,end-512)
    result=bytes(header)+payload
    if len(result)%4096 or result[:524]!=original[:524] or result[540:560]!=original[540:560]:
        raise ValueError('Original lens identity fields changed')
    return result
