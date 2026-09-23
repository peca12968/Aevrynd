#!/usr/bin/env python3
"""Fail-closed verifier for the offline fir-tree cook receipt and GLB outputs."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import struct


EXPECTED_MATERIALS = {
    "fir_tree_01_bark",
    "fir_tree_01_twig",
    "fir_tree_01_dead_branches",
    "fir_tree_01_trunk_c",
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def glb_document(path):
    data = path.read_bytes()
    if len(data) < 28:
        raise ValueError(f"GLB is truncated: {path}")
    magic, version, declared = struct.unpack_from("<III", data)
    length, kind = struct.unpack_from("<II", data, 12)
    if magic != 0x46546C67 or version != 2 or declared != len(data) or kind != 0x4E4F534A or 20 + length > len(data):
        raise ValueError(f"GLB container is invalid: {path}")
    return json.loads(data[20 : 20 + length].rstrip(b" \t\r\n\0"))


def verify(receipt_path):
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    if receipt.get("schemaVersion") != 1 or receipt.get("pipeline") != "cook-poly-haven-fir":
        raise ValueError("Receipt schema or pipeline is unsupported")
    if receipt.get("license") != "CC0-1.0" or receipt.get("officialPage") != "https://polyhaven.com/a/fir_tree_01":
        raise ValueError("Source license provenance is missing")
    if receipt.get("opaqueFoliageRejected") is not True:
        raise ValueError("Opaque-foliage rejection was not asserted")
    texture_inputs = receipt.get("offlineTextureInputs")
    if (not isinstance(texture_inputs, list) or len(texture_inputs) != 10 or
            len({item.get("file") for item in texture_inputs}) != 10 or
            any(not isinstance(item.get("bytes"), int) or item["bytes"] <= 0 or
                not re.fullmatch(r"[0-9A-F]{64}", item.get("sha256", "")) for item in texture_inputs)):
        raise ValueError("Offline texture provenance is incomplete")
    levels = receipt.get("levels")
    if not isinstance(levels, list) or len(levels) != 3 or [item.get("level") for item in levels] != [0, 1, 2]:
        raise ValueError("Exactly three ordered LOD levels are required")
    triangles = [item.get("triangles") for item in levels]
    if not all(isinstance(value, int) and value > 0 for value in triangles) or not triangles[0] > triangles[1] > triangles[2] or triangles[0] > 50000 or triangles[2] > 9000:
        raise ValueError(f"Realtime triangle budgets failed: {triangles}")
    for level in levels:
        path = (receipt_path.parent / level["file"]).resolve(strict=True)
        if path.parent != receipt_path.parent.resolve() or path.stat().st_size != level["bytes"] or digest(path) != level["sha256"]:
            raise ValueError(f"GLB receipt mismatch: {path}")
        document = glb_document(path)
        modes = {item.get("name"): item.get("alphaMode", "OPAQUE") for item in document.get("materials", [])}
        if set(modes) != EXPECTED_MATERIALS or modes["fir_tree_01_twig"] == "OPAQUE":
            raise ValueError(f"Foliage alpha or material set failed: {modes}")
        if any(modes[name] != "OPAQUE" for name in EXPECTED_MATERIALS - {"fir_tree_01_twig"}):
            raise ValueError(f"Solid materials became transparent: {modes}")
        if set(level.get("materials", [])) != EXPECTED_MATERIALS or level.get("materialAlphaModes") != modes:
            raise ValueError("Receipt material declaration differs from GLB")
    return triangles


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", required=True, type=pathlib.Path)
    arguments = parser.parse_args()
    triangles = verify(arguments.receipt.resolve(strict=True))
    print("FIR_COOK_VERIFY=PASS TRIANGLES=" + ",".join(map(str, triangles)))


if __name__ == "__main__":
    main()
