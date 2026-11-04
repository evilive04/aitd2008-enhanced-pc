"""Semantic native UI recipes; all preserved game data comes from local input.

The recipe language permits authored triangle coordinates and solid colors,
references to original fill/triangle records, and named UI field operations.
It deliberately has no opaque byte insertion, binary-delta, font or bitmap
payload operation. The caller authenticates the complete original resource.
"""
from __future__ import annotations

import hashlib
import math
import struct

from vmf_vector_style import Movie
from vmf_place_codec import parse as parse_place


ADAPTERS = ((621, 'epc_previous_sequence'), (622, 'epc_previous_chapter'),
            (623, 'epc_play'), (624, 'epc_next_chapter'),
            (625, 'epc_next_sequence'))


def sha(data):
    return hashlib.sha256(data).hexdigest()


def shape_parts(data, movie, identity, *, allow_nontriangular=False):
    """Read bounded original triangle groups/fills by semantic shape identity."""
    info = movie.shapes[identity]
    start, size = info['offset'], info['size']
    styles = [data[s.offset:s.offset+s.size] for s in movie.styles
              if s.shape == identity]
    at = info['geometry_offset']
    groups = []
    while at + 4 <= start + size:
        command = struct.unpack_from('<I', data, at)[0]
        if command == 0:
            break
        if command != 8 and allow_nontriangular:
            # Some replaced stock chrome uses another native primitive. No
            # source geometry is copied from these shapes; fills still come
            # from the independently parsed, exact-hash-checked style table.
            return tuple(struct.unpack_from('<4f',data,start+12)),styles,[]
        if command != 8 or at + 8 > start + size:
            raise ValueError('Unsupported native shape primitive')
        _, count, style = struct.unpack_from('<IHH', data, at)
        if not count or not 1 <= style <= len(styles) or at + 8 + count*12 > start+size:
            raise ValueError('Native triangle group exceeds shape')
        triangles = [tuple(struct.unpack_from('<6h', data, at+8+i*12))
                     for i in range(count)]
        groups.append((style, triangles))
        at += 8 + count*12
    else:
        raise ValueError('Shape terminator is absent')
    return tuple(struct.unpack_from('<4f', data, start+12)), styles, groups


def _integer(value, low, high):
    if isinstance(value, bool) or not isinstance(value, int) or not low <= value <= high:
        raise ValueError('Recipe integer is outside bounds')
    return value


def encode_shape(original, movie, identity, recipe):
    info = movie.shapes[identity]
    source = original[info['offset']:info['offset']+info['size']]
    if sha(source) != recipe['source_tag_sha256']:
        raise ValueError('Original shape checksum differs')
    old_bounds, old_styles, old_groups = shape_parts(original, movie, identity,
                                                    allow_nontriangular=True)
    bounds = old_bounds if recipe['bounds'] == 'source' else recipe['bounds']
    if len(bounds) != 4 or any(not isinstance(v, (int, float)) or
                             not math.isfinite(v) or abs(v)>1e7 for v in bounds):
        raise ValueError('Invalid authored shape bounds')
    fills = []
    if not 1 <= len(recipe['fills']) <= 4096:
        raise ValueError('Unbounded shape fill list')
    for fill in recipe['fills']:
        if set(fill) == {'source_style'}:
            index = _integer(fill['source_style'], 1, len(old_styles))
            fills.append(old_styles[index-1])
        elif set(fill) == {'rgba'}:
            color = fill['rgba']
            if len(color) != 4:
                raise ValueError('RGBA must contain four channels')
            fills.append(struct.pack('<I4B', 0, *(_integer(v,0,255) for v in color)))
        else:
            raise ValueError('Only original fill references and authored solid colors are allowed')
    commands = bytearray()
    if not 1 <= len(recipe['groups']) <= 4096:
        raise ValueError('Unbounded shape group list')
    for group in recipe['groups']:
        style = _integer(group['style'],1,len(fills))
        triangles = []
        for part in group['triangles']:
            if set(part) == {'source_run'}:
                native_group, first, count = part['source_run']
                native_group = _integer(native_group,0,len(old_groups)-1)
                source_triangles = old_groups[native_group][1]
                first = _integer(first,0,len(source_triangles)-1)
                count = _integer(count,1,len(source_triangles)-first)
                triangles.extend(source_triangles[first:first+count])
            elif set(part) == {'authored'}:
                points = part['authored']
                if len(points) > 65535 or not points:
                    raise ValueError('Unbounded authored triangle run')
                for tri in points:
                    if len(tri) != 6:
                        raise ValueError('An authored triangle needs three 2D vertices')
                    triangles.append(tuple(_integer(v,-32768,32767) for v in tri))
            elif set(part) == {'source_crop'}:
                # Native textured fragments are geometry edits expressed in
                # an original triangle's basis. No original UVs or vertex
                # coordinates are duplicated in the public numeric recipe.
                crop = part['source_crop']
                g = _integer(crop['group'],0,len(old_groups)-1)
                old = old_groups[g][1]
                t = _integer(crop['triangle'],0,len(old)-1)
                ax,ay,bx,by,cx,cy = old[t]
                tri = []
                if len(crop['weights'])!=3:
                    raise ValueError('A source triangle crop needs three vertices')
                for u,v,d in crop['weights']:
                    u = _integer(u,-(1<<63),(1<<63)-1)
                    v = _integer(v,-(1<<63),(1<<63)-1)
                    d = _integer(d,1,(1<<63)-1)
                    x = ax*d+u*(bx-ax)+v*(cx-ax)
                    y = ay*d+u*(by-ay)+v*(cy-ay)
                    if x%d or y%d:
                        raise ValueError('Source crop does not reconstruct integral native vertices')
                    tri.extend((_integer(x//d,-32768,32767),_integer(y//d,-32768,32767)))
                triangles.append(tuple(tri))
            else:
                raise ValueError('Only source triangle references/crops and authored vertices are allowed')
        if not 1 <= len(triangles) <= 65535:
            raise ValueError('Native triangle count exceeds ushort')
        commands.extend(struct.pack('<IHH',8,len(triangles),style))
        for tri in triangles:
            commands.extend(struct.pack('<6h',*tri))
    commands.extend(bytes(4))
    begin = 28+4*(len(fills)+1)
    cursor = begin+len(commands)
    offsets = [begin-4]
    for fill in fills:
        offsets.append(cursor-4)
        cursor += len(fill)
    raw = (struct.pack('<III4f',2,cursor,identity,*bounds) +
           struct.pack('<'+'I'*len(offsets),*offsets) + commands + b''.join(fills))
    if sha(raw) != recipe['output_tag_sha256']:
        raise ValueError('Authored shape recipe differs from reviewed output')
    return raw


def button_events(raw):
    if len(raw)<20 or struct.unpack_from('<II',raw)!=(7,len(raw)):
        raise ValueError('Not a bounded native button')
    at = struct.unpack_from('<I',raw,16)[0]+4
    if not 20 <= at < len(raw):
        raise ValueError('Button action table is invalid')
    rows = []
    while at < len(raw):
        if at+4>len(raw):
            raise ValueError('Truncated button action header')
        size,condition = struct.unpack_from('<HH',raw,at)
        end = at+size if size else len(raw)
        if not at+4 <= end <= len(raw):
            raise ValueError('Invalid button action extent')
        rows.append((condition,at+4,end))
        if not size:
            return rows
        at = end
    raise ValueError('Unterminated native button event list')


def _clear_hover(raw):
    result = bytearray(raw)
    found = 0
    for condition,start,end in button_events(raw):
        if condition == 1:
            result[start:end] = bytes(end-start)
            found += 1
    if found != 1:
        raise ValueError('Expected one native hover action')
    return bytes(result)


def _tag(kind, payload=b''):
    size = (8+len(payload)+3)&~3
    return struct.pack('<II',kind,size)+payload+bytes(size-8-len(payload))


def _recap_records(original, movie):
    buttons = {struct.unpack_from('<I',original,at+8)[0]:original[at:at+size]
               for kind,at,size in movie.tags if kind==7}
    out = []
    for identity,label in ADAPTERS:
        raw = buttons[identity]
        handlers = [raw[start:end] for condition,start,end in button_events(raw)
                    if condition==8]
        if len(handlers)!=1:
            raise ValueError('Original recap release handler is ambiguous')
        # Own Stop opcode, followed by a handler read locally from the game.
        out.extend((_tag(43,label.encode('ascii')+b'\0'),
                    _tag(12,struct.pack('<H',7)+handlers[0]),_tag(1)))
    return out


def _serialize_tree(node, additions):
    raw,children,terminal = node
    identity = struct.unpack_from('<I',raw,8)[0]
    parts = [_serialize_tree(child,additions) if isinstance(child,list) else child
             for child in children]
    if additions and identity == 626:
        parts[-1:-1] = additions
    offsets,extent = [0],0
    for part in parts:
        extent += len(part)
        if struct.unpack_from('<I',part)[0] == 1:
            offsets.append(extent)
    if offsets[-1] != extent-8:
        raise ValueError('Unexpected sprite frame termination')
    frames = len(offsets)-1
    size = 24+4*frames
    header = bytearray(raw[:20])+bytearray(size-20)
    struct.pack_into('<I',header,4,size)
    struct.pack_into('<H',header,12,frames)
    struct.pack_into('<I',header,16,size+extent)
    struct.pack_into('<'+'I'*(frames+1),header,20,*(size+value for value in offsets))
    return bytes(header)+b''.join(parts)


def build_movie(original, recipe):
    if recipe.get('version')!=1 or sha(original)!=recipe['source_sha256']:
        raise ValueError('Unsupported original movie or recipe version')
    movie = Movie(original)
    changed = {int(row['tag_index']):row for row in recipe['field_operations']}
    if len(changed)!=len(recipe['field_operations']):
        raise ValueError('Duplicate field operation')
    shapes = {int(k):v for k,v in recipe['shapes'].items()}
    root,stack = None,[]
    for index,(kind,at,size) in enumerate(movie.tags):
        raw = original[at:at+size]
        if kind==39:
            node = [raw,[],False]
            if stack:
                stack[-1][1].append(node)
            elif root is not None:
                raise ValueError('Multiple native movie roots')
            else:
                root = node
            stack.append(node)
            continue
        if not stack:
            raise ValueError('Native tag is outside a sprite')
        if kind==2:
            identity = struct.unpack_from('<I',raw,8)[0]
            if identity in shapes:
                raw = encode_shape(original,movie,identity,shapes.pop(identity))
        if index in changed:
            op = changed.pop(index)
            if sha(raw)!=op['source_tag_sha256']:
                raise ValueError('Field operation source tag differs')
            name = op['operation']
            if name=='clear_button_hover' and kind==7:
                raw = _clear_hover(raw)
            elif name=='neutral_placement_rgb' and kind==26:
                raw = parse_place(raw).neutral_rgb()
            elif name=='placement_rgb_tint' and kind==26:
                placement = parse_place(raw)
                if placement.color is None or len(op['multiply'])!=3 or len(op['add'])!=3:
                    raise ValueError('Native placement color field is absent')
                color = tuple(_integer(v,-32768,32767) for v in op['multiply']) + (placement.color[3],)
                color += tuple(_integer(v,-32768,32767) for v in op['add']) + (placement.color[7],)
                out = bytearray(raw)
                struct.pack_into('<8h',out,placement.color_offset,*color)
                raw = bytes(out)
            elif name=='clear_placeholder_text' and kind==37:
                # This exact source-checked native edit-text record has its
                # initial string in the final32-byte, nul-terminated field.
                if len(raw)!=152 or not raw[120:151] or raw[151]!=0:
                    raise ValueError('Unexpected native initial text field')
                raw = raw[:120]+bytes(32)
            elif name=='keycap_bitmap_dimensions' and kind==20:
                if len(raw)!=40 or struct.unpack_from('<III',raw,8)!=(33,64,64):
                    raise ValueError('Unexpected native keycap bitmap record')
                out = bytearray(raw)
                struct.pack_into('<II',out,12,1024,1024)
                raw = bytes(out)
            else:
                raise ValueError('Unsupported named semantic field operation')
            if sha(raw)!=op['output_tag_sha256']:
                raise ValueError('Semantic field output differs')
        stack[-1][1].append(raw)
        if kind==0:
            if len(stack)<2:
                raise ValueError('Unexpected root sprite terminator')
            stack.pop()
    if len(stack)!=1 or root is None or shapes or changed:
        raise ValueError('Incomplete recipe or native sprite tree')
    root[1].append(original[-16:-8])
    additions = _recap_records(original,movie) if recipe['recap_adapters'] else []
    output = original[:72]+_serialize_tree(root,additions)+original[-8:]
    if sha(output)!=recipe['output_sha256'] or len(output)!=recipe['output_bytes']:
        raise ValueError(f"Generated {recipe.get('movie','?')} movie differs: "
            f"got {len(output)} bytes/{sha(output)}; expected "
            f"{recipe['output_bytes']} bytes/{recipe['output_sha256']}")
    return output
