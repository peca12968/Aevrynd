import argparse
import json
from pathlib import Path

import bpy


def load_manifest(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def pack_uvs(margin: float) -> None:
    for scene_object in bpy.context.scene.objects:
        scene_object.select_set(scene_object.type == "MESH")
    bpy.context.view_layer.objects.active = next(
        (item for item in bpy.context.selected_objects if item.type == "MESH"), None
    )
    if bpy.context.view_layer.objects.active is None:
        return
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(island_margin=margin)
    bpy.ops.uv.pack_islands(rotate=True, margin=margin)
    bpy.ops.object.mode_set(mode="OBJECT")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--margin", type=float, default=0.012)
    arguments, _ = parser.parse_known_args()
    manifest = load_manifest(Path(arguments.manifest))
    pack_uvs(arguments.margin)
    bpy.context.scene["aevrynd_material_policy"] = json.dumps(manifest["resolutionPolicy"])
    bpy.ops.wm.save_as_mainfile(filepath=str(Path(arguments.output).resolve()))


if __name__ == "__main__":
    main()
