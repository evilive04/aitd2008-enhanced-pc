"""Read-only metadata for the observed AITD VMF/VFF variant. Never rewrites UI.

Published OpenTDU structures supplied layout hints; CRCs, version and byte ranges
are checked against the local AITD files. No third-party implementation is used.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

TRAILER = bytes(8) + b'XIXT' + bytes(4)

def inspect(data):
    if not 72 <= len(data) <= 16 * 1024 * 1024 or data[:4] not in (b'VMF\0', b'VFF\0'):
        raise ValueError('Unsupported VMF/VFF signature or size')
    if data[5] != 2 or struct.unpack_from('<H', data, 6)[0] != 8:
        raise ValueError('Unsupported AITD header version')
    if zlib.crc32(data[:68]) != struct.unpack_from('<I', data, 68)[0]:
        raise ValueError('UI header CRC32 mismatch')
    bounds = struct.unpack_from('<4f', data, 44)
    if not all(math.isfinite(value) and abs(value) <= 1e7 for value in bounds):
        raise ValueError('Invalid frame bounds')
    if bounds[0] > bounds[1] or bounds[2] > bounds[3]:
        raise ValueError('Reversed frame bounds')
    report = {'kind': data[:3].decode('ascii'), 'bytes': len(data),
              'sha256': hashlib.sha256(data).hexdigest(), 'version_fields': [data[5], 8],
              'header_crc32_valid': True, 'opaque_word_8': struct.unpack_from('<I', data, 8)[0],
              'author': data[12:44].split(b'\0')[0].decode('ascii'), 'frame_bounds_fields': bounds,
              'frame_rate_field': struct.unpack_from('<H', data, 60)[0],
              'header_count_fields': list(struct.unpack_from('<3H', data, 62))}
    if data[:3] == b'VFF':
        if len(data) < 160:
            raise ValueError('Truncated observed font header')
        candidate = data[96:160].split(b'\0')[0]
        report.update(font_name_candidate=candidate.decode('ascii') if candidate.isascii() else None,
                      glyph_count_field=struct.unpack_from('<H', data, 82)[0],
                      body_status='Font body not parsed; header only')
        return report
    cursor = 72
    types = Counter()
    fonts = []
    largest = 0
    records = 0
    while cursor < len(data):
        if len(data) - cursor == len(TRAILER) and data[cursor:] == TRAILER:
            break
        if cursor + 8 > len(data):
            raise ValueError('Truncated tag header')
        kind, size = struct.unpack_from('<II', data, cursor)
        if size < 8 or cursor + size > len(data):
            raise ValueError(f'Invalid tag range at 0x{cursor:x}')
        types[kind] += 1
        records += 1
        largest = max(largest, size)
        if records > 1000000:
            raise ValueError('Tag count exceeds limit')
        if kind == 10 and size >= 88:
            name = data[cursor + 24:cursor + 88].split(b'\0')[0]
            fonts.append({'tag_offset': cursor, 'glyph_count_field': struct.unpack_from('<H', data, cursor + 10)[0],
                          'name_candidate': name.decode('ascii') if name.isascii() else None})
        cursor += size
    if data[cursor:] != TRAILER:
        raise ValueError('Unknown terminal layout; no synthetic end was assumed')
    report.update(linear_tag_records=sum(types.values()), tag_id_counts=dict(sorted(types.items())),
                  largest_tag_bytes=largest, tag_ranges_cover_until=cursor,
                  terminal_xixt_layout_valid=True, font_records=fonts,
                  body_status='Linear tag boundaries checked; nested semantics and ActionScript not parsed')
    return report

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('resource', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    source = args.resource.resolve(strict=True)
    destination = args.output.resolve()
    if destination == source or destination.exists():
        parser.error('Choose a new metadata output file')
    if source.stat().st_size > 16 * 1024 * 1024:
        parser.error('Resource exceeds limit')
    report = inspect(source.read_bytes())
    report['filename'] = source.name
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
    print(json.dumps(report))

if __name__ == '__main__':
    main()
