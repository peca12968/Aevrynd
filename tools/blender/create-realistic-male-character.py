import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def output_directory() -> Path:
    separator = sys.argv.index("--")
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


def material(name: str, color: tuple[float, float, float, float], roughness: float,
             metallic: float = 0.0, subsurface: float = 0.0) -> bpy.types.Material:
    result = bpy.data.materials.new(name)
    result.diffuse_color = color
    result.use_nodes = True
    shader = result.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = color
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Metallic"].default_value = metallic
    if "Subsurface Weight" in shader.inputs:
        shader.inputs["Subsurface Weight"].default_value = subsurface
    return result


def finish_object(obj: bpy.types.Object, object_material: bpy.types.Material, bone: str) -> bpy.types.Object:
    obj.data.materials.append(object_material)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    group = obj.vertex_groups.new(name=bone)
    group.add(range(len(obj.data.vertices)), 1.0, "REPLACE")
    return obj


def ellipsoid(name: str, location: tuple[float, float, float], scale: tuple[float, float, float],
              object_material: bpy.types.Material, bone: str, segments: int = 32, rings: int = 20) -> bpy.types.Object:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish_object(obj, object_material, bone)


def capsule(name: str, start: tuple[float, float, float], end: tuple[float, float, float], radius: float,
            object_material: bpy.types.Material, bone: str, taper: float = 0.86) -> bpy.types.Object:
    start_point, end_point = Vector(start), Vector(end)
    direction = end_point - start_point
    middle = (start_point + end_point) * 0.5
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=14, location=middle)
    obj = bpy.context.object
    obj.name = name
    obj.scale = (radius, radius * taper, direction.length * 0.5 + radius * 0.38)
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = Vector((0.0, 0.0, 1.0)).rotation_difference(direction.normalized())
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish_object(obj, object_material, bone)


def cube(name: str, location: tuple[float, float, float], scale: tuple[float, float, float],
         object_material: bpy.types.Material, bone: str, bevel: float) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    modifier = obj.modifiers.new(name="NaturalEdges", type="BEVEL")
    modifier.width = bevel
    modifier.segments = 3
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    return finish_object(obj, object_material, bone)


def create_armature() -> bpy.types.Object:
    armature = bpy.data.armatures.new("MaleHumanoidArmature")
    rig = bpy.data.objects.new("MaleHumanoidRig", armature)
    bpy.context.scene.collection.objects.link(rig)
    rig.show_in_front = True
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")

    bones: dict[str, bpy.types.EditBone] = {}

    def add(name: str, head: tuple[float, float, float], tail: tuple[float, float, float], parent: str | None = None) -> None:
        bone = armature.edit_bones.new(name)
        bone.head, bone.tail = head, tail
        if parent:
            bone.parent = bones[parent]
        bones[name] = bone

    add("Root", (0, 0, 0), (0, 0, 0.18))
    add("Hips", (0, 0, 0.92), (0, 0, 1.10), "Root")
    add("Spine", (0, 0, 1.10), (0, 0, 1.35), "Hips")
    add("Chest", (0, 0, 1.35), (0, 0, 1.62), "Spine")
    add("Neck", (0, 0, 1.62), (0, 0, 1.76), "Chest")
    add("Head", (0, 0, 1.76), (0, 0, 2.02), "Neck")
    for side, sign in (("Left", 1.0), ("Right", -1.0)):
        add(f"{side}Shoulder", (0.12 * sign, 0, 1.58), (0.31 * sign, 0, 1.56), "Chest")
        add(f"{side}UpperArm", (0.31 * sign, 0, 1.56), (0.47 * sign, 0, 1.29), f"{side}Shoulder")
        add(f"{side}LowerArm", (0.47 * sign, 0, 1.29), (0.54 * sign, -0.015, 1.05), f"{side}UpperArm")
        add(f"{side}Hand", (0.54 * sign, -0.015, 1.05), (0.57 * sign, -0.025, 0.91), f"{side}LowerArm")
        add(f"{side}UpperLeg", (0.16 * sign, 0, 1.00), (0.17 * sign, 0, 0.53), "Hips")
        add(f"{side}LowerLeg", (0.17 * sign, 0, 0.53), (0.16 * sign, 0, 0.10), f"{side}UpperLeg")
        add(f"{side}Foot", (0.16 * sign, 0, 0.10), (0.16 * sign, -0.22, 0.045), f"{side}LowerLeg")
    bpy.ops.object.mode_set(mode="OBJECT")
    return rig


def create_character(rig: bpy.types.Object) -> list[bpy.types.Object]:
    skin = material("WarmSkinPBR", (0.48, 0.235, 0.14, 1), 0.48, subsurface=0.08)
    skin_light = material("FaceHighlightPBR", (0.57, 0.31, 0.21, 1), 0.43, subsurface=0.10)
    hair = material("DarkBrownHairPBR", (0.028, 0.014, 0.009, 1), 0.34)
    eye_white = material("EyeScleraPBR", (0.78, 0.76, 0.70, 1), 0.22)
    iris = material("HazelIrisPBR", (0.12, 0.20, 0.10, 1), 0.16)
    shirt = material("DeepBlueShirtPBR", (0.025, 0.075, 0.13, 1), 0.74)
    trousers = material("CharcoalClothPBR", (0.035, 0.045, 0.055, 1), 0.82)
    leather = material("BrownLeatherPBR", (0.09, 0.035, 0.018, 1), 0.46)
    metal = material("BuckleMetalPBR", (0.25, 0.27, 0.28, 1), 0.24, metallic=0.82)
    objects: list[bpy.types.Object] = []

    # 軀幹使用多個解剖量體建立自然肩背、胸肌與腹肌輪廓。
    objects += [ellipsoid("TorsoCore", (0, 0.015, 1.39), (0.36, 0.20, 0.39), shirt, "Chest")]
    objects += [ellipsoid("LeftPectoral", (0.17, -0.155, 1.53), (0.20, 0.075, 0.14), shirt, "Chest"),
                ellipsoid("RightPectoral", (-0.17, -0.155, 1.53), (0.20, 0.075, 0.14), shirt, "Chest")]
    objects += [ellipsoid("LeftLatissimus", (0.27, 0.07, 1.43), (0.15, 0.13, 0.27), shirt, "Chest"),
                ellipsoid("RightLatissimus", (-0.27, 0.07, 1.43), (0.15, 0.13, 0.27), shirt, "Chest")]
    objects += [ellipsoid("Waist", (0, 0.02, 1.17), (0.255, 0.17, 0.25), shirt, "Spine"),
                ellipsoid("Pelvis", (0, 0.02, 1.01), (0.31, 0.19, 0.20), trousers, "Hips")]
    for row, height in enumerate((1.36, 1.25, 1.14)):
        for side, sign in (("Left", 1), ("Right", -1)):
            objects.append(ellipsoid(f"{side}Abdominal{row + 1}", (0.075 * sign, -0.165, height),
                                     (0.072, 0.035, 0.075), shirt, "Spine", 20, 12))

    objects += [ellipsoid("Head", (0, -0.005, 1.86), (0.145, 0.125, 0.19), skin_light, "Head"),
                cube("Jaw", (0, -0.052, 1.785), (0.105, 0.078, 0.075), skin, "Head", 0.045),
                ellipsoid("Chin", (0, -0.092, 1.745), (0.070, 0.040, 0.040), skin_light, "Head", 20, 12),
                ellipsoid("NoseBridge", (0, -0.124, 1.895), (0.025, 0.038, 0.067), skin_light, "Head", 20, 12),
                ellipsoid("NoseTip", (0, -0.147, 1.855), (0.030, 0.025, 0.025), skin_light, "Head", 20, 12),
                capsule("Mouth", (-0.035, -0.137, 1.805), (0.035, -0.137, 1.805), 0.009, hair, "Head", 0.55),
                ellipsoid("LeftCheek", (0.074, -0.105, 1.845), (0.058, 0.032, 0.052), skin_light, "Head", 20, 12),
                ellipsoid("RightCheek", (-0.074, -0.105, 1.845), (0.058, 0.032, 0.052), skin_light, "Head", 20, 12)]
    for side, sign in (("Left", 1), ("Right", -1)):
        objects += [ellipsoid(f"{side}Eye", (0.049 * sign, -0.124, 1.905), (0.026, 0.012, 0.015), eye_white, "Head", 20, 12),
                    ellipsoid(f"{side}Iris", (0.049 * sign, -0.137, 1.905), (0.009, 0.004, 0.009), iris, "Head", 16, 10),
                    capsule(f"{side}Brow", (0.024 * sign, -0.139, 1.940), (0.076 * sign, -0.135, 1.944), 0.006, hair, "Head")]
    objects += [ellipsoid("HairBase", (0, 0.026, 2.005), (0.143, 0.113, 0.070), hair, "Head"),
                capsule("HairFrontLeft", (-0.082, -0.080, 2.016), (-0.010, -0.095, 2.052), 0.028, hair, "Head", 0.62),
                capsule("HairFrontRight", (0.076, -0.080, 2.014), (0.012, -0.096, 2.050), 0.026, hair, "Head", 0.62),
                capsule("HairCrown", (-0.025, 0.005, 2.042), (0.045, -0.015, 2.080), 0.032, hair, "Head", 0.60),
                capsule("NeckForm", (0, 0, 1.62), (0, 0, 1.76), 0.092, skin, "Neck")]

    for side, sign in (("Left", 1.0), ("Right", -1.0)):
        objects += [ellipsoid(f"{side}Deltoid", (0.335 * sign, 0, 1.545), (0.125, 0.125, 0.145), shirt, f"{side}UpperArm"),
                    capsule(f"{side}UpperArmForm", (0.34 * sign, 0, 1.51), (0.47 * sign, 0, 1.29), 0.094, skin, f"{side}UpperArm"),
                    capsule(f"{side}ForearmForm", (0.47 * sign, 0, 1.29), (0.54 * sign, -0.015, 1.05), 0.075, skin, f"{side}LowerArm", 0.72),
                    ellipsoid(f"{side}HandForm", (0.558 * sign, -0.02, 0.98), (0.058, 0.040, 0.090), skin, f"{side}Hand", 24, 14),
                    capsule(f"{side}ThighForm", (0.16 * sign, 0, 0.98), (0.17 * sign, 0, 0.54), 0.145, trousers, f"{side}UpperLeg", 0.82),
                    capsule(f"{side}CalfForm", (0.17 * sign, 0.01, 0.52), (0.16 * sign, 0.015, 0.11), 0.103, skin, f"{side}LowerLeg", 0.70),
                    ellipsoid(f"{side}FootForm", (0.16 * sign, -0.11, 0.055), (0.105, 0.190, 0.060), leather, f"{side}Foot", 24, 14)]

    objects += [cube("Belt", (0, -0.005, 1.075), (0.305, 0.19, 0.025), leather, "Hips", 0.025),
                cube("BeltBuckle", (0, -0.205, 1.075), (0.045, 0.012, 0.035), metal, "Hips", 0.008)]
    for obj in objects:
        modifier = obj.modifiers.new(name="HumanoidSkin", type="ARMATURE")
        modifier.object = rig
        obj.parent = rig
    return objects


def consolidate_character(objects: list[bpy.types.Object], rig: bpy.types.Object) -> bpy.types.Object:
    # 將材質外觀烘入頂點色後合併，符合目前單一 mesh、單一 primitive 的執行期限制。
    for obj in objects:
        source_color = obj.data.materials[0].diffuse_color
        colors = obj.data.color_attributes.new(name="CharacterColor", type="BYTE_COLOR", domain="POINT")
        for value in colors.data:
            value.color = source_color

    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    character = bpy.context.object
    character.name = "RealisticMaleCharacter"
    character.data.name = "RealisticMaleCharacterMesh"
    character.data.color_attributes.active_color = character.data.color_attributes.get("CharacterColor")

    character.data.materials.clear()
    combined = material("CharacterVertexColorPBR", (1.0, 1.0, 1.0, 1.0), 0.48, subsurface=0.04)
    vertex_color = combined.node_tree.nodes.new("ShaderNodeVertexColor")
    vertex_color.layer_name = "CharacterColor"
    shader = combined.node_tree.nodes.get("Principled BSDF")
    combined.node_tree.links.new(vertex_color.outputs["Color"], shader.inputs["Base Color"])
    character.data.materials.append(combined)
    for polygon in character.data.polygons:
        polygon.material_index = 0
    character.parent = rig
    return character


def export_asset(destination: Path, objects: list[bpy.types.Object], rig: bpy.types.Object) -> Path:
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = rig
    output_path = destination / "realistic-male-character.gltf"
    bpy.ops.export_scene.gltf(filepath=str(output_path), export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_skins=True, export_yup=True,
                              export_materials="EXPORT", export_cameras=False, export_lights=False)
    return output_path


def main() -> None:
    destination = output_directory()
    clear_scene()
    rig = create_armature()
    objects = create_character(rig)
    character = consolidate_character(objects, rig)
    output_path = export_asset(destination, [character], rig)
    vertex_count = len(character.data.vertices)
    triangle_count = sum(len(polygon.vertices) - 2 for polygon in character.data.polygons)
    print(f"REALISTIC_MALE_ASSET={output_path}")
    print(f"OBJECTS=1 MATERIALS={len(character.data.materials)} VERTICES={vertex_count} TRIANGLES={triangle_count} BONES={len(rig.data.bones)}")


main()
