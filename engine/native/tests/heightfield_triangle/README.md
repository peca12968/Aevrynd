# Heightfield triangle CPU regression

River query/raster disagreement was reproduced at 0.128202m and made a red
test before the engine change. SampleHeight now uses the CreateMesh a/c/b,
b/c/d diagonal, retaining clamping. River disagreement is zero after repair.

Fresh test translation units compile changed HeightField3D.cpp and link the
read-only forest-integration-143 engine archive for unchanged dependencies.
This is NOT a full-fresh-engine build.

```powershell
cmake -S engine/native/tests/heightfield_triangle -B D:/Download/Aevrynd/b/heightfield-triangle-cpu-fresh -A x64 -DCPU_ENGINE_LIBRARY=D:/Download/Aevrynd/b/forest-integration-143/engine/native/Release/AevryndEngine.lib
cmake --build D:/Download/Aevrynd/b/heightfield-triangle-cpu-fresh --config Release --parallel 2
ctest --test-dir D:/Download/Aevrynd/b/heightfield-triangle-cpu-fresh -C Release --output-on-failure
```

Seven CPU tests passed: HeightfieldTriangleCpu (10201 saddle samples and
clamping), HeightField3D, CharacterController, FootPlacement3D, TerrainClipmap3D,
WorldPartitionTerrainVegetationAdapterFake, WorldPartitionTerrainVegetationDeltaComposeFake.
No existing expected results were changed. No explicit bilinear public
contract was found in these tests. River test passed in 6.38s.

## Disclosed test orchestration incident

TerrainDistanceMaterial3D was initially misclassified as CPU from its name.
It creates D3D12Renderer at line66, generated captures, and failed phase-pixel
validation after 4.22s. It references neither HeightField3D nor SampleHeight.
The prohibited GPU execution was immediately reported and never repeated.
It is now excluded from this dedicated CMake; logs/artifacts remain evidence,
not a SampleHeight regression or GPU acceptance. Other D3D12 tests were not run.

Full fresh integration and approved visual review remain with the parent.
