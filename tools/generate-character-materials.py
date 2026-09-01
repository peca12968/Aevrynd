from __future__ import annotations

import hashlib
import json
import math
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageEnhance


def resized(source: Path, size: int, mode: str | None = None) -> Image.Image:
    image = Image.open(source)
    if mode is not None:
        image = image.convert(mode)
    return image.resize((size, size), Image.Resampling.LANCZOS)


def save(image: Image.Image, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    image.save(destination, format="PNG", optimize=False, compress_level=9)


def tinted(image: Image.Image, color: tuple[int, int, int], amount: float) -> Image.Image:
    overlay = Image.new("RGB", image.size, color)
    return Image.blend(image.convert("RGB"), ImageChops.multiply(image.convert("RGB"), overlay), amount)


def constant_texture(size: int, value: int) -> Image.Image:
    return Image.new("RGB", (size, size), (value, value, value))


def iris_texture(size: int) -> Image.Image:
    pixels = bytearray(size * size * 3)
    center = (size - 1) * 0.5
    for y in range(size):
        dy = (y - center) / center
        for x in range(size):
            dx = (x - center) / center
            radius = math.sqrt(dx * dx + dy * dy)
            angle = math.atan2(dy, dx)
            radial = 0.5 + 0.5 * math.sin(radius * 168.0 + math.sin(angle * 17.0) * 2.2)
            spoke = 0.5 + 0.5 * math.sin(angle * 83.0 + radius * 31.0)
            pupil = 1.0 if radius > 0.22 else max(0.0, radius / 0.22)
            rim = max(0.0, min(1.0, (0.96 - radius) * 18.0))
            light = (0.32 + 0.42 * radial + 0.20 * spoke) * pupil * rim
            index = (y * size + x) * 3
            pixels[index:index + 3] = bytes((int(48 + 70 * light), int(62 + 86 * light), int(34 + 42 * light)))
    return Image.frombytes("RGB", (size, size), bytes(pixels))


def update_manifest(root: Path, identifiers: list[str]) -> None:
    manifest_path = root / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    entries = {entry["id"]: entry for entry in manifest["materials"]}
    ordered = []
    for identifier in identifiers:
        directory = root / identifier
        resolution = Image.open(directory / "base-color.png").size
        channels = {}
        for channel in ("base-color", "roughness", "normal-dx"):
            path = directory / f"{channel}.png"
            data = path.read_bytes()
            channels[channel] = {
                "path": f"{identifier}/{channel}.png",
                "sha256": hashlib.sha256(data).hexdigest(),
                "bytes": len(data),
            }
        entry = entries.get(identifier, {"id": identifier})
        entry["resolution"] = list(resolution)
        entry["channels"] = channels
        ordered.append(entry)
    manifest["materials"] = ordered
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: generate-character-materials.py <material-root>")
    root = Path(sys.argv[1]).resolve()

    # 角色近距離材質保留 2K；衣料與髮片採 1K，避免無效的 4K 常駐成本。
    skin_source = root / "skin"
    skin_was_promoted = Image.open(skin_source / "base-color.png").size != (2048, 2048)
    skin_base = resized(skin_source / "base-color.png", 2048, "RGB")
    skin_normal = resized(skin_source / "normal-dx.png", 2048, "RGB")
    skin_roughness = resized(skin_source / "roughness.png", 2048, "RGB")
    if skin_was_promoted:
        skin_base = tinted(ImageEnhance.Contrast(skin_base).enhance(1.06), (255, 207, 188), 0.16)
        save(skin_base, skin_source / "base-color.png")
        save(skin_normal, skin_source / "normal-dx.png")
        save(skin_roughness, skin_source / "roughness.png")

    lips = root / "lips"
    save(tinted(skin_base, (194, 72, 82), 0.52), lips / "base-color.png")
    save(skin_normal, lips / "normal-dx.png")
    save(constant_texture(2048, 92), lips / "roughness.png")

    sclera = root / "sclera"
    eye_source = root / "eye"
    eye_normal = resized(eye_source / "normal-dx.png", 2048, "RGB")
    save(tinted(resized(eye_source / "base-color.png", 2048, "RGB"), (248, 244, 238), 0.28), sclera / "base-color.png")
    save(eye_normal, sclera / "normal-dx.png")
    save(constant_texture(2048, 46), sclera / "roughness.png")

    iris = root / "iris"
    save(iris_texture(2048), iris / "base-color.png")
    save(eye_normal, iris / "normal-dx.png")
    save(constant_texture(2048, 34), iris / "roughness.png")

    for identifier in ("cloth", "hair"):
        directory = root / identifier
        base_mode = "RGBA" if identifier == "hair" else "RGB"
        if Image.open(directory / "base-color.png").size != (1024, 1024):
            save(resized(directory / "base-color.png", 1024, base_mode), directory / "base-color.png")
            save(resized(directory / "normal-dx.png", 1024, "RGB"), directory / "normal-dx.png")
            save(resized(directory / "roughness.png", 1024, "RGB"), directory / "roughness.png")
        if identifier == "hair":
            hair = Image.open(directory / "base-color.png").convert("RGBA")
            red, green, blue, alpha = hair.split()
            # 舊髮色圖為灰階；只轉換一次成暖棕色，重跑時保持位元穩定。
            if ImageChops.difference(red, green).getbbox() is None and ImageChops.difference(red, blue).getbbox() is None:
                colored = tinted(hair.convert("RGB"), (116, 58, 30), 0.68)
                colored.putalpha(alpha)
                save(colored, directory / "base-color.png")

    update_manifest(root, ["skin", "lips", "sclera", "iris", "cloth", "leather", "hair", "eye", "metal"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
