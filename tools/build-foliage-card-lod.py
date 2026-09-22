#!/usr/bin/env python3
"""Build a deterministic canopy-card AEMESH from foliage components."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import pathlib
import struct
import sys
import tempfile


MAGIC = b"AEM3"
VERSION = 1
HEADER = struct.Struct("<4sIII")
VERTEX = struct.Struct("<fffIIff")
POLICY = "component-centroid-voxel-triplanar-card-v3"
PLANES = ((0, 1), (0, 2), (1, 2))


class CardLodError(ValueError):
    """Raised when the diagnostic card mesh contract cannot be satisfied."""


def fail(message: str) -> None:
    raise CardLodError(message)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_aemesh(data: bytes) -> tuple[list[tuple[float, float, float, int, int, float, float]], list[int]]:
    if len(data) < HEADER.size:
        fail("malformed AEMESH: truncated header")
    magic, version, vertex_count, index_count = HEADER.unpack_from(data)
    if magic != MAGIC or version != VERSION:
        fail("malformed AEMESH: expected AEMESH-v1")
    expected = HEADER.size + vertex_count * VERTEX.size + index_count * 4
    if len(data) != expected:
        fail("malformed AEMESH: file length does not match header")
    if vertex_count == 0 or index_count == 0:
        fail("malformed AEMESH: mesh must contain vertices and triangles")
    if index_count % 3:
        fail("malformed AEMESH: index count is not divisible by three")

    vertices = []
    for ordinal in range(vertex_count):
        vertex = VERTEX.unpack_from(data, HEADER.size + ordinal * VERTEX.size)
        if not all(math.isfinite(value) for value in (*vertex[:3], *vertex[5:])):
            fail(f"malformed AEMESH: non-finite vertex value at {ordinal}")
        vertices.append(vertex)
    indices = list(struct.unpack_from(f"<{index_count}I", data, HEADER.size + vertex_count * VERTEX.size))
    for ordinal, index in enumerate(indices):
        if index >= vertex_count:
            fail(f"malformed AEMESH: index {ordinal} is out of range")
    return vertices, indices


def connected_components(indices: list[int]) -> list[list[int]]:
    triangle_count = len(indices) // 3
    parent = list(range(triangle_count))

    def find(item: int) -> int:
        while parent[item] != item:
            parent[item] = parent[parent[item]]
            item = parent[item]
        return item

    def union(left: int, right: int) -> None:
        left, right = find(left), find(right)
        if left == right:
            return
        if left > right:
            left, right = right, left
        parent[right] = left

    owners: dict[int, int] = {}
    for triangle in range(triangle_count):
        for index in indices[triangle * 3 : triangle * 3 + 3]:
            previous = owners.setdefault(index, triangle)
            union(triangle, previous)
    grouped: dict[int, list[int]] = {}
    for triangle in range(triangle_count):
        grouped.setdefault(find(triangle), []).append(triangle)
    return sorted(grouped.values(), key=lambda value: value[0])


def component_geometry(
    component: list[int], vertices: list[tuple[float, float, float, int, int, float, float]], indices: list[int]
) -> tuple[list[float], list[float], list[float]]:
    used = sorted({indices[triangle * 3 + corner] for triangle in component for corner in range(3)})
    points = [vertices[index][:3] for index in used]
    minimum = [min(point[axis] for point in points) for axis in range(3)]
    maximum = [max(point[axis] for point in points) for axis in range(3)]
    centroid = [sum(point[axis] for point in points) / len(points) for axis in range(3)]
    return centroid, minimum, maximum


def mesh_bounds(vertices: list[tuple[float, float, float, int, int, float, float]]) -> dict[str, list[float]]:
    return {
        "min": [min(vertex[axis] for vertex in vertices) for axis in range(3)],
        "max": [max(vertex[axis] for vertex in vertices) for axis in range(3)],
    }


def voxel_coordinate(value: float, minimum: float, maximum: float, cells: int) -> int:
    if maximum == minimum:
        return 0
    coordinate = int(math.floor((value - minimum) * cells / (maximum - minimum)))
    return max(0, min(cells - 1, coordinate))


def cluster_components(
    vertices: list[tuple[float, float, float, int, int, float, float]],
    indices: list[int],
    components: list[list[int]],
    grid: tuple[int, int, int],
    bounds: dict[str, list[float]],
) -> list[dict[str, object]]:
    clustered: dict[tuple[int, int, int], dict[str, object]] = {}
    for ordinal, component in enumerate(components):
        centroid, minimum, maximum = component_geometry(component, vertices, indices)
        cell = tuple(
            voxel_coordinate(centroid[axis], bounds["min"][axis], bounds["max"][axis], grid[axis])
            for axis in range(3)
        )
        cluster = clustered.setdefault(
            cell,
            {"cell": list(cell), "components": [], "centroidSum": [0.0, 0.0, 0.0], "min": minimum[:], "max": maximum[:]},
        )
        cluster["components"].append(ordinal)
        for axis in range(3):
            cluster["centroidSum"][axis] += centroid[axis]
            cluster["min"][axis] = min(cluster["min"][axis], minimum[axis])
            cluster["max"][axis] = max(cluster["max"][axis], maximum[axis])
    result = []
    for cell in sorted(clustered):
        cluster = clustered[cell]
        count = len(cluster["components"])
        cluster["centroid"] = [value / count for value in cluster.pop("centroidSum")]
        result.append(cluster)
    return result


def pack_normal(normal: tuple[float, float, float]) -> int:
    def channel(value: float) -> int:
        value = max(-1.0, min(1.0, value))
        rounded = int(value * 127.0 + (0.5 if value >= 0.0 else -0.5))
        return rounded & 0xFF

    return channel(normal[0]) | (channel(normal[1]) << 8) | (channel(normal[2]) << 16) | (127 << 24)


def expanded_cluster_bounds(
    cluster: dict[str, object], source_bounds: dict[str, list[float]], grid: tuple[int, int, int], card_scale: float
) -> tuple[list[float], list[float]]:
    minimum, maximum, center = cluster["min"][:], cluster["max"][:], cluster["centroid"]
    for axis in range(3):
        source_span = source_bounds["max"][axis] - source_bounds["min"][axis]
        padding_span = source_span / grid[axis] * 0.05
        half = max((maximum[axis] - minimum[axis]) * 0.5, padding_span) * card_scale
        minimum[axis] = max(source_bounds["min"][axis], min(minimum[axis], center[axis] - half))
        maximum[axis] = min(source_bounds["max"][axis], max(maximum[axis], center[axis] + half))
    return minimum, maximum


def append_card(
    output_vertices: list[tuple[float, float, float, int, int, float, float]],
    output_indices: list[int],
    plane: tuple[int, int],
    minimum: list[float],
    maximum: list[float],
    center: list[float],
    uv_range: dict[str, list[float]],
    uv_variant: int,
) -> None:
    first, second = plane
    third = 3 - first - second
    if maximum[first] <= minimum[first] or maximum[second] <= minimum[second]:
        fail("cannot generate a non-zero-area card within source bounds")
    normal_vector = [0.0, 0.0, 0.0]
    normal_vector[third] = 1.0
    normal = pack_normal(tuple(normal_vector))
    corners = ((minimum[first], minimum[second]), (maximum[first], minimum[second]),
               (maximum[first], maximum[second]), (minimum[first], maximum[second]))
    u0, v0 = uv_range["min"]
    u1, v1 = uv_range["max"]
    normalized_uvs = ((0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0))
    mirror_u = bool(uv_variant & 1)
    mirror_v = bool(uv_variant & 2)
    uvs = tuple((
        u0 + (u1 - u0) * (1.0 - u if mirror_u else u),
        v0 + (v1 - v0) * (1.0 - v if mirror_v else v),
    ) for u, v in normalized_uvs)
    base = len(output_vertices)
    for corner, uv in zip(corners, uvs):
        position = center[:]
        position[first], position[second], position[third] = corner[0], corner[1], center[third]
        output_vertices.append((*position, normal, 0xFFFFFFFF, *uv))
    output_indices.extend((base, base + 1, base + 2, base, base + 2, base + 3))


def encode(vertices: list[tuple[float, float, float, int, int, float, float]], indices: list[int]) -> bytes:
    data = bytearray(HEADER.pack(MAGIC, VERSION, len(vertices), len(indices)))
    for vertex in vertices:
        data.extend(VERTEX.pack(*vertex))
    data.extend(struct.pack(f"<{len(indices)}I", *indices))
    return bytes(data)


def triangle_area_squared(a: tuple[float, ...], b: tuple[float, ...], c: tuple[float, ...]) -> float:
    ab = [b[axis] - a[axis] for axis in range(3)]
    ac = [c[axis] - a[axis] for axis in range(3)]
    cross = (ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2], ab[0] * ac[1] - ab[1] * ac[0])
    return sum(value * value for value in cross) * 0.25


def atomic_write(path: pathlib.Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary_name, path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def build(
    source: pathlib.Path,
    output: pathlib.Path,
    receipt_path: pathlib.Path,
    grid: tuple[int, int, int],
    card_scale: float,
    seed: str,
    lod1_triangles: int,
    diagnostic_only: bool = True,
) -> dict[str, object]:
    resolved = (source.resolve(), output.resolve(), receipt_path.resolve())
    if len(set(resolved)) != 3:
        fail("source, output, and receipt paths must be distinct")
    if any(isinstance(value, bool) or value < 1 or value > 1024 for value in grid):
        fail("grid dimensions must be integers from 1 through 1024")
    if not math.isfinite(card_scale) or card_scale < 1.0 or card_scale > 16.0:
        fail("card scale must be finite and from 1 through 16")
    if isinstance(lod1_triangles, bool) or lod1_triangles < 1:
        fail("LOD1 triangle count must be positive")
    try:
        source_data = source.read_bytes()
    except OSError as error:
        fail(f"unable to read source: {error}")
    vertices, indices = parse_aemesh(source_data)
    bounds = mesh_bounds(vertices)
    nonzero_axes = [axis for axis in range(3) if bounds["max"][axis] > bounds["min"][axis]]
    if len(nonzero_axes) < 2:
        fail("source bounds cannot support a non-zero-area card")
    uv_range = {
        "min": [min(vertex[5] for vertex in vertices), min(vertex[6] for vertex in vertices)],
        "max": [max(vertex[5] for vertex in vertices), max(vertex[6] for vertex in vertices)],
    }
    if uv_range["max"][0] <= uv_range["min"][0] or uv_range["max"][1] <= uv_range["min"][1]:
        fail("source UV range must have non-zero area")
    components = connected_components(indices)
    clusters = cluster_components(vertices, indices, components, grid, bounds)
    if not clusters:
        fail("source has no connected components")

    cards_per_voxel = 3 if len(nonzero_axes) == 3 else 1
    output_triangles = len(clusters) * cards_per_voxel * 2
    if output_triangles >= lod1_triangles:
        fail("card mesh triangle count is not strictly smaller than LOD1")
    output_vertices: list[tuple[float, float, float, int, int, float, float]] = []
    output_indices: list[int] = []
    occupied_receipt = []
    for cluster in clusters:
        minimum, maximum = expanded_cluster_bounds(cluster, bounds, grid, card_scale)
        candidates = [plane for plane in PLANES if plane[0] in nonzero_axes and plane[1] in nonzero_axes]
        digest = hashlib.sha256((POLICY + "\0" + seed + "\0" + ",".join(map(str, cluster["cell"]))).encode("utf-8")).digest()
        if cards_per_voxel == 3:
            selected_planes = candidates
        else:
            selected_planes = [candidates[digest[0] % len(candidates)]]
        uv_variants = []
        for ordinal, plane in enumerate(selected_planes):
            uv_variant = digest[(ordinal + 1) % len(digest)] & 3
            uv_variants.append(uv_variant)
            append_card(output_vertices, output_indices, plane, minimum, maximum, cluster["centroid"], uv_range, uv_variant)
        occupied_receipt.append({
            "cell": cluster["cell"],
            "components": len(cluster["components"]),
            "componentBounds": {"min": cluster["min"], "max": cluster["max"]},
            "cardBounds": {"min": minimum, "max": maximum},
            "planes": ["xyz"[plane[0]] + "xyz"[plane[1]] for plane in selected_planes],
            "uvVariants": uv_variants,
        })

    output_data = encode(output_vertices, output_indices)
    reparsed_vertices, reparsed_indices = parse_aemesh(output_data)
    for triangle in range(len(reparsed_indices) // 3):
        a, b, c = (reparsed_vertices[reparsed_indices[triangle * 3 + corner]] for corner in range(3))
        if not triangle_area_squared(a, b, c) > 0.0:
            fail("internal validation produced a zero-area triangle")
    output_bounds = mesh_bounds(reparsed_vertices)
    if output_bounds != bounds:
        fail("internal validation failed to preserve source bounds")
    if len(reparsed_indices) // 3 >= lod1_triangles:
        fail("internal validation exceeded the LOD1 triangle gate")

    policy_payload = {
        "policy": POLICY,
        "grid": list(grid),
        "cardScale": card_scale,
        "seed": seed,
        "lod1Triangles": lod1_triangles,
    }
    policy_hash = sha256(json.dumps(policy_payload, sort_keys=True, separators=(",", ":")).encode("utf-8"))
    receipt = {
        "schemaVersion": 1,
        "tool": "build-foliage-card-lod.py",
        "format": "AEMESH-v1",
        "diagnosticOnly": diagnostic_only,
        "sourcePath": str(source),
        "outputPath": str(output),
        "sourceSha256": sha256(source_data),
        "outputSha256": sha256(output_data),
        "policy": POLICY,
        "policyHash": policy_hash,
        "seed": seed,
        "grid": list(grid),
        "cardScale": card_scale,
        "sourceTriangles": len(indices) // 3,
        "lod1Triangles": lod1_triangles,
        "triangles": len(output_indices) // 3,
        "cards": len(output_indices) // 6,
        "sourceComponents": len(components),
        "occupiedVoxelCount": len(clusters),
        "occupiedVoxels": occupied_receipt,
        "sourceBounds": bounds,
        "bounds": output_bounds,
        "sourceUvRange": uv_range,
    }
    atomic_write(output, output_data)
    atomic_write(receipt_path, (json.dumps(receipt, indent=2, sort_keys=True) + "\n").encode("utf-8"))
    return receipt


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=pathlib.Path, required=True, help="LOD0 leaf AEMESH-v1 input")
    parser.add_argument("--output", type=pathlib.Path, required=True, help="card AEMESH-v1 output")
    parser.add_argument("--receipt", type=pathlib.Path, required=True, help="JSON receipt output")
    parser.add_argument("--grid", type=int, nargs=3, metavar=("X", "Y", "Z"), default=(8, 8, 8))
    parser.add_argument("--card-scale", type=float, default=1.0, help="bounded cluster-card expansion, 1 through 16")
    parser.add_argument("--seed", default="aevrynd-canopy-card-diagnostic-v1")
    parser.add_argument("--lod1-triangles", type=int, required=True, help="exclusive output triangle ceiling")
    parser.add_argument(
        "--production",
        action="store_true",
        help="mark the receipt as a production asset after the caller has completed production gates",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    arguments = parse_args(argv)
    try:
        receipt = build(
            arguments.source,
            arguments.output,
            arguments.receipt,
            tuple(arguments.grid),
            arguments.card_scale,
            arguments.seed,
            arguments.lod1_triangles,
            not arguments.production,
        )
        print("FOLIAGE_CARD_LOD_RECEIPT=" + json.dumps({
            "cards": receipt["cards"],
            "triangles": receipt["triangles"],
            "occupiedVoxels": receipt["occupiedVoxelCount"],
            "policyHash": receipt["policyHash"],
        }, separators=(",", ":")))
        return 0
    except (CardLodError, OSError, OverflowError, struct.error) as error:
        print(f"FOLIAGE_CARD_LOD_ERROR={error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
