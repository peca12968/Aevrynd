from __future__ import annotations

from dataclasses import dataclass
import math

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree


@dataclass(frozen=True)
class GarmentShellMetrics:
    source_faces: int
    outer_vertices: int
    rim_vertices: int
    boundary_edges: int
    collar_vertices: int
    triangles: int


def _face_components(mesh: bpy.types.Mesh, selected_faces: set[int]) -> list[list[int]]:
    faces_by_edge: dict[tuple[int, int], list[int]] = {}
    for face_index in sorted(selected_faces):
        polygon = mesh.polygons[face_index]
        vertices = list(polygon.vertices)
        for index, first in enumerate(vertices):
            second = vertices[(index + 1) % len(vertices)]
            edge = (min(first, second), max(first, second))
            faces_by_edge.setdefault(edge, []).append(face_index)
    neighbors = {face_index: set() for face_index in selected_faces}
    for face_indices in faces_by_edge.values():
        if len(face_indices) == 2:
            first, second = sorted(face_indices)
            neighbors[first].add(second)
            neighbors[second].add(first)
    components: list[list[int]] = []
    remaining = set(selected_faces)
    while remaining:
        start = min(remaining)
        pending = [start]
        remaining.remove(start)
        component: list[int] = []
        while pending:
            face_index = pending.pop()
            component.append(face_index)
            for neighbor in sorted(neighbors[face_index], reverse=True):
                if neighbor in remaining:
                    remaining.remove(neighbor)
                    pending.append(neighbor)
        components.append(sorted(component))
    return sorted(components, key=lambda value: (-len(value), value[0]))


def select_upper_garment_faces(body: bpy.types.Object, female: bool) -> list[int]:
    mesh = body.data
    bottom_z, sleeve_bottom_z = ((1.015, 1.205) if female else (1.115, 1.285))
    neck_center_z, neck_raise, neck_radius = ((1.495, .060, .225) if female else (1.665, .070, .330))
    torso_radius, sleeve_radius = ((.285, .395) if female else (.405, .525))
    selected: set[int] = set()
    for polygon in mesh.polygons:
        points = [mesh.vertices[index].co for index in polygon.vertices]
        centroid = sum(points, Vector()) / len(points)
        radius = abs(centroid.x)
        neckline_z = neck_center_z + neck_raise * min(1.0, (radius / neck_radius) ** 2)
        inside_torso = radius <= torso_radius
        inside_sleeve = centroid.z >= sleeve_bottom_z and radius <= sleeve_radius
        if bottom_z <= centroid.z <= neckline_z and (inside_torso or inside_sleeve):
            selected.add(polygon.index)
    components = _face_components(mesh, selected)
    if not components or len(components[0]) < 32:
        raise RuntimeError("Upper garment selection did not produce a usable connected component.")
    # 僅保留最大連通區，避免手指或腋下的小型面島變成漂浮布片。
    return components[0]


def _copy_skin_weights(body: bpy.types.Object, garment: bpy.types.Object,
                       source_indices: list[int]) -> None:
    groups = sorted(body.vertex_groups, key=lambda value: value.index)
    target_groups = [garment.vertex_groups.new(name=value.name) for value in groups]
    for target_index, source_index in enumerate(source_indices):
        source_vertex = body.data.vertices[source_index]
        for membership in sorted(source_vertex.groups, key=lambda value: value.group):
            target_groups[membership.group].add([target_index], membership.weight, "REPLACE")


def _normalize_skin_weights(value: bpy.types.Object) -> None:
    groups_by_index = {group.index: group for group in value.vertex_groups}
    for vertex in value.data.vertices:
        influences = sorted(
            ((membership.weight, groups_by_index[membership.group].name)
             for membership in vertex.groups if membership.weight > 0.0),
            key=lambda item: (-item[0], item[1]),
        )[:4]
        total = sum(weight for weight, _ in influences)
        if not influences or total <= 1.0e-12:
            raise RuntimeError("Garment source contains a vertex without usable skin weights.")
        for group in value.vertex_groups:
            group.remove([vertex.index])
        for weight, name in influences:
            value.vertex_groups[name].add([vertex.index], weight / total, "REPLACE")


def _boundary_components(edges: list[tuple[int, int]]) -> list[list[int]]:
    neighbors: dict[int, set[int]] = {}
    for first, second in edges:
        neighbors.setdefault(first, set()).add(second)
        neighbors.setdefault(second, set()).add(first)
    if any(len(values) != 2 for values in neighbors.values()):
        raise RuntimeError("Garment boundary is not composed of closed loops.")
    components: list[list[int]] = []
    remaining = set(neighbors)
    while remaining:
        start = min(remaining)
        pending = [start]
        remaining.remove(start)
        component: list[int] = []
        while pending:
            vertex_index = pending.pop()
            component.append(vertex_index)
            for neighbor in sorted(neighbors[vertex_index], reverse=True):
                if neighbor in remaining:
                    remaining.remove(neighbor)
                    pending.append(neighbor)
        components.append(sorted(component))
    return sorted(components, key=lambda values: values[0])


def _ordered_boundary_loop(edges: list[tuple[int, int]], component: list[int]) -> list[int]:
    component_set = set(component)
    directed = {first: second for first, second in edges
                if first in component_set and second in component_set}
    if len(directed) != len(component):
        raise RuntimeError("Garment boundary winding is inconsistent.")
    start = min(component)
    ordered = [start]
    current = start
    while True:
        current = directed.get(current, -1)
        if current == start:
            break
        if current < 0 or current in ordered or len(ordered) >= len(component):
            raise RuntimeError("Garment boundary loop traversal failed.")
        ordered.append(current)
    if len(ordered) != len(component):
        raise RuntimeError("Garment boundary loop does not cover its component.")
    return ordered


def _delete_covered_body_faces(body: bpy.types.Object, face_indices: list[int]) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.ops.object.mode_set(mode="OBJECT")
    for face_index in face_indices:
        body.data.polygons[face_index].select = True
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.delete(type="ONLY_FACE")
    bpy.ops.object.mode_set(mode="OBJECT")
    body.select_set(False)


def create_upper_garment_shell(body: bpy.types.Object, rig: bpy.types.Object,
                               material: bpy.types.Material, female: bool) -> tuple[bpy.types.Object, GarmentShellMetrics]:
    source_mesh = body.data
    # glTF 僅保留四組蒙皮影響；匯出前先用固定規則裁切並正規化，避免 Blender 隱式選擇。
    _normalize_skin_weights(body)
    source_mesh.update(calc_edges=True)
    selected_faces = select_upper_garment_faces(body, female)
    selected_set = set(selected_faces)
    source_vertices = sorted({index for face_index in selected_faces for index in source_mesh.polygons[face_index].vertices})
    source_to_outer = {source_index: target_index for target_index, source_index in enumerate(source_vertices)}
    thickness = .0030 if female else .0035

    positions = [tuple(source_mesh.vertices[index].co + source_mesh.vertices[index].normal.normalized() * thickness)
                 for index in source_vertices]
    faces: list[tuple[int, ...]] = []
    loop_uvs: list[list[tuple[float, float]]] = []
    smooth_faces: list[bool] = []
    source_uv = source_mesh.uv_layers.active
    for face_index in selected_faces:
        polygon = source_mesh.polygons[face_index]
        faces.append(tuple(source_to_outer[index] for index in polygon.vertices))
        smooth_faces.append(True)
        if source_uv:
            loop_uvs.append([tuple(source_uv.data[loop_index].uv) for loop_index in polygon.loop_indices])
        else:
            loop_uvs.append([(0.0, 0.0)] * len(polygon.vertices))

    edge_owners: dict[tuple[int, int], list[tuple[int, int]]] = {}
    for face_index in selected_faces:
        polygon = source_mesh.polygons[face_index]
        vertices = list(polygon.vertices)
        for index, first in enumerate(vertices):
            second = vertices[(index + 1) % len(vertices)]
            edge_owners.setdefault((min(first, second), max(first, second)), []).append((first, second))
    boundary_edges = sorted(owner[0] for owner in edge_owners.values() if len(owner) == 1)
    boundary_vertices = sorted({index for edge in boundary_edges for index in edge})
    bottom_z, torso_radius, sleeve_radius = ((1.015, .285, .395) if female else (1.115, .405, .525))
    neck_center_z, neck_raise, neck_radius = ((1.495, .060, .225) if female else (1.665, .070, .330))
    components = _boundary_components(boundary_edges)
    if len(components) != 4:
        raise RuntimeError(f"Upper garment expected four boundary loops, received {len(components)}.")
    component_kinds: list[tuple[str, float, list[int]]] = []
    for component in components:
        average_z = sum(source_mesh.vertices[index].co.z for index in component) / len(component)
        average_radius = sum(abs(source_mesh.vertices[index].co.x) for index in component) / len(component)
        average_x = sum(source_mesh.vertices[index].co.x for index in component) / len(component)
        # 袖口的中心位於身體兩側；以中心位置分類比平均半徑穩定，因為低模袖口可能小於胸廓半徑。
        kind = "cuff" if abs(average_x) > torso_radius * .70 else ("waist" if average_z < bottom_z + .16 else "neck")
        component_kinds.append((kind, average_x, component))
    if [kind for kind, _, _ in component_kinds].count("neck") != 1 or [kind for kind, _, _ in component_kinds].count("waist") != 1 or [kind for kind, _, _ in component_kinds].count("cuff") != 2:
        raise RuntimeError(f"Upper garment boundary must contain one neck, one waist, and two cuff loops: {component_kinds}.")
    cuff_centers = sorted(center for kind, center, _ in component_kinds if kind == "cuff")
    if cuff_centers[0] >= -.05 or cuff_centers[1] <= .05:
        raise RuntimeError("Upper garment cuff loops are not separated across the body centerline.")

    # 腰與袖口沿來源邊界做短封邊；領口改由獨立窄飾條遮住低模鋸齒，不移動人體或衣物洞口。
    source_to_inner: dict[int, int] = {}
    regular_boundary_vertices = sorted(index for kind, _, component in component_kinds if kind != "neck"
                                       for index in component)
    for source_index in regular_boundary_vertices:
        source_to_inner[source_index] = len(positions)
        vertex = source_mesh.vertices[source_index]
        positions.append(tuple(vertex.co + vertex.normal.normalized() * .0005))
    for first, second in boundary_edges:
        if first not in source_to_inner:
            continue
        faces.append((source_to_outer[first], source_to_outer[second],
                      source_to_inner[second], source_to_inner[first]))
        loop_uvs.append([(0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0)])
        smooth_faces.append(False)

    neck_component = next(component for kind, _, component in component_kinds if kind == "neck")
    neck_loop = _ordered_boundary_loop(boundary_edges, neck_component)
    smoothed = {index: source_mesh.vertices[index].co.copy() for index in neck_loop}
    for _ in range(5):
        previous = {index: value.copy() for index, value in smoothed.items()}
        smoothed = {
            index: previous[index] * .50
            + previous[neck_loop[(position - 1) % len(neck_loop)]] * .25
            + previous[neck_loop[(position + 1) % len(neck_loop)]] * .25
            for position, index in enumerate(neck_loop)
        }
    collar_height = .010 if female else .012
    collar_inset = .004 if female else .006
    body_tree = BVHTree.FromPolygons(
        [vertex.co.copy() for vertex in source_mesh.vertices],
        [list(polygon.vertices) for polygon in source_mesh.polygons],
        all_triangles=False,
    )
    collar_top: dict[int, int] = {}
    collar_inner: dict[int, int] = {}
    for source_index in neck_loop:
        raised = smoothed[source_index].copy()
        raised.z += collar_height
        raised.x -= math.copysign(min(abs(raised.x), collar_inset), raised.x)
        nearest = body_tree.find_nearest(raised)
        if nearest is None:
            raise RuntimeError("Garment collar cannot project onto the source body.")
        projected, projected_normal, _, distance = nearest
        if not math.isfinite(distance) or distance > .05 or projected_normal.length <= 1.0e-8:
            raise RuntimeError("Garment collar projection is outside the source body.")
        projected_normal.normalize()
        top = projected + projected_normal * (thickness + .0015)
        inner = projected + projected_normal * .0005
        collar_top[source_index] = len(positions)
        positions.append(tuple(top))
        collar_inner[source_index] = len(positions)
        positions.append(tuple(inner))
    cumulative = [0.0]
    for position in range(1, len(neck_loop) + 1):
        first = smoothed[neck_loop[position - 1]]
        second = smoothed[neck_loop[position % len(neck_loop)]]
        cumulative.append(cumulative[-1] + (second - first).length)
    if cumulative[-1] <= 1.0e-8:
        raise RuntimeError("Garment collar has zero circumference.")
    for position, first in enumerate(neck_loop):
        second = neck_loop[(position + 1) % len(neck_loop)]
        u0, u1 = cumulative[position] / cumulative[-1], cumulative[position + 1] / cumulative[-1]
        # 領口飾條朝向與外殼表面一致；邊界owner方向在洞口會指向相反側，因此需反轉。
        faces.append((source_to_outer[second], source_to_outer[first],
                      collar_top[first], collar_top[second]))
        loop_uvs.append([(u1, 0.0), (u0, 0.0), (u0, 1.0), (u1, 1.0)])
        smooth_faces.append(True)
        faces.append((collar_top[first], collar_top[second],
                      collar_inner[second], collar_inner[first]))
        loop_uvs.append([(u0, 0.0), (u1, 0.0), (u1, 1.0), (u0, 1.0)])
        smooth_faces.append(False)

    mesh = bpy.data.meshes.new("UpperGarmentShellMesh")
    mesh.from_pydata(positions, [], faces)
    if mesh.validate(clean_customdata=False):
        raise RuntimeError("Garment mesh validation changed generated topology.")
    mesh.update(calc_edges=True)
    mesh.materials.append(material)
    uv = mesh.uv_layers.new(name=source_uv.name if source_uv else "UVMap")
    if len(mesh.polygons) != len(loop_uvs) or len(mesh.polygons) != len(smooth_faces):
        raise RuntimeError("Garment polygon metadata does not cover the generated topology.")
    for polygon, polygon_uvs, smooth in zip(mesh.polygons, loop_uvs, smooth_faces):
        if len(polygon.loop_indices) != len(polygon_uvs):
            raise RuntimeError("Garment UV loop topology changed unexpectedly.")
        for loop_index, value in zip(polygon.loop_indices, polygon_uvs):
            if not math.isfinite(value[0]) or not math.isfinite(value[1]):
                raise RuntimeError("Garment UV contains a non-finite value.")
            uv.data[loop_index].uv = value
        polygon.use_smooth = smooth

    garment = bpy.data.objects.new("UpperGarmentShell", mesh)
    bpy.context.scene.collection.objects.link(garment)
    repeated_sources = (source_vertices + regular_boundary_vertices
                        + [index for index in neck_loop for _ in range(2)])
    _copy_skin_weights(body, garment, repeated_sources)
    modifier = garment.modifiers.new(name="HumanoidSkin", type="ARMATURE")
    modifier.object = rig
    garment.parent = rig

    if len(_face_components(source_mesh, selected_set)) != 1:
        raise RuntimeError("Upper garment source region is not connected.")
    if not boundary_edges or any(polygon.area <= 1.0e-10 for polygon in mesh.polygons):
        raise RuntimeError("Upper garment shell contains an invalid boundary or degenerate face.")
    for target_index, source_index in enumerate(repeated_sources):
        expected = sorted((membership.group, round(membership.weight, 7))
                          for membership in source_mesh.vertices[source_index].groups)
        actual = sorted((membership.group, round(membership.weight, 7))
                        for membership in mesh.vertices[target_index].groups)
        if expected != actual:
            raise RuntimeError("Upper garment skin weights diverged from the source body.")

    # 領口保留一圈同材質背襯，避免刪面後的低模人體洞口穿過規整飾條；其餘覆蓋面仍移除。
    neck_vertices = set(neck_component)
    neck_guard_faces = {
        face_index for face_index in selected_faces
        if len(neck_vertices.intersection(source_mesh.polygons[face_index].vertices)) >= 2
    }
    if len(neck_guard_faces) < 8:
        raise RuntimeError("Garment collar does not have enough backing faces.")
    for face_index in neck_guard_faces:
        source_mesh.polygons[face_index].material_index = 5
    _delete_covered_body_faces(body, [index for index in selected_faces if index not in neck_guard_faces])

    metrics = GarmentShellMetrics(
        source_faces=len(selected_faces),
        outer_vertices=len(source_vertices),
        rim_vertices=len(boundary_vertices),
        boundary_edges=len(boundary_edges),
        collar_vertices=len(neck_loop) * 2,
        triangles=sum(len(polygon.vertices) - 2 for polygon in mesh.polygons),
    )
    return garment, metrics
