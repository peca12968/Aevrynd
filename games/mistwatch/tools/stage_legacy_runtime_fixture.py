"""Materialize one exact reviewed legacy runtime fixture; identity-only, never world/native acceptance."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import sys

PROFILES = (
    ("legacy-full-v1", 387), ("habitat-full-v1", 402),
    ("authored-nettle-full-v1", 412), ("authored-mistwatch-only-v1", 154),
    ("authored-boulder-full-v2", 414), ("authored-boulder-mistwatch-only-v2", 156),
    ("authored-leaf-cluster-full-v3", 416),
)
TOOL = "games/mistwatch/tools/package_forest_surface.py"
TOOL_BYTES = 23213
TOOL_SHA = "6DB0F1D5B34E267F58E680230630239231677C76B51C6FCC0FA3A641E811FB8A"
SURFACE = "games/mistwatch/native/tests/qualification/runtime-forest-surface-full-v4.json"
CURRENT = "games/mistwatch/native/tests/qualification/runtime-current-forest-full-v5.json"
SURFACE_SHA = "467BE56D0F3C5F1408E014DB50728DD1BCC9D28AB0366CE6B74957C74505F291"
CURRENT_SHA = "8A2B65C6F27D9E7AC7CED21E69327D41514A4D72B22773B582D7D9CD6A57816B"


def load_helpers(source: Path) -> dict:
    # 在執行任何借用函式前先核原 helper 的實體路徑及同一份 raw bytes。
    tool = source / TOOL
    if not tool.is_absolute() or ".." in tool.parts:
        raise ValueError("LEGACY_FIXTURE_HELPER_ABSOLUTE_PATH")
    for path in reversed((tool, *tool.parents)):
        info = path.lstat()
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise ValueError("LEGACY_FIXTURE_HELPER_REPARSE")
    if not stat.S_ISREG(tool.stat().st_mode):
        raise ValueError("LEGACY_FIXTURE_HELPER_NOT_REGULAR")
    raw = tool.read_bytes()
    if len(raw) != TOOL_BYTES or hashlib.sha256(raw).hexdigest().upper() != TOOL_SHA:
        raise ValueError("LEGACY_FIXTURE_HELPER_RAW_PIN")
    namespace = {"__file__": str(tool), "__name__": "mistwatch_legacy_fixture_package_helpers"}
    exec(compile(raw, str(tool), "exec"), namespace)
    namespace["fixed_pin"](tool, TOOL_BYTES, TOOL_SHA)
    return namespace


def checked_entries(p: dict, rows: list, count: int) -> dict:
    wanted, folded = {}, set()
    for row in rows:
        p["require"](isinstance(row, list) and len(row) == 6, "LEGACY_FIXTURE_ENTRY_SHAPE")
        name, origin, mask, kind, size, sha = row
        p["relative_member"](name)
        p["require"](name.isascii() and isinstance(mask, int) and mask > 0,
                     "LEGACY_FIXTURE_ENTRY_DOMAIN")
        p["require"](kind in {"copy", "root-mesh", "root-receipt", "boar"}
                     and isinstance(size, int) and size > 0 and isinstance(sha, str)
                     and len(sha) == 64 and all(c in "0123456789ABCDEF" for c in sha),
                     "LEGACY_FIXTURE_ENTRY_IDENTITY")
        if kind == "copy":
            p["relative_member"](origin)
        else:
            p["require"](origin == "", "LEGACY_FIXTURE_GENERATED_ORIGIN")
        p["require"](name not in wanted and name.casefold() not in folded,
                     "LEGACY_FIXTURE_ENTRY_ALIAS")
        wanted[name] = row
        folded.add(name.casefold())
    p["require"](len(wanted) == count, "LEGACY_FIXTURE_ENTRY_COUNT")
    return wanted


def read_contracts(source: Path, p: dict) -> dict:
    base_pin = p["fixed_pin"](source / p["MAPPING_RELATIVE"], 103994, p["MAPPING_SHA"])
    leaf_pin = p["fixed_pin"](source / p["LEAF_RELATIVE"], 656, p["LEAF_SHA"])
    base = json.loads(p["sealed_bytes"](Path(base_pin["path"]), base_pin))
    leaf = json.loads(p["sealed_bytes"](Path(leaf_pin["path"]), leaf_pin))
    p["require"](base["schema_version"] == leaf["schema_version"] == 1
                 and base["profiles"] == [list(row) for row in PROFILES[:6]]
                 and leaf["profile"] == PROFILES[6][0] and leaf["count"] == 416
                 and leaf["parent"] == PROFILES[4][0], "LEGACY_FIXTURE_CONTRACT_SCHEMA")
    all_rows = checked_entries(p, base["entries"], 414)
    leaf_rows = checked_entries(p, leaf["additions"], 2)
    parent = checked_entries(p, [*all_rows.values(), *leaf_rows.values()], 416)
    profiles = {}
    for index, (name, count) in enumerate(PROFILES):
        rows = list(parent.values()) if index == 6 else [row for row in all_rows.values() if row[2] & (1 << index)]
        profiles[name] = checked_entries(p, rows, count)
        p["require"](all(parent.get(key) == row for key, row in profiles[name].items()),
                     "LEGACY_FIXTURE_NOT_POSITIVE_PARENT_SUBSET")
    return {"parent": parent, "profiles": profiles, "pins": [base_pin, leaf_pin]}


def read_surface(source: Path, p: dict) -> tuple[dict, list]:
    surface_pin = p["fixed_pin"](source / SURFACE, 816, SURFACE_SHA)
    current_pin = p["fixed_pin"](source / CURRENT, 298, CURRENT_SHA)
    surface = json.loads(p["sealed_bytes"](Path(surface_pin["path"]), surface_pin))
    current = json.loads(p["sealed_bytes"](Path(current_pin["path"]), current_pin))
    p["require"](surface["profile"] == "authored-forest-surface-full-v4"
                 and surface["parent"] == PROFILES[6][0] and surface["count"] == 418
                 and surface["parent_manifest_sha256"] == p["LEAF_SHA"]
                 and current["profile"] == "authored-current-forest-full-v5"
                 and current["parent"] == surface["profile"] and current["count"] == 418
                 and current["parent_manifest_sha256"] == SURFACE_SHA and current["additions"] == [],
                 "LEGACY_FIXTURE_SURFACE_SCHEMA")
    heights = checked_entries(p, surface["additions"], 2)
    p["require"](all(row[3] == "copy" for row in heights.values()), "LEGACY_FIXTURE_HEIGHT_COPY_ONLY")
    return heights, [surface_pin, current_pin]


def unique_inputs(p: dict, rows: list) -> list:
    result = {}
    for row in rows:
        key = str(Path(row["path"]).resolve(strict=True)).casefold()
        previous = result.get(key)
        p["require"](previous is None or (previous["bytes"], previous["sha256"])
                     == (row["bytes"], row["sha256"]), "LEGACY_FIXTURE_INPUT_ALIAS_DRIFT")
        if previous is None:
            result[key] = row
    return list(result.values())


def stage(source_root: Path, runtime_dir: Path, out: Path, profile: str) -> dict:
    if profile not in dict(PROFILES):
        raise ValueError("LEGACY_FIXTURE_PROFILE_NOT_ALLOWED")
    p = load_helpers(Path(source_root))
    source = p["input_dir"](source_root)
    runtime = p["input_dir"](runtime_dir)
    p["inspect_path"](out, exists=False)
    p["require"](not os.path.lexists(out), "LEGACY_FIXTURE_OUTPUT_EXISTS")
    parent_dir = p["input_dir"](out.parent)
    p["safe_name"](out.name)
    out = parent_dir / out.name
    p["require"](not p["within"](out, source) and not p["within"](source, out)
                 and not p["within"](out, runtime) and not p["within"](runtime, out),
                 "LEGACY_FIXTURE_INPUT_OUTPUT_OVERLAP")
    contract = read_contracts(source, p)
    selected = checked_entries(p, list(contract["profiles"][profile].values()), dict(PROFILES)[profile])
    p["require"](all(contract["parent"].get(name) == row for name, row in selected.items()),
                 "LEGACY_FIXTURE_SELECTED_MAPPING_CHANGED")
    pool = p["inventory"](runtime / "assets")
    surface_inputs = []
    source_profile = PROFILES[6][0]
    if set(pool) != set(contract["parent"]):
        heights, surface_inputs = read_surface(source, p)
        full = checked_entries(p, [*contract["parent"].values(), *heights.values()], 418)
        p["require"](set(pool) == set(full), "LEGACY_FIXTURE_POOL_MEMBERSHIP")
        for name, row in heights.items():
            surface_inputs.append(p["fixed_pin"](source / row[1], row[4], row[5]))
            p["fixed_pin"](runtime / "assets" / name, row[4], row[5])
        source_profile = "authored-current-forest-full-v5"
    # 必須把原 runtime 傳給既有驗證器；新 fixture 的相對層級不參與 root-stem 推導。
    cohort, reviewed_inputs = p["reviewed_runtime"](
        source, runtime, {name: pool[name] for name in contract["parent"]})
    p["require"](cohort["baseline_runtime_count"] == 416, "LEGACY_FIXTURE_REVIEWED_PARENT")
    before = unique_inputs(p, [p["pin"](Path(__file__).absolute()),
        p["fixed_pin"](source / TOOL, TOOL_BYTES, TOOL_SHA),
        *contract["pins"], *surface_inputs, *reviewed_inputs, *pool.values()])
    p["require"](all(not p["within"](Path(row["path"]), out) for row in before),
                 "LEGACY_FIXTURE_INPUT_IN_OUTPUT")
    out.mkdir(exist_ok=False)
    copied = {}
    try:
        (out / "assets").mkdir(exist_ok=False)
        for name in selected:
            copied[name] = p["copy_exact"](Path(pool[name]["path"]), out / "assets" / name, pool[name], out)
        p["require"](p["inventory"](runtime / "assets") == pool, "LEGACY_FIXTURE_POOL_CHANGED")
        after = [p["assert_pin"](Path(row["path"]), row) for row in before]
        actual = p["inventory"](out / "assets")
        p["require"](actual == copied and set(actual) == set(selected), "LEGACY_FIXTURE_OUTPUT_CHANGED")
        p["require"](all((actual[name]["bytes"], actual[name]["sha256"])
                     == (pool[name]["bytes"], pool[name]["sha256"]) for name in selected),
                     "LEGACY_FIXTURE_OUTPUT_RAW_DRIFT")
        p["require"]({path.name for path in out.iterdir()} == {"assets"}, "LEGACY_FIXTURE_OUTPUT_EXTRA")
        receipt = {"schema": "mistwatch.legacy-runtime-fixture.1",
            "status": "LEGACY_RUNTIME_FIXTURE_READY_IDENTITY_ONLY",
            "source_root": str(source), "original_runtime_dir": str(runtime), "output": str(out),
            "source_pool_profile": source_profile, "source_pool_count": len(pool),
            "runtime_profile": profile, "runtime_asset_count": len(actual),
            "source_inputs_before": before, "source_inputs_after": after, "files": actual,
            "hardlinks_created": False, "runtime_identity_only": True,
            "world_qualified": False, "native_qualified": False, "original_pool_unchanged": True,
            "independent_boar_regeneration": False,
            "root_metadata": "Exact raw from original runtime/generated output; original helper canonical seal; never rebase to fixture."}
        p["write_new"](out / "stage-receipt.json", (json.dumps(receipt, indent=2) + "\n").encode("utf-8"))
        return receipt
    except Exception as error:
        failure = {"schema": "mistwatch.legacy-runtime-fixture.1", "status": "FIRST_DIVERGENCE_PRESERVED",
            "reason": str(error), "runtime_profile": profile, "files_copied_so_far": copied,
            "runtime_identity_only": True, "world_qualified": False, "native_qualified": False}
        try:
            p["write_new"](out / "stage-failure.json", (json.dumps(failure, indent=2) + "\n").encode("utf-8"))
        except (OSError, ValueError) as record_error:
            print("LEGACY_FIXTURE_FAILURE_RECORD_UNAVAILABLE: " + str(record_error), file=sys.stderr)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--runtime-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--profile", choices=[name for name, _ in PROFILES], required=True)
    args = parser.parse_args()
    try:
        receipt = stage(args.source_root, args.runtime_dir, args.out, args.profile)
        print(json.dumps({key: receipt[key] for key in ("status", "runtime_profile", "runtime_asset_count",
            "source_pool_profile", "source_pool_count", "runtime_identity_only", "world_qualified", "native_qualified")},
            separators=(",", ":")))
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("LEGACY_RUNTIME_FIXTURE_REJECTED: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
