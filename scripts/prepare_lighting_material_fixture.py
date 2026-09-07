"""Build isolated OPAQUE/MASK shadow controls using the existing model loader."""
import argparse
import json
from pathlib import Path
import shutil
import struct
import zlib


def quad_model(mode, factor_alpha, reverse=False):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    # A transparent center and solid ring distinguish
    # texture alpha, factor alpha and cutoff from opaque triangle coverage.
    pixels = b"".join(b"\0" + b"".join(bytes((255,255,255,
        0 if 1 <= x <= 2 and 1 <= y <= 2 else 255)) for x in range(4)) for y in range(4))
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB",4,4,8,6,0,0,0)) +
           chunk(b"IDAT",zlib.compress(pixels)) + chunk(b"IEND",b""))
    binary = bytearray()
    views = []
    def append(data):
        binary.extend(b"\0" * (-len(binary) % 4))
        views.append(dict(buffer=0, byteOffset=len(binary), byteLength=len(data)))
        binary.extend(data)
        return len(views)-1
    positions = append(struct.pack("<12f",-1,1,-1, 1,1,-1, 1,1,1, -1,1,1))
    normals = append(struct.pack("<12f", *([0,1,0]*4)))
    uvs = append(struct.pack("<8f",0,0,1,0,1,1,0,1))
    indices = append(struct.pack("<6H",*( (0,1,2,0,2,3) if reverse else (0,2,1,0,3,2) )))
    image = append(png)
    doc = dict(asset=dict(version="2.0"), scene=0, scenes=[dict(nodes=[0])],
        nodes=[dict(mesh=0)], meshes=[dict(primitives=[dict(attributes={"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},indices=3,material=0)])],
        accessors=[dict(bufferView=positions,componentType=5126,count=4,type="VEC3",min=[-1,1,-1],max=[1,1,1]),
                   dict(bufferView=normals,componentType=5126,count=4,type="VEC3"),
                   dict(bufferView=uvs,componentType=5126,count=4,type="VEC2"),
                   dict(bufferView=indices,componentType=5123,count=6,type="SCALAR")],
        bufferViews=views, buffers=[dict(byteLength=len(binary))],
        materials=[dict(pbrMetallicRoughness=dict(baseColorFactor=[1,1,1,factor_alpha],baseColorTexture=dict(index=0),metallicFactor=0,roughnessFactor=1),alphaMode=mode,alphaCutoff=.5)],
        textures=[dict(source=0,sampler=0)], images=[dict(bufferView=image,mimeType="image/png")],
        samplers=[dict(magFilter=9728,minFilter=9984,wrapS=10497,wrapT=10497)])
    encoded = json.dumps(doc,separators=(",",":")).encode()
    encoded += b" " * (-len(encoded)%4)
    binary.extend(b"\0" * (-len(binary)%4))
    return (struct.pack("<III",0x46546c67,2,28+len(encoded)+len(binary)) +
            struct.pack("<II",len(encoded),0x4e4f534a) + encoded +
            struct.pack("<II",len(binary),0x004e4942) + binary)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output",type=Path)
    args=parser.parse_args()
    if args.output.exists():
        parser.error("Choose a fresh fixture directory")
    args.output.mkdir(parents=True)
    for directory in ("shaders","textures","fonts"):
        shutil.copytree(Path("resources")/directory,args.output/directory)
    (args.output/"models").mkdir()
    for name,mode,alpha in (("phone","MASK",.6),("radio","OPAQUE",0),("chair","MASK",.4),("table","MASK",.6)):
        (args.output/"models"/f"apartment_{name}.glb").write_bytes(quad_model(mode,alpha,name=="table"))


if __name__=="__main__":
    main()
