"""Lossless, explicit solid-style edits for the observed AITD VMF variant.

Never guesses which shape is a panel: inventory first, then an exact SHA256 and
shape/style/expected-RGBA edit list. Bitmap fills, geometry, scripts and IDs are
immutable. Original VMF bytes and generated replacements belong outside Git.
"""
import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct
from inspect_vmf import inspect


def sha(data):
    return hashlib.sha256(data).hexdigest()


@dataclass(frozen=True)
class Style:
    shape: int
    index: int
    kind: int
    offset: int
    size: int
    rgba: str | None


class Movie:
    def __init__(self, data):
        self.data = bytes(data)
        self.header = inspect(self.data)
        if self.header['kind'] != 'VMF':
            raise ValueError('Vector movie required, not a font')
        self.tags = []
        self.styles = []
        self.shapes = {}
        cursor = 72
        while cursor < len(data) - 16:
            kind, size = struct.unpack_from('<II', data, cursor)
            self.tags.append((kind, cursor, size))
            if kind == 2:
                self._shape(cursor, size)
            cursor += size

    def _shape(self, start, size):
        if size < 44:
            raise ValueError('Truncated native vector shape')
        data = self.data
        shape = struct.unpack_from('<I', data, start + 8)[0]
        if shape in self.shapes:
            raise ValueError('Duplicate shape ID')
        # Offsets are relative to the tag's size word, not its beginning.
        geometry, first_style = struct.unpack_from('<II', data, start + 28)
        table_end = min(geometry, first_style) + 4
        if not 36 <= table_end <= size or (table_end - 28) % 4:
            raise ValueError('Invalid native shape offset table')
        count = (table_end - 28) // 4
        offsets = tuple(value + 4 for value in struct.unpack_from(
            '<' + 'I' * count, data, start + 28))
        if len(set(offsets)) != count or any(not table_end <= o < size for o in offsets):
            raise ValueError('Invalid or overlapping shape block offsets')
        ordered = sorted(offsets) + [size]
        ends = {a: b for a, b in zip(ordered, ordered[1:])}
        self.shapes[shape] = {'offset': start, 'size': size,
                              'geometry_offset': start + offsets[0],
                              'style_count': count - 1}
        for index, relative in enumerate(offsets[1:], 1):
            extent = ends[relative] - relative
            if extent < 8:
                raise ValueError('Truncated native vector fill')
            absolute = start + relative
            kind = struct.unpack_from('<I', data, absolute)[0]
            if kind == 0 and extent != 8:
                raise ValueError('Unsupported solid fill layout')
            if kind == 0x41 and extent != 32:
                raise ValueError('Unsupported bitmap fill layout')
            self.styles.append(Style(shape, index, kind, absolute, extent,
                                     data[absolute + 4:absolute + 8].hex() if kind == 0 else None))

    def serialize(self):
        # Deliberately concatenate parsed tags, proving byte-complete coverage.
        return self.data[:72] + b''.join(self.data[o:o + s] for _, o, s in self.tags) + self.data[-16:]

    def inventory(self):
        return {'schema': 1, 'sha256': sha(self.data), 'bytes': len(self.data),
                'shape_count': len(self.shapes), 'styles': [s.__dict__ for s in self.styles],
                'runtime_verified': False}

    def patch(self, manifest):
        if manifest.get('schema') != 1 or manifest.get('source_sha256') != sha(self.data):
            raise ValueError('Manifest schema or exact source identity differs')
        edits = manifest.get('edits')
        if not isinstance(edits, list) or not 1 <= len(edits) <= 4096:
            raise ValueError('An explicit bounded edit list is required')
        lookup = {(s.shape, s.index): s for s in self.styles}
        changed = set()
        result = bytearray(self.data)
        seen = set()
        for edit in edits:
            key = (edit['shape'], edit['style'])
            if key in seen:
                raise ValueError('Duplicate edit')
            seen.add(key)
            style = lookup.get(key)
            if style is None or style.kind != 0:
                raise ValueError('Only an existing solid fill may be edited')
            old, new = edit['expected_rgba'], edit['rgba']
            if old != style.rgba:
                raise ValueError('Expected native color differs')
            if (not isinstance(new, str) or len(new) != 8
                    or any(ch not in '0123456789abcdefABCDEF' for ch in new)):
                raise ValueError('RGBA must be eight hex characters')
            color = bytes.fromhex(new)
            if color[3] != bytes.fromhex(old)[3]:
                raise ValueError('Authored alpha must remain unchanged')
            at = style.offset + 4
            result[at:at + 4] = color
            changed.update(range(at, at + 3))
        if any(a != b and i not in changed for i, (a, b) in enumerate(zip(self.data, result))):
            raise AssertionError('A non-color byte changed')
        parsed = Movie(result)
        if parsed.serialize() != result or parsed.shapes != self.shapes:
            raise AssertionError('Native layout did not roundtrip')
        return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('movie', type=Path)
    parser.add_argument('--inventory', type=Path)
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    movie = Movie(args.movie.read_bytes())
    if args.inventory:
        with args.inventory.open('x', encoding='utf-8') as stream:
            json.dump(movie.inventory(), stream, indent=2)
    if args.manifest:
        if not args.output or args.output.exists() or args.output.resolve() == args.movie.resolve():
            parser.error('Choose a new output file; original resources are never overwritten')
        result = movie.patch(json.loads(args.manifest.read_text(encoding='utf-8')))
        with args.output.open('xb') as stream:
            stream.write(result)
        print(json.dumps({'output_sha256': sha(result), 'bytes': len(result), 'runtime_verified': False}))
    elif not args.inventory:
        parser.error('Choose inventory or manifest operation')


if __name__ == '__main__':
    main()
