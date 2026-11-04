"""Authored local tutorial/keycap texture generation; original pixels are read locally."""
import io
import struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageOps
from inspect_scene_ui import Xmbf

def native_bc3(image):
    output=io.BytesIO();image.save(output,format='DDS',pixel_format='DXT5');dds=output.getvalue()
    if dds[:4]!=b'DDS ' or dds[84:88]!=b'DXT5' or len(dds)!=128+image.width*image.height:raise ValueError('Unexpected BC3 encoder output')
    return dds[128:]

def mouse_art():
    scale=8
    image=Image.new('RGBA',(64*scale,64*scale),(0,0,0,0));draw=ImageDraw.Draw(image)
    def box(rect):return tuple(round(value*scale) for value in rect)
    draw.rounded_rectangle(box((3,4,61,62)),radius=8*scale,fill=(5,5,4,165))
    draw.rounded_rectangle(box((3,2,61,60)),radius=8*scale,fill=(24,25,23,252),outline=(211,202,175,248),width=scale)
    draw.line(box((12,4,51,4)),fill=(242,230,201,125),width=scale)
    draw.rounded_rectangle(box((18,9,46,54)),radius=12*scale,fill=(12,14,13,255),outline=(242,230,201,255),width=2*scale)
    # The left top button is the only amber field: actual left mouse click.
    draw.polygon([box(p) for p in [(21,25),(21,19),(22,15),(26,12),(30,11),(31,11),(31,25)]],fill=(208,147,66,255))
    draw.line(box((32,11,32,26)),fill=(242,230,201,255),width=scale)
    draw.line(box((20,27,44,27)),fill=(242,230,201,255),width=scale)
    draw.rounded_rectangle(box((30,16,34,22)),radius=scale,fill=(242,230,201,255))
    # tips.vmf shape34 has a20-twip/pixel bitmap matrix and a900x920-twip
    # quad: it samples only the top-left45x46 pixels of the64x64 resource.
    # The original check occupies45x45. Rasterize the square original artwork
    # once into44x44, preserving aspect, then pad the unused native canvas.
    result=Image.new('RGBA',(64,64),(0,0,0,0))
    result.alpha_composite(image.resize((44,44),Image.Resampling.LANCZOS),(0,0))
    return result

def panel_material(original,slot):
    image=original.copy();pixels=image.load();source=original.load();mask=bytearray(image.width*image.height)
    for y in range(image.height):
        for x in range(image.width):
            if slot==71 and (x>=724 or (572<=x<696 and 112<=y<224)):continue
            r,g,b,a=source[x,y]
            if not a:continue
            high,low=max(r,g,b),min(r,g,b)
            neutral=1.0-(high-low)/max(1,high)
            light=(r*.2126+g*.7152+b*.0722)
            edge=max(0,min(1,(light-38)/160))*max(0,min(1,(neutral-.45)/.45))
            # Independent charcoal material, subtle deterministic film grain;
            # retain authored alpha/silhouette and the native cream metal edge.
            grain=((x*37+y*19+(x*y)%23)%17-8)*.13
            tone=19+5*(1-y/max(1,image.height-1))+grain
            color=(tone*.97,tone,tone*.91)
            if slot==71 and r>110 and r>g*1.4 and b<50:
                strength=min(1,(r-110)/100)
                color=tuple(color[n]*(1-strength*.6)+(100,68,31)[n]*strength*.6 for n in range(3))
            pixels[x,y]=tuple(round(max(0,min(255,color[n]*(1-edge)+(242,230,201)[n]*edge))) for n in range(3))+(a,)
            mask[y*image.width+x]=1
    return image,mask

def encode_replacement(raw,image,mask=None):
    encoded=native_bc3(image)
    if mask is None:return raw[:4096]+encoded
    payload=bytearray(raw[4096:]);width,height=image.size
    for by in range(height//4):
        for bx in range(width//4):
            if any(mask[(by*4+dy)*width+bx*4+dx] for dy in range(4) for dx in range(4)):
                at=(by*(width//4)+bx)*16
                # Preserve alpha bit-for-bit; only the independent color block changes.
                payload[at+8:at+16]=encoded[at+8:at+16]
    return raw[:4096]+payload


def build(config, sources, font_path, native_mouse_images):
    """Return the same four resource replacements as the authored private pack."""
    output = {}
    for slot in (71, 73, 75):
        raw = sources[slot]
        w, h = struct.unpack_from('<II', raw, 524)
        if (raw[512:516] != b'TEXT' or struct.unpack_from('<I', raw, 540)[0] != 15 or
                struct.unpack_from('<II', raw, 568) != (w * h // 2, 3584) or len(raw) != 4096 + w * h):
            raise ValueError('Unsupported original tutorial BC3 texture')
        original = Image.frombytes('RGBA', (w, h), raw[4096:], 'bcn', (3,))
        artwork, mask = (mouse_art(), None) if slot == 75 else panel_material(original, slot)
        output[slot] = encode_replacement(raw, artwork, mask)
    atlas = Image.new('RGBA', (1024, 1024), (0, 0, 0, 0)); draw = ImageDraw.Draw(atlas)
    for name, x, y, width in config['layout']:
        draw.rounded_rectangle((x+3,y+5,x+width-3,y+62),radius=9,fill=(5,6,5,180))
        draw.rounded_rectangle((x+3,y+2,x+width-3,y+59),radius=9,fill=(24,25,23,255),outline=(211,202,175,255),width=2)
        draw.line((x+14,y+5,x+width-14,y+5),fill=(242,230,201,130),width=1)
        label={'LSHIFT':'L SHIFT','RSHIFT':'R SHIFT','LCTRL':'L CTRL','RCTRL':'R CTRL','LALT':'L ALT','RALT':'R ALT','PAGEUP':'PAGE UP','PAGEDOWN':'PAGE DOWN','CAPSLOCK':'CAPS LOCK','SEMICOLON':';'}.get(name,name)
        if name in ('UP','DOWN','LEFT','RIGHT'):
            shapes={'UP':[(32,17),(18,34),(27,34),(27,47),(37,47),(37,34),(46,34)],
                    'DOWN':[(32,47),(18,30),(27,30),(27,17),(37,17),(37,30),(46,30)],
                    'LEFT':[(17,32),(34,18),(34,27),(47,27),(47,37),(34,37),(34,46)],
                    'RIGHT':[(47,32),(30,18),(30,27),(17,27),(17,37),(30,37),(30,46)]}
            draw.polygon([(x+a,y+b) for a,b in shapes[name]],fill=(242,230,201,255))
        else:
            size=35 if len(label)<=2 else 28; font=ImageFont.truetype(str(font_path),size)
            while draw.textbbox((0,0),label,font=font)[2]>width-22:
                size-=1; font=ImageFont.truetype(str(font_path),size)
            draw.text((x+width/2,y+30),label,font=font,anchor='mm',fill=(242,230,201,255),stroke_width=0)
    old_texture=output[75]; encoded=bytearray(native_bc3(atlas)); old_blocks=old_texture[4096:]
    for row in range(16):
        encoded[row*4096:row*4096+256]=old_blocks[row*256:(row+1)*256]
    atlas_name=int(config['atlas_name'],16)
    header=bytearray(old_texture[:4096]);struct.pack_into('<Q',header,516,atlas_name)
    struct.pack_into('<II',header,524,1024,1024);struct.pack_into('<I',header,568,1024*1024//2)
    # Later accepted native mouse cells, matching the final authored atlas:
    # x0/64,y640; preserve every existing keyboard block and the header.
    if len(native_mouse_images)!=2:raise ValueError('Both original native mouse cells are required')
    for i,im in enumerate(native_mouse_images):
        if im.size!=(65,55):raise ValueError('Original native mouse image dimensions differ')
        cell=Image.new('RGBA',(64,64))
        fitted=ImageOps.contain(im.convert('RGBA'),(60,60),Image.Resampling.LANCZOS)
        cell.alpha_composite(fitted,((64-fitted.width)//2,(64-fitted.height)//2));bc3=native_bc3(cell)
        for row in range(16):
            at=((640//4+row)*256+i*16)*16
            encoded[at:at+256]=bc3[row*256:(row+1)*256]
    output[75]=bytes(header)+encoded
    material=bytearray(sources[233]);parsed=Xmbf(material)
    refs=parsed.array(parsed.fields(parsed.root,0)['Textures'])
    if len(refs)!=1:raise ValueError('Unexpected tutorial material texture count')
    key,relative=refs[0];field=parsed.fields(key,relative)['Texture'];at=parsed.base+field[1]
    if struct.unpack_from('<Q',material,at)[0]!=0x6f79158fbf815c79:
        raise ValueError('Original tutorial material texture reference differs')
    struct.pack_into('<Q',material,at,atlas_name);output[233]=bytes(material)
    return output
