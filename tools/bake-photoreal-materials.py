#!/usr/bin/env python3
"""Validate first-batch runtime material sidecars and cook DirectX PBR DDS textures."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image


DIRECTXTEX_SHA256 = "dcfdec10244e02cf5037fba089c55fb7e1326b1c8181742d77d15fa5cb5eef06"
PEAK_WORKING_SET_LIMIT_BYTES = 2 * 1024 * 1024 * 1024
UINT64_MAX = (1 << 64) - 1
CHANNEL_COOK_CONTRACT = {
    "base": {"format": "BC7_UNORM_SRGB", "formatId": 99, "sourceColorSpace": "sRGB"},
    "normal": {"format": "BC5_UNORM", "formatId": 83, "sourceColorSpace": "linear"},
    "arm": {"format": "BC7_UNORM", "formatId": 98, "sourceColorSpace": "linear"},
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def peak_working_set_handle(handle: int) -> int:
    # 中文註解：用 Win32 實測 baker 與 texconv 的 PeakWorkingSet，receipt 只保存可重現的 gate 結果。
    if os.name != "nt":
        return 0
    import ctypes
    from ctypes import wintypes

    class Counters(ctypes.Structure):
        _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD),
                    ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
                    ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                    ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t)]
    counters = Counters()
    counters.cb = ctypes.sizeof(counters)
    ctypes.windll.psapi.GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.POINTER(Counters), wintypes.DWORD]
    ctypes.windll.psapi.GetProcessMemoryInfo.restype = wintypes.BOOL
    if not ctypes.windll.psapi.GetProcessMemoryInfo(handle, ctypes.byref(counters), counters.cb):
        return 0
    return int(counters.PeakWorkingSetSize)


def peak_working_set() -> int:
    if os.name != "nt":
        return 0
    import ctypes
    ctypes.windll.kernel32.GetCurrentProcess.restype = ctypes.c_void_p
    return peak_working_set_handle(ctypes.windll.kernel32.GetCurrentProcess())


def safe_file(root: Path, relative: str) -> Path:
    if not relative or Path(relative).is_absolute() or "\\" in relative or any(part in ("", ".", "..") for part in relative.split("/")):
        raise ValueError(f"Unsafe material path: {relative}")
    candidate = root.joinpath(*relative.split("/"))
    resolved_root, resolved = root.resolve(), candidate.resolve(strict=True)
    if resolved_root != resolved and resolved_root not in resolved.parents:
        raise ValueError(f"Material path escapes asset root: {relative}")
    current = resolved_root
    for part in resolved.relative_to(resolved_root).parts:
        current /= part
        if current.is_symlink():
            raise ValueError(f"Material path crosses a symlink: {relative}")
    if not resolved.is_file():
        raise ValueError(f"Material path is not a file: {relative}")
    return resolved


def dds_header(path: Path) -> dict[str, int]:
    data = path.read_bytes()[:148]
    if len(data) != 148 or data[:4] != b"DDS " or data[84:88] != b"DX10":
        raise ValueError("Cooked texture is not a DX10 DDS")
    import struct
    values = struct.unpack_from("<31I", data, 4)
    dx10 = struct.unpack_from("<5I", data, 128)
    return {"height": values[2], "width": values[3], "mips": values[6], "format": dx10[0], "dimension": dx10[1], "array": dx10[3]}


def texconv_command(texconv: Path, source: Path, output: Path, channel: str,
                    alpha_mode: str = "opaque", alpha_cutoff: float = 0.5) -> list[str]:
    contract = CHANNEL_COOK_CONTRACT.get(channel)
    if contract is None:
        raise ValueError(f"Unknown material cook channel: {channel}")
    format_is_srgb = contract["format"].endswith("_SRGB")
    source_is_srgb = contract["sourceColorSpace"] == "sRGB"
    if format_is_srgb != source_is_srgb or source_is_srgb != (channel == "base"):
        raise ValueError(f"Material color-space contract is inconsistent: {channel}")
    if alpha_mode not in ("opaque", "mask") or not isinstance(alpha_cutoff, (int, float)) or isinstance(alpha_cutoff, bool) or not math.isfinite(float(alpha_cutoff)) or not 0.0 < float(alpha_cutoff) < 1.0:
        raise ValueError("Material alpha cook contract is invalid")
    command = [str(texconv), "-nologo", "-y", "-dx10", "-m", "0", "-f", contract["format"]]
    # 中文註解：只有顏色貼圖宣告 sRGB 輸入；data map 一律保持 linear，避免二次 gamma。
    if source_is_srgb:
        command.append("-srgb")
    if channel == "base" and alpha_mode == "mask":
        # 中文註解：分離過濾 alpha 並保存 alpha-test 覆蓋率，避免葉片在遠距 mip 變薄或消失。
        command.extend(["-sepalpha", "--keep-coverage", f"{float(alpha_cutoff):g}"])
    command.extend(["-o", str(output.parent), str(source)])
    return command


def run_texconv(texconv: Path, source: Path, output: Path, channel: str,
                alpha_mode: str = "opaque", alpha_cutoff: float = 0.5) -> int:
    output.parent.mkdir(parents=True, exist_ok=True)
    completed = subprocess.Popen(texconv_command(texconv, source, output, channel, alpha_mode, alpha_cutoff),
                                 text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = completed.communicate(timeout=300)
    tool_peak = peak_working_set_handle(int(completed._handle)) if os.name == "nt" else 0
    if completed.returncode:
        raise RuntimeError(f"texconv failed: {stdout}\n{stderr}")
    generated = output.parent / (source.stem + ".dds")
    if generated != output:
        generated.replace(output)
    return tool_peak


def black_key_alpha(image: Image.Image, threshold: float) -> Image.Image:
    if not isinstance(threshold, (int, float)) or isinstance(threshold, bool) or not math.isfinite(float(threshold)) or not 0.0 < float(threshold) <= 0.25:
        raise ValueError("Black-key threshold is invalid")
    pixels = np.asarray(image.convert("RGBA"), dtype=np.uint8).copy()
    key = max(1, round(float(threshold) * 255.0))
    maximum = pixels[..., :3].max(axis=2)
    # 中文註解：黑底 JPEG 沒有可信的連續 alpha，明確二值化可避免把黑色預乘邊緣當成可見葉片。
    pixels[..., 3] = np.where(maximum > key, 255, 0).astype(np.uint8)
    return Image.fromarray(pixels, "RGBA")


def validate_mask_alpha(image: Image.Image, alpha_cutoff: float) -> None:
    if not isinstance(alpha_cutoff, (int, float)) or isinstance(alpha_cutoff, bool) or not math.isfinite(float(alpha_cutoff)) or not 0.0 < float(alpha_cutoff) < 1.0:
        raise ValueError("Mask alpha cutoff is invalid")
    alpha = np.asarray(image.convert("RGBA"), dtype=np.uint8)[..., 3]
    cutoff = round(float(alpha_cutoff) * 255.0)
    low, high = int(alpha.min()), int(alpha.max())
    # 中文註解：MASK 必須在 cutoff 兩側都有樣本；常數、全透明或全不透明 alpha 一律拒絕。
    if not low < cutoff <= high:
        raise ValueError("MASK alpha does not cross its cutoff")


def dilate_mask_rgb(image: Image.Image, radius: int, alpha_cutoff: float) -> Image.Image:
    if not isinstance(radius, int) or isinstance(radius, bool) or radius < 0 or radius > 32:
        raise ValueError("Mask RGB dilation radius is invalid")
    if not isinstance(alpha_cutoff, (int, float)) or isinstance(alpha_cutoff, bool) or not math.isfinite(float(alpha_cutoff)) or not 0.0 < float(alpha_cutoff) < 1.0:
        raise ValueError("Mask alpha cutoff is invalid")
    pixels = np.asarray(image.convert("RGBA"), dtype=np.uint8).copy()
    known = pixels[..., 3] >= round(float(alpha_cutoff) * 255.0)
    for _ in range(radius):
        sums = np.zeros((*known.shape, 3), dtype=np.uint32)
        counts = np.zeros(known.shape, dtype=np.uint8)
        for y_offset, x_offset in ((-1, -1), (-1, 0), (-1, 1), (0, -1),
                                   (0, 1), (1, -1), (1, 0), (1, 1)):
            source_y = slice(max(0, -y_offset), known.shape[0] - max(0, y_offset))
            source_x = slice(max(0, -x_offset), known.shape[1] - max(0, x_offset))
            target_y = slice(max(0, y_offset), known.shape[0] - max(0, -y_offset))
            target_x = slice(max(0, x_offset), known.shape[1] - max(0, -x_offset))
            mask = known[source_y, source_x]
            sums[target_y, target_x] += pixels[source_y, source_x, :3] * mask[..., None]
            counts[target_y, target_x] += mask.astype(np.uint8)
        targets = ~known & (counts > 0)
        if not np.any(targets):
            break
        pixels[targets, :3] = (sums[targets] // counts[targets, None]).astype(np.uint8)
        known[targets] = True
    return Image.fromarray(pixels, "RGBA")


def parse_sidecar(sidecar: dict) -> list[dict]:
    if not isinstance(sidecar, dict):
        raise ValueError("Material sidecar schema is invalid")
    schema_version = sidecar.get("schemaVersion")
    if (isinstance(schema_version, bool) or not isinstance(schema_version, int) or schema_version != 1 or
            not isinstance(sidecar.get("materials"), list) or not sidecar["materials"]):
        raise ValueError("Material sidecar schema is invalid")
    if "binding" in sidecar and sidecar["binding"] != "runtime-sidecar":
        raise ValueError("Material sidecar binding is invalid")
    convention = sidecar.get("normalConvention", "OpenGL")
    if convention != "OpenGL":
        raise ValueError("Only OpenGL source normals are accepted for explicit DirectX conversion")
    if (isinstance(sidecar.get("outputResolution"), bool) or not isinstance(sidecar.get("outputResolution"), int) or
            sidecar["outputResolution"] not in (512, 1024, 2048, 4096) or
            isinstance(sidecar.get("expectedMipCount"), bool) or not isinstance(sidecar.get("expectedMipCount"), int) or
            sidecar["expectedMipCount"] != int(math.log2(sidecar["outputResolution"])) + 1):
        raise ValueError("Material output resolution tier or mip contract is invalid")
    parsed = []
    names = set()
    for index, material in enumerate(sidecar["materials"]):
        if not isinstance(material, dict):
            raise ValueError("Material sidecar entry is invalid")
        slot = material.get("slot", index)
        if ("slot" in material and (isinstance(slot, bool) or not isinstance(slot, int) or
                                    slot < 0 or slot > UINT64_MAX)):
            raise ValueError("Material slot is invalid")
        name = material.get("name", f"slot-{slot}")
        if (not isinstance(name, str) or not name or len(name) > 128 or
                any(character not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_" for character in name)):
            raise ValueError("Material slot name is invalid or duplicated")
        name = name.lower()
        if name in names:
            raise ValueError("Material slot name is invalid or duplicated")
        names.add(name)
        base = material.get("baseColor")
        normal = material.get("normalGl") if "normalGl" in material else material.get("normal")
        arm = material.get("packedArm") if "packedArm" in material else material.get("arm")
        ambient_occlusion = material.get("ambientOcclusion")
        roughness = material.get("roughness")
        metallic_scalar = material.get("metallicScalar")
        if not all(isinstance(value, str) and value for value in (base, normal)):
            raise ValueError("baseColor and normalGL are mandatory")
        packed = isinstance(arm, str) and bool(arm)
        has_packed = "packedArm" in material or "arm" in material
        has_separate = any(field in material for field in ("ambientOcclusion", "roughness", "metallicScalar"))
        separate = (isinstance(ambient_occlusion, str) and bool(ambient_occlusion) and
                    isinstance(roughness, str) and bool(roughness) and
                    isinstance(metallic_scalar, (int, float)) and not isinstance(metallic_scalar, bool) and
                    math.isfinite(float(metallic_scalar)) and 0.0 <= float(metallic_scalar) <= 1.0)
        if (has_packed and not packed) or (packed and has_separate) or (has_separate and not separate) or packed == separate:
            raise ValueError("exactly one packed ARM or AO/roughness/metallicScalar source contract is mandatory")
        alpha_mode = material.get("alphaMode", "opaque")
        if alpha_mode not in ("opaque", "mask"):
            raise ValueError("Material alpha mode is invalid")
        allowed = {"name", "slot", "baseColor", "normalGl", "normal", "packedArm", "arm",
                   "ambientOcclusion", "roughness", "metallicScalar", "alphaMode", "alphaCutoff",
                   "doubleSided", "materialClass"}
        alpha_source, alpha_threshold, dilation = None, None, 0
        alpha_cutoff = material.get("alphaCutoff", 0.5)
        double_sided = material.get("doubleSided", False)
        material_class = material.get("materialClass", "surface")
        if not isinstance(alpha_cutoff, (int, float)) or isinstance(alpha_cutoff, bool) or not math.isfinite(float(alpha_cutoff)) or float(alpha_cutoff) != 0.5 or not isinstance(double_sided, bool) or material_class not in ("surface", "foliage"):
            raise ValueError("Material cutoff, sidedness, or class contract is invalid")
        if alpha_mode == "mask":
            allowed.update({"alphaSource", "alphaBlackKeyThreshold", "alphaDilationPixels"})
            alpha_source = material.get("alphaSource")
            dilation = material.get("alphaDilationPixels")
            if "alphaCutoff" not in material or alpha_source not in ("embedded", "black-key") or not isinstance(dilation, int) or isinstance(dilation, bool) or dilation < 0 or dilation > 32:
                raise ValueError("MASK alpha source, cutoff, or dilation contract is invalid")
            alpha_threshold = material.get("alphaBlackKeyThreshold")
            if alpha_source == "black-key":
                if not isinstance(alpha_threshold, (int, float)) or isinstance(alpha_threshold, bool) or not math.isfinite(float(alpha_threshold)) or not 0.0 < float(alpha_threshold) <= 0.25:
                    raise ValueError("MASK black-key threshold is invalid")
            elif "alphaBlackKeyThreshold" in material:
                raise ValueError("Embedded alpha must not declare a black-key threshold")
        if material_class == "foliage" and (alpha_mode != "mask" or not double_sided):
            raise ValueError("Foliage must be a double-sided MASK material")
        if any(key not in allowed for key in material):
            raise ValueError("Material sidecar entry contains an unknown field")
        parsed.append({"name": name, "base": base, "normal": normal, "arm": arm if packed else None,
                       "ambientOcclusion": ambient_occlusion if separate else None,
                       "roughness": roughness if separate else None,
                       "metallicScalar": float(metallic_scalar) if separate else None,
                       "alphaMode": alpha_mode, "alphaSource": alpha_source,
                       "alphaCutoff": float(alpha_cutoff), "alphaBlackKeyThreshold": alpha_threshold,
                       "alphaDilationPixels": dilation, "doubleSided": double_sided,
                       "materialClass": material_class})
    return parsed


def cook(args: argparse.Namespace) -> dict:
    root = Path(args.asset_root).resolve()
    manifest_path = Path(args.runtime_manifest).resolve()
    output = Path(args.output).resolve()
    texconv = Path(args.texconv).resolve()
    if sha256(texconv) != DIRECTXTEX_SHA256:
        raise ValueError("texconv is not the pinned official may2026 binary")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schemaVersion") != 1 or manifest.get("memoryGate") != "pass" or manifest.get("assetCount") != len(manifest.get("assets", [])):
        raise ValueError("Runtime manifest global gate failed")
    selected = set(args.asset or [])
    receipts = []
    tool_peak = 0
    for asset in manifest["assets"]:
        name = asset.get("assetName")
        if selected and name not in selected:
            continue
        files = asset.get("files")
        sidecars = [entry for entry in files or [] if isinstance(entry.get("path"), str) and entry["path"].endswith(".materials.json")]
        if not sidecars:
            continue
        if asset.get("memoryGate") != "pass" or asset.get("license") != "CC0-1.0" or asset.get("commercialUse") is not True:
            raise ValueError(f"Runtime asset gate failed: {name}")
        file_receipts = {entry["path"].replace("\\", "/"): entry for entry in files}
        sidecar_relative = sidecars[0]["path"].replace("\\", "/")
        sidecar_path = safe_file(root, sidecar_relative)
        if sha256(sidecar_path) != sidecars[0]["sha256"].lower():
            raise ValueError(f"Sidecar hash mismatch: {name}")
        sidecar = json.loads(sidecar_path.read_text(encoding="utf-8"))
        materials = parse_sidecar(sidecar)
        output_resolution = sidecar["outputResolution"]
        expected_mips = sidecar["expectedMipCount"]
        asset_directory = sidecar_path.parent
        asset_relative = Path(sidecar_relative).parent.as_posix()
        asset_outputs = []
        with tempfile.TemporaryDirectory(prefix="aevrynd-material-") as temporary_name:
            temporary = Path(temporary_name)
            for material in materials:
                source_paths = {}
                channels = ["base", "normal", "arm"] if material["arm"] else ["base", "normal", "ambientOcclusion", "roughness"]
                for channel in channels:
                    source = safe_file(asset_directory, material[channel])
                    relative = f"{asset_relative}/{material[channel]}"
                    receipt = file_receipts.get(relative)
                    if not receipt or sha256(source) != str(receipt.get("sha256", "")).lower() or source.stat().st_size != int(receipt.get("bytes", 0)):
                        raise ValueError(f"Unreceipted or changed {channel} source: {relative}")
                    source_paths[channel] = source
                images = {channel: Image.open(path).convert("RGBA") for channel, path in source_paths.items()}
                dimensions = {image.size for image in images.values()}
                if len(dimensions) != 1 or next(iter(dimensions))[0] > 4096 or next(iter(dimensions))[1] > 4096:
                    raise ValueError(f"Material channel dimensions do not match: {name}/{material['name']}")
                if material["alphaMode"] == "mask":
                    if material["alphaSource"] == "black-key":
                        images["base"] = black_key_alpha(images["base"], material["alphaBlackKeyThreshold"])
                    elif images["base"].getextrema()[3] == (255, 255):
                        raise ValueError(f"Embedded MASK base color has no alpha variation: {name}/{material['name']}")
                    validate_mask_alpha(images["base"], material["alphaCutoff"])
                    images["base"] = dilate_mask_rgb(images["base"], material["alphaDilationPixels"], material["alphaCutoff"])
                normal = np.asarray(images["normal"], dtype=np.uint8).copy()
                normal[..., 1] = 255 - normal[..., 1]
                if int(normal[..., :3].max()) == 0 or float(normal[..., 2].mean()) < 64.0:
                    raise ValueError("Normal map is black or has an invalid Z channel")
                if material["arm"]:
                    arm_image = images["arm"]
                    packing = "ARM preserved as AO/Roughness/Metallic"
                else:
                    ao = np.asarray(images["ambientOcclusion"].convert("L"), dtype=np.uint8)
                    roughness = np.asarray(images["roughness"].convert("L"), dtype=np.uint8)
                    metallic = np.full_like(ao, round(material["metallicScalar"] * 255.0))
                    alpha = np.full_like(ao, 255)
                    arm_image = Image.fromarray(np.stack((ao, roughness, metallic, alpha), axis=-1), "RGBA")
                    packing = f"AO/Roughness packed with explicit non-metallic scalar {material['metallicScalar']:g}"
                prepared = {"base": images["base"], "normal": Image.fromarray(normal, "RGBA"), "arm": arm_image}
                material_output = []
                for channel, image in prepared.items():
                    png = temporary / f"{material['name']}-{channel}.png"
                    image.save(png, format="PNG", compress_level=1)
                    destination = output / name / f"{material['name']}-{channel}.dds"
                    contract = CHANNEL_COOK_CONTRACT[channel]
                    tool_peak = max(tool_peak, run_texconv(texconv, png, destination, channel,
                                                          material["alphaMode"], material["alphaCutoff"]))
                    header = dds_header(destination)
                    if (header["format"] != contract["formatId"] or header["mips"] != expected_mips or
                            max(header["width"], header["height"]) != output_resolution or header["dimension"] != 3):
                        raise ValueError(f"Cooked DDS contract failed: {destination}")
                    material_output.append({"channel": channel, "path": destination.relative_to(output).as_posix(),
                                            "sourceColorSpace": contract["sourceColorSpace"],
                                            "bytes": destination.stat().st_size, "sha256": sha256(destination), **header})
                asset_outputs.append({"slot": material["name"], "alphaMode": material["alphaMode"],
                                      "alphaSource": material["alphaSource"], "alphaCutoff": material["alphaCutoff"],
                                      "alphaDilationPixels": material["alphaDilationPixels"],
                                      "doubleSided": material["doubleSided"], "materialClass": material["materialClass"],
                                      "normalConversion": "OpenGL green inverted to DirectX", "surfacePacking": packing,
                                      "sources": {channel: {"path": path.relative_to(root).as_posix(), "sha256": sha256(path)} for channel, path in source_paths.items()},
                                      "outputs": material_output})
        receipts.append({"assetName": name, "assetId": asset.get("id"), "officialPage": asset.get("officialPage"),
                         "outputResolution": output_resolution, "expectedMipCount": expected_mips,
                         "license": "CC0-1.0", "commercialUse": True, "materials": asset_outputs})
    if selected and selected != {receipt["assetName"] for receipt in receipts}:
        raise ValueError("One or more selected assets had no valid runtime sidecar")
    if not receipts:
        raise ValueError("No runtime sidecar materials were cooked")
    measured_peak = peak_working_set() + tool_peak
    if os.name == "nt" and (not measured_peak or measured_peak >= PEAK_WORKING_SET_LIMIT_BYTES):
        raise ValueError("Material cooker process-tree peak working set gate failed")
    result = {"schemaVersion": 1, "status": "pass", "runtimeManifestSha256": sha256(manifest_path),
              "quality": "Ultra", "mipPolicy": "complete-chain", "tool": {"name": "DirectXTex texconv", "tag": "may2026", "commit": "4feb3e1", "sha256": sha256(texconv)},
              "memoryGate": "pass", "peakWorkingSetLimitBytes": PEAK_WORKING_SET_LIMIT_BYTES,
              "memoryGateMethod": "Win32 GetProcessMemoryInfo baker peak plus maximum pinned texconv child peak",
              "assets": receipts}
    output.mkdir(parents=True, exist_ok=True)
    (output / "material-bake-receipt.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--asset-root", required=True)
    parser.add_argument("--runtime-manifest", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--texconv", required=True)
    parser.add_argument("--asset", action="append")
    args = parser.parse_args()
    print(json.dumps(cook(args), separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
