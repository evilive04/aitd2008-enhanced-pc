"""Build a source-only public candidate from a positive file allowlist.

This is a publication policy check, not a rights or compatibility certificate.
Nothing from the existing Git history, game installation or private LAB is
copied implicitly. The output parent must already exist; output must be NEW.
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


MAX_FILE_BYTES = 100 * 1024 * 1024 - 1
MAX_TOTAL_BYTES = 256 * 1024 * 1024
CUR_PATH = 'assets/ui/cursor_metal.cur'
MAX_CUR_BYTES = 512 * 1024
FORBIDDEN_PARTS = {'.git', 'work', 'private', 'build', 'logs', 'extracted',
                   '__pycache__', 'research', 'node_modules', '.ssh'}
FORBIDDEN_SUFFIXES = {'.exe', '.dll', '.pdb', '.dmp', '.obj', '.lib', '.exp',
                      '.big', '.fat', '.bik', '.bink', '.vmf', '.vmd', '.xmt',
                      '.xmm', '.hpd', '.dms', '.aitd', '.pebin', '.gpr',
                      '.mp4', '.zip', '.7z', '.rar', '.env'}
ALLOWED_SUFFIXES = {'.cpp', '.cs', '.h', '.inl', '.hlsl', '.hlsli', '.def',
                    '.xaml', '.manifest', '.png', '.jpg', '.jpeg', '.ico',
                    '.md', '.json', '.txt', '.py', '.cmd', '.ps1', '.ini',
                    '.yml', '.yaml'}
TEXT_SUFFIXES = ALLOWED_SUFFIXES - {'.png', '.jpg', '.jpeg', '.ico'}
SPECIAL_NAMES = {'LICENSE', 'NOTICE', '.gitignore', '.gitattributes'}
DOS_NAMES = {'con', 'prn', 'aux', 'nul', *(f'com{i}' for i in range(1, 10)),
             *(f'lpt{i}' for i in range(1, 10))}
FORBIDDEN_TOOL = re.compile(
    r'(?:securom|unpack|runtime.dump|dump.runtime|reconstruct|build_.*probe)', re.I)
SECRET_PATTERNS = (
    ('GitHub token', re.compile(rb'\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{40,})\b')),
    ('OpenAI key', re.compile(rb'\bsk-(?:proj-|svcacct-)?[A-Za-z0-9_-]{32,}\b')),
    ('AWS access key', re.compile(rb'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b')),
    ('private key', re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----')),
)
# Fingerprints of the *textual selectors*, not private executable contents.
# Keeping a second hash avoids publishing the rejected selector in this tool.
PRIVATE_SELECTOR_FINGERPRINTS = (
    '7a2e7b07fbc2e8dd447ee9ffeeabd85f97135ce05964faff223c03b682f48e34',
    'fb7518cd80582136969bc616d67f09282fcf45d43c1f3e58b3faf0be9bed7379',
)


class PackageError(ValueError):
    pass


def reject_reparse(path: Path) -> None:
    """Do not depend on Path.is_junction, which is unavailable on Python 3.11."""
    info = os.lstat(path)
    if stat.S_ISLNK(info.st_mode) or (
            getattr(info, 'st_file_attributes', 0) &
            getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0x400)):
        raise PackageError('Reparse points and symlinks are not public inputs')


def checked_absolute(path: Path) -> Path:
    absolute = Path(os.path.abspath(path))
    for part in reversed((absolute, *absolute.parents)):
        reject_reparse(part)
    return absolute.resolve(strict=True)


def relative_name(value: object) -> str:
    if not isinstance(value, str) or not value or '\\' in value or ':' in value:
        raise PackageError('Allowlist entries must be relative slash-separated paths')
    pieces = value.split('/')
    if any(not p or p in ('.', '..') or p.endswith((' ', '.')) or
           p.split('.')[0].casefold() in DOS_NAMES or any(ord(c) < 32 for c in p)
           for p in pieces):
        raise PackageError('Unsafe allowlist path')
    path = PurePosixPath(value)
    if path.is_absolute() or any(p.casefold() in FORBIDDEN_PARTS for p in pieces):
        raise PackageError('Private/history/build path is not a public input')
    leaf = path.name
    if leaf.casefold().startswith('.env') or path.suffix.casefold() in FORBIDDEN_SUFFIXES:
        raise PackageError('Game binaries, archives, dumps or private environment are forbidden')
    if (path.suffix.casefold() not in ALLOWED_SUFFIXES and leaf not in SPECIAL_NAMES
            and path.as_posix() != CUR_PATH):
        raise PackageError('Unreviewed file type is not allowed')
    if pieces[0].casefold() == 'tools' and FORBIDDEN_TOOL.search(leaf):
        raise PackageError('DRM reconstruction and dump tools are not public inputs')
    return path.as_posix()


def validate_cursor(data: bytes) -> None:
    """Only the reviewed single-image 32-bit DIB CUR format is admitted.

    This does not admit arbitrary .cur files, executable payloads or an opaque
    embedded stream. Every byte belongs to the directory, DIB, pixels or mask.
    """
    if not 62 <= len(data) <= MAX_CUR_BYTES:
        raise PackageError('Cursor extent is outside the reviewed format')
    if struct.unpack_from('<HHH', data) != (0, 2, 1):
        raise PackageError('Only one standard CUR image is supported')
    width, height, colors, reserved, hotspot_x, hotspot_y, size, offset = struct.unpack_from('<BBBBHHII', data, 6)
    width, height = width or 256, height or 256
    if (colors or reserved or hotspot_x >= width or hotspot_y >= height or
            offset != 22 or size != len(data) - offset):
        raise PackageError('Invalid cursor directory or image extent')
    (header_size, dib_width, dib_height, planes, bits, compression, image_size,
     _xppm, _yppm, palette, important) = struct.unpack_from('<IiiHHIIiiII', data, offset)
    pixels = width * 4 * height
    mask = ((width + 31) // 32) * 4 * height
    if (header_size != 40 or dib_width != width or dib_height != height * 2 or
            planes != 1 or bits != 32 or compression != 0 or palette or important or
            image_size not in (0, pixels, pixels + mask) or size != 40 + pixels + mask):
        raise PackageError('Cursor must contain an exact bounded 32-bit DIB and AND mask')


def snapshot_file(root: Path, relative: str) -> bytes:
    current = root
    for piece in PurePosixPath(relative).parts:
        current /= piece
        reject_reparse(current)
    resolved = current.resolve(strict=True)
    if resolved == root or root not in resolved.parents or not resolved.is_file():
        raise PackageError('Allowlist input is not a contained regular file')
    info = resolved.stat()
    limit = MAX_CUR_BYTES if relative == CUR_PATH else MAX_FILE_BYTES
    if info.st_size > limit:
        raise PackageError('File exceeds regular GitHub source-file size policy: ' + relative)
    with resolved.open('rb') as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise PackageError('Input grew beyond source-file size policy: ' + relative)
    if len(data) != info.st_size:
        raise PackageError('Input changed while reading: ' + relative)
    # Recheck parents after the read. The validated byte snapshot, rather than
    # a second file open during packaging, is used for all output artifacts.
    current = root
    for piece in PurePosixPath(relative).parts:
        current /= piece
        reject_reparse(current)
    if current.resolve(strict=True) != resolved:
        raise PackageError('Input path changed during review: ' + relative)
    if relative == CUR_PATH:
        validate_cursor(data)
    if data[:2] == b'MZ' or data[:4] in (b'\x7fELF', b'PK\x03\x04'):
        raise PackageError('Executable/archive payload masquerading as source: ' + relative)
    for label, pattern in SECRET_PATTERNS:
        if pattern.search(data):
            raise PackageError('Credential-like content (' + label + ') in ' + relative)
    lower = re.sub(rb'\s+', b'', data.lower())
    selectors = re.findall(rb'(?<![0-9a-f])[0-9a-f]{64}(?![0-9a-f])', lower)
    for initializer in re.findall(rb'(?:0x[0-9a-f]{2},){31}0x[0-9a-f]{2}', lower):
        selectors.append(b''.join(re.findall(rb'0x([0-9a-f]{2})', initializer)))
    if any(hashlib.sha256(value).hexdigest() in PRIVATE_SELECTOR_FINGERPRINTS for value in selectors):
        raise PackageError('Private reconstructed-image identity in ' + relative)
    if re.search(rb'[a-zA-Z]:[\\/]+Users[\\/]+[a-zA-Z0-9._-]+', data):
        raise PackageError('User-specific absolute path in ' + relative)
    if resolved.suffix.casefold() in TEXT_SUFFIXES or resolved.name in SPECIAL_NAMES:
        try:
            data.decode('utf-8-sig')
        except UnicodeDecodeError as exc:
            raise PackageError('Source text must be UTF-8: ' + relative) from exc
    return data


def load_selection(source: Path, allowlist: Path) -> tuple[dict, dict[str, bytes]]:
    root = checked_absolute(source)
    manifest_path = checked_absolute(allowlist)
    if not manifest_path.is_file() or manifest_path.stat().st_size > 1024 * 1024:
        raise PackageError('Allowlist must be a bounded regular JSON file')
    manifest = json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    if not isinstance(manifest, dict) or manifest.get('schema') != 1:
        raise PackageError('Unsupported allowlist schema')
    for key in ('package', 'version'):
        if not isinstance(manifest.get(key), str) or not re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9._-]{0,79}', manifest[key]):
            raise PackageError('Invalid package/version identity')
    if manifest.get('status') not in ('compatibility-in-progress', 'source-review-candidate'):
        raise PackageError('This source-only tool cannot certify a playable release')
    paths = manifest.get('files')
    if not isinstance(paths, list) or not 1 <= len(paths) <= 4096:
        raise PackageError('Explicit bounded file allowlist required')
    files: dict[str, bytes] = {}
    seen = set()
    total = 0
    for value in paths:
        name = relative_name(value)
        if name.casefold() in seen or name.casefold() == 'package_manifest.json':
            raise PackageError('Duplicate or reserved output name')
        seen.add(name.casefold())
        files[name] = snapshot_file(root, name)
        total += len(files[name])
        if total > MAX_TOTAL_BYTES:
            raise PackageError('Source package exceeds bounded review size')
    if 'README.md' not in files or 'RELEASE_STATUS.md' not in files:
        raise PackageError('Fresh README.md and RELEASE_STATUS.md are mandatory')
    return manifest, files


def build_package(source: Path, allowlist: Path, output: Path) -> dict:
    selection, files = load_selection(source, allowlist)
    output = Path(os.path.abspath(output))
    parent = checked_absolute(output.parent)
    output = parent / output.name
    # All review happens before creating any destination. Exclusive mkdir
    # refuses a pre-existing/raced destination; no error handler deletes it.
    output.mkdir()
    repository = output / 'repository'
    repository.mkdir()
    report = {
        'schema': 1, 'package': selection['package'], 'version': selection['version'],
        'status': selection['status'], 'scope': selection.get('scope', ''),
        'source_only': True, 'legal_clearance': 'not-certified',
        'gameplay_verification': 'not-performed-by-packager',
        'files': {name: {'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}
                  for name, data in sorted(files.items())},
    }
    encoded = (json.dumps(report, indent=2) + '\n').encode('utf-8')
    pending = None
    try:
        for name, data in sorted(files.items()):
            destination = repository.joinpath(*PurePosixPath(name).parts)
            destination.parent.mkdir(parents=True, exist_ok=True)
            with destination.open('xb') as stream:
                stream.write(data)
        with (repository / 'PACKAGE_MANIFEST.json').open('xb') as stream:
            stream.write(encoded)
        archive = output / (selection['package'] + '-' + selection['version'] + '-source.zip')
        # Complete the ZIP in an owned temporary file, then atomically create
        # the final name without replacing anything a concurrent process made.
        with tempfile.NamedTemporaryFile(prefix='.source-', suffix='.pending', dir=output, delete=False) as stream:
            pending = Path(stream.name)
            with zipfile.ZipFile(stream, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as package:
                for name, data in sorted(files.items()):
                    package.writestr('repository/' + name, data)
                package.writestr('repository/PACKAGE_MANIFEST.json', encoded)
            stream.flush()
            os.fsync(stream.fileno())
        os.link(pending, archive)
        pending.unlink()
        pending = None
        with (output / 'PACKAGE_COMPLETE.json').open('xb') as stream:
            stream.write(encoded)
        return report
    except Exception:
        # Preserve the incomplete candidate for inspection, never recursively
        # delete a destination or unlink a raced final ZIP owned by someone else.
        marker = output / 'PACKAGE_FAILED.txt'
        try:
            with marker.open('x', encoding='utf-8') as stream:
                stream.write('Incomplete source candidate. Do not publish this directory.\n')
        except FileExistsError:
            pass
        raise


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--allowlist', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    report = build_package(args.source_root, args.allowlist, args.output_dir)
    print(json.dumps({'output': str(args.output_dir), 'files': len(report['files']),
                      'status': report['status'], 'source_only': True}))


if __name__ == '__main__':
    main()
