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

The checked-in snapshot also covers the committed WaterOptics screen-space refraction API, directional-light PCSS contact-hardening controls, the QWEN timeout supervisor, terrain distance-material LOD, VegetationWorld3D BVH visibility with three-stage LOD, volumetric clouds and cloud shadows, directional-light shafts, preintegrated skin subsurface scattering, water screen-space reflection with IBL fallback, local box-projected reflection probes, and GPU-driven rendering through Phase 1B. The GPU-driven evidence includes compute compaction, a 72-byte VBV/IBV/root-constant/draw indirect ABI, homogeneous multi-group buckets, immutable StaticMeshSubmission3D cold/warm caching, fail-closed eligibility and invalidation, phase telemetry, coverage, identity, direct fallback, and benchmark limits. Bindless descriptor indexing, HZB occlusion, and World Partition remain outside the completed snapshot. External Info documents, Demo outputs, build outputs, captures, generated files, and the AI pack itself are never accepted as snapshot sources.

## Integration roadmap

1. Read-only RAG: reproducible evidence, citations, deterministic retrieval tests, and bounded local inference.
2. Curated learning data: accept only reviewed question/evidence/answer triples with provenance and license metadata.
3. Offline LoRA experiment: train outside the game process only after the data and evaluation gates are met; compare against the frozen RAG baseline.
4. Editor assistant: connect through a versioned localhost named-pipe IPC contract with read-only scene and asset snapshots.
5. Engine subsystem: add opt-in inference scheduling, cancellation, telemetry, model hot-unload, and strict capability permissions. Gameplay and rendering must never wait on inference.

LoRA is not justified until there are at least 2,000 reviewed, deduplicated examples, zero secret or private-data findings, complete license provenance, at least 200 held-out questions across all engine systems, retrieval accuracy of at least 90%, grounded-answer accuracy of at least 85%, and no regression greater than two percentage points on safety tests.
