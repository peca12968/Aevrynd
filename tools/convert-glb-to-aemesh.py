#!/usr/bin/env python3
"""Convert a GLB 2.0 scene into deterministic Aevrynd AEMESH v1 files."""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import math
import os
import pathlib
import struct
import sys

MAGIC_GLTF = 0x46546C67
MAGIC_JSON = 0x4E4F534A
MAGIC_BIN = 0x004E4942
MAGIC_AEM3 = 0x334D4541
COMPONENTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT2": 4, "MAT3": 9, "MAT4": 16}
FORMATS = {5120: ("b", 1, True), 5121: ("B", 1, False), 5122: ("h", 2, True), 5123: ("H", 2, False), 5125: ("I", 4, False), 5126: ("f", 4, None)}
MAX_FILE_BYTES = 1024 * 1024 * 1024
MAX_ACCESSOR_ITEMS = 32 * 1024 * 1024


class ConversionError(RuntimeError):
    pass


def fail(message: str) -> None:
    raise ConversionError(message)


def finite(values) -> bool:
    return all(math.isfinite(float(value)) for value in values)


def checked_range(offset: int, size: int, limit: int, label: str) -> None:
    if offset < 0 or size < 0 or offset > limit or size > limit - offset:
        fail(f"{label} exceeds its buffer")


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            fail(f"Duplicate JSON member is not allowed: {key}")
        result[key] = value
    return result


def read_glb(path: pathlib.Path):
    size = path.stat().st_size
    if size < 20 or size > MAX_FILE_BYTES:
        fail("GLB file size is invalid")
    data = path.read_bytes()
    magic, version, declared = struct.unpack_from("<III", data)
    if magic != MAGIC_GLTF or version != 2 or declared != len(data):
        fail("GLB header is invalid")
    chunks = []
    offset = 12
    while offset < len(data):
        checked_range(offset, 8, len(data), "GLB chunk header")
        length, kind = struct.unpack_from("<II", data, offset)
        offset += 8
        if length % 4:
            fail("GLB chunk length must be four-byte aligned")
        checked_range(offset, length, len(data), "GLB chunk")
        chunks.append((kind, data[offset:offset + length]))
        offset += length
    if offset != len(data) or len(chunks) != 2 or chunks[0][0] != MAGIC_JSON or chunks[1][0] != MAGIC_BIN:
        fail("GLB must contain exactly one JSON chunk followed by one BIN chunk")
    try:
        document = json.loads(chunks[0][1].rstrip(b" \t\r\n\0").decode("utf-8"), object_pairs_hook=unique_object)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        fail(f"GLB JSON is invalid: {error}")
    if document.get("asset", {}).get("version") != "2.0":
        fail("Only glTF 2.0 is supported")
    unsupported = {"KHR_draco_mesh_compression", "EXT_meshopt_compression"}
    extensions = set(document.get("extensionsRequired", [])) | set(document.get("extensionsUsed", []))
    if extensions & unsupported:
        fail("Compressed geometry extensions are not supported")
    buffers = document.get("buffers", [])
    if len(buffers) != 1 or "uri" in buffers[0] or buffers[0].get("byteLength", -1) > len(chunks[1][1]):
        fail("GLB must use one embedded BIN buffer")
    for image in document.get("images", []):
        if "uri" not in image or str(image["uri"]).startswith("data:"):
            continue
        # 轉換器不讀取貼圖；僅允許同一資產樹內已存在的相對參照供材質 receipt 使用。
        uri = str(image["uri"])
        relative = pathlib.PurePosixPath(uri.replace("\\", "/"))
        candidate = path.parent / pathlib.Path(*relative.parts)
        try:
            canonical = candidate.resolve(strict=True)
            canonical.relative_to(path.parent.resolve(strict=True))
        except (OSError, ValueError):
            fail("External image URI escapes or is unavailable")
        if not uri or relative.is_absolute() or ".." in relative.parts or ":" in uri or not canonical.is_file():
            fail("External image URI escapes or is unavailable")
    return document, chunks[1][1]


def normalized(value, component_type, signed):
    if component_type == 5126:
        return float(value)
    if not signed:
        maximum = {5121: 255, 5123: 65535, 5125: 4294967295}[component_type]
        return float(value) / maximum
    maximum = {5120: 127, 5122: 32767}[component_type]
    return max(float(value) / maximum, -1.0)


def decode_accessor(document, binary, index: int):
    accessors = document.get("accessors", [])
    if not isinstance(index, int) or index < 0 or index >= len(accessors):
        fail("Accessor index is invalid")
    accessor = accessors[index]
    count = accessor.get("count", -1)
    kind = accessor.get("type")
    component_type = accessor.get("componentType")
    if not isinstance(count, int) or count < 0 or count > MAX_ACCESSOR_ITEMS or kind not in COMPONENTS or component_type not in FORMATS:
        fail("Accessor declaration is unsupported")
    component_count = COMPONENTS[kind]
    code, component_size, signed = FORMATS[component_type]
    packed_size = component_count * component_size
    values = [[0] * component_count for _ in range(count)]
    if "bufferView" in accessor:
        views = document.get("bufferViews", [])
        view_index = accessor["bufferView"]
        if not isinstance(view_index, int) or view_index < 0 or view_index >= len(views):
            fail("Accessor bufferView is invalid")
        view = views[view_index]
        if view.get("buffer", 0) != 0:
            fail("Only the embedded BIN buffer is supported")
        view_offset = view.get("byteOffset", 0)
        view_length = view.get("byteLength", -1)
        accessor_offset = accessor.get("byteOffset", 0)
        stride = view.get("byteStride", packed_size)
        if not all(isinstance(v, int) for v in (view_offset, view_length, accessor_offset, stride)) or stride < packed_size or stride > 252 or stride % component_size:
            fail("Accessor stride or offset is invalid")
        checked_range(view_offset, view_length, len(binary), "bufferView")
        required = 0 if count == 0 else (count - 1) * stride + packed_size
        checked_range(accessor_offset, required, view_length, "accessor")
        for item in range(count):
            start = view_offset + accessor_offset + item * stride
            values[item] = list(struct.unpack_from("<" + code * component_count, binary, start))
    sparse = accessor.get("sparse")
    if sparse is not None:
        sparse_count = sparse.get("count", -1)
        if not isinstance(sparse_count, int) or sparse_count < 0 or sparse_count > count:
            fail("Sparse accessor count is invalid")
        indices_desc, values_desc = sparse.get("indices", {}), sparse.get("values", {})
        index_type = indices_desc.get("componentType")
        if index_type not in (5121, 5123, 5125):
            fail("Sparse indices must be unsigned integers")
        index_code, index_size, _ = FORMATS[index_type]
        def sparse_slice(desc, item_size, label):
            view_index = desc.get("bufferView")
            views = document.get("bufferViews", [])
            if not isinstance(view_index, int) or view_index < 0 or view_index >= len(views):
                fail(f"Sparse {label} bufferView is invalid")
            view = views[view_index]
            if view.get("buffer", 0) != 0 or "byteStride" in view:
                fail(f"Sparse {label} bufferView is unsupported")
            view_offset, view_length, local = view.get("byteOffset", 0), view.get("byteLength", -1), desc.get("byteOffset", 0)
            if not all(isinstance(v, int) for v in (view_offset, view_length, local)):
                fail(f"Sparse {label} offsets are invalid")
            checked_range(view_offset, view_length, len(binary), f"sparse {label} bufferView")
            checked_range(local, sparse_count * item_size, view_length, f"sparse {label}")
            return view_offset + local
        index_start = sparse_slice(indices_desc, index_size, "indices")
        value_start = sparse_slice(values_desc, packed_size, "values")
        previous = -1
        for sparse_index in range(sparse_count):
            target = struct.unpack_from("<" + index_code, binary, index_start + sparse_index * index_size)[0]
            if target <= previous or target >= count:
                fail("Sparse accessor indices must be unique and increasing")
            previous = target
            values[target] = list(struct.unpack_from("<" + code * component_count, binary, value_start + sparse_index * packed_size))
    if accessor.get("normalized", False):
        if component_type == 5126:
            fail("Floating-point accessor cannot be normalized")
        values = [[normalized(value, component_type, signed) for value in row] for row in values]
    if not all(finite(row) for row in values):
        fail("Accessor contains NaN or infinity")
    return values, component_type, kind


def identity():
    return [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]


def multiply(left, right):
    return [sum(left[row * 4 + k] * right[k * 4 + column] for k in range(4)) for row in range(4) for column in range(4)]


def node_matrix(node):
    if "matrix" in node:
        source = node["matrix"]
        if len(source) != 16 or not finite(source): fail("Node matrix is invalid")
        # glTF stores matrices column-major; internal math is row-major.
        return [float(source[column * 4 + row]) for row in range(4) for column in range(4)]
    translation = node.get("translation", [0, 0, 0])
    rotation = node.get("rotation", [0, 0, 0, 1])
    scale = node.get("scale", [1, 1, 1])
    if len(translation) != 3 or len(rotation) != 4 or len(scale) != 3 or not finite(translation + rotation + scale): fail("Node TRS is invalid")
    x, y, z, w = map(float, rotation)
    length = math.sqrt(x*x + y*y + z*z + w*w)
    if length < 1e-12: fail("Node quaternion is zero")
    x, y, z, w = x/length, y/length, z/length, w/length
    sx, sy, sz = map(float, scale)
    r = [1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w), 0,
         2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w), 0,
         2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y), 0,
         0, 0, 0, 1]
    s = [sx,0,0,0, 0,sy,0,0, 0,0,sz,0, 0,0,0,1]
    t = identity(); t[3], t[7], t[11] = map(float, translation)
    return multiply(t, multiply(r, s))


def transform_position(matrix, value):
    x, y, z = value[:3]
    result = [matrix[0]*x+matrix[1]*y+matrix[2]*z+matrix[3], matrix[4]*x+matrix[5]*y+matrix[6]*z+matrix[7], matrix[8]*x+matrix[9]*y+matrix[10]*z+matrix[11]]
    if not finite(result): fail("Transformed position is invalid")
    return result


def transform_normal(matrix, value):
    a,b,c,d,e,f,g,h,i = matrix[0],matrix[1],matrix[2],matrix[4],matrix[5],matrix[6],matrix[8],matrix[9],matrix[10]
    det = a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g)
    if not math.isfinite(det) or abs(det) < 1e-12: fail("Node transform is singular")
    inv = [(e*i-f*h)/det,(c*h-b*i)/det,(b*f-c*e)/det,(f*g-d*i)/det,(a*i-c*g)/det,(c*d-a*f)/det,(d*h-e*g)/det,(b*g-a*h)/det,(a*e-b*d)/det]
    x,y,z = value[:3]
    result = [inv[0]*x+inv[3]*y+inv[6]*z, inv[1]*x+inv[4]*y+inv[7]*z, inv[2]*x+inv[5]*y+inv[8]*z]
    length = math.sqrt(sum(v*v for v in result))
    if not math.isfinite(length) or length < 1e-12: fail("Transformed normal is invalid")
    return [v/length for v in result]


def pack_snorm(value):
    value = max(-1.0, min(1.0, value))
    return int(math.floor(value * 127.0 + (0.5 if value >= 0 else -0.5))) & 255


def pack_normal(normal):
    return pack_snorm(normal[0]) | (pack_snorm(normal[1]) << 8) | (pack_snorm(-normal[2]) << 16) | (127 << 24)


def safe_name(value, fallback):
    text = str(value or fallback).lower()
    result = "".join(character if character.isalnum() else "-" for character in text).strip("-")
    while "--" in result: result = result.replace("--", "-")
    return result[:64] or fallback


def scene_instances(document):
    nodes = document.get("nodes", [])
    scenes = document.get("scenes", [])
    scene_index = document.get("scene", 0)
    if not scenes or not isinstance(scene_index, int) or scene_index < 0 or scene_index >= len(scenes): fail("Default scene is invalid")
    result, visiting = [], set()
    def visit(index, parent, path):
        if not isinstance(index, int) or index < 0 or index >= len(nodes): fail("Scene node index is invalid")
        if index in visiting: fail("Scene node cycle detected")
        visiting.add(index)
        node = nodes[index]
        world = multiply(parent, node_matrix(node))
        current_path = path + [index]
        if "mesh" in node: result.append((index, node["mesh"], world, current_path))
        for child in node.get("children", []): visit(child, world, current_path)
        visiting.remove(index)
    for root in scenes[scene_index].get("nodes", []): visit(root, identity(), [])
    if not result: fail("Default scene has no mesh instances")
    return result


def transform_determinant(matrix):
    a,b,c,d,e,f,g,h,i = matrix[0],matrix[1],matrix[2],matrix[4],matrix[5],matrix[6],matrix[8],matrix[9],matrix[10]
    return a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g)


def write_aemesh(path, positions, normals, uvs, indices, reverse_winding):
    expected = 16 + len(positions) * 28 + len(indices) * 4
    with path.open("wb") as output:
        output.write(struct.pack("<IIII", MAGIC_AEM3, 1, len(positions), len(indices)))
        for position, normal, uv in zip(positions, normals, uvs):
            output.write(struct.pack("<fffIIff", position[0], position[1], -position[2], pack_normal(normal), 0xFFFFFFFF, uv[0], uv[1]))
        for triangle in range(0, len(indices), 3):
            order = (indices[triangle], indices[triangle+2], indices[triangle+1]) if reverse_winding else (indices[triangle], indices[triangle+1], indices[triangle+2])
            output.write(struct.pack("<III", *order))
    if path.stat().st_size != expected: fail("AEMESH exact-length validation failed")
    return expected


def peak_working_set():
    if os.name != "nt": return 0
    class Counters(ctypes.Structure):
        _fields_ = [("cb", ctypes.c_ulong), ("PageFaultCount", ctypes.c_ulong), ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t), ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t), ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t), ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t)]
    counters = Counters(); counters.cb = ctypes.sizeof(counters)
    kernel32, psapi = ctypes.WinDLL("kernel32", use_last_error=True), ctypes.WinDLL("psapi", use_last_error=True)
    kernel32.GetCurrentProcess.restype = ctypes.c_void_p
    psapi.GetProcessMemoryInfo.argtypes = (ctypes.c_void_p, ctypes.POINTER(Counters), ctypes.c_ulong)
    psapi.GetProcessMemoryInfo.restype = ctypes.c_int
    return counters.PeakWorkingSetSize if psapi.GetProcessMemoryInfo(kernel32.GetCurrentProcess(), ctypes.byref(counters), counters.cb) else 0


def convert(source, output_directory, receipt_path, asset, lod):
    document, binary = read_glb(source)
    meshes, materials = document.get("meshes", []), document.get("materials", [])
    output_directory.mkdir(parents=True, exist_ok=True)
    outputs = []
    for instance_ordinal, (node_index, mesh_index, matrix, node_path) in enumerate(scene_instances(document)):
        if not isinstance(mesh_index, int) or mesh_index < 0 or mesh_index >= len(meshes): fail("Node mesh index is invalid")
        for primitive_index, primitive in enumerate(meshes[mesh_index].get("primitives", [])):
            if primitive.get("mode", 4) != 4 or "extensions" in primitive: fail("Only uncompressed triangle primitives are supported")
            attributes = primitive.get("attributes", {})
            if not all(name in attributes for name in ("POSITION", "NORMAL", "TEXCOORD_0")): fail("Primitive requires POSITION, NORMAL, and TEXCOORD_0")
            positions, position_type, position_kind = decode_accessor(document, binary, attributes["POSITION"])
            normals, _, normal_kind = decode_accessor(document, binary, attributes["NORMAL"])
            uvs, _, uv_kind = decode_accessor(document, binary, attributes["TEXCOORD_0"])
            quantized = "KHR_mesh_quantization" in document.get("extensionsUsed", [])
            valid_position_type = position_type == 5126 or (quantized and position_type in (5120, 5121, 5122, 5123))
            if not valid_position_type or position_kind != "VEC3" or normal_kind != "VEC3" or uv_kind != "VEC2" or not positions or len(normals) != len(positions) or len(uvs) != len(positions): fail("Primitive attribute formats or counts are invalid")
            if "indices" in primitive:
                decoded_indices, index_type, index_kind = decode_accessor(document, binary, primitive["indices"])
                if index_kind != "SCALAR" or index_type not in (5121, 5123, 5125): fail("Primitive indices are invalid")
                indices = [int(row[0]) for row in decoded_indices]
            else:
                indices = list(range(len(positions)))
            if len(indices) == 0 or len(indices) % 3 or any(index < 0 or index >= len(positions) for index in indices): fail("Primitive index range is invalid")
            transformed_positions = [transform_position(matrix, value) for value in positions]
            transformed_normals = [transform_normal(matrix, value) for value in normals]
            for triangle in range(0, len(indices), 3):
                if len({indices[triangle], indices[triangle + 1], indices[triangle + 2]}) != 3:
                    fail("Primitive contains a degenerate triangle")
            material_index = primitive.get("material", -1)
            if not isinstance(material_index, int) or material_index < -1 or material_index >= len(materials): fail("Primitive material index is invalid")
            material_name = safe_name(materials[material_index].get("name") if material_index >= 0 else None, f"material-{material_index if material_index >= 0 else 'none'}")
            filename = f"{safe_name(asset, 'asset')}-lod{lod}-node{node_index}-mesh{mesh_index}-primitive{primitive_index}-material{material_index if material_index >= 0 else 'none'}-{material_name}.aemesh"
            destination = output_directory / filename
            determinant = transform_determinant(matrix)
            if not math.isfinite(determinant) or abs(determinant) < 1e-12: fail("Node transform is singular")
            # Aevrynd 的 Z 反射會改變 handedness；負縮放會再翻轉一次。
            reverse_winding = determinant > 0
            byte_count = write_aemesh(destination, transformed_positions, transformed_normals, uvs, indices, reverse_winding)
            digest = hashlib.sha256(destination.read_bytes()).hexdigest()
            minimum = [min(value[axis] for value in transformed_positions) for axis in range(3)]
            maximum = [max(value[axis] for value in transformed_positions) for axis in range(3)]
            outputs.append({"file": filename, "sha256": digest, "bytes": byte_count, "nodeIndex": node_index, "nodePath": node_path, "instanceOrdinal": instance_ordinal, "meshIndex": mesh_index, "primitiveIndex": primitive_index, "materialIndex": material_index, "materialName": material_name, "vertices": len(positions), "indices": len(indices), "triangles": len(indices)//3, "boundsMinimumGltf": minimum, "boundsMaximumGltf": maximum, "windingReversed": reverse_winding, "hasNormals": True, "hasUv": True})
    if not outputs: fail("No triangle primitives were converted")
    receipt = {"schemaVersion": 1, "converter": "convert-glb-to-aemesh.py", "assetName": asset, "lod": lod, "sourceFile": source.name, "sourceSha256": hashlib.sha256(source.read_bytes()).hexdigest(), "outputs": outputs, "outputCount": len(outputs), "materialPrimitiveBijection": len({(x['instanceOrdinal'], x['primitiveIndex']) for x in outputs}) == len(outputs), "totalBytes": sum(x["bytes"] for x in outputs), "totalTriangles": sum(x["triangles"] for x in outputs), "peakWorkingSetBytes": peak_working_set(), "peakWorkingSetScope": "converter-process-no-children", "peakLimitBytes": 2 * 1024 * 1024 * 1024}
    receipt["memoryGate"] = receipt["peakWorkingSetBytes"] > 0 and receipt["peakWorkingSetBytes"] < receipt["peakLimitBytes"]
    if not receipt["memoryGate"]: fail("Peak working set gate failed")
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    receipt_path.write_text(json.dumps(receipt, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return receipt


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=pathlib.Path)
    parser.add_argument("--output-directory", required=True, type=pathlib.Path)
    parser.add_argument("--receipt", required=True, type=pathlib.Path)
    parser.add_argument("--asset", required=True)
    parser.add_argument("--lod", required=True, type=int, choices=range(16))
    arguments = parser.parse_args()
    try:
        receipt = convert(arguments.source.resolve(strict=True), arguments.output_directory.resolve(), arguments.receipt.resolve(), arguments.asset, arguments.lod)
        print("AEMESH_RECEIPT=" + json.dumps({"outputs": receipt["outputCount"], "triangles": receipt["totalTriangles"], "bytes": receipt["totalBytes"], "peakWorkingSetBytes": receipt["peakWorkingSetBytes"]}, separators=(",", ":")))
        return 0
    except (ConversionError, OSError, ValueError, OverflowError, MemoryError) as error:
        print(f"AEMESH_ERROR={error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
