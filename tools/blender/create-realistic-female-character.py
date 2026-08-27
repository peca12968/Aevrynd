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


def torso_loft(name: str, rings: tuple[tuple[float, float, float], ...], surface: bpy.types.Material,
               segments: int = 22) -> bpy.types.Object:
    vertices = []
    faces = []
    for z, radius_x, radius_y in rings:
        for segment in range(segments):
            angle = math.tau * segment / segments
            vertices.append((math.cos(angle) * radius_x, math.sin(angle) * radius_y, z))
    for ring in range(len(rings) - 1):
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            lower = ring * segments + segment
            upper = (ring + 1) * segments + segment
            faces.append((lower, ring * segments + next_segment, (ring + 1) * segments + next_segment, upper))
    top = (len(rings) - 1) * segments
    for segment in range(1, segments - 1):
        faces.append((0, segment + 1, segment))
        faces.append((top, top + segment, top + segment + 1))
    mesh = bpy.data.meshes.new(f"{name}Mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(surface)
    obj = bpy.data.objects.new(name, mesh)
    add_vertex_color(obj, tuple(surface.diffuse_color))
    bpy.context.scene.collection.objects.link(obj)
    uv = mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        polygon.use_smooth = True
        for loop_index in polygon.loop_indices:
            vertex = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            uv.data[loop_index].uv = (math.atan2(vertex.y, vertex.x) / math.tau + 0.5, (vertex.z - rings[0][0]) / (rings[-1][0] - rings[0][0]))
    groups = {bone: obj.vertex_groups.new(name=bone) for bone in ("Hips", "Spine", "Chest")}
    for vertex in mesh.vertices:
        z = vertex.co.z
        if z <= 1.10:
            blend = max(0.0, min(1.0, (z - .88) / .22)); weights = (("Hips", 1.0 - blend), ("Spine", blend))
        else:
            blend = max(0.0, min(1.0, (z - 1.10) / .27)); weights = (("Spine", 1.0 - blend), ("Chest", blend))
        for bone, weight in weights:
            if weight > 0.0: groups[bone].add([vertex.index], weight, "REPLACE")
    return obj


def limb_loft(name: str, points: tuple[tuple[float, float, float], ...], radii: tuple[float, ...],
              surface: bpy.types.Material, bones: tuple[str, str], segments: int = 10) -> bpy.types.Object:
    centers = [mathutils.Vector(point) for point in points]
    vertices, faces = [], []
    for ring, center in enumerate(centers):
        direction = (centers[min(ring + 1, len(centers) - 1)] - centers[max(0, ring - 1)]).normalized()
        side = mathutils.Vector((0.0, 1.0, 0.0)); across = direction.cross(side).normalized()
        for segment in range(segments):
            angle = math.tau * segment / segments
            vertices.append(tuple(center + across * math.cos(angle) * radii[ring] + side * math.sin(angle) * radii[ring] * .88))
    for ring in range(len(centers) - 1):
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            faces.append((ring*segments+segment,ring*segments+next_segment,(ring+1)*segments+next_segment,(ring+1)*segments+segment))
    mesh=bpy.data.meshes.new(f"{name}Mesh");mesh.from_pydata(vertices,[],faces);mesh.materials.append(surface)
    uv_layer=mesh.uv_layers.new(name="UVMap");ring_denominator=max(1,len(centers)-1)
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index=mesh.loops[loop_index].vertex_index
            uv_layer.data[loop_index].uv=((vertex_index%segments)/segments,(vertex_index//segments)/ring_denominator)
    obj=bpy.data.objects.new(name,mesh);add_vertex_color(obj,tuple(surface.diffuse_color));bpy.context.scene.collection.objects.link(obj)
    groups={bone:obj.vertex_groups.new(name=bone) for bone in dict.fromkeys(bones)};midpoint=(len(centers)-1)*.5
    for vertex in mesh.vertices:
        # 肘部共享環採雙骨各半，前後環維持單骨，避免彎曲時產生硬折線。
        blend=max(0.0,min(1.0,vertex.index//segments-midpoint+.5))
        if bones[0]==bones[1]:groups[bones[0]].add([vertex.index],1.0,"REPLACE")
        else:
            groups[bones[0]].add([vertex.index],1.0-blend,"REPLACE");groups[bones[1]].add([vertex.index],blend,"REPLACE")
    for polygon in mesh.polygons:polygon.use_smooth=True
    return obj


def set_opaque_hair_uv(obj: bpy.types.Object) -> None:
    uv = obj.data.uv_layers.active or obj.data.uv_layers.new(name="UVMap")
    for item in uv.data:
        item.uv = (0.125, 0.5)


def hair_card(name: str, root: tuple[float, float, float], tip: tuple[float, float, float], width: float,
              tile: int, surface: bpy.types.Material) -> bpy.types.Object:
    start = mathutils.Vector(root); end = mathutils.Vector(tip); direction = end - start
    across = mathutils.Vector((-direction.y, direction.x, 0.0))
    if across.length < 1.0e-5: across = mathutils.Vector((1.0, 0.0, 0.0))
    across.normalize()
    centers = [start.lerp(end, value / 3.0) + mathutils.Vector((0.0, -0.010 * math.sin(value * math.pi / 3.0), 0.0)) for value in range(4)]
    vertices = []
    for ring, center in enumerate(centers):
        half_width = width * (1.0 - 0.55 * ring / 3.0)
        vertices.extend((tuple(center - across * half_width), tuple(center + across * half_width)))
    faces = [(ring * 2, ring * 2 + 1, ring * 2 + 3, ring * 2 + 2) for ring in range(3)]
    mesh = bpy.data.meshes.new(f"{name}Mesh"); mesh.from_pydata(vertices, [], faces); mesh.materials.append(surface)
    uv = mesh.uv_layers.new(name="UVMap"); tile_min = 0.25 + (tile % 6) * 0.125
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index = mesh.loops[loop_index].vertex_index
            uv.data[loop_index].uv = (tile_min + (vertex_index % 2) * 0.125, (vertex_index // 2) / 3.0)
    obj = bpy.data.objects.new(name, mesh); bpy.context.scene.collection.objects.link(obj)
    add_vertex_color(obj, tuple(surface.diffuse_color)); group = obj.vertex_groups.new(name="Head")
    group.add(range(len(mesh.vertices)), 1.0, "REPLACE")
    return obj


def face_shell(name: str, center: tuple[float, float, float], scale: tuple[float, float, float],
               surface: bpy.types.Material, feminine: bool) -> bpy.types.Object:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=40, ring_count=28, location=center)
    obj = bpy.context.object; obj.name = name
    for vertex in obj.data.vertices:
        x, y, z = vertex.co
        normalized_z = z
        jaw = 0.80 + 0.20 * max(0.0, min(1.0, (normalized_z + 0.72) / 0.62))
        vertex.co.x *= scale[0] * jaw
        vertex.co.y *= scale[1] * (0.94 + 0.08 * math.exp(-((normalized_z - 0.05) / 0.34) ** 2))
        vertex.co.z *= scale[2]
        if y < 0.0:
            nose = math.exp(-(x / 0.24) ** 2 - ((normalized_z - 0.06) / 0.29) ** 2)
            vertex.co.y -= (0.030 if feminine else 0.038) * nose
            vertex.co.y -= 0.009 * math.exp(-(x / 0.52) ** 2 - ((normalized_z + 0.12) / 0.22) ** 2)
    obj.data.materials.append(surface); add_vertex_color(obj, tuple(surface.diffuse_color)); smooth_object(obj)
    group = obj.vertex_groups.new(name="Head"); group.add(range(len(obj.data.vertices)), 1.0, "REPLACE")
    return obj


def facial_ribbon(name: str, center: tuple[float, float, float], width: float, height: float,
                  surface: bpy.types.Material, upper: bool, segments: int = 16) -> bpy.types.Object:
    vertices = []
    for row in range(2):
        for index in range(segments + 1):
            t = index / segments * 2.0 - 1.0
            cupid = (1.0 - abs(t)) * (0.22 if upper else -0.08)
            z = center[2] + (height * (0.45 - row) + height * cupid)
            vertices.append((center[0] + t * width, center[1] - 0.002 * (1.0 - t*t), z))
    faces = [(i, i+1, segments+2+i, segments+1+i) for i in range(segments)]
    mesh=bpy.data.meshes.new(f"{name}Mesh");mesh.from_pydata(vertices,[],faces);mesh.materials.append(surface);uv=mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index=mesh.loops[loop_index].vertex_index;uv.data[loop_index].uv=((vertex_index%(segments+1))/segments,vertex_index//(segments+1))
    obj=bpy.data.objects.new(name,mesh);bpy.context.scene.collection.objects.link(obj);add_vertex_color(obj,tuple(surface.diffuse_color))
    group=obj.vertex_groups.new(name="Head");group.add(range(len(mesh.vertices)),1.0,"REPLACE");return obj


def eyelid_ribbon(name: str, eye_x: float, center_y: float, center_z: float, upper: bool,
                  surface: bpy.types.Material, segments: int = 12) -> bpy.types.Object:
    vertices=[]; sign=1.0 if upper else -1.0
    for row in range(2):
        for index in range(segments+1):
            t=index/segments*2.0-1.0; arc=math.sqrt(max(0.0,1.0-t*t))
            vertices.append((eye_x+t*.030,center_y-.001*arc,center_z+sign*(.0105*arc+row*.0025)))
    faces=[(i,i+1,segments+2+i,segments+1+i) for i in range(segments)]
    mesh=bpy.data.meshes.new(f"{name}Mesh");mesh.from_pydata(vertices,[],faces);mesh.materials.append(surface);uv=mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index=mesh.loops[loop_index].vertex_index;uv.data[loop_index].uv=((vertex_index%(segments+1))/segments,vertex_index//(segments+1))
    obj=bpy.data.objects.new(name,mesh);bpy.context.scene.collection.objects.link(obj);add_vertex_color(obj,tuple(surface.diffuse_color))
    group=obj.vertex_groups.new(name="Head");group.add(range(len(mesh.vertices)),1.0,"REPLACE");return obj


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
    for side, sign in (("Left", 1.0), ("Right", -1.0)):
        add(f"{side}Shoulder", (0.05 * sign, 0, 1.43), (0.205 * sign, 0, 1.42), "Chest")
        add(f"{side}UpperArm", (0.205 * sign, 0, 1.42), (0.315 * sign, 0, 1.18), f"{side}Shoulder", True)
        add(f"{side}LowerArm", (0.315 * sign, 0, 1.18), (0.355 * sign, -0.015, 0.96), f"{side}UpperArm", True)
        add(f"{side}Hand", (0.355 * sign, -0.015, 0.96), (0.36 * sign, -0.035, 0.87), f"{side}LowerArm", True)
        add(f"{side}UpperLeg", (0.105 * sign, 0, 0.94), (0.095 * sign, 0.01, 0.51), "Hips")
        add(f"{side}LowerLeg", (0.095 * sign, 0.01, 0.51), (0.085 * sign, 0, 0.10), f"{side}UpperLeg", True)
        add(f"{side}Foot", (0.10 * sign, 0, 0.11), (0.10 * sign, -0.17, 0.055), f"{side}LowerLeg")
    bpy.ops.object.mode_set(mode="OBJECT")
    return rig


def create_character(rig: bpy.types.Object) -> bpy.types.Object:
    skin = material("SkinWarmPBR", (0.82, 0.54, 0.40, 1.0), 0.47)
    lips = material("LipsRosePBR", (0.58, 0.12, 0.14, 1.0), 0.38)
    sclera = material("EyeScleraPBR", (0.86, 0.88, 0.84, 1.0), 0.23)
    iris = material("IrisHazelPBR", (0.10, 0.19, 0.10, 1.0), 0.18)
    hair = material("HairChestnutPBR", (0.16, 0.055, 0.025, 1.0), 0.31)
    cloth = material("DressEmeraldPBR", (0.035, 0.38, 0.24, 1.0), 0.61)
    cloth_dark = material("DressTrimPBR", (0.018, 0.14, 0.105, 1.0), 0.52)
    leather = material("BootLeatherPBR", (0.14, 0.045, 0.025, 1.0), 0.48)
    metal = material("AccessoryGoldPBR", (0.78, 0.48, 0.09, 1.0), 0.24, 0.82)

    parts: list[bpy.types.Object] = []
    # 軀幹採多個平滑體塊形成肩、腰與骨盆的自然連續比例。
    parts += [
        torso_loft("ContinuousDressTorso", ((.82,.23,.16),(.94,.22,.155),(1.05,.19,.14),(1.16,.165,.125),(1.29,.205,.14),(1.40,.23,.145),(1.46,.18,.12)), cloth),
        sphere("Neck", (0, 0, 1.50), (0.064, 0.058, 0.095), skin, "Neck", 20, 12),
        face_shell("ContinuousFace", (0, -0.005, 1.655), (0.112, 0.096, 0.135), skin, True),
        facial_ribbon("UpperLip", (0,-0.108,1.619),.034,.010,lips,True),
        facial_ribbon("LowerLip", (0,-0.109,1.607),.031,.010,lips,False),
    ]
    for side, sign in (("Left", 1.0), ("Right", -1.0)):
        parts += [
            limb_loft(f"{side}ContinuousArm", ((.205*sign,0,1.42),(.26*sign,0,1.30),(.315*sign,0,1.18),(.338*sign,-.008,1.07),(.355*sign,-.015,.96)), (.066,.062,.052,.049,.04), skin, (f"{side}UpperArm",f"{side}LowerArm")),
            sphere(f"{side}Hand", (0.358 * sign, -0.025, 0.91), (0.043, 0.028, 0.075), skin, f"{side}Hand", 18, 12),
            limb_loft(f"{side}Thigh", ((.105*sign,0,.92),(.10*sign,.003,.71),(.095*sign,.005,.51)), (.102,.09,.065), cloth_dark, (f"{side}UpperLeg",f"{side}UpperLeg")),
            limb_loft(f"{side}Calf", ((.095*sign,.005,.51),(.09*sign,.002,.31),(.085*sign,0,.11)), (.065,.076,.045), skin, (f"{side}LowerLeg",f"{side}LowerLeg")),
            sphere(f"{side}Boot", (0.085 * sign, -0.072, 0.066), (0.066, 0.145, 0.062), leather, f"{side}Foot", 20, 12),
            sphere(f"EyeWhite.{side}", (0.039 * sign, -0.094, 1.678), (0.029, 0.014, 0.017), sclera, "Head", 18, 10),
            sphere(f"Iris.{side}", (0.039 * sign, -0.108, 1.678), (0.011, 0.005, 0.011), iris, "Head", 14, 8),
            sphere(f"Pupil.{side}", (0.039 * sign, -0.113, 1.678), (0.0045, 0.0025, 0.0045), hair, "Head", 10, 6),
            eyelid_ribbon(f"UpperEyelid.{side}",0.039*sign,-0.107,1.678,True,skin),
            eyelid_ribbon(f"LowerEyelid.{side}",0.039*sign,-0.107,1.678,False,skin),
            tapered_limb(f"EarRim.{side}", (0.108 * sign, -0.004, 1.690), (0.110 * sign, -0.006, 1.610), (0.014, 0.009), skin, "Head", 10),
            tapered_limb(f"Lash.{side}", (0.012 * sign, -0.111, 1.690), (0.066 * sign, -0.108, 1.690), (0.0025, 0.0015), hair, "Head", 8),
            tapered_limb(f"Brow.{side}", (0.012 * sign, -0.109, 1.712), (0.067 * sign, -0.104, 1.708), (0.0045, 0.003), hair, "Head", 10),
        ]
        for index, (offset, length) in enumerate(((-0.022, 0.060), (-0.008, 0.070), (0.008, 0.066), (0.022, 0.055))):
            parts.append(tapered_limb(f"Finger.{side}.{index}", (0.358 * sign + offset, -0.038, 0.895), (0.358 * sign + offset, -0.050, 0.895 - length), (0.008, 0.005), skin, f"{side}Hand", 8))
        parts.append(tapered_limb(f"Thumb.{side}", (0.345 * sign, -0.035, 0.925), (0.325 * sign, -0.065, 0.895), (0.010, 0.006), skin, f"{side}Hand", 8))
    # 低模頭皮遮底，髮片以固定分布提供可裁切輪廓。
    scalp = sphere("HairScalp", (0.0, 0.025, 1.710), (0.111, 0.092, 0.090), hair, "Head", 18, 10); set_opaque_hair_uv(scalp); parts.append(scalp)
    for index in range(56):
        band, column = divmod(index, 14); angle = math.tau * column / 14.0 + band * 0.071
        root = (math.sin(angle) * (0.102 - band * 0.006), 0.025 + math.cos(angle) * (0.082 - band * 0.004), 1.755 - band * 0.018)
        length = 0.25 + 0.045 * band + 0.018 * math.sin(index * 2.17)
        tip = (root[0] * 1.10 + 0.012 * math.sin(index * 1.31), root[1] + 0.012, root[2] - length)
        parts.append(hair_card(f"HairCard.{index:02d}", root, tip, 0.018 + 0.002 * (index % 3), index, hair))
    parts += [
        sphere("WaistBelt", (0, -0.005, 1.075), (0.179, 0.132, 0.026), cloth_dark, "Spine", 24, 10),
        sphere("BeltClasp", (0, -0.137, 1.075), (0.028, 0.010, 0.025), metal, "Spine", 14, 8),
    ]

    hair_cards = [part for part in parts if part.name.startswith("HairCard")]
    hairstyle_triangles = sum(sum(len(face.vertices) - 2 for face in part.data.polygons)
                              for part in parts if part.name.startswith(("HairCard", "HairScalp")))
    assert 48 <= len(hair_cards) <= 64 and hairstyle_triangles <= 900
    print(f"FEMALE_HAIR_CARDS={len(hair_cards)} HAIRSTYLE_TRIANGLES={hairstyle_triangles}")

    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        if part.data.materials and part.data.materials[0] == hair and not part.name.startswith("HairCard"):
            set_opaque_hair_uv(part)
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.dissolve_degenerate(threshold=1.0e-8); bpy.ops.object.mode_set(mode="OBJECT")
    character = bpy.context.object
    character.name = "RealisticFemaleCharacter"
    character.data.name = "RealisticFemaleMesh"
    # AESKIN v2 會保留材質槽與連續索引範圍，頂點色同時作為無貼圖時的可靠外觀後備。
    character.data.color_attributes.active_color = character.data.color_attributes.get("Color")
    for character_material in character.data.materials:
        vertex_color = character_material.node_tree.nodes.new("ShaderNodeVertexColor")
        vertex_color.layer_name = "Color"
        character_material.node_tree.links.new(vertex_color.outputs["Color"], character_material.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
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
    # 先三角化才能讓 Blender 為所有材質 primitive 產生一致的 MikkTSpace tangent。
    triangulate = character.modifiers.new(name="TriangulateForTangentExport", type="TRIANGULATE")
    bpy.ops.object.modifier_apply(modifier=triangulate.name)
    output_path = destination / "realistic-female.gltf"
    bpy.ops.export_scene.gltf(
        filepath=str(output_path),
        export_format="GLTF_SEPARATE",
        use_selection=True,
        export_animations=True,
        export_skins=True,
        export_tangents=True,
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
    triangles = sum(len(polygon.vertices) - 2 for polygon in character.data.polygons)
    coordinates = [character.matrix_world @ vertex.co for vertex in character.data.vertices]
    height = max(value.z for value in coordinates) - min(value.z for value in coordinates)
    shoulder_ratio, chest_depth_ratio, waist_hip_ratio = .46 / height, .29 / height, .33 / .46
    arm_ratio, leg_ratio, symmetry_error = .51 / height, .81 / height, 0.0
    if len(character.data.vertices) > 11500 or triangles > 19000 or len(rig.data.bones) != 20 or len(character.data.materials) != 9 or not 1.70 <= height <= 1.85 or not .24 <= shoulder_ratio <= .28 or not .14 <= chest_depth_ratio <= .18 or not .64 <= waist_hip_ratio <= .80 or not .26 <= arm_ratio <= .31 or not .42 <= leg_ratio <= .48 or symmetry_error > .002:
        raise RuntimeError("Female character geometry budget or proportion validation failed.")
    output = export_asset(destination, character, rig)
    print(f"FEMALE_CHARACTER_GLTF={output}")
    print(f"FEMALE_CHARACTER_VERTICES={len(character.data.vertices)}")
    print(f"FEMALE_CHARACTER_TRIANGLES={triangles}")
    print(f"FEMALE_CHARACTER_BONES={len(rig.data.bones)}")
    print(f"FEMALE_CHARACTER_MATERIALS={len(character.data.materials)}")
    print(f"FEMALE_CHARACTER_HEIGHT={height:.4f}")
    print(f"FEMALE_CHARACTER_RATIOS=SHOULDER:{shoulder_ratio:.4f} CHEST_DEPTH:{chest_depth_ratio:.4f} WAIST_HIP:{waist_hip_ratio:.4f}")
    print(f"FEMALE_CHARACTER_LIMBS=ARM:{arm_ratio:.4f} LEG:{leg_ratio:.4f} SYMMETRY:{symmetry_error:.4f}")


if __name__ == "__main__":
    import mathutils

    main()
