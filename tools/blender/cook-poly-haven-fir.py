"""Cook one licensed Poly Haven fir into deterministic, material-preserving GLB LODs.

This script is intentionally asset-specific.  The source library contains three trees and
many presentation objects; accepting the whole scene would silently turn a forest asset
into an unbounded multi-object import.  Only fir_tree_01_c's authored LOD1/LOD2 are used.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import struct
import sys

import bpy


SOURCE_OBJECTS = ("fir_tree_01_c_LOD1", "fir_tree_01_c_LOD2")
MATERIALS = (
    "fir_tree_01_bark",
    "fir_tree_01_twig",
    "fir_tree_01_dead_branches",
    "fir_tree_01_trunk_c",
)
TEXTURES = {
    "bark": ("fir_tree_01_bark_diff_1k.png", "fir_tree_01_bark_nor_gl_1k.png", "fir_tree_01_bark_rough_1k.png"),
    "trunk": ("fir_tree_01_trunk_c_diff_1k.png", "fir_tree_01_trunk_c_nor_gl_1k.png", "fir_tree_01_trunk_c_rough_1k.png"),
    "twig": ("fir_tree_01_twig_diff_1k.png", "fir_tree_01_twig_nor_gl_1k.png", "fir_tree_01_twig_rough_1k.png", "fir_tree_01_twig_alpha_1k.png"),
}
LOD_PLAN = (
    (0, "fir_tree_01_c_LOD1", 0.65),
    (1, "fir_tree_01_c_LOD2", 0.50),
    (2, "fir_tree_01_c_LOD2", 0.18),
)


def parse_arguments():
    values = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--asset-name", default="fir-tree-01-c")
    return parser.parse_args(values)


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def make_material(name: str, family: str, texture_root: pathlib.Path, alpha: bool = False):
    paths = [texture_root / item for item in TEXTURES[family]]
    missing = [str(path) for path in paths if not path.is_file()]
    if missing:
        raise ValueError("Required offline texture is missing: " + ", ".join(missing))
    material = bpy.data.materials.new(name=name)
    material.use_nodes = True
    material.diffuse_color = (0.18, 0.23, 0.12, 1.0) if family == "twig" else (0.24, 0.16, 0.09, 1.0)
    material.use_backface_culling = not alpha
    if alpha:
        # Blender 5 maps DITHERED to a non-opaque glTF material.  The verifier checks
        # the exported GLB rather than trusting this authoring-side property.
        material.surface_render_method = "DITHERED"
        material.diffuse_color = (*material.diffuse_color[:3], 0.999)
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    coordinates = nodes.new("ShaderNodeTexCoord")
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])

    color = nodes.new("ShaderNodeTexImage")
    color.image = bpy.data.images.load(str(paths[0]), check_existing=True)
    color.image.colorspace_settings.name = "sRGB"
    links.new(coordinates.outputs["UV"], color.inputs["Vector"])
    links.new(color.outputs["Color"], shader.inputs["Base Color"])

    normal_image = nodes.new("ShaderNodeTexImage")
    normal_image.image = bpy.data.images.load(str(paths[1]), check_existing=True)
    normal_image.image.colorspace_settings.name = "Non-Color"
    links.new(coordinates.outputs["UV"], normal_image.inputs["Vector"])
    normal = nodes.new("ShaderNodeNormalMap")
    links.new(normal_image.outputs["Color"], normal.inputs["Color"])
    links.new(normal.outputs["Normal"], shader.inputs["Normal"])

    roughness = nodes.new("ShaderNodeTexImage")
    roughness.image = bpy.data.images.load(str(paths[2]), check_existing=True)
    roughness.image.colorspace_settings.name = "Non-Color"
    links.new(coordinates.outputs["UV"], roughness.inputs["Vector"])
    links.new(roughness.outputs["Color"], shader.inputs["Roughness"])
    shader.inputs["Metallic"].default_value = 0.0

    if alpha:
        alpha_image = nodes.new("ShaderNodeTexImage")
        alpha_image.image = bpy.data.images.load(str(paths[3]), check_existing=True)
        alpha_image.image.colorspace_settings.name = "Non-Color"
        links.new(coordinates.outputs["UV"], alpha_image.inputs["Vector"])
        links.new(alpha_image.outputs["Color"], shader.inputs["Alpha"])
    return material


def install_runtime_materials(texture_root: pathlib.Path):
    # Preserve the library materials for provenance while reserving the canonical
    # names for the simplified runtime graph.  Otherwise Blender silently appends
    # ".001", which breaks deterministic material binding downstream.
    for name in MATERIALS:
        existing = bpy.data.materials.get(name)
        if existing is not None:
            existing.name = name + "_authoring"
    replacements = {
        "fir_tree_01_bark": make_material("fir_tree_01_bark", "bark", texture_root),
        "fir_tree_01_twig": make_material("fir_tree_01_twig", "twig", texture_root, alpha=True),
        "fir_tree_01_dead_branches": make_material("fir_tree_01_dead_branches", "bark", texture_root),
        "fir_tree_01_trunk_c": make_material("fir_tree_01_trunk_c", "trunk", texture_root),
    }
    return replacements


def bounds(obj):
    points = [obj.matrix_world @ vertex.co for vertex in obj.data.vertices]
    return {
        "minimum": [min(point[axis] for point in points) for axis in range(3)],
        "maximum": [max(point[axis] for point in points) for axis in range(3)],
    }


def inspect_glb(path: pathlib.Path):
    data = path.read_bytes()
    magic, version, declared = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67 or version != 2 or declared != len(data):
        raise ValueError("Exported GLB header is invalid")
    json_length, json_kind = struct.unpack_from("<II", data, 12)
    if json_kind != 0x4E4F534A:
        raise ValueError("Exported GLB has no leading JSON chunk")
    document = json.loads(data[20 : 20 + json_length].rstrip(b" \t\r\n\0"))
    modes = {item.get("name"): item.get("alphaMode", "OPAQUE") for item in document.get("materials", [])}
    if set(modes) != set(MATERIALS):
        raise ValueError(f"Exported material set drifted: {sorted(modes)}")
    if modes["fir_tree_01_twig"] == "OPAQUE":
        raise ValueError("Twig foliage was exported as opaque")
    if any(modes[name] != "OPAQUE" for name in MATERIALS if name != "fir_tree_01_twig"):
        raise ValueError(f"Non-foliage material unexpectedly became transparent: {modes}")
    return modes


def prepare_object(source_name: str, replacements, ratio: float):
    source = bpy.data.objects.get(source_name)
    if source is None or source.type != "MESH":
        raise ValueError(f"Required authored object is missing: {source_name}")
    names = [slot.material.name if slot.material else None for slot in source.material_slots]
    if tuple(names) != tuple(name + "_authoring" for name in MATERIALS):
        raise ValueError(f"Authored material slots drifted for {source_name}: {names}")
    obj = source.copy()
    obj.data = source.data.copy()
    bpy.context.scene.collection.objects.link(obj)
    obj.name = "fir-tree-01-c-runtime"
    obj.location = (0.0, 0.0, 0.0)
    obj.rotation_euler = (0.0, 0.0, 0.0)
    obj.scale = (1.0, 1.0, 1.0)
    if not obj.data.uv_layers:
        source_uv = obj.data.attributes.get("UVMap")
        if source_uv is None or source_uv.domain != "CORNER" or source_uv.data_type != "FLOAT_VECTOR":
            raise ValueError(f"Authored corner UVMap is unavailable for {source_name}")
        values = [(float(item.vector[0]), float(item.vector[1])) for item in source_uv.data]
        source_uv.name = "AevryndSourceUv"
        runtime_uv = obj.data.uv_layers.new(name="UVMap")
        if len(runtime_uv.data) != len(values):
            raise ValueError("Corner UV cardinality changed during conversion")
        for index, value in enumerate(values):
            runtime_uv.data[index].uv = value
        obj.data.uv_layers.active = runtime_uv
        obj.data.uv_layers.active_render = runtime_uv
    for index, name in enumerate(MATERIALS):
        obj.data.materials[index] = replacements[name]
    if ratio < 1.0:
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        modifier = obj.modifiers.new(name="AevryndFarLod", type="DECIMATE")
        modifier.decimate_type = "COLLAPSE"
        modifier.ratio = ratio
        modifier.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=modifier.name)
        obj.select_set(False)
    triangulate = obj.modifiers.new(name="AevryndTriangulate", type="TRIANGULATE")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.modifier_apply(modifier=triangulate.name)
    obj.select_set(False)
    if not obj.data.polygons or any(len(face.vertices) != 3 for face in obj.data.polygons):
        raise ValueError("Cooked mesh is empty or non-triangular")
    used_slots = {face.material_index for face in obj.data.polygons}
    if used_slots != set(range(len(MATERIALS))):
        raise ValueError(f"Cooked mesh lost a material primitive: {sorted(used_slots)}")
    return obj


def main():
    args = parse_arguments()
    source = args.source.resolve(strict=True)
    output = args.output.resolve()
    if source.suffix.lower() != ".blend" or source.name != "fir_tree_01_1k.blend":
        raise ValueError("Only the pinned fir_tree_01_1k.blend source is accepted")
    texture_root = source.parent / "textures"
    output.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(source))
    bpy.context.preferences.edit.use_global_undo = False
    if any(bpy.data.objects.get(name) is None for name in SOURCE_OBJECTS):
        raise ValueError("Pinned fir tree authored LODs are unavailable")
    replacements = install_runtime_materials(texture_root)
    levels = []
    for level, source_name, ratio in LOD_PLAN:
        bpy.ops.object.select_all(action="DESELECT")
        obj = prepare_object(source_name, replacements, ratio)
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        destination = output / f"{args.asset_name}-lod{level}.glb"
        bpy.ops.export_scene.gltf(
            filepath=str(destination),
            export_format="GLB",
            use_selection=True,
            export_apply=True,
            export_materials="EXPORT",
            export_image_format="AUTO",
            export_cameras=False,
            export_lights=False,
            export_animations=False,
            export_extras=False,
        )
        material_modes = inspect_glb(destination)
        level_bounds = bounds(obj)
        levels.append({
            "level": level,
            "authoredSourceObject": source_name,
            "decimateRatio": ratio,
            "vertices": len(obj.data.vertices),
            "triangles": len(obj.data.polygons),
            "materials": list(MATERIALS),
            "materialAlphaModes": material_modes,
            "bounds": level_bounds,
            "file": destination.name,
            "bytes": destination.stat().st_size,
            "sha256": sha256(destination),
        })
        bpy.data.objects.remove(obj, do_unlink=True)
    triangles = [entry["triangles"] for entry in levels]
    if not triangles[0] > triangles[1] > triangles[2] or triangles[0] > 50000 or triangles[2] > 9000:
        raise ValueError(f"Realtime LOD triangle budget failed: {triangles}")
    reference_size = [levels[0]["bounds"]["maximum"][axis] - levels[0]["bounds"]["minimum"][axis] for axis in range(3)]
    for entry in levels[1:]:
        size = [entry["bounds"]["maximum"][axis] - entry["bounds"]["minimum"][axis] for axis in range(3)]
        if any(abs(size[axis] - reference_size[axis]) > max(0.12, reference_size[axis] * 0.025) for axis in range(3)):
            raise ValueError(f"LOD bounds drift exceeds tolerance: {size} versus {reference_size}")
    receipt = {
        "schemaVersion": 1,
        "pipeline": "cook-poly-haven-fir",
        "assetName": args.asset_name,
        "license": "CC0-1.0",
        "officialPage": "https://polyhaven.com/a/fir_tree_01",
        "source": str(source),
        "sourceSha256": sha256(source),
        "offlineTextureInputs": [
            {"file": path.name, "bytes": path.stat().st_size, "sha256": sha256(path)}
            for path in sorted({texture_root / name for values in TEXTURES.values() for name in values})
        ],
        "blenderVersion": bpy.app.version_string,
        "opaqueFoliageRejected": True,
        "levels": levels,
    }
    receipt_path = output / f"{args.asset_name}-cook-receipt.json"
    receipt_path.write_text(json.dumps(receipt, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    print(f"FIR_COOK_RECEIPT={receipt_path} TRIANGLES={','.join(map(str, triangles))}")


if __name__ == "__main__":
    main()
