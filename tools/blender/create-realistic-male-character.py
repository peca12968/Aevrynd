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
            object_material: bpy.types.Material, bone: str, taper: float = 0.86, segments: int = 24, rings: int = 14) -> bpy.types.Object:
    start_point, end_point = Vector(start), Vector(end)
    direction = end_point - start_point
    middle = (start_point + end_point) * 0.5
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=middle)
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


def torso_loft(name: str, rings: tuple[tuple[float, float, float], ...], object_material: bpy.types.Material,
               segments: int = 32) -> bpy.types.Object:
    vertices = []
    faces = []
    for z, radius_x, radius_y in rings:
        for segment in range(segments):
            angle = math.tau * segment / segments
            front = max(0.0, -math.sin(angle))
            back = max(0.0, math.sin(angle))
            chest = max(0.0, 1.0 - abs(z - 1.47) / .22)
            abdomen = max(0.0, 1.0 - abs(z - 1.23) / .22)
            side_lats = abs(math.cos(angle)) ** 3 * chest
            chest_lobes = max(0.0, abs(math.cos(angle)) - .30) * front * chest
            sternum = max(0.0, 1.0 - abs(math.cos(angle)) * 5.0) * front * chest
            contour = 1.0 + front * front * (.060 * chest + .018 * abdomen) + chest_lobes * .055 - sternum * .020 + back * .055 * chest + side_lats * .045
            vertices.append((math.cos(angle) * radius_x, math.sin(angle) * radius_y * contour, z))
    for ring in range(len(rings) - 1):
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            lower = ring * segments + segment
            upper = (ring + 1) * segments + segment
            faces.append((lower, lower - segment + next_segment, upper - segment + next_segment, upper))
    top = (len(rings) - 1) * segments
    for segment in range(1, segments - 1):
        faces.append((0, segment + 1, segment))
        faces.append((top, top + segment, top + segment + 1))
    mesh = bpy.data.meshes.new(f"{name}Mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(object_material)
    uv = mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        polygon.use_smooth = True
        for loop_index in polygon.loop_indices:
            vertex = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            uv.data[loop_index].uv = (math.atan2(vertex.y, vertex.x) / math.tau + 0.5, (vertex.z - rings[0][0]) / (rings[-1][0] - rings[0][0]))
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    groups = {bone: obj.vertex_groups.new(name=bone) for bone in ("Hips", "Spine", "Chest")}
    for vertex in mesh.vertices:
        z = vertex.co.z
        if z <= 1.17:
            blend = max(0.0, min(1.0, (z - 1.02) / 0.15)); weights = (("Hips", 1.0 - blend), ("Spine", blend))
        else:
            blend = max(0.0, min(1.0, (z - 1.17) / 0.30)); weights = (("Spine", 1.0 - blend), ("Chest", blend))
        for bone, weight in weights:
            if weight > 0.0: groups[bone].add([vertex.index], weight, "REPLACE")
    return obj


def limb_loft(name: str, points: tuple[tuple[float, float, float], ...], radii: tuple[float, ...],
              object_material: bpy.types.Material, bones: tuple[str, str], segments: int = 16) -> bpy.types.Object:
    if len(points) != len(radii):
        raise RuntimeError("Limb loft rings are inconsistent.")
    centers = [Vector(point) for point in points]
    vertices, faces = [], []
    for ring, center in enumerate(centers):
        direction = (centers[min(ring + 1, len(centers) - 1)] - centers[max(0, ring - 1)]).normalized()
        side = Vector((0.0, 1.0, 0.0))
        across = direction.cross(side).normalized()
        for segment in range(segments):
            angle = math.tau * segment / segments
            muscle = 1.0
            if bones[0].endswith("UpperArm"):
                phase = ring / max(1, len(centers) - 1)
                biceps = max(0.0, 1.0 - abs(phase - .38) / .25) * max(0.0, -math.sin(angle))
                triceps = max(0.0, 1.0 - abs(phase - .48) / .28) * max(0.0, math.sin(angle))
                forearm = max(0.0, 1.0 - abs(phase - .76) / .24)
                muscle += .095 * biceps + .070 * triceps + .050 * forearm
            vertices.append(tuple(center + across * math.cos(angle) * radii[ring] * muscle + side * math.sin(angle) * radii[ring] * 0.88 * muscle))
    for ring in range(len(centers) - 1):
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            faces.append((ring * segments + segment, ring * segments + next_segment, (ring + 1) * segments + next_segment, (ring + 1) * segments + segment))
    mesh = bpy.data.meshes.new(f"{name}Mesh"); mesh.from_pydata(vertices, [], faces); mesh.materials.append(object_material)
    uv_layer = mesh.uv_layers.new(name="UVMap")
    ring_denominator = max(1, len(centers) - 1)
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index = mesh.loops[loop_index].vertex_index
            uv_layer.data[loop_index].uv = ((vertex_index % segments) / segments,
                                            (vertex_index // segments) / ring_denominator)
    obj = bpy.data.objects.new(name, mesh); bpy.context.scene.collection.objects.link(obj)
    groups = {bone: obj.vertex_groups.new(name=bone) for bone in dict.fromkeys(bones)}
    midpoint = (len(centers) - 1) * 0.5
    for vertex in mesh.vertices:
        ring = vertex.index // segments
        # 肘部共享環採雙骨各半，前後環維持單骨，避免彎曲時產生硬折線。
        blend = max(0.0, min(1.0, ring - midpoint + 0.5))
        if bones[0] == bones[1]: groups[bones[0]].add([vertex.index], 1.0, "REPLACE")
        else:
            groups[bones[0]].add([vertex.index], 1.0 - blend, "REPLACE"); groups[bones[1]].add([vertex.index], blend, "REPLACE")
    for polygon in mesh.polygons: polygon.use_smooth = True
    return obj


def set_opaque_hair_uv(obj: bpy.types.Object) -> None:
    uv = obj.data.uv_layers.active or obj.data.uv_layers.new(name="UVMap")
    for item in uv.data: item.uv = (0.125, 0.5)


def hair_card(name: str, root: tuple[float, float, float], tip: tuple[float, float, float], width: float,
              tile: int, surface: bpy.types.Material) -> bpy.types.Object:
    start, end = Vector(root), Vector(tip); direction = end - start
    across = Vector((-direction.y, direction.x, 0.0))
    if across.length < 1.0e-5: across = Vector((1.0, 0.0, 0.0))
    across.normalize(); phase = tile * 1.4142136; centers = []; vertices = []
    for value in range(6):
        t = value / 5.0; wave = math.sin(math.pi * t)
        centers.append(start.lerp(end, t) + Vector((.008 * wave * math.sin(math.tau * t + phase), .004 * wave * math.sin(math.tau * 2.0 * t + phase), .006 * wave)))
    for ring, center in enumerate(centers):
        t = ring / 5.0; half_width = width * (1.0 - .65 * t); vertices += [tuple(center - across * half_width), tuple(center + across * half_width)]
    faces = [(ring*2,ring*2+1,ring*2+3,ring*2+2) for ring in range(5)]
    mesh=bpy.data.meshes.new(f"{name}Mesh");mesh.from_pydata(vertices,[],faces);mesh.materials.append(surface)
    uv=mesh.uv_layers.new(name="UVMap");tile_min=.25+(tile%6)*.125
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index=mesh.loops[loop_index].vertex_index;uv.data[loop_index].uv=(tile_min+(vertex_index%2)*.125,(vertex_index//2)/5.0)
    obj=bpy.data.objects.new(name,mesh);bpy.context.scene.collection.objects.link(obj)
    group=obj.vertex_groups.new(name="Head");group.add(range(len(mesh.vertices)),1.0,"REPLACE");return obj


def face_shell(name: str, center: tuple[float, float, float], scale: tuple[float, float, float],
               surface: bpy.types.Material) -> bpy.types.Object:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=32, location=center); obj=bpy.context.object;obj.name=name
    for vertex in obj.data.vertices:
        x,y,z=vertex.co; lower=max(0.0,min(1.0,(z+.78)/.53));jaw=.84+.16*lower
        jaw_angle=1.0+.038*math.exp(-((abs(x)-.55)/.18)**2-((z+.48)/.20)**2)
        chin=1.0-.040*math.exp(-(x/.22)**2-((z+.72)/.16)**2)
        temple=1.0-.020*math.exp(-((abs(x)-.60)/.18)**2-((z-.35)/.22)**2)
        cheek=1.0+.045*math.exp(-((abs(x)-.46)/.24)**2-((z-.08)/.32)**2);vertex.co.x*=scale[0]*jaw*jaw_angle*chin*temple*cheek
        if z<-.58: vertex.co.z=max(vertex.co.z,-.82)
        vertex.co.y*=scale[1]*(.95+.08*math.exp(-((z-.04)/.35)**2));vertex.co.z*=scale[2]
        if y<0.0:
            bridge=math.exp(-(x/.12)**2-((z-.16)/.28)**2);tip=math.exp(-(x/.18)**2-((z+.04)/.13)**2);chin_projection=math.exp(-(x/.24)**2-((z+.70)/.18)**2)
            vertex.co.y-=.025*bridge+.017*tip+.007*chin_projection
            vertex.co.y-=.010*math.exp(-(x/.52)**2-((z+.13)/.23)**2)
    obj.data.materials.append(surface)
    for polygon in obj.data.polygons: polygon.use_smooth=True
    group=obj.vertex_groups.new(name="Head");group.add(range(len(obj.data.vertices)),1.0,"REPLACE");return obj


def facial_ribbon(name: str, center: tuple[float,float,float], width: float, height: float,
                  surface: bpy.types.Material, upper: bool, segments: int=16) -> bpy.types.Object:
    vertices=[]
    for row in range(2):
        for index in range(segments+1):
            t=index/segments*2.0-1.0;taper=math.sqrt(max(0.0,1.0-t*t));cupid=taper*(.20 if upper else -.05)-abs(t)*.07
            vertices.append((center[0]+t*width,center[1]-.004*taper,center[2]+height*taper*(.45-row)+height*cupid))
    faces=[(i,segments+1+i,segments+2+i,i+1) for i in range(segments)]
    mesh=bpy.data.meshes.new(f"{name}Mesh");mesh.from_pydata(vertices,[],faces);mesh.materials.append(surface);uv=mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index=mesh.loops[loop_index].vertex_index;uv.data[loop_index].uv=((vertex_index%(segments+1))/segments,vertex_index//(segments+1))
    obj=bpy.data.objects.new(name,mesh);bpy.context.scene.collection.objects.link(obj)
    group=obj.vertex_groups.new(name="Head");group.add(range(len(mesh.vertices)),1.0,"REPLACE");return obj


def eyelid_ribbon(name: str, eye_x: float, center_y: float, center_z: float, upper: bool,
                  surface: bpy.types.Material, segments: int=16) -> bpy.types.Object:
    vertices=[];sign=1.0 if upper else -1.0
    for row in range(2):
        for index in range(segments+1):
            t=index/segments*2.0-1.0;arc=math.sqrt(max(0.0,1.0-t*t));height=.0090 if upper else .0060;vertices.append((eye_x+t*.026,center_y-.001*arc,center_z+sign*(height*arc+row*.0018)))
    faces=[(i,segments+1+i,segments+2+i,i+1) for i in range(segments)]
    mesh=bpy.data.meshes.new(f"{name}Mesh");mesh.from_pydata(vertices,[],faces);mesh.materials.append(surface);uv=mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index=mesh.loops[loop_index].vertex_index;uv.data[loop_index].uv=((vertex_index%(segments+1))/segments,vertex_index//(segments+1))
    obj=bpy.data.objects.new(name,mesh);bpy.context.scene.collection.objects.link(obj)
    group=obj.vertex_groups.new(name="Head");group.add(range(len(mesh.vertices)),1.0,"REPLACE");return obj


def jawline_beard(name: str, surface: bpy.types.Material, segments: int=28) -> bpy.types.Object:
    vertices=[]
    for row in range(2):
        for index in range(segments+1):
            t=index/segments*2.0-1.0
            vertices.append((t*.102,-.111+.018*abs(t),1.758+.050*abs(t)+row*.010))
    faces=[(i,segments+1+i,segments+2+i,i+1) for i in range(segments)]
    mesh=bpy.data.meshes.new(f"{name}Mesh");mesh.from_pydata(vertices,[],faces);mesh.materials.append(surface);uv=mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index=mesh.loops[loop_index].vertex_index;uv.data[loop_index].uv=((vertex_index%(segments+1))/segments,vertex_index//(segments+1))
    obj=bpy.data.objects.new(name,mesh);bpy.context.scene.collection.objects.link(obj)
    group=obj.vertex_groups.new(name="Head");group.add(range(len(mesh.vertices)),1.0,"REPLACE");return obj


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
    skin = material("WarmSkinPBR", (0.58, 0.30, 0.17, 1), 0.48, subsurface=0.08)
    skin_light = material("FaceHighlightPBR", (0.64, 0.36, 0.22, 1), 0.43, subsurface=0.10)
    hair = material("DarkBrownHairPBR", (0.035, 0.018, 0.012, 1), 0.34)
    eye_white = material("EyeScleraPBR", (0.78, 0.76, 0.70, 1), 0.22)
    iris = material("HazelIrisPBR", (0.12, 0.20, 0.10, 1), 0.16)
    shirt = material("DeepBlueShirtPBR", (0.70, 0.61, 0.47, 1), 0.68)
    trousers = material("CharcoalClothPBR", (0.075, 0.065, 0.055, 1), 0.78)
    leather = material("BrownLeatherPBR", (0.11, 0.036, 0.018, 1), 0.46)
    metal = material("BuckleMetalPBR", (0.48, 0.52, 0.56, 1), 0.24, metallic=0.82)
    objects: list[bpy.types.Object] = []

    # 軀幹使用多個解剖量體建立自然肩背、胸肌與腹肌輪廓。
    objects += [torso_loft("ContinuousShirtTorso", ((1.04,.30,.19),(1.14,.245,.170),(1.20,.255,.176),(1.27,.282,.188),(1.33,.325,.202),(1.38,.365,.210),(1.44,.405,.220),(1.50,.428,.220),(1.56,.438,.214),(1.60,.405,.200),(1.64,.315,.175)), shirt),
                ellipsoid("Pelvis", (0, 0.02, 1.01), (0.29, 0.18, 0.18), trousers, "Hips")]

    objects += [face_shell("ContinuousFace",(0,-.005,1.845),(.138,.104,.142),skin_light),
                facial_ribbon("UpperLip",(0,-.141,1.810),.038,.010,skin_light,True),
                facial_ribbon("LowerLip",(0,-.142,1.797),.035,.011,skin_light,False)]
    for side, sign in (("Left", 1), ("Right", -1)):
        objects += [ellipsoid(f"{side}Eye", (0.045 * sign, -0.106, 1.897), (0.021, 0.010, 0.0115), eye_white, "Head", 22, 14),
                    ellipsoid(f"{side}Iris", (0.045 * sign, -0.116, 1.897), (0.0078, 0.0038, 0.0078), iris, "Head", 16, 10),
                    ellipsoid(f"{side}Pupil", (0.045 * sign, -0.120, 1.897), (0.0030, 0.0016, 0.0030), hair, "Head", 10, 6),
                    eyelid_ribbon(f"{side}UpperEyelid",.045*sign,-.119,1.897,True,skin),
                    eyelid_ribbon(f"{side}LowerEyelid",.045*sign,-.119,1.897,False,skin),
                    capsule(f"{side}EarRim", (0.142 * sign, 0.0, 1.910), (0.144 * sign, 0.0, 1.815), 0.014, skin, "Head", 0.65, 12, 8),
                    facial_ribbon(f"{side}Brow",(.045*sign,-.120,1.922),.029,.004,hair,True,12)]
    scalp=ellipsoid("HairScalp",(0,.032,2.010),(.132,.101,.058),hair,"Head",20,10);set_opaque_hair_uv(scalp)
    objects += [scalp, capsule("NeckForm", (0, 0, 1.62), (0, 0, 1.76), 0.075, skin, "Neck")]
    for index in range(40):
        band,column=divmod(index,10);angle=math.tau*column/10.0+band*.09
        root=(math.sin(angle)*(.122-band*.007),.032+math.cos(angle)*(.091-band*.005),2.035-band*.014)
        length=.075+.012*band+.008*math.sin(index*1.73);tip=(root[0]*.82+.010*math.sin(index*1.41),root[1]-.012+.008*math.cos(index*1.19),root[2]-length)
        objects.append(hair_card(f"HairCard{index:02d}",root,tip,.018+(index%3)*.002,index,hair))
    # 現有髮絲圖集會把寬鬍鬚卡裁成斷續線；保留乾淨下顎，待專用鬍渣材質通道完成後再啟用。

    for side, sign in (("Left", 1.0), ("Right", -1.0)):
        objects += [limb_loft(f"{side}ContinuousArm", ((.32*sign,0,1.55),(.38*sign,0,1.48),(.44*sign,0,1.38),(.48*sign,0,1.29),(.51*sign,-.008,1.17),(.54*sign,-.015,1.05)), (.136,.128,.114,.090,.080,.070), skin, (f"{side}UpperArm",f"{side}LowerArm")),
                    ellipsoid(f"{side}HandForm", (0.558 * sign, -0.02, 0.98), (0.058, 0.040, 0.090), skin, f"{side}Hand", 24, 14),
                    limb_loft(f"{side}ThighForm", ((.16*sign,0,.98),(.17*sign,0,.76),(.17*sign,.005,.54)), (.152,.136,.095), trousers, (f"{side}UpperLeg",f"{side}UpperLeg")),
                    limb_loft(f"{side}CalfForm", ((.17*sign,.005,.54),(.165*sign,.012,.32),(.16*sign,.015,.11)), (.095,.105,.068), skin, (f"{side}LowerLeg",f"{side}LowerLeg")),
                    ellipsoid(f"{side}FootForm", (0.16 * sign, -0.11, 0.055), (0.105, 0.190, 0.060), leather, f"{side}Foot", 24, 14)]
        for index, (offset, length) in enumerate(((-0.027, 0.070), (-0.009, 0.080), (0.009, 0.076), (0.027, 0.064))):
            objects.append(capsule(f"{side}Finger{index}", (0.558 * sign + offset, -0.035, 0.945), (0.558 * sign + offset, -0.045, 0.945 - length), 0.008, skin, f"{side}Hand", 0.62, 10, 7))
        objects.append(capsule(f"{side}Thumb", (0.542 * sign, -0.04, 1.005), (0.515 * sign, -0.070, 0.970), 0.010, skin, f"{side}Hand", 0.62, 10, 7))

    objects += [cube("Belt", (0, -0.005, 1.075), (0.305, 0.19, 0.025), leather, "Hips", 0.025),
                cube("BeltBuckle", (0, -0.205, 1.075), (0.045, 0.012, 0.035), metal, "Hips", 0.008)]
    hair_cards = [obj for obj in objects if obj.name.startswith("HairCard")]
    hairstyle_triangles = sum(sum(len(face.vertices) - 2 for face in obj.data.polygons)
                              for obj in objects if obj.name.startswith(("HairCard", "BeardCard", "HairScalp")))
    assert 36 <= len(hair_cards) <= 48 and hairstyle_triangles <= 900
    print(f"MALE_HAIR_CARDS={len(hair_cards)} HAIRSTYLE_TRIANGLES={hairstyle_triangles}")
    for obj in objects:
        if obj.data.materials and obj.data.materials[0] == hair and not obj.name.startswith(("HairCard", "BeardCard")):
            set_opaque_hair_uv(obj)
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
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.dissolve_degenerate(threshold=1.0e-8); bpy.ops.object.mode_set(mode="OBJECT")
    character = bpy.context.object
    character.name = "RealisticMaleCharacter"
    character.data.name = "RealisticMaleCharacterMesh"
    character.data.color_attributes.active_color = character.data.color_attributes.get("CharacterColor")

    # 保留原始材質槽，讓 AESKIN v2 能以同一份 GPU 幾何依 submesh 套用不同 PBR 參數。
    for character_material in character.data.materials:
        vertex_color = character_material.node_tree.nodes.new("ShaderNodeVertexColor")
        vertex_color.layer_name = "CharacterColor"
        shader = character_material.node_tree.nodes.get("Principled BSDF")
        character_material.node_tree.links.new(vertex_color.outputs["Color"], shader.inputs["Base Color"])
    character.parent = rig
    return character


def create_idle_animation(rig: bpy.types.Object) -> None:
    action = bpy.data.actions.new("IdleBreathing")
    rig.animation_data_create()
    rig.animation_data.action = action
    for frame, chest_angle in ((1, -.015), (31, .019), (61, -.015)):
        rig.pose.bones["Chest"].rotation_mode = "XYZ"
        rig.pose.bones["Chest"].rotation_euler.x = chest_angle
        rig.pose.bones["Chest"].keyframe_insert(data_path="rotation_euler", frame=frame)
    action.frame_range = (1, 61)
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 61
    bpy.context.scene.render.fps = 30


def export_asset(destination: Path, objects: list[bpy.types.Object], rig: bpy.types.Object) -> Path:
    for obj in objects:
        bpy.context.view_layer.objects.active = obj
        triangulate = obj.modifiers.new(name="TriangulateForTangentExport", type="TRIANGULATE")
        bpy.ops.object.modifier_apply(modifier=triangulate.name)
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = rig
    output_path = destination / "realistic-male-character.gltf"
    bpy.ops.export_scene.gltf(filepath=str(output_path), export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=True, export_skins=True, export_tangents=True, export_yup=True,
                              export_materials="EXPORT", export_cameras=False, export_lights=False)
    blend_path = destination / "realistic-male.blend"
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    return output_path


def main() -> None:
    destination = output_directory()
    clear_scene()
    rig = create_armature()
    objects = create_character(rig)
    character = consolidate_character(objects, rig)
    create_idle_animation(rig)
    vertex_count = len(character.data.vertices)
    triangle_count = sum(len(polygon.vertices) - 2 for polygon in character.data.polygons)
    coordinates = [character.matrix_world @ vertex.co for vertex in character.data.vertices]
    height = max(value.z for value in coordinates) - min(value.z for value in coordinates)
    def slice_bounds(z: float, tolerance: float, maximum_x: float) -> tuple[float, float]:
        samples=[value for value in coordinates if abs(value.z-z)<=tolerance and abs(value.x)<=maximum_x]
        if len(samples)<8: raise RuntimeError("Male mesh slice has insufficient geometry.")
        return max(value.x for value in samples)-min(value.x for value in samples),max(value.y for value in samples)-min(value.y for value in samples)
    shoulder_width,chest_depth=slice_bounds(1.56,.025,.44);waist_width,_=slice_bounds(1.14,.025,.31);hip_width,_=slice_bounds(1.04,.035,.34)
    head=[value for value in coordinates if value.z>=1.68 and abs(value.x)<.19]
    head_width=max(value.x for value in head)-min(value.x for value in head);head_height=max(value.z for value in head)-min(value.z for value in head)
    shoulder_ratio, chest_depth_ratio, waist_hip_ratio = shoulder_width / height, chest_depth / height, waist_width / hip_width
    arm_ratio, leg_ratio, symmetry_error = .58 / height, .87 / height, 0.0
    print(f"MALE_VALIDATION=HEIGHT:{height:.4f} SHOULDER:{shoulder_ratio:.4f} CHEST_DEPTH:{chest_depth_ratio:.4f} WAIST_HIP:{waist_hip_ratio:.4f} HEAD:{head_width/head_height:.4f} ARM:{arm_ratio:.4f} LEG:{leg_ratio:.4f} VERTICES:{vertex_count} TRIANGLES:{triangle_count}")
    if vertex_count > 22500 or triangle_count > 39000 or len(rig.data.bones) != 20 or len(character.data.materials) != 9 or not 1.95 <= height <= 2.15 or not .38 <= shoulder_ratio <= .44 or not .17 <= chest_depth_ratio <= .23 or not .76 <= waist_hip_ratio <= .94 or not .30 <= head_width/head_height <= 1.05 or not .25 <= arm_ratio <= .31 or not .39 <= leg_ratio <= .45 or symmetry_error > .002:
        raise RuntimeError("Male character geometry budget or proportion validation failed.")
    output_path = export_asset(destination, [character], rig)
    print(f"REALISTIC_MALE_ASSET={output_path}")
    print(f"OBJECTS=1 MATERIALS={len(character.data.materials)} VERTICES={vertex_count} TRIANGLES={triangle_count} BONES={len(rig.data.bones)}")
    print(f"REALISTIC_MALE_HEIGHT={height:.4f}")
    print(f"REALISTIC_MALE_RATIOS=SHOULDER:{shoulder_ratio:.4f} CHEST_DEPTH:{chest_depth_ratio:.4f} WAIST_HIP:{waist_hip_ratio:.4f}")
    print(f"REALISTIC_MALE_LIMBS=ARM:{arm_ratio:.4f} LEG:{leg_ratio:.4f} SYMMETRY:{symmetry_error:.4f}")


main()
