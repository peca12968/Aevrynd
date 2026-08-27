import math
import sys
from pathlib import Path

import bpy


def output_directory() -> Path:
    separator = sys.argv.index("--")
    if separator + 1 >= len(sys.argv):
        raise RuntimeError("An output directory is required.")
    destination = Path(sys.argv[separator + 1]).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    return destination


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for collection in (bpy.data.meshes, bpy.data.armatures, bpy.data.materials):
        for item in list(collection):
            if item.users == 0:
                collection.remove(item)


def create_character() -> tuple[bpy.types.Object, bpy.types.Object]:
    segments = 16
    heights = (0.0, 0.32, 0.78, 1.22, 1.62, 1.94, 2.12)
    radii = (0.24, 0.36, 0.46, 0.50, 0.42, 0.30, 0.12)
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, ...]] = []
    for height, radius in zip(heights, radii):
        for segment in range(segments):
            angle = math.tau * segment / segments
            vertices.append((math.cos(angle) * radius, math.sin(angle) * radius, height))
    bottom_center = len(vertices)
    vertices.append((0.0, 0.0, heights[0]))
    top_center = len(vertices)
    vertices.append((0.0, 0.0, heights[-1]))
    for ring in range(len(heights) - 1):
        lower = ring * segments
        upper = (ring + 1) * segments
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            faces.append((lower + segment, lower + next_segment, upper + next_segment, upper + segment))
    for segment in range(segments):
        next_segment = (segment + 1) % segments
        faces.append((bottom_center, next_segment, segment))
        top_ring = (len(heights) - 1) * segments
        faces.append((top_center, top_ring + segment, top_ring + next_segment))

    mesh = bpy.data.meshes.new("ValidationCharacterMesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        polygon.use_smooth = True
        for loop_index in polygon.loop_indices:
            vertex = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            angle = math.atan2(vertex.y, vertex.x) / math.tau + 0.5
            uv_layer.data[loop_index].uv = (angle, max(0.0, min(1.0, vertex.z / heights[-1])))

    mesh_object = bpy.data.objects.new("ValidationCharacter", mesh)
    bpy.context.scene.collection.objects.link(mesh_object)

    armature = bpy.data.armatures.new("ValidationRig")
    rig_object = bpy.data.objects.new("ValidationRig", armature)
    bpy.context.scene.collection.objects.link(rig_object)
    bpy.context.view_layer.objects.active = rig_object
    rig_object.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    root_bone = armature.edit_bones.new("Root")
    root_bone.head = (0.0, 0.0, 0.0)
    root_bone.tail = (0.0, 0.0, 1.05)
    upper_bone = armature.edit_bones.new("Upper")
    upper_bone.head = root_bone.tail
    upper_bone.tail = (0.32, 0.0, 2.12)
    upper_bone.parent = root_bone
    upper_bone.use_connect = True
    bpy.ops.object.mode_set(mode="OBJECT")

    root_group = mesh_object.vertex_groups.new(name="Root")
    upper_group = mesh_object.vertex_groups.new(name="Upper")
    for vertex in mesh.vertices:
        blend = max(0.0, min(1.0, (vertex.co.z - 0.72) / 0.66))
        root_weight = 1.0 - blend
        if root_weight > 0.0:
            root_group.add([vertex.index], root_weight, "REPLACE")
        if blend > 0.0:
            upper_group.add([vertex.index], blend, "REPLACE")

    modifier = mesh_object.modifiers.new(name="ValidationSkin", type="ARMATURE")
    modifier.object = rig_object
    mesh_object.parent = rig_object
    return mesh_object, rig_object


def export_asset(destination: Path, mesh_object: bpy.types.Object, rig_object: bpy.types.Object) -> Path:
    bpy.ops.object.select_all(action="DESELECT")
    mesh_object.select_set(True)
    rig_object.select_set(True)
    bpy.context.view_layer.objects.active = mesh_object
    output_path = destination / "validation-character.gltf"
    bpy.ops.export_scene.gltf(
        filepath=str(output_path),
        export_format="GLTF_SEPARATE",
        use_selection=True,
        export_animations=False,
        export_skins=True,
        export_yup=True,
    )
    return output_path


def main() -> None:
    destination = output_directory()
    clear_scene()
    mesh_object, rig_object = create_character()
    output_path = export_asset(destination, mesh_object, rig_object)
    print(f"BLENDER_SKIN_ASSET={output_path}")


main()
