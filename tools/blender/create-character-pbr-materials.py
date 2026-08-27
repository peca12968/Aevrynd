import hashlib
import json
import math
import struct
import sys
import zlib
from pathlib import Path

import bpy


SEED = 0xAE7219


def output_directory() -> Path:
    separator = sys.argv.index("--")
    destination = Path(sys.argv[separator + 1]).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    return destination


def noise(x: int, y: int, seed: int) -> float:
    value = (x * 0x1F123BB5) ^ (y * 0x5F356495) ^ seed
    value ^= value >> 15
    value = (value * 0x2C1B3C6D) & 0xFFFFFFFF
    value ^= value >> 12
    value = (value * 0x297A2D39) & 0xFFFFFFFF
    value ^= value >> 15
    return (value & 0xFFFFFF) / 0xFFFFFF


def field(x: int, y: int, size: int, seed: int, scale: int) -> float:
    cell_x, cell_y = x // scale, y // scale
    fraction_x, fraction_y = (x % scale) / scale, (y % scale) / scale
    fraction_x = fraction_x * fraction_x * (3.0 - 2.0 * fraction_x)
    fraction_y = fraction_y * fraction_y * (3.0 - 2.0 * fraction_y)
    lower = noise(cell_x, cell_y, seed) * (1.0 - fraction_x) + noise(cell_x + 1, cell_y, seed) * fraction_x
    upper = noise(cell_x, cell_y + 1, seed) * (1.0 - fraction_x) + noise(cell_x + 1, cell_y + 1, seed) * fraction_x
    return lower * (1.0 - fraction_y) + upper * fraction_y


def height(kind: str, x: int, y: int, size: int, seed: int) -> float:
    u, v = x / size, y / size
    broad = field(x, y, size, seed, max(8, size // 32))
    detail = field(x, y, size, seed ^ 0x91E10DA5, max(3, size // 128))
    if kind == "skin":
        pores = max(0.0, noise(x, y, seed ^ 0x47A1) - 0.72) * 0.42
        return broad * 0.12 + detail * 0.08 - pores
    if kind == "cloth":
        weave = (math.sin(u * size * math.pi) + math.sin(v * size * math.pi)) * 0.035
        return broad * 0.06 + weave
    if kind == "leather":
        grain = abs(detail - 0.5) * 0.28
        return broad * 0.14 + grain
    if kind == "hair":
        strands = math.sin((u * 54.0 + v * 7.0 + broad * 2.0) * math.tau) * 0.12
        return strands + detail * 0.04
    if kind == "eye":
        radius = math.hypot(u - 0.5, v - 0.5)
        radial = math.sin(math.atan2(v - 0.5, u - 0.5) * 34.0 + radius * 90.0) * max(0.0, 0.48 - radius)
        return radial * 0.24 + detail * 0.025
    brushed = math.sin((u * 92.0 + broad * 1.4) * math.tau) * 0.025
    return brushed + detail * 0.012


def color(kind: str, channel: str, x: int, y: int, size: int, seed: int) -> tuple[int, int, int]:
    h = height(kind, x, y, size, seed)
    if channel == "normal-dx":
        left = height(kind, max(0, x - 1), y, size, seed)
        right = height(kind, min(size - 1, x + 1), y, size, seed)
        down = height(kind, x, max(0, y - 1), size, seed)
        up = height(kind, x, min(size - 1, y + 1), size, seed)
        nx, ny, nz = -(right - left) * 3.2, (up - down) * 3.2, 1.0
        length = math.sqrt(nx * nx + ny * ny + nz * nz)
        return tuple(round((component / length * 0.5 + 0.5) * 255) for component in (nx, ny, nz))
    fine = noise(x, y, seed ^ 0xD00DFEED) - 0.5
    if channel == "roughness":
        base = {"skin": 0.48, "cloth": 0.82, "leather": 0.52, "hair": 0.36, "eye": 0.18, "metal": 0.24}[kind]
        value = max(0.04, min(0.98, base + h * 0.18 + fine * 0.05))
        level = round(value * 255)
        return level, level, level
    # 執行期會再乘上角色頂點色，因此基底保持中性，只提供微小明暗細節。
    neutral = {"skin": 0.94, "cloth": 0.90, "leather": 0.88, "hair": 0.86, "eye": 0.97, "metal": 0.91}[kind]
    variation = h * 0.075 + fine * 0.018
    value = round(max(0.82, min(1.0, neutral + variation)) * 255)
    return value, value, value


def chunk(name: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + name + payload + struct.pack(">I", zlib.crc32(name + payload) & 0xFFFFFFFF)


def write_png(path: Path, size: int, kind: str, channel: str, seed: int) -> None:
    compressor = zlib.compressobj(9)
    compressed = bytearray()
    for y in range(size):
        row = bytearray((0,))
        for x in range(size):
            row.extend(color(kind, channel, x, y, size, seed))
        compressed.extend(compressor.compress(row))
    compressed.extend(compressor.flush())
    header = b"\x89PNG\r\n\x1a\n"
    header += chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
    path.write_bytes(header + chunk(b"IDAT", bytes(compressed)) + chunk(b"IEND", b""))


def write_hair_base_color(path: Path, size: int, seed: int) -> None:
    compressor = zlib.compressobj(9)
    compressed = bytearray()
    for y in range(size):
        row = bytearray((0,))
        v = y / max(1, size - 1)
        for x in range(size):
            rgb = color("hair", "base-color", x, y, size, seed)
            if x < size // 4:
                alpha = 255
            else:
                tile_width = size / 8.0
                local_u = ((x - size / 4.0) % tile_width) / tile_width
                strand = abs(local_u - 0.5)
                width = (0.43 - 0.18 * v) * (0.82 + 0.18 * noise(x // 3, y // 5, seed ^ 0xA17A))
                edge = max(0.0, min(1.0, (width - strand) * 40.0))
                tip = max(0.0, min(1.0, (1.0 - v) * 8.0))
                alpha = round(255.0 * edge * tip)
            row.extend((*rgb, alpha))
        compressed.extend(compressor.compress(row))
    compressed.extend(compressor.flush())
    header = b"\x89PNG\r\n\x1a\n"
    header += chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
    path.write_bytes(header + chunk(b"IDAT", bytes(compressed)) + chunk(b"IEND", b""))


def main() -> None:
    destination = output_directory()
    entries = []
    for material_index, kind in enumerate(("skin", "cloth", "leather", "hair", "eye", "metal")):
        size = 1024 if kind in ("skin", "eye") else 512
        folder = destination / kind
        folder.mkdir(parents=True, exist_ok=True)
        files = {}
        for channel_index, channel in enumerate(("base-color", "roughness", "normal-dx")):
            path = folder / f"{channel}.png"
            texture_seed = SEED ^ (material_index * 0x9E3779B9) ^ channel_index
            if kind == "hair" and channel == "base-color":
                write_hair_base_color(path, size, texture_seed)
            else:
                write_png(path, size, kind, channel, texture_seed)
            files[channel] = {"path": str(path.relative_to(destination)).replace("\\", "/"), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "bytes": path.stat().st_size}
        entries.append({"id": kind, "resolution": [size, size], "channels": files})
    manifest = {"schemaVersion": 1, "name": "Aevrynd Original Character PBR Materials", "seed": SEED,
                "license": "CC0-1.0", "origin": "Original procedural output generated by this project", "materials": entries}
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"CHARACTER_PBR_MATERIALS={len(entries)} FILES={len(entries) * 3} OUTPUT={destination}")


main()
