"""Extract owned chapters credits by native sprite placement and glyph lookup.

Exact supported resource only. Output is generated locally: no game text is
embedded in this source tool. Movie/timeline bytes are never modified.
"""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import struct
from menu_credit_text import audit

SOURCE_SHA = '7671e27915e539b1af12592cb8f868f1bd58910036e6f70d121ec342db299a3f'
CREDITS_SPRITE = 393
DISPLAY_CHARACTERS = {0x96: '\u2013', 0x99: '\u2122', 0x93: '\u201c', 0x94: '\u201d'}


def extract(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA:
        raise ValueError('Unknown chapters movie identity')
    report = audit(data, True)
    texts = {t['dictionary_id']: t for t in report['static_text']}
    stack, state, first = [], {}, {}
    cursor = 72
    frame = 0
    while cursor < len(data) - 16:
        kind, size = struct.unpack_from('<II', data, cursor)
        if kind == 39:
            stack.append(struct.unpack_from('<I', data, cursor + 8)[0])
        elif kind == 0:
            if not stack:
                raise ValueError('Unbalanced native sprite stack')
            stack.pop()
        elif stack and stack[-1] == CREDITS_SPRITE:
            if kind == 26:
                depth, flags, matrix_flags = struct.unpack_from('<HBB', data, cursor + 8)
                value = state.get(depth, dict(id=None, x=0, y=0, sx=1, sy=1)).copy()
                at = cursor + 12
                if flags & 2:
                    value['id'] = struct.unpack_from('<I', data, at)[0]
                    at += 4
                if flags & 4:
                    if matrix_flags not in (0, 1, 2, 3):
                        raise ValueError('Unverified credits placement transform')
                    value.update(x=0, y=0, sx=1, sy=1)
                    if matrix_flags & 1:
                        value['sx'], value['sy'] = struct.unpack_from('<2f', data, at)
                        at += 8
                    if matrix_flags & 2:
                        value['x'], value['y'] = struct.unpack_from('<2f', data, at)
                        at += 8
                if at > cursor + size:
                    raise ValueError('Placement extent differs')
                state[depth] = value
                if flags & 2 and value['id'] in texts and value['id'] not in first:
                    first[value['id']] = dict(frame=frame, depth=depth, placement_offset=cursor, **value)
            elif kind == 1:
                frame += 1
        cursor += size
    if stack != [0] or frame != 603 or len(first) != 246:
        raise ValueError('Known credits timeline coverage differs')
    pages = defaultdict(list)
    for key, placement in first.items():
        text = texts[key]
        matrix = struct.unpack_from('<6f', data, text['tag_offset'] + 28)
        if matrix[:4] != (1., 1., 0., 0.):
            raise ValueError('Unverified native text local transform')
        lines = []
        for run in text['runs']:
            for codepoint, pos in zip(run['codepoints'], run['positions']):
                if not lines or lines[-1]['local_y'] != pos[1]:
                    lines.append(dict(local_y=pos[1], local_x=pos[0], codepoints=[]))
                lines[-1]['codepoints'].append(codepoint)
        for line in lines:
            pages[placement['frame']].append(dict(
                tag_id=key, tag_offset=text['tag_offset'], depth=placement['depth'],
                x=placement['x'] + placement['sx'] * (matrix[4] + line['local_x']),
                y=placement['y'] + placement['sy'] * (matrix[5] + line['local_y']),
                codepoints=line['codepoints']))
    output = []
    provenance_pages = []
    duplicates = []
    for number in sorted(pages):
        # Duplicate DESIGN TEAM plates are authored foreground/shadow copies.
        # Suppress only identical complete bodies on the same timeline page.
        by_tag = defaultdict(list)
        for line in pages[number]:
            by_tag[line['tag_id']].append(line)
        bodies = {}
        excluded = set()
        for tag, lines in by_tag.items():
            body = tuple(tuple(line['codepoints']) for line in lines)
            if body in bodies:
                excluded.add(tag)
                duplicates.append(dict(frame=number, kept_tag=bodies[body], duplicate_tag=tag))
            else:
                bodies[body] = tag
        lines = sorted((line for line in pages[number] if line['tag_id'] not in excluded), key=lambda line: (line['y'], line['x']))
        rows = []
        for line in lines:
            value = ''.join(DISPLAY_CHARACTERS.get(cp, chr(cp)) for cp in line['codepoints']).rstrip()
            if not value:
                continue
            if rows and abs(rows[-1]['y'] - line['y']) < .01:
                rows[-1]['columns'].append(value)
            else:
                rows.append(dict(y=line['y'], columns=[value]))
        output.append('\n'.join(' — '.join(row['columns']) for row in rows))
        provenance_pages.append(dict(frame=number, lines=lines, display_rows=rows))
    text = '\n\n'.join(output) + '\n'
    return text, {'schema': 1, 'source_sha256': SOURCE_SHA, 'credits_sprite': CREDITS_SPRITE,
                  'timeline_frames': frame, 'placed_text_tag_count': len(first),
                  'all_static_text_tags': len(texts), 'excluded_noncredits_text_tags': sorted(set(texts) - set(first)),
                  'source_glyph_lookup': 'Exact inline font index->codepoint table; original signed advances and Y position commands',
                  'ordering': 'First timeline placement frame, then native transformed line Y/X; columns at identical baselines joined with an em dash',
                  'duplicate_bodies': duplicates, 'legacy_display_mapping': {hex(cp): ch for cp, ch in DISPLAY_CHARACTERS.items()},
                  'mapping_note': 'Original C1 glyph keys retained in raw evidence; four legacy Windows-1252 punctuation keys rendered as dash/trademark/quotes in UTF8 display text',
                  'runtime_verified': False, 'pages': provenance_pages}


