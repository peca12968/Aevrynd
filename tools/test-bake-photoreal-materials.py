#!/usr/bin/env python3
"""Schema and DDS contract checks for the offline material baker."""

import importlib.util
from pathlib import Path


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

print("schema=pass packed-arm=pass separate-orm=pass normalGL=required traversal=blocked")
