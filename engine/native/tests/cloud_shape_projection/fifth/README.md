# Fifth cloud shape candidate: NO-GO

Only this fixture and `b/cloud-shape-fifth-scan` were written. Production shader
SHA256 remains `5FAF1184F3048121797EE423DD97C3FCE8C7C445B9638BBCA023A33F9DCB103C`.
No GPU, Git or production build was run.

## Formula and bounded scan

The actual candidate is in `Candidate.hlsli`, compiled through the same CPU HLSL
arithmetic adapter as prior fixtures. Production lattice noise/FBM is extracted
read-only during configure. Fixed camera bits, preset times 46800/68400 and seeds
are unchanged. There is no phase-dependent parameter selection.

For advected coordinate `q=p*frequency`, two low-frequency noise samples generate
XZ domain displacement `warp*(noise-.5)`. Two coverage samples at warped scales
1 and 1.67 produce `cellMix=.65*cell+.35*secondary`.
`shape=smoothstep(threshold,threshold+.18,cellMix)`.
Bottom/top profile widths are `.10+asymmetry*(1-cell)` and
`.16+asymmetry*secondary`, respectively. Detail erosion is
`cloudErosion*detail*(1-shape)*(.35+.65*h)`: core protected, upper edge more eroded.
Final density is nonnegative remaining shape times this profile, cloud density,
and the scanned mass scale. No slab displacement or thickness scaling is used.
Shadow density calls exactly the same function.

160 combinations:

- frequency: .00018, .00022, .00028, .00036, .00050
- threshold: .40, .44, .48, .52
- mass scale: 1, 1.5
- warp: .4, .8
- asymmetry: .15, .30

All **160 fail the joint day/dusk contract**. Gate remains span<=65%,
coverage/mean tau>=95% baseline, both contour variations>=125% baseline.
No passing candidate reached the additional 32-step convergence check.

## Representative sensitivity, not a selected asset

Frequency .00022, threshold .44, mass scale 1:

| Warp / asymmetry | Day span | Dusk span | Day coverage / mean tau | Dusk coverage / mean tau |
|---|---:|---:|---|---|
| .4 / .15 | .515625 | .750000 | .186391 / .310807 | .200666 / .350040 |
| .4 / .30 | .515625 | .734375 | .179929 / .285167 | .193769 / .312802 |
| .8 / .15 | .494792 | .760417 | .166860 / .260474 | .195747 / .324392 |
| .8 / .30 | .494792 | .744792 | .159481 / .238716 | .188223 / .290188 |

The .4/.30 option preserves mass and coverage at both times but dusk remains
73.44% wide with top/bottom range 23/14 rather than required >=26.25/16.25.
Increasing warp improves day but not dusk; stronger asymmetric erosion reduces
coverage and eventually violates the mean-tau constraint. This is not a green fix.

## Hostile and build evidence

16,000 sampled positions across the scan pass finite/nonnegative density and
exact main/shadow equality. Zero thickness/density, outside slab, NaN/Inf position,
negative mass scale and NaN time return zero. Runtime cost is seven lattice-noise
samples per density evaluation (two warp, two coverage, three FBM), unchanged
16 ray steps. This cost is counted analytically, not measured on GPU.

    cmake -S engine/native/tests/cloud_shape_projection/fifth -B D:/Download/Aevrynd/b/cloud-shape-fifth-scan -G "Visual Studio 17 2022" -A x64
    cmake --build D:/Download/Aevrynd/b/cloud-shape-fifth-scan --config Release
    ctest --test-dir D:/Download/Aevrynd/b/cloud-shape-fifth-scan -C Release -V

Fresh compile succeeded. Test result RED, 8.46 seconds. `scan.csv` has 320 phase
rows and SHA256 `86477A09BFE852F1D9EAB34EF8075CD4BDB48228DD57AEFE217C2150B70D636F`.
The density oracle uses midpoint sampling, not GPU jitter/temporal/occlusion.
No production candidate or full atmosphere pass is claimed.
