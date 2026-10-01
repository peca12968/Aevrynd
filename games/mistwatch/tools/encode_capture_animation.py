"""Encode native engine frame captures without synthesizing or interpolating frames."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageChops

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
parser.add_argument("prefix")
parser.add_argument("output", type=Path)
parser.add_argument("--frames", type=int, default=36)
args = parser.parse_args()
if not 2 <= args.frames <= 120 or args.output.suffix.lower() != ".webp":
    raise ValueError("Require 2..120 captured frames and a .webp output")
if args.output.exists():
    raise FileExistsError(args.output)
frames = []
for index in range(args.frames):
    source = args.directory / f"{args.prefix}-{index:03d}.png"
    with Image.open(source) as image:
        frames.append(image.convert("RGB"))
if any(frame.size != frames[0].size for frame in frames):
    raise ValueError("Capture sizes differ")
changed = sum(ImageChops.difference(a, b).getbbox() is not None for a, b in zip(frames, frames[1:]))
if not changed:
    raise ValueError("All native captures are identical; cannot claim motion")
frames[0].save(args.output, format="WEBP", save_all=True, append_images=frames[1:],
               duration=100, loop=0, quality=90, method=4)
with Image.open(args.output) as result:
    if result.n_frames != args.frames or result.size != frames[0].size:
        raise ValueError("Encoded animation failed frame count or size verification")
print(json.dumps({"output": str(args.output), "native_frames": args.frames,
                  "changed_transitions": changed, "frame_duration_ms": 100,
                  "size": frames[0].size, "bytes": args.output.stat().st_size}, indent=2))
