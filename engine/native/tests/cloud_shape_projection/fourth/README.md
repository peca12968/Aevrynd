# Fourth cloud shape fixture — NO-GO

This fixture does not modify or integrate any production shader. It reuses the
extracted production noise functions and fixed third-day/dusk camera/time/seed
inputs from the parent projection test. Midpoint 16-step density integration is
an isolation oracle, not exact GPU jitter, occlusion, temporal or lighting parity.

## Formula

For advected world position `p` and frequency `f`:

    L = .65 * ValueNoise(R0(p*f)) + .35 * ValueNoise(R1(p*f*1.67 + (17,31,11)))
    S = smoothstep(threshold, threshold+.18, L)
    D = existing three-octave detail FBM(p*.0047+29.3)
    E = erosion * D * (1-S) * .65
    density = max(0,S-E) * originalHeightProfile * cloudDensity * massScale

High-frequency erosion vanishes at the core `S=1`. The original global slab is
unchanged. Shadow density directly calls the same candidate density function,
so there is no separate approximation. This is a bounded shape/erosion experiment,
not an implementation claim for the full Hillaire or Nubis renderer.
Conceptual reference: [Guerrilla Nubis](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine).

## Parameter scan and result

Initial scan: four frequencies × four thresholds × three density scales = 48,
zero jointly passing candidates. Evidence: `b/cloud-shape-fourth`.

Expanded scan: frequencies { .00012,.00018,.00022,.00028,.00036,.00044,.00050 },
thresholds { .42,.46,.50,.54,.58,.62,.66 }, scales {1,1.5,2} = **147 candidates**.
Every candidate must pass both cameras with the same parameters. No seed/time or
camera was tuned per phase. **Zero jointly passing candidates.**

Unchanged acceptance: span <=.65; coverage and mean tau >=.95 of original;
top and bottom contour ranges >=1.25 of original. A passing 16-step candidate
would also be checked at 32 steps; none reached that conditional check.

Example f=.00022, threshold=.46, scale=1:

| Metric | Day | Dusk |
|---|---:|---:|
| Span | .494792 | .718750 |
| Coverage | .182243 | .196952 |
| Mean tau | .274760 | .317668 |
| Top variation | 31 | 22 |
| Bottom variation | 23 | 14 |
| All gates | PASS | FAIL |

Both phases preserve cloud mass/coverage, but dusk still fails span and silhouette
variation. Conversely f=.00050/.46/1 passes dusk, but day span is 1.0. These are
not interchangeable phase-specific assets and neither is a candidate for integration.

Hostile checks pass: 14,700 sampled positions are finite, nonnegative and exactly
equal between main/shadow; zero density, zero thickness, outside slab, NaN/Inf
positions, negative density scale and NaN time fail closed to zero. The formula
has bounded five lattice-noise evaluations (two macro + three detail octaves),
with no added ray steps. CPU adapter compilation is not an FXC/GPU certification.

## Reproduce

    cmake -S engine/native/tests/cloud_shape_projection/fourth -B D:/Download/Aevrynd/b/cloud-shape-fourth-expanded -G "Visual Studio 17 2022" -A x64
    cmake --build D:/Download/Aevrynd/b/cloud-shape-fourth-expanded --config Release
    ctest --test-dir D:/Download/Aevrynd/b/cloud-shape-fourth-expanded -C Release -V

Fresh configure/build succeeded. CTest failed intentionally (no passing shape),
6.18 seconds wall time. `scan.csv` contains all 294 per-phase measurement rows.
SHA256: `6045E12609D9C8718A8B7954A86665F4125D618839E4A053B3C9CE2AF0A7212B`.
Formal `VolumetricAtmosphere.hlsl` unchanged SHA256:
`5FAF1184F3048121797EE423DD97C3FCE8C7C445B9638BBCA023A33F9DCB103C`.
No Git/GPU or production build was run. Existing atmosphere regressions were not
run because there is no accepted production delta. Do not promote this fixture.
