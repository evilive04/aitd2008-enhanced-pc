"""Generate texture/title mod resources locally from an owner's original game.

No original game resource is embedded or downloaded. The executable, licensing
and archives are opened read-only. Output must be NEW, outside the game and
source repository, and is local-only: do not upload the generated mods.
Requires 64-bit Python, Pillow and NumPy (see requirements-ui.txt).
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import struct

from decode_bank import decode
from inspect_fat import read_index
from extract_runtime_ui import plain_path, file_sha, STOCK_EXE_SHA256

MAX_BANK = 128 * 1024 * 1024
SHA = re.compile(r'^[0-9a-f]{64}$')
PACKS = {'water':'enhanced-actual-background-pond-v150',
         'title':'enhanced-intro-title-v149',
         'lens':'enhanced-lens-symbol-v142',
         'keycaps':'modern-pc-tutorial'}


def sha(data):return hashlib.sha256(data).hexdigest()


def resource_spec(value):
    if (not isinstance(value,dict) or value.get('archive') not in ('a','b','c') or
            type(value.get('bank_index')) is not int or type(value.get('storage_index')) is not int or
            not 0 <= value['bank_index'] <= 65535 or not 0 <= value['storage_index'] <= 100000 or
            not SHA.fullmatch(value.get('original_sha256',''))):
        raise ValueError('Invalid original-resource recipe identity')
    return value


def asset_name(value):
    if not isinstance(value,str) or not value or '\\' in value or ':' in value:
        raise ValueError('Invalid generated asset name')
    parts=value.split('/')
    if any(not q or q in ('.','..') or q.endswith((' ','.')) for q in parts):
        raise ValueError('Unsafe generated asset path')
    p=PurePosixPath(value)
    if p.is_absolute() or p.suffix.lower() not in ('.xmt','.xmg','.xmi','.xmm'):
        raise ValueError('Unexpected generated resource type')
    return p


class Originals:
    def __init__(self,game):
        self.game=game;self.index={};self.banks={}
    def get(self,record,bank_sha=None):
        r=resource_spec(record);archive=r['archive'];bank=r['bank_index'];key=(archive,bank)
        if archive not in self.index:
            fat=plain_path(self.game/f'alone_compressed_{archive}.fat')
            _,self.index[archive]=read_index(fat)
        if key not in self.banks:
            entries=self.index[archive]
            if not 0 <= bank < len(entries):raise ValueError('Original bank index is absent')
            big=plain_path(self.game/f'alone_compressed_{archive}.big');entry=entries[bank]
            if entry['offset']<0 or entry['size']<=0 or entry['offset']+entry['padded_size']>big.stat().st_size:
                raise ValueError('Original bank exceeds archive extent')
            with big.open('rb') as stream:
                data,metadata,proof=decode(stream,entry,MAX_BANK)
            if not proof['stream_complete']:raise ValueError('Original bank decompression is incomplete')
            self.banks[key]=(bytes(data),{f['storage_index']:f for f in metadata['files']})
        data,files=self.banks[key]
        if bank_sha is not None and (not SHA.fullmatch(bank_sha) or sha(data)!=bank_sha):
            raise ValueError('Original complete-bank identity differs')
        f=files.get(r['storage_index'])
        if f is None or not 0<f['size']<=64*1024*1024 or f['offset']<0 or f['offset']+f['size']>len(data):
            raise ValueError('Required original resource is absent or outside bounds')
        raw=data[f['offset']:f['offset']+f['size']]
        if sha(raw)!=r['original_sha256']:raise ValueError('Original resource hash differs')
        return raw


class Output:
    def __init__(self,path):self.path=path;self.packs=[];self.files=[]
    def pack(self,name,records,payloads):
        target=self.path/'mods'/PACKS[name];target.mkdir(parents=True)
        manifest={'version':1,'id':PACKS[name],'target_exe_sha256':STOCK_EXE_SHA256,'replacements':[]}
        if {r['storage_index'] for r in records} != set(payloads):
            raise ValueError('Generated replacement set differs from authored recipe')
        for r in records:
            resource_spec(r);payload=payloads[r['storage_index']];relative=asset_name(r['asset'])
            actual=sha(payload)
            if not SHA.fullmatch(r.get('sha256','')) or actual!=r['sha256']:
                raise ValueError('Generated '+name+' resource differs from approved output: slot '+str(r['storage_index'])+
                                 '; expected '+str(r.get('sha256'))+'; generated '+actual)
            destination=target.joinpath(*relative.parts);destination.parent.mkdir(parents=True,exist_ok=True)
            with destination.open('xb') as stream:stream.write(payload)
            item={k:r[k] for k in ('archive','bank_index','storage_index','original_sha256','asset','sha256')}
            manifest['replacements'].append(item)
            self.files.append({'file':destination.relative_to(self.path).as_posix(),'sha256':r['sha256'],'bytes':len(payload)})
        with (target/'mod.json').open('x',encoding='utf-8') as stream:json.dump(manifest,stream,indent=2)
        self.packs.append(PACKS[name])


def load_keycap_template(source_root):
    """Use reviewed project graphics, never a player's installed font version."""
    from PIL import Image
    root = Path(source_root)
    config_path = plain_path(root / 'configs/texture-recipes.json')
    if config_path.stat().st_size > 1024 * 1024:
        raise ValueError('Recipe metadata exceeds limit')
    config = json.loads(config_path.read_text(encoding='utf-8'))
    if config.get('schema') != 1 or config.get('target_exe_sha256') != STOCK_EXE_SHA256:
        raise ValueError('Unsupported texture recipe metadata')
    record = config['keycaps']
    if record.get('template') != 'assets/ui/keycaps-template.png':
        raise ValueError('Unsupported keycap template path')
    template_path = plain_path(root / record['template'])
    if template_path.stat().st_size > 1024 * 1024 or file_sha(template_path) != record['template_sha256']:
        raise ValueError('Keycap graphics failed their integrity check. Extract the complete current Release again.')
    with Image.open(template_path) as image:
        if image.format != 'PNG' or image.size != (1024, 1024) or image.mode != 'RGBA':
            raise ValueError('Unsupported keycap image format or dimensions')
        image.load()
        result = image.copy()
    print('Portable keycap graphics confirmed; no Windows font dependency.', flush=True)
    return result


def generate(game,output,logo=None,font=None,recipes=('water','title','lens','keycaps')):
    if struct.calcsize('P')!=8:raise ValueError('Use 64-bit Python for bounded high-resolution texture generation')
    source_root=Path(__file__).resolve().parents[1]
    game=plain_path(game);output=plain_path(output,missing_leaf=True)
    if output.exists() or output==game or game in output.parents or output==source_root or source_root in output.parents:
        raise ValueError('Output must be NEW and outside the game and source repository')
    executable=plain_path(game/'Alone.exe')
    if not executable.is_file() or executable.stat().st_size>64*1024*1024 or file_sha(executable)!=STOCK_EXE_SHA256:
        raise ValueError('This helper requires the unmodified supported original executable')
    config_path=plain_path(source_root/'configs/texture-recipes.json')
    if config_path.stat().st_size>1024*1024:raise ValueError('Recipe metadata exceeds limit')
    config=json.loads(config_path.read_text(encoding='utf-8'))
    if config.get('schema')!=1 or config.get('target_exe_sha256')!=STOCK_EXE_SHA256:
        raise ValueError('Recipe metadata does not target the supported original game')
    selected=tuple(recipes)
    if not selected or len(set(selected))!=len(selected) or any(x not in PACKS for x in selected):
        raise ValueError('Unknown or duplicate texture recipe selection')
    logo_data=None
    if 'title' in selected:
        logo_path=plain_path(logo or source_root/'assets/ui/aitd_logo.png')
        if logo_path.stat().st_size>16*1024*1024:raise ValueError('Project logo exceeds size bounds')
        logo_data=logo_path.read_bytes()
        if sha(logo_data)!=config['title']['logo_sha256']:raise ValueError('Use the approved project logo; no automatic redesign is performed')
    if 'keycaps' in selected:
        template=load_keycap_template(source_root)
    originals=Originals(game);output.mkdir();writer=Output(output)
    try:
        if 'water' in selected:
            from texture_recipe_water import build
            c=config['water'];normal=originals.get(c['normal'],c['bank_sha256']);gloss=originals.get(c['gloss'],c['bank_sha256'])
            writer.pack('water',[c['normal']],{c['normal']['storage_index']:build(normal,gloss,c)})
        if 'title' in selected:
            from texture_recipe_title import build
            c=config['title'];sources={r['storage_index']:originals.get(r,c['bank_sha256']) for r in c['replacements']}
            writer.pack('title',c['replacements'],build(c,sources,logo_data))
        if 'lens' in selected:
            from texture_recipe_lens import build
            r=config['lens'];writer.pack('lens',[r],{r['storage_index']:build(originals.get(r))})
        if 'keycaps' in selected:
            from texture_recipe_keycaps import build
            from inspect_vmf import inspect as inspect_vmf
            c=config['keycaps'];tips=originals.get(c['tips']);inspect_vmf(tips)
            sources={r['storage_index']:originals.get(r) for r in c['replacements']}
            from extract_runtime_ui import texture_image, native_mouse_points, mouse_image
            atlas=texture_image(originals.get(c['mouse_atlas']),(1024,512))
            points=native_mouse_points(originals.get(c['inventory_movie']))
            native_mice=[mouse_image(atlas,points,tx) for tx in (-5962,-4618)]
            writer.pack('keycaps',c['replacements'],build(c,sources,template,native_mice))
        if file_sha(executable)!=STOCK_EXE_SHA256:raise ValueError('Original executable changed during local generation')
        report={'schema':1,'source_executable_sha256':STOCK_EXE_SHA256,'original_files_written':False,
                'redistributable_game_content':False,'packs':writer.packs,'files':writer.files}
        with (output/'LOCAL_TEXTURE_GENERATION.json').open('x',encoding='utf-8') as stream:json.dump(report,stream,indent=2)
        with (output/'READ-ME-LOCAL-ONLY.txt').open('x',encoding='utf-8') as stream:
            stream.write('Generated locally from your own original game plus project recipes.\nDo not upload these XMT/XMG/XMI resources or generated mods to GitHub.\nNo executable, activation or original archive was changed.\n')
        return report
    except Exception:
        # Preserve only the incomplete owned output. Never erase pre-existing or raced paths.
        try:
            with (output/'GENERATION_FAILED.txt').open('x',encoding='utf-8') as stream:
                stream.write('Incomplete local texture generation. Do not install this output.\n')
        except OSError:pass
        raise


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--logo',type=Path);parser.add_argument('--font',type=Path,help=argparse.SUPPRESS)
    parser.add_argument('--recipes',default='water,title,lens,keycaps',help='Comma-separated recipe subset')
    args=parser.parse_args();report=generate(args.game,args.output,args.logo,args.font,args.recipes.split(','))
    print(json.dumps({'output':str(args.output),'packs':report['packs'],'files':len(report['files']),'original_files_written':False}))


if __name__=='__main__':main()
