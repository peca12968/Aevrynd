import argparse
import ctypes
import gc
import hashlib
import json
import pathlib
import sys
import urllib.parse

import bpy


def parse_arguments():
    arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--asset-name", required=True)
    parser.add_argument("--ratios", default="1,0.5,0.15")
    parser.add_argument("--level-offset", type=int, default=0)
    return parser.parse_args(arguments)


def import_source(source):
    suffix = source.suffix.lower()
    if suffix in (".gltf", ".glb"):
        bpy.ops.import_scene.gltf(filepath=str(source))
    elif suffix == ".fbx":
        bpy.ops.wm.fbx_import(filepath=str(source))
    elif suffix == ".obj":
        bpy.ops.wm.obj_import(filepath=str(source))
    elif suffix == ".blend":
        bpy.ops.wm.open_mainfile(filepath=str(source))
    else:
        raise ValueError(f"Unsupported source format: {suffix}")


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def mesh_statistics():
    objects = [item for item in bpy.context.scene.objects if item.type == "MESH"]
    return objects, sum(len(item.data.vertices) for item in objects), sum(len(item.data.polygons) for item in objects)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def source_dependencies(source):
    paths = [source]
    if source.suffix.lower() == ".gltf":
        document = json.loads(source.read_text(encoding="utf-8"))
        uris = [item.get("uri") for item in document.get("buffers", []) + document.get("images", [])]
        for uri in uris:
            if not uri or uri.startswith("data:"):
                continue
            parsed = urllib.parse.urlsplit(uri)
            if parsed.scheme or parsed.netloc or parsed.query or parsed.fragment:
                raise ValueError(f"External glTF dependency URI is not allowed: {uri}")
            decoded = urllib.parse.unquote(parsed.path)
            dependency = (source.parent / decoded).resolve()
            # glTF 外部檔只能位於來源目錄內，避免封裝以 ../ 或絕對路徑讀取任意檔案。
            try:
                dependency.relative_to(source.parent)
            except ValueError as error:
                raise ValueError(f"glTF dependency escapes source directory: {uri}") from error
            if not dependency.is_file():
                raise ValueError(f"Missing glTF dependency: {uri}")
            paths.append(dependency)
    return sorted(set(paths))


def main():
    arguments = parse_arguments()
    source = pathlib.Path(arguments.source).resolve()
    output = pathlib.Path(arguments.output).resolve()
    ratios = [float(value) for value in arguments.ratios.split(",")]
    if not source.is_file() or not ratios or (arguments.level_offset == 0 and ratios[0] != 1.0) or any(value <= 0 or value > 1 for value in ratios):
        raise ValueError("Source or LOD ratios are invalid.")
    dependencies = source_dependencies(source)
    output.mkdir(parents=True, exist_ok=True)
    levels = []
    source_triangles = None
    for level, ratio in enumerate(ratios, start=arguments.level_offset):
        reset_scene()
        bpy.context.preferences.edit.use_global_undo = False
        import_source(source)
        objects, _, triangles = mesh_statistics()
        if not objects or triangles == 0:
            raise ValueError("Imported scene contains no usable mesh triangles.")
        # 材質貼圖由獨立 PBR sidecar 管理；LOD 幾何階段先卸載影像，降低 decimate 峰值。
        for item in objects:
            item.data.materials.clear()
        for image in list(bpy.data.images):
            bpy.data.images.remove(image)
        for material in list(bpy.data.materials):
            bpy.data.materials.remove(material)
        gc.collect()
        if sys.platform == "win32":
            process = ctypes.windll.kernel32.GetCurrentProcess()
            ctypes.windll.kernel32.SetProcessWorkingSetSize(process, -1, -1)
        if source_triangles is None:
            source_triangles = triangles
        if ratio < 1:
            for item in objects:
                bpy.context.view_layer.objects.active = item
                item.select_set(True)
                modifier = item.modifiers.new(name="AevryndLod", type="DECIMATE")
                modifier.ratio = ratio
                bpy.ops.object.modifier_apply(modifier=modifier.name)
                item.select_set(False)
        objects, vertices, triangles = mesh_statistics()
        destination = output / f"{arguments.asset_name}-lod{level}.glb"
        bpy.ops.export_scene.gltf(filepath=str(destination), export_format="GLB", export_apply=True)
        levels.append({
            "level": level,
            "requestedRatio": ratio,
            "vertices": vertices,
            "triangles": triangles,
            "file": destination.name,
            "bytes": destination.stat().st_size,
            "sha256": sha256(destination),
        })
        # 每個 LOD 匯出後主動清除 Blender 孤兒資料並歸還工作集，避免大型樹木逐級累積記憶體。
        reset_scene()
        for _ in range(3):
            bpy.ops.outliner.orphans_purge(do_recursive=True)
        gc.collect()
        if sys.platform == "win32":
            process = ctypes.windll.kernel32.GetCurrentProcess()
            ctypes.windll.kernel32.SetProcessWorkingSetSize(process, -1, -1)
    if any(levels[index]["triangles"] >= levels[index - 1]["triangles"] for index in range(1, len(levels))):
        raise ValueError("LOD triangle counts must strictly decrease.")
    receipt = {
        "schemaVersion": 1,
        "assetName": arguments.asset_name,
        "source": str(source),
        "sourceSha256": sha256(source),
        "sourceFiles": [{"file": str(path), "sha256": sha256(path)} for path in dependencies],
        "blenderVersion": bpy.app.version_string,
        "levels": levels,
    }
    receipt_path = output / f"{arguments.asset_name}-lod-receipt.json"
    receipt_path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(f"LOD_RECEIPT={receipt_path} LEVELS={len(levels)} SOURCE_TRIANGLES={source_triangles}")


if __name__ == "__main__":
    main()
