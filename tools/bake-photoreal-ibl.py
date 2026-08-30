#!/usr/bin/env python3
"""Bake a Radiance equirectangular environment into Aevrynd DX10 DDS IBL assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import struct
import subprocess
import tempfile
from pathlib import Path

import numpy as np


DIRECTXTEX_SHA256 = "dcfdec10244e02cf5037fba089c55fb7e1326b1c8181742d77d15fa5cb5eef06"
DIRECTXTEX_TAG = "may2026"
DIRECTXTEX_COMMIT = "4feb3e1"
PEAK_WORKING_SET_LIMIT_BYTES = 2 * 1024 * 1024 * 1024


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def peak_working_set_handle(handle: int) -> int:
    # 中文註解：從實際 Win32 處理序 handle 讀 PeakWorkingSet，不用估算值冒充量測值。
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


def read_radiance(path: Path) -> np.ndarray:
    if not path.is_file() or path.stat().st_size > 256 * 1024 * 1024:
        raise ValueError("Radiance source size is outside the safe limit")
    data = path.read_bytes()
    if len(data) < 64 or not data.startswith((b"#?RADIANCE", b"#?RGBE")):
        raise ValueError("Radiance signature is missing")
    header_end = data.find(b"\n\n")
    if header_end < 0:
        raise ValueError("Radiance header is unterminated")
    header_lines = data[:header_end].decode("ascii", "strict").splitlines()
    if header_lines.count("FORMAT=32-bit_rle_rgbe") != 1:
        raise ValueError("Radiance RLE format declaration is missing or duplicated")
    resolution_end = data.find(b"\n", header_end + 2)
    if resolution_end < 0:
        raise ValueError("Radiance resolution is missing")
    tokens = data[header_end + 2:resolution_end].decode("ascii", "strict").split()
    if len(tokens) != 4 or tokens[0] != "-Y" or tokens[2] != "+X":
        raise ValueError("Only canonical -Y +X Radiance images are accepted")
    height, width = int(tokens[1]), int(tokens[3])
    if width < 8 or width > 32767 or height <= 0 or width > 16384 or height > 8192:
        raise ValueError("Radiance dimensions are outside the safe limit")
    position = resolution_end + 1
    rgbe = np.empty((height, width, 4), dtype=np.uint8)
    for y in range(height):
        if position + 4 > len(data) or data[position:position + 2] != b"\x02\x02":
            raise ValueError("Legacy or truncated Radiance scanline is not accepted")
        scan_width = (data[position + 2] << 8) | data[position + 3]
        position += 4
        if scan_width != width:
            raise ValueError("Radiance scanline width mismatch")
        for channel in range(4):
            x = 0
            while x < width:
                if position >= len(data):
                    raise ValueError("Truncated Radiance RLE packet")
                count = data[position]
                position += 1
                if count > 128:
                    run = count - 128
                    if not run or position >= len(data) or x + run > width:
                        raise ValueError("Invalid Radiance RLE run")
                    rgbe[y, x:x + run, channel] = data[position]
                    position += 1
                    x += run
                else:
                    if not count or position + count > len(data) or x + count > width:
                        raise ValueError("Invalid Radiance RLE literal")
                    rgbe[y, x:x + count, channel] = np.frombuffer(data[position:position + count], np.uint8)
                    position += count
                    x += count
    if position != len(data):
        raise ValueError("Radiance image has trailing or unparsed bytes")
    exponent = rgbe[..., 3].astype(np.int32)
    scale = np.where(exponent > 0, np.ldexp(np.ones_like(exponent, dtype=np.float32), exponent - 136), 0.0)
    rgb = (rgbe[..., :3].astype(np.float32) + 0.5) * scale[..., None]
    if not np.isfinite(rgb).all() or float(np.max(rgb)) <= 0.0:
        raise ValueError("Radiance image is black or non-finite")
    return rgb


def cube_directions(size: int) -> list[np.ndarray]:
    coordinate = (2.0 * (np.arange(size, dtype=np.float32) + 0.5) / size) - 1.0
    u, v = np.meshgrid(coordinate, coordinate)
    faces = [np.stack((np.ones_like(u), -v, -u), -1), np.stack((-np.ones_like(u), -v, u), -1),
             np.stack((u, np.ones_like(u), v), -1), np.stack((u, -np.ones_like(u), -v), -1),
             np.stack((u, -v, np.ones_like(u)), -1), np.stack((-u, -v, -np.ones_like(u)), -1)]
    return [face / np.linalg.norm(face, axis=-1, keepdims=True) for face in faces]


def sample_environment(environment: np.ndarray, directions: np.ndarray, rotation: float) -> np.ndarray:
    height, width, _ = environment.shape
    longitude = np.arctan2(directions[..., 2], directions[..., 0]) + rotation
    latitude = np.arccos(np.clip(directions[..., 1], -1.0, 1.0))
    x = np.mod(longitude / (2.0 * np.pi) + 0.5, 1.0) * width - 0.5
    y = np.clip(latitude / np.pi * height - 0.5, 0.0, height - 1.0)
    x0 = np.floor(x).astype(np.int64) % width
    y0 = np.floor(y).astype(np.int64)
    x1 = (x0 + 1) % width
    y1 = np.minimum(y0 + 1, height - 1)
    tx, ty = (x - np.floor(x))[..., None], (y - np.floor(y))[..., None]
    top = environment[y0, x0] * (1.0 - tx) + environment[y0, x1] * tx
    bottom = environment[y1, x0] * (1.0 - tx) + environment[y1, x1] * tx
    return top * (1.0 - ty) + bottom * ty


def radical_inverse(index: np.ndarray) -> np.ndarray:
    bits = index.astype(np.uint32)
    bits = ((bits << 16) | (bits >> 16))
    bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1)
    bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2)
    bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4)
    bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8)
    return bits.astype(np.float64) * 2.3283064365386963e-10


def tangent_basis(normal: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    up = np.where((np.abs(normal[:, 1]) < 0.999)[:, None], np.array([0.0, 1.0, 0.0]), np.array([1.0, 0.0, 0.0]))
    tangent = np.cross(up, normal)
    tangent /= np.linalg.norm(tangent, axis=1, keepdims=True)
    return tangent, np.cross(normal, tangent)


def prefilter_face(environment: np.ndarray, directions: np.ndarray, roughness: float, samples: int, rotation: float) -> np.ndarray:
    flat = directions.reshape(-1, 3)
    if roughness <= 1e-5:
        return sample_environment(environment, directions, rotation).astype(np.float32)
    result = np.empty((flat.shape[0], 3), np.float32)
    xi_index = np.arange(samples, dtype=np.uint32)
    xi_x = (xi_index.astype(np.float64) + 0.5) / samples
    xi_y = radical_inverse(xi_index)
    alpha = max(roughness * roughness, 1e-4)
    phi = 2.0 * np.pi * xi_x
    cos_theta = np.sqrt((1.0 - xi_y) / (1.0 + (alpha * alpha - 1.0) * xi_y))
    sin_theta = np.sqrt(np.maximum(0.0, 1.0 - cos_theta * cos_theta))
    local_h = np.stack((np.cos(phi) * sin_theta, np.sin(phi) * sin_theta, cos_theta), axis=1).astype(np.float32)
    for start in range(0, flat.shape[0], 256):
        normal = flat[start:start + 256]
        tangent, bitangent = tangent_basis(normal)
        half_vector = (tangent[:, None, :] * local_h[None, :, 0:1] + bitangent[:, None, :] * local_h[None, :, 1:2] + normal[:, None, :] * local_h[None, :, 2:3])
        reflected = 2.0 * np.sum(normal[:, None, :] * half_vector, axis=2, keepdims=True) * half_vector - normal[:, None, :]
        weight = np.maximum(np.sum(normal[:, None, :] * reflected, axis=2), 0.0)
        values = sample_environment(environment, reflected, rotation)
        denominator = np.maximum(weight.sum(axis=1, keepdims=True), 1e-8)
        result[start:start + normal.shape[0]] = (values * weight[..., None]).sum(axis=1) / denominator
    return result.reshape(directions.shape).astype(np.float32)


def irradiance_face(environment: np.ndarray, directions: np.ndarray, samples: int, rotation: float) -> np.ndarray:
    flat = directions.reshape(-1, 3)
    result = np.empty((flat.shape[0], 3), np.float32)
    index = np.arange(samples, dtype=np.uint32)
    r = np.sqrt((index.astype(np.float64) + 0.5) / samples)
    phi = 2.0 * np.pi * radical_inverse(index)
    local = np.stack((r * np.cos(phi), r * np.sin(phi), np.sqrt(1.0 - r * r)), axis=1).astype(np.float32)
    for start in range(0, flat.shape[0], 512):
        normal = flat[start:start + 512]
        tangent, bitangent = tangent_basis(normal)
        world = tangent[:, None, :] * local[None, :, 0:1] + bitangent[:, None, :] * local[None, :, 1:2] + normal[:, None, :] * local[None, :, 2:3]
        result[start:start + normal.shape[0]] = sample_environment(environment, world, rotation).mean(axis=1) * np.pi
    return result.reshape(directions.shape).astype(np.float32)


def integrate_brdf(size: int, samples: int) -> np.ndarray:
    output = np.zeros((size, size, 2), np.float32)
    index = np.arange(samples, dtype=np.uint32)
    xi_x = (index.astype(np.float64) + 0.5) / samples
    xi_y = radical_inverse(index)
    for y in range(size):
        roughness = (y + 0.5) / size
        alpha = roughness * roughness
        phi = 2.0 * np.pi * xi_x
        cos_theta = np.sqrt((1.0 - xi_y) / (1.0 + (alpha * alpha - 1.0) * xi_y))
        sin_theta = np.sqrt(np.maximum(0.0, 1.0 - cos_theta * cos_theta))
        h = np.stack((np.cos(phi) * sin_theta, np.sin(phi) * sin_theta, cos_theta), axis=1)
        for x in range(size):
            nov = (x + 0.5) / size
            v = np.array([math.sqrt(max(0.0, 1.0 - nov * nov)), 0.0, nov])
            voh = np.maximum(h @ v, 0.0)
            l = 2.0 * voh[:, None] * h - v
            nol = np.maximum(l[:, 2], 0.0)
            noh = np.maximum(h[:, 2], 0.0)
            valid = nol > 0.0
            k = (roughness * roughness) * 0.5
            gv = nov / (nov * (1.0 - k) + k)
            gl = nol / (nol * (1.0 - k) + k)
            visibility = np.where(valid, (gv * gl * voh) / np.maximum(noh * nov, 1e-8), 0.0)
            fresnel = np.power(1.0 - voh, 5.0)
            output[y, x, 0] = np.mean((1.0 - fresnel) * visibility)
            output[y, x, 1] = np.mean(fresnel * visibility)
    return output


def write_dds(path: Path, faces: list[list[np.ndarray]], dxgi_format: int, cube: bool) -> None:
    mip_count = len(faces[0])
    width = faces[0][0].shape[1]
    height = faces[0][0].shape[0]
    if any(len(face) != mip_count for face in faces) or (cube and len(faces) != 6):
        raise ValueError("DDS face/mip topology is invalid")
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x8
    if mip_count > 1:
        flags |= 0x20000
    caps = 0x1000 | (0x8 | 0x400000 if mip_count > 1 else 0)
    caps2 = 0xFE00 if cube else 0
    header = struct.pack("<7I11I8I5I", 124, flags, height, width, width * (8 if dxgi_format == 10 else 4), 0, mip_count,
                         *([0] * 11), 32, 0x4, struct.unpack("<I", b"DX10")[0], 0, 0, 0, 0, 0,
                         caps, caps2, 0, 0, 0)
    dx10 = struct.pack("<5I", dxgi_format, 3, 0x4 if cube else 0, 1, 0)
    with path.open("wb") as stream:
        stream.write(b"DDS " + header + dx10)
        for face in faces:
            for image in face:
                if not np.isfinite(image).all():
                    raise ValueError("Non-finite texel cannot be written")
                if dxgi_format == 10:
                    rgba = np.ones((*image.shape[:2], 4), np.float16)
                    rgba[..., :3] = np.maximum(image[..., :3], 0.0).astype(np.float16)
                    stream.write(rgba.astype("<f2").tobytes())
                elif dxgi_format == 34:
                    stream.write(np.maximum(image[..., :2], 0.0).astype("<f2").tobytes())
                else:
                    raise ValueError("Unsupported DDS source format")


def read_dds_header(path: Path) -> dict[str, int]:
    data = path.read_bytes()[:148]
    if len(data) != 148 or data[:4] != b"DDS " or data[84:88] != b"DX10":
        raise ValueError("DDS DX10 header is invalid")
    values = struct.unpack_from("<31I", data, 4)
    dx10 = struct.unpack_from("<5I", data, 128)
    return {"height": values[2], "width": values[3], "mips": values[6], "caps2": values[27], "format": dx10[0], "dimension": dx10[1], "misc": dx10[2], "array": dx10[3]}


def run_texconv(texconv: Path, source: Path, output: Path, format_name: str) -> int:
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [str(texconv), "-nologo", "-y", "-m", "0", "-f", format_name, "-o", str(output.parent), str(source)]
    completed = subprocess.Popen(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = completed.communicate(timeout=600)
    tool_peak = peak_working_set_handle(int(completed._handle)) if os.name == "nt" else 0
    if completed.returncode:
        raise RuntimeError(f"texconv failed: {stdout}\n{stderr}")
    generated = output.parent / source.name
    if generated != output:
        generated.replace(output)
    return tool_peak


def bake(args: argparse.Namespace) -> dict:
    source, output, texconv = Path(args.source).resolve(), Path(args.output).resolve(), Path(args.texconv).resolve()
    if sha256(texconv) != DIRECTXTEX_SHA256:
        raise ValueError("texconv hash does not match the pinned official may2026 binary")
    if sha256(source) != args.source_sha256.lower():
        raise ValueError("HDR source hash mismatch")
    environment = read_radiance(source)
    rotation = math.radians(args.rotation_degrees)
    output.mkdir(parents=True, exist_ok=True)
    tool_peak = 0
    with tempfile.TemporaryDirectory(prefix="aevrynd-ibl-") as temporary:
        temporary = Path(temporary)
        mip_count = int(math.log2(args.specular_size)) + 1
        specular_faces = [[] for _ in range(6)]
        for mip in range(mip_count):
            size = max(1, args.specular_size >> mip)
            roughness = mip / max(1, mip_count - 1)
            for face, directions in enumerate(cube_directions(size)):
                specular_faces[face].append(prefilter_face(environment, directions, roughness, args.specular_samples, rotation))
        specular_fp = temporary / "environment-specular.dds"
        write_dds(specular_fp, specular_faces, 10, True)
        tool_peak = max(tool_peak, run_texconv(texconv, specular_fp, output / "environment-specular.dds", "BC6H_UF16"))

        irradiance_faces = [[irradiance_face(environment, direction, args.irradiance_samples, rotation)] for direction in cube_directions(args.irradiance_size)]
        irradiance_fp = temporary / "environment-irradiance.dds"
        write_dds(irradiance_fp, irradiance_faces, 10, True)
        tool_peak = max(tool_peak, run_texconv(texconv, irradiance_fp, output / "environment-irradiance.dds", "BC6H_UF16"))

        brdf = integrate_brdf(args.brdf_size, args.brdf_samples)
        write_dds(output / "environment-brdf.dds", [[brdf]], 34, False)

    outputs = []
    for name, expected_format in (("environment-specular.dds", 95), ("environment-irradiance.dds", 95), ("environment-brdf.dds", 34)):
        path = output / name
        header = read_dds_header(path)
        if header["format"] != expected_format or header["dimension"] != 3:
            raise ValueError(f"{name} has an unexpected DX10 format")
        outputs.append({"path": name, "bytes": path.stat().st_size, "sha256": sha256(path), **header})
    measured_peak = peak_working_set() + tool_peak
    if os.name == "nt" and (not measured_peak or measured_peak >= PEAK_WORKING_SET_LIMIT_BYTES):
        raise ValueError("IBL baker process-tree peak working set gate failed")
    receipt = {
        "schemaVersion": 1, "status": "pass", "source": str(source), "sourceSha256": sha256(source),
        "sourceLicense": "CC0-1.0", "commercialUse": True, "rotationDegrees": args.rotation_degrees,
        "specularSamples": args.specular_samples, "irradianceSamples": args.irradiance_samples, "brdfSamples": args.brdf_samples,
        "tool": {"name": "DirectXTex texconv", "tag": DIRECTXTEX_TAG, "commit": DIRECTXTEX_COMMIT,
                 "officialUrl": "https://github.com/microsoft/DirectXTex/releases/tag/may2026", "sha256": sha256(texconv)},
        "memoryGate": "pass", "peakWorkingSetLimitBytes": PEAK_WORKING_SET_LIMIT_BYTES,
        "memoryGateMethod": "Win32 GetProcessMemoryInfo baker peak plus maximum pinned texconv child peak",
        "outputs": outputs,
    }
    (output / "ibl-bake-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    return receipt


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--source-sha256", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--texconv", required=True)
    parser.add_argument("--specular-size", type=int, default=128)
    parser.add_argument("--irradiance-size", type=int, default=32)
    parser.add_argument("--brdf-size", type=int, default=128)
    parser.add_argument("--specular-samples", type=int, default=64)
    parser.add_argument("--irradiance-samples", type=int, default=128)
    parser.add_argument("--brdf-samples", type=int, default=128)
    parser.add_argument("--rotation-degrees", type=float, default=0.0)
    args = parser.parse_args()
    if args.specular_size < 16 or args.specular_size & (args.specular_size - 1) or args.irradiance_size > 128 or args.brdf_size > 512:
        raise SystemExit("IBL dimensions are invalid")
    print(json.dumps(bake(args), separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
