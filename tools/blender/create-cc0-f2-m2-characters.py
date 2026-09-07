import math
import json
import struct
import sys
import heapq
import hashlib
from pathlib import Path

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import intersect_ray_tri

sys.path.insert(0, str(Path(__file__).resolve().parent))
from aevrynd_garment_pipeline import create_upper_garment_shell


def canonicalize_gltf_binary(gltf_path: Path) -> None:
    document = json.loads(gltf_path.read_text(encoding="utf-8"))
    if len(document.get("buffers", [])) != 1:
        raise RuntimeError("Character glTF export must contain exactly one binary buffer.")
    binary_path = gltf_path.parent / document["buffers"][0]["uri"]
    binary = bytearray(binary_path.read_bytes())

    def accessor_location(accessor_index: int) -> tuple[dict, int, int]:
        accessor = document["accessors"][accessor_index]
        view = document["bufferViews"][accessor["bufferView"]]
        offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        return accessor, offset, view.get("byteStride", 0)

    def canonicalize_unit_vectors(accessor_index: int, components: int) -> None:
        accessor, offset, byte_stride = accessor_location(accessor_index)
        expected_type = "VEC3" if components == 3 else "VEC4"
        if accessor["componentType"] != 5126 or accessor["type"] != expected_type:
            raise RuntimeError("Character glTF unit vector accessor is unsupported.")
        stride = byte_stride or components * 4
        for item in range(accessor["count"]):
            item_offset = offset + item * stride
            values = list(struct.unpack_from("<" + "f" * components, binary, item_offset))
            length = math.sqrt(sum(value * value for value in values[:3]))
            if length <= 0.0:
                raise RuntimeError("Character glTF contains a zero-length unit vector.")
            # 引擎最終以 signed-normalized 8-bit 儲存法線與切線；採同一量化格可精確消除 Blender 漂移。
            canonical = [round(value / length * 127.0) / 127.0 for value in values[:3]]
            if components == 4:
                canonical.append(1.0 if values[3] >= 0.0 else -1.0)
            struct.pack_into("<" + "f" * components, binary, item_offset, *canonical)

    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            attributes = primitive.get("attributes", {})
            canonicalize_unit_vectors(attributes["NORMAL"], 3)
            if "TANGENT" in attributes:
                canonicalize_unit_vectors(attributes["TANGENT"], 4)
            index_accessor, index_offset, index_stride = accessor_location(primitive["indices"])
            component_type = index_accessor["componentType"]
            if index_accessor["type"] != "SCALAR" or index_accessor["count"] % 3 != 0:
                raise RuntimeError("Character glTF indices must contain packed triangles.")
            index_format = {5123: "H", 5125: "I"}.get(component_type)
            if index_format is None:
                raise RuntimeError("Character glTF index component type is unsupported.")
            index_size = struct.calcsize(index_format)
            stride = index_stride or index_size
            indices = [struct.unpack_from("<" + index_format, binary, index_offset + index * stride)[0]
                       for index in range(index_accessor["count"])]
            triangles = []
            for start in range(0, len(indices), 3):
                triangle = indices[start:start + 3]
                rotations = [tuple(triangle[index:] + triangle[:index]) for index in range(3)]
                triangles.append(min(rotations))
            for index, value in enumerate(component for triangle in sorted(triangles) for component in triangle):
                struct.pack_into("<" + index_format, binary, index_offset + index * stride, value)

            weight_index = attributes.get("WEIGHTS_0")
            if weight_index is None:
                continue
            weight_accessor, weight_offset, weight_stride = accessor_location(weight_index)
            if weight_accessor["componentType"] != 5126 or weight_accessor["type"] != "VEC4":
                raise RuntimeError("Character glTF weights must use four floats.")
            stride = weight_stride or 16
            for vertex in range(weight_accessor["count"]):
                offset = weight_offset + vertex * stride
                weights = list(struct.unpack_from("<4f", binary, offset))
                total = sum(weights)
                if total <= 0.0:
                    raise RuntimeError("Character glTF contains an empty skin weight.")
                quantized = [math.floor(max(0.0, value) / total * 65535.0) for value in weights]
                largest = max(range(4), key=lambda index: (weights[index], -index))
                quantized[largest] += 65535 - sum(quantized)
                struct.pack_into("<4f", binary, offset, *(value / 65535.0 for value in quantized))

    binary_path.write_bytes(binary)


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
        create_material("DarkBrownHairPBR", (.35, .12, .04, 1), .50),
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
                # F2：以自然沙漏比例加強骨盆、腰線與肩背，避免只放大胸髖造成誇張輪廓。
                x_scale = (1.0 + .115 * math.exp(-((t - .52) / .115) ** 2)
                           - .105 * math.exp(-((t - .645) / .075) ** 2)
                           + .075 * math.exp(-((t - .79) / .105) ** 2))
                y_scale = (1.0 + .075 * math.exp(-((t - .52) / .12) ** 2)
                           + .035 * math.exp(-((t - .76) / .11) ** 2))
            else:
                # M2：胸背、三角肌與前臂分開塑形，腰髖維持精實而非方柱。
                x_scale = (1.0 + .17 * math.exp(-((t - .79) / .12) ** 2)
                           + .075 * math.exp(-((t - .69) / .10) ** 2)
                           - .045 * math.exp(-((t - .55) / .085) ** 2))
                y_scale = 1.0 + .135 * math.exp(-((t - .73) / .13) ** 2)
            vertex.co.x *= x_scale
            vertex.co.y *= y_scale
            absolute_x = abs(vertex.co.x)
            hand_start = .34 if female else .47
            if absolute_x > hand_start and t < .58:
                # 手指朝掌心形成輕微自然彎曲，修正筆直張開的 mannequin 手型。
                hand_weight = min(1.0, (absolute_x - hand_start) / (.13 if female else .16))
                vertex.co.y -= (.010 if female else .013) * hand_weight
                vertex.co.z += (.006 if female else .008) * hand_weight * hand_weight
            if t < .135:
                # 足弓與腳尖保留前後層次，避免角色落地時像尖錐或懸浮鞋楦。
                toe_weight = max(0.0, min(1.0, (-vertex.co.y - .015) / .18))
                vertex.co.z += (.010 if female else .012) * math.sin(toe_weight * math.pi)


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
        # M2 原始手掌較短，手指樞紐向內收 7cm，使骨點落在實際手部輪廓內。
        finger_base = arm[4][0] if female else arm[4][0] - .07
        finger_tip = ((finger_base + (.085 if female else .105)) * sign, -.055, arm[4][1] - (.055 if female else .07))
        add(f"{side}Fingers", (finger_base * sign, -.03, arm[4][1]), finger_tip, f"{side}Hand")
        add(f"{side}UpperLeg", (leg[0][0] * sign, 0, leg[0][1]), (leg[1][0] * sign, 0, leg[1][1]), "Hips")
        add(f"{side}LowerLeg", (leg[1][0] * sign, 0, leg[1][1]), (leg[2][0] * sign, 0, leg[2][1]), f"{side}UpperLeg")
        add(f"{side}Foot", (leg[2][0] * sign, 0, leg[2][1]), (leg[3][0] * sign, leg[3][1], leg[3][2]), f"{side}LowerLeg")
        add(f"{side}Toe", (leg[3][0] * sign, leg[3][1], leg[3][2]),
            (leg[3][0] * sign, leg[3][1] - (.10 if female else .13), leg[3][2] - .012), f"{side}Foot")
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


def scalp_cap(name: str, location: tuple[float, float, float], scale: tuple[float, float, float],
              surface: bpy.types.Material) -> bpy.types.Object:
    obj = sphere_part(name, location, scale, surface, True)
    mesh = obj.data
    # 髮帽只保留頭頂與後腦，前額和耳側交由髮片打破球形安全帽輪廓。
    remove = []
    for polygon in mesh.polygons:
        center = sum((mesh.vertices[index].co for index in polygon.vertices), Vector()) / len(polygon.vertices)
        if center.z < -scale[2] * .18 or (center.y < -scale[1] * .58 and center.z < scale[2] * .42):
            remove.append(polygon.index)
    if remove:
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="DESELECT")
        bpy.ops.object.mode_set(mode="OBJECT")
        for index in remove:
            mesh.polygons[index].select = True
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.delete(type="FACE")
        bpy.ops.object.mode_set(mode="OBJECT")
        obj.select_set(False)
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
        for layer in range(5):
            for column in range(14):
                angle = -1.48 + column * (2.96 / 13.0) + layer * .018
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
            for layer in range(3):
                root = (side * (.070 + layer * .014), -.058 + layer * .006, 1.735 - layer * .012)
                middle = (side * (.098 + layer * .010), -.070, 1.565 - layer * .025)
                tip = (side * (.115 + layer * .012), -.030, 1.355 - layer * .035)
                cards.append(hair_card(f"F2FaceHair.{len(cards):02d}", (root, middle, tip),
                                       .014 + layer * .002, len(cards), hair_material))
    else:
        # M2 以四圈短髮建立漸短側邊，上方髮束略向一側梳起，保持成熟俐落輪廓。
        for band in range(5):
            for column in range(12):
                angle = math.tau * column / 12.0 + band * .038
                root = (math.sin(angle) * (.112 - band * .006),
                        -.070 + math.cos(angle) * (.098 - band * .005),
                        2.038 + band * .014)
                swept_angle = angle + .25 + band * .035
                tip = (math.sin(swept_angle) * (.090 - band * .004) + .012,
                       -.072 + math.cos(swept_angle) * (.084 - band * .004),
                       root[2] + .010 + band * .003)
                middle = ((root[0] + tip[0]) * .5 + .006,
                          (root[1] + tip[1]) * .5,
                          (root[2] + tip[2]) * .5 + .010)
                cards.append(hair_card(f"M2ShortHair.{len(cards):02d}", (root, middle, tip),
                                       .014 + (column % 3) * .0015, len(cards), hair_material))
    return cards


def remap_male_hair_card_uvs(body: bpy.types.Object) -> dict:
    """將 M2 髮片移出不透明頭皮區，同時維持唯讀來源不變。"""
    hair_slot = next((index for index, material in enumerate(body.data.materials)
                      if material and material.name == "DarkBrownHairPBR"), None)
    uv_layer = body.data.uv_layers.active
    if hair_slot is None or uv_layer is None:
        raise RuntimeError("M2 hair material or UV layer is unavailable.")

    hair_polygons = {polygon.index for polygon in body.data.polygons
                     if polygon.material_index == hair_slot}
    by_vertex: dict[int, list[int]] = {}
    for polygon_index in hair_polygons:
        for vertex_index in body.data.polygons[polygon_index].vertices:
            by_vertex.setdefault(vertex_index, []).append(polygon_index)

    components: list[list[int]] = []
    remaining = set(hair_polygons)
    while remaining:
        seed = min(remaining); remaining.remove(seed); component = []; pending = [seed]
        while pending:
            polygon_index = pending.pop(); component.append(polygon_index)
            for vertex_index in body.data.polygons[polygon_index].vertices:
                for neighbour in by_vertex[vertex_index]:
                    if neighbour in remaining:
                        remaining.remove(neighbour); pending.append(neighbour)
        components.append(sorted(component))

    cards = []
    for component in components:
        vertices = {vertex_index for polygon_index in component
                    for vertex_index in body.data.polygons[polygon_index].vertices}
        if len(component) == 2 and len(vertices) == 6 and all(
                len(body.data.polygons[index].vertices) == 4 for index in component):
            cards.append((min(vertices), component))
    cards.sort(key=lambda value: value[0])
    if len(cards) != 60:
        raise RuntimeError(f"Expected 60 M2 hair-card islands, observed {len(cards)}.")

    tile_usage = set()
    card_polygons = {polygon_index for _, component in cards for polygon_index in component}
    card_loops = 0
    for card_index, (_, component) in enumerate(cards):
        tile = card_index % 6; tile_usage.add(tile)
        tile_min = .25 + tile * .125
        for polygon_index in component:
            for loop_index in body.data.polygons[polygon_index].loop_indices:
                source_u = uv_layer.data[loop_index].uv.x
                local_u = .125 if source_u >= .0625 else 0.0
                uv_layer.data[loop_index].uv.x = tile_min + local_u
                card_loops += 1

    card_uvs = [uv_layer.data[loop_index].uv.x for polygon_index in card_polygons
                for loop_index in body.data.polygons[polygon_index].loop_indices]
    if tile_usage != set(range(6)) or not card_uvs or min(card_uvs) < .25 or max(card_uvs) > 1.0:
        raise RuntimeError("M2 hair-card strand atlas remap failed.")
    scalp_band_loops = sum(
        uv_layer.data[loop_index].uv.x <= .1250001
        for polygon_index in hair_polygons - card_polygons
        for loop_index in body.data.polygons[polygon_index].loop_indices)
    if scalp_band_loops == 0:
        raise RuntimeError("M2 opaque scalp band was lost during hair-card remap.")
    return {"cardIslands": len(cards), "cardLoops": card_loops,
            "strandTiles": sorted(tile_usage), "minimumCardU": min(card_uvs),
            "maximumCardU": max(card_uvs), "scalpBandLoops": scalp_band_loops}


def create_face_features(eyes: list[bpy.types.Object], materials: list[bpy.types.Material], female: bool) -> list[bpy.types.Object]:
    iris_material = materials[3 if female else 4]
    hair_material = materials[4 if female else 2]
    sclera_material = materials[2 if female else 3]
    features: list[bpy.types.Object] = []
    eye_x, eye_y, eye_z = ((.034, -.168, 1.665) if female else (.039, -.220, 1.902))
    for index, sign in enumerate((1.0, -1.0)):
        features.append(sphere_part(f"EyeSclera.{index}", (eye_x * sign, eye_y, eye_z),
                                    (.019 if female else .020, .009, .010), sclera_material))
        features.append(sphere_part(f"Iris.{index}", (eye_x * sign, eye_y - .0085, eye_z),
                                    (.0065 if female else .0070, .0022, .0065 if female else .0070), iris_material))
        features.append(sphere_part(f"Pupil.{index}", (eye_x * sign, eye_y - .0105, eye_z),
                                    (.0028, .0012, .0028), hair_material, True))
        features.append(sphere_part(f"Brow.{index}", (eye_x * sign, eye_y + .004, eye_z + (.028 if female else .031)),
                                    (.028 if female else .032, .004, .0045), hair_material, True))
    lip_z = 1.604 if female else 1.824
    # 正式 AESKIN 的負 Z 是臉部前方；此處以 Blender 負 Y 保留約 8–13mm 的局部深度餘裕。
    lip_y = -.181 if female else -.232
    features.append(sphere_part("UpperLip", (0, lip_y, lip_z + .004),
                                (.029 if female else .032, .004, .006), materials[1]))
    features.append(sphere_part("LowerLip", (0, lip_y - .001, lip_z - .005),
                                (.027 if female else .030, .0045, .0065), materials[1]))
    if female:
        # F2 的長髮先以貼頭髮帽建立乾淨髮際線；後續髮片可在不改身體拓撲下持續增加。
        features.append(scalp_cap("F2HairScalp", (0, -.004, 1.700), (.116, .120, .116), hair_material))
    else:
        # M2 採短髮輪廓，避免舊版寬亮面髮帽造成卡通感。
        features.append(scalp_cap("M2HairScalp", (0, -.094, 1.990), (.126, .128, .104), hair_material))
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
    expected_hair_cards = 76 if female else 60
    hair_triangles = sum(sum(len(polygon.vertices) - 2 for polygon in obj.data.polygons) for obj in hair_cards)
    rig = create_rig(female)
    extras = objects[1:] + features
    bind_to_rig(objects[0], extras, rig)
    garment, garment_metrics = create_upper_garment_shell(objects[0], rig, materials[5], female)
    character = consolidate([objects[0], garment] + extras, rig, materials, "RealisticFemaleCharacter" if female else "RealisticMaleCharacter")
    create_animation(rig)
    bpy.ops.object.select_all(action="DESELECT"); character.select_set(True); rig.select_set(True); bpy.context.view_layer.objects.active = rig
    filename = "realistic-female.gltf" if female else "realistic-male-character.gltf"
    bpy.ops.export_scene.gltf(filepath=str(destination / filename), export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=True, export_skins=True, export_tangents=True, export_yup=True,
                              export_materials="EXPORT", export_cameras=False, export_lights=False)
    # Blender 可在不同程序中改變面索引順序及極少數自動權重末位；寫檔後正規化以保證逐位元重現。
    canonicalize_gltf_binary(destination / filename)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(destination / ("realistic-female.blend" if female else "realistic-male.blend")))
    triangles = sum(len(polygon.vertices) - 2 for polygon in character.data.polygons)
    print(f"CC0_CHARACTER={'F2' if female else 'M2'} VERTICES={len(character.data.vertices)} TRIANGLES={triangles} BONES={len(rig.data.bones)} MATERIALS={len(character.data.materials)} HAIR_CARDS={len(hair_cards)} HAIR_TRIANGLES={hair_triangles} GARMENT_FACES={garment_metrics.source_faces} GARMENT_VERTICES={garment_metrics.outer_vertices} GARMENT_RIM={garment_metrics.rim_vertices} GARMENT_BOUNDARY={garment_metrics.boundary_edges} COLLAR_VERTICES={garment_metrics.collar_vertices} GARMENT_TRIANGLES={garment_metrics.triangles}")
    if (len(character.data.vertices) > 30000 or triangles > 60000 or len(rig.data.bones) != 24
            or len(character.data.materials) != 9 or len(hair_cards) != expected_hair_cards or hair_triangles > 900
            or garment_metrics.source_faces < 1000 or garment_metrics.boundary_edges < 100
            or garment_metrics.collar_vertices < 32 or garment_metrics.triangles > 4000):
        raise RuntimeError("CC0 character geometry budget validation failed.")


def inspect_hand_branches(body: bpy.types.Object, female: bool, side: str, source_vertex_limit: int | None = None) -> dict:
    # 以原始邊連通與腕部測地距離辨識分支；不從包圍盒或自動權重猜指頭。
    sign = 1.0 if side == "Left" else -1.0
    vertices = [v.co.copy() for v in body.data.vertices]
    top = 1.02 if female else 1.16
    selected = {i for i, p in enumerate(vertices)
                if (source_vertex_limit is None or i < source_vertex_limit)
                and p.x * sign > (.32 if female else .39) and (.6 if female else .7) < p.z < top}
    adjacency = {i: [] for i in selected}
    for edge in body.data.edges:
        a, b = edge.vertices
        if a in selected and b in selected:
            length = (vertices[a] - vertices[b]).length
            adjacency[a].append((b, length)); adjacency[b].append((a, length))

    def distances(seeds: list[int]) -> dict[int, float]:
        result = {i: math.inf for i in selected}
        queue = []
        for i in seeds:
            result[i] = 0.0; heapq.heappush(queue, (0.0, i))
        while queue:
            value, i = heapq.heappop(queue)
            if value != result[i]:
                continue
            for j, length in adjacency[i]:
                if value + length < result[j]:
                    result[j] = value + length; heapq.heappush(queue, (value + length, j))
        return result

    distance = distances([i for i in selected if vertices[i].z > top - .015])
    if not selected or any(not math.isfinite(d) for d in distance.values()):
        raise RuntimeError("Hand topology is disconnected from the wrist seed.")
    parent, peak, events, winner_merge = {}, {}, [], {}

    def find(i: int) -> int:
        while parent[i] != i:
            parent[i] = parent[parent[i]]; i = parent[i]
        return i

    for i in sorted(distance, key=lambda k: (distance[k], k), reverse=True):
        parent[i] = i; peak[i] = i
        roots = {find(j) for j, _ in adjacency[i] if j in parent}
        for r in sorted(roots, key=lambda k: (distance[peak[k]], k), reverse=True):
            a, b = find(i), find(r)
            if a == b:
                continue
            if distance[peak[a]] < distance[peak[b]]:
                a, b = b, a
            prominence = distance[peak[b]] - distance[i]
            if prominence > .025:
                events.append({"tipIndex": peak[b], "length": prominence})
                winner_merge.setdefault(peak[a], distance[peak[a]] - distance[i])
            parent[b] = a
    roots = {find(i) for i in parent}
    if len(roots) != 1:
        raise RuntimeError("Hand branch analysis did not produce one palm component.")
    dominant = peak[next(iter(roots))]
    if dominant not in winner_merge:
        raise RuntimeError("Hand dominant branch has no independently observed saddle.")
    events.append({"tipIndex": dominant, "length": winner_merge[dominant]})
    if len(events) != 5:
        raise RuntimeError(f"Expected five significant hand branches, observed {len(events)}.")
    thumb = min(events, key=lambda e: abs(vertices[e["tipIndex"]].x))
    ordered = sorted((e for e in events if e is not thumb), key=lambda e: vertices[e["tipIndex"]].y)
    if min(abs(vertices[e["tipIndex"]].x) for e in ordered) - abs(vertices[thumb["tipIndex"]].x) < .025:
        raise RuntimeError("Thumb opposition cannot be identified from the source topology.")
    entries = dict(zip(("Thumb", "Index", "Middle", "Ring", "Little"), [thumb] + ordered))
    all_distances = {name: distances([entry["tipIndex"]]) for name, entry in entries.items()}
    for name, entry in entries.items():
        # 每指區域止於觀測到的分支鞍點，交界不憑最近骨頭自動擴散到其他指。
        local = all_distances[name]
        domain_length = entry["length"] * (1.5 if name == "Thumb" else 1.2)
        domain = [i for i in selected if local[i] <= domain_length
                  and name == min(entries, key=lambda other: all_distances[other][i])]
        if len(domain) < 24:
            raise RuntimeError(f"Insufficient source vertices for {side}{name}.")
        centers = []
        parts = ("Metacarpal", "Proximal", "Distal") if name == "Thumb" else ("Proximal", "Intermediate", "Distal")
        for fraction in ((1.45, .95, .48, .04) if name == "Thumb" else (.95, .64, .33, .04)):
            target = entry["length"] * fraction
            nearest = sorted(domain, key=lambda i: abs(local[i] - target))[:8]
            centers.append(sum((vertices[i] for i in nearest), Vector()) / len(nearest))
        if any((b-a).length < .008 for a, b in zip(centers, centers[1:])):
            raise RuntimeError(f"Degenerate source-derived finger chain: {side}{name}; lengths={[ (b-a).length for a,b in zip(centers,centers[1:])]}; prominence={entry['length']}.")
        entry.update({"sourceVertices": domain, "domainLength": domain_length, "parts": parts, "centers": [list(p) for p in centers],
                      "distances": {i: local[i] for i in domain}})
    return {"side": side, "branches": entries, "sourceHandVertices": sorted(selected)}


def triangle_shaft_clearance(points: list[Vector], center: Vector, axis: Vector,
                             radius: float = .02, half_length: float = .08) -> float:
    # 將三角形裁入有限柄的兩端平面，再對投影凸多邊形精確最小化徑向距離。
    # 頂點、邊內點與面內點都納入，不能以三個頂點未穿透代替整片面未穿透。
    polygon = [point-center for point in points]
    for sign in (1.0, -1.0):
        clipped = []
        for a,b in zip(polygon,polygon[1:]+polygon[:1]):
            fa = half_length-sign*a.dot(axis); fb = half_length-sign*b.dot(axis)
            if fa >= 0:
                clipped.append(a)
            if (fa < 0) != (fb < 0):
                clipped.append(a+(b-a)*(fa/(fa-fb)))
        polygon = clipped
        if not polygon:
            return math.inf
    radial = [point-axis*point.dot(axis) for point in polygon]
    minimum = math.inf
    for a,b in zip(radial,radial[1:]+radial[:1]):
        edge = b-a
        parameter = min(1.0,max(0.0,-a.dot(edge)/edge.length_squared)) if edge.length_squared > 1e-20 else 0.0
        minimum = min(minimum,(a+edge*parameter).length)
    area = sum(axis.dot(a.cross(b)) for a,b in zip(radial,radial[1:]+radial[:1]))
    if abs(area) > 1e-12:
        signs = [axis.dot((b-a).cross(-a)) for a,b in zip(radial,radial[1:]+radial[:1])]
        if min(signs) >= -1e-12 or max(signs) <= 1e-12:
            minimum = 0.0
    return minimum-radius


def pose_hand_grip_prototype(body: bpy.types.Object, rig: bpy.types.Object, hands: list[dict]) -> None:
    # 柄軸由四指根排列決定；這是資產原型，不假稱符合舊版沿手指方向的武器 socket。
    rig.animation_data_create(); rig.animation_data.action = bpy.data.actions.new("GripPrototype")
    body.data.calc_loop_triangles()
    source_triangles = [tuple(t.vertices) for t in body.data.loop_triangles]
    for hand in hands:
        if hand["side"] != "Right":
            hand["scope"] = "OUT_OF_SCOPE_NON_GRIPPING_BIND_POSE"
            continue
        hand["scope"] = "RIGHT_HAND_SWORD_GRIP"
        branches = hand["branches"]
        roots = [Vector(branches[d]["centers"][0]) for d in ("Index", "Middle", "Ring", "Little")]
        center = sum(roots, Vector()) / 4
        axis = (roots[-1]-roots[0]).normalized()
        along = sum((Vector(branches[d]["centers"][-1])-Vector(branches[d]["centers"][0])
                     for d in ("Index", "Middle", "Ring", "Little")), Vector()).normalized()
        normal = axis.cross(along).normalized()
        thumb_offset = Vector(branches["Thumb"]["centers"][0])-center
        if normal.dot(thumb_offset) < 0:
            normal.negate()
        palm_center = center.copy()
        mcp_vertices = {i for digit, entry in branches.items() if digit != "Thumb"
                        for i in entry["sourceVertices"] if entry["distances"][i] >= entry["length"]*.8}
        for placement_step in range(23):
            center = palm_center + normal*(.028+placement_step*.001) + along*.012
            minimum_mcp = min(((body.data.vertices[i].co-center)-axis*(body.data.vertices[i].co-center).dot(axis)).length
                              for i in mcp_vertices)
            if minimum_mcp >= .021:
                break
        if minimum_mcp < .021:
            raise RuntimeError("No handle placement clears the source MCP skin within the bounded palm offset.")
        hand["handle"] = {"center": list(center), "axis": list(axis), "radius": .02,
                          "length": .16, "matchesLegacySwordSocket": False,
                          "sourceMcpClearance": minimum_mcp-.02, "palmNormalOffset": .028+placement_step*.001}
        for digit, entry in branches.items():
            bones = [rig.pose.bones[hand["side"]+digit+part] for part in entry["parts"]]
            original_basis = [bone.matrix_basis.copy() for bone in bones]
            for bone in bones:
                bone.rotation_mode = "QUATERNION"; bone.keyframe_insert(data_path="rotation_quaternion", frame=1)
            best = None
            digit_vertices = set(entry["sourceVertices"])
            digit_triangles = [tri for tri in source_triangles if any(i in digit_vertices for i in tri)]
            # 用實際蒙皮厚度限制接觸；搜尋的是關節方向的解，不修改半徑、骨長或頂點位置。
            for step in range(193):
                requested_radius = .018 + step*.00025
                for bone, basis in zip(bones, original_basis):
                    bone.matrix_basis = basis
                bpy.context.view_layer.update()
                if digit == "Thumb":
                    root = bones[0].head.copy(); offset = root-center; axial = axis*offset.dot(axis)
                    radial = offset-axial; target = center+axial+radial.normalized()*requested_radius
                    # 三段 CCD 納入掌骨對掌旋轉，所有關節位置只由父骨旋轉決定。
                    for iteration in range(32):
                        for bone in reversed(bones):
                            head = bone.head.copy(); current = bones[-1].tail-head; desired = target-head
                            if min(current.length, desired.length) < 1e-6:
                                continue
                            delta = current.rotation_difference(desired)
                            if delta.angle > .25:
                                delta = delta.slerp(type(delta)(), 1-.25/delta.angle)
                            bone.matrix = Matrix.Translation(head) @ (delta @ bone.matrix.to_quaternion()).to_matrix().to_4x4()
                            bpy.context.view_layer.update()
                        if (bones[-1].tail-target).length < .0001:
                            break
                else:
                    for bone in bones:
                        head = bone.head.copy(); radial = head-center-axis*(head-center).dot(axis)
                        radius = radial.length
                        if radius < .005:
                            raise RuntimeError("Finger joint starts at the cylinder axis.")
                        tangent = axis.cross(radial).normalized()
                        forward = Vector(entry["centers"][-1])-Vector(entry["centers"][0])
                        if tangent.dot(forward) < 0:
                            tangent.negate()
                        half_angle = min(.75, bone.length/(2*radius))
                        next_radial = radial.normalized()*math.cos(2*half_angle)+tangent*math.sin(2*half_angle)
                        target = center+axis*(head-center).dot(axis)+next_radial*requested_radius
                        rotation = bone.matrix.to_quaternion()
                        delta = (rotation @ Vector((0,1,0))).rotation_difference((target-head).normalized())
                        bone.matrix = Matrix.Translation(head) @ (delta @ rotation).to_matrix().to_4x4()
                        bpy.context.view_layer.update()
                evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get()); mesh = evaluated.to_mesh()
                try:
                    minimum = .02+min(triangle_shaft_clearance([mesh.vertices[i].co for i in tri],center,axis)
                                      for tri in digit_triangles)
                finally:
                    evaluated.to_mesh_clear()
                score = abs(minimum-.0205) + (1.0 if minimum < .02 else 0.0)
                if best is None or score < best[0]:
                    best = (score, [b.matrix_basis.copy() for b in bones], minimum, requested_radius)
                if .02 <= minimum <= .021:
                    break
            for bone, basis in zip(bones, best[1]):
                bone.matrix_basis = basis
            bpy.context.view_layer.update()
            entry["contactSolver"] = {"iterations": step+1, "requestedCenterlineRadius": best[3],
                                      "minimumSurfaceRadius": best[2], "passed": .02 <= best[2] <= .021}
            for bone in bones:
                bone.keyframe_insert(data_path="rotation_quaternion", frame=31)
    bpy.context.scene.frame_set(31)
    bpy.context.scene.frame_start = 1; bpy.context.scene.frame_end = 31
    for layer in rig.animation_data.action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for keyframe in curve.keyframe_points:
                        keyframe.interpolation = "LINEAR"
    bpy.context.view_layer.update()
    evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    try:
        mesh.calc_loop_triangles()
        for hand in hands:
            if hand["side"] != "Right":
                continue
            center = Vector(hand["handle"]["center"]); axis = Vector(hand["handle"]["axis"])
            domain = set(hand["sourceHandVertices"])
            for digit, entry in hand["branches"].items():
                distances = []
                finite_distances = []
                for i in entry["sourceVertices"]:
                    offset = mesh.vertices[i].co-center
                    radial_distance = (offset-axis*offset.dot(axis)).length
                    distances.append(radial_distance)
                    radial_gap, cap_gap = radial_distance-.02, abs(offset.dot(axis))-.08
                    finite_distances.append(math.hypot(max(radial_gap,0),max(cap_gap,0))+min(max(radial_gap,cap_gap),0))
                entry["gripRadialDistance"] = {"minimum": min(distances), "maximum": max(distances),
                                               "minimumSourceVertex": entry["sourceVertices"][distances.index(min(distances))],
                                               "insideCylinderVertices": sum(d < .02 for d in distances),
                                               "maximumPenetration": max(0.0, .02-min(distances)),
                                               "minimumSurfaceGap": max(0.0, min(distances)-.02)}
                entry["finiteHandleVertexContact"] = {"minimumSignedDistance": min(finite_distances),
                                                       "insideVertices": sum(d<0 for d in finite_distances),
                                                       "passed": 0 <= min(finite_distances) <= .001,
                                                       "triangleSurfaceVerified": False}
                digit_vertices = set(entry["sourceVertices"])
                digit_triangles = [tri for tri in source_triangles if any(i in digit_vertices for i in tri)]
                clearance = min(triangle_shaft_clearance([mesh.vertices[i].co for i in tri],center,axis) for tri in digit_triangles)
                entry["continuousShaftContact"] = {"minimumClearance": clearance,
                                                   "trianglesChecked": len(digit_triangles),
                                                   "passed": math.isfinite(clearance) and 0 <= clearance <= .001}
                bones = [rig.pose.bones[hand["side"]+digit+part] for part in entry["parts"]]
                points = [bone.head.copy() for bone in bones]+[bones[-1].tail.copy()]
                radial = [(p-center)-axis*(p-center).dot(axis) for p in points]
                forward = Vector(entry["centers"][-1])-Vector(entry["centers"][0])
                direction = 1 if axis.cross(radial[0]).dot(forward) >= 0 else -1
                increments = [direction*math.atan2(axis.dot(a.cross(b)), a.dot(b)) for a,b in zip(radial,radial[1:])]
                alignments = [direction*axis.cross(a).normalized().dot((q-p).normalized())
                              for a,p,q in zip(radial,points,points[1:])]
                entry["wrapEvidence"] = {"jointArcDegrees": [math.degrees(a) for a in increments],
                                         "totalArcDegrees": math.degrees(sum(increments)),
                                         "tangentAlignments": alignments,
                                         "passed": digit != "Thumb" and min(increments) > 0 and sum(increments) >= math.pi/2 and min(alignments) > .2,
                                         "tipRadialDirection": list(radial[-1].normalized())}
            thumb_radial = Vector(hand["branches"]["Thumb"]["wrapEvidence"]["tipRadialDirection"])
            finger_radial = sum((Vector(e["wrapEvidence"]["tipRadialDirection"]) for digit,e in hand["branches"].items() if digit != "Thumb"), Vector()).normalized()
            opposition = thumb_radial.dot(finger_radial)
            hand["branches"]["Thumb"]["wrapEvidence"]["oppositionDot"] = opposition
            hand["branches"]["Thumb"]["wrapEvidence"]["passed"] = opposition < -.2
            triangles = [tuple(t.vertices) for t in mesh.loop_triangles if all(i in domain for i in t.vertices)]
            clearances = [triangle_shaft_clearance([mesh.vertices[i].co for i in tri],center,axis) for tri in triangles]
            hand["continuousShaftContact"] = {"minimumClearance": min(clearances),
                                              "trianglesChecked": len(triangles),
                                              "penetratingTriangles": sum(d < 0 for d in clearances),
                                              "passed": min(clearances) >= 0 and all(e["continuousShaftContact"]["passed"] for e in hand["branches"].values())}
            hand["degenerateTriangles"] = sum((mesh.vertices[b].co-mesh.vertices[a].co).cross(
                                               mesh.vertices[c].co-mesh.vertices[a].co).length < 1e-10
                                              for a,b,c in triangles)
            tree = BVHTree.FromPolygons([v.co for v in mesh.vertices], triangles, all_triangles=True)
            candidates = [(a,b) for a,b in tree.overlap(tree) if a < b and not set(triangles[a]) & set(triangles[b])]
            intersections = 0; unresolved = 0
            for first, second in candidates:
                a = [mesh.vertices[i].co for i in triangles[first]]; b = [mesh.vertices[i].co for i in triangles[second]]
                normal_a = (a[1]-a[0]).cross(a[2]-a[0]); normal_b = (b[1]-b[0]).cross(b[2]-b[0])
                if min(normal_a.length, normal_b.length) < 1e-10:
                    unresolved += 1; continue
                if abs(normal_a.normalized().dot(normal_b.normalized())) > .999999:
                    unresolved += 1; continue
                hit = False
                for source, target in ((a,b),(b,a)):
                    for p,q in zip(source,source[1:]+source[:1]):
                        result = intersect_ray_tri(*target, q-p, p, True)
                        if result is not None and 1e-6 < (result-p).dot(q-p)/(q-p).length_squared < 1-1e-6:
                            hit = True
                intersections += int(hit)
            hand["selfIntersectionCandidates"] = len(candidates)
            hand["selfIntersectionTrianglesChecked"] = len(triangles)
            hand["selfIntersections"] = intersections; hand["selfIntersectionUnresolvedPairs"] = unresolved
            hand["selfIntersectionVerified"] = unresolved == 0
            hand["gripContactPassed"] = all(.02 <= e["gripRadialDistance"]["minimum"] <= .021
                                            for e in hand["branches"].values())
            hand["wrapPassed"] = all(e["wrapEvidence"]["passed"] for e in hand["branches"].values())
            hand["finiteHandleContactPassed"] = all(e["finiteHandleVertexContact"]["passed"] for e in hand["branches"].values())
    finally:
        evaluated.to_mesh_clear()


def preview_hand_grip(body: bpy.types.Object, hands: list[dict], destination: Path, sex: str) -> None:
    hand = next(h for h in hands if h["side"] == "Right")
    center = Vector(hand["handle"]["center"])
    bpy.context.view_layer.update()
    evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get()); mesh = evaluated.to_mesh()
    try:
        vertices = [v.co.copy() for v in mesh.vertices]
        # 只裁掉遠端前臂以便檢查原型，不影響實際姿勢或任何引擎擷取。
        faces = [list(p.vertices) for p in mesh.polygons if all((vertices[i]-center).length < .22 for i in p.vertices)]
        preview_mesh = bpy.data.meshes.new("GripPreview"); preview_mesh.from_pydata(vertices, [], faces); preview_mesh.update()
    finally:
        evaluated.to_mesh_clear()
    preview = bpy.data.objects.new("GripPreview", preview_mesh); bpy.context.collection.objects.link(preview); body.hide_render = True
    preview_mesh.materials.append(create_material("GripPreviewSkin", (.55,.35,.22,1), .65))
    for p in preview_mesh.polygons:
        p.use_smooth = True
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=.02, depth=.16, location=center)
    cylinder = bpy.context.object; cylinder.name = "GripReferenceCylinder"
    cylinder.rotation_euler = Vector((0,0,1)).rotation_difference(Vector(hand["handle"]["axis"])).to_euler()
    cylinder.data.materials.append(create_material("GripReference", (.06,.12,.20,1), .6))
    lamp = bpy.data.lights.new("GripPreviewLight", "AREA"); lamp.energy = 150; lamp.size = .8
    light = bpy.data.objects.new("GripPreviewLight", lamp); bpy.context.collection.objects.link(light)
    light.location = center+Vector((-.5,-.5,.5)); light.rotation_euler = (center-light.location).to_track_quat('-Z','Y').to_euler()
    cam = bpy.data.cameras.new("GripPreviewCamera"); camera = bpy.data.objects.new("GripPreviewCamera", cam); bpy.context.collection.objects.link(camera)
    cam.type = "ORTHO"; cam.ortho_scale = .32
    scene = bpy.context.scene; scene.camera = camera; scene.render.engine = "CYCLES"; scene.cycles.device = "CPU"; scene.cycles.samples = 16
    scene.render.resolution_x = 640; scene.render.resolution_y = 640; scene.render.resolution_percentage = 100
    for name, offset in (("front", (0,-1,0)), ("right", (-1,0,0))):
        camera.location = center+Vector(offset); camera.rotation_euler = (center-camera.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath = str(destination / (sex+"-grip-"+name+".png")); bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(preview, do_unlink=True); body.hide_render = False


def hand_rig_prototype(bundle: Path, destination: Path) -> bool:
    # 原型只允許指定 bld 子目錄；不會覆寫正式模型或使用者 Blender 檔案。
    allowed = Path(r"D:\Download\Aevrynd\bld\hand-rig-prototype-20260907").resolve()
    destination = destination.resolve()
    if destination != allowed and allowed not in destination.parents:
        raise RuntimeError("Hand prototype output must stay inside its dedicated bld directory.")
    destination.mkdir(parents=True, exist_ok=True)
    test_axis = Vector((0,0,1)); test_center = Vector()
    # 負例：三頂點皆在柱外，但三角形內部跨越圓柱，必須判為穿透。
    if triangle_shaft_clearance([Vector((.03,0,0)),Vector((-.015,.026,0)),Vector((-.015,-.026,0))],test_center,test_axis) >= 0:
        raise RuntimeError("Continuous triangle/cylinder interior rejection self-test failed.")
    if triangle_shaft_clearance([Vector((.021,-.01,0)),Vector((.021,.01,0)),Vector((.021,0,.03))],test_center,test_axis) < 0:
        raise RuntimeError("Continuous triangle/cylinder clear face self-test failed.")
    if not math.isinf(triangle_shaft_clearance([Vector((0,0,.09)),Vector((.01,0,.09)),Vector((0,.01,.09))],test_center,test_axis)):
        raise RuntimeError("Finite cylinder end-plane clipping self-test failed.")
    all_accepted = True
    for female in (True, False):
        clear_scene()
        body = load_base_objects(bundle, female)[0]
        normalize_and_shape([body], female, 1.78 if female else 2.04)
        original = [tuple(v.co) for v in body.data.vertices]
        report = {"schemaVersion": 1, "units": "meters", "sex": "female" if female else "male",
                  "accepted": False, "sourceGeometryUnchanged": False, "hands": []}
        report_path = destination / (report["sex"] + "-grip-report.json")
        try:
            hands = [inspect_hand_branches(body, female, side) for side in ("Left", "Right")]
            rig = create_rig(female)
            bpy.context.view_layer.objects.active = rig; rig.select_set(True)
            bpy.ops.object.mode_set(mode="EDIT")
            for hand in hands:
                side = hand["side"]
                rig.data.edit_bones[side + "Fingers"].use_deform = True
                for digit, entry in hand["branches"].items():
                    points = [Vector(p) for p in entry["centers"]]
                    previous = rig.data.edit_bones[side + "Fingers"]
                    for index, (head, tail) in enumerate(zip(points, points[1:])):
                        bone = rig.data.edit_bones.new(f"{side}{digit}{entry['parts'][index]}")
                        bone.head = head; bone.tail = tail; bone.parent = previous
                        bone.use_deform = False
                        bone.use_connect = index > 0; bone.align_roll(Vector((0, 1, 0)))
                        previous = bone
            bpy.ops.object.mode_set(mode="OBJECT")
            bind_to_rig(body, [], rig)
            # 新指骨僅綁定已驗證來源分支；保留腕與掌原有蒙皮，不允許自動權重跨指污染。
            digit_names = [f"{hand['side']}{digit}{part}" for hand in hands
                           for digit, entry in hand["branches"].items() for part in entry["parts"]]
            for name in digit_names:
                rig.data.bones[name].use_deform = True
                group = body.vertex_groups.get(name) or body.vertex_groups.new(name=name)
                group.remove(range(len(body.data.vertices)))
            for hand in hands:
                for digit, entry in hand["branches"].items():
                    names = [hand["side"] + digit + part for part in entry["parts"]]
                    for vertex in entry["sourceVertices"]:
                        fraction = min(1.0, max(0.0, entry["distances"][vertex] / (entry["domainLength"] if digit == "Thumb" else entry["length"])))
                        coordinate = min(float(len(names)-1), max(0.0, (1-fraction) * len(names)-.5))
                        lower = int(coordinate); upper = min(len(names)-1, lower+1); blend = coordinate-lower
                        root_fraction = entry["distances"][vertex]/entry["domainLength"]
                        root_blend = min(1.0, max(0.0, (1-root_fraction)/.2))
                        for group in body.vertex_groups:
                            group.remove([vertex])
                        body.vertex_groups[names[lower]].add([vertex], (1-blend)*root_blend, "REPLACE")
                        if upper != lower and blend > 0:
                            body.vertex_groups[names[upper]].add([vertex], blend*root_blend, "REPLACE")
                        if root_blend < 1:
                            body.vertex_groups[hand["side"]+"Hand"].add([vertex], 1-root_blend, "REPLACE")
                    entry["jointAxes"] = []
                    entry["weights"] = []
                    for name in names:
                        bone = rig.data.bones[name]
                        entry["jointAxes"].append({"name": name, "head": list(bone.head_local), "tail": list(bone.tail_local),
                                                  "bendAxis": list(bone.matrix_local.to_3x3().col[0])})
                        group = body.vertex_groups[name]
                        values = [next((g.weight for g in v.groups if g.group == group.index), 0.0) for v in body.data.vertices]
                        dominant = sum(value > 0 and value >= max((g.weight for g in body.data.vertices[i].groups), default=0)
                                       for i, value in enumerate(values))
                        entry["weights"].append({"name": name, "nonzero": sum(v > 0 for v in values), "dominant": dominant})
                        if not dominant:
                            raise RuntimeError("Finger joint has no dominant source vertices: " + name)
            report["hands"] = hands
            # 驗證前先套用引擎四權重與16-bit量化，避免匯出時截斷改變已驗證的蒙皮表面。
            report["nonHandStaticRootFallbackVertices"] = []
            all_hand_vertices = set(i for hand in hands for i in hand["sourceHandVertices"])
            for vertex in body.data.vertices:
                influences = sorted(((g.group,g.weight) for g in vertex.groups if g.weight>0),
                                    key=lambda item:(-item[1],item[0]))[:4]
                total = sum(weight for _,weight in influences)
                if total <= 0:
                    if vertex.index in all_hand_vertices:
                        raise RuntimeError("Prototype hand vertex has no skin influences: "+str(vertex.index))
                    # 非本輪手部的原始靜態點只綁Root保留bind位置，不宣稱全身蒙皮已驗證。
                    influences = [(body.vertex_groups["Root"].index,1.0)]; total = 1.0
                    report["nonHandStaticRootFallbackVertices"].append(vertex.index)
                quantized = [math.floor(weight/total*65535) for _,weight in influences]
                quantized[0] += 65535-sum(quantized)
                for group_index in [g.group for g in vertex.groups]:
                    body.vertex_groups[group_index].remove([vertex.index])
                for (group,_),weight in zip(influences,quantized):
                    if weight:
                        body.vertex_groups[group].add([vertex.index],weight/65535,"REPLACE")
            report["maximumSkinInfluences"] = max(len(v.groups) for v in body.data.vertices)
            report["skinWeightQuantization"] = 65535
            report["sourceGeometryUnchanged"] = original == [tuple(v.co) for v in body.data.vertices]
            report["boneCount"] = len(rig.data.bones)
            # 先保留可審查的分指骨架；握柄與碰撞尚未通過前不得當正式可用資產。
            pose_hand_grip_prototype(body, rig, hands)
            preview_hand_grip(body, hands, destination, report["sex"])
            report["contactLimits"] = {"cylinderRadius": .02, "maximumPenetration": 0.0, "maximumSurfaceGap": .001}
            report["contactBasis"] = "Continuous source-triangle surface clipped to finite cylinder, not vertex-only contact"
            report["sampledAnimationFrame"] = 31
            report["remainingFailures"] = []
            for hand in hands:
                if hand["side"] != "Right":
                    continue
                if not hand["wrapPassed"]:
                    report["remainingFailures"].append(hand["side"]+": finger wrap or thumb opposition failed")
                if not hand["continuousShaftContact"]["passed"]:
                    report["remainingFailures"].append(hand["side"]+": continuous triangle/finite-cylinder contact failed")
                if hand["selfIntersections"] or not hand["selfIntersectionVerified"]:
                    report["remainingFailures"].append(hand["side"]+": hand self-intersection failed or unresolved")
                if hand["degenerateTriangles"]:
                    report["remainingFailures"].append(hand["side"]+": degenerate hand triangles")
            if not report["sourceGeometryUnchanged"]:
                report["remainingFailures"].append("Source positions changed")
            report["accepted"] = not report["remainingFailures"]
            report["status"] = "RIGHT_HAND_PROTOTYPE_GATES_PASSED" if report["accepted"] else "NO_GO_GRIP_CONTACT"
            report["firstDivergence"] = next(iter(report["remainingFailures"]), None)
            bpy.ops.wm.save_as_mainfile(filepath=str(destination / (report["sex"] + "-hand-rig-prototype.blend")))
            if report["accepted"]:
                add_point_colors(body)
                if not body.data.materials:
                    body.data.materials.append(create_material("PrototypeSkin",(.55,.35,.22,1),.65))
                bpy.ops.object.select_all(action="DESELECT"); body.select_set(True); rig.select_set(True)
                bpy.context.view_layer.objects.active = rig
                path = destination/(report["sex"]+"-hand-rig-prototype.gltf")
                bpy.ops.export_scene.gltf(filepath=str(path),export_format="GLTF_SEPARATE",use_selection=True,
                                          export_animations=True,export_skins=True,export_tangents=True,export_yup=True,
                                          export_materials="EXPORT",export_cameras=False,export_lights=False,
                                          export_force_sampling=True,export_frame_range=True,
                                          export_optimize_animation_size=False,
                                          export_sampling_interpolation_fallback="LINEAR")
                canonicalize_gltf_binary(path)
        except Exception as error:
            report["status"] = "NO_GO"; report["firstDivergence"] = str(error)
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print("HAND_RIG_PROTOTYPE", report["sex"], report["status"], flush=True)
        all_accepted = all_accepted and report["accepted"]
    return all_accepted


def retrofit_hand_readonly(source: Path, destination: Path, female: bool, output_stem: str | None = None) -> bool:
    # 只讀使用者來源，所有派生資料只進獨立bld；此路徑禁止任何save_as_mainfile。
    sex = "female" if female else "male"
    authorized = Path(rf"D:\Download\Aevrynd\Games\games\character_lab\assets\model-packs\realistic-{sex}\source\realistic-{sex}.blend").resolve()
    allowed = Path(r"D:\Download\Aevrynd\bld\hand-rig-prototype-20260907").resolve()
    source = source.resolve(); destination = destination.resolve()
    if source != authorized or (destination != allowed and allowed not in destination.parents):
        raise RuntimeError("Readonly retrofit source/output is outside the authorized paths.")
    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    expected_hash = ("27bfe2f1f0bd0291bd00696c4b44521f37e7d247e867c043a46baa30ed743d35" if female else
                     "f8534f97cd21aab6ca56d3f020afc5f20e0f368dd463a8878e87a33d68bb46d6")
    if source_hash != expected_hash:
        raise RuntimeError(f"Readonly {sex} source differs from the inspected source identity.")
    destination.mkdir(parents=True,exist_ok=True)
    output_stem = output_stem or (sex+"-retrofit")
    if not output_stem.replace("-", "").isalnum():
        raise RuntimeError("Readonly retrofit output stem is unsafe.")
    report = {"schemaVersion":1,"sourceSha256":source_hash,"accepted":False,"scope":sex+"-right-hand-retrofit",
              "leftHand":"OUT_OF_SCOPE_UNMODIFIED","sourceSaved":False}
    try:
        bpy.ops.wm.open_mainfile(filepath=str(source))
        body = bpy.data.objects.get("RealisticFemaleCharacter" if female else "RealisticMaleCharacter")
        rig = bpy.data.objects.get("F2FemaleRig" if female else "M2MaleRig")
        if body is None or rig is None or body.type != "MESH" or rig.type != "ARMATURE" or body.parent != rig:
            raise RuntimeError("Readonly source character/rig structure is unsupported.")
        positions = [tuple(v.co) for v in body.data.vertices]
        materials = [m.name for m in body.data.materials]
        polygon_materials = [p.material_index for p in body.data.polygons]
        hair_uv_remap = remap_male_hair_card_uvs(body) if not female else None
        actions = [a.name for a in bpy.data.actions]
        original_bones = {b.name:(tuple(b.head_local),tuple(b.tail_local),b.parent.name if b.parent else None) for b in rig.data.bones}
        if len(materials) != 9 or len(original_bones) != 24 or "GripPrototype" in actions:
            raise RuntimeError("Readonly source has unexpected material, rig, or animation identity.")
        bundle = Path(r"D:\Download\Aevrynd\AssetCache\BlenderHumanBaseMeshes\v1.4.1\human-base-meshes-bundle-v1.4.1\human_base_meshes_bundle.blend")
        base = load_base_objects(bundle,female)[0]; normalize_and_shape([base],female,1.78 if female else 2.04)
        base_count = len(base.data.vertices)
        if positions[:base_count] != [tuple(v.co) for v in base.data.vertices]:
            raise RuntimeError("CC0 base vertex prefix does not exactly map to the readonly character.")
        base_edges = {tuple(sorted(e.vertices)) for e in base.data.edges}
        source_edges = {tuple(sorted(e.vertices)) for e in body.data.edges if all(i<base_count for i in e.vertices)}
        if base_edges != source_edges:
            raise RuntimeError("Readonly base topology differs from the inspected CC0 prefix.")
        bpy.data.objects.remove(base,do_unlink=True)
        hand = inspect_hand_branches(body,female,"Right",base_count)
        rig.animation_data.action = None
        for pose in rig.pose.bones:
            pose.matrix_basis = Matrix.Identity(4)
        bpy.context.view_layer.objects.active=rig; rig.select_set(True); bpy.ops.object.mode_set(mode="EDIT")
        for digit,entry in hand["branches"].items():
            points=[Vector(p) for p in entry["centers"]]; previous=rig.data.edit_bones["RightFingers"]
            for index,(head,tail) in enumerate(zip(points,points[1:])):
                bone=rig.data.edit_bones.new("Right"+digit+entry["parts"][index]); bone.head=head; bone.tail=tail; bone.parent=previous
                bone.use_connect=index>0; bone.align_roll(Vector((0,1,0))); previous=bone
        bpy.ops.object.mode_set(mode="OBJECT")
        for digit,entry in hand["branches"].items():
            names=["Right"+digit+part for part in entry["parts"]]
            for name in names:
                if body.vertex_groups.get(name):
                    raise RuntimeError("Readonly source already contains corrective vertex group: "+name)
                body.vertex_groups.new(name=name)
            for i in entry["sourceVertices"]:
                fraction=min(1.0,max(0.0,entry["distances"][i]/(entry["domainLength"] if digit=="Thumb" else entry["length"])))
                coordinate=min(float(len(names)-1),max(0.0,(1-fraction)*len(names)-.5)); lower=int(coordinate); upper=min(len(names)-1,lower+1); blend=coordinate-lower
                root_blend=min(1.0,max(0.0,(1-entry["distances"][i]/entry["domainLength"])/.2))
                for group_index in [g.group for g in body.data.vertices[i].groups]:body.vertex_groups[group_index].remove([i])
                body.vertex_groups[names[lower]].add([i],(1-blend)*root_blend,"REPLACE")
                if upper!=lower and blend>0:body.vertex_groups[names[upper]].add([i],blend*root_blend,"REPLACE")
                if root_blend<1:body.vertex_groups["RightHand"].add([i],1-root_blend,"REPLACE")
        for vertex in body.data.vertices:
            influences=sorted(((g.group,g.weight) for g in vertex.groups if g.weight>0),key=lambda a:(-a[1],a[0]))[:4]
            total=sum(w for _,w in influences)
            if total<=0:raise RuntimeError("Readonly source contains an unweighted vertex.")
            weights=[math.floor(w/total*65535) for _,w in influences];weights[0]+=65535-sum(weights)
            for group_index in [g.group for g in vertex.groups]:body.vertex_groups[group_index].remove([vertex.index])
            for (index,_),weight in zip(influences,weights):
                if weight:body.vertex_groups[index].add([vertex.index],weight/65535,"REPLACE")
        maximum_skin_influences=max(len([g for g in vertex.groups if g.weight>0]) for vertex in body.data.vertices)
        skin_weights_normalized=all(abs(sum(g.weight for g in vertex.groups if g.weight>0)-1.0)<=1e-6
                                    for vertex in body.data.vertices)
        if maximum_skin_influences>4 or not skin_weights_normalized:
            raise RuntimeError("Readonly retrofit violates the four-influence normalized skin contract.")
        for digit,entry in hand["branches"].items():
            entry["weights"]=[]
            for part in entry["parts"]:
                name="Right"+digit+part; group=body.vertex_groups[name]
                weighted=[v for v in body.data.vertices if any(g.group==group.index and g.weight>0 for g in v.groups)]
                dominant=sum(any(g.group==group.index and g.weight>=max(w.weight for w in v.groups) for g in v.groups) for v in weighted)
                entry["weights"].append({"name":name,"nonzero":len(weighted),"dominant":dominant})
                if not dominant:raise RuntimeError("Retrofit joint has no dominant vertices: "+name)
        pose_hand_grip_prototype(body,rig,[hand]); preview_hand_grip(body,[hand],destination,output_stem)
        report.update({"hands":[hand],"materialNames":materials,"sourceVertexCount":len(positions),"baseVertexPrefix":base_count,
                       "sourceEdgeCount":len(body.data.edges),"sourcePolygonCount":len(body.data.polygons),
                       "existingBoneCount":len(original_bones),"addedBoneCount":len(rig.data.bones)-len(original_bones),
                       "boneCount":len(rig.data.bones),"boneNames":[b.name for b in rig.data.bones],
                        "maximumSkinInfluences":maximum_skin_influences,"skinWeightsNormalized":skin_weights_normalized,
                        "skinWeightQuantization":65535,
                        "hairUvRemap":hair_uv_remap,
                       "sourcePositionsUnchanged":positions==[tuple(v.co) for v in body.data.vertices],
                       "materialsUnchanged":materials==[m.name for m in body.data.materials] and polygon_materials==[p.material_index for p in body.data.polygons],
                       "existingBonesUnchanged":all(original_bones[n]==(tuple(rig.data.bones[n].head_local),tuple(rig.data.bones[n].tail_local),rig.data.bones[n].parent.name if rig.data.bones[n].parent else None) for n in original_bones),
                       "existingActions":actions,"allActions":[a.name for a in bpy.data.actions]})
        report["accepted"]=report["sourcePositionsUnchanged"] and report["materialsUnchanged"] and report["existingBonesUnchanged"] and all(name in report["allActions"] for name in actions) and hand["continuousShaftContact"]["passed"] and hand["wrapPassed"] and hand["selfIntersectionVerified"] and not hand["selfIntersections"] and not hand["degenerateTriangles"]
        report["status"]="READONLY_RETROFIT_PROTOTYPE_PASSED" if report["accepted"] else "NO_GO_RETROFIT_GATE"
        if report["accepted"]:
            bpy.ops.object.select_all(action="DESELECT");body.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
            path=destination/(output_stem+".gltf")
            bpy.ops.export_scene.gltf(filepath=str(path),export_format="GLTF_SEPARATE",use_selection=True,export_animations=True,
                                      export_skins=True,export_tangents=True,export_yup=True,export_materials="EXPORT",export_cameras=False,export_lights=False,
                                      export_force_sampling=True,export_frame_range=False,export_optimize_animation_size=False,export_sampling_interpolation_fallback="LINEAR")
            canonicalize_gltf_binary(path)
            document=json.loads(path.read_text()); report["exportMaterialCount"]=len(document.get("materials",[]));report["exportAnimationNames"]=[a.get("name") for a in document.get("animations",[])]
            if report["exportMaterialCount"]!=9 or not all(name in report["exportAnimationNames"] for name in actions):
                raise RuntimeError("Readonly retrofit export lost materials or existing animation clips.")
    except Exception as error:
        report["accepted"]=False;report["status"]="NO_GO";report["firstDivergence"]=str(error)
    report["sourceHashUnchanged"]=hashlib.sha256(source.read_bytes()).hexdigest()==source_hash
    report["accepted"]=report["accepted"] and report["sourceHashUnchanged"]
    (destination/(sex+"-retrofit-report.json")).write_text(json.dumps(report,indent=2),encoding="utf-8")
    print("READONLY_RETROFIT",report["status"],flush=True)
    return report["accepted"]


def main() -> None:
    values = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if values and values[0] == "--readonly-female-retrofit":
        if len(values) not in {3,4}:raise RuntimeError("Expected readonly female blend, dedicated bld output, and optional output stem.")
        if not retrofit_hand_readonly(Path(values[1]),Path(values[2]),True,values[3] if len(values)==4 else None):sys.exit(2)
        return
    if values and values[0] == "--readonly-male-retrofit":
        if len(values) not in {3,4}:raise RuntimeError("Expected readonly male blend, dedicated bld output, and optional output stem.")
        if not retrofit_hand_readonly(Path(values[1]),Path(values[2]),False,values[3] if len(values)==4 else None):sys.exit(2)
        return
    if values and values[0] == "--hand-rig-prototype":
        if len(values) != 3:
            raise RuntimeError("Expected prototype flag, CC0 bundle, and dedicated bld output directory.")
        if not hand_rig_prototype(Path(values[1]).resolve(), Path(values[2]).resolve()):
            sys.exit(2)
        return
    bundle, female_output, male_output = arguments()
    export_character(bundle, female_output, True)
    export_character(bundle, male_output, False)


main()
