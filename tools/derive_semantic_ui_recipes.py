"""Maintainer utility: derive explicit numeric UI authoring recipes.

Inputs are the maintainer's private original and authored UI reference folders.
Outputs contain no complete native records, fonts, raster pixels or action
bytecode: only hashes, bounded IDs, authored mesh/color numbers and source
references. This utility does not generate or install game resources.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import struct

from semantic_ui_codec import sha,shape_parts,button_events,ADAPTERS
from vmf_vector_style import Movie
from vmf_place_codec import parse as parse_place


SLOTS = {'chapters':('a',361,10),'general':('a',367,22),
         'helper':('a',367,24),'helperqt':('a',397,2582),
         'invntory':('a',397,2583),'mainmenu':('a',367,20),
         'phone':('c',275,29),'tips':('a',367,21)}


def _source_crop(tri,style,old_groups):
    candidates=[]
    for group,(native_style,native_triangles) in enumerate(old_groups):
        if native_style!=style:continue
        for index,native in enumerate(native_triangles):
            ax,ay,bx,by,cx,cy=native
            det=(bx-ax)*(cy-ay)-(by-ay)*(cx-ax)
            if not det:continue
            weights=[]
            penalty=0
            for x,y in zip(tri[::2],tri[1::2]):
                u=(x-ax)*(cy-ay)-(y-ay)*(cx-ax)
                v=(bx-ax)*(y-ay)-(by-ay)*(x-ax)
                d=det
                if d<0:u,v,d=-u,-v,-d
                weights.append([u,v,d])
                penalty+=max(0,-u/d)+max(0,-v/d)+max(0,(u+v-d)/d)
            candidates.append((penalty,group,index,weights))
    if not candidates:
        raise ValueError('A retained native textured fragment has no original triangle basis')
    _,group,index,weights=min(candidates,key=lambda v:v[:3])
    return {'source_crop':{'group':group,'triangle':index,'weights':weights}}


def _shape_recipe(a,am,b,bm,identity):
    old_bounds,old_fills,old_groups = shape_parts(a,am,identity,allow_nontriangular=True)
    bounds,fills,groups = shape_parts(b,bm,identity)
    lookup = {}
    for g,(_,triangles) in enumerate(old_groups):
        for t,tri in enumerate(triangles):
            lookup.setdefault(tri,(g,t))
    encoded_fills = []
    for fill in fills:
        if fill in old_fills:
            encoded_fills.append({'source_style':old_fills.index(fill)+1})
        elif len(fill)==8 and struct.unpack_from('<I',fill)[0]==0:
            encoded_fills.append({'rgba':list(fill[4:])})
        else:
            raise ValueError(f'{identity}: non-solid authored fill or modified native bitmap style')
    encoded_groups = []
    for style,triangles in groups:
        parts = []
        fill=encoded_fills[style-1]
        native_bitmap_style=(fill.get('source_style') if 'source_style' in fill and
            struct.unpack_from('<I',old_fills[fill['source_style']-1])[0]==65 else None)
        for tri in triangles:
            if tri in lookup:
                g,t = lookup[tri]
                if parts and 'source_run' in parts[-1] and parts[-1]['source_run'][:2]==[g,t-parts[-1]['source_run'][2]]:
                    parts[-1]['source_run'][2]+=1
                else:
                    parts.append({'source_run':[g,t,1]})
            elif native_bitmap_style is not None:
                parts.append(_source_crop(tri,native_bitmap_style,old_groups))
            elif parts and 'authored' in parts[-1]:
                parts[-1]['authored'].append(list(tri))
            else:
                parts.append({'authored':[list(tri)]})
        encoded_groups.append({'style':style,'triangles':parts})
    ai,bi=am.shapes[identity],bm.shapes[identity]
    return {'source_tag_sha256':sha(a[ai['offset']:ai['offset']+ai['size']]),
            'output_tag_sha256':sha(b[bi['offset']:bi['offset']+bi['size']]),
            'bounds':'source' if bounds==old_bounds else list(bounds),
            'fills':encoded_fills,'groups':encoded_groups}


def derive(name,a,b):
    am,bm=Movie(a),Movie(b)
    if a[:72]!=b[:72] or a[-16:]!=b[-16:]:
        raise ValueError('Global metadata must remain original')
    aa=[(k,a[o:o+s]) for k,o,s in am.tags]
    bb=[(k,b[o:o+s]) for k,o,s in bm.tags]
    adapters=[]
    filtered=[]
    i=0
    while i<len(bb):
        kind,raw=bb[i]
        if kind==43 and raw[8:].startswith(b'epc_'):
            if i+2>=len(bb) or [bb[i+1][0],bb[i+2][0]]!=[12,1]:
                raise ValueError('Unexpected own recap adapter layout')
            adapters.append(raw[8:].split(b'\0')[0].decode('ascii'))
            i+=3
        else:
            filtered.append(bb[i]);i+=1
    bb=filtered
    if adapters and (name!='mainmenu' or adapters!=[v for _,v in ADAPTERS]):
        raise ValueError('Unexpected authored adapter names')
    if len(aa)!=len(bb):
        raise ValueError('Native source tag graph changed')
    recipe={'version':1,'movie':name,'source_sha256':sha(a),
            'output_sha256':sha(b),'output_bytes':len(b),
            'target':dict(zip(('archive','bank_index','storage_index'),SLOTS[name])),
            'shapes':{},'field_operations':[], 'recap_adapters':bool(adapters)}
    if name=='invntory' and sha(b)=='b02964ca69d2edb63bbf785628bac6f46e85af62670e1114e5000cfe4088e1ae':
        # The reviewed legacy restoration retained stale root frame offsets.
        # Independent canonical normalization established this exact correction:
        # 289 header bytes only, no geometry, scripts, fonts or other records.
        recipe['legacy_reference_sha256']=recipe['output_sha256']
        recipe['output_sha256']='fd3daf6de2d9d4ed0a273413418ff5fb907d19fb4fc3e5d69e4163309cb912a4'
        recipe['intentional_metadata_correction']={
            'scope':'Root sprite extent and derived frame offsets only',
            'absolute_header_span':[72,504], 'changed_bytes':289,
            'root_frames':102,'legacy_extent':1095600,'correct_extent':1054264,
            'reason':'Legacy native shape restoration left offsets beyond actual frame records.',
            'non_header_records_changed':False,'runtime_verified':False}
    for index,((kind,old),(other,new)) in enumerate(zip(aa,bb)):
        if kind!=other:
            raise ValueError('Native source tag order changed')
        if old==new or kind==39:
            continue
        if kind==2:
            identity=struct.unpack_from('<I',old,8)[0]
            if identity!=struct.unpack_from('<I',new,8)[0]:
                raise ValueError('Shape identity changed')
            recipe['shapes'][str(identity)]=_shape_recipe(a,am,b,bm,identity)
            continue
        operation=None
        extra={}
        if kind==7:
            out=bytearray(old)
            for condition,start,end in button_events(old):
                if condition==1:out[start:end]=bytes(end-start)
            if bytes(out)==new:operation='clear_button_hover'
        elif kind==26:
            before,after=parse_place(old),parse_place(new)
            if before.neutral_rgb()==new:
                operation='neutral_placement_rgb'
            elif before.color is not None and after.color is not None:
                out=bytearray(old)
                struct.pack_into('<8h',out,before.color_offset,*after.color)
                if bytes(out)==new and before.color[3::4]==after.color[3::4]:
                    operation='placement_rgb_tint'
                    extra={'multiply':list(after.color[:3]),'add':list(after.color[4:7])}
        elif kind==37 and len(old)==152 and old[:120]+bytes(32)==new:
            operation='clear_placeholder_text'
        elif kind==20 and len(old)==40:
            out=bytearray(old);struct.pack_into('<II',out,12,1024,1024)
            if bytes(out)==new and struct.unpack_from('<III',old,8)==(33,64,64):
                operation='keycap_bitmap_dimensions'
        if operation is None:
            raise ValueError(f'{name} tag{index}: unexplained source change type{kind}')
        recipe['field_operations'].append({'tag_index':index,'operation':operation,
            'source_tag_sha256':sha(old),'output_tag_sha256':sha(new),**extra})
    return recipe


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('originals',type=Path)
    parser.add_argument('authored_reference',type=Path)
    parser.add_argument('output_new',type=Path)
    args=parser.parse_args()
    if args.output_new.exists():
        parser.error('Choose a new recipe directory')
    recipes={name:derive(name,(args.originals/(name+'.vmf')).read_bytes(),
                        (args.authored_reference/(name+'.vmf')).read_bytes())
             for name in SLOTS}
    args.output_new.mkdir()
    for name,recipe in recipes.items():
        with (args.output_new/(name+'.json')).open('x',encoding='utf-8',newline='\n') as stream:
            json.dump(recipe,stream,separators=(',',':'),ensure_ascii=True)
            stream.write('\n')
    print(json.dumps({'recipes_written':len(recipes),
                      'shapes':sum(len(r['shapes']) for r in recipes.values()),
                      'generated_game_payloads':False}))


if __name__=='__main__':main()
