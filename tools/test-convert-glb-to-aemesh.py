#!/usr/bin/env python3
"""Hostile and deterministic tests for the production GLB converter."""

import importlib.util
import json
import pathlib
import subprocess
import struct
import tempfile

TOOL = pathlib.Path(__file__).with_name("convert-glb-to-aemesh.py")
SPEC = importlib.util.spec_from_file_location("aevrynd_glb_converter", TOOL)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def make_glb(path, mutate=None):
    positions = struct.pack("<9f", 0, 0, 0, 1, 0, 0, 0, 1, 0)
    normals = struct.pack("<9f", 0, 0, 1, 0, 0, 1, 0, 0, 1)
    uvs = struct.pack("<6H", 0, 0, 65535, 0, 0, 65535)
    indices = struct.pack("<3B", 0, 1, 2)
    padding = b"\0"
    binary = positions + normals + uvs + indices + padding
    document = {
        "asset": {"version": "2.0"}, "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "translation": [2, 3, 4]}],
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(positions)},
            {"buffer": 0, "byteOffset": len(positions), "byteLength": len(normals)},
            {"buffer": 0, "byteOffset": len(positions)+len(normals), "byteLength": len(uvs)},
            {"buffer": 0, "byteOffset": len(positions)+len(normals)+len(uvs), "byteLength": len(indices)}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5123, "normalized": True, "count": 3, "type": "VEC2"},
            {"bufferView": 3, "componentType": 5121, "count": 3, "type": "SCALAR"}],
        "materials": [{"name": "Test Material"}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0}]}]}
    if mutate:
        replacement = mutate(document, binary)
        if replacement is not None:
            binary = replacement
    encoded = json.dumps(document, separators=(",", ":")).encode()
    encoded += b" " * ((4 - len(encoded) % 4) % 4)
    total = 12 + 8 + len(encoded) + 8 + len(binary)
    path.write_bytes(struct.pack("<III", MODULE.MAGIC_GLTF, 2, total) + struct.pack("<II", len(encoded), MODULE.MAGIC_JSON) + encoded + struct.pack("<II", len(binary), MODULE.MAGIC_BIN) + binary)


def must_reject(path, mutate):
    make_glb(path, mutate)
    try:
        MODULE.convert(path, path.parent / "out", path.parent / "receipt.json", "hostile", 0)
    except MODULE.ConversionError:
        return
    raise AssertionError("Hostile GLB was accepted")


def main():
    with tempfile.TemporaryDirectory(prefix="aevrynd-glb-hostile-") as temporary:
        root = pathlib.Path(temporary)
        source = root / "valid.glb"
        make_glb(source)
        first = MODULE.convert(source, root / "first", root / "first.json", "fixture", 0)
        second = MODULE.convert(source, root / "second", root / "second.json", "fixture", 0)
        assert first["outputCount"] == 1 and first["materialPrimitiveBijection"] and first["memoryGate"]
        a = root / "first" / first["outputs"][0]["file"]
        b = root / "second" / second["outputs"][0]["file"]
        assert a.read_bytes() == b.read_bytes()
        magic, version, vertices, indices = struct.unpack_from("<IIII", a.read_bytes())
        assert (magic, version, vertices, indices) == (MODULE.MAGIC_AEM3, 1, 3, 3)
        assert len(a.read_bytes()) == 16 + vertices * 28 + indices * 4
        x, y, z = struct.unpack_from("<fff", a.read_bytes(), 16)
        assert (x, y, z) == (2.0, 3.0, -4.0)
        converted_indices = struct.unpack_from("<III", a.read_bytes(), 16 + vertices * 28)
        assert converted_indices == (0, 2, 1)
        sparse_binary = struct.pack("<3f", 0, 0, 0) + bytes([1]) + b"\0\0\0" + struct.pack("<f", 7)
        sparse_document = {"bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 12}, {"buffer": 0, "byteOffset": 12, "byteLength": 1}, {"buffer": 0, "byteOffset": 16, "byteLength": 4}], "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "SCALAR", "sparse": {"count": 1, "indices": {"bufferView": 1, "componentType": 5121}, "values": {"bufferView": 2}}}]}
        sparse_values, _, _ = MODULE.decode_accessor(sparse_document, sparse_binary, 0)
        assert sparse_values == [[0.0], [7.0], [0.0]]
        must_reject(root / "external.glb", lambda doc, _: doc["buffers"][0].update(uri="escape.bin"))
        must_reject(root / "image-escape.glb", lambda doc, _: doc.update(images=[{"uri": "../escape.png"}]))
        must_reject(root / "draco.glb", lambda doc, _: doc.update(extensionsRequired=["KHR_draco_mesh_compression"]))
        must_reject(root / "overflow.glb", lambda doc, _: doc["accessors"][0].update(count=MODULE.MAX_ACCESSOR_ITEMS + 1))
        must_reject(root / "bad-index.glb", lambda doc, _: doc["accessors"][3].update(byteOffset=999999))
        must_reject(root / "cycle.glb", lambda doc, _: doc["nodes"][0].update(children=[0]))
        must_reject(root / "nan.glb", lambda doc, _: doc["nodes"][0].update(translation=[float("nan"), 0, 0]))
        must_reject(root / "singular.glb", lambda doc, _: doc["nodes"][0].update(scale=[0, 1, 1]))
        must_reject(root / "degenerate.glb", lambda _, binary: binary[:-4] + bytes([0, 1, 1, 0]))
        duplicate = root / "duplicate-json.glb"
        make_glb(duplicate)
        raw = duplicate.read_bytes()
        json_length = struct.unpack_from("<I", raw, 12)[0]
        json_chunk = raw[20:20 + json_length].rstrip(b" ")
        duplicated = json_chunk[:-1] + b',"scene":0}'
        duplicated += b" " * ((4 - len(duplicated) % 4) % 4)
        binary_offset = 20 + json_length
        binary_chunk = raw[binary_offset + 8:]
        total = 12 + 8 + len(duplicated) + 8 + len(binary_chunk)
        duplicate.write_bytes(struct.pack("<III", MODULE.MAGIC_GLTF, 2, total) + struct.pack("<II", len(duplicated), MODULE.MAGIC_JSON) + duplicated + struct.pack("<II", len(binary_chunk), MODULE.MAGIC_BIN) + binary_chunk)
        try:
            MODULE.convert(duplicate, root / "duplicate-out", root / "duplicate.json", "hostile", 0)
        except MODULE.ConversionError:
            pass
        else:
            raise AssertionError("Duplicate JSON member was accepted")
        outside_directory = root.parent / (root.name + "-outside")
        outside_directory.mkdir()
        outside = outside_directory / "outside.png"
        outside.write_bytes(b"outside")
        link = root / "image-link.png"
        try:
            try:
                link.symlink_to(outside)
                uri = "image-link.png"
            except OSError:
                link = root / "image-link"
                subprocess.run(["cmd.exe", "/d", "/c", "mklink", "/J", str(link), str(outside_directory)], check=True, capture_output=True)
                uri = "image-link/outside.png"
            must_reject(root / "image-symlink.glb", lambda doc, _: doc.update(images=[{"uri": uri}]))
        finally:
            if link.is_symlink():
                link.unlink(missing_ok=True)
            elif link.exists():
                subprocess.run(["cmd.exe", "/d", "/c", "rmdir", str(link)], check=True, capture_output=True)
            outside.unlink(missing_ok=True)
            outside_directory.rmdir()
    print("glbConverterHostile=pass deterministic=pass exactLength=pass winding=pass normalized=pass")


if __name__ == "__main__":
    main()
