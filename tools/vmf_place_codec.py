"""Exact observed native PlaceObject26 layout, derived from r29 927ED0/927F70."""
from dataclasses import dataclass
import struct


@dataclass
class Place:
    raw: bytes
    depth: int
    flags: int
    child: int | None
    matrix: tuple | None
    color: tuple | None
    color_offset: int | None
    ratio: int | None
    clip_depth: int | None
    name: str | None

    def serialize(self):return self.raw

    def neutral_rgb(self):
        if self.color is None:raise ValueError('Placement has no native CX')
        out=bytearray(self.raw);c=self.color
        struct.pack_into('<8h',out,self.color_offset,256,256,256,c[3],0,0,0,c[7])
        return bytes(out)


def parse(raw):
    if len(raw)<12 or struct.unpack_from('<II',raw)!=(26,len(raw)):raise ValueError('Invalid native placement extent')
    depth,flags=struct.unpack_from('<HH',raw,8);at=12
    def take(fmt):
        nonlocal at
        size=struct.calcsize(fmt)
        if at+size>len(raw):raise ValueError('Truncated native placement field')
        value=struct.unpack_from(fmt,raw,at);at+=size;return value
    child=take('<H')[0] if flags&2 else None
    matrix=None
    if flags&4:
        at=(at+3)&~3
        scale=take('<2f') if flags&0x100 else (1.,1.)
        rotate=take('<2f') if flags&0x80 else (0.,0.)
        move=take('<2f') if flags&0x200 else (0.,0.)
        matrix=scale+rotate+move
    color=None;color_offset=None
    if flags&8:
        at=(at+3)&~3;color_offset=at;color=take('<8h')
    ratio=take('<H')[0] if flags&16 else None
    clip_depth=take('<H')[0] if flags&64 else None
    name=None
    if flags&32:
        try:end=raw.index(b'\0',at)
        except ValueError as e:raise ValueError('Unterminated native instance name') from e
        name=raw[at:end].decode('cp1252');at=end+1
    if at>len(raw):raise ValueError('Placement fields exceed extent')
    return Place(raw,depth,flags,child,matrix,color,color_offset,ratio,clip_depth,name)


def point(matrix,x,y):
    sx,sy,rx,ry,tx,ty=matrix
    return sx*x+rx*y+tx,ry*x+sy*y+ty


def compose(parent,child):
    a,b,c,d,x,y=parent;e,f,g,h,u,v=child
    return (a*e+c*h,b*f+d*g,a*g+c*f,d*e+b*h,a*u+c*v+x,d*u+b*v+y)
