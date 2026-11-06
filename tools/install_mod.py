"""Install only this mod's files; derive game assets locally, preserve the game.

Requires a clean, legitimately activated supported Steam installation. Never
changes Alone.exe, activation, original archives or the user's saved games.
Use the hash-listed file list shipped in a release; hashes provide integrity,
not publisher authentication. Do not use untrusted release archives.
"""
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import struct
import subprocess
import sys
import tempfile

from extract_runtime_ui import STOCK_EXE_SHA256, file_sha, plain_path

FORBIDDEN = {'alone.exe', 'paul.dll', 'binkw32.dll', 'lang.ini', 'prefs.aitd', 'save.aitd', 'res.aitd'}
GAME_SUFFIXES = {'.big', '.fat', '.aitd', '.bik', '.dmp', '.hpd', '.dms'}
MAX_FILES = 4096
DOS_NAMES = {'con', 'prn', 'aux', 'nul', *(f'com{n}' for n in range(1, 10)), *(f'lpt{n}' for n in range(1, 10))}


def relative_name(value: object) -> str:
    if not isinstance(value, str) or not value or '\\' in value or ':' in value:
        raise ValueError('Invalid installation path')
    path = PurePosixPath(value)
    if path.is_absolute() or any(p in ('', '.', '..') or p.endswith((' ', '.')) or
            p.split('.')[0].casefold() in DOS_NAMES or any(ord(c) < 32 for c in p) for p in value.split('/')):
        raise ValueError('Invalid installation path')
    if path.name.casefold() in FORBIDDEN or path.suffix.casefold() in GAME_SUFFIXES:
        raise ValueError('Game files are never installation targets')
    if path.parts[0] not in ('assets', 'enhanced-ui', '.enhanced', 'mods') and value not in (
            'd3d9.dll', 'dinput8.dll', 'AITD Enhanced Launcher.exe', 'aitd-enhanced.ini', 'uninstall mod.exe'):
        raise ValueError('Unreviewed installation target')
    return path.as_posix()


def below(root: Path, relative: str) -> Path:
    path = root.joinpath(*PurePosixPath(relative_name(relative)).parts)
    # Check lexical components before any filesystem traversal can redirect.
    for part in (*reversed(path.parents), path):
        if not part.exists() and not part.is_symlink():
            continue
        info = os.lstat(part)
        if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise ValueError('Installation through links or junctions is unsupported')
    return path


def verify_game(game: Path) -> Path:
    game = plain_path(game)
    cache_suffix = '\\mods\\.cache\\' + '0' * 32 + '\\alone_compressed_c.big'
    if game.drive.startswith('\\\\') or len(str(game)) + len(cache_suffix) >= 260:
        raise ValueError('Use a shorter local game folder. The original game requires paths below the Windows MAX_PATH limit.')
    executable = plain_path(game / 'Alone.exe')
    if file_sha(executable) != STOCK_EXE_SHA256:
        raise ValueError('Select the unchanged supported Steam game. Start and activate it normally first.')
    catalog = Path(__file__).resolve().parents[1] / 'configs' / 'required-game-files.json'
    required = json.loads(plain_path(catalog).read_text(encoding='utf-8'))
    names = required.get('files')
    if required.get('schema') != 1 or not isinstance(names, list) or len(names) != 107:
        raise ValueError('Setup original-file catalog is incomplete')
    for name in names:
        if not isinstance(name, str) or '\\' in name or ':' in name or any(p in ('', '.', '..') for p in name.split('/')):
            raise ValueError('Invalid original-file catalog')
        try:
            source = plain_path(game.joinpath(*PurePosixPath(name).parts), missing_leaf=True)
        except FileNotFoundError:
            raise ValueError('Original game data is missing: ' + name + '. Use Steam Verify installed files, then start the original game normally.') from None
        if not source.is_file() or source.stat().st_size == 0:
            raise ValueError('Original game data is missing: ' + name + '. Use Steam Verify installed files, then start the original game normally.')
    if (game / '.enhanced-install.json').exists():
        raise ValueError('An Enhanced installation already exists. Keep its files and use a clean game folder.')
    if os.name == 'nt':
        tasklist = Path(os.environ['WINDIR']) / 'System32' / 'tasklist.exe'
        process_list = subprocess.run([str(tasklist), '/FI', 'IMAGENAME eq Alone.exe', '/FO', 'CSV', '/NH'],
                                      capture_output=True, check=True, creationflags=0x08000000)
        if b'"alone.exe"' in process_list.stdout.lower():
            raise ValueError('Close Alone in the Dark before installing the mod.')
    # No settings/profile reads and no activation probes are performed here.
    return game


def load_payload(distribution: Path) -> dict[str, Path]:
    distribution = plain_path(distribution)
    manifest = plain_path(distribution / 'PAYLOAD_MANIFEST.json')
    if manifest.stat().st_size > 2 * 1024 * 1024:
        raise ValueError('Oversized release manifest')
    data = json.loads(manifest.read_text(encoding='utf-8'))
    if data.get('schema') != 1 or data.get('target_exe_sha256') != STOCK_EXE_SHA256:
        raise ValueError('Unsupported release manifest')
    records = data.get('files')
    if not isinstance(records, dict) or not 4 <= len(records) <= MAX_FILES:
        raise ValueError('Invalid release file list')
    result = {}
    names = set()
    for name, row in records.items():
        name = relative_name(name)
        if name.casefold() in names:
            raise ValueError('Duplicate installation path')
        names.add(name.casefold())
        source = plain_path(below(distribution / 'payload', name))
        if not isinstance(row, dict) or source.stat().st_size != row.get('bytes') or file_sha(source) != row.get('sha256'):
            raise ValueError('Release file failed its integrity check: ' + name)
        result[name] = source
    for required in ('d3d9.dll', 'dinput8.dll', 'AITD Enhanced Launcher.exe', 'aitd-enhanced.ini', 'uninstall mod.exe'):
        if required not in result:
            raise ValueError('Incomplete release')
    return result


def dependencies() -> None:
    if sys.version_info < (3, 12) or struct.calcsize('P') != 8:
        raise ValueError('Asset preparation requires 64-bit Python 3.12 or newer.')
    import PIL
    import numpy
    if PIL.__version__ != '13.0.0' or numpy.__version__ != '2.5.4':
        raise ValueError('Install the versions in requirements-install.txt before running setup.')


def collect(directory: Path, prefix: str) -> dict[str, Path]:
    result = {}
    for source in sorted(directory.rglob('*')):
        plain_path(source)
        if source.is_file():
            name = prefix + '/' + source.relative_to(directory).as_posix()
            result[relative_name(name)] = source
    return result


def run_preparation(command: list[str], tools: Path, label: str) -> None:
    """Forward child stderr explicitly: inherited hidden-console handles can lose it."""
    from collections import deque
    tail = deque(maxlen=24)
    with subprocess.Popen(command, cwd=tools, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, encoding='utf-8',
                          errors='replace', creationflags=0x08000000 if os.name == 'nt' else 0) as child:
        assert child.stdout is not None
        for line in child.stdout:
            line = line.rstrip('\r\n')
            # These are our fixed local helpers, not shell commands or plugins.
            # Bound individual log records while preserving ordinary tracebacks.
            if len(line) > 8192:
                line = line[:8192] + ' [line truncated]'
            print('[' + label + '] ' + line, flush=True)
            tail.append(line)
        status = child.wait()
    if status:
        detail = '\n'.join(tail) or 'The helper returned no diagnostic output.'
        raise RuntimeError(label + ' preparation failed (exit ' + str(status) + ').\n' +
                           detail + '\nNo mod files have been copied into the game folder.')


def prepare(game: Path, distribution: Path, stage: Path) -> dict[str, Path]:
    dependencies()
    files = load_payload(distribution)
    for name in files:
        if below(game, name).exists():
            raise ValueError('Existing file would be replaced. Use a clean game installation: ' + name)
    tools = Path(__file__).resolve().parent
    # Report this machine-specific prerequisite before lengthy resource generation.
    from generate_texture_mods import check_keycap_font
    check_keycap_font(distribution)
    # Every generator receives the original read-only installation and a NEW
    # destination outside it. No original-content staging tree is distributed.
    jobs = [('extract_runtime_ui.py', 'native'), ('generate_texture_mods.py', 'textures'),
            ('generate_ui_mods.py', 'interface'), ('generate_map_mods.py', 'maps')]
    for script, leaf in jobs:
        print('Preparing local assets: ' + leaf, flush=True)
        command = [sys.executable, '-E', '-u', '-X', 'utf8', str(tools / script), str(game), str(stage / leaf)]
        if leaf == 'textures':
            command += ['--logo', str(distribution / 'payload' / 'assets' / 'ui' / 'aitd_logo.png')]
        run_preparation(command, tools, leaf)
    files.update(collect(stage / 'native' / 'assets', 'assets'))
    for leaf in ('textures', 'interface', 'maps'):
        for target, source in collect(stage / leaf / 'mods', 'mods').items():
            if target in files:
                raise ValueError('Two generators claimed the same file')
            files[target] = source
    if len(files) > MAX_FILES:
        raise ValueError('Too many installation files')
    # Python is selected locally. Never distribute a maintainer's absolute path.
    ini = configparser.ConfigParser(interpolation=None)
    ini.optionxform = str
    ini.read(files['aitd-enhanced.ini'], encoding='utf-8-sig')
    if not ini.has_section('Mods'):
        ini.add_section('Mods')
    ini['Mods']['AssetOverrides'] = 'true'
    ini['Mods']['PythonExecutable'] = sys.executable
    ini['Mods']['CompilerScript'] = r'.enhanced\tools\compile_asset_mods.py'
    configured = stage / 'aitd-enhanced.ini'
    # Win32 profile APIs recognize the UTF-16 BOM; UTF-8 user paths are not
    # reliably decoded by those APIs on systems with another ANSI code page.
    with configured.open('x', encoding='utf-16') as stream:
        ini.write(stream)
    files['aitd-enhanced.ini'] = configured
    return files


def commit(game: Path, files: dict[str, Path]) -> dict:
    game = verify_game(game)
    plan = []
    seen = set()
    for name, source in sorted(files.items()):
        name = relative_name(name)
        if name.casefold() in seen:
            raise ValueError('Duplicate installation destination')
        seen.add(name.casefold())
        destination = below(game, name)
        if destination.exists():
            raise ValueError('Existing file would be replaced: ' + name)
        plan.append((name, plain_path(source), destination))
    created = []
    report = {'schema': 1, 'original_exe_sha256': STOCK_EXE_SHA256,
              'generated_game_assets_are_local_only': True, 'files': {}, 'created_directories': []}
    created_directories = set()
    try:
        for name, source, destination in plan:
            below(game, name)
            parent = destination.parent
            while parent != game and not parent.exists():
                created_directories.add(parent.relative_to(game).as_posix())
                parent = parent.parent
            destination.parent.mkdir(parents=True, exist_ok=True)
            expected = file_sha(source)
            # Exclusive creation: a concurrent file is never overwritten.
            with destination.open('xb') as output:
                identity = os.fstat(output.fileno())
                created.append((destination, identity.st_dev, identity.st_ino))
                with source.open('rb') as input_stream:
                    shutil.copyfileobj(input_stream, output)
            if file_sha(destination) != expected:
                raise ValueError('Copy integrity failure: ' + name)
            report['files'][name] = {'sha256': expected, 'bytes': destination.stat().st_size}
        if file_sha(game / 'Alone.exe') != STOCK_EXE_SHA256:
            raise ValueError('Original executable changed during setup')
        marker = game / '.enhanced-install.json'
        report['created_directories'] = sorted(created_directories)
        with marker.open('x', encoding='utf-8') as stream:
            identity = os.fstat(stream.fileno())
            created.append((marker, identity.st_dev, identity.st_ino))
            json.dump(report, stream, indent=2)
            stream.write('\n')
        return report
    except Exception:
        # Only remove exact files created by this invocation. Preserve raced,
        # redirected or replaced files and all pre-existing directories.
        for path, device, inode in reversed(created):
            try:
                plain_path(path)
                info = os.lstat(path)
                if (info.st_dev, info.st_ino) == (device, inode):
                    path.unlink()
            except (OSError, ValueError):
                pass
        raise


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--distribution', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    game = verify_game(args.game)
    scratch = Path(tempfile.mkdtemp(prefix='aitd-enhanced-setup-'))
    identity = os.lstat(scratch)
    try:
        files = prepare(game, args.distribution, scratch)
        report = commit(game, files)
    finally:
        # Validate the exact absolute, still-owned temporary target before any
        # recursive cleanup. A changed path is preserved for manual inspection.
        try:
            checked = plain_path(scratch)
            current = os.lstat(checked)
            if (checked.parent == Path(tempfile.gettempdir()).resolve() and
                    checked.name.startswith('aitd-enhanced-setup-') and
                    (current.st_dev, current.st_ino) == (identity.st_dev, identity.st_ino)):
                shutil.rmtree(checked)
        except (OSError, ValueError):
            pass
    print('Installed ' + str(len(report['files'])) + ' mod files. Original game and saved games were preserved.', flush=True)


if __name__ == '__main__':
    main()
