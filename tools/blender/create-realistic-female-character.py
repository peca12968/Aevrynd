import math
import sys
from pathlib import Path

import bpy


def output_directory() -> Path:
    default = Path(__file__).resolve().parents[2] / "games" / "character_lab" / "assets" / "model-packs" / "realistic-female" / "source"
    if "--" in sys.argv:
        separator = sys.argv.index("--")
        if separator + 1 < len(sys.argv):
            default = Path(sys.argv[separator + 1])
    destination = default.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    return destination


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for collection in (bpy.data.meshes, bpy.data.curves, bpy.data.armatures, bpy.data.materials):
        for item in list(collection):
            if item.users == 0:
                collection.remove(item)


def material(name: str, color: tuple[float, float, float, float], roughness: float, metallic: float = 0.0) -> bpy.types.Material:
    value = bpy.data.materials.new(name)
    value.diffuse_color = color
    value.use_nodes = True
    shader = value.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = color
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Specular IOR Level"].default_value = 0.32
    return value


def smooth_object(value: bpy.types.Object) -> None:
    if value.type != "MESH":
        return
    for polygon in value.data.polygons:
        polygon.use_smooth = True


def add_vertex_color(value: bpy.types.Object, color: tuple[float, float, float, float]) -> None:
    attribute = value.data.color_attributes.new(name="Color", type="BYTE_COLOR", domain="CORNER")
    for item in attribute.data:
        item.color = color


def sphere(name: str, location: tuple[float, float, float], scale: tuple[float, float, float], surface: bpy.types.Material, bone: str, segments: int = 24, rings: int = 16) -> bpy.types.Object:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=location)
    value = bpy.context.object
    value.name = name
    value.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    value.data.materials.append(surface)
    add_vertex_color(value, tuple(surface.diffuse_color))
    smooth_object(value)
    group = value.vertex_groups.new(name=bone)
    group.add(range(len(value.data.vertices)), 1.0, "REPLACE")
    return value


def tapered_limb(name: str, start: tuple[float, float, float], end: tuple[float, float, float], radii: tuple[float, float], surface: bpy.types.Material, bone: str, vertices: int = 18) -> bpy.types.Object:
    direction = mathutils.Vector(end) - mathutils.Vector(start)
    midpoint = (mathutils.Vector(start) + mathutils.Vector(end)) * 0.5
    bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radii[0], radius2=radii[1], depth=direction.length, location=midpoint)
    value = bpy.context.object
    value.name = name
    value.rotation_mode = "QUATERNION"
    value.rotation_quaternion = direction.to_track_quat("Z", "Y")
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    value.data.materials.append(surface)
    add_vertex_color(value, tuple(surface.diffuse_color))
    smooth_object(value)
    group = value.vertex_groups.new(name=bone)
    group.add(range(len(value.data.vertices)), 1.0, "REPLACE")
    return value


def create_rig() -> bpy.types.Object:
    armature = bpy.data.armatures.new("RealisticFemaleArmature")
    rig = bpy.data.objects.new("RealisticFemaleRig", armature)
    bpy.context.scene.collection.objects.link(rig)
    rig.show_in_front = True
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")

    bones: dict[str, bpy.types.EditBone] = {}

    def add(name: str, head: tuple[float, float, float], tail: tuple[float, float, float], parent: str | None = None, connected: bool = False) -> None:
        bone = armature.edit_bones.new(name)
        bone.head = head
        bone.tail = tail
        if parent:
            bone.parent = bones[parent]
            bone.use_connect = connected
        bones[name] = bone

    add("Root", (0, 0, 0), (0, 0, 0.10))
    add("Hips", (0, 0, 0.91), (0, 0, 1.04), "Root")
    add("Spine", (0, 0, 1.04), (0, 0, 1.27), "Hips", True)
    add("Chest", (0, 0, 1.27), (0, 0, 1.45), "Spine", True)
    add("Neck", (0, 0, 1.45), (0, 0, 1.54), "Chest", True)
    add("Head", (0, 0, 1.54), (0, 0, 1.76), "Neck", True)
    for side, sign in (("L", 1.0), ("R", -1.0)):
        add(f"Shoulder.{side}", (0.05 * sign, 0, 1.43), (0.205 * sign, 0, 1.42), "Chest")
        add(f"UpperArm.{side}", (0.205 * sign, 0, 1.42), (0.315 * sign, 0, 1.18), f"Shoulder.{side}", True)
        add(f"Forearm.{side}", (0.315 * sign, 0, 1.18), (0.355 * sign, -0.015, 0.96), f"UpperArm.{side}", True)
        add(f"Hand.{side}", (0.355 * sign, -0.015, 0.96), (0.36 * sign, -0.035, 0.87), f"Forearm.{side}", True)
        add(f"UpperLeg.{side}", (0.105 * sign, 0, 0.94), (0.095 * sign, 0.01, 0.51), "Hips")
        add(f"LowerLeg.{side}", (0.095 * sign, 0.01, 0.51), (0.085 * sign, 0, 0.10), f"UpperLeg.{side}", True)
        add(f"Foot.{side}", (0.10 * sign, 0, 0.11), (0.10 * sign, -0.17, 0.055), f"LowerLeg.{side}")
    bpy.ops.object.mode_set(mode="OBJECT")
    return rig


def create_character(rig: bpy.types.Object) -> bpy.types.Object:
    skin = material("SkinWarmPBR", (0.64, 0.34, 0.25, 1.0), 0.47)
    lips = material("LipsRosePBR", (0.42, 0.075, 0.07, 1.0), 0.38)
    sclera = material("EyeScleraPBR", (0.86, 0.88, 0.84, 1.0), 0.23)
    iris = material("IrisHazelPBR", (0.10, 0.19, 0.10, 1.0), 0.18)
    hair = material("HairChestnutPBR", (0.055, 0.018, 0.009, 1.0), 0.31)
    cloth = material("DressEmeraldPBR", (0.018, 0.16, 0.115, 1.0), 0.61)
    cloth_dark = material("DressTrimPBR", (0.008, 0.045, 0.04, 1.0), 0.52)
    leather = material("BootLeatherPBR", (0.055, 0.018, 0.014, 1.0), 0.48)
    metal = material("AccessoryGoldPBR", (0.55, 0.31, 0.055, 1.0), 0.24, 0.82)

    parts: list[bpy.types.Object] = []
    # 軀幹採多個平滑體塊形成肩、腰與骨盆的自然連續比例。
    parts += [
        sphere("Pelvis", (0, 0.01, 0.96), (0.215, 0.158, 0.155), cloth, "Hips"),
        sphere("Waist", (0, 0.0, 1.17), (0.168, 0.125, 0.205), cloth, "Spine"),
        sphere("Torso", (0, -0.005, 1.34), (0.225, 0.145, 0.205), cloth, "Chest"),
        sphere("Neck", (0, 0, 1.50), (0.064, 0.058, 0.095), skin, "Neck", 20, 12),
        sphere("Head", (0, -0.005, 1.655), (0.112, 0.096, 0.135), skin, "Head", 32, 24),
        sphere("Jaw", (0, -0.061, 1.615), (0.082, 0.048, 0.072), skin, "Head", 24, 16),
        sphere("Nose", (0, -0.102, 1.663), (0.020, 0.028, 0.034), skin, "Head", 16, 10),
        sphere("UpperLip", (0, -0.105, 1.618), (0.034, 0.009, 0.009), lips, "Head", 16, 8),
        sphere("LowerLip", (0, -0.107, 1.607), (0.031, 0.010, 0.009), lips, "Head", 16, 8),
    ]
    for side, sign in (("L", 1.0), ("R", -1.0)):
        parts += [
            tapered_limb(f"UpperArm.{side}", (0.205 * sign, 0, 1.42), (0.315 * sign, 0, 1.18), (0.066, 0.054), skin, f"UpperArm.{side}"),
            tapered_limb(f"Forearm.{side}", (0.315 * sign, 0, 1.18), (0.355 * sign, -0.015, 0.96), (0.053, 0.040), skin, f"Forearm.{side}"),
            sphere(f"Hand.{side}", (0.358 * sign, -0.025, 0.91), (0.043, 0.028, 0.075), skin, f"Hand.{side}", 18, 12),
            tapered_limb(f"Thigh.{side}", (0.105 * sign, 0, 0.92), (0.095 * sign, 0.005, 0.51), (0.102, 0.076), cloth_dark, f"UpperLeg.{side}", 20),
            tapered_limb(f"Calf.{side}", (0.095 * sign, 0.005, 0.51), (0.085 * sign, 0, 0.11), (0.076, 0.048), skin, f"LowerLeg.{side}", 20),
            sphere(f"Boot.{side}", (0.085 * sign, -0.072, 0.066), (0.066, 0.145, 0.062), leather, f"Foot.{side}", 20, 12),
            sphere(f"EyeWhite.{side}", (0.039 * sign, -0.094, 1.678), (0.029, 0.014, 0.017), sclera, "Head", 18, 10),
            sphere(f"Iris.{side}", (0.039 * sign, -0.108, 1.678), (0.011, 0.005, 0.011), iris, "Head", 14, 8),
            sphere(f"UpperEyelid.{side}", (0.039 * sign, -0.106, 1.691), (0.031, 0.006, 0.008), skin, "Head", 14, 8),
            tapered_limb(f"Lash.{side}", (0.012 * sign, -0.111, 1.690), (0.066 * sign, -0.108, 1.690), (0.0025, 0.0015), hair, "Head", 8),
            tapered_limb(f"Brow.{side}", (0.012 * sign, -0.109, 1.712), (0.067 * sign, -0.104, 1.708), (0.0045, 0.003), hair, "Head", 10),
        ]
    # 後腦與錯落髮束避免單一頭盔形髮型，仍維持可控面數。
    parts.append(sphere("HairCrown", (0.008, 0.028, 1.704), (0.119, 0.102, 0.105), hair, "Head", 28, 18))
    strand_roots = ((-0.105, 1.72, 0.34), (-0.072, 1.745, 0.40), (-0.025, 1.755, 0.45), (0.022, 1.75, 0.39), (0.067, 1.735, 0.35), (0.105, 1.705, 0.31))
    for index, (x, top, length) in enumerate(strand_roots):
        sway = (-0.018 if index < 3 else 0.026) + 0.009 * math.sin(index * 2.3)
        parts.append(tapered_limb(f"HairStrand.{index:02d}", (x, 0.068, top), (x + sway, 0.075 + 0.008 * (index % 2), top - length), (0.024, 0.012), hair, "Head", 12))
    parts.append(tapered_limb("SideFringe", (-0.082, -0.075, 1.744), (0.028, -0.101, 1.700), (0.018, 0.008), hair, "Head", 12))
    parts += [
        sphere("DressSkirt", (0, 0.015, 0.91), (0.245, 0.175, 0.205), cloth, "Hips", 28, 18),
        sphere("WaistBelt", (0, -0.005, 1.075), (0.179, 0.132, 0.026), cloth_dark, "Spine", 24, 10),
        sphere("BeltClasp", (0, -0.137, 1.075), (0.028, 0.010, 0.025), metal, "Spine", 14, 8),
    ]

    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    character = bpy.context.object
    character.name = "RealisticFemaleCharacter"
    character.data.name = "RealisticFemaleMesh"
    # AESKIN v1 僅支援單一 primitive；部位差異已寫入頂點色，材質槽統一為一個 PBR atlas 入口。
    unified = material("FemaleCharacterUnifiedPBR", (1.0, 1.0, 1.0, 1.0), 0.46)
    vertex_color = unified.node_tree.nodes.new("ShaderNodeVertexColor")
    vertex_color.layer_name = "Color"
    unified.node_tree.links.new(vertex_color.outputs["Color"], unified.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
    character.data.materials.clear()
    character.data.materials.append(unified)
    for polygon in character.data.polygons:
        polygon.material_index = 0
    modifier = character.modifiers.new(name="HumanoidSkin", type="ARMATURE")
    modifier.object = rig
    character.parent = rig
    character["asset_origin"] = "Original procedural geometry generated by Aevrynd tools"
    character["commercial_use"] = True
    character["age_category"] = "adult"
    return character


def create_idle_animation(rig: bpy.types.Object) -> None:
    action = bpy.data.actions.new("IdleBreathing")
    rig.animation_data_create()
    rig.animation_data.action = action
    for frame, chest_angle in ((1, -0.018), (31, 0.022), (61, -0.018)):
        rig.pose.bones["Chest"].rotation_mode = "XYZ"
        rig.pose.bones["Chest"].rotation_euler.x = chest_angle
        rig.pose.bones["Chest"].keyframe_insert(data_path="rotation_euler", frame=frame)
    action.frame_range = (1, 61)
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 61
    bpy.context.scene.render.fps = 30


def export_asset(destination: Path, character: bpy.types.Object, rig: bpy.types.Object) -> Path:
    bpy.ops.object.select_all(action="DESELECT")
    character.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = character
    output_path = destination / "realistic-female.gltf"
    bpy.ops.export_scene.gltf(
        filepath=str(output_path),
        export_format="GLTF_SEPARATE",
        use_selection=True,
        export_animations=True,
        export_skins=True,
        export_yup=True,
        export_apply=False,
    )
    blend_path = destination / "realistic-female.blend"
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    return output_path


def main() -> None:
    destination = output_directory()
    clear_scene()
    bpy.context.scene["asset_license"] = "Original work; commercial use permitted without external asset dependencies"
    rig = create_rig()
    character = create_character(rig)
    create_idle_animation(rig)
    output = export_asset(destination, character, rig)
    triangles = sum(len(polygon.vertices) - 2 for polygon in character.data.polygons)
    print(f"FEMALE_CHARACTER_GLTF={output}")
    print(f"FEMALE_CHARACTER_VERTICES={len(character.data.vertices)}")
    print(f"FEMALE_CHARACTER_TRIANGLES={triangles}")
    print(f"FEMALE_CHARACTER_BONES={len(rig.data.bones)}")
    print(f"FEMALE_CHARACTER_MATERIALS={len(character.data.materials)}")


if __name__ == "__main__":
    import mathutils

    main()
