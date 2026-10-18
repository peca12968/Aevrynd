"""Tiny synthetic IO/mocked-contract tests only; no real runtime, native validation, or large copy."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import stat
import sys
import tempfile
import types
import unittest
from unittest import mock

S = None
HELPER_RAW = b""


def put(path: Path, raw: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(raw)


class MiniPool:
    def __init__(self, root: Path, surface: bool = False):
        self.root = root
        self.source = root / "source"
        self.runtime = root / "b/bin/Release"
        self.source.mkdir()
        self.runtime.mkdir(parents=True)
        put(self.source / S.TOOL, HELPER_RAW)
        self.p = S.load_helpers(self.source)
        self.parent, self.data, self.rows = {}, {}, []
        for index in range(414):
            mask = 16
            for bit, (_, count) in enumerate(S.PROFILES[:6]):
                if index < count:
                    mask |= 1 << bit
            name, kind = f"data/member-{index:03}.bin", "copy"
            if index == 0:
                name, kind = "mistwatch-canopy-preview/root-stem-derivation.json", "root-receipt"
            elif index == 1:
                name, kind = "mistwatch-canopy-preview/meshes/root.aemesh", "root-mesh"
            elif index == 2:
                name, kind = "mistwatch-wildlife/boar.aeskin", "boar"
            row = self.entry(name, f"mock-bytes-{index}\n".encode(), mask, kind)
            self.rows.append(row)
            self.parent[name] = row
        for index in range(2):
            row = self.entry(f"mistwatch-leaf-clusters/leaf-{index}.bin", f"leaf-{index}\n".encode(), 64)
            self.parent[row[0]] = row
        self.heights = {}
        for label in ("moss", "leaf"):
            row = self.entry(f"mistwatch-materials/{label}-height/height.png", f"height-{label}\n".encode(), 128)
            self.heights[row[0]] = row
        for name in [*self.parent, *(self.heights if surface else {})]:
            put(self.runtime / "assets" / name, self.data[name])
        self.contract_pins = []
        for name in ("contract-base.json", "contract-leaf.json", "contract-surface.json", "contract-current.json"):
            path = self.source / name
            put(path, json.dumps({"mock_contract": name}).encode())
            self.contract_pins.append(self.p["pin"](path))
        self.producers = []
        for index in range(8):
            path = self.source / f"producers/input-{index}.txt"
            put(path, f"mock-producer-{index}\n".encode())
            self.producers.append(self.p["pin"](path))
        generated = self.runtime.parents[1] / "games/mistwatch/native/generated-root-stem"
        self.generated = []
        for name, data_name in (("author-receipt.json", self.rows[0][0]),
                                ("island-tree-01-lod2-trunk-root-stem.aemesh", self.rows[1][0])):
            path = generated / name
            put(path, self.data[data_name])
            self.generated.append(self.p["pin"](path))
        self.profiles = {}
        for index, (name, _) in enumerate(S.PROFILES):
            self.profiles[name] = dict(self.parent) if index == 6 else {
                row[0]: row for row in self.rows if row[2] & (1 << index)}
        self.p["reviewed_runtime"] = self.reviewed_mock

    def entry(self, name: str, raw: bytes, mask: int, kind: str = "copy") -> list:
        origin = "inputs/" + name if kind == "copy" else ""
        row = [name, origin, mask, kind, len(raw), hashlib.sha256(raw).hexdigest().upper()]
        self.data[name] = raw
        if origin:
            put(self.source / origin, raw)
        return row

    def reviewed_mock(self, source: Path, runtime: Path, assets: dict) -> tuple:
        # 僅替代大型已封契約／producer邊界；真安全路徑、inventory及copy_exact不替換。
        self.p["require"](source == self.source and runtime == self.runtime,
                          "MOCK_ORIGINAL_RUNTIME_NOT_PRESERVED")
        self.p["require"](set(assets) == set(self.parent), "MOCK_REVIEWED_MEMBERSHIP")
        inputs = [*self.contract_pins[:2], *self.producers, *self.generated]
        for row in self.producers + self.generated:
            self.p["assert_pin"](Path(row["path"]), row)
        for name, row in self.parent.items():
            self.p["require"]((assets[name]["bytes"], assets[name]["sha256"]) == (row[4], row[5]),
                              "MOCK_REVIEWED_RUNTIME_CONTENT:" + name)
            if row[3] == "copy":
                inputs.append(self.p["fixed_pin"](source / row[1], row[4], row[5]))
        return {"baseline_runtime_count": 416, "runtime_profile": S.PROFILES[6][0]}, inputs

    def run(self, output: Path, profile: str = "authored-mistwatch-only-v1") -> dict:
        contract = {"parent": self.parent, "profiles": self.profiles, "pins": self.contract_pins[:2]}
        with mock.patch.object(S, "load_helpers", return_value=self.p), \
             mock.patch.object(S, "read_contracts", return_value=contract), \
             mock.patch.object(S, "read_surface", return_value=(self.heights, self.contract_pins[2:])):
            return S.stage(self.source, self.runtime, output, profile)


class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="mistwatch-legacy-fixture-mock-")
        self.addCleanup(self.directory.cleanup)
        self.base = Path(self.directory.name).resolve()
        self.mini = MiniPool(self.base)

    def rejected(self, reason: str, output: Path | None = None):
        target = output or self.base / "out"
        with self.assertRaises(ValueError) as caught:
            self.mini.run(target)
        self.assertEqual(str(caught.exception), reason)
        return target

    def test_all_seven_positive_mappings_from_416(self):
        before = self.mini.p["inventory"](self.mini.runtime / "assets")
        for index, (profile, count) in enumerate(S.PROFILES):
            output = self.base / f"out-{index}"
            receipt = self.mini.run(output, profile)
            self.assertEqual(receipt["status"], "LEGACY_RUNTIME_FIXTURE_READY_IDENTITY_ONLY")
            self.assertEqual(receipt["runtime_asset_count"], count)
            actual = self.mini.p["inventory"](output / "assets")
            self.assertEqual(set(actual), set(self.mini.profiles[profile]))
            for name, row in actual.items():
                self.assertEqual((row["bytes"], row["sha256"]), (before[name]["bytes"], before[name]["sha256"]))
                self.assertFalse(Path(row["path"]).samefile(before[name]["path"]))
            self.assertTrue((output / "stage-receipt.json").is_file())
            self.assertFalse(receipt["world_qualified"] or receipt["native_qualified"])
        self.assertEqual(before, self.mini.p["inventory"](self.mini.runtime / "assets"))

    def test_418_pool_selects_positive_416_and_preserves_heights(self):
        for name in self.mini.heights:
            put(self.mini.runtime / "assets" / name, self.mini.data[name])
        before = self.mini.p["inventory"](self.mini.runtime / "assets")
        receipt = self.mini.run(self.base / "out", S.PROFILES[6][0])
        self.assertEqual((receipt["source_pool_count"], receipt["runtime_asset_count"]), (418, 416))
        self.assertEqual(receipt["source_pool_profile"], "authored-current-forest-full-v5")
        self.assertEqual(set(receipt["files"]), set(self.mini.parent))
        self.assertEqual(before, self.mini.p["inventory"](self.mini.runtime / "assets"))

    def test_extra_pool_member(self):
        put(self.mini.runtime / "assets/unreviewed.bin", b"extra")
        self.assertFalse(self.rejected("LEGACY_FIXTURE_POOL_MEMBERSHIP").exists())

    def test_missing_pool_member(self):
        (self.mini.runtime / "assets" / self.mini.rows[3][0]).unlink()
        self.assertFalse(self.rejected("LEGACY_FIXTURE_POOL_MEMBERSHIP").exists())

    def test_same_count_wrong_path(self):
        original = self.mini.runtime / "assets" / self.mini.rows[3][0]
        original.rename(original.with_name("replacement.bin"))
        self.rejected("LEGACY_FIXTURE_POOL_MEMBERSHIP")

    def test_wrong_case_path(self):
        original = self.mini.runtime / "assets" / self.mini.rows[3][0]
        original.rename(original.with_name(original.name.upper()))
        self.rejected("LEGACY_FIXTURE_POOL_MEMBERSHIP")

    def test_corrupt_pool_bytes(self):
        name = self.mini.rows[3][0]
        put(self.mini.runtime / "assets" / name, b"changed")
        self.rejected("MOCK_REVIEWED_RUNTIME_CONTENT:" + name)

    def test_corrupt_source_copy(self):
        put(self.mini.source / self.mini.rows[3][1], b"changed")
        self.rejected("PACKAGE_LITERAL_SOURCE_PIN")

    def test_corrupt_generated_metadata(self):
        put(Path(self.mini.generated[0]["path"]), b"changed")
        self.rejected("PACKAGE_SOURCE_IDENTITY_CHANGED")

    def test_corrupt_418_height(self):
        for name in self.mini.heights:
            put(self.mini.runtime / "assets" / name, self.mini.data[name])
        put(self.mini.runtime / "assets" / next(iter(self.mini.heights)), b"changed")
        self.rejected("PACKAGE_LITERAL_SOURCE_PIN")

    def test_unsafe_selected_path(self):
        profile = "authored-mistwatch-only-v1"
        changed = copy.deepcopy(self.mini.profiles[profile])
        first = next(iter(changed))
        row = changed.pop(first)
        row[0] = "../escape.bin"
        changed[row[0]] = row
        self.mini.profiles[profile] = changed
        self.rejected("PACKAGE_COHORT_RELATIVE_PATH")

    def test_mock_reparse_attribute_rejected_without_os_changes(self):
        target = self.mini.runtime / "assets" / self.mini.rows[3][0]
        original_lstat = Path.lstat
        def observed_lstat(path, *args, **kwargs):
            if path == target:
                return types.SimpleNamespace(st_mode=stat.S_IFREG | 0o600, st_file_attributes=0x400)
            return original_lstat(path, *args, **kwargs)
        with mock.patch.object(Path, "lstat", autospec=True, side_effect=observed_lstat):
            self.rejected("PACKAGE_REPARSE_PATH")

    def test_fresh_collision_preserves_existing(self):
        output = self.base / "out"
        put(output / "owned.txt", b"preserve")
        self.rejected("LEGACY_FIXTURE_OUTPUT_EXISTS", output)
        self.assertEqual((output / "owned.txt").read_bytes(), b"preserve")
        self.assertEqual({path.name for path in output.iterdir()}, {"owned.txt"})

    def test_overlap_rejected(self):
        self.rejected("LEGACY_FIXTURE_INPUT_OUTPUT_OVERLAP", self.mini.source / "fixture")
        self.rejected("LEGACY_FIXTURE_INPUT_OUTPUT_OVERLAP", self.mini.runtime / "fixture")

    def test_partial_failure_preserved_and_not_reused(self):
        original_copy = self.mini.p["copy_exact"]
        attempts = 0
        def failing_copy(*args):
            nonlocal attempts
            attempts += 1
            if attempts == 2:
                raise OSError("MOCK_SECOND_COPY_FAILURE")
            return original_copy(*args)
        self.mini.p["copy_exact"] = failing_copy
        output = self.base / "out"
        with self.assertRaisesRegex(OSError, "^MOCK_SECOND_COPY_FAILURE$"):
            self.mini.run(output)
        failure = json.loads((output / "stage-failure.json").read_bytes())
        self.assertEqual(len(failure["files_copied_so_far"]), 1)
        self.assertEqual(len(self.mini.p["inventory"](output / "assets")), 1)
        self.assertFalse((output / "stage-receipt.json").exists())
        before = (output / "stage-failure.json").read_bytes()
        self.rejected("LEGACY_FIXTURE_OUTPUT_EXISTS", output)
        self.assertEqual((output / "stage-failure.json").read_bytes(), before)

    def test_source_pool_changes_during_copy(self):
        original_copy = self.mini.p["copy_exact"]
        changed = False
        def changing_copy(origin, *args):
            nonlocal changed
            result = original_copy(origin, *args)
            if not changed:
                origin.write_bytes(b"mock-mutation-after-copy")
                changed = True
            return result
        self.mini.p["copy_exact"] = changing_copy
        output = self.rejected("LEGACY_FIXTURE_POOL_CHANGED")
        self.assertTrue(changed and (output / "stage-failure.json").is_file())
        self.assertFalse((output / "stage-receipt.json").exists())

    def test_unknown_profile_is_not_inferred(self):
        for profile in ("authored-current-forest-full-v5", "AUTHORED-LEAF-CLUSTER-FULL-V3", "auto"):
            with self.assertRaisesRegex(ValueError, "^LEGACY_FIXTURE_PROFILE_NOT_ALLOWED$"):
                self.mini.run(self.base / "out", profile)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--helper-source-root", type=Path, required=True,
                        help="Read only the sealed package helper source; never production runtime/build outputs.")
    parser.add_argument("--stage-script", type=Path, default=Path(__file__).with_name("stage_legacy_runtime_fixture.py"))
    args = parser.parse_args()
    global S, HELPER_RAW
    path = args.stage_script
    S = types.ModuleType("legacy_fixture_under_test")
    S.__file__ = str(path)
    exec(compile(path.read_bytes(), str(path), "exec"), S.__dict__)
    HELPER_RAW = (args.helper_source_root / S.TOOL).read_bytes()
    if len(HELPER_RAW) != S.TOOL_BYTES or hashlib.sha256(HELPER_RAW).hexdigest().upper() != S.TOOL_SHA:
        raise ValueError("MOCK_TEST_HELPER_RAW_PIN")
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(FixtureTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.wasSuccessful():
        print(f"LEGACY_RUNTIME_FIXTURE_MOCK_TESTS_PASS tests={result.testsRun} mock_contracts=1 native=0 live_artifacts=0 large_copy=0")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
