"""Measure the decoded alpha-test coverage of every BC7 DDS mip."""
import argparse
import io
import json
import struct
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument("dds", type=Path)
args = parser.parse_args()
data = args.dds.read_bytes()
if data[:4] != b"DDS " or data[84:88] != b"DX10":
    raise ValueError("Expected a DX10 DDS texture")
height, width = struct.unpack_from("<II", data, 12)
mips = struct.unpack_from("<I", data, 28)[0]
if struct.unpack_from("<I", data, 128)[0] not in (98, 99):
    raise ValueError("Expected BC7 UNORM or SRGB")
offset = 148
measurements = []
for level in range(mips):
    count = ((width + 3) // 4) * ((height + 3) // 4) * 16
    if offset + count > len(data):
        raise ValueError("Truncated mip payload")
    header = bytearray(data[:148])
    struct.pack_into("<II", header, 12, height, width)
    struct.pack_into("<I", header, 28, 1)
    image = Image.open(io.BytesIO(header + data[offset:offset + count])).convert("RGBA")
    histogram = image.getchannel("A").histogram()
    total = width * height
    measurements.append({"mip": level, "width": width, "height": height,
        "coverage_0_5": sum(histogram[128:]) / total,
        "coverage_0_25": sum(histogram[64:]) / total,
        "coverage_0_15": sum(histogram[39:]) / total})
    offset += count
    width, height = max(1, width // 2), max(1, height // 2)
print(json.dumps({"texture": str(args.dds), "decoded_mips": measurements}, indent=2))
