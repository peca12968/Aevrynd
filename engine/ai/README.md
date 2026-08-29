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

The checked-in snapshot also covers the committed WaterOptics screen-space refraction API, directional-light PCSS contact-hardening controls, the QWEN timeout supervisor, terrain distance-material LOD, VegetationWorld3D BVH visibility with three-stage LOD, volumetric clouds and cloud shadows, directional-light shafts, preintegrated skin subsurface scattering, water screen-space reflection with IBL fallback, local box-projected reflection probes, GPU-driven rendering through current-frame HZB occlusion, and World Partition Phase 2. World Partition evidence covers asynchronous CPU preparation, typed terrain-and-vegetation payloads, `GpuUploadReceipt3D`, `PrepareCpu`/`AcquirePrepared`, physical asset-record alias leases, `ChunkedVegetationWorld3D` owner blocks, atomic ECS activation and destruction batches, epoch cancellation, CPU/GPU/inflight/per-update budgets, four activation rollback checkpoints, retiring tombstones, main-thread guards, overflow protection, deterministic negative-coordinate queries, fail-safe destruction, and A/B shared-asset retirement. Fake-source tests define deterministic lifecycle boundaries while the real D3D12 smoke and benchmark validate four-chunk renderer residency, seven-or-more uploads, rollback, retirement, and performance integration. Origin rebasing and delta persistence remain outside Phase 2.

Current-frame HZB benchmark evidence uses symmetric ABBA/BAAB quartets, normalized within-block residual controls, directional block-drift and outlier environment gates, and affinity restoration. Eight synthetic statistics cases cover stable boundaries, real regression injection, drift, isolated outliers, bursts, and invalid input; the benchmark requires three clean baselines and three injected failures.

The immutable Engine Lab acceptance Demo `v2.51.0-engine-v2.86.0-4568daa1` is formally released from source `4568daa1`. Its release evidence records 115 managed files, zero manifest mismatches, 81/81 tests, GPU validation, five captures, and an atomic latest pointer. The release manifest SHA-256 is `85dc3a35bcbe5c1bc9dac6fbeded92b7aaee17344e5fb2a48843a139b11432d9`; the Engine Lab executable SHA-256 is `8b8e0df288970927092ed7c9a4d437d852c33ca5a8d1dc856a0de3612753df94`. This publishes the v2.51 acceptance Demo, while the World Partition Phase 2 core remains an evolving capability rather than a claim of final completion. Temporal hybrid occlusion, bindless descriptor indexing, origin rebasing, and delta persistence remain pending. External Info documents, Demo outputs, build outputs, captures, generated files, and the AI pack itself are never accepted as snapshot sources.

## Integration roadmap

1. Read-only RAG: reproducible evidence, citations, deterministic retrieval tests, and bounded local inference.
2. Curated learning data: accept only reviewed question/evidence/answer triples with provenance and license metadata.
3. Offline LoRA experiment: train outside the game process only after the data and evaluation gates are met; compare against the frozen RAG baseline.
4. Editor assistant: connect through a versioned localhost named-pipe IPC contract with read-only scene and asset snapshots.
5. Engine subsystem: add opt-in inference scheduling, cancellation, telemetry, model hot-unload, and strict capability permissions. Gameplay and rendering must never wait on inference.

LoRA is not justified until there are at least 2,000 reviewed, deduplicated examples, zero secret or private-data findings, complete license provenance, at least 200 held-out questions across all engine systems, retrieval accuracy of at least 90%, grounded-answer accuracy of at least 85%, and no regression greater than two percentage points on safety tests.
