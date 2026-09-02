#!/usr/bin/env python3
"""Build and validate the deterministic first-batch runtime render manifest."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import os
import pathlib
import stat
import struct
import sys
from collections import Counter, defaultdict

FORMAT_NAMES = {
    34: "R16G16_FLOAT",
    83: "BC5_UNORM",
    95: "BC6H_UF16",
    98: "BC7_UNORM",
    99: "BC7_UNORM_SRGB",
}
MESH_MATERIALS = {
    ("boulder-01", 0): "boulder_01:slot-0",
    ("coastal-cliff-01", 0): "coastal_cliff_01:slot-0",
    ("dead-tree-trunk-02", 0): "dead_tree_trunk_02:slot-0",
    ("island-tree-01", 0): "island_tree_01:trunk",
    ("island-tree-01", 1): "island_tree_01:leaves",
    ("island-tree-01", 2): "island_tree_01:branches",
}
STANDALONE_ROLES = {
    "forest_ground_04": "terrain",
    "Ground108": "ground",
    "RoofingTiles014B": "roof",
    "Bricks100": "wall",
    "stony_dirt_path": "path",
}


class ManifestError(ValueError):
    pass


def load_json(path: pathlib.Path):
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def json_bytes(value) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2, sort_keys=False) + "\n").encode("utf-8")


def safe_relative_path(value: str) -> pathlib.PurePosixPath:
    if not isinstance(value, str) or not value or "\\" in value or ":" in value:
        raise ManifestError(f"unsafe path: {value!r}")
    pure = pathlib.PurePosixPath(value)
    if pure.is_absolute() or any(part in ("", ".", "..") for part in pure.parts):
        raise ManifestError(f"unsafe path: {value!r}")
    try:
        value.encode("ascii")
    except UnicodeEncodeError as error:
        raise ManifestError(f"non-ASCII runtime path: {value!r}") from error
    return pure


def checked_disk_path(root: pathlib.Path, relative: str) -> pathlib.Path:
    pure = safe_relative_path(relative)
    current = root
    for part in pure.parts:
        current = current / part
        if current.exists() or current.is_symlink():
            details = current.lstat()
            attributes = getattr(details, "st_file_attributes", 0)
            reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
            if current.is_symlink() or attributes & reparse_flag:
                raise ManifestError(f"reparse point is forbidden: {relative}")
    return current


def source_metadata(asset, inputs):
    return {
        "provider": asset["source"],
        "officialPage": asset["officialPage"],
        "inputs": [{"path": item["path"], "sha256": item["sha256"]} for item in inputs],
    }


def parse_aemesh(path: pathlib.Path):
    data = path.read_bytes()
    if len(data) < 16:
        raise ManifestError(f"short AEMESH: {path}")
    magic, version, vertices, indices = struct.unpack_from("<4sIII", data)
    if magic != b"AEM3" or version != 1 or len(data) != 16 + vertices * 28 + indices * 4:
        raise ManifestError(f"invalid AEMESH structure: {path}")
    return vertices, indices


def parse_dds(path: pathlib.Path):
    data = path.read_bytes()
    if len(data) < 148 or data[:4] != b"DDS " or data[84:88] != b"DX10":
        raise ManifestError(f"invalid DX10 DDS header: {path}")
    height, width, mips = struct.unpack_from("<III", data, 12)[0], struct.unpack_from("<I", data, 16)[0], struct.unpack_from("<I", data, 28)[0]
    dxgi, dimension, misc, array_size = struct.unpack_from("<IIIII", data, 128)[:4]
    mips = max(1, mips)
    faces = 6 if misc & 4 else 1
    total = 0
    for _ in range(array_size * faces):
        w, h = width, height
        for _mip in range(mips):
            if dxgi in (83, 95, 98, 99):
                total += max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * 16
            elif dxgi == 34:
                total += w * h * 4
            else:
                raise ManifestError(f"unsupported DDS format {dxgi}: {path}")
            w, h = max(1, w // 2), max(1, h // 2)
    if 148 + total != len(data):
        raise ManifestError(f"DDS subresource size mismatch: {path}")
    return {"width": width, "height": height, "mips": mips, "format": FORMAT_NAMES[dxgi], "arraySize": array_size, "faces": faces, "dimension": dimension}


def build_manifest(asset_root: pathlib.Path, cooked_root: pathlib.Path):
    runtime = load_json(asset_root / "runtime-asset-manifest.json")
    runtime_assets = {item["id"]: item for item in runtime["assets"]}
    materials_receipt = load_json(cooked_root / "materials" / "material-bake-receipt.json")
    ibl_receipt = load_json(cooked_root / "ibl" / "ibl-bake-receipt.json")
    mesh_receipts = [load_json(path) for path in sorted((cooked_root / "meshes").rglob("*-aemesh-receipt.json"))]

    files = []
    materials = []
    mesh_parts = []
    material_ids = defaultdict(list)
    mesh_part_ids = defaultdict(list)

    for asset in materials_receipt["assets"]:
        provenance = runtime_assets[asset["assetId"]]
        for material in asset["materials"]:
            material_id = f"{asset['assetId']}:{material['slot']}"
            outputs = {item["channel"]: item for item in material["outputs"]}
            paths = {}
            for channel in ("base", "arm", "normal"):
                output = outputs[channel]
                relative = f"materials/{output['path']}"
                disk = cooked_root / pathlib.PurePosixPath(relative)
                details = parse_dds(disk)
                entry = {
                    "path": relative,
                    "bytes": output["bytes"],
                    "sha256": output["sha256"],
                    "format": details["format"],
                    "mips": details["mips"],
                    "dimensions": {"width": details["width"], "height": details["height"], "arraySize": details["arraySize"], "faces": details["faces"]},
                    "source": source_metadata(provenance, list(material["sources"].values())),
                    "license": "CC0-1.0",
                    "commercialUse": True,
                }
                files.append(entry)
                paths[channel] = relative
            material_entry = {
                "id": material_id,
                "assetId": asset["assetId"],
                "slot": material["slot"],
                "baseColorPath": paths["base"],
                "surfacePath": paths["arm"],
                "normalPath": paths["normal"],
                "surfaceEncoding": "packed-orm",
                "alphaMode": material["alphaMode"],
                "alphaCutoff": material.get("alphaCutoff", 0.5),
                "doubleSided": material.get("doubleSided", False),
                "materialClass": material.get("materialClass", "surface"),
            }
            if asset["assetId"] in STANDALONE_ROLES:
                material_entry["standaloneRole"] = STANDALONE_ROLES[asset["assetId"]]
            materials.append(material_entry)
            material_ids[asset["assetId"]].append(material_id)

    grouped = defaultdict(dict)
    mesh_dimensions = {}
    for receipt in mesh_receipts:
        asset_name = receipt["assetName"]
        asset_id = asset_name.replace("-", "_")
        provenance = runtime_assets[asset_id]
        source_inputs = [{"path": f"{asset_name}/{receipt['sourceFile']}", "sha256": receipt["sourceSha256"]}]
        for output in receipt["outputs"]:
            relative = f"meshes/{asset_name}/{output['file']}"
            disk = cooked_root / pathlib.PurePosixPath(relative)
            vertices, indices = parse_aemesh(disk)
            if vertices != output["vertices"] or indices != output["indices"]:
                raise ManifestError(f"AEMESH receipt mismatch: {relative}")
            files.append({
                "path": relative,
                "bytes": output["bytes"],
                "sha256": output["sha256"],
                "format": "AEMESH-v1",
                "mips": None,
                "dimensions": {
                    "vertices": vertices,
                    "indices": indices,
                    "triangles": output["triangles"],
                    "boundsMinimum": output["boundsMinimumGltf"],
                    "boundsMaximum": output["boundsMaximumGltf"],
                },
                "source": source_metadata(provenance, source_inputs),
                "license": "CC0-1.0",
                "commercialUse": True,
            })
            key = (asset_name, output["materialIndex"])
            grouped[key][receipt["lod"]] = relative
            mesh_dimensions[relative] = output

    for key in sorted(grouped):
        asset_name, material_index = key
        lod_map = grouped[key]
        if sorted(lod_map) != [0, 1, 2] or key not in MESH_MATERIALS:
            raise ManifestError(f"incomplete or unknown mesh part: {key}")
        material_id = MESH_MATERIALS[key]
        part_id = f"{asset_name}:material-{material_index}"
        mesh_parts.append({"id": part_id, "assetId": asset_name.replace("-", "_"), "materialId": material_id, "lodPaths": [lod_map[index] for index in range(3)]})
        mesh_part_ids[asset_name.replace("-", "_")].append(part_id)

    ibl_paths = {}
    ibl_provenance = runtime_assets["xanderklinge"]
    for output in ibl_receipt["outputs"]:
        relative = f"ibl/{output['path']}"
        disk = cooked_root / pathlib.PurePosixPath(relative)
        details = parse_dds(disk)
        files.append({
            "path": relative,
            "bytes": output["bytes"],
            "sha256": output["sha256"],
            "format": details["format"],
            "mips": details["mips"],
            "dimensions": {"width": details["width"], "height": details["height"], "arraySize": details["arraySize"], "faces": details["faces"]},
            "source": source_metadata(ibl_provenance, [{"path": "xanderklinge/source/xanderklinge_1k.hdr", "sha256": ibl_receipt["sourceSha256"]}]),
            "license": "CC0-1.0",
            "commercialUse": True,
        })
        if "specular" in output["path"]:
            ibl_paths["specularPath"] = relative
        elif "irradiance" in output["path"]:
            ibl_paths["irradiancePath"] = relative
        elif "brdf" in output["path"]:
            ibl_paths["brdfLutPath"] = relative

    files.sort(key=lambda item: item["path"])
    materials.sort(key=lambda item: item["id"])
    mesh_parts.sort(key=lambda item: item["id"])
    assets = []
    for asset in runtime["assets"]:
        asset_id = asset["id"]
        details = {
            "id": asset_id,
            "name": asset["assetName"],
            "kind": asset["kind"],
            "provider": asset["source"],
            "officialPage": asset["officialPage"],
            "license": asset["license"],
            "commercialUse": asset["commercialUse"],
        }
        if material_ids[asset_id]:
            details["materialIds"] = sorted(material_ids[asset_id])
        if mesh_part_ids[asset_id]:
            details["meshPartIds"] = sorted(mesh_part_ids[asset_id])
        if asset_id == "xanderklinge":
            details["iblBinding"] = "environment"
        assets.append(details)
    assets.sort(key=lambda item: item["id"])

    payload_bytes = sum(item["bytes"] for item in files)
    manifest = {
        "schemaVersion": 1,
        "manifestType": "aevrynd-runtime-render-package",
        "exactAssetSet": {"id": runtime["catalogId"], "count": 10, "assetIds": sorted(runtime_assets)},
        "licensePolicy": {"required": "CC0-1.0", "commercialUse": True, "provenanceEmbedded": True},
        "files": files,
        "materials": materials,
        "meshParts": mesh_parts,
        "ibl": {**ibl_paths, "rotationRadiansMilli": 0},
        "runtimeSystems": {
            "waterOptics": {
                "fileBacked": False,
                "binding": "engine-runtime",
                "features": ["absorption", "fresnel", "reflection", "refraction", "roughness"],
            }
        },
        "assets": assets,
        "package": {
            "payloadFileCount": len(files),
            "payloadBytes": payload_bytes,
            "manifestPath": "runtime-render-manifest.json",
            "manifestBytes": 0,
            "packageFileCount": len(files) + 1,
            "packageBytes": payload_bytes,
            "manifestHashPolicy": "external-envelope",
            "excludedClasses": ["GLB", "HDR", "JPG", "JSON-sidecar", "raw-receipt", "ToolCache"],
        },
    }
    # 中文註解：manifestBytes 會影響自身長度，迭代至固定點以保持可重現。
    for _ in range(8):
        length = len(json_bytes(manifest))
        if manifest["package"]["manifestBytes"] == length:
            break
        manifest["package"]["manifestBytes"] = length
        manifest["package"]["packageBytes"] = payload_bytes + length
    return manifest


def validate_manifest(manifest, cooked_root: pathlib.Path, verify_disk=True):
    if manifest.get("schemaVersion") != 1 or manifest.get("manifestType") != "aevrynd-runtime-render-package":
        raise ManifestError("unsupported manifest schema")
    exact = manifest.get("exactAssetSet", {})
    if exact.get("count") != 10 or len(exact.get("assetIds", [])) != 10 or len(set(exact["assetIds"])) != 10:
        raise ManifestError("exact10 contract failed")
    files = manifest.get("files", [])
    if len(files) != 54:
        raise ManifestError("runtime payload must contain exactly 54 files")
    paths = [item.get("path") for item in files]
    if len(set(paths)) != len(paths):
        raise ManifestError("duplicate runtime file path")
    if paths != sorted(paths):
        raise ManifestError("runtime files are not deterministically ordered")
    file_map = {item["path"]: item for item in files}
    for item in files:
        relative = item["path"]
        safe_relative_path(relative)
        if any(token in relative.lower() for token in ("toolcache", ".glb", ".hdr", ".jpg", "receipt.json", ".materials.json")):
            raise ManifestError(f"non-runtime payload included: {relative}")
        if item.get("license") != "CC0-1.0" or item.get("commercialUse") is not True:
            raise ManifestError(f"license gate failed: {relative}")
        source = item.get("source", {})
        if not source.get("officialPage") or not source.get("provider") or not source.get("inputs"):
            raise ManifestError(f"provenance missing: {relative}")
        if verify_disk:
            disk = checked_disk_path(cooked_root, relative)
            if not disk.is_file():
                raise ManifestError(f"runtime file missing: {relative}")
            if disk.stat().st_size != item.get("bytes") or sha256(disk) != item.get("sha256"):
                raise ManifestError(f"runtime file identity mismatch: {relative}")
            if item["format"] == "AEMESH-v1":
                vertices, indices = parse_aemesh(disk)
                if item.get("mips") is not None or vertices != item["dimensions"].get("vertices") or indices != item["dimensions"].get("indices"):
                    raise ManifestError(f"AEMESH metadata mismatch: {relative}")
            else:
                details = parse_dds(disk)
                dimensions = item.get("dimensions", {})
                if item.get("format") != details["format"] or item.get("mips") != details["mips"] or any(dimensions.get(key) != details[key] for key in ("width", "height", "arraySize", "faces")):
                    raise ManifestError(f"DDS metadata mismatch: {relative}")

    material_ids = [item.get("id") for item in manifest.get("materials", [])]
    part_ids = [item.get("id") for item in manifest.get("meshParts", [])]
    asset_ids = [item.get("id") for item in manifest.get("assets", [])]
    if len(material_ids) != 11 or len(set(material_ids)) != 11 or len(part_ids) != 6 or len(set(part_ids)) != 6 or len(asset_ids) != 10 or len(set(asset_ids)) != 10:
        raise ManifestError("material, mesh part, or asset identity contract failed")
    if set(asset_ids) != set(exact["assetIds"]):
        raise ManifestError("asset metadata differs from exact10")
    references = []
    for material in manifest["materials"]:
        if material.get("assetId") not in exact["assetIds"] or not material.get("slot") or material.get("id") != f"{material['assetId']}:{material['slot']}":
            raise ManifestError("material asset or slot binding failed")
        if material.get("surfaceEncoding") != "packed-orm":
            raise ManifestError("unsupported surface encoding")
        if material.get("alphaMode", "opaque") not in ("opaque", "mask"):
            raise ManifestError("unsupported material alpha mode")
        alpha_cutoff = material.get("alphaCutoff", 0.5)
        if type(alpha_cutoff) not in (int, float) or not 0.0 <= alpha_cutoff <= 1.0 or not math.isfinite(float(alpha_cutoff)):
            raise ManifestError("invalid material alpha cutoff")
        if type(material.get("doubleSided", False)) is not bool:
            raise ManifestError("invalid material double-sided flag")
        material_class = material.get("materialClass", "surface")
        if material_class not in ("surface", "foliage"):
            raise ManifestError("unsupported material class")
        if material_class == "foliage" and (material.get("alphaMode", "opaque") != "mask" or material.get("doubleSided", False) is not True):
            raise ManifestError("foliage material must be masked and double-sided")
        references.extend([material.get("baseColorPath"), material.get("surfacePath"), material.get("normalPath")])
    for part in manifest["meshParts"]:
        if part.get("materialId") not in material_ids or len(part.get("lodPaths", [])) != 3:
            raise ManifestError("mesh material or LOD binding failed")
        references.extend(part["lodPaths"])
    ibl = manifest.get("ibl", {})
    references.extend([ibl.get("specularPath"), ibl.get("irradiancePath"), ibl.get("brdfLutPath")])
    if ibl.get("rotationRadiansMilli") != 0:
        raise ManifestError("IBL orientation contract failed")
    counts = Counter(references)
    if set(counts) != set(file_map) or any(value != 1 for value in counts.values()):
        raise ManifestError("payload reference set is missing, duplicated, or unreferenced")
    water = manifest.get("runtimeSystems", {}).get("waterOptics", {})
    if water.get("fileBacked") is not False or water.get("binding") != "engine-runtime":
        raise ManifestError("runtime water optics contract failed")
    package = manifest.get("package", {})
    payload_bytes = sum(item["bytes"] for item in files)
    encoded_length = len(json_bytes(manifest))
    if package.get("payloadFileCount") != 54 or package.get("payloadBytes") != payload_bytes or package.get("manifestPath") != "runtime-render-manifest.json" or package.get("manifestBytes") != encoded_length or package.get("packageFileCount") != 55 or package.get("packageBytes") != payload_bytes + encoded_length or package.get("manifestHashPolicy") != "external-envelope":
        raise ManifestError("package exact-set accounting failed")
    if verify_disk:
        actual = {path.relative_to(cooked_root).as_posix() for path in (cooked_root / "meshes").rglob("*.aemesh")}
        actual |= {path.relative_to(cooked_root).as_posix() for folder in ("materials", "ibl") for path in (cooked_root / folder).rglob("*.dds")}
        if actual != set(file_map):
            raise ManifestError("cooked GPU payload disk set differs from manifest")
    return True


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--asset-root", type=pathlib.Path, required=True)
    parser.add_argument("--cooked-root", type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("--validate-only", action="store_true")
    arguments = parser.parse_args(argv)
    asset_root = arguments.asset_root.resolve()
    cooked_root = (arguments.cooked_root or asset_root / "cooked").resolve()
    output = (arguments.output or cooked_root / "runtime-render-manifest.json").resolve()
    if arguments.validate_only:
        manifest = load_json(output)
    else:
        manifest = build_manifest(asset_root, cooked_root)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(json_bytes(manifest))
    validate_manifest(manifest, cooked_root)
    print(json.dumps({"status": "pass", "path": str(output), "files": 54, "bytes": output.stat().st_size, "sha256": sha256(output), "packageBytes": manifest["package"]["packageBytes"]}, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ManifestError, KeyError, OSError, ValueError) as error:
        print(f"runtime render manifest failed: {error}", file=sys.stderr)
        raise SystemExit(1)
