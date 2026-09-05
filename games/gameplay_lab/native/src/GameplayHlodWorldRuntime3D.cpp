#include "GameplayHlodWorldRuntime3D.h"

#include <Aevrynd/Engine.h>
#include <Aevrynd/SceneECS3D.h>
#include <Aevrynd/WorldPartition3D.h>

#include <deque>
#include <chrono>
#include <mutex>
#include <stop_token>
#include <type_traits>

namespace GameplayLab {
namespace {
static_assert(std::is_nothrow_move_assignable_v<Aevrynd::Scene3D>);
static_assert(std::is_nothrow_move_assignable_v<Aevrynd::DrawList>);

class SingleGameplayHlodChunkSource3D final
    : public Aevrynd::IWorldChunkSource3D {
public:
  explicit SingleGameplayHlodChunkSource3D(
      std::shared_ptr<const Aevrynd::TerrainVegetationChunkPayload3D> payload,
      std::uint32_t completionDelayPolls)
      : payload_(std::move(payload)), completionDelayPolls_(completionDelayPolls) {}

  bool BeginLoad(Aevrynd::WorldChunkRequest3D request,
                 std::stop_token cancellation) override {
    if (cancellation.stop_requested() || request.coordinate != Aevrynd::WorldChunkCoordinate3D{})
      return false;
    const Aevrynd::WorldChunkPayload3D payload{
        payload_, payload_->cpuBytes, payload_->conservativeGpuBytes};
    std::lock_guard lock(mutex_);
    completions_.push_back({{request, payload, true}, completionDelayPolls_});
    return true;
  }
  bool TryPopCompletion(Aevrynd::WorldChunkLoadCompletion3D &completion) override {
    std::lock_guard lock(mutex_);
    if (completions_.empty())
      return false;
    if (completions_.front().remainingPolls) {
      --completions_.front().remainingPolls;
      return false;
    }
    completion = std::move(completions_.front().completion);
    completions_.pop_front();
    return true;
  }
  void WaitIdle() override { ++waitIdleCalls_; }
  [[nodiscard]] std::uint32_t WaitIdleCalls() const noexcept {
    return waitIdleCalls_;
  }
  std::size_t TrimUnusedMemory() override {
    std::lock_guard lock(mutex_);
    const auto before = completions_.size() * sizeof(PendingCompletion);
    std::deque<PendingCompletion>(completions_).swap(completions_);
    const auto after = completions_.size() * sizeof(PendingCompletion);
    return before > after ? before - after : 0;
  }

private:
  struct PendingCompletion {
    Aevrynd::WorldChunkLoadCompletion3D completion{};
    std::uint32_t remainingPolls{};
  };
  std::shared_ptr<const Aevrynd::TerrainVegetationChunkPayload3D> payload_;
  std::uint32_t completionDelayPolls_{}, waitIdleCalls_{};
  std::mutex mutex_;
  std::deque<PendingCompletion> completions_;
};
} // 命名空間

struct GameplayHlodWorldRuntime3D::State {
  Aevrynd::Engine *engine{};
  // HLOD source/proxy 不可與 palette 共用 registry；否則正式 OnRender 同時
  // 收集一般 SceneECS 與 frozen HLOD 時，rock 會被提交兩次。
  Aevrynd::SceneECSRegistry3D registry{};
  std::shared_ptr<Aevrynd::TerrainVegetationChunkPayload3D> payload{};
  std::unique_ptr<SingleGameplayHlodChunkSource3D> source{};
  Aevrynd::ChunkedVegetationWorld3D vegetation{};
  std::unique_ptr<Aevrynd::TerrainVegetationWorldChunkActivationTarget3D> target{};
  std::unique_ptr<Aevrynd::WorldPartition3D> partition{};
  GameplayHlodWorldRuntimeReceipt3D receipt{};
  GameplayHlodWorldRuntimeOptions3D options{};
  Aevrynd::StableObjectId3D worldId{};
  bool readyObserved{}, fatal{};
};

namespace {
void AssignPartitionLifecycle(
    const Aevrynd::WorldPartitionStats3D &stats,
    GameplayHlodWorldRuntimeReceipt3D &receipt) noexcept {
  receipt.partition = stats;
  receipt.partitionRequests = stats.totalQueued;
  receipt.partitionDispatches = stats.totalStarted;
  receipt.partitionCompletions = stats.totalCompleted;
  receipt.partitionPending = stats.queued || stats.loading ||
      stats.loadingDelta || stats.cpuReady || stats.activating ||
      stats.savingDelta || stats.retiring || stats.inflightLoads;
}

void ResetAttempt(GameplayHlodWorldRuntimeReceipt3D &receipt,
                  std::uint64_t frameId) noexcept {
  receipt.attemptFrame = frameId;
  receipt.frozenFrame = 0;
  receipt.readiness = GameplayHlodWorldReadiness3D::RetryableNotReady;
  receipt.terminalReason = GameplayHlodWorldTerminalReason3D::FirstFramePending;
  receipt.worldPartitionResident = false;
  receipt.selected = false;
  receipt.frozen = false;
  receipt.collected = false;
  receipt.attemptRetryCounted = false;
  receipt.frameInvariantPassed = false;
  receipt.identityInvariantPassed = false;
  receipt.dataInvariantPassed = false;
  receipt.lifecycleInvariantPassed = false;
  receipt.stagingCopyNanoseconds = 0;
  receipt.stagingCopyBytesEstimate = 0;
  receipt.transactionalStagingWip = false;
  receipt.frame = {};
  receipt.collection = {};
  receipt.integration = {};
}
} // namespace

GameplayHlodWorldRuntime3D::GameplayHlodWorldRuntime3D() {
  unavailableReceipt_.readiness = GameplayHlodWorldReadiness3D::Fatal;
  unavailableReceipt_.terminalReason =
      GameplayHlodWorldTerminalReason3D::RuntimeUnavailable;
}
GameplayHlodWorldRuntime3D::~GameplayHlodWorldRuntime3D() { Shutdown(); }

bool GameplayHlodWorldRuntime3D::Initialize(
    Aevrynd::Engine &engine, ShowcaseRenderAssetBindings3D &assets,
    std::span<const ShowcaseSceneDescriptor3D> descriptors,
    Aevrynd::StableObjectId3D worldId,
    GameplayHlodWorldRuntimeOptions3D options) {
  if (state_ || !assets.ProductionActive())
    return false;
  auto state = std::make_unique<State>();
  state->engine = &engine;
  state->options = options;
  state->receipt.ownership.owner =
      GameplaySceneEcsSubmissionOwner3D::HlodWorldRuntime;
  state->receipt.ownership.independentRegistry = true;
  state->worldId = worldId;
  if (!assets.BuildProductionHlodChunk3D(descriptors, worldId, {},
                                          state->payload,
                                          state->receipt.build))
    return false;
  const auto manifests = state->payload->hlodManifest
      ? state->payload->hlodManifest->Clusters()
      : std::span<const Aevrynd::TerrainVegetationHlodCluster3D>{};
  if (manifests.empty() || !manifests.front().proxy.prepared.levelCount ||
      !manifests.front().proxy.prepared.cpuMeshes[0].Valid())
    return false;
  auto *library = assets.ProductionMeshLibrary3D();
  if (!library)
    return false;
  state->source = std::make_unique<SingleGameplayHlodChunkSource3D>(
      state->payload, options.sourceCompletionDelayPolls);
  Aevrynd::TerrainVegetationChunkSettings3D targetSettings{};
  targetSettings.chunkSize = 512.f;
  targetSettings.terrainCells = 1;
  targetSettings.vegetationPerChunk = 0;
  // Adapter 會建立共用植被資源；使用同一份真實 cooked proxy mesh，避免測試幾何替代。
  targetSettings.vegetationLods.fill(
      manifests.front().proxy.prepared.cpuMeshes[0]);
  targetSettings.buildingMesh = {};
  targetSettings.worldId = worldId;
  state->target =
      std::make_unique<Aevrynd::TerrainVegetationWorldChunkActivationTarget3D>(
          engine, state->registry, state->vegetation, library, nullptr,
          targetSettings);
  Aevrynd::WorldPartitionSettings3D partitionSettings{};
  partitionSettings.spatial = {512.f, 0, 0, 1, 1, 0x4a17u};
  partitionSettings.workerCount = 1;
  partitionSettings.maximumInflightLoads = 1;
  partitionSettings.maximumCpuReadyBytes = 256u * 1024u * 1024u;
  partitionSettings.maximumGpuResidentBytes = 256u * 1024u * 1024u;
  partitionSettings.maximumPreparationBytesPerUpdate = 256u * 1024u * 1024u;
  partitionSettings.maximumActivationBytesPerUpdate = 256u * 1024u * 1024u;
  state->partition = std::make_unique<Aevrynd::WorldPartition3D>(
      *state->source, *state->target, partitionSettings);
  for (std::uint32_t update = 0; update < options.prewarmUpdates; ++update)
    state->partition->Update({});
  AssignPartitionLifecycle(state->partition->Stats(), state->receipt);
  state->receipt.worldPartitionResident = state->receipt.partition.resident == 1;
  state->receipt.lifecycleInvariantPassed = state->receipt.LifecycleConsistent();
  state_ = std::move(state);
  return true;
}

GameplayHlodWorldReadiness3D GameplayHlodWorldRuntime3D::CollectFrame(
    std::uint64_t frameId, const Aevrynd::Camera3D &camera,
    Aevrynd::Vec2 viewport, Aevrynd::Scene3D &scene,
    Aevrynd::DrawList &draw) {
  if (!state_)
    return GameplayHlodWorldReadiness3D::Fatal;
  if (state_->fatal)
    return GameplayHlodWorldReadiness3D::Fatal;
  auto next = state_->receipt;
  ResetAttempt(next, frameId);
  auto &ownership = next.ownership;
  const auto contract = EvaluateGameplayHlodWorldContract3D(
      {frameId, ownership.submittedFrame,
        GameplayEnvironmentIdentityContract3D(state_->worldId),
       next.build.Complete() && next.build.hlodDataHash &&
           next.build.hlodManifestHash});
  const auto finishFatal = [&](GameplayHlodWorldTerminalReason3D reason) {
    next.readiness = GameplayHlodWorldReadiness3D::Fatal;
    next.terminalReason = reason;
    next.sourceWaitIdleCalls = state_->source->WaitIdleCalls();
    state_->receipt = next;
    state_->fatal = true;
    return next.readiness;
  };
  if (contract.readiness == GameplayHlodWorldReadiness3D::Fatal) {
    ++ownership.rejectedDuplicateFrames;
    return finishFatal(contract.reason);
  }
  ++ownership.frameRequests;
  state_->partition->Update({});
  AssignPartitionLifecycle(state_->partition->Stats(), next);
  next.worldPartitionResident = next.partition.resident == 1;
  next.lifecycleInvariantPassed = next.LifecycleConsistent();
  if (!next.lifecycleInvariantPassed)
    return finishFatal(GameplayHlodWorldTerminalReason3D::DataInvariantViolation);
  if (!next.worldPartitionResident) {
    if (state_->readyObserved) {
      return finishFatal(GameplayHlodWorldTerminalReason3D::DataInvariantViolation);
    }
    ++next.firstFrameRetryCount;
    next.firstFrameRetryObserved = true;
    next.attemptRetryCounted = true;
    const auto retry = EvaluateGameplayHlodWorldRetry3D(
        next.firstFrameRetryCount, next.firstFrameRetryLimit);
    next.readiness = retry.readiness;
    next.terminalReason = retry.reason;
    next.lastRetryReason = retry.reason;
    next.sourceWaitIdleCalls = state_->source->WaitIdleCalls();
    state_->receipt = next;
    state_->fatal = retry.readiness == GameplayHlodWorldReadiness3D::Fatal;
    return next.readiness;
  }
  state_->readyObserved = true;
  if (state_->options.failureFrame == frameId &&
      state_->options.failureInjection ==
          GameplayHlodWorldFailureInjection3D::IdentityInvariant)
    return finishFatal(
        GameplayHlodWorldTerminalReason3D::IdentityInvariantViolation);
  if (state_->options.failureFrame == frameId &&
      state_->options.failureInjection ==
          GameplayHlodWorldFailureInjection3D::DataInvariant)
    return finishFatal(GameplayHlodWorldTerminalReason3D::DataInvariantViolation);
  const auto copyStart = std::chrono::steady_clock::now();
  Aevrynd::Scene3D stagedScene;
  Aevrynd::DrawList stagedDraw;
  try {
    stagedScene = scene;
    stagedDraw = draw;
  } catch (...) {
    return finishFatal(GameplayHlodWorldTerminalReason3D::DataInvariantViolation);
  }
  next.stagingCopyNanoseconds = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - copyStart).count());
  next.stagingCopyBytesEstimate =
      static_cast<std::uint64_t>(scene.MeshCount()) * sizeof(Aevrynd::MeshObject3D) +
      static_cast<std::uint64_t>(draw.meshInstances.size()) *
          sizeof(Aevrynd::MeshInstance3D) +
      static_cast<std::uint64_t>(draw.worldVertices.size()) *
          sizeof(Aevrynd::Vertex3D) +
      static_cast<std::uint64_t>(draw.worldIndices.size()) * sizeof(std::uint32_t);
  next.transactionalStagingWip = true;
  Aevrynd::HlodRuntimeSelectionSettings3D selection{};
  selection.quality.maximumScreenError = .1f;
  selection.maximumVisibilityTransitionsPerFrame = state_->receipt.build.clusters;
  selection.cameraTeleportDistance = 128.f;
  Aevrynd::HlodSelectionView3D view{camera, viewport, {}, 512.f, false, false};
  next.selected = state_->target->SelectHlodFrame3D(frameId, view, selection);
  if (!next.selected)
    return finishFatal(GameplayHlodWorldTerminalReason3D::DataInvariantViolation);
  const auto token = state_->target->FreezeHlodFrame3D(frameId);
  next.frozen = token.has_value();
  if (!token)
    return finishFatal(GameplayHlodWorldTerminalReason3D::FrameInvariantViolation);
  const auto collection =
      state_->target->CollectFrozenHlodFrame3D(*token, stagedScene, stagedDraw);
  next.collected = collection.has_value();
  if (!collection)
    return finishFatal(GameplayHlodWorldTerminalReason3D::DataInvariantViolation);
  next.collection = *collection;
  next.frozenFrame = token->frameId;
  ++ownership.acceptedSubmissions;
  ownership.submittedFrame = token->frameId;
  next.integration = state_->target->HlodStats();
  next.frame = state_->target->HlodRuntimeStats();
  next.aggregateProxyBuilt = next.build.aggregateProxyBuilt;
  next.gameplayLabOnRenderConnected = false;
  next.allDescriptorKindsCovered = false;
  next.frameInvariantPassed = next.frozenFrame == frameId &&
      next.frame.frameId == frameId && next.frame.frozen;
  next.identityInvariantPassed = ownership.Complete();
  AssignPartitionLifecycle(state_->partition->Stats(), next);
  next.lifecycleInvariantPassed = next.ReadyLifecycleComplete();
  next.dataInvariantPassed = next.build.Complete() &&
      next.worldPartitionResident && next.selected && next.frozen &&
      next.collected && next.integration.attachedClusters == next.build.clusters &&
      next.collection.submittedRenderables != 0;
  if (state_->options.failureFrame == frameId &&
      state_->options.failureInjection ==
          GameplayHlodWorldFailureInjection3D::LateDataInvariant)
    next.dataInvariantPassed = false;
  next.readiness = GameplayHlodWorldReadiness3D::Ready;
  next.terminalReason = GameplayHlodWorldTerminalReason3D::None;
  if (!next.SeamComplete()) {
    const auto reason = !next.frameInvariantPassed
        ? GameplayHlodWorldTerminalReason3D::FrameInvariantViolation
        : !next.identityInvariantPassed
            ? GameplayHlodWorldTerminalReason3D::IdentityInvariantViolation
            : GameplayHlodWorldTerminalReason3D::DataInvariantViolation;
    return finishFatal(reason);
  }
  scene = std::move(stagedScene);
  draw = std::move(stagedDraw);
  ++next.successfulFrames;
  next.sourceWaitIdleCalls = state_->source->WaitIdleCalls();
  state_->receipt = next;
  return next.readiness;
}

void GameplayHlodWorldRuntime3D::Shutdown() {
  if (!state_)
    return;
  if (state_->partition) {
    state_->partition->Reset();
    state_->partition->WaitIdle();
  }
  state_->partition.reset();
  state_->target.reset();
  state_->source.reset();
  state_->payload.reset();
  state_.reset();
}

bool GameplayHlodWorldRuntime3D::Initialized() const noexcept {
  return state_ && state_->partition && state_->target;
}

GameplayHlodWorldRuntimeReceipt3D
GameplayHlodWorldRuntime3D::Receipt() const noexcept {
  return state_ ? state_->receipt : unavailableReceipt_;
}

} // GameplayLab 命名空間
