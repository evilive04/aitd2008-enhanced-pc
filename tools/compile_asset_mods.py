"""Compile native AITD asset packs into an original-preserving archive cache.

No game asset is distributed with this tool. Replacements must already be in
the game's own format; FBX/PNG/motlist are not converted by renaming them.
"""
import argparse
import configparser
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import stat
import struct
import tempfile
import zlib
from decode_bank import decode, align
from inspect_bank import inspect as inspect_bank
from inspect_fat import read_index
from bank_container import encode_raw_bank

EXE_SHA = 'd8a7370a1f4f2418cfeb2f22c0d2e9dafa59f606ff83fbb6fc8311ba84df4818'
SHA = re.compile(r'^[0-9a-f]{64}$')
ID = re.compile(r'^[a-z0-9][a-z0-9_-]{0,47}$')
MAX_BANK = 128 * 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def _reparse(info):
    # Path.is_junction does not exist on supported Python 3.11. lstat exposes
    # Windows reparse attributes without following a junction or symlink.
    return (stat.S_ISLNK(info.st_mode) or
            bool(getattr(info, 'st_file_attributes', 0) &
                 getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0x400)))


def _plain_path(path, missing_ok=False):
    """Check every lexical component BEFORE resolve can follow a reparse point."""
    path = Path(os.path.abspath(path))
    for current in (*reversed(path.parents), path):
        try:
            info = os.lstat(current)
        except FileNotFoundError:
            if missing_ok:
                continue
            raise
        if _reparse(info):
            raise ValueError('Reparse-point paths are unsupported')
    return path


def _identity(path):
    info = os.lstat(path)
    if _reparse(info):
        raise ValueError('Reparse-point paths are unsupported')
    return info.st_dev, info.st_ino


def _remove_pending(destination, cache, identity, cache_identity):
    """Remove only our still-owned direct child; never follow redirected paths."""
    destination, cache = Path(destination), Path(cache)
    _plain_path(cache)
    if destination.parent != cache or not re.fullmatch(r'[0-9a-f]{32}\.pending-[^/\\]+', destination.name):
        raise ValueError('Pending cache is outside its intended directory')
    if _identity(cache) != cache_identity:
        raise ValueError('Cache directory changed during compilation')
    if not destination.exists():
        return
    _plain_path(destination)
    if _identity(destination) != identity:
        raise ValueError('Pending cache ownership changed during compilation')
    if destination.resolve(strict=True).parent != cache.resolve(strict=True):
        raise ValueError('Pending cache escaped its intended directory')
    # Reject links throughout the tree before recursive removal. rmtree never
    # receives a computed target until its absolute containment is established.
    for folder, directories, files in os.walk(destination, followlinks=False):
        _plain_path(folder)
        for name in directories + files:
            if _reparse(os.lstat(Path(folder) / name)):
                raise ValueError('Pending cache contains a reparse point')
    shutil.rmtree(destination)


def contained(root, relative):
    """Reject drive/ADS/UNC, traversal and symlink or junction escapes."""
    if not isinstance(relative, str) or not relative or '\\' in relative or ':' in relative:
        raise ValueError('Asset path must be a relative slash-separated path')
    pieces = relative.split('/')
    if any(p in ('', '.', '..') or p.endswith((' ', '.')) for p in pieces):
        raise ValueError('Unsafe asset path')
    root = _plain_path(root).resolve(strict=True)
    target = _plain_path(root.joinpath(*pieces))
    resolved = target.resolve(strict=True)
    if resolved == root or root not in resolved.parents:
        raise ValueError('Asset escapes pack root')
    return resolved


def rebuild_bank(source, replacements):
    """Repack the observed PC BANK metadata, retaining every opaque identity."""
    old = inspect_bank(source)
    files = sorted(old['files'], key=lambda f: f['offset'])
    if not replacements or not set(replacements) <= {f['storage_index'] for f in files}:
        raise ValueError('Replacement slots absent or invalid')
    start = struct.unpack_from('<I', source, 68)[0]
    if files[0]['offset'] != start or align(files[-1]['offset'] + files[-1]['size'], 2048) != len(source):
        raise ValueError('Unsupported payload extent')
    if old['packed_size_field'] != sum(align(f['size'], 32) for f in files):
        raise ValueError('Unsupported packed size relationship')
    sizes = next(s for s in old['sections'] if s['name'] == 'sizes')
    output = bytearray(source[:start])
    previous_end, packed_size = start, 0
    for f in files:
        alignment = f['opaque_words'][2]
        if alignment not in (16, 32) or f['offset'] != align(previous_end, alignment):
            raise ValueError('Unsupported payload alignment')
        original = source[f['offset']:f['offset'] + f['size']]
        payload = replacements.get(f['storage_index'], original)
        offset = align(len(output), alignment)
        padding = source[previous_end:f['offset']]
        needed = offset - len(output)
        output.extend(padding if len(padding) == needed else bytes(needed))
        output.extend(payload)
        struct.pack_into('<II', output, sizes['offset'] + 8 + f['storage_index'] * 20, offset, len(payload))
        packed_size += align(len(payload), 32)
        previous_end = f['offset'] + f['size']
    end = len(output)
    struct.pack_into('<I', output, sizes['offset'] + 8 + old['file_count'] * 20, end)
    needed = align(end, 4096) - end
    trailing = source[previous_end:]
    output.extend(trailing if len(trailing) == needed else bytes(needed))
    if len(output) > MAX_BANK:
        raise ValueError('Rebuilt BANK exceeds limit')
    struct.pack_into('<II', output, 24, len(output), packed_size)
    struct.pack_into('<I', output, sizes['offset'] + 4, zlib.crc32(output[sizes['offset'] + 8:sizes['end']]) ^ 0xffffffff)
    struct.pack_into('<I', output, 4, zlib.crc32(output[8:72]) ^ 0xffffffff)
    new = inspect_bank(bytes(output))
    if old['raw_type_map'] != new['raw_type_map'] or old['year'] != new['year']:
        raise ValueError('Unintended BANK metadata change')
    for before, after in zip(old['files'], new['files']):
        if any(before[k] != after[k] for k in ('filename', 'tree_path', 'tree_index', 'storage_index', 'opaque_words')):
            raise ValueError('Unintended resource identity change')
        expected = replacements.get(before['storage_index'], source[before['offset']:before['offset'] + before['size']])
        if after['payload_sha256'] != digest(expected):
            raise ValueError('Rebuilt payload hash mismatch')
    return bytes(output)


def interface_policy(game):
    """Match the launcher's UI switches without changing third-party packs.

    Generated UI movies intentionally alter the native chrome. Disabling only
    the C++ presenter leaves that altered fallback behind. Use the player's
    original movie when its replacement presenter is disabled. A missing INI
    preserves the standalone compiler's historical behavior.
    """
    path = _plain_path(game / 'aitd-enhanced.ini', missing_ok=True)
    if not path.exists():
        return None
    if not path.is_file() or path.stat().st_size > 1024 * 1024:
        raise ValueError('Mod configuration is missing or exceeds the size limit')
    raw = path.read_bytes()
    encoding = 'utf-16' if raw[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig'
    text = raw.decode(encoding)
    section = ''
    values = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith((';', '#')):
            continue
        if line.startswith('[') and line.endswith(']'):
            section = line[1:-1].strip().casefold()
        elif section == 'ui' and '=' in line:
            key, value = line.split('=', 1)
            value = value.strip()
            if len(value) >= 2 and value[0] == value[-1] and value[0] in ('"', "'"):
                value = value[1:-1]
            values.setdefault(key.strip().casefold(), value.casefold())
    # Native ConfigFlag defaults these opt-in presenters to false.
    interface = values.get('remakeinterface', 'false') in ('true', '1')
    menu = values.get('remakemenu', 'false') in ('true', '1')
    return {'interface': interface, 'menu': menu}


UI_MOVIES = {
    ('a', 361, 10): 'chapters', ('a', 367, 22): 'general',
    ('a', 367, 24): 'helper', ('a', 397, 2582): 'helperqt',
    ('a', 397, 2583): 'invntory', ('a', 367, 20): 'mainmenu',
    ('c', 275, 29): 'phone', ('a', 367, 21): 'tips',
}


def load_packs(mods, ui_policy=None):
    records, packs, keys = [], [], set()
    mods = _plain_path(mods)
    for folder in sorted(mods.iterdir()):
        if folder.name.startswith('.'):
            continue
        _plain_path(folder)
        if not folder.is_dir():
            continue
        manifest = folder / 'mod.json'
        if not manifest.exists():
            continue
        manifest = contained(folder, 'mod.json')
        if manifest.stat().st_size > 1024 * 1024:
            raise ValueError('Manifest exceeds limit')
        data = json.loads(contained(folder, 'mod.json').read_text(encoding='utf-8-sig'))
        if data.get('version') != 1 or not ID.fullmatch(data.get('id', '')) or data.get('target_exe_sha256') != EXE_SHA:
            raise ValueError('Pack version/id/executable mismatch')
        replacements = data.get('replacements')
        if not isinstance(replacements, list) or not 0 < len(replacements) <= 4096:
            raise ValueError('Pack must declare bounded replacement entries')
        pack = {'id': data['id'], 'manifest_sha256': file_sha(manifest)}
        packs.append(pack)
        omitted = []
        declared = set()
        for item in replacements:
            archive, bank, slot = item.get('archive'), item.get('bank_index'), item.get('storage_index')
            if archive not in ('a', 'b', 'c') or type(bank) is not int or type(slot) is not int or min(bank, slot) < 0:
                raise ValueError('Invalid native resource identity')
            key = archive, bank, slot
            if key in declared:
                raise ValueError('Pack declares a duplicate native resource')
            declared.add(key)
            if any(not isinstance(item.get(k), str) or not SHA.fullmatch(item[k]) for k in ('original_sha256', 'sha256')):
                raise ValueError('Original and replacement SHA256 required')
            asset = contained(folder, item.get('asset'))
            if not asset.is_file() or not 0 < asset.stat().st_size <= 64 * 1024 * 1024:
                raise ValueError('Replacement file size outside limit')
            payload = asset.read_bytes()
            if digest(payload) != item['sha256']:
                raise ValueError('Replacement SHA256 mismatch')
            movie = UI_MOVIES.get(key)
            if (ui_policy is not None and data['id'] == 'enhanced-native-ui-v173'
                    and movie is not None and
                    (not ui_policy['interface'] or (movie == 'mainmenu' and not ui_policy['menu']))):
                omitted.append(list(key))
                continue
            if key in keys:
                raise ValueError('Conflicting asset packs: duplicate native resource')
            keys.add(key)
            records.append((key, item, payload))
        if omitted:
            # Only changed selections get a new cache identity. The default
            # enabled layout reuses its existing immutable archive generation.
            pack['omitted_ui_resources'] = sorted(omitted)
    return records, packs


def activate(cache, report):
    """Verify the immutable generation, then atomically publish its selector."""
    cache = _plain_path(cache)
    generation = report['generation']
    if not re.fullmatch('[0-9a-f]{32}', generation):
        raise ValueError('Invalid generation identity')
    ini = configparser.ConfigParser()
    ini.optionxform = str
    ini['Cache'] = {'Version': '1', 'Generation': generation, 'ExecutableSHA256': EXE_SHA}
    for entry in report['archives']:
        archive = entry['archive']
        if archive not in ('a', 'b', 'c'):
            raise ValueError('Invalid cached archive')
        for extension in ('big', 'fat'):
            expected = entry[f'{extension}_sha256']
            asset = contained(cache, f'{generation}/alone_compressed_{archive}.{extension}')
            if not SHA.fullmatch(expected) or file_sha(asset) != expected:
                raise ValueError('Existing cache hash mismatch')
        ini[f'Archive{archive.upper()}'] = {'Enabled': '1', 'BigSHA256': entry['big_sha256'], 'FatSHA256': entry['fat_sha256']}
    cache_identity = _identity(cache)
    fd, name = tempfile.mkstemp(prefix='active.', suffix='.pending', dir=cache)
    pending = Path(name)
    info = os.fstat(fd)
    identity = info.st_dev, info.st_ino
    failure = None
    try:
        stream = os.fdopen(fd, 'w', encoding='utf-16')
        fd = -1  # The stream now owns the descriptor, including on write failure.
        with stream:
            ini.write(stream)
        _plain_path(cache / 'active.ini', missing_ok=True)
        if _identity(cache) != cache_identity:
            raise ValueError('Cache directory changed during selector publication')
        os.replace(pending, cache / 'active.ini')
    except BaseException as error:
        failure = error
        raise
    finally:
        if fd >= 0:
            os.close(fd)
        # Never remove a same-name file created by another process, nor follow
        # a redirected parent during failure cleanup.
        try:
            _plain_path(cache)
            if _identity(cache) != cache_identity:
                raise ValueError('Cache directory changed during selector cleanup')
            if pending.exists() and _identity(pending) == identity:
                _plain_path(pending)
                pending.unlink()
        except (OSError, ValueError) as cleanup_error:
            if failure is None:
                raise
            failure.add_note('Selector cleanup was refused: ' + str(cleanup_error))



def deactivate_empty(mods):
    mods = _plain_path(mods)
    cache = _plain_path(mods / '.cache', missing_ok=True)
    selector = _plain_path(cache / 'active.ini', missing_ok=True)
    if selector.exists():
        selector.unlink()
    return {'version':1,'generation':None,'packs':[],'archives':[],'deactivated':True,'original_files_written':False}


def compile_mods(game, mods):
    game, mods = _plain_path(game).resolve(strict=True), _plain_path(mods).resolve(strict=True)
    if mods == game or game in mods.parents:
        # Only the conventional game/mods subdirectory is admitted inside game.
        if mods != game / 'mods':
            raise ValueError('Use game/mods or a separate private mods directory')
    if file_sha(game / 'Alone.exe') != EXE_SHA:
        raise ValueError('Unsupported game executable')
    records, packs = load_packs(mods, interface_policy(game))
    if not records:
        return deactivate_empty(mods)
    source_identity = {a: file_sha(game / f'alone_compressed_{a}.fat') for a in sorted({r[0][0] for r in records})}
    generation = digest(json.dumps({'compiler_revision': 2, 'packs': packs, 'fat': source_identity}, sort_keys=True).encode())[:32]
    cache = _plain_path(mods / '.cache', missing_ok=True)
    cache.mkdir(exist_ok=True)
    _plain_path(cache)
    destination = cache / generation
    if destination.exists():
        report_path = contained(cache, f'{generation}/compile-report.json')
        report = json.loads(report_path.read_text(encoding='utf-8'))
        if report.get('version') != 1 or report.get('generation') != generation or report.get('packs') != packs:
            raise ValueError('Existing cache report identity mismatch')
        activate(cache, report)
        return report
    destination = Path(tempfile.mkdtemp(prefix=generation + '.pending-', dir=cache))
    pending_identity, cache_identity = _identity(destination), _identity(cache)
    published = False
    failure = None
    try:
        report = {'version': 1, 'compiler_revision': 2, 'generation': generation, 'packs': packs, 'archives': [], 'original_files_written': False}
        for archive in source_identity:
            fat_path, big_path = (game / f'alone_compressed_{archive}.{ext}' for ext in ('fat', 'big'))
            fat_bytes, entries = read_index(fat_path)
            fat = bytearray(fat_bytes)
            grouped = {}
            for (a, bank, slot), item, payload in records:
                if a == archive:
                    grouped.setdefault(bank, {})[slot] = (item, payload)
            required = big_path.stat().st_size + sum(len(p) for slots in grouped.values() for _, p in slots.values()) + 128 * 1024 * 1024 * len(grouped)
            if shutil.disk_usage(cache).free < required:
                raise ValueError('Insufficient space for independent archive cache')
            cached_big = destination / big_path.name
            shutil.copyfile(big_path, cached_big)  # Never use hardlinks: append must not touch originals.
            changes = []
            with big_path.open('rb') as original, cached_big.open('ab') as output:
                for bank, slots in sorted(grouped.items()):
                    if not 0 <= bank < len(entries):
                        raise ValueError('Bank index outside FAT')
                    entry = entries[bank]
                    source, metadata, _ = decode(original, entry, MAX_BANK)
                    by_slot = {f['storage_index']: f for f in metadata['files']}
                    replacements = {}
                    for slot, (item, payload) in slots.items():
                        if slot not in by_slot or by_slot[slot]['payload_sha256'] != item['original_sha256']:
                            raise ValueError('Original resource hash mismatch')
                        # Native identity, format and skeleton compatibility remain pack-author responsibilities.
                        replacements[slot] = payload
                    rebuilt = rebuild_bank(bytes(source), replacements)
                    original.seek(entry['offset'])
                    prefix = original.read(56)
                    start = struct.unpack_from('>I', prefix, 24)[0]
                    if not 2048 <= start <= 1024 * 1024:
                        raise ValueError('Unsupported source header size')
                    original.seek(entry['offset'])
                    encoded = encode_raw_bank(rebuilt, original.read(start))
                    decoded, _, _ = decode(io.BytesIO(encoded), dict(entry, offset=0, size=len(encoded)), MAX_BANK)
                    if bytes(decoded) != rebuilt:
                        raise ValueError('Encoded cache failed exact round trip')
                    offset = align(output.tell(), 2048)
                    output.write(bytes(offset - output.tell()))
                    output.write(encoded)
                    at = 384 + 16 + len(entries) * 8 + bank * 32
                    struct.pack_into('<4Q', fat, at, len(encoded), align(len(encoded), 2048), align(len(rebuilt), 4096) + 6144, offset)
                    changes.append({'bank': bank, 'slots': sorted(slots), 'decoded_sha256': digest(rebuilt)})
            cached_fat = destination / fat_path.name
            cached_fat.write_bytes(fat)
            read_index(cached_fat)
            big_hash, fat_hash = file_sha(cached_big), file_sha(cached_fat)
            report['archives'].append({'archive': archive, 'big_sha256': big_hash, 'fat_sha256': fat_hash, 'changes': changes})
        (destination / 'compile-report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        destination.rename(cache / generation)
        published = True
        activate(cache, report)  # Publish only a complete validated generation.
        return report
    except BaseException as error:
        failure = error
        raise
    finally:
        if not published:
            try:
                _remove_pending(destination, cache, pending_identity, cache_identity)
            except (OSError, ValueError) as cleanup_error:
                if failure is None:
                    raise
                failure.add_note('Pending cache cleanup was refused: ' + str(cleanup_error))



if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('mods', type=Path)
    args = parser.parse_args()
    print(json.dumps(compile_mods(args.game, args.mods), indent=2))
