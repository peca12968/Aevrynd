#!/usr/bin/env python3
"""Schema and DDS contract checks for the offline material baker."""

import importlib.util
import json
from pathlib import Path
from unittest.mock import patch

import numpy as np
from PIL import Image


SCRIPT = Path(__file__).with_name("bake-photoreal-materials.py")
SPEC = importlib.util.spec_from_file_location("aevrynd_material_baker", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


valid = {"schemaVersion": 1, "binding": "runtime-sidecar", "outputResolution": 1024, "expectedMipCount": 11, "materials": [
    {"slot": 0, "baseColor": "textures/base.jpg", "normalGl": "textures/normal.jpg", "packedArm": "textures/arm.jpg"}]}
parsed = MODULE.parse_sidecar(valid)
assert parsed[0]["name"] == "slot-0" and parsed[0]["alphaCutoff"] == 0.5
assert parsed[0]["doubleSided"] is False and parsed[0]["materialClass"] == "surface"

separate = {"schemaVersion": 1, "binding": "runtime-sidecar", "outputResolution": 1024, "expectedMipCount": 11, "materials": [
    {"name": "ground", "baseColor": "textures/base.jpg", "normalGl": "textures/normal.jpg",
     "ambientOcclusion": "textures/ao.jpg", "roughness": "textures/rough.jpg", "metallicScalar": 0.0}]}
separate_parsed = MODULE.parse_sidecar(separate)
assert separate_parsed[0]["arm"] is None and separate_parsed[0]["metallicScalar"] == 0.0

# 中文註解：顏色貼圖必須有 -srgb，法線與 ORM 是 data map，絕不允許 gamma 轉換。
texconv = Path("texconv.exe")
source = Path("fixture.png")
output = Path("cooked.dds")
base_command = MODULE.texconv_command(texconv, source, output, "base")
normal_command = MODULE.texconv_command(texconv, source, output, "normal")
arm_command = MODULE.texconv_command(texconv, source, output, "arm")
mask_command = MODULE.texconv_command(texconv, source, output, "base", "mask", 0.5)
assert "-srgb" in base_command and base_command[base_command.index("-f") + 1] == "BC7_UNORM_SRGB"
assert "-srgb" not in normal_command and normal_command[normal_command.index("-f") + 1] == "BC5_UNORM"
assert "-srgb" not in arm_command and arm_command[arm_command.index("-f") + 1] == "BC7_UNORM"
assert "-sepalpha" not in base_command and "--keep-coverage" not in base_command
assert "-sepalpha" in mask_command and mask_command[mask_command.index("--keep-coverage") + 1] == "0.5"
assert "-sepalpha" not in MODULE.texconv_command(texconv, source, output, "normal", "mask", 0.5)
try:
    MODULE.texconv_command(texconv, source, output, "height")
except ValueError:
    pass
else:
    raise AssertionError("unknown data channel did not fail closed")

with patch.dict(MODULE.CHANNEL_COOK_CONTRACT, {"arm": {"format": "BC7_UNORM_SRGB", "formatId": 99, "sourceColorSpace": "sRGB"}}):
    try:
        MODULE.texconv_command(texconv, source, output, "arm")
    except ValueError:
        pass
    else:
        raise AssertionError("data map accepted an sRGB cook contract")

for mutation in (
    {"schemaVersion": 2, "outputResolution": 1024, "expectedMipCount": 11, "materials": valid["materials"]},
    {"schemaVersion": 1, "binding": "embedded", "outputResolution": 1024, "expectedMipCount": 11, "materials": valid["materials"]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{"slot": 0, "baseColor": "a", "normalGl": "b"}]},
    {"schemaVersion": 1, "normalConvention": "DirectX", "outputResolution": 1024, "expectedMipCount": 11, "materials": valid["materials"]},
    {"schemaVersion": 1, "outputResolution": 1000, "expectedMipCount": 10, "materials": valid["materials"]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 10, "materials": valid["materials"]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{"slot": 0, "baseColor": "a", "normalGl": "b",
                                          "ambientOcclusion": "c", "roughness": "d", "metallicScalar": -0.1}]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{"slot": 0, "baseColor": "a", "normalGl": "b",
                                          "packedArm": "c", "ambientOcclusion": "d", "roughness": "e", "metallicScalar": 0.0}]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{"slot": 0, "baseColor": "a", "normalGl": "b", "packedArm": "c", "alphaMode": "mask"}]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{"slot": 0, "baseColor": "a", "normalGl": "b", "packedArm": "c", "alphaMode": "mask", "alphaSource": "black-key", "alphaCutoff": 0.4, "alphaBlackKeyThreshold": 0.05, "alphaDilationPixels": 4}]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{"slot": 0, "baseColor": "a", "normalGl": "b", "packedArm": "c", "alphaMode": "mask", "alphaSource": "black-key", "alphaCutoff": 0.5, "alphaBlackKeyThreshold": 0.0, "alphaDilationPixels": 4}]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{"slot": 0, "baseColor": "a", "normalGl": "b", "packedArm": "c", "alphaMode": "mask", "alphaSource": "embedded", "alphaCutoff": 0.5, "alphaBlackKeyThreshold": 0.05, "alphaDilationPixels": 4}]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{"slot": 0, "baseColor": "a", "normalGl": "b", "packedArm": "c", "alphaMode": "opaque", "alphaSource": "black-key"}]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{"slot": 0, "baseColor": "a", "normalGl": "b", "packedArm": "c", "materialClass": "foliage", "doubleSided": True}]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{"slot": 0, "baseColor": "a", "normalGl": "b", "packedArm": "c", "alphaMode": "mask", "alphaSource": "embedded", "alphaCutoff": 0.5, "alphaDilationPixels": 4, "materialClass": "foliage", "doubleSided": False}]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{"slot": 0, "baseColor": "a", "normalGl": "b", "packedArm": "c", "materialClass": "water"}]},
    {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{"slot": 0, "baseColor": "a", "normalGl": "b", "packedArm": "c", "doubleSided": "true"}]},
):
    try:
        MODULE.parse_sidecar(mutation)
    except ValueError:
        continue
    raise AssertionError("hostile material sidecar passed")

try:
    MODULE.safe_file(Path.cwd(), "../outside.jpg")
except ValueError:
    pass
else:
    raise AssertionError("path traversal passed")

mask_sidecar = {"schemaVersion": 1, "outputResolution": 1024, "expectedMipCount": 11, "materials": [{
    "name": "leaves", "baseColor": "a", "normalGl": "b", "packedArm": "c", "alphaMode": "mask",
    "alphaSource": "black-key", "alphaBlackKeyThreshold": 16 / 255, "alphaCutoff": 0.5,
    "alphaDilationPixels": 1}]}
mask_material = MODULE.parse_sidecar(mask_sidecar)[0]
assert mask_material["alphaSource"] == "black-key" and mask_material["alphaDilationPixels"] == 1
island_root = SCRIPT.parent.parent / "engine" / "assets" / "realistic-cc0-first-batch" / "island-tree-01"
island_sidecar = json.loads((island_root / "island-tree-01.materials.json").read_text(encoding="utf-8"))
island_leaves = next(item for item in MODULE.parse_sidecar(island_sidecar) if item["name"] == "leaves")
assert island_leaves["alphaSource"] == "black-key" and island_leaves["alphaCutoff"] == 0.5 and island_leaves["alphaDilationPixels"] == 8
assert island_leaves["doubleSided"] is True and island_leaves["materialClass"] == "foliage"
island_keyed = MODULE.black_key_alpha(Image.open(island_root / island_leaves["base"]), island_leaves["alphaBlackKeyThreshold"])
MODULE.validate_mask_alpha(island_keyed, island_leaves["alphaCutoff"])
fixture_pixels = np.zeros((5, 5, 4), dtype=np.uint8)
fixture_pixels[..., 3] = 255
fixture_pixels[2, 2, :3] = (80, 110, 30)
fixture_pixels[0, 0, :3] = (16, 0, 0)
keyed = MODULE.black_key_alpha(Image.fromarray(fixture_pixels, "RGBA"), 16 / 255)
keyed_pixels = np.asarray(keyed)
assert keyed_pixels[0, 0, 3] == 0 and keyed_pixels[2, 2, 3] == 255
MODULE.validate_mask_alpha(keyed, 0.5)
dilated = np.asarray(MODULE.dilate_mask_rgb(keyed, 1, 0.5))
assert dilated[2, 1, :3].tolist() == [80, 110, 30] and dilated[2, 1, 3] == 0
assert np.array_equal(dilated[..., 3], keyed_pixels[..., 3])

for invalid_alpha in (0, 128, 255):
    invalid_pixels = np.zeros((4, 4, 4), dtype=np.uint8)
    invalid_pixels[..., :3] = 80
    invalid_pixels[..., 3] = invalid_alpha
    try:
        MODULE.validate_mask_alpha(Image.fromarray(invalid_pixels, "RGBA"), 0.5)
    except ValueError:
        pass
    else:
        raise AssertionError("constant embedded MASK alpha passed")

for black_key_rgb in ((0, 0, 0), (90, 120, 30)):
    invalid_pixels = np.zeros((4, 4, 4), dtype=np.uint8)
    invalid_pixels[..., :3] = black_key_rgb
    invalid_pixels[..., 3] = 255
    invalid_keyed = MODULE.black_key_alpha(Image.fromarray(invalid_pixels, "RGBA"), 16 / 255)
    try:
        MODULE.validate_mask_alpha(invalid_keyed, 0.5)
    except ValueError:
        pass
    else:
        raise AssertionError("constant black-key MASK alpha passed")

print("schema=pass packed-arm=pass separate-orm=pass srgb-base=required data-maps=linear mask-black-key=explicit mask-alpha-range=fail-closed mask-rgb-dilation=pass mask-mip-coverage=preserved hostile-color-space=blocked normalGL=required traversal=blocked")
