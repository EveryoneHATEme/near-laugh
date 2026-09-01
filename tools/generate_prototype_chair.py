"""Generate the project-owned bounded prototype chair GLB."""

from __future__ import annotations

import json
import math
import struct
from pathlib import Path


POSITIONS: list[tuple[float, float, float]] = []
NORMALS: list[tuple[float, float, float]] = []
UVS: list[tuple[float, float]] = []
INDICES: list[int] = []


def add_box(
    minimum: tuple[float, float, float], maximum: tuple[float, float, float]
) -> None:
    x0, y0, z0 = minimum
    x1, y1, z1 = maximum
    faces = (
        ((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (0, 0, 1)),
        ((x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (0, 0, -1)),
        ((x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (-1, 0, 0)),
        ((x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (1, 0, 0)),
        ((x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0), (0, 1, 0)),
        ((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1), (0, -1, 0)),
    )
    for first, second, third, fourth, normal in faces:
        base = len(POSITIONS)
        POSITIONS.extend((first, second, third, fourth))
        NORMALS.extend((normal,) * 4)
        UVS.extend(((0, 0), (1, 0), (1, 1), (0, 1)))
        INDICES.extend((base, base + 1, base + 2, base, base + 2, base + 3))


def aligned(data: bytearray) -> None:
    while len(data) % 4:
        data.append(0)


def append_values(data: bytearray, fmt: str, values: list[tuple] | list[int]) -> tuple[int, int]:
    aligned(data)
    offset = len(data)
    for value in values:
        if isinstance(value, tuple):
            data.extend(struct.pack(fmt, *value))
        else:
            data.extend(struct.pack(fmt, value))
    return offset, len(data) - offset


def main() -> None:
    # A single primitive assembled from a seat, back, and four separated legs.
    add_box((-0.55, 0.78, -0.48), (0.55, 0.98, 0.48))
    add_box((-0.55, 0.98, 0.31), (0.55, 1.82, 0.48))
    for x0, x1 in ((-0.50, -0.36), (0.36, 0.50)):
        for z0, z1 in ((-0.43, -0.29), (0.29, 0.43)):
            add_box((x0, 0.0, z0), (x1, 0.78, z1))

    binary = bytearray()
    position_view = append_values(binary, "<3f", POSITIONS)
    normal_view = append_values(binary, "<3f", NORMALS)
    uv_view = append_values(binary, "<2f", UVS)
    index_view = append_values(binary, "<H", INDICES)
    aligned(binary)

    minimum = [min(value[axis] for value in POSITIONS) for axis in range(3)]
    maximum = [max(value[axis] for value in POSITIONS) for axis in range(3)]
    views = (position_view, normal_view, uv_view, index_view)
    document = {
        "asset": {"version": "2.0", "generator": "near-laugh"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "PrototypeChair"}],
        "meshes": [{"primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
            "indices": 3,
            "mode": 4,
        }]}],
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": offset, "byteLength": length}
            for offset, length in views
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(POSITIONS),
             "type": "VEC3", "min": minimum, "max": maximum},
            {"bufferView": 1, "componentType": 5126, "count": len(NORMALS),
             "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": len(UVS),
             "type": "VEC2"},
            {"bufferView": 3, "componentType": 5123, "count": len(INDICES),
             "type": "SCALAR"},
        ],
    }
    json_bytes = json.dumps(document, separators=(",", ":")).encode("utf-8")
    json_bytes += b" " * (-len(json_bytes) % 4)
    total_length = 12 + 8 + len(json_bytes) + 8 + len(binary)
    glb = bytearray(struct.pack("<III", 0x46546C67, 2, total_length))
    glb.extend(struct.pack("<II", len(json_bytes), 0x4E4F534A))
    glb.extend(json_bytes)
    glb.extend(struct.pack("<II", len(binary), 0x004E4942))
    glb.extend(binary)

    destination = Path(__file__).resolve().parents[1] / "resources" / "models" / "prototype_chair.glb"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(glb)
    print(f"wrote {destination} ({len(glb)} bytes, {len(INDICES)} indices)")


if __name__ == "__main__":
    main()
