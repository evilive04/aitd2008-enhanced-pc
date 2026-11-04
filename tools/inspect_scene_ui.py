"""Read observed XMBF schemas and selected scene UI metadata. No asset writer.

Type references are offsets in 4-byte units from the type-table base, not ordinal
indices. Array/string addresses are relative to the serialized data root.
Unknown layouts fail before any UI field is assigned a meaning.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

class Xmbf:
    def __init__(self, data):
        if not 28 <= len(data) <= 64 * 1024 * 1024:
            raise ValueError('XMBF size outside bounds')
        self.data = data
        magic, version, strings, types, root, root_name, root_type = struct.unpack_from('<7I', data)
        if magic != int.from_bytes(b'XMBF','little') or version != 0x101 or strings != 28:
            raise ValueError('Unsupported PC XMBF header')
        if not strings < types < root < len(data) or types % 4 or root % 4:
            raise ValueError('Invalid XMBF region boundaries')
        self.base = root
        names = {}
        cursor = strings
        while cursor < types:
            if types - cursor <= 7 and data[cursor:types] == b'A' * (types - cursor):
                break
            end = data.find(b'\0', cursor, types)
            if end < 0 or end - cursor > 4096 or len(names) >= 16384:
                raise ValueError('Invalid schema string pool')
            names[cursor - strings] = data[cursor:end].decode('ascii')
            cursor = end + 1
        self.types = {}
        cursor = types
        while cursor < root:
            if root - cursor <= 15 and data[cursor:root] == b'A' * (root - cursor):
                break
            if cursor + 8 > root or len(self.types) >= 8192:
                raise ValueError('Invalid type table extent')
            key = (cursor - types) // 4
            packed, half_size, name = struct.unpack_from('<HHI', data, cursor)
            cursor += 8
            kind = packed & 15
            count = (packed & 0x7ff0) >> 4
            if kind > 13 or name not in names or cursor + count * 8 > root:
                raise ValueError('Invalid type descriptor')
            fields = []
            for _ in range(count):
                field, reference = struct.unpack_from('<II', data, cursor)
                cursor += 8
                if field not in names:
                    raise ValueError('Field name outside string pool')
                fields.append((names[field], reference))
            self.types[key] = {'name': names[name], 'kind': kind,
                               'bytes': half_size * 2 + (packed >> 15), 'fields': fields}
        expected_sizes = (0,1,1,2,4,8,1,2,4,8,4,8,4,8)
        for key, definition in self.types.items():
            if definition['kind']:
                if definition['fields'] or definition['bytes'] != expected_sizes[definition['kind']]:
                    raise ValueError('Unsupported primitive serialization')
            else:
                measured = 0
                for name, reference in definition['fields']:
                    target = self.types.get(reference & 0x7fffffff)
                    if target is None:
                        raise ValueError('Type reference does not target a descriptor')
                    measured += 8 if reference >> 31 else target['bytes']
                if measured != definition['bytes']:
                    raise ValueError('Class field sizes disagree with declared packed size')
        if root_type not in self.types or self.types[root_type]['name'] != names.get(root_name):
            raise ValueError('Root type key and name disagree')
        self.root = root_type

    def span(self, relative, length):
        if relative < 0 or length < 0 or self.base + relative + length > len(self.data):
            raise ValueError('Serialized data range exceeds file')
        return self.base + relative

    def fields(self, key, relative):
        definition = self.types[key]
        if definition['kind'] != 0:
            raise ValueError('Object descriptor expected')
        self.span(relative, definition['bytes'])
        result = {}
        cursor = relative
        for name, reference in definition['fields']:
            if name in result:
                raise ValueError('Duplicate field name')
            target = reference & 0x7fffffff
            result[name] = (target, cursor, bool(reference >> 31))
            cursor += 8 if reference >> 31 else self.types[target]['bytes']
        return result

    def array(self, field, cap=20000):
        key, relative, array = field
        if not array:
            raise ValueError('Array field expected')
        count, pointer = struct.unpack_from('<II', self.data, self.span(relative, 8))
        stride = self.types[key]['bytes']
        if count > cap or (count and not stride):
            raise ValueError('Unsupported or excessive array extent')
        if count:
            self.span(pointer, count * stride)
        return [(key, pointer + i * stride) for i in range(count)]

    def value(self, field):
        key, relative, array = field
        if array:
            raise ValueError('Scalar field expected')
        kind = self.types[key]['kind']
        at = self.span(relative, self.types[key]['bytes'])
        formats = {1:'B',2:'b',3:'h',4:'i',5:'q',6:'B',7:'H',8:'I',9:'Q',10:'f',11:'d'}
        if kind in formats:
            return struct.unpack_from('<' + formats[kind], self.data, at)[0]
        if kind == 12:
            pointer = struct.unpack_from('<I', self.data, at)[0]
            if not pointer:
                return None
            start = self.span(pointer, 1)
            end = self.data.find(b'\0', start, min(start + 4097, len(self.data)))
            if end < 0:
                raise ValueError('Unterminated data string')
            return self.data[start:end].decode('utf-8')
        raise ValueError('Unsupported scalar kind')

    def resource(self, key, relative):
        if self.types[key]['name'] != '__StructScDataRsc':
            raise ValueError('Unexpected resource descriptor type')
        fields = self.fields(key, relative)
        return {'rsc': f'{self.value(fields["rsc"]):016x}',
                'rscType': self.value(fields['rscType']), 'rscSubType': self.value(fields['rscSubType'])}

def inspect(data):
    scene = Xmbf(data)
    root = scene.fields(scene.root, 0)
    export_key, export_at, is_array = root['ExportType_LocalData']
    if is_array:
        raise ValueError('Unexpected LocalData export wrapper')
    exported = scene.fields(export_key, export_at)['ArrayOf_LocalData']
    result = []
    for key, at in scene.array(exported, cap=256):
        fields = scene.fields(key, at)
        entry = {'name': scene.value(fields['__Name']), 'resources': {}, 'bitmap_setups': []}
        for name in ('FlashCommonTexts','FlashUnicodeFont','FlashBitmaps'):
            entry['resources'][name] = [scene.resource(k,p) for k,p in scene.array(fields[name])]
        for name in ('FlashGeneral','MainMenu','ChaptersMenu','CellPhone','Helper','Tips'):
            k,p,array = fields[name]
            if array or scene.types[k]['name'] != 'S_FlashDesc':
                raise ValueError('Unexpected Flash descriptor')
            description = scene.fields(k,p)
            entry['resources'][name] = {label:[scene.resource(ek,ep) for ek,ep in scene.array(description[label])]
                                        for label in ('Texts','Flash')}
        for k,p in scene.array(fields['FlashBitmapsSetups']):
            if scene.types[k]['name'] != 'S_FlashBitmapSetup':
                raise ValueError('Unexpected bitmap setup type')
            setup = scene.fields(k,p)
            entry['bitmap_setups'].append({name:scene.value(setup[name]) for name in ('BitmapName','RelativeWidth','BaseLine')})
        result.append(entry)
    return {'sha256': hashlib.sha256(data).hexdigest(), 'schema_types': len(scene.types),
            'schema_class_sizes_consistent': True, 'local_data': result,
            'scope': 'Selected serialized scene UI fields only; resource hashes are not resolved filenames or proven runtime overrides'}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('scene', type=Path)
    parser.add_argument('report', type=Path)
    args = parser.parse_args()
    output = args.report.resolve()
    if output.exists() or output == args.scene.resolve() or output.suffix.lower() != '.json':
        parser.error('Choose a new metadata JSON output')
    if args.scene.stat().st_size > 64 * 1024 * 1024:
        parser.error('Input exceeds limit')
    report = inspect(args.scene.read_bytes())
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
    print(json.dumps({'sha256': report['sha256'], 'schema_types': report['schema_types'],
                      'local_data_entries': len(report['local_data']),
                      'bitmap_setup_counts': [len(r['bitmap_setups']) for r in report['local_data']]}))

if __name__ == '__main__':
    main()
