"""Losslessly encode fixed-view PPM readbacks and check stored-RGB patches."""
import argparse
import json
from pathlib import Path
import struct
import zlib


def read_ppm(path):
    magic, extent, maximum, pixels = path.read_bytes().split(b"\n", 3)
    width, height = map(int, extent.split())
    if magic != b"P6" or maximum != b"255" or len(pixels) != width * height * 3:
        raise ValueError(f"Unsupported capture: {path}")
    return width, height, pixels


def write_png(path, width, height, pixels):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    scanlines = b"".join(b"\0" + pixels[y*width*3:(y+1)*width*3] for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b""))


def read_png(path):
    """Decode and verify the lossless filter-zero RGB files written above."""
    data=path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"Not a PNG: {path}")
    offset=8
    compressed=bytearray()
    width=height=0
    while offset < len(data):
        size=struct.unpack_from(">I",data,offset)[0]
        kind=data[offset+4:offset+8]
        payload=data[offset+8:offset+8+size]
        crc=struct.unpack_from(">I",data,offset+8+size)[0]
        if zlib.crc32(kind+payload)!=crc:
            raise ValueError(f"Corrupt PNG: {path}")
        if kind==b"IHDR":
            width,height,depth,color,compression,filtering,interlace=struct.unpack(">IIBBBBB",payload)
            if (depth,color,compression,filtering,interlace)!=(8,2,0,0,0):
                raise ValueError(f"Unsupported PNG profile: {path}")
        elif kind==b"IDAT":
            compressed.extend(payload)
        offset+=size+12
    scanlines=zlib.decompress(compressed)
    stride=width*3+1
    if len(scanlines)!=stride*height or any(scanlines[y*stride]!=0 for y in range(height)):
        raise ValueError(f"Unsupported PNG scanlines: {path}")
    return width,height,b"".join(scanlines[y*stride+1:(y+1)*stride] for y in range(height))


def patch(pixels, width, rectangle):
    x, y, w, h = rectangle
    return b"".join(pixels[((y+row)*width+x)*3:((y+row)*width+x+w)*3] for row in range(h))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--prune-ppm",action="store_true",help="Remove duplicate PPM only after PNG pixel round-trip verification")
    args = parser.parse_args()
    captures = {}
    paths={path.stem:path for path in args.directory.glob("*.png")}
    paths.update({path.stem:path for path in args.directory.glob("*.ppm")})
    for path in paths.values():
        width, height, pixels = read_ppm(path) if path.suffix==".ppm" else read_png(path)
        if (width, height) != (1920, 1080):
            raise ValueError("Acceptance captures require 1920x1080")
        captures[path.stem] = pixels
        if path.suffix==".ppm":
            png=path.with_suffix(".png")
            write_png(png, width, height, pixels)
            if args.prune_ppm:
                if read_png(png)!=(width,height,pixels):
                    raise ValueError(f"PNG round-trip changed stored RGB: {path}")
                path.unlink()
    report = {}
    rectangle = [920, 510, 80, 60]
    for name in ("wall", "door-closed", "offscreen-wall", "face-seam"):
        on = patch(captures[name + "-on"], 1920, rectangle)
        off = patch(captures[name + "-off"], 1920, rectangle)
        differences = [abs(a-b) for a, b in zip(on, off)]
        mean, maximum = sum(differences)/len(differences), max(differences)
        report[name] = dict(patch_xywh=rectangle, mean_absolute_stored_rgb_error=mean,
                            maximum_channel_error=maximum, passed=mean <= 2 and maximum <= 4)
    for name in ("lit", "door-open"):
        rgb = patch(captures[name + "-on"], 1920, rectangle)
        mean = sum(rgb)/len(rgb)
        report[name] = dict(patch_xywh=rectangle, mean_stored_rgb=mean, passed=mean > 16)
    differences = [abs(a-b) for a, b in zip(captures["furnished-on"], captures["furnished-editor"])]
    report["editor-initial-agreement"] = dict(mean_absolute_stored_rgb_error=sum(differences)/len(differences),
        maximum_channel_error=max(differences), passed=max(differences) == 0)
    if "lit-unshadowed-on" in captures:
        differences = [abs(a-b) for a, b in zip(captures["lit-on"], captures["lit-unshadowed-on"])]
        report["unoccluded-surface"] = dict(mean_absolute_stored_rgb_error=sum(differences)/len(differences),
            maximum_channel_error=max(differences), passed=max(differences) <= 4)
    if "wall-flashlight-on" in captures:
        for name, left, right in (("flashlight-with-point-disabled","wall-flashlight-off","zero-lights-flashlight-on"),
                                  ("flashlight-through-blocked-point-patch","wall-flashlight-on","wall-flashlight-off")):
            a=patch(captures[left],1920,rectangle)
            b=patch(captures[right],1920,rectangle)
            differences=[abs(x-y) for x,y in zip(a,b)]
            report[name]=dict(patch_xywh=rectangle,maximum_channel_error=max(differences),
                              mean_stored_rgb=sum(a)/len(a),passed=max(differences)<=4 and sum(a)/len(a)>16)
        dark=patch(captures["furnished-zero-ambient-off"],1920,rectangle)
        report["zero-ambient-all-off"]=dict(patch_xywh=rectangle,maximum_channel=max(dark),passed=max(dark)==0)
    if "material-mask-on" in captures:
        for name,lit in (("material-mask",True),("material-opaque",False),("material-factor-discard",True)):
            a=patch(captures[name+"-on"],1920,rectangle)
            reference="material-unoccluded-on" if lit else name+"-off"
            b=patch(captures[reference],1920,rectangle)
            differences=[abs(x-y) for x,y in zip(a,b)]
            report[name]=dict(patch_xywh=rectangle,mean_absolute_stored_rgb_error=sum(differences)/len(differences),
                maximum_channel_error=max(differences),mean_stored_rgb=sum(a)/len(a),
                passed=max(differences)<=4 and (not lit or sum(a)/len(a)>16))
        ring=[980,650,40,30]
        rgb=patch(captures["material-mask-on"],1920,ring)
        reference=patch(captures["material-unoccluded-on"],1920,ring)
        report["material-mask-solid-ring"]=dict(patch_xywh=ring,maximum_channel=max(rgb),
            unoccluded_mean=sum(reference)/len(reference),passed=max(rgb)<=4 and sum(reference)/len(reference)>16)
        differences=[abs(x-y) for x,y in zip(captures["material-factor-discard-on"],captures["material-unoccluded-on"])]
        report["material-factor-discard-whole-frame"]=dict(maximum_channel_error=max(differences),passed=max(differences)<=4)
        if "material-backface-mask-on" in captures:
            differences=[abs(x-y) for x,y in zip(captures["material-mask-on"],captures["material-backface-mask-on"])]
            report["material-backface-coverage"]=dict(maximum_channel_error=max(differences),passed=max(differences)<=4)
    (args.directory / "patch-results.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    if not all(item["passed"] for item in report.values()):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
