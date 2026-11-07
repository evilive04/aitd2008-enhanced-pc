"""Lossless inline bitmap-font and native text-stream codec; read-only CLI.

Native text opcodes are supported by the existing private 0x928410 analysis.
No glyph outline format is invented. Original text export must remain private.
"""
import argparse
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import struct
from inspect_vmf import inspect as inspect_header
from menu_credit_fonts import inspect as inspect_links


@dataclass(frozen=True)
class Glyph:
    codepoint: int
    relative: int
    raw: bytes

    def operands(self):
        return struct.unpack('<2I13f', self.raw)


class Font:
    def __init__(self, raw):
        if len(raw) < 1112 or struct.unpack_from('<II', raw) != (10, len(raw)):
            raise ValueError('Invalid inline font extent')
        self.prefix = raw[:88]
        self.font_id, count = struct.unpack_from('<HH', raw, 8)
        self.atlas_id = struct.unpack_from('<I', raw, 12)[0]
        self.name = raw[24:88].split(b'\0')[0].decode('ascii')
        if len(raw) != 1112 + count * 68:
            raise ValueError('Unsupported font record stride')
        self.fast = struct.unpack_from('<256I', raw, 88)
        self.glyphs = []
        for i in range(count):
            codepoint, relative = struct.unpack_from('<II', raw, 1112 + i * 8)
            start = 1112 + count * 8 + i * 60
            if relative + 4 != start:
                raise ValueError('Glyph relative offset differs')
            glyph = Glyph(codepoint, relative, raw[start:start + 60])
            values = glyph.operands()
            if values[1] != 8 or values[-1] != 0 or not all(math.isfinite(v) for v in values[2:]):
                raise ValueError('Unsupported bitmap glyph command/terminator/operands')
            self.glyphs.append(glyph)
        mapping = {g.codepoint: g.relative for g in self.glyphs}
        if len(mapping) != count or list(mapping) != sorted(mapping):
            raise ValueError('Unsorted or duplicate glyph codepoints')
        if any(offset != mapping.get(cp, 0) for cp, offset in enumerate(self.fast)):
            raise ValueError('Fast and complete glyph lookups disagree')

    def serialize(self):
        return (self.prefix + struct.pack('<256I', *self.fast)
                + b''.join(struct.pack('<II', g.codepoint, g.relative) for g in self.glyphs)
                + b''.join(g.raw for g in self.glyphs))


@dataclass(frozen=True)
class Command:
    opcode: int
    payload: tuple

    FORMATS = {1: '<Bh', 2: '<Bh', 4: '<3BI', 8: '<BHI', 16: '<BHH', 255: '<Bh'}

    def serialize(self):
        return bytes([self.opcode]) + struct.pack(self.FORMATS[self.opcode], *self.payload)


class Text:
    def __init__(self, raw, fonts):
        if len(raw) < 56 or struct.unpack_from('<II', raw) != (11, len(raw)):
            raise ValueError('Invalid native text extent')
        self.prefix = raw[:52]
        self.commands = []
        self.runs = []
        self.tail = b''
        cursor = 52
        font = None
        remaining = 0
        current = None
        x = y = 0
        scale = (1024, 1024)
        while cursor < len(raw):
            opcode = raw[cursor]
            if not opcode:
                self.tail = raw[cursor:]
                if any(self.tail) or remaining:
                    raise ValueError('Text terminator/padding or glyph count differs')
                return
            fmt = Command.FORMATS.get(opcode)
            if fmt is None or cursor + 1 + struct.calcsize(fmt) > len(raw):
                raise ValueError('Unknown/truncated native text opcode')
            payload = struct.unpack_from(fmt, raw, cursor + 1)
            self.commands.append(Command(opcode, payload))
            if opcode == 8:
                if remaining:
                    raise ValueError('Unfinished previous text run')
                font = fonts.get(payload[1])
                if font is None:
                    raise ValueError('Unknown text font dictionary ID')
                remaining = payload[2]
                if not 1 <= remaining <= 65536:
                    raise ValueError('Invalid native glyph count')
                current = {'font_id': font.font_id, 'font_name': font.name,
                           'codepoints': [], 'advances': [], 'positions': []}
                self.runs.append(current)
            elif opcode == 1:
                x = payload[1]
            elif opcode == 2:
                y = payload[1]
            elif opcode == 16:
                scale = (payload[1], payload[2])
            elif opcode == 255:
                if font is None or not remaining or payload[0] >= len(font.glyphs):
                    raise ValueError('Text glyph index/count differs')
                current['codepoints'].append(font.glyphs[payload[0]].codepoint)
                current['advances'].append(payload[1])
                current['positions'].append((x, y, *scale))
                x += payload[1]
                remaining -= 1
            cursor += 1 + struct.calcsize(fmt)
        raise ValueError('Native text terminator missing')

    def serialize(self):
        return self.prefix + b''.join(c.serialize() for c in self.commands) + self.tail


def audit(data, include_text=False):
    inspect_header(data)
    links = inspect_links(data)
    fonts = {}
    tags = []
    cursor = 72
    while cursor < len(data) - 16:
        kind, size = struct.unpack_from('<II', data, cursor)
        raw = data[cursor:cursor + size]
        if kind == 10:
            font = Font(raw)
            if font.font_id in fonts or font.serialize() != raw:
                raise ValueError('Duplicate font ID or failed exact font roundtrip')
            fonts[font.font_id] = font
        tags.append((kind, cursor, raw))
        cursor += size
    rebuilt = [data[:72]]
    static = []
    dynamic = []
    for kind, offset, raw in tags:
        replacement = raw
        if kind == 10:
            replacement = fonts[struct.unpack_from('<H', raw, 8)[0]].serialize()
        elif kind == 11:
            text = Text(raw, fonts)
            replacement = text.serialize()
            entry = {'tag_offset': offset, 'dictionary_id': struct.unpack_from('<I', raw, 8)[0],
                     'run_count': len(text.runs), 'glyphs': sum(len(r['codepoints']) for r in text.runs)}
            if include_text:
                entry['runs'] = text.runs
            static.append(entry)
        elif kind == 37:
            if len(raw) < 32:
                raise ValueError('Truncated dynamic text tag')
            font_id, height_word = struct.unpack_from('<HH', raw, 28)
            if font_id not in fonts:
                raise ValueError('Dynamic text references an unknown font ID')
            dynamic.append({'tag_offset': offset, 'dictionary_id': struct.unpack_from('<I', raw, 8)[0],
                            'font_id': font_id, 'height_word': height_word})
        if replacement != raw:
            raise ValueError('Tag roundtrip mismatch')
        rebuilt.append(replacement)
    rebuilt.append(data[-16:])
    if b''.join(rebuilt) != data:
        raise ValueError('Complete movie roundtrip mismatch')
    return {'sha256': hashlib.sha256(data).hexdigest(), 'fonts': [
        {'dictionary_id': f.font_id, 'name': f.name, 'glyph_count': len(f.glyphs),
         'atlas_id': f.atlas_id, 'bitmap_quad_commands': len(f.glyphs)} for f in fonts.values()],
        'static_text': static, 'dynamic_text': dynamic, 'exact_roundtrip': True,
        'font_descriptor_links_validated': len(links['fonts']), 'runtime_verified': False}


