# God coverage candidate margin CPU regression

The formal OID gate remains 0.0004 / 369 pixels at 1280x720. The old
320x180 posed-triangle union reported 24 samples (384 equivalent pixels),
while the observed full-resolution required-player OID was 326 pixels.
This proxy does not perform scene depth or material-alpha rejection.
The 58-pixel difference cannot be uniquely attributed to raster aliasing,
occlusion, or material coverage from the available evidence.

Candidate filtering now requires ceil(369 * 1.25) = 462 equivalent pixels,
which requires 29 coarse samples / 464 pixels. Subtracting the observed
58-pixel loss and one 16-pixel coarse quantum leaves 390 pixels. This is an
empirical selection margin, not a universal occlusion bound or GPU pass.
The final GPU gate is unchanged.

The single-direction refinement failed the stricter filter. Selection now
retains the best raster candidate from each existing direction and explores
at most eight directions, each with the existing 24 height / 8 pan steps.
Every candidate still passes Resolve/Validate and the unchanged building,
river, LOD, world visibility, collision/terrain fixed-point contracts.
No eligible candidate returns valid=false.

## Reproduce (CPU only)

```powershell
cmake -S games/gameplay_lab/native/tests/god_coverage_margin -B D:/Download/Aevrynd/b/god-coverage-margin-cpu-fresh -A x64 -DCPU_ENGINE_LIBRARY=D:/Download/Aevrynd/b/temporal-token-build-139/engine/native/Release/AevryndEngine.lib
cmake --build D:/Download/Aevrynd/b/god-coverage-margin-cpu-fresh --config Release --parallel 2
ctest --test-dir D:/Download/Aevrynd/b/god-coverage-margin-cpu-fresh -C Release --output-on-failure
```

Fresh test / production camera translation units link a read-only existing
engine archive; this is not a fresh whole-engine build. No GPU is initialized.

Result: 2/2 PASS. Production integration took 82.27 seconds across its
multiple camera/constraint cases. Spawn and running coverage both equal
0.000503472 (29 samples / 464 equivalent pixels). The integration exercises
female/male, cached authority, Validate, Resolve idempotence, hostile drifting
constraints, actual collision and terrain constraints, building/river/LOD.
The dedicated hostile smoke rejects NaN, infinity, negative and >1 coverage,
rejects 24/28 samples, accepts 29, and verifies the formal gate is unchanged.

Receipts: build-directory coverage-margin-receipt.json and
Testing/Temporary/LastTest.log. No GPU OID or visual acceptance is claimed.
The eight-direction search adds bounded CPU work; runtime startup latency
should be measured separately from cached frame rendering.
