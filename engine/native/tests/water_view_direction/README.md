# Orthographic water view direction

Production change is limited to World.hlsl. Water uses a constant direction
opposite the depth row of an orthographic view-projection matrix. Perspective
water retains normalize(cameraPosition-worldPosition). Non-water materials
retain their original expression. Existing matrices suffice; no renderer
constant layout, reflection switch, material or gameplay source changed.

The test extracts the actual HLSL helper and translates only scalar/vector
type and intrinsic spellings for C++ execution against actual Camera matrices.
Old perspective-only helper fails orthographic invariance (maximum error
1.45675). Fixed helper has zero error across 1681 water-plane points;
perspective directions still differ by 1.11585 and match the old formula.
Existing WaterOptics3DSmoke also passes. FXC successfully compiles production
WorldPixelMain ps_5_0. No GPU rendering was performed.

```powershell
cmake -S engine/native/tests/water_view_direction -B D:/Download/Aevrynd/b/water-view-direction-cpu-fresh -A x64
cmake --build D:/Download/Aevrynd/b/water-view-direction-cpu-fresh --config Release --parallel 2
ctest --test-dir D:/Download/Aevrynd/b/water-view-direction-cpu-fresh -C Release --output-on-failure
```

Final result: build exit0, 2/2 PASS. This proves camera semantics, not removal
of the observed triangle color bands. Parent-owned same-camera GPU validation
is still required. No existing upper-level CMake was modified.

## Diagnostic environment follow-up

The diagnostic-only renderer entry now accepts
`AEVRYND_WATER_AERIAL_DIAGNOSTIC=1..9/A..D` only in builds configured with the
independent, default-OFF `AEVRYND_WATER_COMPONENT_DIAGNOSTICS=ON`.
Unset selects production; other nonempty values fail before GPU initialization.
Option-OFF builds ignore this variable and retain their original defaults.
The broad internal hooks must remain OFF for Gameplay capture: the existing
TakeProductionReadbackBundle3D guard intentionally returns empty in hooks-ON
builds. That guard has not been changed. Water-only diagnostic images remain
diagnostic evidence, not production visual acceptance.
WaterEnvironmentCpu extracts the actual parser and checks unset, all twenty
legal values, and hostile zero/out-of-range/signed/padded/multi-digit values.
It also verifies disabled-option semantics ignore all values.
Modes 6..9 isolate absorbed transmission, Fresnel reflection, RGB transmission
and foam blend. Their temporaries are preprocessor-excluded from production.
Focused configure/build/test now passes 3/3; this is not a full GPU gate.
Controlled capture evidence is isolated under `b/water-components-146`.
The initial mode-2-physical and mode-3 images were produced by a hooks-OFF
executable and are production controls, not component-isolation evidence.
