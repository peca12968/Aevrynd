import argparse
import json
import math
import pathlib
import sys

import bpy
import mathutils


def main():
    values = sys.argv[sys.argv.index("--") + 1 :]
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--receipt", required=True)
    parser.add_argument("--preview")
    arguments = parser.parse_args(values)
    source = pathlib.Path(arguments.source).resolve()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.edit.use_global_undo = False
    bpy.ops.import_scene.gltf(filepath=str(source))
    objects = [item for item in bpy.context.scene.objects if item.type == "MESH"]
    if not objects:
        raise ValueError("No mesh objects were imported.")
    triangles = sum(len(item.data.polygons) for item in objects)
    vertices = sum(len(item.data.vertices) for item in objects)
    has_uv = all(len(item.data.uv_layers) > 0 for item in objects)
    has_normals = all(len(item.data.corner_normals) > 0 for item in objects)
    material_slots = sum(len(item.material_slots) for item in objects)
    points = [item.matrix_world @ vertex.co for item in objects for vertex in item.data.vertices]
    minimum = [min(point[index] for point in points) for index in range(3)]
    maximum = [max(point[index] for point in points) for index in range(3)]
    if not triangles or not has_uv or not has_normals or not material_slots or not all(math.isfinite(value) for value in minimum + maximum):
        raise ValueError("Mesh channel, material, triangle, or bounds validation failed.")
    receipt = {
        "schemaVersion": 1,
        "source": str(source),
        "vertices": vertices,
        "triangles": triangles,
        "meshObjects": len(objects),
        "materialSlots": material_slots,
        "hasUv": has_uv,
        "hasNormals": has_normals,
        "boundsMinimum": minimum,
        "boundsMaximum": maximum,
        "visualNonEmpty": triangles > 0 and any(maximum[i] > minimum[i] for i in range(3)),
    }
    if arguments.preview:
        center = [(minimum[i] + maximum[i]) * 0.5 for i in range(3)]
        extent = max(maximum[i] - minimum[i] for i in range(3))
        camera_data = bpy.data.cameras.new("PreviewCamera")
        camera = bpy.data.objects.new("PreviewCamera", camera_data)
        bpy.context.collection.objects.link(camera)
        camera.location = (center[0] + extent * 1.45, center[1] - extent * 1.45, center[2] + extent * 0.7)
        camera.rotation_euler = ((mathutils.Vector(center) - camera.location).to_track_quat("-Z", "Y").to_euler())
        bpy.context.scene.camera = camera
        light_data = bpy.data.lights.new("PreviewSun", type="SUN")
        light_data.energy = 4.0
        light = bpy.data.objects.new("PreviewSun", light_data)
        bpy.context.collection.objects.link(light)
        light.rotation_euler = (0.6, -0.4, -0.5)
        scene = bpy.context.scene
        scene.render.engine = "BLENDER_EEVEE"
        scene.render.resolution_x = 512
        scene.render.resolution_y = 512
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = "PNG"
        scene.render.filepath = str(pathlib.Path(arguments.preview).resolve())
        scene.world = bpy.data.worlds.new("PreviewWorld")
        scene.world.color = (0.05, 0.07, 0.1)
        bpy.ops.render.render(write_still=True)
    pathlib.Path(arguments.receipt).write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(f"LOD_INSPECTION=PASS TRIANGLES={triangles} MATERIALS={material_slots}")


if __name__ == "__main__":
    main()
