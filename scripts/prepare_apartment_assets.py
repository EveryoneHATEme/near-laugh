"""Prepare only the selected base-color game assets; never modify the source pack."""

import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct


def read_glb(path):
    raw = path.read_bytes()
    magic, version, size = struct.unpack_from('<III', raw)
    if (magic, version, size) != (0x46546C67, 2, len(raw)):
        raise ValueError(f'{path}: invalid GLB header')
    json_size, json_type = struct.unpack_from('<II', raw, 12)
    if json_type != 0x4E4F534A:
        raise ValueError(f'{path}: missing JSON chunk')
    document = json.loads(raw[20:20 + json_size])
    binary_size, binary_type = struct.unpack_from('<II', raw, 20 + json_size)
    if binary_type != 0x004E4942:
        raise ValueError(f'{path}: missing BIN chunk')
    binary = raw[28 + json_size:28 + json_size + binary_size]
    return document, binary, hashlib.sha256(raw).hexdigest()


def image_bytes(document, binary, image_index):
    image = document['images'][image_index]
    if image.get('mimeType') != 'image/png' or 'uri' in image:
        raise ValueError('Selected image must be an embedded PNG')
    view = document['bufferViews'][image['bufferView']]
    start = view.get('byteOffset', 0)
    return binary[start:start + view['byteLength']]


def prepare_model(source, target, name):
    doc, binary, digest = read_glb(source)
    if (len(doc['nodes']) != 1 or len(doc['meshes']) != 1
            or len(doc['meshes'][0]['primitives']) != 1
            or doc['nodes'][0].get('children') or doc.get('animations')
            or doc.get('skins')):
        raise ValueError(f'{source}: selected profile changed')
    primitive = copy.deepcopy(doc['meshes'][0]['primitives'][0])
    if (set(primitive['attributes']) != {'POSITION', 'NORMAL', 'TEXCOORD_0'}
            or primitive.get('mode', 4) != 4 or primitive.get('targets')):
        raise ValueError(f'{source}: selected geometry profile changed')
    original_material = doc['materials'][primitive['material']]
    primitive['material'] = 0
    image_index = doc['textures'][original_material['pbrMetallicRoughness']
                                 ['baseColorTexture']['index']]['source']
    image_view = doc['images'][image_index]['bufferView']
    accessors = copy.deepcopy(doc['accessors'])
    used_views = sorted({a['bufferView'] for a in accessors} | {image_view})
    packed = bytearray()
    views = []
    remap = {}
    for old_index in used_views:
        old = doc['bufferViews'][old_index]
        packed.extend(b'\0' * (-len(packed) % 4))
        view = {k: v for k, v in old.items()
                if k in ('byteLength', 'byteStride', 'target')}
        view.update(buffer=0, byteOffset=len(packed))
        offset = old.get('byteOffset', 0)
        packed.extend(binary[offset:offset + old['byteLength']])
        remap[old_index] = len(views)
        views.append(view)
    for accessor in accessors:
        accessor['bufferView'] = remap[accessor['bufferView']]
    material = {'name': name,
                'pbrMetallicRoughness': {
                    'baseColorTexture': {'index': 0},
                    'baseColorFactor': [1, 1, 1, 1],
                    'metallicFactor': 0, 'roughnessFactor': 1},
                'alphaMode': 'MASK' if name == 'phone' else 'OPAQUE'}
    if name == 'phone':
        material['alphaCutoff'] = 0.5
    node = {k: v for k, v in doc['nodes'][0].items()
            if k in ('mesh', 'name', 'matrix', 'translation', 'rotation', 'scale')}
    output = {'asset': {'version': '2.0', 'generator': 'near-laugh selected static profile 1'},
              'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [node],
              'meshes': [{'primitives': [primitive]}], 'accessors': accessors,
              'bufferViews': views, 'buffers': [{'byteLength': len(packed)}],
              'materials': [material], 'textures': [{'sampler': 0, 'source': 0}],
              'samplers': [{'magFilter': 9728, 'minFilter': 9984,
                            'wrapS': 10497, 'wrapT': 10497}],
              'images': [{'bufferView': remap[image_view], 'mimeType': 'image/png'}]}
    json_chunk = json.dumps(output, separators=(',', ':')).encode('utf-8')
    json_chunk += b' ' * (-len(json_chunk) % 4)
    packed.extend(b'\0' * (-len(packed) % 4))
    result = (struct.pack('<III', 0x46546C67, 2, 28 + len(json_chunk) + len(packed))
              + struct.pack('<II', len(json_chunk), 0x4E4F534A) + json_chunk
              + struct.pack('<II', len(packed), 0x004E4942) + packed)
    target.write_bytes(result)
    # Verify preserved source vertex/index views and base-color bytes.
    staged, staged_binary, _ = read_glb(target)
    if image_bytes(staged, staged_binary, 0) != image_bytes(doc, binary, image_index):
        raise ValueError('Base-color image changed during staging')
    for old_index in used_views:
        old, new = doc['bufferViews'][old_index], views[remap[old_index]]
        a, b = old.get('byteOffset', 0), new['byteOffset']
        if binary[a:a + old['byteLength']] != staged_binary[b:b + new['byteLength']]:
            raise ValueError('Geometry/image bytes changed during staging')
    if hashlib.sha256(source.read_bytes()).hexdigest() != digest:
        raise ValueError('Source asset changed during preparation')
    return digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path('house_interior_pack'))
    parser.add_argument('--resources', type=Path, default=Path('resources'))
    args = parser.parse_args()
    source_root = args.source.resolve()
    resource_root = args.resources.resolve()
    if resource_root == source_root or source_root in resource_root.parents:
        parser.error('Prepared resources must be outside the read-only source pack')
    models = args.resources / 'models'
    textures = args.resources / 'textures'
    models.mkdir(parents=True, exist_ok=True)
    textures.mkdir(parents=True, exist_ok=True)
    records = []
    for name in ('chair', 'table', 'phone', 'radio'):
        relative = f'props/{name}.glb'
        target = models / f'apartment_{name}.glb'
        digest = prepare_model(args.source / relative, target, name)
        records.append({'source': relative, 'sha256': digest,
                        'output': f'models/{target.name}'})
    for source_name, output_name in (('floorWood', 'apartment_wood_floor'),
                                     ('wallWallpaper', 'apartment_wallpaper')):
        relative = f'geometry/{source_name}.glb'
        doc, binary, digest = read_glb(args.source / relative)
        texture = doc['materials'][0]['pbrMetallicRoughness']['baseColorTexture']['index']
        pixels = image_bytes(doc, binary, doc['textures'][texture]['source'])
        target = textures / f'{output_name}.png'
        target.write_bytes(pixels)
        records.append({'source': relative, 'sha256': digest,
                        'output': f'textures/{target.name}'})
    for record in records:
        if hashlib.sha256((args.source / record['source']).read_bytes()).hexdigest() != record['sha256']:
            raise ValueError('Source asset changed during preparation')
    manifest = {'profile': 1, 'source': 'User-supplied house_interior_pack',
                'author': 'DissonantVoid',
                'source_url': 'https://dissonantvoid.itch.io/psx-retro-interior-pack',
                'assets': records}
    (models / 'apartment_assets.sources.json').write_text(
        json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'Prepared {len(records)} selected resources; source pack unchanged.')


if __name__ == '__main__':
    main()
