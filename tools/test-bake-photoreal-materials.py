#!/usr/bin/env python3
"""Schema and DDS contract checks for the offline material baker."""

import importlib.util
from pathlib import Path
from unittest.mock import patch


SCRIPT = Path(__file__).with_name("bake-photoreal-materials.py")
SPEC = importlib.util.spec_from_file_location("aevrynd_material_baker", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


valid = {"schemaVersion": 1, "binding": "runtime-sidecar", "outputResolution": 1024, "expectedMipCount": 11, "materials": [
    {"slot": 0, "baseColor": "textures/base.jpg", "normalGl": "textures/normal.jpg", "packedArm": "textures/arm.jpg"}]}
parsed = MODULE.parse_sidecar(valid)
assert parsed[0]["name"] == "slot-0"

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
assert "-srgb" in base_command and base_command[base_command.index("-f") + 1] == "BC7_UNORM_SRGB"
assert "-srgb" not in normal_command and normal_command[normal_command.index("-f") + 1] == "BC5_UNORM"
assert "-srgb" not in arm_command and arm_command[arm_command.index("-f") + 1] == "BC7_UNORM"
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

print("schema=pass packed-arm=pass separate-orm=pass srgb-base=required data-maps=linear hostile-color-space=blocked normalGL=required traversal=blocked")
