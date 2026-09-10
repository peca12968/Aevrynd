# God camera frozen-pose regression

First divergence: UpdateCamera selected femalePose_/malePose_ directly,
while OnRender selected the frozen snapshot during capture staging/warmup.
Capture freezes a fixed-simulation pose and calls UpdateCamera again, but
later live animation updates could overwrite camera subject evidence with
a different pose. The existing render equality guard correctly rejected it.

UpdateCamera now uses the same GameplayUseFrozenPose3D and
SelectGameplayPresentationPoses3D calls as OnRender before skinning the
camera proxy and hashing the subject. No gate or render guard changed.

The standalone CPU test extracts the actual production authority selector
from GameplayLabGame.cpp at CMake configure time, then compiles it into a
test with controlled pose snapshots. It covers all 32 freeze-state flag
combinations, both sexes, and 65 changing live ticks: 4160 comparisons.
The old selector failed at flags=3, male, tick=0; fixed code passes all.
There are 1950 cases where old live selection differs from render selection.
Synthetic pose hashes test snapshot ownership, not full mesh/camera rendering.

```powershell
cmake -S games/gameplay_lab/native/tests/god_frozen_pose -B D:/Download/Aevrynd/b/god-frozen-pose-cpu-fresh -A x64
cmake --build D:/Download/Aevrynd/b/god-frozen-pose-cpu-fresh --config Release --parallel 2
ctest --test-dir D:/Download/Aevrynd/b/god-frozen-pose-cpu-fresh -C Release --output-on-failure
```

Result: 1/1 PASS, 0.04 seconds. No engine archive/GPU required. Full Normal
CaptureSmoke integration remains the parent agent's verification gate.

## Follow-up after fresh143

The full capture still rejected the pose guard, so the selector fix alone is
not accepted as the complete root-cause fix. Added 910 ordered render reads
across prepared/frozen/warming/paired/readback/unfreeze transitions, with
simulation publication held fixed as in the capture waiting path; these pass.
RequestCapturePhase itself does not mutate the frozen-selection flags.

Mismatch now writes god-pose-authority-mismatch.json inside captureDirectory,
including receipt/live/frozen hashes, camera hashes, layout validity, tick,
capture state flags, and frozen selection before/after RequestCapturePhase.
The original throw remains unconditional on mismatch; no render-time camera
or animation evaluation is introduced. The prelude invalid-layout fallback
also exits before normal authority publication, which is a separate candidate
cause awaiting the runtime diagnostic. Do not treat the current CPU pass as
full Normal acceptance.

## Confirmed prelude phase fix

Fresh143 runtime diagnostic confirmed tick25 / captureIndex0, invalid receipt
and layout, subjectHash=0, all formal flags false, active pose equal to live
female pose. This was the prelude fallback, not a frozen-selection change.

The local production phase predicate permits invalid authority only outside
active route, warmup, paired freeze, ready-to-stage and pose freeze. Any one
of those states rejects invalid authority. A valid receipt always requires
a valid layout and a nonzero exact subject/active hash match. Diagnostic JSON
and the original throw remain; camera selection and formal coverage gates
are unchanged. No render-time camera/animation evaluation was added.

The CPU test compiles this production predicate directly and checks all
384 combinations of five phase flags, receipt/layout validity and zero,
matching, mismatching hash. Result remains 1/1 PASS (0.04 seconds), alongside
the existing 4160 pose and 910 transition checks. Full GPU integration pending.
