"""Read-only inspector for the observed PC XMBF/FAT schema and bounded metadata.

Never extracts assets or rewrites archives. Optional adaptive reads use the checked
PC block/page reader. Original installation is opened exclusively for reads.
"""
import argparse
import csv
import hashlib
import json
import re
import struct
import zlib
from collections import Counter
from pathlib import Path
from lzo1x_block import decompress

SCHEMA_SHA256 = '3f3f88b1cdcb0bb459f7034048a64b2591be68cb215a1c5a55956759f23c205b'

def read_index(path):
    if path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError('Index exceeds inspection limit')
    data = path.read_bytes()
    if hashlib.sha256(data[:384]).hexdigest() != SCHEMA_SHA256:
        raise ValueError('Unrecognized XMBF schema; refusing guessed offsets')
    count, hash_relative, count2, files_relative = struct.unpack_from('<4I', data, 384)
    if not count or count != count2 or hash_relative != 16 or files_relative != 16 + count * 8:
        raise ValueError('Unexpected index array layout')
    if 384 + files_relative + count * 32 != len(data):
        raise ValueError('Index count does not match exact length')
    entries = []
    for i in range(count):
        key = struct.unpack_from('<Q', data, 384 + hash_relative + i * 8)[0]
        size, padded, decoded, offset = struct.unpack_from('<4Q', data, 384 + files_relative + i * 32)
        entries.append(dict(index=i, hashcode=f'{key:016x}', size=size, padded_size=padded,
                            decompression_size=decoded, offset=offset))
    return data, entries

def prefix_metadata(stream, entry, blocks, directory=False, adaptive=False, metadata_limit=1024*1024):
    stream.seek(entry['offset'])
    header = stream.read(min(56, entry['size']))
    if len(header) != 56 or header[:16] != bytes.fromhex('00000000000000020000000000000001'):
        return {'status': 'unknown_outer_header'}
    chunk, table, start = struct.unpack_from('>3I', header, 16)
    total = struct.unpack_from('>Q', header, 32)[0]
    if not 0 < chunk <= 1024 * 1024 or not 0 < total or not 56 <= table < start < entry['size']:
        raise ValueError('Invalid outer prefix layout')
    count = (total + chunk - 1) // chunk
    if table + count * 4 > start:
        raise ValueError('Block table exceeds header region')
    stream.seek(entry['offset'] + table)
    wanted = min(count, blocks)
    packed = stream.read(wanted * 4)
    if len(packed) != wanted * 4:
        raise ValueError('Truncated prefix block sizes')
    sizes = struct.unpack('>' + str(wanted) + 'I', packed)
    decoded = bytearray()
    cursor = start
    for size in sizes:
        expected = min(chunk, total - len(decoded))
        if not 0 < size <= chunk or cursor + size > entry['size']:
            raise ValueError('Prefix block exceeds bank bounds')
        stream.seek(entry['offset'] + cursor)
        source = stream.read(size)
        if len(source) != size:
            raise ValueError('Truncated bank block')
        decoded.extend(source if size == expected else decompress(source, expected))
        cursor = (cursor + size + 15) // 16 * 16
    identifiers = []
    pattern = re.compile(rb'(?:^|[_/\\.\-])(?:ui|hud|fonts?|interface|inventory|menus?)(?:[_/\\.\-]|$)', re.I)
    for match in re.finditer(rb'[A-Za-z0-9_./\\:\-]{6,160}', decoded):
        if pattern.search(match.group()):
            identifiers.append(match.group().decode('ascii'))
    bank_tag = decoded[8:12] == b'KNAB'
    bank_header_crc = (len(decoded) >= 72 and struct.unpack_from('<I', decoded)[0] == 64
                       and struct.unpack_from('<I', decoded, 4)[0] == (zlib.crc32(decoded[8:72]) ^ 0xffffffff))
    bank_size_matches = len(decoded) >= 28 and struct.unpack_from('<I', decoded, 24)[0] == total
    result = {'status': 'prefix_decoded', 'chunk_bytes': chunk, 'stream_output_bytes': total,
            'first_block_stored_bytes': sizes[0], 'prefix_bytes': len(decoded), 'prefix_blocks': wanted,
            'bank_marker_at_8': bank_tag, 'bank_header_checksum_valid': bank_header_crc,
            'bank_declared_size_matches_stream': bank_size_matches,
            'bank_year': struct.unpack_from('<I', decoded, 44)[0] if bank_tag and len(decoded) >= 48 else None,
            'bank_declared_file_count': struct.unpack_from('<I', decoded, 40)[0] if bank_tag and len(decoded) >= 48 else None,
            'ui_identifier_candidates': identifiers[:16],
            'prefix_sha256': hashlib.sha256(decoded).hexdigest()}
    if directory:
        from inspect_bank import inspect as inspect_bank
        try:
            needed = struct.unpack_from('<I', decoded, 68)[0] if bank_header_crc and len(decoded) >= 72 else 0
            expanded = adaptive and needed > len(decoded)
            if expanded:
                from decode_bank import decode
                decoded, bank, stream_report = decode(stream, entry, metadata_limit, prefix_bytes=needed)
                result.update(prefix_bytes=len(decoded), prefix_sha256=hashlib.sha256(decoded).hexdigest(),
                              prefix_blocks=stream_report['blocks'], prefix_expanded=True,
                              prefix_page_count=stream_report['pages'])
            else:
                bank = inspect_bank(bytes(decoded), allow_prefix=True)
                result['prefix_expanded'] = False
            result['directory'] = {'status': 'parsed', 'file_count': bank['file_count'],
                                   'metadata_sections_validated': len(bank['sections']),
                                   'year': bank['year'], 'order_index_bytes': bank['order_index_bytes'],
                                   'file_payloads_hashed': False,
                                   'files': [{key: item[key] for key in ('tree_path', 'filename', 'storage_index', 'offset', 'size')}
                                             for item in bank['files']]}
        except ValueError as error:
            result['directory'] = {'status': 'not_parsed', 'reason': str(error)}
    return result

def inspect(path, probe, blocks, directory=False, adaptive=False, metadata_limit=1024*1024):
    data, entries = read_index(path)
    payload = path.with_suffix('.big')
    payload_size = payload.stat().st_size
    physical = sorted(entries, key=lambda e: e['offset'])
    if any(e['size'] > e['padded_size'] or e['offset'] + e['padded_size'] > payload_size for e in entries):
        raise ValueError('Entry exceeds payload bounds')
    if any(a['offset'] + a['padded_size'] > b['offset'] for a, b in zip(physical, physical[1:])):
        raise ValueError('Overlapping bank ranges')
    if probe:
        with payload.open('rb') as stream:
            for entry in entries:
                try:
                    entry['prefix'] = prefix_metadata(stream, entry, blocks, directory, adaptive, metadata_limit)
                except ValueError as error:
                    entry['prefix'] = {'status': 'not_decoded', 'reason': str(error)}
    types = Counter()
    candidates = []
    directory_status = Counter()
    directory_failures = Counter()
    ui_pattern = re.compile(r'(^|[/\\_.\-])(ui|hud|fonts?|fnt|glyph|interface|inventory|menus?|front_?end)([/\\_.\-]|$)', re.I)
    for entry in entries:
        bank = entry.get('prefix', {}).get('directory', {})
        directory_status[bank.get('status', 'not_requested')] += 1
        if bank.get('reason'):
            directory_failures[bank['reason']] += 1
        for item in bank.get('files', []):
            extension = Path(item['filename']).suffix.lower() or '<none>'
            types[extension] += 1
            if extension in ('.vmf', '.vff', '.fnt', '.font', '.ttf', '.otf', '.gfx') or ui_pattern.search(item['tree_path']):
                candidates.append({'bank_index': entry['index'], 'bank_hashcode': entry['hashcode'], **item})
    return {'index': path.name, 'index_sha256': hashlib.sha256(data).hexdigest(),
            'schema_sha256': SCHEMA_SHA256, 'payload_bytes': payload_size, 'entry_count': len(entries),
            'bounds_and_nonoverlap': True,
            'alignment_2048': all(e['offset'] % 2048 == 0 and e['padded_size'] % 2048 == 0 for e in entries),
            'prefix_status_counts': dict(Counter(e.get('prefix', {}).get('status', 'not_requested') for e in entries)),
            'bank_marker_count': sum(e.get('prefix', {}).get('bank_marker_at_8', False) for e in entries),
            'valid_bank_header_checksums': sum(e.get('prefix', {}).get('bank_header_checksum_valid', False) for e in entries),
            'matching_bank_declared_sizes': sum(e.get('prefix', {}).get('bank_declared_size_matches_stream', False) for e in entries),
            'ui_candidate_banks': sum(bool(e.get('prefix', {}).get('ui_identifier_candidates')) for e in entries),
            'directory_status_counts': dict(directory_status), 'directory_file_types': dict(types.most_common()),
            'directory_failure_reasons': dict(directory_failures),
            'directory_failure_examples': [
                {'bank_index': e['index'], 'bank_hashcode': e['hashcode'],
                 'reason': e.get('prefix', {}).get('directory', {}).get('reason')}
                for e in entries if e.get('prefix', {}).get('directory', {}).get('reason')][:8],
            'bank_year_counts': dict(Counter(str(e.get('prefix', {}).get('bank_year')) for e in entries)),
            'directory_total_files': sum(types.values()),
            'expanded_metadata_prefixes': sum(e.get('prefix', {}).get('prefix_expanded', False) for e in entries),
            'max_observed_prefix_bytes': max((e.get('prefix', {}).get('prefix_bytes', 0) for e in entries), default=0),
            'ui_candidate_file_count': len(candidates), 'ui_candidate_files': candidates[:500],
            'ui_candidate_files_truncated': len(candidates) > 500,
            'entries': entries}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--probe-prefixes', action='store_true')
    parser.add_argument('--prefix-blocks', type=int, choices=range(1, 5), default=1)
    parser.add_argument('--summary', action='store_true', help='Save aggregate metadata without per-bank entries')
    parser.add_argument('--bank-directories', action='store_true', help='Validate internal directory metadata where it fits in the prefix')
    parser.add_argument('--adaptive-directories', action='store_true', help='Read only as far as complete metadata requires, within the configured limit')
    parser.add_argument('--max-metadata-kib', type=int, choices=range(4,8193), default=1024, metavar='4..8192')
    parser.add_argument('--resource-manifest', type=Path, help='Optional new CSV with names/ranges only, outside installation')
    args = parser.parse_args()
    if args.bank_directories and not args.probe_prefixes:
        parser.error('--bank-directories requires --probe-prefixes')
    if args.adaptive_directories and not args.bank_directories:
        parser.error('--adaptive-directories requires --bank-directories')
    if args.adaptive_directories and args.max_metadata_kib < args.prefix_blocks * 4:
        parser.error('Metadata limit must cover the initially requested prefix blocks')
    root = args.game.resolve(strict=True)
    destination = args.output.resolve()
    if destination == root or root in destination.parents:
        parser.error('Metadata output must be outside the installation')
    if destination.exists():
        parser.error('Output exists; choose a new filename')
    manifest = args.resource_manifest.resolve() if args.resource_manifest else None
    if manifest and (not args.bank_directories or manifest.exists() or manifest == destination or manifest == root or root in manifest.parents):
        parser.error('Manifest needs --bank-directories and a new path outside the installation')
    reports = [inspect(root / f'alone_compressed_{label}.fat', args.probe_prefixes, args.prefix_blocks,
                       args.bank_directories, args.adaptive_directories, args.max_metadata_kib*1024) for label in 'abc']
    destination.parent.mkdir(parents=True, exist_ok=True)
    saved = [{key: value for key, value in item.items() if key != 'entries'} for item in reports] if args.summary else reports
    report = {'scope': 'Observed PC schema; bounded metadata inspection only. No extraction or writes to game.', 'archives': saved}
    created = []
    try:
        with destination.open('x', encoding='utf-8') as stream:
            created.append(destination)
            json.dump(report, stream, indent=2)
        if manifest:
            manifest.parent.mkdir(parents=True, exist_ok=True)
            with manifest.open('x', encoding='utf-8', newline='') as stream:
                created.append(manifest)
                fields = ['archive', 'bank_index', 'bank_hashcode', 'storage_index', 'tree_path', 'filename', 'size', 'offset']
                writer = csv.DictWriter(stream, fieldnames=fields)
                writer.writeheader()
                for archive in reports:
                    for entry in archive['entries']:
                        for item in entry.get('prefix', {}).get('directory', {}).get('files', []):
                            writer.writerow({'archive': archive['index'], 'bank_index': entry['index'],
                                             'bank_hashcode': entry['hashcode'], **item})
    except Exception:
        for path in reversed(created):
            path.unlink(missing_ok=True)
        raise
    for item in reports:
        print(json.dumps({key: value for key, value in item.items() if key not in ('entries', 'ui_candidate_files', 'directory_file_types')}))

if __name__ == '__main__':
    main()
