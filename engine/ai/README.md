# Aevrynd Engine Copilot

This directory contains the first read-only integration stage for the local QWEN model. It is a retrieval assistant, not an autonomous code writer.

## Safety boundary

- Knowledge is rebuilt only by an explicit developer command.
- Repository sources are read from one Git `HEAD` snapshot; unrelated unfinished working-tree edits and untracked tests cannot leak into the pack.
- Rebuilds fail closed when a source matches a private-key or common credential pattern.
- Queries read the checked-in knowledge pack and cannot write repository files.
- Local inference is delegated to `tools/ci/invoke-local-qwen.ps1`, which enforces file type, input size, timeout, CPU, memory, structured-output, and fallback limits.
- Model output is advisory. It cannot edit files, execute commands, approve a release, or decide code correctness.
- Every citation retains its source path, source SHA-256, source revision, engine version, and chunk identifier.
- Every query validates the strict manifest contract, knowledge SHA-256, exact JSONL-to-chunk parity, expected chunk count, safe source paths, and required exclusions before any model process can start.
- Corrupt manifests, modified chunks, unexpected chunk files, unknown questions, and unavailable inference all fail closed to Codex without launching the model.
- Knowledge remains `trainingEligible=false` until every source has an explicitly reviewed license; repository ownership alone is not treated as training permission.
- `config/knowledge-schema.json` defines the versioned chunk and manifest contracts used by deterministic validation.

## Commands

```powershell
pwsh -NoProfile -File tools/ai/build-engine-knowledge.ps1
pwsh -NoProfile -File tools/ai/query-engine-copilot.ps1 -Question "What does VisibilityBvh3D do?"
pwsh -NoProfile -File tools/ai/test-engine-copilot.ps1 -ModelCases 0
```

The deterministic retrieval baseline always covers the complete fixed evaluation set. Local generation remains policy-gated: the measured safe route is a fixed-schema, single-feature classification of at most 1 KiB and 96 output tokens. Larger grounded-answer cases fall back to Codex until a matching benchmark passes.

The current routing evidence is intentionally narrow. A 528-byte Bloom classification completed in 19.408 seconds with a 14.94 GiB peak working set at 96 tokens. A 3,218-byte, three-feature batch with 512 tokens timed out at 30 seconds, so multi-feature batching is not eligible for local dispatch.

The checked-in snapshot also covers the committed WaterOptics screen-space refraction API, directional-light PCSS contact-hardening controls, the QWEN timeout supervisor, terrain distance-material LOD, VegetationWorld3D BVH visibility with three-stage LOD, volumetric clouds and cloud shadows, directional-light shafts, preintegrated skin subsurface scattering, water screen-space reflection with IBL fallback, local box-projected reflection probes, GPU-driven rendering through current-frame HZB occlusion, and World Partition Phase 2. World Partition evidence covers asynchronous CPU preparation, typed terrain-and-vegetation payloads, `GpuUploadReceipt3D`, `PrepareCpu`/`AcquirePrepared`, physical asset-record alias leases, `ChunkedVegetationWorld3D` owner blocks, atomic ECS activation and destruction batches, epoch cancellation, CPU/GPU/inflight/per-update budgets, four activation rollback checkpoints, retiring tombstones, main-thread guards, overflow protection, deterministic negative-coordinate queries, fail-safe destruction, and A/B shared-asset retirement. Fake-source tests define deterministic lifecycle boundaries while the real D3D12 smoke and benchmark validate four-chunk renderer residency, seven-or-more uploads, rollback, retirement, and performance integration.

Large-world origin rebasing now uses `GlobalPosition3D`, with signed 64-bit chunk coordinates and a bounded local offset. Normalization validates binary64 shift values before integer conversion, including exact `+2^63`, its lower `nextafter`, exact `-2^63`, and lower overflow. The frame-boundary coordinator uses two-phase Prepare/Commit/Abort transactions across Scene/ECS, vegetation, collision, physics previous state, navigation, controller, camera, DrawList, World Partition, and reflection-probe captures. Renderer commits invalidate temporal motion, static submission generations, fog/cloud/probe-local coordinates, camera history, and pending probe generations as required. World Partition `UpdateGlobal` preserves chunk-local typed payloads, and serialization wire version 1 round-trips global positions while rejecting future versions. The Windows GUI test wrapper preserves the child process exit code, including an exact 37 verification.

Dirty Delta persistence now stores typed building, vegetation, and terrain edits per global chunk without duplicating procedural base content. The V1 codec remains readable and the next successful save writes the V2 frame contract. A flushed, atomically replaced snapshot is the only durable commit; the journal is a replaceable mirror rather than an append-only authority. Frames retain CRC32C for accidental corruption and add a SHA-256 parent/payload digest, exact chunk-key and base-revision checks, and same-generation digest-divergence rejection. Snapshot-only recovery, corrupt-mirror quarantine, three consecutive restarts, legacy migration, commit failure, stale completions, and twenty save rotations are fixed tests.

The asynchronous store reserves request, active, and completion counts and bytes before admission, with checked arithmetic, bounded backpressure, paired pop/Reset accounting, and honest Trim reporting. World Partition gates activation in `LoadingDelta`, gates final retirement in `SavingDelta`, loads canonical boundary dependencies, rejects stale epoch/request receipts, and enforces its owner-thread contract. Terrain/vegetation/building composition uses stable IDs, base revisions, chunk-local payloads, canonical west/northwest boundary ownership, atomic ECS/physics/resource rollback, and restart-consistent rendering.

Current-frame HZB benchmark evidence uses symmetric ABBA/BAAB quartets, normalized within-block residual controls, directional block-drift and outlier environment gates, and affinity restoration. Eight synthetic statistics cases cover stable boundaries, real regression injection, drift, isolated outliers, bursts, and invalid input; the benchmark requires three clean baselines and three injected failures.

The immutable Engine Lab acceptance Demo `v2.51.0-engine-v2.86.0-4568daa1` is formally released from source `4568daa1`. Its release evidence records 115 managed files, zero manifest mismatches, 81/81 tests, GPU validation, five captures, and an atomic latest pointer. The release manifest SHA-256 is `85dc3a35bcbe5c1bc9dac6fbeded92b7aaee17344e5fb2a48843a139b11432d9`; the Engine Lab executable SHA-256 is `8b8e0df288970927092ed7c9a4d437d852c33ca5a8d1dc856a0de3612753df94`. This publishes the v2.51 acceptance Demo, while the World Partition Phase 2 core remains an evolving capability rather than a claim of final completion. Temporal hybrid occlusion, bindless descriptor indexing, origin rebasing, and delta persistence remain pending. External Info documents, Demo outputs, build outputs, captures, generated files, and the AI pack itself are never accepted as snapshot sources.

The immutable Engine Lab acceptance Demo `v2.52.0-engine-v2.86.0-e1584944` is formally released from source `e1584944`. Its evidence records 217 managed files, zero manifest mismatches, 89/89 tests, seven GPU validation checks, five captures, an atomic latest pointer, and 17 retained historical directories. The release manifest SHA-256 is `0ef9370595a4caf7af9fab7c90edc78d560d4610a9badb3b8446d36842b7312d`; the Engine Lab executable SHA-256 is `519a8379edbd3df140184649c29f79079eb8b8fbcec972923522fea4baa4c527`. This publishes the v2.52 large-world origin-rebasing acceptance Demo; it does not claim that World Partition or large-world persistence is finally complete. Temporal hybrid occlusion, bindless descriptor indexing, and delta persistence remain pending. External Info documents, Demo outputs, build outputs, captures, generated files, and the AI pack itself are never accepted as snapshot sources.

The immutable Engine Lab acceptance Demo `v2.53.0-engine-v2.86.0-73f10a96` is formally released from source `73f10a96`. Its evidence records 273 managed files, zero manifest mismatches, 95/95 tests, eight GPU validation checks, six captures, an atomic latest pointer, and 18 retained historical directories. The release manifest SHA-256 is `3bbbfbc89dc04f5115bc760147990fd7573f1e5c1e426d763ffdb862ab4a5c74`; the Engine Lab executable SHA-256 is `fb3eb977bc28f91f41a55f667421f663727270a43616763db0e15592d72a434c`. The true delta capture is 320x180, 10,410 bytes, has variance 66.4745, and begins with SHA-256 `681554B5`. This publishes the v2.53 Dirty Delta acceptance slice; it does not claim final MMO persistence, server authority, schema migration tooling, temporal hybrid occlusion, or bindless indexing. External Info documents, Demo outputs, build outputs, captures, generated files, and the AI pack itself remain excluded from snapshot sources.

## Integration roadmap

1. Read-only RAG: reproducible evidence, citations, deterministic retrieval tests, and bounded local inference.
2. Curated learning data: accept only reviewed question/evidence/answer triples with provenance and license metadata.
3. Offline LoRA experiment: train outside the game process only after the data and evaluation gates are met; compare against the frozen RAG baseline.
4. Editor assistant: connect through a versioned localhost named-pipe IPC contract with read-only scene and asset snapshots.
5. Engine subsystem: add opt-in inference scheduling, cancellation, telemetry, model hot-unload, and strict capability permissions. Gameplay and rendering must never wait on inference.

LoRA is not justified until there are at least 2,000 reviewed, deduplicated examples, zero secret or private-data findings, complete license provenance, at least 200 held-out questions across all engine systems, retrieval accuracy of at least 90%, grounded-answer accuracy of at least 85%, and no regression greater than two percentage points on safety tests.
