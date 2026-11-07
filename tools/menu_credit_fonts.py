"""Validate observed inline font tables and link their atlas IDs to tag64.

Read-only metadata. Does not infer an external atlas filename from a numeric ID.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
from inspect_vmf import inspect as inspect_vmf

def inspect(data):
    header = inspect_vmf(data)
    if header['kind'] != 'VMF':
        raise ValueError('This reader is for inline VMF fonts, not VFF')
    descriptors = {}
    font_tags = []
    cursor = 72
    while cursor < len(data) - 16:
        kind, size = struct.unpack_from('<II', data, cursor)
        if kind == 10:
            font_tags.append((cursor, size))
        if kind == 64:
            if size != 76:
                raise ValueError('Unsupported tag64 descriptor extent')
            key, width, height = struct.unpack_from('<3I', data, cursor + 8)
            token = data[cursor + 40:cursor + 72]
            raw_name = token[2:].split(b'\0')[0]
            if not raw_name.isascii() or not raw_name or not 0 < width <= 16384 or not 0 < height <= 16384:
                raise ValueError('Unsupported font atlas descriptor')
            if key in descriptors:
                raise ValueError('Duplicate atlas descriptor index')
            descriptors[key] = {'tag_offset': cursor, 'dictionary_index': key,
                                'name_candidate': raw_name.decode('ascii'), 'dimensions_fields': [width,height],
                                'resource_token_hex': token.hex(), 'token_prefix_field': int.from_bytes(token[:2],'little')}
        cursor += size
    fonts = []
    for tag, size in font_tags:
        count = struct.unpack_from('<H', data, tag + 10)[0]
        atlas = struct.unpack_from('<I', data, tag + 12)[0]
        name = data[tag + 24:tag + 88].split(b'\0')[0].decode('ascii')
        if size != 1112 + count * 68:
            raise ValueError('Inline font differs from observed 60-byte glyph layout')
        fast = struct.unpack_from('<256I', data, tag + 88)
        pairs = [struct.unpack_from('<II', data, tag + 1112 + i * 8) for i in range(count)]
        mapping = dict(pairs)
        if list(mapping) != sorted(mapping) or len(mapping) != count:
            raise ValueError('Inline glyph codepoints not sorted unique')
        if any(offset != mapping.get(key, 0) for key,offset in enumerate(fast)):
            raise ValueError('Inline fast lookup disagrees with complete table')
        glyph_start = tag + 1112 + count * 8
        candidate_uv_range = []
        for i, (key, relative) in enumerate(pairs):
            absolute = tag + 4 + relative
            if absolute != glyph_start + i * 60:
                raise ValueError('Inline glyph offset mismatch')
            fields = struct.unpack_from('<2I13f', data, absolute)
            if not all(math.isfinite(v) for v in fields[2:]):
                raise ValueError('Nonfinite inline metric')
            candidate_uv_range.extend(fields[j] for j in (8,9,12,13))
        descriptor = descriptors.get(atlas)
        # Empty placeholder fonts may have no atlas. Do not invent one.
        if count and (not descriptor or descriptor['name_candidate'] != name):
            raise ValueError('Font does not match its candidate atlas descriptor')
        fonts.append({'tag_offset': tag, 'name': name, 'glyph_count': count,
                      'atlas_dictionary_index': atlas, 'descriptor': descriptor,
                      'glyph_table_consistent': True, 'record_stride': 60,
                      'candidate_uv_min': min(candidate_uv_range) if candidate_uv_range else None,
                      'candidate_uv_max': max(candidate_uv_range) if candidate_uv_range else None,
                      'font_tag_sha256': hashlib.sha256(data[tag:tag + size]).hexdigest()})
    return {'sha256': header['sha256'], 'fonts': fonts, 'tag64_descriptors': list(descriptors.values()),
            'scope': 'Inline table and descriptor links only; atlas pixel source and runtime behavior not proven'}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('vmf', nargs='+', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or output.suffix.lower() != '.json':
        parser.error('Choose a new metadata JSON output')
    reports = []
    for source in args.vmf:
        if source.stat().st_size > 16 * 1024 * 1024:
            parser.error('Input exceeds limit')
        if source.resolve() == output:
            parser.error('Output must differ from input')
        reports.append({'filename': source.name, **inspect(source.read_bytes())})
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x', encoding='utf-8') as stream:
        json.dump(reports, stream, indent=2)
    print(json.dumps([{'filename': r['filename'], 'fonts': len(r['fonts']), 'atlas_descriptors': len(r['tag64_descriptors'])} for r in reports]))

