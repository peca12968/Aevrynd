#!/usr/bin/env python3
"""Hostile and deterministic tests for the production GLB converter."""

import importlib.util
import hashlib
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


def add_texture_transform(document, binary, *, mismatch=False, transform_override=None):
    secondary_uvs = struct.pack("<6H", 0, 0, 65535, 0, 0, 65535)
    offset = len(binary)
    binary += secondary_uvs
    document["buffers"][0]["byteLength"] = len(binary)
    document["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": len(secondary_uvs)})
    document["accessors"].append({"bufferView": len(document["bufferViews"]) - 1,
                                  "componentType": 5123, "normalized": True,
                                  "count": 3, "type": "VEC2"})
    document["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_1"] = len(document["accessors"]) - 1
    transform = {"offset": [0.25, 0.5], "rotation": 1.5707963267948966,
                 "scale": [2.0, 3.0], "texCoord": 1}
    if transform_override:
        transform.update(transform_override)
    infos = []
    for index in range(3):
        selected = dict(transform)
        if mismatch and index == 2:
            selected["offset"] = [0.5, 0.5]
        infos.append({"index": index, "texCoord": 0,
                      "extensions": {"KHR_texture_transform": selected}})
    document["textures"] = [{}, {}, {}]
    document["extensionsUsed"] = ["KHR_texture_transform"]
    document["materials"][0]["pbrMetallicRoughness"] = {
        "baseColorTexture": infos[0], "metallicRoughnessTexture": infos[1]}
    document["materials"][0]["normalTexture"] = infos[2]
    return binary


def remove_material_and_uv(document, binary):
    primitive = document["meshes"][0]["primitives"][0]
    primitive.pop("material")
    primitive["attributes"].pop("TEXCOORD_0")
    return binary


def invalid_texture_index(document, binary):
    binary = add_texture_transform(document, binary)
    document["materials"][0]["normalTexture"]["index"] = True
    return binary


def invalid_base_texcoord(document, binary):
    binary = add_texture_transform(document, binary)
    document["materials"][0]["normalTexture"]["texCoord"] = True
    return binary


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
        for receipt, directory in ((first, root / "first"), (second, root / "second")):
            assert receipt["sourceSha256"] == hashlib.sha256(source.read_bytes()).hexdigest()
            for output in receipt["outputs"]:
                payload = directory / output["file"]
                assert output["bytes"] == payload.stat().st_size
                assert output["sha256"] == hashlib.sha256(payload.read_bytes()).hexdigest()
        canonical_first = {key: value for key, value in first.items() if key != "peakWorkingSetBytes"}
        canonical_second = {key: value for key, value in second.items() if key != "peakWorkingSetBytes"}
        assert canonical_first == canonical_second
        magic, version, vertices, indices = struct.unpack_from("<IIII", a.read_bytes())
        assert (magic, version, vertices, indices) == (MODULE.MAGIC_AEM3, 1, 3, 3)
        assert len(a.read_bytes()) == 16 + vertices * 28 + indices * 4
        x, y, z = struct.unpack_from("<fff", a.read_bytes(), 16)
        assert (x, y, z) == (2.0, 3.0, -4.0)
        converted_indices = struct.unpack_from("<III", a.read_bytes(), 16 + vertices * 28)
        assert converted_indices == (0, 2, 1)
        transformed_source = root / "transformed.glb"
        make_glb(transformed_source, add_texture_transform)
        transformed_receipt = MODULE.convert(transformed_source, root / "transformed-out",
                                               root / "transformed.json", "transformed", 0)
        transformed_output = root / "transformed-out" / transformed_receipt["outputs"][0]["file"]
        transformed_bytes = transformed_output.read_bytes()
        transformed_uvs = [struct.unpack_from("<ff", transformed_bytes, 16 + index * 28 + 20)
                           for index in range(3)]
        expected_uvs = [(0.25, 0.5), (0.25, 2.5), (-2.75, 0.5)]
        assert all(abs(actual[axis] - expected[axis]) < 1e-6
                   for actual, expected in zip(transformed_uvs, expected_uvs)
                   for axis in range(2))
        metadata = transformed_receipt["outputs"][0]
        assert metadata["sourceTexCoord"] == 1
        assert metadata["textureTransform"] == {
            "offset": [0.25, 0.5], "rotation": 1.5707963267948966,
            "scale": [2.0, 3.0]}
        assert all(abs(actual - expected) < 1e-6 for actual, expected in zip(
            metadata["effectiveUvMinimum"], [-2.75, 0.5]))
        assert all(abs(actual - expected) < 1e-6 for actual, expected in zip(
            metadata["effectiveUvMaximum"], [0.25, 2.5]))
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
        must_reject(root / "texture-transform-mismatch.glb",
                    lambda doc, binary: add_texture_transform(doc, binary, mismatch=True))
        must_reject(root / "texture-transform-bad-offset.glb",
                    lambda doc, binary: add_texture_transform(doc, binary, transform_override={"offset": [0.0]}))
        must_reject(root / "texture-transform-nan.glb",
                    lambda doc, binary: add_texture_transform(doc, binary, transform_override={"rotation": float("nan")}))
        must_reject(root / "texture-transform-bool-texcoord.glb",
                    lambda doc, binary: add_texture_transform(doc, binary, transform_override={"texCoord": True}))
        must_reject(root / "texture-transform-unsupported-texcoord.glb",
                    lambda doc, binary: add_texture_transform(doc, binary, transform_override={"texCoord": MODULE.MAX_TEXCOORD_SET + 1}))
        must_reject(root / "texture-transform-missing-attribute.glb",
                    lambda doc, binary: add_texture_transform(doc, binary, transform_override={"texCoord": 2}))
        must_reject(root / "texture-transform-no-material-no-uv.glb", remove_material_and_uv)
        must_reject(root / "texture-transform-signed-uv.glb",
                    lambda doc, _: doc["accessors"][2].update(componentType=5122))
        must_reject(root / "texture-transform-uint-uv.glb",
                    lambda doc, _: doc["accessors"][2].update(componentType=5125))
        must_reject(root / "texture-transform-unnormalized-uv.glb",
                    lambda doc, _: doc["accessors"][2].update(normalized=False))
        must_reject(root / "texture-transform-bool-accessor.glb",
                    lambda doc, _: doc["meshes"][0]["primitives"][0]["attributes"].update(TEXCOORD_0=True))
        must_reject(root / "texture-transform-bool-material.glb",
                    lambda doc, _: doc["meshes"][0]["primitives"][0].update(material=True))
        must_reject(root / "texture-transform-bool-scene.glb",
                    lambda doc, _: doc.update(scene=True))
        must_reject(root / "texture-transform-bool-mesh.glb",
                    lambda doc, _: doc["nodes"][0].update(mesh=True))
        must_reject(root / "texture-transform-bool-texture-index.glb", invalid_texture_index)
        must_reject(root / "texture-transform-bool-base-texcoord.glb", invalid_base_texcoord)
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
    island = TOOL.parent.parent / "engine/assets/realistic-cc0-first-batch/island-tree-01/island-tree-01-lod0.glb"
    document, binary = MODULE.read_glb(island)
    primitive = document["meshes"][0]["primitives"][1]
    material = document["materials"][primitive["material"]]
    assert material["name"] == "island_tree_01_leaves"
    texture_infos = [material["pbrMetallicRoughness"]["baseColorTexture"],
                     material["pbrMetallicRoughness"]["metallicRoughnessTexture"],
                     material["normalTexture"]]
    assert all(isinstance(info, dict) for info in texture_infos)
    raw_transforms = [info["extensions"]["KHR_texture_transform"] for info in texture_infos]
    assert raw_transforms[0] == raw_transforms[1] == raw_transforms[2]
    transform = MODULE.texture_coordinate_transform(document, primitive["material"], primitive["attributes"])
    source_uvs, _, kind = MODULE.decode_accessor(
        document, binary, primitive["attributes"][f"TEXCOORD_{transform['texCoord']}"])
    effective_uvs = MODULE.transform_texture_coordinates(source_uvs, transform)
    minimum = [min(value[axis] for value in effective_uvs) for axis in range(2)]
    maximum = [max(value[axis] for value in effective_uvs) for axis in range(2)]
    assert kind == "VEC2" and transform["texCoord"] == 0
    assert document["accessors"][primitive["indices"]]["count"] // 3 == 1060032
    expected_samples = [(0.5926743365, 0.2914828247),
                        (0.5375536898, 0.0702656010),
                        (0.3936055261, 0.0309486211)]
    actual_samples = [effective_uvs[0], effective_uvs[123456], effective_uvs[-1]]
    assert all(abs(actual[axis] - expected[axis]) < 1e-8
               for actual, expected in zip(actual_samples, expected_samples)
               for axis in range(2))
    assert 0.00835 < minimum[0] < 0.00837 and 0.01270 < minimum[1] < 0.01272
    assert 0.82028 < maximum[0] < 0.82031 and 0.98259 < maximum[1] < 0.98262
    print("glbConverterHostile=pass semanticReceiptDeterministic=pass receiptPayloadIntegrity=pass exactLength=pass winding=pass normalized=pass textureTransform=pass islandUvRegression=pass")


if __name__ == "__main__":
    main()
