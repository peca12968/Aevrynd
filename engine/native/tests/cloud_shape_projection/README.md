# Cloud shape projection: RED, candidate rejected

This is a focused CPU density projection, not GPU capture parity or a visual GO.
The CMake target extracts actual production HLSL noise/density functions. The adapter
only implements HLSL arithmetic/intrinsics. It uses capture cameraRawBits from
`b/reference-adoption-water-20260908/bin/Release/gameplay-captures-normal/third-{day,dusk}.compare-v1.json`,
the scene preset times 46800/68400, seeds 0x9e3779b9/0x243f6a88, and 16 integration samples.
It excludes geometry occlusion, lighting, temporal history and compositing; midpoint
sampling replaces GPU BlueR2 jitter. Thus it isolates density morphology, not exact pixels.

At alpha > .15, original day/dusk maximum contiguous horizontal row spans are
83.3333% / 89.0625%. Initial >90% band criterion did NOT reproduce a failure.
The subsequently explicit >80% band-existence definition does reproduce it;
this is a new morphology criterion, not a change to an existing engine gate.

| Metric | Original day | Original dusk | Rejected envelope day | Rejected envelope dusk |
|---|---:|---:|---:|---:|
| Maximum row span | .833333 | .890625 | .833333 | .833333 |
| Top range, pixels | 11 | 21 | 11 | 23 |
| Bottom range, pixels | 11 | 13 | 11 | 12 |
| Coverage | .164159 | .189718 | .151476 | .177566 |
| Mean optical depth | .252800 | .284957 | .218761 | .246331 |
| P95 optical depth | 2.01139 | 2.14544 | 1.79886 | 1.91987 |
| Maximum optical depth | 4.35468 | 4.49372 | 4.24172 | 4.36445 |

Candidate used one additional continuous 3D noise to vary lower/upper bounds,
with the same helper in shadow density. It failed: day row span unchanged and mean
optical depth dropped about 13.5%, exceeding the 5% preservation requirement.
Its production changes were withdrawn; the candidate extracted source and executable
remain under `b/cloud-shape-candidate` as rejected evidence. Original evidence is
under `b/cloud-shape-red`. Do not rebuild those evidence directories.

Final acceptance assertions require BOTH cameras: row span <=65%, coverage and mean
optical depth >=95% of original, top and bottom contour ranges >=125% of original.
Current production remains RED intentionally. No successful production fix is claimed.

Reproduce in a new build directory:

    cmake -S engine/native/tests/cloud_shape_projection -B <fresh-build> -G "Visual Studio 17 2022" -A x64
    cmake --build <fresh-build> --config Release
    ctest --test-dir <fresh-build> -C Release -V

Next work must redistribute cloud body support rather than merely contracting the
slab envelope. Do not reduce total cloud mass or alter lighting to manufacture green.

## Second round: local mass compensation, also rejected

Both candidates used the same local profile helper for visible and shadow density,
and divided density by the local vertical thickness to compensate its integral.
No global slab/preset/lighting changes were made.

| Metric | Second A day/dusk | Second B day/dusk |
|---|---|---|
| Maximum row span | .838542 / .843750 | .838542 / .609375 |
| Top range | 10 / 21 | 18 / 26 |
| Bottom range | 12 / 12 | 12 / 16 |
| Coverage | .137490 / .163580 | .120756 / .152054 |
| Mean optical depth | .263298 / .279005 | .258550 / .281138 |
| P95 optical depth | 2.25515 / 2.33421 | 2.10090 / 2.19019 |
| Maximum optical depth | 5.32826 / 4.91807 | 7.93806 / 6.30773 |

A uses displacement `(layout-.5)*.65`, thickness .62.
B uses an offset low-frequency layout sample at 2.7 scale, smoothly distributed
center displacement in [-.275,.275], thickness .45. Its support stays within the
original slab; the same profile is used for shadow density. This improves dusk
span but fails day span, coverage, and lower silhouette range. Compensation
preserves mean optical depth while concentrating it into fewer covered pixels.
Thus the proposal does not meet the joint morphology/coverage contract.

Reproducible extracted source, executables and CTest output are preserved in
`b/cloud-shape-second` and `b/cloud-shape-second-b`. Both fresh builds succeeded,
both CTests failed. Both production candidates were withdrawn. No shader delta
from these attempts is left for integration; no GPU test was run. Existing
atmosphere regressions were not run because there is no accepted shader candidate.

### Variant C: pure translation, rejected

`b/cloud-shape-second-c-full` translates the whole density field (base/detail noise
and height profile) by `(CloudLayout(advected)-.5)*.30*cloudThickness`, bounded
to +/-15% of slab thickness. No amplitude compensation or thickness reduction.
The shadow function has the same translation. The field is zero-centered in its
noise-value range, not proven zero-mean over this particular visible weather mask.
The global march slab is unchanged, so translated tail clipping remains a limitation.

Day/dusk results: span .833333/.838542; top range 11/21; bottom range 10/13;
coverage .166811/.185812; mean tau .252446/.280713; P95 2.00908/2.16259;
maximum tau 4.28121/4.41183. Mass/coverage retention passes, morphology fails.
An intermediate profile-only translation is retained in `b/cloud-shape-second-c`
but is not the complete translated density function and is not the final C result.
The full C fresh compile passed, CTest failed, and shader changes were withdrawn.
Per coordination, no further noise variants were attempted and no GPU was run.
