#!/usr/bin/env python3
"""Validate cooked first-batch AEMESH outputs and receipts without loading them together."""

import argparse
import hashlib
import json
import math
import pathlib
import struct


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path("engine/assets/realistic-cc0-first-batch/cooked/meshes"))
    root = parser.parse_args().root.resolve(strict=True)
    manifest = json.loads((root / "first-batch-aemesh-manifest.json").read_text(encoding="utf-8-sig"))
    receipts = sorted(root.glob("*/*-aemesh-receipt.json"))
    assert manifest["schemaVersion"] == 1 and len(receipts) == 18 and manifest["memoryGate"]
    files = 0
    total_bytes = total_triangles = 0
    island_slots = {}
    fir_slots = {}
    for receipt_path in receipts:
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
        assert receipt["memoryGate"] and receipt["peakWorkingSetBytes"] < 2 * 1024**3
        assert receipt["peakWorkingSetScope"] == "converter-process-no-children"
        assert receipt["materialPrimitiveBijection"] and receipt["outputCount"] == len(receipt["outputs"])
        if receipt["assetName"] == "island-tree-01": island_slots[receipt["lod"]] = sorted(item["materialIndex"] for item in receipt["outputs"])
        if receipt["assetName"] == "fir-tree-01-c": fir_slots[receipt["lod"]] = sorted(item["materialIndex"] for item in receipt["outputs"])
        for item in receipt["outputs"]:
            path = receipt_path.parent / item["file"]
            data = path.read_bytes()
            magic, version, vertices, indices = struct.unpack_from("<IIII", data)
            assert magic == 0x334D4541 and version == 1 and vertices == item["vertices"] and indices == item["indices"]
            assert len(data) == 16 + vertices * 28 + indices * 4 == item["bytes"]
            assert indices > 0 and indices % 3 == 0
            assert hashlib.sha256(data).hexdigest() == item["sha256"]
            for vertex in range(vertices):
                position_uv = struct.unpack_from("<fffIIff", data, 16 + vertex * 28)
                assert all(math.isfinite(value) for value in position_uv[:3] + position_uv[5:])
            positions = [struct.unpack_from("<fff", data, 16 + vertex * 28) for vertex in range(vertices)]
            actual_min = [min(value[axis] for value in positions) for axis in range(3)]
            actual_max = [max(value[axis] for value in positions) for axis in range(3)]
            expected_min = [item["boundsMinimumGltf"][0], item["boundsMinimumGltf"][1], -item["boundsMaximumGltf"][2]]
            expected_max = [item["boundsMaximumGltf"][0], item["boundsMaximumGltf"][1], -item["boundsMinimumGltf"][2]]
            assert all(abs(actual_min[axis] - expected_min[axis]) < 1e-4 and abs(actual_max[axis] - expected_max[axis]) < 1e-4 for axis in range(3))
            index_offset = 16 + vertices * 28
            for index in struct.iter_unpack("<I", memoryview(data)[index_offset:]): assert index[0] < vertices
            files += 1; total_bytes += len(data); total_triangles += indices // 3
    assert island_slots == {0: [0, 1, 2], 1: [0, 1, 2], 2: [0, 1, 2]}
    assert fir_slots == {0: [0, 1, 2, 3], 1: [0, 1, 2, 3], 2: [0, 1, 2, 3]}
    assert files == manifest["outputFiles"] and total_bytes == manifest["totalBytes"] and total_triangles == manifest["totalTriangles"]
    print(f"firstBatchAemesh=pass files={files} bytes={total_bytes} triangles={total_triangles} peak={manifest['maximumPeakWorkingSetBytes']} islandSlots=3x3 firSlots=3x4")


if __name__ == "__main__":
    main()
