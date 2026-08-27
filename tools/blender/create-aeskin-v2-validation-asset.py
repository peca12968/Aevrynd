import sys
from pathlib import Path

import bpy


destination = Path(sys.argv[sys.argv.index("--") + 1]).resolve()
destination.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)

materials = []
for index, (name, color) in enumerate((("ValidationRed", (0.8, 0.1, 0.05, 1.0)), ("ValidationBlue", (0.05, 0.2, 0.8, 1.0)))):
    value = bpy.data.materials.new(name)
    value.diffuse_color = color
    value.use_nodes = True
    shader = value.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = color
    shader.inputs["Roughness"].default_value = (0.35, 0.8)[index]
    shader.inputs["Metallic"].default_value = (0.1, 0.6)[index]
    materials.append(value)

mesh = bpy.data.meshes.new("V2ValidationMesh")
mesh.from_pydata([(-0.5, 0, 0), (0, 0, 0), (-0.5, 0, 1), (0.5, 0, 0), (0.5, 0, 1), (0, 0, 1)], [], [(0, 1, 2), (3, 4, 5)])
mesh.materials.clear()
for value in materials:
    mesh.materials.append(value)
mesh.polygons[0].material_index = 0
mesh.polygons[1].material_index = 1
uv = mesh.uv_layers.new(name="UVMap")
for loop in uv.data:
    loop.uv = (0.5, 0.5)
mesh_object = bpy.data.objects.new("V2ValidationCharacter", mesh)
bpy.context.scene.collection.objects.link(mesh_object)

armature = bpy.data.armatures.new("V2ValidationArmature")
rig = bpy.data.objects.new("V2ValidationRig", armature)
bpy.context.scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
root = armature.edit_bones.new("Root")
root.head, root.tail = (0, 0, 0), (0, 0, 0.5)
chest = armature.edit_bones.new("Chest")
chest.head, chest.tail, chest.parent = (0, 0, 0.5), (0, 0, 1), root
bpy.ops.object.mode_set(mode="OBJECT")

for name, indices in (("Root", (0, 1, 3)), ("Chest", (2, 4, 5))):
    group = mesh_object.vertex_groups.new(name=name)
    group.add(indices, 1.0, "REPLACE")
modifier = mesh_object.modifiers.new(name="ValidationSkin", type="ARMATURE")
modifier.object = rig
mesh_object.parent = rig

action = bpy.data.actions.new("LinearChestMove")
rig.animation_data_create()
rig.animation_data.action = action
bone = rig.pose.bones["Chest"]
for frame, height in ((1, 0.0), (31, 0.1)):
    bone.location.z = height
    bone.keyframe_insert(data_path="location", frame=frame)
for layer in action.layers:
    for strip in layer.strips:
        for channel_bag in strip.channelbags:
            for curve in channel_bag.fcurves:
                for point in curve.keyframe_points:
                    point.interpolation = "LINEAR"
bpy.context.scene.frame_start, bpy.context.scene.frame_end = 1, 31
bpy.context.scene.render.fps = 30

bpy.ops.object.select_all(action="DESELECT")
mesh_object.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = mesh_object
bpy.ops.export_scene.gltf(filepath=str(destination / "aeskin-v2-validation.gltf"), export_format="GLTF_SEPARATE",
                          use_selection=True, export_skins=True, export_animations=True, export_yup=True,
                          export_force_sampling=False)
