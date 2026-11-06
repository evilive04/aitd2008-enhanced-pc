"""Stage a new preview Release from explicit own-source and own-build inputs.

No game installation is an input. Build provenance is an integrity record,
not a signature, rights certificate or claim of complete gameplay parity.
The caller must supply a fresh output directory. Nothing is overwritten.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import tempfile
import zipfile

from package_public import (PackageError, checked_absolute, reject_reparse,
                            relative_name, snapshot_file)


VERSION = '0.1.0-rc3'
STATUS = 'preview-awaiting-user-gameplay'
TARGET = 'd8a7370a1f4f2418cfeb2f22c0d2e9dafa59f606ff83fbb6fc8311ba84df4818'
VIDEO_SHA = 'c8d4dc89d06e0c16a364f1fd255be0e7dae4e80ed26894cdd94e43d96723e77a'
VIDEO_BYTES = 121912755
# Inputs for local texture generation, outside the installed payload. Artwork
# distribution permission is a separate release prerequisite, not proven by SHA.
MAP_MEDIA = {
    'new-530.png': (96969, '2b057dded2c37a4e683d45c87cfaa8659ff1ee475f8667e7268ee22a15ee5068'),
    'new-534.png': (25148, 'ef180c7986c62a25131ad582c8deee86dcd29efd1e3fc16786f9804237f883dc'),
    'new-536.png': (99313, '2a2643356d3f7ce368d4c52bd2ab2c515b0c0652756d3b584bc9e01673370f25'),
    'new-538.png': (133136, '9b2f79dfc45e840b7c1c0333c0e885cc795cf086618327fcb17410bbc1eb94b1'),
    'new-541.png': (52456, '686b2f600f707271a5b8587257867d71c8c973be0af253e3858356aa4bed1567'),
    'new-545.png': (64857, '019df340d451282d02aefb94f83bf9c452c22657ac148a68470af138f89f42f8'),
}
BUILD_SCRIPTS = ('tools/build-performance.cmd', 'tools/build-volume-input.cmd',
                 'tools/build-custom-launcher.cmd', 'tools/build-installer.cmd')
COMPONENTS = {
    'renderer': ('build/performance/d3d9.dll', 'payload/d3d9.dll'),
    'input': ('build/volume-input/dinput8.dll', 'payload/dinput8.dll'),
    'launcher': ('build/launcher/Spustit hru.exe', 'payload/AITD Enhanced Launcher.exe'),
    'setup': ('build/installer/AITD Enhanced Setup.exe', 'Setup.exe'),
}
HELPERS = ('bank_container.py', 'compile_asset_mods.py', 'decode_bank.py',
           'extract_runtime_ui.py', 'generate_texture_mods.py', 'generate_ui_mods.py',
           'generate_map_mods.py',
           'inspect_bank.py', 'inspect_fat.py', 'inspect_scene_ui.py', 'inspect_vmf.py',
           'install_mod.py', 'lzo1x_block.py', 'semantic_ui_codec.py',
           'texture_recipe_keycaps.py', 'texture_recipe_lens.py',
           'texture_recipe_title.py', 'texture_recipe_water.py',
           'vmf_place_codec.py', 'vmf_vector_style.py')
COMPILER_HELPERS = ('compile_asset_mods.py', 'bank_container.py', 'decode_bank.py',
                    'inspect_bank.py', 'inspect_fat.py', 'lzo1x_block.py')
MOVIES = ('chapters', 'general', 'helper', 'helperqt', 'invntory', 'mainmenu',
          'phone', 'tips')
DOCS = ('README-INSTALL.md', 'RELEASE_STATUS.md', 'LICENSE', 'ASSET_RIGHTS.md',
        'THIRD_PARTY_NOTICES.md', 'requirements-install.txt')
ART = {
    'aitd_logo.png': '231772bf3592140438faa12c49638e15dc8b6484c29288266ac761c3dfe18f45',
    'episode_cards.png': 'bb3a595a97b5c7fc9ddcdee9bc72eec79b2337c0b495d4b04396fd5f491c929f',
    'cursor_metal.png': 'c82993013e5c5622f94c1004630d2d9bb022962d311be08d81755a12da2a5f8a',
    'cursor_metal.cur': 'e97513b6bd4dee0271b6d14f14c47c823b9c40adb43eb79ec2e0c00e9a5a532d',
    'slate.png': '1c35529753c96787fec0060281cdb830215f6bf1a816ab5e118f4809f9b37822',
    'roots-corner.png': '2ed2f05d262a3edc633071f049d65c77bee2f2cdbbaa9a3fbc37989149a422f9',
}


def source_mapping():
    """The manifest can select exactly this reviewed set, never arbitrary files."""
    result = {name: name for name in DOCS}
    result.update({'tools/' + name: 'tools/' + name for name in HELPERS})
    for name in COMPILER_HELPERS:
        result['payload/.enhanced/tools/' + name] = 'tools/' + name
    for name in (*[movie + '.json' for movie in MOVIES], 'README.md'):
        relative = 'release/semantic-ui-recipes/' + name
        result[relative] = relative
    result['configs/texture-recipes.json'] = 'configs/texture-recipes.json'
    result['configs/required-game-files.json'] = 'configs/required-game-files.json'
    result['configs/map-recipes.json'] = 'configs/map-recipes.json'
    result['payload/aitd-enhanced.ini'] = 'configs/enhanced-defaults.ini'
    for name in ART:
        result['payload/assets/ui/' + name] = 'assets/ui/' + name
    result['payload/assets/ui/PROVENANCE.md'] = 'assets/ui/PROVENANCE.md'
    return result


def media_mapping():
    result = {'payload/enhanced-ui/menu-loop.mp4': 'menu-loop.mp4'}
    result.update({'assets/map-icons/' + name: 'map-icons/' + name for name in MAP_MEDIA})
    return result


def json_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise PackageError('Duplicate JSON key: ' + key)
        result[key] = value
    return result


def parse_json(data):
    return json.loads(data.decode('utf-8-sig'), object_pairs_hook=json_object)


def digest(data):
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def exact_digest(data, record, label):
    if (not isinstance(record, dict) or type(record.get('bytes')) is not int or
            not isinstance(record.get('sha256'), str) or
            not re.fullmatch('[0-9a-f]{64}', record['sha256']) or
            digest(data) != {key: record[key] for key in ('bytes', 'sha256')}):
        raise PackageError('Integrity mismatch: ' + label)


def read_build(root, relative):
    """Build inputs use separate policy; source snapshot deliberately rejects PE."""
    current = root
    for part in PurePosixPath(relative).parts:
        current /= part
        reject_reparse(current)
    if root not in current.resolve(strict=True).parents:
        raise PackageError('Build component leaves source root')
    before = os.lstat(current)
    if not stat.S_ISREG(before.st_mode) or not 256 <= before.st_size <= 32 * 1024 * 1024:
        raise PackageError('Invalid component size/type: ' + relative)
    with current.open('rb') as stream:
        opened = os.fstat(stream.fileno())
        if (before.st_dev, before.st_ino, before.st_size) != (opened.st_dev, opened.st_ino, opened.st_size):
            raise PackageError('Build component changed while opening')
        data = stream.read(32 * 1024 * 1024 + 1)
    after = os.lstat(current)
    if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) != (
            after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns) or len(data) != before.st_size:
        raise PackageError('Build component changed while reading')
    current = root
    for part in PurePosixPath(relative).parts:
        current /= part
        reject_reparse(current)
    if hashlib.sha256(data).hexdigest() == TARGET:
        raise PackageError('An original game executable is never a mod component')
    return data


def validate_pe(data, component):
    """Read bounded PE metadata; custom exports distinguish our proxy DLLs."""
    def u16(at):
        if not 0 <= at <= len(data) - 2:
            raise PackageError('Truncated PE')
        return struct.unpack_from('<H', data, at)[0]

    def u32(at):
        if not 0 <= at <= len(data) - 4:
            raise PackageError('Truncated PE')
        return struct.unpack_from('<I', data, at)[0]

    if data[:2] != b'MZ':
        raise PackageError('Component is not a PE image')
    pe = u32(0x3c)
    if not 64 <= pe <= len(data) - 24 or data[pe:pe + 4] != b'PE\0\0':
        raise PackageError('Invalid PE signature')
    sections, optional_size, flags = u16(pe + 6), u16(pe + 20), u16(pe + 22)
    optional = pe + 24
    if (u16(pe + 4) != 0x14c or not 1 <= sections <= 96 or
            optional_size < 224 or optional + optional_size > len(data) or
            u16(optional) != 0x10b or u32(optional + 92) < 15):
        raise PackageError('Expected a bounded x86 PE32 mod component')
    table = optional + optional_size
    if table + sections * 40 > len(data):
        raise PackageError('Truncated PE section table')
    spans = []
    for at in range(table, table + sections * 40, 40):
        rva, raw_size, raw = u32(at + 12), u32(at + 16), u32(at + 20)
        if raw_size and (raw < table + sections * 40 or raw + raw_size > len(data)):
            raise PackageError('PE section exceeds image')
        spans.append((rva, raw_size, raw))

    def raw_at(rva, size):
        for base, length, raw in spans:
            if base <= rva and 0 <= size <= length and rva - base <= length - size:
                return raw + rva - base
        raise PackageError('PE directory is outside a file-backed section')

    if component in ('renderer', 'input'):
        if not flags & 0x2000:
            raise PackageError('Native proxy is not a DLL')
        export_rva, export_size = u32(optional + 96), u32(optional + 100)
        if export_size < 40:
            raise PackageError('Proxy export directory is missing')
        export = raw_at(export_rva, 40)
        count = u32(export + 24)
        if not 1 <= count <= 4096:
            raise PackageError('Invalid proxy export count')
        names_table = raw_at(u32(export + 32), count * 4)
        names = set()
        for at in range(names_table, names_table + count * 4, 4):
            name = bytearray()
            for offset in range(256):
                pos = raw_at(u32(at) + offset, 1)
                value = data[pos]
                if not value:
                    break
                name.append(value)
            else:
                raise PackageError('Unbounded export name')
            names.add(bytes(name))
        required = ({b'AITDPlayUiSound', b'Direct3DCreate9'} if component == 'renderer'
                    else {b'AITDGetPhoneState', b'AITDGetMenuState', b'DirectInput8Create'})
        if not required <= names:
            raise PackageError('Own proxy exports missing: ' + component)
    else:
        if flags & 0x2000:
            raise PackageError('Launcher/setup must be executables')
        clr_rva, clr_size = u32(optional + 96 + 14 * 8), u32(optional + 100 + 14 * 8)
        if clr_size < 72:
            raise PackageError('Expected an own managed launcher/setup')
        clr = raw_at(clr_rva, 72)
        metadata = raw_at(u32(clr + 8), u32(clr + 12))
        metadata_size = u32(clr + 12)
        if metadata_size > 16 * 1024 * 1024 or data[metadata:metadata + 4] != b'BSJB':
            raise PackageError('Invalid managed metadata')
        marker = b'EnhancedLauncher' if component == 'launcher' else b'EnhancedSetup'
        if marker + b'\0' not in data[metadata:metadata + metadata_size]:
            raise PackageError('Own managed component identity is absent')


def validate_provenance(root, provenance):
    if (provenance.get('schema') != 1 or provenance.get('kind') != 'local-source-build' or
            provenance.get('target_exe_sha256') != TARGET):
        raise PackageError('Original-Steam local-source-build provenance is required')
    public = parse_json(snapshot_file(root, 'release/public-files.json'))
    source_names = public.get('files')
    if not isinstance(source_names, list) or not all(isinstance(name, str) for name in source_names):
        raise PackageError('Public source allowlist is invalid')
    source_names = [relative_name(name) for name in source_names]
    required_sources = {name for name in source_names if name.startswith('src/')} | set(BUILD_SCRIPTS)
    # A missing entry point must not be concealed by reducing the source manifest.
    entry_points = {'src/performance/proxy.cpp', 'src/performance/volume_input.cpp',
                    'src/testing/native_test_launcher.cpp', 'src/installer/Setup.cs',
                    'src/launcher/LauncherUi.cs', 'src/diagnostics/d3d9.def',
                    'src/input/dinput8.def'}
    if not entry_points <= required_sources:
        raise PackageError('Public source tree omits a component entry point')
    records = provenance.get('sources')
    if not isinstance(records, dict) or set(records) != required_sources:
        raise PackageError('Provenance sources must exactly match public src/ plus four build scripts')
    for name in sorted(required_sources):
        exact_digest(snapshot_file(root, name), records[name], name)
    components = provenance.get('components')
    if not isinstance(components, dict) or set(components) != set(COMPONENTS):
        raise PackageError('Exactly four own build components are required')
    result = {}
    for component, (relative, target) in COMPONENTS.items():
        row = components[component]
        if not isinstance(row, dict) or row.get('path') != relative:
            raise PackageError('Build component path is fixed: ' + component)
        data = read_build(root, relative)
        exact_digest(data, row, relative)
        validate_pe(data, component)
        result[target] = data
        if component == 'setup':
            # The same own binary selects the removal UI only under this name.
            result['payload/uninstall mod.exe'] = data
    return result


def validate_selection(root, media, provenance_path):
    selection = parse_json(snapshot_file(root, 'release/runtime-files.json'))
    expected_source = source_mapping()
    expected_binary = {target: path for path, target in COMPONENTS.values()}
    expected_binary['payload/uninstall mod.exe'] = COMPONENTS['setup'][0]
    if (selection.get('schema') != 1 or selection.get('version') != VERSION or
            selection.get('status') != STATUS or selection.get('target_exe_sha256') != TARGET or
            selection.get('source_files') != expected_source or
            selection.get('build_files') != expected_binary or
            selection.get('media_files') != media_mapping()):
        raise PackageError('Runtime manifest differs from the explicit reviewed Release allowlist')
    proof = parse_json(provenance_path.read_bytes())
    result = validate_provenance(root, proof)
    for target, relative in expected_source.items():
        data = snapshot_file(root, relative)
        if relative.startswith('assets/ui/') and Path(relative).name in ART:
            if digest(data)['sha256'] != ART[Path(relative).name]:
                raise PackageError('Approved artwork changed: ' + relative)
        result[target] = data
    requirements = [line.strip() for line in result['requirements-install.txt'].decode('utf-8').splitlines()
                    if line.strip() and not line.lstrip().startswith('#')]
    if requirements != ['Pillow==13.0.0', 'numpy==2.5.4']:
        raise PackageError('Runtime dependencies differ from the approved recipe versions')
    media_file = media / 'menu-loop.mp4'
    reject_reparse(media_file)
    info = media_file.stat()
    if not stat.S_ISREG(info.st_mode) or info.st_size != VIDEO_BYTES:
        raise PackageError('Approved menu video is missing or has changed size')
    with media_file.open('rb') as stream:
        video = stream.read(VIDEO_BYTES + 1)
    reject_reparse(media_file)
    exact_digest(video, {'bytes': VIDEO_BYTES, 'sha256': VIDEO_SHA}, 'owner-approved menu-loop.mp4')
    result['payload/enhanced-ui/menu-loop.mp4'] = video
    map_recipe = parse_json(result['configs/map-recipes.json'])
    markers = map_recipe.get('markers')
    if (not isinstance(markers, list) or len(markers) != len(MAP_MEDIA) or
            not all(isinstance(row, dict) for row in markers) or
            {row.get('art'): row.get('art_sha256') for row in markers} !=
            {name: expected[1] for name, expected in MAP_MEDIA.items()}):
        raise PackageError('Map recipe artwork catalog differs from the fixed media allowlist')
    for name, (length, expected_sha) in MAP_MEDIA.items():
        data = snapshot_file(media, 'map-icons/' + name)
        exact_digest(data, {'bytes': length, 'sha256': expected_sha}, 'map-icons/' + name)
        if (data[:8] != b'\x89PNG\r\n\x1a\n' or len(data) < 33 or
                data[12:16] != b'IHDR' or struct.unpack_from('>II', data, 16) != (256, 256)):
            raise PackageError('Map media is not the fixed 256x256 PNG: ' + name)
        result['assets/map-icons/' + name] = data
    if sum(len(data) for data in result.values()) > 256 * 1024 * 1024:
        raise PackageError('Release exceeds the bounded snapshot limit')
    payload = {name.removeprefix('payload/'): digest(data)
               for name, data in sorted(result.items()) if name.startswith('payload/')}
    payload_manifest = {'schema': 1, 'version': VERSION, 'status': STATUS,
                        'target_exe_sha256': TARGET, 'files': payload}
    result['PAYLOAD_MANIFEST.json'] = json_bytes(payload_manifest)
    return result, proof


def json_bytes(value):
    return (json.dumps(value, indent=2, ensure_ascii=False) + '\n').encode('utf-8')


def write_new(root, relative, data):
    current = root
    reject_reparse(current)
    parts = PurePosixPath(relative).parts
    for part in parts[:-1]:
        current /= part
        try:
            current.mkdir()
        except FileExistsError:
            pass
        reject_reparse(current)
        if not current.is_dir():
            raise PackageError('Release parent is not a directory')
    target = current / parts[-1]
    with target.open('xb') as stream:
        stream.write(data)
    reject_reparse(target)


def build_release(source, media, provenance, output):
    source = checked_absolute(source)
    media = checked_absolute(media)
    provenance = checked_absolute(provenance)
    if (not source.is_dir() or not media.is_dir() or not provenance.is_file() or
            provenance.stat().st_size > 4 * 1024 * 1024):
        raise PackageError('Invalid source, media or provenance input')
    parent = checked_absolute(output.parent)
    output = parent / output.name
    if output.name in ('', '.', '..') or output == source or source in output.parents or output == media or media in output.parents:
        raise PackageError('Choose a NEW output directory outside source/media')
    # Complete all input policy/identity checks before creating output.
    files, proof = validate_selection(source, media, provenance)
    output.mkdir(exist_ok=False)
    try:
        release = output / 'Release'
        release.mkdir()
        for name, data in sorted(files.items()):
            write_new(release, name, data)
        report = {'schema': 1, 'version': VERSION, 'status': STATUS,
                  'target_exe_sha256': TARGET, 'gameplay_verification': 'awaiting-user-check',
                  'rights_clearance': 'not-certified', 'original_game_files_included': False,
                  'build_provenance_sha256': hashlib.sha256(json_bytes(proof)).hexdigest(),
                  'files': {name: digest(data) for name, data in sorted(files.items())}}
        report_bytes = json_bytes(report)
        write_new(release, 'RELEASE_MANIFEST.json', report_bytes)
        zip_name = 'AITD-Enhanced-PC-' + VERSION + '.zip'
        fd, temporary = tempfile.mkstemp(prefix='.release-', suffix='.zip.pending', dir=output)
        pending = Path(temporary)
        with os.fdopen(fd, 'w+b') as stream:
            identity = os.fstat(stream.fileno())
            with zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_STORED, allowZip64=True) as archive:
                # Snapshot bytes are used for folder and ZIP; no broad discovery.
                for name, data in sorted({**files, 'RELEASE_MANIFEST.json': report_bytes}.items()):
                    archive.writestr('Release/' + name, data)
            stream.flush()
            os.fsync(stream.fileno())
        reject_reparse(pending)
        current = os.lstat(pending)
        if (current.st_dev, current.st_ino) != (identity.st_dev, identity.st_ino):
            raise PackageError('Temporary ZIP was replaced; preserving the changed path')
        reject_reparse(output)
        # A raced pre-existing final archive is preserved, never overwritten.
        os.link(pending, output / zip_name)
        current = os.lstat(pending)
        if (current.st_dev, current.st_ino) == (identity.st_dev, identity.st_ino):
            pending.unlink()
        return report
    except Exception:
        # Preserve incomplete output for inspection; never recursively delete it.
        try:
            write_new(output, 'PACKAGE_FAILED.txt', b'Incomplete preview. Do not publish.\n')
        except (OSError, ValueError):
            pass
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', required=True, type=Path)
    parser.add_argument('--media-root', required=True, type=Path)
    parser.add_argument('--provenance', required=True, type=Path)
    parser.add_argument('--output-dir', required=True, type=Path)
    args = parser.parse_args()
    report = build_release(args.source_root, args.media_root, args.provenance, args.output_dir)
    print('Staged preview ' + report['version'] + ': ' + str(len(report['files'])) + ' explicit files.')
    print('Status: ' + report['status'] + '. This is not a gameplay or rights certificate.')


if __name__ == '__main__':
    main()
