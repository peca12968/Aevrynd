import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


def arguments() -> tuple[Path, Path, Path]:
    separator = sys.argv.index("--")
    values = sys.argv[separator + 1:]
    if len(values) != 3:
        raise RuntimeError("Expected bundle, female output, and male output paths.")
    return tuple(Path(value).resolve() for value in values)


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for collection in (bpy.data.meshes, bpy.data.armatures, bpy.data.materials, bpy.data.actions):
        for item in list(collection):
            if item.users == 0:
                collection.remove(item)


def create_material(name: str, color: tuple[float, float, float, float], roughness: float,
                    metallic: float = 0.0) -> bpy.types.Material:
    value = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    value.diffuse_color = color
    value.use_nodes = True
    shader = value.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = color
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Metallic"].default_value = metallic
    return value


def material_set(female: bool) -> list[bpy.types.Material]:
    if female:
        return [
            create_material("SkinWarmPBR", (.62, .34, .20, 1), .47),
            create_material("LipsRosePBR", (.42, .12, .11, 1), .38),
            create_material("EyeScleraPBR", (.86, .88, .84, 1), .23),
            create_material("IrisHazelPBR", (.10, .19, .10, 1), .18),
            create_material("HairChestnutPBR", (.025, .012, .008, 1), .48),
            create_material("DressEmeraldPBR", (.055, .050, .045, 1), .68),
            create_material("DressTrimPBR", (.025, .022, .020, 1), .58),
            create_material("BootLeatherPBR", (.09, .025, .014, 1), .48),
            create_material("AccessoryGoldPBR", (.78, .48, .09, 1), .24, .82),
        ]
    return [
        create_material("WarmSkinPBR", (.58, .30, .17, 1), .48),
        create_material("FaceHighlightPBR", (.64, .36, .22, 1), .43),
        create_material("DarkBrownHairPBR", (.025, .012, .008, 1), .50),
        create_material("EyeScleraPBR", (.78, .76, .70, 1), .22),
        create_material("HazelIrisPBR", (.12, .20, .10, 1), .16),
        create_material("DeepBlueShirtPBR", (.045, .050, .060, 1), .68),
        create_material("CharcoalClothPBR", (.075, .065, .055, 1), .78),
        create_material("BrownLeatherPBR", (.11, .036, .018, 1), .46),
        create_material("BuckleMetalPBR", (.48, .52, .56, 1), .24, .82),
    ]


def load_base_objects(bundle: Path, female: bool) -> list[bpy.types.Object]:
    prefix = "GEO-body_female_realistic" if female else "GEO-body_male_realistic"
    # 原始眼球帶有資產集合父級座標；身體載入後由本生成器在引擎座標重建眼球，避免雙重轉換。
    names = [prefix]
    with bpy.data.libraries.load(str(bundle), link=False) as (source, target):
        missing = [name for name in names if name not in source.objects]
        if missing:
            raise RuntimeError(f"CC0 base mesh objects are missing: {missing}")
        target.objects = names
    objects = list(target.objects)
    for obj in objects:
        bpy.context.scene.collection.objects.link(obj)
        obj.asset_clear()
        obj.animation_data_clear()
        obj.modifiers.clear()
    return objects


def normalize_and_shape(objects: list[bpy.types.Object], female: bool, height: float) -> None:
    body = objects[0]
    world = [body.matrix_world @ Vector(corner) for corner in body.bound_box]
    minimum_x, maximum_x = min(v.x for v in world), max(v.x for v in world)
    minimum_z, maximum_z = min(v.z for v in world), max(v.z for v in world)
    scale = height / (maximum_z - minimum_z)
    transform = Matrix.Translation((-(minimum_x + maximum_x) * .5 * scale, 0, -minimum_z * scale)) @ Matrix.Scale(scale, 4)
    for obj in objects:
        obj.matrix_world = transform @ obj.matrix_world
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        obj.select_set(False)
        for vertex in obj.data.vertices:
            t = vertex.co.z / height
            if female:
                # F2：收腰、擴髖並保留運動型肩背，所有調整維持原始人體拓撲。
                x_scale = (1.0 + .10 * math.exp(-((t - .52) / .11) ** 2)
                           - .10 * math.exp(-((t - .64) / .07) ** 2)
                           + .05 * math.exp(-((t - .77) / .10) ** 2))
                y_scale = 1.0 + .08 * math.exp(-((t - .52) / .12) ** 2) + .05 * math.exp(-((t - .76) / .10) ** 2)
            else:
                # M2：擴張胸背與肩線，腰髖維持精實比例。
                x_scale = (1.0 + .14 * math.exp(-((t - .78) / .12) ** 2)
                           + .06 * math.exp(-((t - .68) / .11) ** 2)
                           - .03 * math.exp(-((t - .55) / .09) ** 2))
                y_scale = 1.0 + .11 * math.exp(-((t - .72) / .14) ** 2)
            vertex.co.x *= x_scale
            vertex.co.y *= y_scale


def create_rig(female: bool) -> bpy.types.Object:
    armature = bpy.data.armatures.new("F2FemaleArmature" if female else "M2MaleArmature")
    rig = bpy.data.objects.new("F2FemaleRig" if female else "M2MaleRig", armature)
    bpy.context.scene.collection.objects.link(rig)
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

    if female:
        add("Root", (0, 0, 0), (0, 0, .10)); add("Hips", (0, 0, .88), (0, 0, 1.03), "Root")
        add("Spine", (0, 0, 1.03), (0, 0, 1.27), "Hips"); add("Chest", (0, 0, 1.27), (0, 0, 1.45), "Spine")
        add("Neck", (0, 0, 1.45), (0, 0, 1.54), "Chest"); add("Head", (0, 0, 1.54), (0, 0, 1.75), "Neck")
        arm = ((.06, 1.43), (.21, 1.41), (.34, 1.18), (.40, .96), (.42, .86))
        leg = ((.11, .94), (.10, .51), (.09, .10), (.09, -.13, .055))
    else:
        add("Root", (0, 0, 0), (0, 0, .12)); add("Hips", (0, 0, .98), (0, 0, 1.13), "Root")
        add("Spine", (0, 0, 1.13), (0, 0, 1.39), "Hips"); add("Chest", (0, 0, 1.39), (0, 0, 1.63), "Spine")
        add("Neck", (0, 0, 1.63), (0, 0, 1.76), "Chest"); add("Head", (0, 0, 1.76), (0, 0, 2.02), "Neck")
        arm = ((.12, 1.59), (.31, 1.56), (.48, 1.29), (.55, 1.05), (.58, .92))
        leg = ((.16, 1.01), (.17, .54), (.16, .10), (.16, -.20, .05))
    for side, sign in (("Left", 1.0), ("Right", -1.0)):
        add(f"{side}Shoulder", (arm[0][0] * sign, 0, arm[0][1]), (arm[1][0] * sign, 0, arm[1][1]), "Chest")
        add(f"{side}UpperArm", (arm[1][0] * sign, 0, arm[1][1]), (arm[2][0] * sign, 0, arm[2][1]), f"{side}Shoulder")
        add(f"{side}LowerArm", (arm[2][0] * sign, 0, arm[2][1]), (arm[3][0] * sign, -.01, arm[3][1]), f"{side}UpperArm")
        add(f"{side}Hand", (arm[3][0] * sign, -.01, arm[3][1]), (arm[4][0] * sign, -.03, arm[4][1]), f"{side}LowerArm")
        add(f"{side}UpperLeg", (leg[0][0] * sign, 0, leg[0][1]), (leg[1][0] * sign, 0, leg[1][1]), "Hips")
        add(f"{side}LowerLeg", (leg[1][0] * sign, 0, leg[1][1]), (leg[2][0] * sign, 0, leg[2][1]), f"{side}UpperLeg")
        add(f"{side}Foot", (leg[2][0] * sign, 0, leg[2][1]), (leg[3][0] * sign, leg[3][1], leg[3][2]), f"{side}LowerLeg")
    bpy.ops.object.mode_set(mode="OBJECT")
    rig.select_set(False)
    return rig


def add_point_colors(obj: bpy.types.Object) -> None:
    previous = obj.data.color_attributes.get("CharacterColor")
    if previous:
        obj.data.color_attributes.remove(previous)
    colors = obj.data.color_attributes.new(name="CharacterColor", type="BYTE_COLOR", domain="POINT")
    for color in colors.data:
        color.color = (1, 1, 1, 1)
    obj.data.color_attributes.active_color = colors


def assign_materials(body: bpy.types.Object, eyes: list[bpy.types.Object], materials: list[bpy.types.Material], female: bool) -> None:
    body.data.materials.clear()
    for value in materials:
        body.data.materials.append(value)
    skin, shirt, trousers, boots = (0, 5, 6, 7)
    for polygon in body.data.polygons:
        points = [body.data.vertices[index].co for index in polygon.vertices]
        maximum_z = max(point.z for point in points)
        maximum_radius = max(abs(point.x) for point in points)
        if maximum_z < .23:
            polygon.material_index = boots
        elif maximum_z < (1.02 if female else 1.12):
            polygon.material_index = trousers
        elif maximum_z < (1.48 if female else 1.66) and maximum_radius < (.29 if female else .43):
            polygon.material_index = shirt
        else:
            polygon.material_index = skin
    eye_material = 2 if female else 3
    for eye in eyes:
        eye.data.materials.clear(); eye.data.materials.append(materials[eye_material])
        for polygon in eye.data.polygons:
            polygon.material_index = 0


def sphere_part(name: str, location: tuple[float, float, float], scale: tuple[float, float, float],
                surface: bpy.types.Material, opaque_uv: bool = False) -> bpy.types.Object:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=28, ring_count=18, location=location)
    obj = bpy.context.object; obj.name = name; obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(surface)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    if opaque_uv:
        uv = obj.data.uv_layers.active or obj.data.uv_layers.new(name="UVMap")
        for value in uv.data:
            value.uv = (.125, .5)
    return obj


def hair_card(name: str, points: tuple[tuple[float, float, float], ...], width: float,
              tile: int, surface: bpy.types.Material) -> bpy.types.Object:
    centers = [Vector(point) for point in points]
    vertices: list[tuple[float, float, float]] = []
    for index, center in enumerate(centers):
        direction = centers[min(index + 1, len(centers) - 1)] - centers[max(0, index - 1)]
        across = Vector((-direction.z, 0.0, direction.x))
        if across.length < 1.0e-5:
            across = Vector((1.0, 0.0, 0.0))
        across.normalize()
        progress = index / max(1, len(centers) - 1)
        half_width = width * (1.0 - progress * .72)
        vertices.extend((tuple(center - across * half_width), tuple(center + across * half_width)))
    faces = [(index * 2, index * 2 + 1, index * 2 + 3, index * 2 + 2)
             for index in range(len(centers) - 1)]
    mesh = bpy.data.meshes.new(f"{name}Mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(surface)
    uv = mesh.uv_layers.new(name="UVMap")
    tile_min = 0.0 if tile < 0 else .25 + (tile % 6) * .125
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index = mesh.loops[loop_index].vertex_index
            uv.data[loop_index].uv = (tile_min + (vertex_index % 2) * .125,
                                      (vertex_index // 2) / max(1, len(centers) - 1))
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def create_hair_cards(materials: list[bpy.types.Material], female: bool) -> list[bpy.types.Object]:
    hair_material = materials[4 if female else 2]
    cards: list[bpy.types.Object] = []
    if female:
        # F2 使用四層後側長髮與四束臉側髮，避免球形髮帽的塑膠頭盔輪廓。
        for layer in range(4):
            for column in range(13):
                angle = -1.45 + column * (2.90 / 12.0) + layer * .022
                root = (math.sin(angle) * (.104 + layer * .006),
                        .018 + math.cos(angle) * (.096 + layer * .006),
                        1.735 - layer * .010 - abs(math.sin(angle)) * .012)
                length = .31 + .11 * math.cos(angle) ** 2 + .025 * math.sin(column * 1.7 + layer)
                tip = (root[0] * 1.24 + math.sin(column * 1.31 + layer) * .012,
                       .052 + layer * .010,
                       root[2] - length)
                first = (root[0] * .96 + tip[0] * .04,
                         root[1] + .018,
                         root[2] - length * .18)
                middle = ((root[0] + tip[0]) * .5 + math.sin(column * 1.11 + layer) * .014,
                          (root[1] + tip[1]) * .5 + .025,
                          (root[2] + tip[2]) * .5 + .025)
                cards.append(hair_card(f"F2LongHair.{len(cards):02d}", (root, first, middle, tip),
                                       .015 + (column % 3) * .002, len(cards), hair_material))
        for side in (-1.0, 1.0):
            for layer in range(2):
                root = (side * (.070 + layer * .014), -.058 + layer * .006, 1.735 - layer * .012)
                middle = (side * (.098 + layer * .010), -.070, 1.565 - layer * .025)
                tip = (side * (.115 + layer * .012), -.030, 1.355 - layer * .035)
                cards.append(hair_card(f"F2FaceHair.{len(cards):02d}", (root, middle, tip),
                                       .014 + layer * .002, len(cards), hair_material))
    else:
        # M2 以四圈短髮建立漸短側邊，上方髮束略向一側梳起，保持成熟俐落輪廓。
        for band in range(4):
            for column in range(12):
                angle = math.tau * column / 12.0 + band * .045
                root = (math.sin(angle) * (.112 - band * .006),
                        .020 + math.cos(angle) * (.098 - band * .005),
                        2.038 + band * .014)
                swept_angle = angle + .25 + band * .035
                tip = (math.sin(swept_angle) * (.090 - band * .004) + .012,
                       .018 + math.cos(swept_angle) * (.084 - band * .004),
                       root[2] + .010 + band * .003)
                middle = ((root[0] + tip[0]) * .5 + .006,
                          (root[1] + tip[1]) * .5,
                          (root[2] + tip[2]) * .5 + .010)
                cards.append(hair_card(f"M2ShortHair.{len(cards):02d}", (root, middle, tip),
                                       .014 + (column % 3) * .0015, -1, hair_material))
    return cards


def create_face_features(eyes: list[bpy.types.Object], materials: list[bpy.types.Material], female: bool) -> list[bpy.types.Object]:
    iris_material = materials[3 if female else 4]
    hair_material = materials[4 if female else 2]
    sclera_material = materials[2 if female else 3]
    features: list[bpy.types.Object] = []
    eye_x, eye_y, eye_z = ((.034, -.105, 1.665) if female else (.039, -.116, 1.902))
    for index, sign in enumerate((1.0, -1.0)):
        features.append(sphere_part(f"EyeSclera.{index}", (eye_x * sign, eye_y, eye_z),
                                    (.019 if female else .020, .009, .010), sclera_material))
        features.append(sphere_part(f"Iris.{index}", (eye_x * sign, eye_y - .0085, eye_z),
                                    (.0065 if female else .0070, .0022, .0065 if female else .0070), iris_material))
        features.append(sphere_part(f"Pupil.{index}", (eye_x * sign, eye_y - .0105, eye_z),
                                    (.0028, .0012, .0028), hair_material, True))
        features.append(sphere_part(f"Brow.{index}", (eye_x * sign, eye_y - .004, eye_z + (.028 if female else .031)),
                                    (.028 if female else .032, .004, .0045), hair_material, True))
    lip_z = 1.604 if female else 1.824
    lip_y = -.117 if female else -.128
    features.append(sphere_part("UpperLip", (0, lip_y, lip_z + .004),
                                (.029 if female else .032, .004, .006), materials[1]))
    features.append(sphere_part("LowerLip", (0, lip_y - .001, lip_z - .005),
                                (.027 if female else .030, .0045, .0065), materials[1]))
    if female:
        # F2 的長髮先以貼頭髮帽建立乾淨髮際線；後續髮片可在不改身體拓撲下持續增加。
        features.append(sphere_part("F2HairScalp", (0, -.004, 1.700), (.116, .120, .116), hair_material, True))
    else:
        # M2 採短髮輪廓，避免舊版寬亮面髮帽造成卡通感。
        features.append(sphere_part("M2HairScalp", (0, -.004, 1.990), (.126, .128, .104), hair_material, True))
    features.extend(create_hair_cards(materials, female))
    # AESKIN sidecar採嚴格材質集合；尚未外露的唇妝／金屬槽以體內微型標記保留穩定索引。
    for material_index in (1, 8):
        features.append(sphere_part(f"MaterialMarker.{material_index}", (0, 0, .5 + material_index * .001),
                                    (.0005, .0005, .0005), materials[material_index]))
    return features


def bind_to_rig(body: bpy.types.Object, extras: list[bpy.types.Object], rig: bpy.types.Object) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True); rig.select_set(True); bpy.context.view_layer.objects.active = rig
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    for obj in extras:
        group = obj.vertex_groups.new(name="Head")
        group.add(range(len(obj.data.vertices)), 1.0, "REPLACE")
        modifier = obj.modifiers.new(name="HumanoidSkin", type="ARMATURE"); modifier.object = rig
        obj.parent = rig


def consolidate(objects: list[bpy.types.Object], rig: bpy.types.Object,
                materials: list[bpy.types.Material], name: str) -> bpy.types.Object:
    for obj in objects:
        add_point_colors(obj)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    character = bpy.context.object
    character.name = name; character.data.name = f"{name}Mesh"
    polygon_materials = [character.material_slots[polygon.material_index].material.name for polygon in character.data.polygons]
    character.data.materials.clear()
    for value in materials:
        character.data.materials.append(value)
    index_by_name = {value.name: index for index, value in enumerate(materials)}
    for polygon, material_name in zip(character.data.polygons, polygon_materials):
        polygon.material_index = index_by_name[material_name]
        polygon.use_smooth = True
    character.parent = rig
    return character


def create_animation(rig: bpy.types.Object) -> None:
    action = bpy.data.actions.new("IdleBreathing")
    rig.animation_data_create(); rig.animation_data.action = action
    for frame, angle in ((1, -.012), (31, .016), (61, -.012)):
        bone = rig.pose.bones["Chest"]; bone.rotation_mode = "XYZ"; bone.rotation_euler.x = angle
        bone.keyframe_insert(data_path="rotation_euler", frame=frame)
    bpy.context.scene.frame_start = 1; bpy.context.scene.frame_end = 61; bpy.context.scene.render.fps = 30


def export_character(bundle: Path, destination: Path, female: bool) -> None:
    clear_scene(); destination.mkdir(parents=True, exist_ok=True)
    objects = load_base_objects(bundle, female)
    target_height = 1.78 if female else 2.04
    normalize_and_shape(objects, female, target_height)
    materials = material_set(female)
    assign_materials(objects[0], [], materials, female)
    features = create_face_features([], materials, female)
    hair_cards = [obj for obj in features if obj.name.startswith(("F2LongHair", "F2FaceHair", "M2ShortHair"))]
    expected_hair_cards = 56 if female else 48
    hair_triangles = sum(sum(len(polygon.vertices) - 2 for polygon in obj.data.polygons) for obj in hair_cards)
    rig = create_rig(female)
    extras = objects[1:] + features
    bind_to_rig(objects[0], extras, rig)
    character = consolidate([objects[0]] + extras, rig, materials, "RealisticFemaleCharacter" if female else "RealisticMaleCharacter")
    create_animation(rig)
    bpy.ops.object.select_all(action="DESELECT"); character.select_set(True); rig.select_set(True); bpy.context.view_layer.objects.active = rig
    filename = "realistic-female.gltf" if female else "realistic-male-character.gltf"
    bpy.ops.export_scene.gltf(filepath=str(destination / filename), export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=True, export_skins=True, export_tangents=True, export_yup=True,
                              export_materials="EXPORT", export_cameras=False, export_lights=False)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(destination / ("realistic-female.blend" if female else "realistic-male.blend")))
    triangles = sum(len(polygon.vertices) - 2 for polygon in character.data.polygons)
    print(f"CC0_CHARACTER={'F2' if female else 'M2'} VERTICES={len(character.data.vertices)} TRIANGLES={triangles} BONES={len(rig.data.bones)} MATERIALS={len(character.data.materials)} HAIR_CARDS={len(hair_cards)} HAIR_TRIANGLES={hair_triangles}")
    if (len(character.data.vertices) > 30000 or triangles > 60000 or len(rig.data.bones) != 20
            or len(character.data.materials) != 9 or len(hair_cards) != expected_hair_cards or hair_triangles > 900):
        raise RuntimeError("CC0 character geometry budget validation failed.")


def main() -> None:
    bundle, female_output, male_output = arguments()
    export_character(bundle, female_output, True)
    export_character(bundle, male_output, False)


main()
