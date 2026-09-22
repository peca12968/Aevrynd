#!/usr/bin/env python3
"""Hostile self-tests for build-foliage-card-lod.py."""

from __future__ import annotations

import importlib.util
import json
import math
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest


sys.dont_write_bytecode = True
TOOL = pathlib.Path(__file__).with_name("build-foliage-card-lod.py")
SPEC = importlib.util.spec_from_file_location("foliage_card_lod", TOOL)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


def make_mesh() -> bytes:
    vertices = []
    indices = []
    boxes = ((0.0, 0.0, 0.0), (4.0, 2.0, 3.0), (8.0, 6.0, 7.0))
    for x, y, z in boxes:
        base = len(vertices)
        vertices.extend(((x, y, z), (x + 2.0, y, z), (x + 2.0, y + 2.0, z + 1.0), (x, y + 2.0, z + 1.0)))
        indices.extend((base, base + 1, base + 2, base, base + 2, base + 3))
    data = bytearray(struct.pack("<4sIII", b"AEM3", 1, len(vertices), len(indices)))
    for ordinal, (x, y, z) in enumerate(vertices):
        u = float(ordinal & 1) * 0.75 + 0.1
        v = float((ordinal >> 1) & 1) * 0.5 + 0.2
        data.extend(struct.pack("<fffIIff", x, y, z, 0, 0xFFFFFFFF, u, v))
    data.extend(struct.pack(f"<{len(indices)}I", *indices))
    return bytes(data)


class FoliageCardLodTests(unittest.TestCase):
    def run_tool(self, data: bytes, extra: tuple[str, ...] = ()):
        temporary = tempfile.TemporaryDirectory()
        root = pathlib.Path(temporary.name)
        source, output, receipt = root / "source.aemesh", root / "cards.aemesh", root / "receipt.json"
        source.write_bytes(data)
        command = [sys.executable, str(TOOL), "--source", str(source), "--output", str(output),
                   "--receipt", str(receipt), "--grid", "3", "3", "3", "--lod1-triangles", "20", *extra]
        process = subprocess.run(command, capture_output=True, text=True, check=False)
        return temporary, process, output, receipt

    def test_receipt_occupancy_bounds_uv_and_nonzero_cards(self):
        temporary, process, output, receipt_path = self.run_tool(make_mesh())
        with temporary:
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertIn("FOLIAGE_CARD_LOD_RECEIPT=", process.stdout)
            receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
            vertices, indices = MODULE.parse_aemesh(output.read_bytes())
            self.assertEqual(receipt["occupiedVoxelCount"], 3)
            self.assertEqual(receipt["cards"], 9)
            self.assertEqual(receipt["triangles"], 18)
            self.assertLess(receipt["triangles"], receipt["lod1Triangles"])
            self.assertEqual(receipt["bounds"], receipt["sourceBounds"])
            for actual, expected in zip(receipt["sourceUvRange"]["min"], (0.1, 0.2)):
                self.assertAlmostEqual(actual, expected)
            for actual, expected in zip(receipt["sourceUvRange"]["max"], (0.85, 0.7)):
                self.assertAlmostEqual(actual, expected)
            self.assertEqual(len(receipt["sourceSha256"]), 64)
            self.assertEqual(len(receipt["outputSha256"]), 64)
            self.assertEqual(len(receipt["policyHash"]), 64)
            self.assertTrue(receipt["diagnosticOnly"])
            self.assertTrue(all(item["components"] == 1 and len(item["planes"]) == 3 for item in receipt["occupiedVoxels"]))
            variants = [variant for item in receipt["occupiedVoxels"] for variant in item["uvVariants"]]
            self.assertTrue(all(0 <= variant <= 3 for variant in variants))
            self.assertGreaterEqual(len(set(variants)), 2)
            for triangle in range(len(indices) // 3):
                a, b, c = (vertices[indices[triangle * 3 + corner]] for corner in range(3))
                self.assertGreater(MODULE.triangle_area_squared(a, b, c), 0.0)

    def test_output_and_policy_are_deterministic(self):
        first_temp, first, first_output, first_receipt = self.run_tool(make_mesh(), ("--seed", "fixed", "--card-scale", "1.5"))
        second_temp, second, second_output, second_receipt = self.run_tool(make_mesh(), ("--seed", "fixed", "--card-scale", "1.5"))
        with first_temp, second_temp:
            self.assertEqual(first.returncode, 0, first.stderr)
            self.assertEqual(second.returncode, 0, second.stderr)
            self.assertEqual(first_output.read_bytes(), second_output.read_bytes())
            first_json = json.loads(first_receipt.read_text(encoding="utf-8"))
            second_json = json.loads(second_receipt.read_text(encoding="utf-8"))
            self.assertEqual(first_json["outputSha256"], second_json["outputSha256"])
            self.assertEqual(first_json["policyHash"], second_json["policyHash"])

    def test_production_mode_is_explicit_and_preserves_geometry(self):
        diagnostic_temp, diagnostic, diagnostic_output, diagnostic_receipt = self.run_tool(make_mesh())
        production_temp, production, production_output, production_receipt = self.run_tool(make_mesh(), ("--production",))
        with diagnostic_temp, production_temp:
            self.assertEqual(diagnostic.returncode, 0, diagnostic.stderr)
            self.assertEqual(production.returncode, 0, production.stderr)
            self.assertTrue(json.loads(diagnostic_receipt.read_text(encoding="utf-8"))["diagnosticOnly"])
            self.assertFalse(json.loads(production_receipt.read_text(encoding="utf-8"))["diagnosticOnly"])
            self.assertEqual(diagnostic_output.read_bytes(), production_output.read_bytes())

    def test_rejects_malformed_nan_and_out_of_range(self):
        nan_mesh = bytearray(make_mesh())
        struct.pack_into("<f", nan_mesh, 16, math.nan)
        cases = {
            "truncated": b"AEM3",
            "wrong-version": struct.pack("<4sIII", b"AEM3", 2, 0, 0),
            "nan": bytes(nan_mesh),
            "out-of-range": make_mesh()[:-4] + struct.pack("<I", 9999),
        }
        for label, data in cases.items():
            with self.subTest(label=label):
                temporary, process, output, receipt = self.run_tool(data)
                with temporary:
                    self.assertNotEqual(process.returncode, 0)
                    self.assertIn("FOLIAGE_CARD_LOD_ERROR=", process.stderr)
                    self.assertFalse(output.exists())
                    self.assertFalse(receipt.exists())

    def test_rejects_zero_area_source_bounds_and_triangle_budget(self):
        flat = bytearray(make_mesh())
        vertex_count = struct.unpack_from("<I", flat, 8)[0]
        for ordinal in range(vertex_count):
            offset = 16 + ordinal * 28
            struct.pack_into("<fff", flat, offset, 1.0, 1.0, 1.0)
        for label, data, extra in (
            ("zero-area", bytes(flat), ()),
            ("triangle-budget", make_mesh(), ("--lod1-triangles", "12")),
            ("bad-scale", make_mesh(), ("--card-scale", "nan")),
            ("bad-grid", make_mesh(), ("--grid", "0", "2", "2")),
        ):
            with self.subTest(label=label):
                temporary, process, output, receipt = self.run_tool(data, extra)
                with temporary:
                    self.assertNotEqual(process.returncode, 0)
                    self.assertFalse(output.exists())
                    self.assertFalse(receipt.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
