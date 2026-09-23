#!/usr/bin/env python3
import importlib.util
import json
import pathlib
import struct
import tempfile
import unittest


TOOL = pathlib.Path(__file__).with_name("verify-poly-haven-fir-cook.py")
SPEC = importlib.util.spec_from_file_location("fir_verify", TOOL)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def write_glb(path, twig_mode="MASK"):
    document = {
        "asset": {"version": "2.0"},
        "materials": [
            {"name": "fir_tree_01_bark"},
            {"name": "fir_tree_01_twig", "alphaMode": twig_mode},
            {"name": "fir_tree_01_dead_branches"},
            {"name": "fir_tree_01_trunk_c"},
        ],
    }
    payload = json.dumps(document, separators=(",", ":")).encode()
    payload += b" " * ((4 - len(payload) % 4) % 4)
    binary = b"\0\0\0\0"
    total = 12 + 8 + len(payload) + 8 + len(binary)
    path.write_bytes(struct.pack("<III", 0x46546C67, 2, total) + struct.pack("<II", len(payload), 0x4E4F534A) + payload + struct.pack("<II", len(binary), 0x004E4942) + binary)


class FirCookReceiptTests(unittest.TestCase):
    def make_receipt(self, root, twig_mode="MASK"):
        levels = []
        for level, triangles in enumerate((34656, 22079, 6600)):
            path = root / f"fir-tree-01-c-lod{level}.glb"
            write_glb(path, twig_mode)
            modes = {name: "OPAQUE" for name in MODULE.EXPECTED_MATERIALS}
            modes["fir_tree_01_twig"] = twig_mode
            levels.append({"level": level, "triangles": triangles, "file": path.name, "bytes": path.stat().st_size, "sha256": MODULE.digest(path), "materials": sorted(MODULE.EXPECTED_MATERIALS), "materialAlphaModes": modes})
        receipt = {"schemaVersion": 1, "pipeline": "cook-poly-haven-fir", "license": "CC0-1.0", "officialPage": "https://polyhaven.com/a/fir_tree_01", "opaqueFoliageRejected": True, "offlineTextureInputs": [{"file": f"texture-{index}.png", "bytes": index + 1, "sha256": "A" * 64} for index in range(10)], "levels": levels}
        path = root / "receipt.json"
        path.write_text(json.dumps(receipt), encoding="utf-8")
        return path

    def test_accepts_receipted_masked_foliage(self):
        with tempfile.TemporaryDirectory() as directory:
            receipt = self.make_receipt(pathlib.Path(directory))
            self.assertEqual(MODULE.verify(receipt), [34656, 22079, 6600])

    def test_rejects_opaque_foliage(self):
        with tempfile.TemporaryDirectory() as directory:
            receipt = self.make_receipt(pathlib.Path(directory), "OPAQUE")
            with self.assertRaisesRegex(ValueError, "Foliage alpha"):
                MODULE.verify(receipt)

    def test_rejects_tampered_glb(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            receipt = self.make_receipt(root)
            with (root / "fir-tree-01-c-lod1.glb").open("ab") as stream:
                stream.write(b"tamper")
            with self.assertRaisesRegex(ValueError, "receipt mismatch"):
                MODULE.verify(receipt)


if __name__ == "__main__":
    unittest.main()
