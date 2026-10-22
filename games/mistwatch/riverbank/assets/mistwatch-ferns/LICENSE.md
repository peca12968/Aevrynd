# Fern 02 — Poly Haven

Official asset: https://polyhaven.com/a/fern_02

License: CC0-1.0, https://creativecommons.org/publicdomain/zero/1.0/
Poly Haven asset license: https://polyhaven.com/license

Modeling: Rico Cilliers. Scanning: Rob Tuytel.

This directory contains the actual mesh and texture assets, not website preview renders.
Official download size/MD5 and locally recorded SHA-256 are in download-receipt.json.
The original six files remain unchanged in source/.

The four mesh variants are not LOD levels. Cooking removes display-only node
translations and preserves local pivots; the game explicitly applies a uniform
asset scale, without modifying world root placements. AEMESH uses left-handed Z
and reversed winding. Material cooking combines the official separate alpha,
preserves MASK coverage in mipmaps, converts GL normals to DX, and uses ARM.R as
ambient occlusion (an explicit integration choice not declared in the source glTF).

No Unreal Engine code or assets are included.
