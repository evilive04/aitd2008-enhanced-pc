"""Wrap the approved map-marker PNGs in locally read original texture headers.

Usage: python tools/generate_map_mods.py GAME_DIR OUTPUT_NEW_DIR [--art-dir DIR]

No game header, native image or compressed game payload is embedded here. The
six PNGs must separately have permission for public distribution. Generated XMT
resources are local-only and must never be uploaded. Original executable,
activation files and archives are opened read-only.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct

from PIL import Image

from extract_runtime_ui import plain_path,file_sha,STOCK_EXE_SHA256
from generate_texture_mods import Originals,sha
from texture_recipe_keycaps import native_bc3


PACK_ID='enhanced-map-icons-v140'
MARKERS={530:17,534:19,536:18,538:16,541:22,545:23}


def generate(game,output,art_dir=None):
    root=Path(__file__).resolve().parents[1]
    game=plain_path(game).resolve(strict=True)
    output=plain_path(output,missing_leaf=True)
    if (not game.is_dir() or output.exists() or output==game or game in output.parents or
        output==root or root in output.parents):
        raise ValueError('Choose a NEW output directory outside the game and source repository')
    executable=plain_path(game/'Alone.exe')
    if (not executable.is_file() or not 0<executable.stat().st_size<=64*1024*1024 or
        file_sha(executable)!=STOCK_EXE_SHA256):
        raise ValueError('The unmodified supported Steam executable is required')
    config_path=plain_path(root/'configs/map-recipes.json')
    if not config_path.is_file() or not 0<config_path.stat().st_size<=64*1024:
        raise ValueError('Map recipe metadata is missing or exceeds bounds')
    config=json.loads(config_path.read_text(encoding='utf-8'))
    records=config.get('markers',[])
    if (config.get('schema')!=1 or config.get('target_exe_sha256')!=STOCK_EXE_SHA256 or
        not isinstance(records,list) or len(records)!=6 or
        {r.get('bitmap'):r.get('storage_index') for r in records}!=MARKERS):
        raise ValueError('Map recipe catalog differs from the six supported native markers')
    art_dir=plain_path(art_dir or root/'assets/map-icons').resolve(strict=True)
    if not art_dir.is_dir():
        raise ValueError('Map artwork path must be an existing directory')
    originals=Originals(game)
    generated=[]
    for record in records:
        bitmap=record['bitmap']
        if (record.get('archive')!='c' or record.get('bank_index')!=140 or
            record.get('art')!=f'new-{bitmap}.png' or
            record.get('asset')!=f'textures/map-{bitmap}.xmt'):
            raise ValueError('Unsafe or unsupported map recipe resource path')
        source=originals.get(record)
        if len(source)<4096:
            raise ValueError('Original marker texture header is truncated')
        old_width,old_height=struct.unpack_from('<II',source,524)
        if (source[512:516]!=b'TEXT' or
            struct.unpack_from('<I',source,540)[0]!=15 or
            struct.unpack_from('<II',source,568)!=(old_width*old_height//2,3584) or
            [old_width,old_height]!=record['original_dimensions']):
            raise ValueError('Unsupported original single-level BC3 marker texture')
        art_path=plain_path(art_dir/record['art'])
        if (not art_path.is_file() or not 0<art_path.stat().st_size<=16*1024*1024 or
            file_sha(art_path)!=record['art_sha256']):
            raise ValueError('Map artwork differs from the approved source PNG')
        with Image.open(art_path) as image:
            if image.format!='PNG' or image.size!=(256,256):
                raise ValueError('Map artwork must be the approved 256x256 PNG')
            artwork=image.convert('RGBA')
        # Exactly the original v140 wrapping operation: native UV mapping and
        # all other header fields are source data read locally, never shipped.
        header=bytearray(source[:4096])
        struct.pack_into('<II',header,524,256,256)
        struct.pack_into('<I',header,568,256*256//2)
        payload=bytes(header)+native_bc3(artwork)
        payload+=bytes((-len(payload))%4096)
        if sha(payload)!=record['sha256']:
            raise ValueError(f'Encoded marker{bitmap} differs from reviewed output: '
                             f"expected {record['sha256']}; generated {sha(payload)}")
        generated.append((record,payload))
    # All sources and exact outputs are authenticated before output creation.
    output.mkdir()
    try:
        pack=output/'mods'/PACK_ID
        (pack/'textures').mkdir(parents=True)
        replacements=[]
        for record,payload in generated:
            with (pack/record['asset']).open('xb') as stream:
                stream.write(payload)
            replacements.append({k:record[k] for k in ('archive','bank_index','storage_index',
                                     'original_sha256','asset','sha256')})
        manifest={'version':1,'id':PACK_ID,'target_exe_sha256':STOCK_EXE_SHA256,
                  'replacements':replacements}
        with (pack/'mod.json').open('x',encoding='utf-8',newline='\n') as stream:
            json.dump(manifest,stream,indent=2);stream.write('\n')
        report={'schema':1,'source_executable_sha256':STOCK_EXE_SHA256,
            'original_files_written':False,'activation_modified':False,
            'redistributable_generated_resources':False,'pack':PACK_ID,
            'resources':replacements,'recipe_sha256':file_sha(config_path),
            'artwork_sha256':{r['art']:r['art_sha256'] for r in records},
            'original_banks':[{'archive':archive,'bank_index':bank,'decoded_sha256':sha(data)}
                              for (archive,bank),(data,_) in originals.banks.items()]}
        with (output/'LOCAL_MAP_GENERATION.json').open('x',encoding='utf-8',newline='\n') as stream:
            json.dump(report,stream,indent=2);stream.write('\n')
        with (output/'READ-ME-LOCAL-ONLY.txt').open('x',encoding='utf-8',newline='\n') as stream:
            stream.write('Generated from your original game and the separate map artwork.\n'
                         'Do not publish or redistribute generated XMT resources or this mods folder.\n'
                         'No executable, activation data or original archive was modified.\n')
    except Exception:
        try:
            with (output/'GENERATION_FAILED.txt').open('x',encoding='utf-8') as stream:
                stream.write('Incomplete local map generation. Do not install this output.\n')
        except OSError:
            pass
        raise
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_dir',type=Path)
    parser.add_argument('output_new_dir',type=Path)
    parser.add_argument('--art-dir',type=Path)
    args=parser.parse_args()
    report=generate(args.game_dir,args.output_new_dir,args.art_dir)
    print(json.dumps({'output':str(args.output_new_dir),'pack':report['pack'],
                      'generated_resources':len(report['resources']),'original_files_written':False}))


if __name__=='__main__':main()
