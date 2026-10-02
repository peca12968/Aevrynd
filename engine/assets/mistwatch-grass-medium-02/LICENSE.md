# Grass Medium 02

Original asset by Rico Cilliers / Poly Haven, CC0-1.0.
Source: https://polyhaven.com/a/grass_medium_02
License: https://polyhaven.com/license

Original glTF, binary, three glTF JPEG textures and separate alpha PNG are preserved byte-for-byte in `source/`. MD5, SHA-256, sizes and official URLs are recorded in `download-receipt.json`.

Integration removes only display-node translations, retains local geometry/UV/topology, and converts coordinate handedness through the existing converter. Five variants are plant types, not distance LODs. Runtime uniformly scales each type to at least 1.02338 m maximum local Y and 0.584 m horizontal radius, retaining its local pivot and aspect ratio; a narrow type becomes slightly taller rather than losing footprint. Original placement/scales and the 35 m selection rule remain unchanged. Runtime authors inverse-height wind weights without replacing packed XYZ normals.

Original glTF declares BLEND. The runtime deliberately uses the official separate coverage mask with a 0.5 alpha test, RGB dilation and coverage-preserving mip generation; this is an integration policy, not an assertion that the original asset used MASK. GL normals are converted once to DirectX. ARM.R supplies ambient occlusion in addition to original roughness/metal channels. BC7 sRGB base, BC7 linear ARM and BC5 linear normal DDS files contain complete 12-level mip chains. No website example render is redistributed or presented as engine evidence.
