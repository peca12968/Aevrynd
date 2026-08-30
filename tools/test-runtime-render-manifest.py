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
        if first_bytes != second_bytes or first_bytes != module.json_bytes(authoritative):
            raise AssertionError("A/B manifests are not byte-identical")
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

    print("runtime-render-manifest-tests=pass; deterministic=3-way; hostile=12")


if __name__ == "__main__":
    main()
