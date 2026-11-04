"""Validate a privately decoded PC BANK and emit metadata, never extracted assets.

Related Eden/TDU2 notes supplied structural hints; enforce observed bounds,
section checksums, names/count and order before assigning any file range.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

def inspect(data, allow_prefix=False):
    if not 72 <= len(data) <= 128 * 1024 * 1024:
        raise ValueError('BANK size outside inspection bounds')
    sections = []

    def section(label, offset):
        if offset < 0 or offset + 8 > len(data):
            raise ValueError(f'{label}: header outside bank')
        length, checksum = struct.unpack_from('<II', data, offset)
        end = offset + 8 + length
        if end > len(data):
            raise ValueError(f'{label}: section exceeds bank')
        payload = data[offset + 8:end]
        if (zlib.crc32(payload) ^ 0xffffffff) != checksum:
            raise ValueError(f'{label}: unfinalized CRC32 mismatch')
        if any(offset < previous['end'] and previous['offset'] < end for previous in sections):
            raise ValueError('Overlapping metadata sections')
        sections.append({'name': label, 'offset': offset, 'end': end, 'payload_bytes': length,
                         'checksum_valid': True})
        return payload

    header = section('header', 0)
    if len(header) != 64 or header[:12] != b'KNAB' + bytes(8):
        raise ValueError('Unsupported BANK header')
    declared, packed, block1, block2, count, year, sizes_at, types_at, tree_at, order_at, unknown_at, payload_at = struct.unpack_from('<12I', header, 16)
    if declared < len(data) or (not allow_prefix and declared != len(data)) or not 0 < count <= 65536 or year not in (2007, 2008):
        raise ValueError('Unsupported PC BANK length/count/year')
    if unknown_at:
        raise ValueError('Unknown additional metadata section; interpretation refused')
    if any(not value or value & (value - 1) or value > 4096 for value in (block1, block2)):
        raise ValueError('Unsupported BANK alignment fields')
    sizes = section('sizes', sizes_at)
    if len(sizes) != (count + 1) * 20:
        raise ValueError('Unsupported sizes record stride')
    records = []
    for i in range(count + 1):
        offset, size, word0, word1, word2 = struct.unpack_from('<5I', sizes, i * 20)
        if offset < payload_at or offset + size > declared:
            raise ValueError('File range exceeds payload bounds')
        records.append({'offset': offset, 'size': size, 'opaque_words': [word0, word1, word2]})
    if records[-1]['size'] != 0 or records[-1]['opaque_words'][:2] != [0, 0]:
        raise ValueError('Unrecognized terminating sizes record')
    files = records[:-1]
    physical = sorted(files, key=lambda record: record['offset'])
    if any(a['offset'] + a['size'] > b['offset'] for a, b in zip(physical, physical[1:])):
        raise ValueError('Overlapping file payloads')
    if records[-1]['offset'] != max(item['offset'] + item['size'] for item in files):
        raise ValueError('Terminator does not mark final unpadded payload end')
    types = section('types', types_at) if types_at else None
    if types is not None and len(types) != count * 4:
        raise ValueError('Unexpected type-map length')
    tree = section('tree', tree_at)
    order = section('order', order_at)
    width = len(order) // count
    if len(order) != count * width or width not in (1, 2) or (width == 1 and count > 256):
        raise ValueError('Unexpected order section length')
    indices = list(order) if width == 1 else list(struct.unpack('<' + str(count) + 'H', order))
    if sorted(indices) != list(range(count)):
        raise ValueError('Order section is not a permutation')
    if payload_at > declared or any(s['end'] > payload_at for s in sections):
        raise ValueError('Payload begins before metadata ends or exceeds declared length')

    cursor = 0
    nodes = 0
    names = []

    def byte():
        nonlocal cursor
        if cursor >= len(tree):
            raise ValueError('Truncated name tree')
        result = tree[cursor]
        cursor += 1
        return result

    def name(length):
        nonlocal cursor
        if cursor + length > len(tree):
            raise ValueError('Name exceeds tree section')
        value = tree[cursor:cursor + length].decode('ascii')
        if not value or any(ord(character) < 32 for character in value):
            raise ValueError('Unsupported name encoding')
        cursor += length
        return value

    def node(parents):
        nonlocal nodes, cursor
        nodes += 1
        if nodes > 100000 or len(parents) > 64:
            raise ValueError('Tree complexity exceeds limits')
        tag = byte()
        if not tag:
            raise ValueError('Unexpected early tree terminator')
        if tag >= 128:
            children = byte()
            first = children
            children &= 0x7f
            shift = 7
            while first & 0x80:
                first = byte()
                if shift > 14:
                    raise ValueError('Directory child count exceeds bounded varint')
                children |= (first & 0x7f) << shift
                shift += 7
                if not first and shift > 7:
                    raise ValueError('Noncanonical directory child count')
            if not 0 < children <= count:
                raise ValueError('Directory child count exceeds declared file count')
            label = name(256 - tag)
            for _ in range(children):
                node(parents + [label])
        else:
            label = name(tag)
            extension = next((part for part in reversed(parents) if part.startswith('.') and len(part) > 1), '')
            names.append({'tree_path': '/'.join(parents + [label]), 'filename': label + extension})
            if len(names) > count:
                raise ValueError('Tree contains more files than declared')

    while len(names) < count:
        node([])
    if any(tree[cursor:]):
        raise ValueError('Unparsed nonzero tree trailer')
    result = []
    for tree_index, item in enumerate(names):
        storage_index = indices[tree_index]
        entry = {**item, 'tree_index': tree_index, 'storage_index': storage_index, **files[storage_index]}
        entry['payload_sha256'] = (hashlib.sha256(data[entry['offset']:entry['offset'] + entry['size']]).hexdigest()
                                   if not allow_prefix else None)
        result.append(entry)
    return {'bank_sha256': hashlib.sha256(data).hexdigest() if declared == len(data) else None,
            'observed_prefix_sha256': hashlib.sha256(data).hexdigest(), 'bank_bytes': declared,
            'observed_bytes': len(data), 'complete_bank_provided': declared == len(data),
            'mode': 'metadata_prefix' if allow_prefix else 'complete_bank', 'year': year,
            'file_count': count, 'packed_size_field': packed, 'block_fields': [block1, block2],
            'payload_alignment_mod_block_field': payload_at % block2, 'order_index_bytes': width,
            'sections': sections, 'raw_type_map': list(struct.unpack('<' + str(count) + 'I', types)) if types else None,
            'files': result, 'file_payloads_hashed': not allow_prefix,
            'scope': 'Metadata reader only; no extraction or writer; opaque record words not interpreted'}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bank', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--prefix', action='store_true', help='Validate complete metadata from a partial bank; no payload hashes')
    args = parser.parse_args()
    source = args.bank.resolve(strict=True)
    destination = args.output.resolve()
    if destination == source or destination.exists():
        parser.error('Choose a new metadata output file')
    if source.stat().st_size > 128 * 1024 * 1024:
        parser.error('Input exceeds limit')
    report = inspect(source.read_bytes(), allow_prefix=args.prefix)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
    print(json.dumps({key: value for key, value in report.items() if key not in ('files', 'sections', 'raw_type_map')}))
    print(json.dumps({'first_files': [{'filename': item['filename'], 'bytes': item['size']} for item in report['files'][:12]],
                      'list_truncated': len(report['files']) > 12}))

if __name__ == '__main__':
    main()
