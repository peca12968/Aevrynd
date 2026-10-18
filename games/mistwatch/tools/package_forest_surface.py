"""Explicit portable Forest Surface WIP package; never a publication gate."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat
import sys


GROUPS = (
    "mistwatch-boulders", "mistwatch-broadleaf", "mistwatch-canopy-preview",
    "mistwatch-ferns", "mistwatch-forest", "mistwatch-grass",
    "mistwatch-materials", "mistwatch-wildlife",
)
HEIGHTS = (
    ("moss-height/Ground037_SurfaceOrmHeight_Level0.png", 1531546,
     "012D2378AA6E62CCA05AD66D69B512966D5894694B7B24D39AFD0CA7CA655F38"),
    ("leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png", 9102955,
     "9F3259F4772F294EDCA1DE03FA4E8D7ED65C0924238B1A6D0B27B8FED7C0EF85"),
)
PROVENANCE = (
    ("manifest.json", 2357, "784EB6D76CAD2DD2624D167D39B9A80DDA405D626155EDCA4DC88FB431AEFF5B"),
    ("LICENSE.txt", 919, "CCACA0F3711DA7EB29F3D5924C186968E0043015CA6618C305B0A07298DF0ADD"),
)
WOOD = (
    ("base-color.jpg", 943056,
     "698C09B337375A9AAE12A76AD09FF2FDA11D735E6C1BBB18D72875BD61988DB6"),
    ("roughness.jpg", 807719,
     "C79F640DC2D7EF81D69DE463EF6676B7C540D598FB0E462A6EB39EB5462770A9"),
    ("normal-dx.png", 4886175,
     "743411ACF7332D8176D4E402237BB4B2D28A89FDC9BA160BB1EEE2DE6FBFB74B"),
)
COHORT = "authored-leaf-cluster-full-v3"
MAPPING_RELATIVE = "games/mistwatch/native/tests/qualification/runtime-profiles-v1.json"
LEAF_RELATIVE = "games/mistwatch/native/tests/qualification/runtime-leaf-cluster-full-v3.json"
MAPPING_SHA = "1A0CC543A038BE1B98A5F32E27D9A37C11D53921536743B928FA36F712A8078A"
LEAF_SHA = "DE5E956E5191F620301D8088471C5FCF4BCADF3071BA51AA808EA6832C49AC4A"
LEGACY_ADDITIONS = frozenset({
    "mistwatch-boulders/materials/slot-0-base-source-srgb-v1.dds",
    "mistwatch-boulders/meshes/boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh",
    "mistwatch-materials/poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png",
    "mistwatch-materials/poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png",
    "mistwatch-materials/poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png",
    "mistwatch-materials/poly-haven/forest_leaves_04/DERIVATION.txt",
    "mistwatch-materials/poly-haven/forest_leaves_04/LICENSE.txt",
})
REPARSE = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
REJECT_DIRS = frozenset({
    "cache", "caches", "material-cache", "shader-cache", "__pycache__",
    "evidence", "test-artifacts", "test-workspaces", "saves", "savedgames",
    "user-data", ".cache", ".git", ".svn",
})
REJECT_SUFFIXES = frozenset({
    ".tmp", ".partial", ".log", ".pyc", ".pyo", ".bak", ".cache", ".lock", ".dmp",
})
EXE_NAME = "Aevrynd-Mistwatch.exe"
TOOL_RELATIVE = "games/mistwatch/tools/package_forest_surface.py"
LAUNCHER = (
    '@echo off\r\n'
    'cd /d "%~dp0"\r\n'
    'start "" "%~dp0Aevrynd-Mistwatch.exe" --forest-surface-candidate\r\n'
).encode("ascii")


def require(value: bool, reason: str) -> None:
    if not value:
        raise ValueError(reason)


def safe_name(name: str) -> None:
    require(bool(name) and name not in {".", ".."}, "PACKAGE_PATH_COMPONENT")
    require(not any(ord(c) < 32 or c in '<>:"/\\|?*' for c in name),
            "PACKAGE_PATH_CHARACTERS")
    require(not name.endswith((".", " ")), "PACKAGE_PATH_TRAILING_CHARACTER")
    stem = name.split(".", 1)[0].casefold()
    require(stem not in {"con", "prn", "aux", "nul"}
            and stem not in {f"com{i}" for i in range(1, 10)}
            and stem not in {f"lpt{i}" for i in range(1, 10)},
            "PACKAGE_PATH_RESERVED_DEVICE")


def inspect_path(path: Path, *, exists: bool = True) -> None:
    # 逐層檢查；resolve 後才檢查會掩蓋 Windows junction 或 symbolic link。
    require(path.is_absolute() and ".." not in path.parts, "PACKAGE_ABSOLUTE_PATH")
    for current in reversed((path, *path.parents)):
        try:
            info = current.lstat()
        except FileNotFoundError:
            require(not exists and current == path, "PACKAGE_PATH_MISSING")
            continue
        require(not stat.S_ISLNK(info.st_mode)
                and not (getattr(info, "st_file_attributes", 0) & REPARSE),
                "PACKAGE_REPARSE_PATH")


def input_dir(text: str) -> Path:
    path = Path(text)
    inspect_path(path)
    require(path.is_dir(), "PACKAGE_DIRECTORY_REQUIRED")
    return path.resolve(strict=True)


def within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def pin(path: Path) -> dict:
    inspect_path(path)
    info = path.stat()
    require(stat.S_ISREG(info.st_mode), "PACKAGE_REGULAR_FILE_REQUIRED")
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
            size += len(block)
    inspect_path(path)
    after = path.stat()
    require((info.st_size, info.st_mtime_ns, info.st_ino, info.st_dev)
            == (after.st_size, after.st_mtime_ns, after.st_ino, after.st_dev)
            and size == info.st_size, "PACKAGE_INPUT_CHANGED_DURING_READ")
    return {"path": str(path), "bytes": size, "sha256": digest.hexdigest().upper()}


def assert_pin(path: Path, expected: dict) -> dict:
    current = pin(path)
    require(current == expected, "PACKAGE_SOURCE_IDENTITY_CHANGED")
    return current


def inventory(root: Path) -> dict[str, dict]:
    inspect_path(root)
    require(root.is_dir(), "PACKAGE_INVENTORY_DIRECTORY")
    result: dict[str, dict] = {}
    names: set[str] = set()

    def visit(directory: Path) -> None:
        for path in sorted(directory.iterdir(), key=lambda p: p.name):
            safe_name(path.name)
            inspect_path(path)
            relative = path.relative_to(root).as_posix()
            key = relative.casefold()
            require(key not in names, "PACKAGE_DUPLICATE_CASEFOLD_PATH")
            names.add(key)
            require(path.name.casefold() not in REJECT_DIRS, "PACKAGE_RUNTIME_USER_DIRECTORY")
            require(path.suffix.casefold() not in REJECT_SUFFIXES,
                    "PACKAGE_RUNTIME_TEMP_OR_USER_FILE")
            if path.is_dir():
                visit(path)
                continue
            require(path.name.casefold() not in {"save.txt", "receipt.json", "package-receipt.json"},
                    "PACKAGE_RUNTIME_TEMP_OR_USER_FILE")
            result[relative] = pin(path)

    visit(root)
    require(bool(result), "PACKAGE_EMPTY_INVENTORY")
    return result


def group_rows(rows: dict[str, dict]) -> dict[str, dict]:
    return {name: value for name, value in rows.items()
            if PurePosixPath(name).parts[0] in GROUPS}


def fixed_pin(path: Path, size: int, sha: str) -> dict:
    row = pin(path)
    require(row["bytes"] == size and row["sha256"] == sha, "PACKAGE_LITERAL_SOURCE_PIN")
    return row


def sealed_bytes(path: Path, expected: dict) -> bytes:
    data = path.read_bytes()
    require(len(data) == expected["bytes"] and hashlib.sha256(data).hexdigest().upper()
            == expected["sha256"], "PACKAGE_PARSED_INPUT_IDENTITY")
    assert_pin(path, expected)
    return data


def relative_member(name: str) -> None:
    require(isinstance(name, str) and bool(name) and "\\" not in name,
            "PACKAGE_COHORT_RELATIVE_PATH")
    require(not name.startswith("/") and all(part not in {"", ".", ".."}
            for part in name.split("/")), "PACKAGE_COHORT_RELATIVE_PATH")
    for part in name.split("/"):
        safe_name(part)


def generated_metadata(data: bytes, root_source: Path, generated_mesh: Path) -> bytes:
    text = data.decode("utf-8")
    for key, path in (("source", root_source), ("output", generated_mesh)):
        prefix = '"' + key + '": "'
        # 與固定producer json.dumps(meta, indent=2)完全相同的ASCII escaping。
        escaped = json.dumps(str(path))[1:-1]
        require(text.count(prefix) == 1 and text.count(prefix + escaped + '"') == 1,
                "PACKAGE_COHORT_GENERATED_PATH")
        text = text.replace(prefix + escaped + '"', prefix + "@" + key.upper() + "@\"")
    canonical = text.encode("utf-8")
    require(len(canonical) == 250893 and hashlib.sha256(canonical).hexdigest().upper()
            == "104607F9FA1E815A37583CCB8812A3982138B4F0F86230CDA91C494D50C649D6",
            "PACKAGE_COHORT_GENERATED_METADATA")
    return canonical


def reviewed_runtime(source: Path, runtime: Path, assets: dict[str, dict]) -> tuple[dict, list[dict]]:
    # 舊133門保持凍結；此封裝只接受已有固定來源清單的完整v3，不能以實際輸出自填預期。
    mapping_pin = fixed_pin(source / MAPPING_RELATIVE, 103994, MAPPING_SHA)
    leaf_pin = fixed_pin(source / LEAF_RELATIVE, 656, LEAF_SHA)
    mapping = json.loads(sealed_bytes(source / MAPPING_RELATIVE, mapping_pin))
    leaf = json.loads(sealed_bytes(source / LEAF_RELATIVE, leaf_pin))
    require(mapping["schema_version"] == 1 and leaf["schema_version"] == 1
            and leaf["profile"] == COHORT and leaf["parent"] == "authored-boulder-full-v2"
            and leaf["count"] == 416, "PACKAGE_COHORT_SCHEMA")
    parent = {row[0]: row for row in mapping["entries"] if row[2] & 16}
    legacy = {row[0]: row for row in mapping["entries"]
              if row[2] & 1 and PurePosixPath(row[0]).parts[0] in GROUPS}
    current = {name: row for name, row in parent.items()
               if PurePosixPath(name).parts[0] in GROUPS}
    require(len(parent) == 414 and len(legacy) == 133 and len(current) == 140,
            "PACKAGE_COHORT_FIXED_MEMBERSHIP")
    parent_mapping = "".join("\n".join(str(value) for value in
                             (name, row[1], row[3], row[4], row[5])) + "\n"
                             for name, row in parent.items()).encode("utf-8")
    require(leaf["parent_mapping_sha256"] == "D1CB0D00DAF1853E1A3F8393B6D15610223931C6EC313555E110FD495E6093F3"
            and hashlib.sha256(parent_mapping).hexdigest().upper() == leaf["parent_mapping_sha256"],
            "PACKAGE_COHORT_PARENT_MAPPING")
    require(set(legacy) <= set(current) and set(current) - set(legacy) == LEGACY_ADDITIONS,
            "PACKAGE_COHORT_LEGACY_LINEAGE")
    for name, row in legacy.items():
        require(current[name][1:2] + current[name][3:] == row[1:2] + row[3:],
                "PACKAGE_COHORT_LEGACY_ASSET_CHANGED")
    wanted = dict(parent)
    require(len(leaf["additions"]) == 2, "PACKAGE_COHORT_LEAF_ADDITIONS")
    for row in leaf["additions"]:
        require(row[0] not in wanted and row[3] == "copy", "PACKAGE_COHORT_LEAF_DUPLICATE")
        wanted[row[0]] = row
    require(len(wanted) == 416 and set(assets) == set(wanted), "PACKAGE_COHORT_RUNTIME_MEMBERSHIP")
    inputs = [mapping_pin, leaf_pin]
    require(len(mapping["producer_inputs"]) == 8, "PACKAGE_COHORT_PRODUCERS")
    for name, size, sha in mapping["producer_inputs"]:
        relative_member(name)
        inputs.append(fixed_pin(source / name, size, sha))
    require(len(runtime.parents) >= 2, "PACKAGE_COHORT_BUILD_ROOT")
    generated = runtime.parents[1] / "games/mistwatch/native/generated-root-stem"
    generated_mesh = generated / "island-tree-01-lod2-trunk-root-stem.aemesh"
    mesh_pin = fixed_pin(generated_mesh, 165000,
                         "96EA5581E6A5D2A0C3260DEEBE614A4FA9BCCA0A77772E5EAE42E0C88730BC7C")
    inputs.append(mesh_pin)
    generated_receipt = generated / "author-receipt.json"
    receipt_pin = pin(generated_receipt)
    inputs.append(receipt_pin)
    # raw receipt含本輪絕對路徑；僅精確替換兩個已核對欄位後，核對既有完整metadata固定seal。
    root_source = source / mapping["producer_inputs"][1][0]
    generated_metadata(sealed_bytes(generated_receipt, receipt_pin), root_source, generated_mesh)
    for name, row in wanted.items():
        relative_member(name)
        actual = assets[name]
        if row[3] == "root-receipt":
            expected = receipt_pin
        else:
            expected = {"bytes": row[4], "sha256": row[5]}
        require(actual["bytes"] == expected["bytes"] and actual["sha256"] == expected["sha256"],
                "PACKAGE_COHORT_RUNTIME_CONTENT:" + name)
        if row[3] == "copy":
            relative_member(row[1])
            inputs.append(fixed_pin(source / row[1], row[4], row[5]))
        elif row[3] == "root-mesh":
            require(actual["bytes"] == mesh_pin["bytes"] and actual["sha256"] == mesh_pin["sha256"],
                    "PACKAGE_COHORT_GENERATED_MESH")
        else:
            require(row[3] in {"root-receipt", "boar"}, "PACKAGE_COHORT_UNKNOWN_PRODUCER")
    return ({"runtime_profile": COHORT, "mapping_sha256": MAPPING_SHA, "leaf_mapping_sha256": LEAF_SHA,
             "legacy_eight_groups_count": 133, "legacy_eight_group_members": sorted(legacy),
             "current_eight_groups_count": 140, "legacy_additions": sorted(LEGACY_ADDITIONS),
             "baseline_runtime_count": 416, "independent_boar_regeneration": False}, inputs)


def copy_exact(source: Path, target: Path, expected: dict, output: Path) -> dict:
    require(within(target, output) and target != output, "PACKAGE_TARGET_ESCAPE")
    require(not os.path.lexists(target), "PACKAGE_TARGET_ALREADY_EXISTS")
    assert_pin(source, expected)
    target.parent.mkdir(parents=True, exist_ok=True)
    inspect_path(target.parent)
    digest = hashlib.sha256()
    size = 0
    # exclusive 實體複製；不以 hardlink 共用可變的 build 資料。
    with source.open("rb") as incoming, target.open("xb") as outgoing:
        while block := incoming.read(1024 * 1024):
            outgoing.write(block)
            digest.update(block)
            size += len(block)
        outgoing.flush()
        os.fsync(outgoing.fileno())
    require(size == expected["bytes"] and digest.hexdigest().upper() == expected["sha256"],
            "PACKAGE_COPY_INPUT_CHANGED")
    assert_pin(source, expected)
    copied = pin(target)
    require((copied["bytes"], copied["sha256"])
            == (expected["bytes"], expected["sha256"]), "PACKAGE_COPY_CONTENT_MISMATCH")
    return copied


def write_new(path: Path, data: bytes) -> None:
    inspect_path(path.parent)
    with path.open("xb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())


def package(args: argparse.Namespace) -> dict:
    source = input_dir(args.source_root)
    runtime = input_dir(args.runtime_dir)
    output = Path(args.output)
    inspect_path(output, exists=False)
    require(not os.path.lexists(output), "PACKAGE_OUTPUT_ALREADY_EXISTS")
    inspect_path(output.parent)
    require(output.parent.is_dir(), "PACKAGE_OUTPUT_PARENT_REQUIRED")
    output = output.parent.resolve(strict=True) / output.name
    safe_name(output.name)
    require(not within(output, source) and not within(source, output)
            and not within(output, runtime) and not within(runtime, output),
            "PACKAGE_OUTPUT_SOURCE_OVERLAP")

    tool = Path(__file__).absolute()
    inspect_path(tool)
    require(tool.resolve(strict=True) == source / TOOL_RELATIVE,
            "PACKAGE_TOOL_SOURCE_ROOT_MISMATCH")
    exe = runtime / EXE_NAME
    exe_pin = pin(exe)
    require(exe_pin["bytes"] > 0, "PACKAGE_EMPTY_EXECUTABLE")
    assets = inventory(runtime / "assets")
    baseline = group_rows(assets)
    cohort, cohort_inputs = reviewed_runtime(source, runtime, assets)
    require(len(baseline) == 140, "PACKAGE_CURRENT_EIGHT_GROUPS_140")
    require(all((runtime / "assets" / group).is_dir() for group in GROUPS),
            "PACKAGE_ORIGINAL_GROUP_MISSING")
    # v3完整清單與舊133子集合都已核對；新高度不能已存在，也不能以大小寫別名混入。
    asset_names = {name.casefold() for name in assets}
    height_source = source / "engine/assets/mistwatch-surface-height"
    additions = []
    for name, size, sha in HEIGHTS:
        relative = "mistwatch-materials/" + name
        require(relative.casefold() not in asset_names, "PACKAGE_HEIGHT_ALREADY_IN_BASELINE")
        additions.append((relative, height_source / name,
                          fixed_pin(height_source / name, size, sha)))
    for name, size, sha in WOOD:
        fixed_pin(runtime / "assets/mistwatch-materials/poly-haven/worn_planks/1k" / name,
                  size, sha)

    metadata = [(height_source / name, "surface-provenance/" + name)
                for name, _, _ in PROVENANCE]
    metadata_pins = {str(height_source / name): fixed_pin(height_source / name, size, sha)
                     for name, size, sha in PROVENANCE}
    require(all(row["bytes"] > 0 for row in metadata_pins.values()), "PACKAGE_EMPTY_PROVENANCE")
    manifest_path = height_source / "manifest.json"
    require(isinstance(json.loads(sealed_bytes(manifest_path, metadata_pins[str(manifest_path)])), dict),
            "PACKAGE_PROVENANCE_JSON_OBJECT")
    if args.build_receipt:
        build_receipt = Path(args.build_receipt)
        build_pin = pin(build_receipt)
        require(not within(build_receipt.resolve(strict=True), output), "PACKAGE_BUILD_RECEIPT_OVERLAP")
        with build_receipt.open("r", encoding="utf-8-sig") as stream:
            require(isinstance(json.load(stream), dict), "PACKAGE_BUILD_RECEIPT_JSON_OBJECT")
        metadata.append((build_receipt, "surface-provenance/build-receipt.json"))
        metadata_pins[str(build_receipt)] = build_pin

    shaders_path = runtime / "shaders"
    shaders = inventory(shaders_path) if os.path.lexists(shaders_path) else {}
    raw_inputs = [pin(tool), exe_pin, *assets.values(), *shaders.values(), *cohort_inputs,
                  *(row for _, _, row in additions), *metadata_pins.values()]
    unique_inputs = {}
    for row in raw_inputs:
        key = row["path"].casefold()
        require(key not in unique_inputs or unique_inputs[key] == row, "PACKAGE_INPUT_ALIAS_OR_DRIFT")
        unique_inputs[key] = row
    before = list(unique_inputs.values())
    source_paths = [row["path"].casefold() for row in before]
    require(len(source_paths) == len(set(source_paths)), "PACKAGE_DUPLICATE_INPUT")
    require(all(not within(Path(row["path"]), output) for row in before),
            "PACKAGE_INPUT_IN_OUTPUT")
    output.mkdir(exist_ok=False)
    copied: dict[str, dict] = {}
    try:
        copied[EXE_NAME] = copy_exact(exe, output / EXE_NAME, exe_pin, output)
        for name, row in assets.items():
            relative = "assets/" + name
            copied[relative] = copy_exact(Path(row["path"]), output / relative, row, output)
        for name, row in shaders.items():
            relative = "shaders/" + name
            copied[relative] = copy_exact(Path(row["path"]), output / relative, row, output)
        for name, path, row in additions:
            relative = "assets/" + name
            require(relative not in copied, "PACKAGE_DUPLICATE_COPY")
            copied[relative] = copy_exact(path, output / relative, row, output)
        for path, relative in metadata:
            copied[relative] = copy_exact(path, output / relative, metadata_pins[str(path)], output)
        write_new(output / "Launch-Forest-Surface-WIP.cmd", LAUNCHER)
        copied["Launch-Forest-Surface-WIP.cmd"] = pin(output / "Launch-Forest-Surface-WIP.cmd")
        require(inventory(runtime / "assets") == assets, "PACKAGE_RUNTIME_INVENTORY_DRIFT")
        require((inventory(shaders_path) if os.path.lexists(shaders_path) else {}) == shaders,
                "PACKAGE_SHADER_INVENTORY_DRIFT")
        after = [assert_pin(Path(row["path"]), row) for row in before]
        output_assets = inventory(output / "assets")
        updated = group_rows(output_assets)
        require(len(updated) == 142 and len(output_assets) == 418,
                "PACKAGE_CURRENT_SURFACE_GROUPS_142_RUNTIME_418")
        require(set(updated) - set(baseline) == {name for name, _, _ in additions},
                "PACKAGE_UNEXPECTED_GROUP_ADDITION")
        for name, row in assets.items():
            require((output_assets[name]["bytes"], output_assets[name]["sha256"])
                    == (row["bytes"], row["sha256"]), "PACKAGE_ORIGINAL_ASSET_CHANGED")
        # 執行檔、shader、授權、啟動器也做終態核對，不只核對 assets 子目錄。
        final_payload = inventory(output)
        require(final_payload == copied, "PACKAGE_OUTPUT_INVENTORY_DRIFT")
        receipt = {
            "schema": "mistwatch.forest-surface-package.2", "status": "PACKAGE_READY_WIP",
            "created_utc": datetime.now(timezone.utc).isoformat(),
            "source_root": str(source), "runtime_dir": str(runtime), "output": str(output),
            "source_inputs_before": before, "source_inputs_after": after,
            "files": copied, "runtime_asset_count_before": len(assets),
            "runtime_asset_count_after": len(output_assets),
            "cohort": cohort, "original_eight_groups_count": 140, "surface_eight_groups_count": 142,
            "shader_delivery": "runtime-shaders-copied" if shaders else "embedded-executable-Fh",
            "runtime_shader_count": len(shaders), "launcher_argument": "--forest-surface-candidate",
            "launcher_capture_environment": False, "hardlinks_created": False,
            "build_receipt_supplied": bool(args.build_receipt),
            # 封裝來源身分不等於全建置、人工畫面、完整玩法或發布資格。
            "fresh_full_build_verified": False, "full_gameplay_verified": False,
            "full_GBV_verified": False, "art_accepted": False, "published": False,
        }
        write_new(output / "package-receipt.json",
                  (json.dumps(receipt, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
        return receipt
    except Exception as error:
        # 保留第一次失敗的部分輸出；不刪除、不覆寫，也不製造 PACKAGE_READY。
        failure = {"schema": "mistwatch.forest-surface-package.2",
                   "status": "FIRST_DIVERGENCE_PRESERVED", "reason": str(error),
                   "source_inputs_before": before, "files_copied_so_far": copied,
                   "art_accepted": False, "published": False}
        write_new(output / "package-failure.json",
                  (json.dumps(failure, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--runtime-dir", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--build-receipt", help="Optional pinned JSON; not accepted as build qualification")
    args = parser.parse_args()
    try:
        receipt = package(args)
        print("FOREST_SURFACE_PACKAGE_READY_WIP assets="
              + str(receipt["runtime_asset_count_after"]) + " profile=" + COHORT
              + " legacy_groups=133 baseline_groups=140 surface_groups=142")
        return 0
    except (OSError, ValueError) as error:
        print("FOREST_SURFACE_PACKAGE_REJECTED: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
