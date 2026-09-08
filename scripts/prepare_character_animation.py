"""Prepare the pinned Quaternius mannequin; Python standard library only.

The source root is mandatory and remains read-only. Ordinary builds use the
committed derivative and never execute this script or require local downloads.
"""

import argparse
import bisect
import copy
import csv
import hashlib
import json
import math
from pathlib import Path
import struct
import zipfile
import zlib


SOURCE_GLB = "source/Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb"
SOURCE_ZIP = "downloads/Universal Animation Library[Standard].zip"
SOURCE_LICENSE = "source/Universal Animation Library[Standard]/License.txt"
SOURCE_SHA = "69591853d817488edaa8fd9bf8fc1d821eaeaf789f8627b3cd23b41c4ed67997"
ARCHIVE_SHA = "cc73fc4e495b82958207316596317a3f40b9fa38065bde1027937452da537724"
CLIPS = {"Idle_Loop": "idle", "Walk_Loop": "walk", "Interact": "interact"}
IDENTITY = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)
FORMATS = {5121: "B", 5123: "H", 5125: "I", 5126: "f"}
WIDTHS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def read_glb(raw):
    if len(raw) < 28 or struct.unpack_from("<III", raw) != (0x46546C67, 2, len(raw)):
        raise ValueError("Invalid GLB header")
    size, kind = struct.unpack_from("<II", raw, 12)
    if kind != 0x4E4F534A:
        raise ValueError("Missing JSON chunk")
    doc = json.loads(raw[20:20 + size])
    binary_size, kind = struct.unpack_from("<II", raw, 20 + size)
    if kind != 0x004E4942 or 28 + size + binary_size != len(raw):
        raise ValueError("Missing or invalid BIN chunk")
    return doc, raw[28 + size:]


def accessor(doc, binary, index):
    acc = doc["accessors"][index]
    view = doc["bufferViews"][acc["bufferView"]]
    fmt = "<" + FORMATS[acc["componentType"]] * WIDTHS[acc["type"]]
    width = struct.calcsize(fmt)
    stride = view.get("byteStride", width)
    offset = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    if acc.get("sparse") or stride < width or offset + (acc["count"] - 1) * stride + width > len(binary):
        raise ValueError(f"Accessor {index}: unsupported storage")
    return [struct.unpack_from(fmt, binary, offset + i * stride) for i in range(acc["count"])]


def normalize(q):
    length = math.sqrt(sum(v * v for v in q))
    if not math.isfinite(length) or length < 1e-10:
        raise ValueError("Degenerate quaternion")
    return tuple(v / length for v in q)


def trs(node):
    x, y, z, w = normalize(node.get("rotation", (0, 0, 0, 1)))
    sx, sy, sz = node.get("scale", (1, 1, 1))
    tx, ty, tz = node.get("translation", (0, 0, 0))
    return ((1 - 2 * (y*y + z*z)) * sx, 2 * (x*y - z*w) * sy, 2 * (x*z + y*w) * sz, tx,
            2 * (x*y + z*w) * sx, (1 - 2 * (x*x + z*z)) * sy, 2 * (y*z - x*w) * sz, ty,
            2 * (x*z - y*w) * sx, 2 * (y*z + x*w) * sy, (1 - 2 * (x*x + y*y)) * sz, tz,
            0., 0., 0., 1.)


def multiply(a, b):
    return tuple(sum(a[r*4+k] * b[k*4+c] for k in range(4)) for r in range(4) for c in range(4))


def inverse(a):
    rows = [[*a[r*4:r*4+4], *IDENTITY[r*4:r*4+4]] for r in range(4)]
    for c in range(4):
        pivot = max(range(c, 4), key=lambda r: abs(rows[r][c]))
        rows[c], rows[pivot] = rows[pivot], rows[c]
        scale = rows[c][c]
        if abs(scale) < 1e-12:
            raise ValueError("Singular bind transform")
        rows[c] = [v / scale for v in rows[c]]
        for r in range(4):
            if r != c:
                scale = rows[r][c]
                rows[r] = [v - scale * t for v, t in zip(rows[r], rows[c])]
    return tuple(v for row in rows for v in row[4:])


def globals_for(nodes, roots):
    result = [None] * len(nodes)

    def visit(index, parent):
        if result[index] is not None:
            raise ValueError("Hierarchy is not a tree")
        result[index] = multiply(parent, trs(nodes[index]))
        for child in nodes[index].get("children", []):
            visit(child, result[index])

    for root in roots:
        visit(root, IDENTITY)
    if any(value is None for value in result):
        raise ValueError("Unreachable node")
    return result


def transform(m, p):
    x, y, z = p
    return (m[0]*x + m[1]*y + m[2]*z + m[3],
            m[4]*x + m[5]*y + m[6]*z + m[7],
            m[8]*x + m[9]*y + m[10]*z + m[11])


class Model:
    def __init__(self, doc, binary):
        self.doc, self.binary = doc, binary
        self.joints = doc["skins"][0]["joints"]
        self.names = {node["name"]: i for i, node in enumerate(doc["nodes"])}
        self.binds = [tuple(value[c*4+r] for r in range(4) for c in range(4))
                      for value in accessor(doc, binary, doc["skins"][0]["inverseBindMatrices"])]
        self.clips = {}
        for clip in doc["animations"]:
            channels = []
            for channel in clip["channels"]:
                sampler = clip["samplers"][channel["sampler"]]
                channels.append((channel["target"]["node"], channel["target"]["path"],
                                 [x[0] for x in accessor(doc, binary, sampler["input"])],
                                 accessor(doc, binary, sampler["output"])))
            self.clips[clip["name"]] = channels
        self.primitives = []
        for primitive in doc["meshes"][0]["primitives"]:
            attrs = {name: accessor(doc, binary, index) for name, index in primitive["attributes"].items()}
            indices = [i[0] for i in accessor(doc, binary, primitive["indices"])]
            self.primitives.append((attrs, indices, primitive["material"]))

    def pose(self, clip=None, time=0):
        nodes = [dict(node) for node in self.doc["nodes"]]
        if clip is not None:
            for node, path, times, values in self.clips[clip]:
                hi = min(bisect.bisect_right(times, time), len(times)-1)
                lo = max(0, hi-1)
                fraction = max(0., min(1., (time-times[lo])/(times[hi]-times[lo]))) if hi != lo else 0.
                first, last = values[lo], values[hi]
                if path == "rotation":
                    first, last = normalize(first), normalize(last)
                    dot = sum(a*b for a,b in zip(first, last))
                    if dot < 0:
                        last, dot = tuple(-v for v in last), -dot
                    if dot < 0.9995:
                        angle = math.acos(min(1., dot))
                        a = math.sin((1-fraction)*angle)/math.sin(angle)
                        b = math.sin(fraction*angle)/math.sin(angle)
                        value = normalize(tuple(a*x+b*y for x,y in zip(first,last)))
                    else:
                        value = normalize(tuple(a+(b-a)*fraction for a,b in zip(first,last)))
                else:
                    value = tuple(a+(b-a)*fraction for a,b in zip(first,last))
                nodes[node][path] = value
        return globals_for(nodes, self.doc["scenes"][0]["nodes"])

    def vertices(self, pose):
        matrices = [multiply(pose[joint], bind) for joint, bind in zip(self.joints, self.binds)]
        result = []
        for attrs, _, _ in self.primitives:
            vertices = []
            for p, joints, weights in zip(attrs["POSITION"], attrs["JOINTS_0"], attrs["WEIGHTS_0"]):
                x = y = z = 0.
                for joint, weight in zip(joints, weights):
                    if weight:
                        q = transform(matrices[joint], p)
                        x += q[0]*weight
                        y += q[1]*weight
                        z += q[2]*weight
                vertices.append((x, y, z))
            result.append(vertices)
        return result


def prepare(source_doc, source_binary):
    doc = {key: copy.deepcopy(source_doc[key]) for key in ("scene", "scenes", "nodes", "meshes", "skins")}
    doc["asset"] = {"version": "2.0", "generator": "near-laugh character preparation 1"}
    doc["accessors"], doc["bufferViews"] = [], []
    binary = bytearray()
    remap = {}

    def add_accessor(old, replacement=None):
        if old in remap and replacement is None:
            return remap[old]
        original = source_doc["accessors"][old]
        values = accessor(source_doc, source_binary, old) if replacement is None else replacement
        fmt = "<" + FORMATS[original["componentType"]] * WIDTHS[original["type"]]
        raw = b"".join(struct.pack(fmt, *value) for value in values)
        binary.extend(b"\0" * (-len(binary) % 4))
        view = {"buffer": 0, "byteOffset": len(binary), "byteLength": len(raw)}
        acc = {"bufferView": len(doc["bufferViews"]), "componentType": original["componentType"],
               "count": len(values), "type": original["type"]}
        if "min" in original:
            acc["min"] = [min(value[k] for value in values) for k in range(len(values[0]))]
            acc["max"] = [max(value[k] for value in values) for k in range(len(values[0]))]
        binary.extend(raw)
        doc["bufferViews"].append(view)
        index = len(doc["accessors"])
        doc["accessors"].append(acc)
        remap[old] = index
        return index

    maximum_scale_drift = 0.
    for node in doc["nodes"]:
        drift = max(abs(v-1) for v in node.get("scale", (1,1,1)))
        maximum_scale_drift = max(maximum_scale_drift, drift)
        if drift > 0.00001:
            raise ValueError(f"{node['name']}: meaningful rest scale")
        node.pop("scale", None)
        if "rotation" in node:
            node["rotation"] = normalize(node["rotation"])
    for primitive in doc["meshes"][0]["primitives"]:
        primitive["attributes"].pop("TEXCOORD_1")
        primitive["attributes"] = {key: add_accessor(value) for key,value in primitive["attributes"].items()}
        primitive["indices"] = add_accessor(primitive["indices"])
    rest = globals_for(doc["nodes"], doc["scenes"][0]["nodes"])
    binds = [inverse(rest[joint]) for joint in doc["skins"][0]["joints"]]
    doc["skins"][0]["inverseBindMatrices"] = add_accessor(source_doc["skins"][0]["inverseBindMatrices"],
        [tuple(value[r*4+c] for c in range(4) for r in range(4)) for value in binds])
    doc["animations"] = []
    for original_name, name in CLIPS.items():
        original = next(clip for clip in source_doc["animations"] if clip["name"] == original_name)
        clip = {"name": name, "samplers": [], "channels": []}
        for channel in original["channels"]:
            sampler = original["samplers"][channel["sampler"]]
            path = channel["target"]["path"]
            values = accessor(source_doc, source_binary, sampler["output"])
            if path == "scale":
                drift = max(abs(v-1) for value in values for v in value)
                maximum_scale_drift = max(maximum_scale_drift, drift)
                if drift > 0.00001:
                    raise ValueError(f"{name}: meaningful animated scale")
                continue
            if path not in ("translation", "rotation") or sampler.get("interpolation", "LINEAR") != "LINEAR":
                raise ValueError(f"{name}: unexpected channel")
            replacement = [normalize(value) for value in values] if path == "rotation" else None
            clip["channels"].append({"sampler": len(clip["samplers"]), "target": copy.deepcopy(channel["target"])})
            clip["samplers"].append({"input": add_accessor(sampler["input"]),
                                     "output": add_accessor(sampler["output"], replacement), "interpolation": "LINEAR"})
        doc["animations"].append(clip)
    doc["materials"] = [{"name": material["name"], "doubleSided": True, "alphaMode": "OPAQUE",
                         "pbrMetallicRoughness": {"baseColorFactor": material["pbrMetallicRoughness"]["baseColorFactor"],
                                                  "metallicFactor": 0, "roughnessFactor": 1}}
                        for material in source_doc["materials"]]
    doc["buffers"] = [{"byteLength": len(binary)}]
    encoded = json.dumps(doc, separators=(",", ":"), allow_nan=False).encode("utf-8")
    encoded += b" " * (-len(encoded) % 4)
    binary.extend(b"\0" * (-len(binary) % 4))
    raw = (struct.pack("<III", 0x46546C67, 2, 28+len(encoded)+len(binary)) +
           struct.pack("<II", len(encoded), 0x4E4F534A) + encoded +
           struct.pack("<II", len(binary), 0x004E4942) + binary)
    return raw, maximum_scale_drift


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8", newline="\n")


def verify_prepared(doc, binary):
    if set(doc) != {"asset","scene","scenes","nodes","meshes","skins","accessors","bufferViews","animations","materials","buffers"}:
        raise ValueError("Prepared asset retains an excluded top-level feature")
    if len(doc["nodes"]) > 80 or len(doc["skins"]) != 1 or len(doc["skins"][0]["joints"]) > 65:
        raise ValueError("Prepared skeleton exceeds profile")
    if len(doc["meshes"]) != 1 or len(doc["scenes"]) != 1 or doc["scene"] != 0:
        raise ValueError("Prepared scene exceeds profile")
    if len(doc["buffers"]) != 1 or set(doc["buffers"][0]) != {"byteLength"}:
        raise ValueError("Prepared binary must be embedded")
    expected_attributes = {"POSITION","NORMAL","TEXCOORD_0","JOINTS_0","WEIGHTS_0"}
    vertex_count = index_count = 0
    for primitive in doc["meshes"][0]["primitives"]:
        if set(primitive["attributes"]) != expected_attributes or set(primitive) != {"attributes","indices","material"}:
            raise ValueError("Prepared primitive retains excluded attributes/features")
        vertex_count += doc["accessors"][primitive["attributes"]["POSITION"]]["count"]
        index_count += doc["accessors"][primitive["indices"]]["count"]
    if vertex_count > 10000 or index_count > 50000:
        raise ValueError("Prepared geometry exceeds profile")
    if [clip["name"] for clip in doc["animations"]] != list(CLIPS.values()):
        raise ValueError("Prepared clips differ from catalog")
    for clip in doc["animations"]:
        seen = set()
        for channel in clip["channels"]:
            key = channel["target"]["node"], channel["target"]["path"]
            if key in seen or key[1] not in ("translation","rotation"):
                raise ValueError("Prepared animation retains duplicate/unsupported channel")
            seen.add(key)
        for sampler in clip["samplers"]:
            times = [x[0] for x in accessor(doc,binary,sampler["input"])]
            if (sampler["interpolation"] != "LINEAR" or len(times) > 256 or times[0] < 0
                    or not 0 < times[-1] <= 10 or any(b <= a for a,b in zip(times,times[1:]))):
                raise ValueError("Prepared keys exceed profile")
    for material in doc["materials"]:
        if set(material) != {"name","doubleSided","alphaMode","pbrMetallicRoughness"}:
            raise ValueError("Prepared material retains an excluded feature")
        pbr = material["pbrMetallicRoughness"]
        if (set(pbr) != {"baseColorFactor","metallicFactor","roughnessFactor"}
                or pbr["metallicFactor"] != 0 or pbr["roughnessFactor"] != 1
                or not material["doubleSided"] or material["alphaMode"] != "OPAQUE"):
            raise ValueError("Prepared material exceeds diffuse profile")


def contact_sheet(model, samples, target):
    """Independent orthographic mesh reference, no graphics/runtime dependencies."""
    cell_width, cell_height = 320, 340
    width, height = cell_width * len(samples), cell_height * 2
    pixels = bytearray((238, 239, 242) * width * height)
    depth = [-math.inf] * (width * height)
    for column, (_, vertices) in enumerate(samples):
        for row in range(2):
            # Front looks down -Z; side looks down +X. Both retain +Y up.
            def project(p):
                u, d = (p[0], p[2]) if row == 0 else (p[2], -p[0])
                return (column*cell_width + cell_width/2 + u*150,
                        row*cell_height + cell_height-35 - p[1]*150, d)

            floor_y = row*cell_height + cell_height-35
            for x in range(column*cell_width, (column+1)*cell_width):
                p = (floor_y*width+x)*3
                pixels[p:p+3] = bytes((120, 125, 132))
            for primitive, points in zip(model.primitives, vertices):
                _, indices, material = primitive
                factor = model.doc["materials"][material]["pbrMetallicRoughness"]["baseColorFactor"]
                projected = [project(p) for p in points]
                for i in range(0, len(indices), 3):
                    ia, ib, ic = indices[i:i+3]
                    a, b, c = projected[ia], projected[ib], projected[ic]
                    area = (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
                    if abs(area) < 1e-9:
                        continue
                    low_x = max(column*cell_width, math.floor(min(a[0],b[0],c[0])))
                    high_x = min((column+1)*cell_width-1, math.ceil(max(a[0],b[0],c[0])))
                    low_y = max(row*cell_height, math.floor(min(a[1],b[1],c[1])))
                    high_y = min((row+1)*cell_height-1, math.ceil(max(a[1],b[1],c[1])))
                    v = tuple(points[ib][k]-points[ia][k] for k in range(3))
                    w = tuple(points[ic][k]-points[ia][k] for k in range(3))
                    normal = (v[1]*w[2]-v[2]*w[1],v[2]*w[0]-v[0]*w[2],v[0]*w[1]-v[1]*w[0])
                    length = math.sqrt(sum(n*n for n in normal))
                    light = 0.5 + 0.5*abs(sum(n*l for n,l in zip(normal, (.267,.802,.535))))/max(length,1e-10)
                    rgb = bytes(round(255 * min(1., max(0., value*light))**(1/2.2)) for value in factor[:3])
                    for y in range(low_y, high_y+1):
                        for x in range(low_x, high_x+1):
                            px, py = x+.5, y+.5
                            u = ((b[0]-px)*(c[1]-py)-(b[1]-py)*(c[0]-px))/area
                            v = ((c[0]-px)*(a[1]-py)-(c[1]-py)*(a[0]-px))/area
                            w = 1-u-v
                            if min(u,v,w) < -1e-8:
                                continue
                            distance = u*a[2]+v*b[2]+w*c[2]
                            offset = y*width+x
                            if distance > depth[offset]:
                                depth[offset] = distance
                                pixels[offset*3:offset*3+3] = rgb
    raw = b"".join(b"\0"+pixels[y*width*3:(y+1)*width*3] for y in range(height))

    def chunk(kind, payload):
        return struct.pack(">I", len(payload))+kind+payload+struct.pack(">I", zlib.crc32(kind+payload))

    target.write_bytes(b"\x89PNG\r\n\x1a\n"+
                       chunk(b"IHDR",struct.pack(">IIBBBBB",width,height,8,2,0,0,0))+
                       chunk(b"IDAT",zlib.compress(raw,9))+chunk(b"IEND",b""))


def calibrate(model, output):
    evidence = output / "evidence"
    evidence.mkdir(exist_ok=True)
    duration = {name: max(times[-1] for _,_,times,_ in channels) for name,channels in model.clips.items()}
    traced = ("foot_l", "ball_l", "foot_r", "ball_r", "hand_l", "hand_r")
    trajectories = {}
    with (evidence / "joint-samples.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["clip","time_s","phase"]+[f"{joint}_{axis}_m" for joint in traced for axis in "xyz"])
        for clip in ("walk", "interact"):
            count = round(duration[clip]*240)
            samples = []
            for i in range(count+1):
                time = duration[clip]*i/count
                pose = model.pose(clip,time)
                points = {name: transform(pose[model.names[name]], (0,0,0)) for name in traced}
                samples.append((time,points))
                writer.writerow([clip,format(time,".9f"),format(i/count,".9f")]+
                                [format(v,".9f") for joint in traced for v in points[joint]])
            trajectories[clip] = samples
    # Heel strike: maximal forward ankle extension; the heel is down and toes
    # remain raised. Half-open cycle avoids duplicate endpoint contact.
    contacts = sorted((max(range(len(trajectories["walk"])-1),
                           key=lambda i: trajectories["walk"][i][1][joint][2]), joint)
                      for joint in ("foot_l", "foot_r"))
    phases = [trajectories["walk"][i][0]/duration["walk"] for i,_ in contacts]
    # Fit the backward stance motion of each toe while planted. These windows
    # are the observed flat toe portion after heel strike and before toe lift.
    slopes = []
    for joint,start,end in (("ball_l",.125,.5),("ball_r",.625,1.)):
        observations = [(time,p[joint][2]) for time,p in trajectories["walk"]
                        if start-1e-9 <= time/duration["walk"] <= end+1e-9]
        mean_t = sum(t for t,_ in observations)/len(observations)
        mean_z = sum(z for _,z in observations)/len(observations)
        slope = sum((t-mean_t)*(z-mean_z) for t,z in observations)/sum((t-mean_t)**2 for t,_ in observations)
        residual = max(abs(z-(mean_z+slope*(t-mean_t))) for t,z in observations)
        slopes.append({"joint": joint,"phase_window": [start,end],"backward_speed_m_s": -slope,
                       "maximum_fit_residual_m": residual})
    cycle_distance = sum(s["backward_speed_m_s"] for s in slopes)/len(slopes)*duration["walk"]
    interaction_sample = max(range(len(trajectories["interact"])),
                             key=lambda i: trajectories["interact"][i][1]["hand_l"][2])
    interaction_time, interaction_points = trajectories["interact"][interaction_sample]
    minimum_spacing = min(phases[1]-phases[0],1+phases[0]-phases[1])*cycle_distance/1.5
    if minimum_spacing < .2:
        raise ValueError("Calibrated contact spacing fails P07b at 1.5 m/s")
    bounds_min, bounds_max = [math.inf]*3, [-math.inf]*3
    samples = []
    for clip in (None,"idle","walk","interact"):
        count = 0 if clip is None else math.ceil(duration[clip]*60)
        for i in range(count+1):
            time = 0 if count == 0 else duration[clip]*i/count
            vertices = model.vertices(model.pose(clip,time))
            for primitive in vertices:
                for point in primitive:
                    for axis in range(3):
                        bounds_min[axis] = min(bounds_min[axis],point[axis])
                        bounds_max[axis] = max(bounds_max[axis],point[axis])
            samples.append((clip,time))
    preview_min = [round(math.floor((v-.05)/.05)*.05, 6) for v in bounds_min]
    preview_max = [round(math.ceil((v+.05)/.05)*.05, 6) for v in bounds_max]
    rest = model.pose()
    ankle, toe = (transform(rest[model.names[name]],(0,0,0)) for name in ("foot_l","ball_l"))
    forward = tuple(toe[k]-ankle[k] for k in range(3))
    capsule_height = math.ceil((bounds_max[1]+.02)/.05)*.05
    # Torso/pelvis proxy intentionally excludes moving arms. Radius is rounded
    # up from the bind chest envelope; retain that envelope as review evidence.
    bind_points = [p for primitive in model.vertices(rest) for p in primitive]
    torso = [p for p in bind_points if .8 <= p[1] <= 1.3]
    torso_radius = max(math.hypot(p[0],p[2]) for p in torso)
    capsule_radius = math.ceil((torso_radius+.02)/.05)*.05
    catalog = {"version": 1,"models": [{"id": "test-mannequin", "resource": "characters/test-mannequin.glb",
               "feet_origin": [0,0,0],"forward_axis": [0,0,1],
               "capsule": {"radius_m": capsule_radius,"height_m": capsule_height},
               "preview_bounds": {"min": preview_min,"max": preview_max},
               "clips": {"idle": {"name": "idle","duration_s": duration["idle"],"loop": True},
                         "walk": {"name": "walk","duration_s": duration["walk"],"loop": True},
                         "interact": {"name": "interact","duration_s": duration["interact"],"loop": False}},
               "walk_cycle_distance_m": cycle_distance,"walk_contact_phases": phases,
               "interaction_phase": interaction_time/duration["interact"],
               "supported_speed_m_s": {"min": .25,"max": 1.5}}]}
    measurements = {"version": 1,"units": "metres and seconds", "joint_sample_rate_hz": 240,
                    "mesh_sample_rate_hz": 60,"mesh_pose_count_including_bind": len(samples),
                    "sampled_mesh_bounds": {"min": bounds_min,"max": bounds_max},
                    "preview_margin_m": .05,"preview_rounding_m": .05,
                    "rest_left_ankle_to_toe": forward,"rest_minimum_sole_y_m": min(p[1] for p in bind_points),
                    "rest_torso_radius_y_0_8_to_1_3_m": torso_radius,
                    "capsule_margin_m": .02,"capsule_rounding_m": .05,
                    "contacts": [{"joint": joint,"phase": phase,
                                  "position": trajectories["walk"][i][1][joint]}
                                 for (i,joint),phase in zip(contacts,phases)],
                    "stance_fits": slopes,"walk_cycle_distance_m": cycle_distance,
                    "native_walk_speed_m_s": cycle_distance/duration["walk"],
                    "minimum_contact_spacing_at_1_5_m_s": minimum_spacing,
                    "p07b_minimum_required_spacing_s": .2,
                    "interaction": {"joint": "hand_l","time_s": interaction_time,
                                     "phase": interaction_time/duration["interact"],
                                     "position": interaction_points["hand_l"],"rule": "maximum forward reach"}}
    validate_catalog(catalog)
    write_json(output / "catalog.json",catalog)
    write_json(evidence / "calibration.json",measurements)
    captures = [("bind",None,0),("idle-0","idle",0),("walk-left-contact","walk",phases[0]*duration["walk"]),
                ("walk-midstep","walk",duration["walk"]*.25),
                ("walk-right-contact","walk",phases[1]*duration["walk"]),
                ("interact-reach","interact",interaction_time)]
    contact_sheet(model,[(name,model.vertices(model.pose(clip,time))) for name,clip,time in captures],
                  evidence / "pose-reference.png")
    write_json(evidence / "pose-reference.json", {"views": ["front (+Z camera)","side (-X camera)"],
               "columns": [{"name": name,"clip": clip,"time_s": time} for name,clip,time in captures],
               "pixels_per_metre": 150,"rendering": "independent Python CPU skinning, orthographic flat triangle lighting"})
    return catalog, measurements


def validate_catalog(catalog):
    entry = catalog["models"][0]
    for key in ("feet_origin", "forward_axis"):
        if len(entry[key]) != 3 or not all(math.isfinite(v) for v in entry[key]):
            raise ValueError(f"Catalog {key}: expected finite three-vector")
    if abs(sum(v*v for v in entry["forward_axis"])-1) > 1e-5:
        raise ValueError("Catalog forward_axis: expected unit vector")
    for low,high,feet in zip(entry["preview_bounds"]["min"],entry["preview_bounds"]["max"],entry["feet_origin"]):
        if not math.isfinite(low) or not math.isfinite(high) or not low <= feet <= high or low >= high:
            raise ValueError("Catalog preview bounds: invalid finite envelope")
    radius,height = entry["capsule"]["radius_m"],entry["capsule"]["height_m"]
    distance = entry["walk_cycle_distance_m"]
    low_speed,high_speed = entry["supported_speed_m_s"]["min"],entry["supported_speed_m_s"]["max"]
    if not all(math.isfinite(v) and v > 0 for v in (radius,height,distance,low_speed,high_speed)):
        raise ValueError("Catalog capsule/distance/speed: expected finite positive values")
    if height < 2*radius or not .25 <= low_speed <= high_speed <= 1.5:
        raise ValueError("Catalog capsule/speed: unsupported limits")
    first,second = entry["walk_contact_phases"]
    if not 0 <= first < second < 1 or not 0 < entry["interaction_phase"] < 1:
        raise ValueError("Catalog contact/interaction phases: invalid interval")
    if min(second-first,1+first-second)*distance/high_speed < .2:
        raise ValueError("Catalog contacts: spacing below 0.20 s at maximum speed")


def catalog_header(catalog):
    entry = catalog["models"][0]

    def floats(values):
        return "{"+", ".join(repr(float(value))+"F" for value in values)+"}"

    return f'''// Generated by scripts/prepare_character_animation.py --catalog-header.
// Calibration and provenance: resources/characters/catalog.json and evidence/.
#ifndef CORE_ANIMATION_CHARACTER_CATALOG_HPP
#define CORE_ANIMATION_CHARACTER_CATALOG_HPP

#include <array>
#include <cstddef>
#include <limits>
#include <string_view>

struct CharacterCatalogEntry {{
  std::string_view id;
  std::string_view resource;
  std::array<float, 3> feet_origin;
  std::array<float, 3> forward_axis;
  float capsule_radius_m;
  float capsule_height_m;
  std::array<float, 3> preview_min;
  std::array<float, 3> preview_max;
  double walk_cycle_distance_m;
  std::array<double, 2> walk_contact_phases;
  double interaction_phase;
  double minimum_speed_m_s;
  double maximum_speed_m_s;
}};

[[nodiscard]] constexpr bool characterCatalogIsValid(const CharacterCatalogEntry& entry) {{
  const auto finite = [](double value) {{
    return value >= -std::numeric_limits<double>::max() && value <= std::numeric_limits<double>::max();
  }};
  double forward_length_squared = 0;
  for (std::size_t axis = 0; axis < 3; ++axis) {{
    if (!finite(entry.feet_origin[axis]) || !finite(entry.forward_axis[axis]) ||
        !finite(entry.preview_min[axis]) || !finite(entry.preview_max[axis]) ||
        entry.preview_min[axis] >= entry.preview_max[axis] ||
        entry.feet_origin[axis] < entry.preview_min[axis] || entry.feet_origin[axis] > entry.preview_max[axis]) return false;
    forward_length_squared += static_cast<double>(entry.forward_axis[axis]) * entry.forward_axis[axis];
  }}
  const double first = entry.walk_contact_phases[0], second = entry.walk_contact_phases[1];
  const double spacing = (second-first < 1+first-second ? second-first : 1+first-second);
  return !entry.id.empty() && !entry.resource.empty() &&
         forward_length_squared >= 0.99999 && forward_length_squared <= 1.00001 &&
         finite(entry.capsule_radius_m) && entry.capsule_radius_m > 0 &&
         finite(entry.capsule_height_m) && entry.capsule_height_m >= 2*entry.capsule_radius_m &&
         finite(entry.walk_cycle_distance_m) && entry.walk_cycle_distance_m > 0 &&
         finite(entry.minimum_speed_m_s) && finite(entry.maximum_speed_m_s) &&
         entry.minimum_speed_m_s >= 0.25 && entry.minimum_speed_m_s <= entry.maximum_speed_m_s &&
         entry.maximum_speed_m_s <= 1.5 && first >= 0 && first < second && second < 1 &&
         entry.interaction_phase > 0 && entry.interaction_phase < 1 &&
         spacing*entry.walk_cycle_distance_m/entry.maximum_speed_m_s >= 0.20;
}}

inline constexpr CharacterCatalogEntry test_mannequin_catalog{{
    "{entry['id']}", "{entry['resource']}",
    {floats(entry['feet_origin'])}, {floats(entry['forward_axis'])},
    {entry['capsule']['radius_m']}F, {entry['capsule']['height_m']}F,
    {floats(entry['preview_bounds']['min'])}, {floats(entry['preview_bounds']['max'])},
    {entry['walk_cycle_distance_m']},
    {{{entry['walk_contact_phases'][0]}, {entry['walk_contact_phases'][1]}}},
    {entry['interaction_phase']},
    {entry['supported_speed_m_s']['min']}, {entry['supported_speed_m_s']['max']}}};

static_assert(characterCatalogIsValid(test_mannequin_catalog));

#endif
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="Explicit root holding the pinned acquisition manifest/downloads/source")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "resources/characters")
    parser.add_argument("--catalog-header", type=Path, help="Explicit optional generated C++ catalog header output")
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    if output == source or source in output.parents or output in source.parents:
        parser.error("Output must be separate from the read-only source tree")
    if args.catalog_header and (args.catalog_header.resolve() == source or source in args.catalog_header.resolve().parents):
        parser.error("Catalog header must be outside the read-only source tree")
    raw = (source / SOURCE_GLB).read_bytes()
    archive = (source / SOURCE_ZIP).read_bytes()
    license_raw = (source / SOURCE_LICENSE).read_bytes()
    if sha(raw) != SOURCE_SHA or sha(archive) != ARCHIVE_SHA:
        raise ValueError("Source GLB or archive differs from pinned SHA-256")
    with zipfile.ZipFile(source / SOURCE_ZIP) as package:
        if package.testzip() is not None:
            raise ValueError("Source archive CRC failed")
        name = next(name for name in package.namelist() if name.endswith("/Unreal-Godot/UAL1_Standard.glb"))
        if package.read(name) != raw:
            raise ValueError("Extracted GLB differs from original archive")
        license_name = next(name for name in package.namelist() if name.endswith("/License.txt"))
        if package.read(license_name) != license_raw:
            raise ValueError("Included license differs from original archive")
    source_doc, source_binary = read_glb(raw)
    derivative, scale_drift = prepare(source_doc, source_binary)
    repeated, repeated_drift = prepare(source_doc, source_binary)
    if derivative != repeated or scale_drift != repeated_drift:
        raise ValueError("Preparation is not deterministic")
    doc, binary = read_glb(derivative)
    verify_prepared(doc, binary)
    original_model, model = Model(source_doc, source_binary), Model(doc, binary)
    original_bind = original_model.vertices(original_model.pose())
    prepared_bind = model.vertices(model.pose())
    bind_error = max(math.dist(a,b) for pa,pb in zip(original_bind,prepared_bind) for a,b in zip(pa,pb))
    if bind_error > 0.0001:
        raise ValueError(f"Bind geometry drift {bind_error} exceeds 0.0001 m")
    output.mkdir(parents=True, exist_ok=True)
    (output / "test-mannequin.glb").write_bytes(derivative)
    (output / "LICENSE-Quaternius.txt").write_bytes(license_raw)
    catalog, calibration = calibrate(model, output)
    header = catalog_header(catalog)
    if args.catalog_header:
        args.catalog_header.parent.mkdir(parents=True, exist_ok=True)
        args.catalog_header.write_text(header, encoding="utf-8", newline="\n")
    manifest = {"version": 1,"author": "Quaternius","license": "CC0-1.0",
                "source": {"source_page": "https://quaternius.itch.io/universal-animation-library",
                           "edition": "Standard","itch_upload_id": "17958403",
                           "archive": SOURCE_ZIP,"archive_bytes": len(archive),"archive_sha256": ARCHIVE_SHA,
                           "archive_crc_verified": True,"extracted_glb_matches_archive": True,
                           "glb": SOURCE_GLB,"glb_bytes": len(raw),"glb_sha256": SOURCE_SHA,
                           "included_license_sha256": sha(license_raw)},
                "derivative": {"file": "test-mannequin.glb","bytes": len(derivative),"sha256": sha(derivative),
                               "catalog_sha256": sha((output / "catalog.json").read_bytes()),
                               "catalog_header_sha256": sha(header.encode("utf-8")),
                               "selected_clips": CLIPS,"joint_count": len(model.joints),
                               "node_count": len(doc["nodes"]),
                               "source_vertices": sum(len(p[0]["POSITION"]) for p in model.primitives),
                               "indices": sum(len(p[1]) for p in model.primitives)},
                "preparation": {"script": "scripts/prepare_character_animation.py","version": 1,
                                "dependencies": "Python standard library",
                                "maximum_unit_scale_drift": scale_drift,"scale_normalization_limit": .00001,
                                "bind_maximum_displacement_m": bind_error,"bind_displacement_limit_m": .0001,
                                "repeated_preparation_byte_identical": True,
                                "conversions": ["Keep only idle/walk/interact with stable logical names",
                                                "Remove secondary UV data and 40 unselected clips",
                                                "Normalize negligible rest and constant animation scales to one; remove scale channels",
                                                "Normalize rest/key quaternions; preserve hierarchy and root orientation",
                                                "Recompute inverse binds from normalized rest transforms",
                                                "Preserve both base colors; use opaque two-sided diffuse metallic=0 roughness=1",
                                                "Repack only used accessors into embedded binary"]}}
    write_json(output / "manifest.json",manifest)
    print(json.dumps({"derivative_sha256": sha(derivative), "bytes": len(derivative),
                      "maximum_scale_drift": scale_drift, "bind_max_error_m": bind_error,
                      "calibration": calibration}, indent=2))
    if (source / SOURCE_GLB).read_bytes() != raw or (source / SOURCE_ZIP).read_bytes() != archive or (source / SOURCE_LICENSE).read_bytes() != license_raw:
        raise ValueError("A source file changed during preparation")


if __name__ == "__main__":
    main()
