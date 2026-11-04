"""Generate four native UI PNGs locally from the user's original game.

No game content is embedded in this program. Input files are read only; the
original executable, licensing and archives are never changed. The NEW output
directory contains game-derived images for local installation, not publication.
Requires Python 3.11+ and Pillow (see requirements-ui.txt).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import stat
import struct

from PIL import Image, ImageChops, ImageDraw

from decode_bank import decode
from inspect_fat import read_index
from inspect_vmf import inspect as inspect_vmf


STOCK_EXE_SHA256 = 'd8a7370a1f4f2418cfeb2f22c0d2e9dafa59f606ff83fbb6fc8311ba84df4818'
MAX_BANK_BYTES = 128 * 1024 * 1024
BANKS = {
    306: 'e85e6e4833008141836efe761cc42272c0d2e121b52ef9bace4b2a179c63ec4b',
    397: '04d4674e8fbb3aa4e05d87ce2f1a9e0ef54c5b7b95e605d910dc04fbbf878b56',
}
RESOURCES = {
    'movie': (397, 2583, 'invntory.vmf',
              '7ee826f3ca8f268ee542965af10d6d9a090ea20fe747acf0dee8a043bcac213b'),
    'mouse': (306, 249, 'fla_0011.xmt',
              '38d168a718bf8e447da493b392541e4b73cab79f906faed8a84f27602eb06db2'),
    'bullet': (306, 236, 'fla_0073.xmt',
               '87f4d3b5f194cd4ff58829ee32deb0087f26d10f4c358ca22b8679d45caac816'),
    'flame': (306, 238, 'fla_0060.xmt',
              'e109871bfb9aff88c7e4a2529b4ed522bc7461972501e060a10e85ec9034e4da'),
}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def plain_path(path: Path, *, missing_leaf: bool = False) -> Path:
    """Reject redirected lexical path components on Python 3.11 as well."""
    path = Path(os.path.abspath(path))
    for component in (*reversed(path.parents), path):
        try:
            info = os.lstat(component)
        except FileNotFoundError:
            if missing_leaf and component == path:
                continue
            raise
        if stat.S_ISLNK(info.st_mode) or (
                getattr(info, 'st_file_attributes', 0) &
                getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0x400)):
            raise ValueError('Reparse-point and symlink paths are unsupported')
    return path


def read_resources(game: Path) -> tuple[dict[str, bytes], dict]:
    executable = plain_path(game / 'Alone.exe')
    if not executable.is_file() or not 0 < executable.stat().st_size <= 64 * 1024 * 1024:
        raise ValueError('Original game executable is missing or outside size bounds')
    if file_sha(executable) != STOCK_EXE_SHA256:
        raise ValueError('This helper requires the unmodified supported Steam executable')
    fat = plain_path(game / 'alone_compressed_a.fat')
    big = plain_path(game / 'alone_compressed_a.big')
    if not fat.is_file() or not big.is_file():
        raise ValueError('Original archive A is missing')
    _, index = read_index(fat)
    resources: dict[str, bytes] = {}
    bank_proof = []
    with big.open('rb') as stream:
        for bank_index, expected_sha in BANKS.items():
            if not 0 <= bank_index < len(index):
                raise ValueError('Required bank is absent from original FAT')
            entry = index[bank_index]
            if (entry['offset'] < 0 or entry['size'] <= 0 or
                    entry['offset'] + entry['padded_size'] > big.stat().st_size):
                raise ValueError('Required bank exceeds archive bounds')
            decoded, directory, proof = decode(stream, entry, MAX_BANK_BYTES)
            if not proof['stream_complete'] or sha(decoded) != expected_sha:
                raise ValueError('Original bank identity differs; modified archives are unsupported')
            by_slot = {record['storage_index']: record for record in directory['files']}
            for key, (wanted_bank, slot, filename, expected) in RESOURCES.items():
                if wanted_bank != bank_index:
                    continue
                record = by_slot.get(slot)
                if (record is None or record['filename'] != filename or
                        (key != 'movie' and '/invntory/' not in record['tree_path'].lower())):
                    raise ValueError('Original resource identity differs')
                start, size = record['offset'], record['size']
                if not 0 < size <= 16 * 1024 * 1024:
                    raise ValueError('Resource size exceeds local UI extraction bounds')
                payload = bytes(decoded[start:start + size])
                if len(payload) != size or sha(payload) != expected:
                    raise ValueError('Original resource checksum differs')
                resources[key] = payload
            bank_proof.append({'archive': 'a', 'bank_index': bank_index,
                               'decoded_sha256': expected_sha,
                               'decoded_bytes': len(decoded)})
    if set(resources) != set(RESOURCES):
        raise ValueError('A required original UI resource was not obtained')
    return resources, {'executable_sha256': STOCK_EXE_SHA256,
                       'fat_sha256': file_sha(fat), 'banks': bank_proof}


def texture_image(raw: bytes, expected_size: tuple[int, int]) -> Image.Image:
    if len(raw) < 4096 or raw[512:516] != b'TEXT':
        raise ValueError('Unsupported original texture header')
    width, height = struct.unpack_from('<II', raw, 524)
    fmt = struct.unpack_from('<I', raw, 540)[0]
    if (width, height) != expected_size or fmt != 15:
        raise ValueError('Original UI texture dimensions or BC3 format differ')
    length = width * height  # BC3: 16 bytes per 4x4 block.
    if len(raw) < 4096 + length:
        raise ValueError('Original texture level is truncated')
    return Image.frombytes('RGBA', (width, height), raw[4096:4096 + length], 'bcn', (3,))


def native_mouse_points(movie: bytes) -> list[tuple[int, int]]:
    """Read only the supported original mask triangles, never bundle them."""
    inspect_vmf(movie)
    cursor, found = 72, None
    while cursor < len(movie) - 16:
        kind, size = struct.unpack_from('<II', movie, cursor)
        if kind == 2 and struct.unpack_from('<I', movie, cursor + 8)[0] == 162:
            if found is not None or size < 44:
                raise ValueError('Ambiguous or truncated native mouse mask')
            geometry = struct.unpack_from('<I', movie, cursor + 28)[0] + 4
            if not 36 <= geometry <= size - 8:
                raise ValueError('Native mask geometry offset exceeds its shape')
            at = cursor + geometry
            command, count, style = struct.unpack_from('<IHH', movie, at)
            if (command, count, style) != (8, 4, 1) or geometry + 8 + count * 12 > size:
                raise ValueError('Unsupported native mouse mask triangles')
            found = [struct.unpack_from('<hh', movie, at + 8 + point * 4)
                     for point in range(count * 3)]
        cursor += size
    if found is None:
        raise ValueError('Original inventory mouse mask is missing')
    return found


def mouse_image(atlas: Image.Image, points: list[tuple[int, int]], tx: int) -> Image.Image:
    # Original inventory sprite164 PC mouse frame placements. The original
    # fill uses20twips/texel; both mask and bitmap placement scale is0.78125.
    ty = -1156
    pixels = [(x / 20 - tx / 15.625, y / 20 - ty / 15.625) for x, y in points]
    box = (math.floor(min(x for x, _ in pixels)), math.floor(min(y for _, y in pixels)),
           math.ceil(max(x for x, _ in pixels)), math.ceil(max(y for _, y in pixels)))
    if (box[0] < 0 or box[1] < 0 or box[2] > atlas.width or box[3] > atlas.height or
            box[2] - box[0] != 65 or box[3] - box[1] != 55):
        raise ValueError('Native mouse crop differs from supported original art')
    result = atlas.crop(box)
    supersample = 8
    mask = Image.new('L', (result.width * supersample, result.height * supersample))
    draw = ImageDraw.Draw(mask)
    for triangle in range(4):
        draw.polygon([((x - box[0]) * supersample, (y - box[1]) * supersample)
                      for x, y in pixels[triangle * 3:triangle * 3 + 3]], fill=255)
    mask = mask.resize(result.size, Image.Resampling.LANCZOS)
    result.putalpha(ImageChops.multiply(result.getchannel('A'), mask))
    return result


def extract(game: Path, output: Path) -> dict:
    game = plain_path(game).resolve(strict=True)
    if not game.is_dir():
        raise ValueError('Game path must be an existing directory')
    output = plain_path(output, missing_leaf=True)
    project = Path(__file__).resolve().parents[1]
    if output.exists() or output == game or game in output.parents or output == project or project in output.parents:
        raise ValueError('Choose a NEW output directory outside the game and source repository')
    resources, source_proof = read_resources(game)
    atlas = texture_image(resources['mouse'], (1024, 512))
    points = native_mouse_points(resources['movie'])
    bullet = texture_image(resources['bullet'], (128, 32))
    bbox = bullet.getchannel('A').getbbox()
    if bbox is None:
        raise ValueError('Original bullet image is transparent')
    bullet = bullet.crop(bbox).transpose(Image.Transpose.ROTATE_270)
    if bullet.size != (31, 88):
        raise ValueError('Original bullet alpha footprint differs')
    images = {
        'mouse_left_native.png': mouse_image(atlas, points, -5962),
        'mouse_right_native.png': mouse_image(atlas, points, -4618),
        'ammo_bullet_native.png': bullet,
        'ammo_flame_native.png': texture_image(resources['flame'], (32, 64)),
    }
    output.mkdir()  # Exclusive claim, after all source parsing/rendering.
    target = output / 'assets' / 'ui'
    target.mkdir(parents=True)
    report = {'schema': 1, 'scope': 'locally-generated original-game UI images',
              'redistributable_game_content': False, 'original_files_written': False,
              'activation_or_protection_modified': False, 'source': source_proof,
              'resources': {key: {'archive': 'a', 'bank_index': spec[0],
                                   'storage_index': spec[1], 'filename': spec[2],
                                   'sha256': spec[3]} for key, spec in RESOURCES.items()},
              'files': []}
    try:
        for name, image in images.items():
            destination = target / name
            with destination.open('xb') as stream:
                image.save(stream, 'PNG')
            report['files'].append({'file': 'assets/ui/' + name,
                                    'dimensions': list(image.size),
                                    'sha256': file_sha(destination)})
        with (output / 'LOCAL_GENERATION_MANIFEST.json').open('x', encoding='utf-8') as stream:
            json.dump(report, stream, indent=2)
        with (output / 'READ-ME-LOCAL-ONLY.txt').open('x', encoding='utf-8') as stream:
            stream.write('Generated from your own original game. For local use with the mod only.\n'
                         'Do not upload these original-game-derived PNG files or this directory to GitHub.\n'
                         'No game archives, executable or activation were modified.\n')
    except Exception:
        # Never recursively delete a path that could have been replaced by
        # somebody else. Preserve the partial owned output for inspection.
        try:
            with (output / 'EXTRACTION_FAILED.txt').open('x', encoding='utf-8') as stream:
                stream.write('Extraction did not complete. Do not install this partial output.\n')
        except OSError:
            pass
        raise
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path, help='Original supported Steam game directory')
    parser.add_argument('output', type=Path, help='NEW directory outside the game and repository')
    args = parser.parse_args()
    report = extract(args.game, args.output)
    print(json.dumps({'output': str(args.output), 'files': report['files'],
                      'original_files_written': False, 'redistributable_game_content': False}, indent=2))


if __name__ == '__main__':
    main()
