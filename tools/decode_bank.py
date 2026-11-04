"""Decode one explicitly selected PC bank into a private analysis folder.

Never writes the installation, public outputs or source repository. No bulk mode,
archive writer, injection or executable target is provided.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from inspect_bank import inspect as inspect_bank
from lzo1x_block import decompress

def align(value, boundary):
    return (value + boundary - 1) // boundary * boundary

def decode(stream, entry, max_bytes, prefix_bytes=None):
    stream.seek(entry['offset'])
    header = stream.read(56)
    if len(header) != 56 or header[:16] != bytes.fromhex('00000000000000020000000000000001'):
        raise ValueError('Unsupported outer header')
    chunk, table, start = struct.unpack_from('>3I', header, 16)
    total, stored = struct.unpack_from('>2Q', header, 32)
    page_limit = struct.unpack_from('>I', header, 48)[0]
    if chunk != 4096 or table != 2048 or not table < start < entry['size']:
        raise ValueError('Unsupported PC block/table layout')
    if not 0 < total <= 0xffffffff or start + stored != entry['size']:
        raise ValueError('Output limit or stored-size boundary mismatch')
    target = total if prefix_bytes is None else min(total, prefix_bytes)
    if not 0 < target or min(total, align(target, chunk)) > max_bytes:
        raise ValueError('Requested output exceeds the inspection limit')
    if not chunk <= page_limit <= 16 * 1024 * 1024 or page_limit % 2048:
        raise ValueError('Unsupported page budget')
    total_count = (total + chunk - 1) // chunk
    count = (target + chunk - 1) // chunk
    if table + total_count * 4 > start:
        raise ValueError('Block table exceeds header region')
    stream.seek(entry['offset'] + table)
    raw_sizes = stream.read(count * 4)
    if len(raw_sizes) != count * 4:
        raise ValueError('Truncated block table')
    sizes = struct.unpack('>' + str(count) + 'I', raw_sizes)
    output = bytearray()
    cursor = start
    page_start = start
    pages = 1
    page_padding = 0
    for size in sizes:
        expected = min(chunk, total - len(output))
        if not 0 < size <= expected:
            raise ValueError('Unexpected stored block length')
        if size == expected and size != chunk:
            raise ValueError('Partial raw tail is not supported by the native fixed-chunk dispatcher')
        padded = align(size, 16)
        if cursor + padded > page_start + page_limit:
            following = align(cursor, 2048)
            page_padding += following - cursor
            cursor = following
            page_start = cursor
            pages += 1
        if cursor + size > start + stored:
            raise ValueError('Block exceeds stored stream')
        stream.seek(entry['offset'] + cursor)
        source = stream.read(size)
        if len(source) != size:
            raise ValueError('Truncated block data')
        output.extend(source if size == expected else decompress(source, expected))
        cursor += padded
    complete = count == total_count
    if len(output) != min(total, count * chunk) or (complete and align(cursor, 2048) != start + stored):
        raise ValueError('Decoded or final padded source length mismatch')
    output = output[:target]
    bank = inspect_bank(bytes(output), allow_prefix=prefix_bytes is not None)
    report = {'decoded_bytes': len(output), 'decoded_sha256': hashlib.sha256(output).hexdigest(),
              'source_bytes': entry['size'], 'blocks': count, 'total_blocks': total_count,
              'stream_complete': complete, 'source_end_checked': complete, 'pages': pages,
              'page_budget': page_limit, 'page_padding_bytes': page_padding,
              'metadata_sections_validated': len(bank['sections']), 'bank_file_count': bank['file_count'],
              'scope': 'Bounded LZO/raw decode and metadata CRC/order/bounds validation; no game-load or asset writer validation'}
    return output, bank, report

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--archive', choices=['a', 'b', 'c'], required=True)
    parser.add_argument('--bank-index', type=int, required=True)
    parser.add_argument('--private-output', type=Path, required=True)
    parser.add_argument('--max-mib', type=int, choices=range(1, 129), default=64)
    parser.add_argument('--report', type=Path, help='Optional new metadata summary outside installation')
    args = parser.parse_args()
    from inspect_fat import read_index
    root = args.game.resolve(strict=True)
    project = Path(__file__).resolve().parents[1]
    public_root = project.parent if project.parent.name == 'outputs' else project
    private = args.private_output.resolve()
    if any(private == forbidden or forbidden in private.parents for forbidden in (root, project, public_root)):
        parser.error('Decoded game bytes must stay outside the installation, repository and public outputs')
    stem = f'bank_{args.archive}_{args.bank_index}'
    destination = private / f'{stem}.decoded.bin'
    metadata_destination = private / f'{stem}.directory.json'
    if destination.exists() or metadata_destination.exists():
        parser.error('Private destination already exists; preserve the previous capture')
    report_destination = args.report.resolve() if args.report else None
    if report_destination and (report_destination.exists() or report_destination == root or root in report_destination.parents):
        parser.error('Choose a new metadata report outside the installation')
    _, entries = read_index(root / f'alone_compressed_{args.archive}.fat')
    if not 0 <= args.bank_index < len(entries):
        parser.error('Bank index out of range')
    entry = entries[args.bank_index]
    payload = root / f'alone_compressed_{args.archive}.big'
    if entry['offset'] + entry['padded_size'] > payload.stat().st_size:
        parser.error('Entry exceeds BIG payload')
    with payload.open('rb') as stream:
        decoded, bank, report = decode(stream, entry, args.max_mib * 1024 * 1024)
    report.update(archive=args.archive, bank_index=args.bank_index, bank_hashcode=entry['hashcode'])
    private.mkdir(parents=True, exist_ok=True)
    created = []
    try:
        with destination.open('xb') as stream:
            created.append(destination)
            stream.write(decoded)
        with metadata_destination.open('x', encoding='utf-8') as stream:
            created.append(metadata_destination)
            json.dump(bank, stream, indent=2)
        if report_destination:
            report_destination.parent.mkdir(parents=True, exist_ok=True)
            with report_destination.open('x', encoding='utf-8') as stream:
                created.append(report_destination)
                json.dump(report, stream, indent=2)
    except Exception:
        for path in reversed(created):
            path.unlink(missing_ok=True)
        raise
    print(json.dumps(report))

if __name__ == '__main__':
    main()
