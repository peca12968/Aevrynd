#!/usr/bin/env python3
"""Hostile unit checks for the offline IBL baker."""

import importlib.util
import tempfile
from pathlib import Path

import numpy as np


SCRIPT = Path(__file__).with_name("bake-photoreal-ibl.py")
SPEC = importlib.util.spec_from_file_location("aevrynd_ibl_baker", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def expect_failure(action, label):
    try:
        action()
    except (ValueError, OverflowError):
        return
    raise AssertionError(label)


def radiance_bytes(width=8, height=2, orientation=b"-Y 2 +X 8", format_line=b"FORMAT=32-bit_rle_rgbe", trailing=b""):
    payload = bytearray(b"#?RADIANCE\n" + format_line + b"\n\n" + orientation + b"\n")
    for y in range(height):
        payload += bytes((2, 2, width >> 8, width & 255))
        # 中文註解：刻意同時使用 literal 與 run，覆蓋兩種合法 RLE packet。
        payload += bytes((width, *[(x + y + 1) & 255 for x in range(width)]))
        payload += bytes((128 + width, 64 + y))
        payload += bytes((128 + width, 128))
        payload += bytes((128 + width, 129))
    return bytes(payload) + trailing


with tempfile.TemporaryDirectory(prefix="aevrynd-ibl-test-") as temporary_name:
    temporary = Path(temporary_name)
    black = temporary / "black.hdr"
    width, height = 8, 2
    payload = bytearray(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 8\n")
    for _ in range(height):
        payload += bytes((2, 2, 0, width))
        for _ in range(4):
            payload += bytes((128 + width, 0))
    black.write_bytes(payload)
    expect_failure(lambda: MODULE.read_radiance(black), "black HDR passed")

    valid = temporary / "valid.hdr"
    valid.write_bytes(radiance_bytes())
    decoded = MODULE.read_radiance(valid)
    assert decoded.shape == (2, 8, 3) and np.isfinite(decoded).all() and decoded[0, 0, 0] != decoded[0, 1, 0]
    for name, bytes_ in (
        ("missing-format", radiance_bytes(format_line=b"EXPOSURE=1")),
        ("duplicate-format", radiance_bytes(format_line=b"FORMAT=32-bit_rle_rgbe\nFORMAT=32-bit_rle_rgbe")),
        ("wrong-orientation", radiance_bytes(orientation=b"+Y 2 +X 8")),
        ("trailing", radiance_bytes(trailing=b"x")),
        ("truncated", radiance_bytes()[:-1]),
    ):
        hostile = temporary / f"{name}.hdr"
        hostile.write_bytes(bytes_)
        expect_failure(lambda hostile=hostile: MODULE.read_radiance(hostile), f"{name} HDR passed")

    faces = [[np.ones((4, 4, 3), np.float32), np.ones((2, 2, 3), np.float32)] for _ in range(6)]
    dds = temporary / "cube.dds"
    MODULE.write_dds(dds, faces, 10, True)
    header = MODULE.read_dds_header(dds)
    assert header["width"] == 4 and header["height"] == 4 and header["mips"] == 2 and header["misc"] == 4
    expect_failure(lambda: MODULE.write_dds(temporary / "bad-face.dds", faces[:5], 10, True), "five-face cube passed")
    nonfinite = [[np.full((1, 1, 3), np.nan, np.float32)] for _ in range(6)]
    expect_failure(lambda: MODULE.write_dds(temporary / "nonfinite.dds", nonfinite, 10, True), "non-finite DDS passed")

    environment = np.zeros((4, 8, 3), np.float32)
    environment[..., 0] = np.arange(8, dtype=np.float32)[None, :]
    direction = np.array([[[0.0, 0.0, 1.0]]], np.float32)
    unrotated = MODULE.sample_environment(environment, direction, 0.0)
    rotated = MODULE.sample_environment(environment, direction, np.pi)
    assert not np.allclose(unrotated, rotated), "rotation did not change environment sampling"
    directions = MODULE.cube_directions(4)
    assert len(directions) == 6 and all(face.shape == (4, 4, 3) for face in directions)

print("rle-literal=pass rle-run=pass malformed=blocked black=blocked nonfinite=blocked faces=6 mips=2 rotation=changed")
