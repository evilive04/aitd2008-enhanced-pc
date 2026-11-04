"""Generate eight enhanced UI movies locally from the supported original game.

Usage: python tools/generate_ui_mods.py GAME_DIR OUTPUT_NEW_DIR

Original executable, activation data and archives are opened only for reading.
The new output contains game-derived resources for local installation only;
never publish it or add it to a release. Requires Python 3.11+ and the project's
local generation dependencies. Independent texture recipes are separate.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from decode_bank import decode
from inspect_fat import read_index
from extract_runtime_ui import STOCK_EXE_SHA256,file_sha,plain_path
from semantic_ui_codec import build_movie,sha


TARGETS={'chapters':('a',361,10),'general':('a',367,22),
         'helper':('a',367,24),'helperqt':('a',397,2582),
         'invntory':('a',397,2583),'mainmenu':('a',367,20),
         'phone':('c',275,29),'tips':('a',367,21)}
MAX_BANK_BYTES=256*1024*1024
PACK_ID='enhanced-native-ui-v173'


def read_original_movies(game,recipes):
    exe=plain_path(game/'Alone.exe')
    if not exe.is_file() or not 0<exe.stat().st_size<=64*1024*1024 or file_sha(exe)!=STOCK_EXE_SHA256:
        raise ValueError('The unmodified supported Steam executable is required')
    movies={}
    proof={'executable_sha256':STOCK_EXE_SHA256,'archives':[],'banks':[]}
    for archive in ('a','c'):
        fat=plain_path(game/f'alone_compressed_{archive}.fat')
        big=plain_path(game/f'alone_compressed_{archive}.big')
        if not fat.is_file() or not big.is_file():
            raise ValueError('An original archive is missing')
        _,entries=read_index(fat)
        archive_size=big.stat().st_size
        proof['archives'].append({'archive':archive,'fat_sha256':file_sha(fat)})
        with big.open('rb') as stream:
            wanted=sorted({bank for a,bank,_ in TARGETS.values() if a==archive})
            for bank in wanted:
                if not 0<=bank<len(entries):
                    raise ValueError('Required original bank is absent')
                entry=entries[bank]
                if (entry['offset']<0 or entry['size']<=0 or
                    entry['padded_size']<entry['size'] or
                    entry['offset']+entry['padded_size']>archive_size):
                    raise ValueError('Original bank exceeds archive bounds')
                decoded,directory,report=decode(stream,entry,MAX_BANK_BYTES)
                if not report['stream_complete']:
                    raise ValueError('Original bank stream is incomplete')
                records={row['storage_index']:row for row in directory['files']}
                for name,target in TARGETS.items():
                    if target[:2]!=(archive,bank):
                        continue
                    record=records.get(target[2])
                    if record is None or record['filename'].lower()!=name+'.vmf':
                        raise ValueError('Original UI resource identity differs')
                    start,size=record['offset'],record['size']
                    if not 0<=start<len(decoded) or not 0<size<=16*1024*1024 or start+size>len(decoded):
                        raise ValueError('Original UI resource exceeds bounded bank span')
                    raw=bytes(decoded[start:start+size])
                    if sha(raw)!=recipes[name]['source_sha256']:
                        raise ValueError(f'Original {name} movie differs from supported source')
                    movies[name]=raw
                proof['banks'].append({'archive':archive,'bank_index':bank,
                    'decoded_sha256':sha(decoded),'decoded_bytes':len(decoded)})
    if set(movies)!=set(TARGETS):
        raise ValueError('A required original UI movie is missing')
    return movies,proof


def generate(game,output):
    game=plain_path(game).resolve(strict=True)
    if not game.is_dir():
        raise ValueError('Game path must be an existing directory')
    output=plain_path(output,missing_leaf=True)
    project=Path(__file__).resolve().parents[1]
    if (output.exists() or output==game or game in output.parents or
        output==project or project in output.parents):
        raise ValueError('Choose a NEW output directory outside the game and source repository')
    recipe_root=plain_path(project/'release'/'semantic-ui-recipes')
    recipes={}
    for name,target in TARGETS.items():
        path=plain_path(recipe_root/(name+'.json'))
        if not path.is_file() or not 0<path.stat().st_size<=32*1024*1024:
            raise ValueError('Semantic UI recipe is missing or exceeds bounds')
        recipe=json.loads(path.read_text(encoding='utf-8'))
        location=recipe.get('target',{})
        if (recipe.get('movie')!=name or
            tuple(location.get(k) for k in ('archive','bank_index','storage_index'))!=target):
            raise ValueError('Recipe target differs from the fixed supported UI catalog')
        recipes[name]=recipe
    originals,proof=read_original_movies(game,recipes)
    # Authenticate and construct every payload before creating any output.
    generated={name:build_movie(originals[name],recipe) for name,recipe in recipes.items()}
    output.mkdir()
    try:
        pack=output/'mods'/PACK_ID
        (pack/'ui').mkdir(parents=True)
        entries=[]
        for name,raw in generated.items():
            asset='ui/'+name+'.vmf'
            with (pack/asset).open('xb') as stream:
                stream.write(raw)
            archive,bank,slot=TARGETS[name]
            entries.append({'archive':archive,'bank_index':bank,'storage_index':slot,
                'original_sha256':recipes[name]['source_sha256'],'asset':asset,'sha256':sha(raw)})
        manifest={'version':1,'id':PACK_ID,'target_exe_sha256':STOCK_EXE_SHA256,
                  'replacements':entries}
        with (pack/'mod.json').open('x',encoding='utf-8',newline='\n') as stream:
            json.dump(manifest,stream,indent=2);stream.write('\n')
        proof.update({'version':1,'kind':'locally-generated-native-ui-movies',
            'redistributable':False,'generated_resources':entries,
            'coverage':'Eight VMF movies only. Native texture recipes are generated separately.',
            'recipe_sha256':{name:file_sha(recipe_root/(name+'.json')) for name in recipes},
            'preserved_game_data':'Read locally: fonts, bitmaps/styles, native scripts, masks and unchanged records.',
            'opaque_byte_insertions':False,'activation_or_original_archive_writes':False})
        with (output/'LOCAL_GENERATION_MANIFEST.json').open('x',encoding='utf-8',newline='\n') as stream:
            json.dump(proof,stream,indent=2);stream.write('\n')
        with (output/'READ-ME-LOCAL-ONLY.txt').open('x',encoding='utf-8',newline='\n') as stream:
            stream.write('Generated from your original game for local installation only.\n'
                         'Do not publish or redistribute this folder or add it to Git.\n'
                         'Original executable, activation and archives were not modified.\n')
    except Exception:
        with (output/'INCOMPLETE.txt').open('x',encoding='utf-8') as stream:
            stream.write('Generation did not complete. Do not install this directory.\n')
        raise
    return proof


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_dir',type=Path)
    parser.add_argument('output_new_dir',type=Path)
    args=parser.parse_args()
    proof=generate(args.game_dir,args.output_new_dir)
    print(json.dumps({'generated_movies':len(proof['generated_resources']),
        'output':str(args.output_new_dir),'redistributable':False}))


if __name__=='__main__':main()
