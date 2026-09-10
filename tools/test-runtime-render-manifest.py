#!/usr/bin/env python3
"""Fresh deterministic and hostile tests for the runtime render manifest."""

from __future__ import annotations

import copy
import importlib.util
import os
import pathlib
import subprocess
import tempfile


def require_failure(module, manifest, cooked_root, label):
    try:
        module.validate_manifest(manifest, cooked_root, verify_disk=True)
    except (module.ManifestError, KeyError, TypeError, ValueError):
        return
    raise AssertionError(f"hostile case was accepted: {label}")


def require_source_failure(module, asset_root, provenance, gltfpack, label):
    try:
        module.validate_tree_small_02_source_chain(
            asset_root, provenance=provenance, gltfpack=gltfpack)
    except (module.ManifestError, KeyError, TypeError, ValueError):
        return
    raise AssertionError(f"hostile source-chain case was accepted: {label}")


def main():
    tools_root = pathlib.Path(__file__).resolve().parent
    repo_root = tools_root.parent
    asset_root = repo_root / "engine" / "assets" / "realistic-cc0-first-batch"
    cooked_root = asset_root / "cooked"
    specification = importlib.util.spec_from_file_location("runtime_render_manifest", tools_root / "write-runtime-render-manifest.py")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    authoritative = module.load_json(cooked_root / "runtime-render-manifest.json")
    module.validate_manifest(authoritative, cooked_root)

    with tempfile.TemporaryDirectory(prefix="aevrynd-runtime-render-") as directory:
        temporary = pathlib.Path(directory)
        first = module.build_manifest(asset_root, cooked_root)
        second = module.build_manifest(asset_root, cooked_root)
        first_bytes = module.json_bytes(first)
        second_bytes = module.json_bytes(second)
        if first_bytes != second_bytes:
            raise AssertionError("A/B manifests are not byte-identical")
        module.validate_manifest(first, cooked_root)
        source_root = asset_root / "tree-small-02"
        provenance = module.load_json(source_root / "tree-small-02-provenance-receipt.json")
        gltfpack = module.load_json(source_root / "gltfpack-runtime-receipt.json")
        source_cases = []
        changed = copy.deepcopy(provenance); changed["validation"]["officialMd5AndBytesMatchAll"] = False; source_cases.append(("provenance-official-audit", changed, gltfpack))
        changed = copy.deepcopy(provenance); changed["files"][0]["bytes"] += 1; source_cases.append(("provenance-source-bytes", changed, gltfpack))
        changed = copy.deepcopy(gltfpack); changed["source"]["path"] = "D:/escape/tree_small_02_1k.gltf"; source_cases.append(("gltfpack-absolute-source", provenance, changed))
        changed = copy.deepcopy(gltfpack); changed["source"]["provenanceReceiptBytes"] -= 1; source_cases.append(("gltfpack-provenance-bytes", provenance, changed))
        changed = copy.deepcopy(gltfpack); changed["source"]["provenanceReceiptSha256"] = "0" * 64; source_cases.append(("gltfpack-provenance-hash", provenance, changed))
        changed = copy.deepcopy(gltfpack); changed["source"]["dependencies"][0]["path"] = "../tree_small_02.bin"; source_cases.append(("gltfpack-bin-traversal", provenance, changed))
        changed = copy.deepcopy(gltfpack); changed["source"]["dependencies"][0]["sha256"] = "0" * 64; source_cases.append(("gltfpack-bin-hash", provenance, changed))
        changed = copy.deepcopy(gltfpack); changed["levels"][0]["sha256"] = "0" * 64; source_cases.append(("gltfpack-output-hash", provenance, changed))
        for label, provenance_case, gltfpack_case in source_cases:
            require_source_failure(module, asset_root, provenance_case, gltfpack_case, label)
        semantic_fields = ("alphaMode", "alphaCutoff", "doubleSided", "materialClass")
        if any(set(semantic_fields) - set(material) for material in authoritative["materials"]):
            raise AssertionError("authoritative material rendering semantics are incomplete")
        if first_bytes != module.json_bytes(authoritative):
            raise AssertionError("generated manifest differs from the authoritative semantic manifest")
        legacy = copy.deepcopy(authoritative)
        for material in legacy["materials"]:
            for field in semantic_fields:
                material.pop(field)
        # 中文註解：獨立 legacy fixture 改變 manifest 長度，重算自我描述封包收據。
        for _ in range(8):
            manifest_bytes = len(module.json_bytes(legacy))
            if legacy["package"]["manifestBytes"] == manifest_bytes:
                break
            legacy["package"]["manifestBytes"] = manifest_bytes
            legacy["package"]["packageBytes"] = legacy["package"]["payloadBytes"] + manifest_bytes
        module.validate_manifest(legacy, cooked_root)
        (temporary / "a.json").write_bytes(first_bytes)
        (temporary / "b.json").write_bytes(second_bytes)

        cases = []
        changed = copy.deepcopy(authoritative); changed["schemaVersion"] = 2; cases.append(("schema", changed))
        changed = copy.deepcopy(authoritative); changed["files"][0]["path"] = "../escape.dds"; cases.append(("traversal", changed))
        changed = copy.deepcopy(authoritative); changed["files"][0]["path"] = "materials/file.dds:stream"; cases.append(("ADS", changed))
        changed = copy.deepcopy(authoritative); changed["files"][1]["path"] = changed["files"][0]["path"]; cases.append(("duplicate", changed))
        changed = copy.deepcopy(authoritative); changed["materials"][0]["baseColorPath"] = changed["materials"][0]["normalPath"]; cases.append(("unreferenced", changed))
        changed = copy.deepcopy(authoritative); changed["materials"][0]["baseColorPath"] = "materials/missing.dds"; cases.append(("missing", changed))
        changed = copy.deepcopy(authoritative); changed["files"][0]["sha256"] = "0" * 64; cases.append(("hash", changed))
        changed = copy.deepcopy(authoritative); changed["files"][0]["bytes"] += 1; cases.append(("bytes", changed))
        changed = copy.deepcopy(authoritative); changed["files"][0]["format"] = "BC1_UNORM"; cases.append(("format", changed))
        dds_index = next(index for index, item in enumerate(authoritative["files"]) if item["mips"] is not None)
        changed = copy.deepcopy(authoritative); changed["files"][dds_index]["mips"] += 1; cases.append(("mips", changed))
        changed = copy.deepcopy(authoritative); changed["files"][dds_index]["dimensions"]["width"] += 1; cases.append(("dimensions", changed))
        changed = copy.deepcopy(authoritative); changed["files"].pop(); cases.append(("file-count", changed))
        changed = copy.deepcopy(authoritative); changed["exactAssetSet"]["count"] -= 1; cases.append(("exact-asset-count", changed))
        changed = copy.deepcopy(authoritative); changed["assets"].pop(); cases.append(("asset-metadata-count", changed))
        changed = copy.deepcopy(authoritative); changed["materials"].pop(); cases.append(("production-material-count", changed))
        changed = copy.deepcopy(authoritative); changed["meshParts"].pop(); cases.append(("production-mesh-part-count", changed))
        changed = copy.deepcopy(authoritative); changed["package"]["packageFileCount"] -= 1; cases.append(("package-derived-count", changed))
        changed = copy.deepcopy(authoritative); changed["materials"][0]["alphaCutoff"] = -0.01; cases.append(("alpha-cutoff-low", changed))
        changed = copy.deepcopy(authoritative); changed["materials"][0]["alphaCutoff"] = 1.01; cases.append(("alpha-cutoff-high", changed))
        changed = copy.deepcopy(authoritative); changed["materials"][0]["alphaCutoff"] = float("inf"); cases.append(("alpha-cutoff-non-finite", changed))
        changed = copy.deepcopy(authoritative); changed["materials"][0]["doubleSided"] = "false"; cases.append(("double-sided-type", changed))
        changed = copy.deepcopy(authoritative); changed["materials"][0]["materialClass"] = "terrain"; cases.append(("material-class", changed))
        foliage_index = next(index for index, material in enumerate(authoritative["materials"]) if material["materialClass"] == "foliage")
        changed = copy.deepcopy(authoritative); changed["materials"][foliage_index]["alphaMode"] = "opaque"; cases.append(("opaque-foliage", changed))
        changed = copy.deepcopy(authoritative); changed["materials"][foliage_index]["doubleSided"] = False; cases.append(("single-sided-foliage", changed))
        for label, manifest in cases:
            require_failure(module, manifest, cooked_root, label)

        # 中文註解：能建立符號連結時，驗證器必須在讀檔前拒絕 reparse point。
        target = pathlib.Path(authoritative["files"][0]["path"])
        link_root = temporary / "cooked"
        link_root.mkdir()
        link = link_root / "linked.aemesh"
        try:
            link.symlink_to(cooked_root / target)
        except OSError:
            # 中文註解：Windows 未啟用開發人員模式時，改用不需管理員權限的目錄 junction。
            junction = link_root / "linked-directory"
            result = subprocess.run(
                ["cmd.exe", "/d", "/c", "mklink", "/J", str(junction), str(cooked_root)],
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode != 0:
                raise AssertionError(f"could not create reparse test fixture: {result.stderr or result.stdout}")
            try:
                module.checked_disk_path(link_root, f"linked-directory/{target.as_posix()}")
            except module.ManifestError:
                print("reparse-hostile=pass")
            else:
                raise AssertionError("directory reparse point was accepted")
            finally:
                os.rmdir(junction)
        else:
            try:
                module.checked_disk_path(link_root, "linked.aemesh")
            except module.ManifestError:
                print("reparse-hostile=pass")
            else:
                raise AssertionError("reparse point was accepted")

    print("runtime-render-manifest-tests=pass; deterministic=3-way; manifest-hostile=24; source-chain-hostile=8; legacy=pass")


if __name__ == "__main__":
    main()
