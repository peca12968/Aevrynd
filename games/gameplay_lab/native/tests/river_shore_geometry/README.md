# River shoreline CPU geometry slice

## Final CPU status after expanded engine ownership

The block described below was subsequently resolved by changing only
HeightField3D.cpp::SampleHeight to CreateMesh triangle interpolation.
The disagreement gate was red at 0.128202m and green at zero. All seven
selected CPU regressions passed; river gates remain green. See
engine/native/tests/heightfield_triangle/README.md for commands and the
disclosed, non-repeated TerrainDistanceMaterial classification incident.
Candidate is CPU-verified, not GPU/visual accepted.

## Local refinement candidate — NOT READY

The 1m collision-storage / local 1m display / far 4m display candidate is
implemented but must not be integrated as a completed collision-consistent
slice. CPU raster/topology gates pass; a supplemental measurement found
HeightField3D::SampleHeight uses bilinear interpolation and differs from the
actual raster triangle height by up to 0.128202m at water vertices. Changing
that engine-wide query is outside this task's ownership. Parent decision is
required before further changes or reverting this candidate.

Red build: `D:/Download/Aevrynd/b/river-refinement-cpu-red`, failed the new
1m refinement requirement against the original 4m production carve.
Candidate build: `D:/Download/Aevrynd/b/river-refinement-cpu-green`, 1/1 raster
CPU test PASS, 5.84s. This pass does NOT certify bilinear collision agreement.

Candidate receipts: `new-shore-receipt.json`, `adaptive-terrain-receipt.json`,
and `height-query-difference.json` in the candidate build directory.
Terrain display: 39499 vertices / 78484 triangles, not 524288 full fine-grid
triangles; interior boundary and nonmanifold edges zero, area 262144m2.
Water: 5140 vertices / 27330 indices, one component, overlap/nonmanifold zero,
boundary depth maximum 0.0000157058m, safe-lane drift 0.000329063m.
No GPU test was run. No CMake file was changed; explicitly reconfigure to
refresh the extracted production helper after edits.

This smoke extracts the production carve and water mesh functions directly from
`GameplayLabGame.cpp` at CMake configure time. Reconfigure after changing them.
It uses the actual `HeightField3D` generator and its raster triangle diagonal,
not a bilinear-height proxy. No renderer or GPU is created.

```powershell
cmake -S games/gameplay_lab/native/tests/river_shore_geometry -B D:/Download/Aevrynd/b/river-shore-geometry-cpu-fresh -A x64
cmake --build D:/Download/Aevrynd/b/river-shore-geometry-cpu-fresh --config Release --parallel 2
ctest --test-dir D:/Download/Aevrynd/b/river-shore-geometry-cpu-fresh -C Release --output-on-failure
```

The build directory receives `old-boundary-failure.json` and
`new-shore-receipt.json`. The old formula is retained explicitly as a regression
baseline: all 130 original bank samples fail a 1cm boundary-depth tolerance,
with 1.1311–5.78768m depth.

The new candidate has 538 shared vertices, 2358 indices, one connected component,
288 shoreline vertices and edges. Maximum boundary-depth error is 8.73208e-6m;
minimum sampled water depth is -9.80496e-6m (float roundoff, within 1mm tolerance).
There are zero nonmanifold edges, zero boundary degree violations, zero positive
triangle-overlap pairs above 1e-4 square meters, and zero UV/normal violations.
The original spawn-to-cottage centerline differs by at most 0.000598837m.

The carve now has a local dry bank band and closed longitudinal ends. This is a
real terrain-height change, not a color, fog, shader, or alpha workaround. It can
raise local natural depressions; its appearance still requires native review.
Collision descriptors, solid/non-solid classification, water submission identity,
forest density and shaders are untouched. This does not prove full movement or
GPU visual acceptance. Legacy environment helper counters describing a 64-slice
130-vertex strip are outside this slice and are not evidence for the new mesh.
