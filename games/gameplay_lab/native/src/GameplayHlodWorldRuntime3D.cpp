#include "GameplayHlodWorldRuntime3D.h"

#include <Aevrynd/Engine.h>
#include <Aevrynd/WorldPartition3D.h>

#include <deque>
#include <mutex>
#include <stop_token>

namespace GameplayLab {
namespace {
class SingleGameplayHlodChunkSource3D final
    : public Aevrynd::IWorldChunkSource3D {
public:
  explicit SingleGameplayHlodChunkSource3D(
      std::shared_ptr<const Aevrynd::TerrainVegetationChunkPayload3D> payload)
      : payload_(std::move(payload)) {}

  bool BeginLoad(Aevrynd::WorldChunkRequest3D request,
                 std::stop_token cancellation) override {
    if (cancellation.stop_requested() || request.coordinate != Aevrynd::WorldChunkCoordinate3D{})
      return false;
    const Aevrynd::WorldChunkPayload3D payload{
        payload_, payload_->cpuBytes, payload_->conservativeGpuBytes};
    std::lock_guard lock(mutex_);
    completions_.push_back({request, payload, true});
    return true;
  }
  bool TryPopCompletion(Aevrynd::WorldChunkLoadCompletion3D &completion) override {
    std::lock_guard lock(mutex_);
    if (completions_.empty())
      return false;
    completion = std::move(completions_.front());
    completions_.pop_front();
    return true;
  }
  void WaitIdle() override {}
  std::size_t TrimUnusedMemory() override {
    std::lock_guard lock(mutex_);
    const auto before = completions_.size() * sizeof(Aevrynd::WorldChunkLoadCompletion3D);
    std::deque<Aevrynd::WorldChunkLoadCompletion3D>(completions_).swap(completions_);
    const auto after = completions_.size() * sizeof(Aevrynd::WorldChunkLoadCompletion3D);
    return before > after ? before - after : 0;
  }

private:
  std::shared_ptr<const Aevrynd::TerrainVegetationChunkPayload3D> payload_;
  std::mutex mutex_;
  std::deque<Aevrynd::WorldChunkLoadCompletion3D> completions_;
};
} // 命名空間

struct GameplayHlodWorldRuntime3D::State {
  Aevrynd::Engine *engine{};
  Aevrynd::SceneECSRegistry3D *registry{};
  std::shared_ptr<Aevrynd::TerrainVegetationChunkPayload3D> payload{};
  std::unique_ptr<SingleGameplayHlodChunkSource3D> source{};
  Aevrynd::ChunkedVegetationWorld3D vegetation{};
  std::unique_ptr<Aevrynd::TerrainVegetationWorldChunkActivationTarget3D> target{};
  std::unique_ptr<Aevrynd::WorldPartition3D> partition{};
  GameplayHlodWorldRuntimeReceipt3D receipt{};
};

GameplayHlodWorldRuntime3D::GameplayHlodWorldRuntime3D() = default;
GameplayHlodWorldRuntime3D::~GameplayHlodWorldRuntime3D() { Shutdown(); }

bool GameplayHlodWorldRuntime3D::Initialize(
    Aevrynd::Engine &engine, Aevrynd::SceneECSRegistry3D &registry,
    ShowcaseRenderAssetBindings3D &assets,
    std::span<const ShowcaseSceneDescriptor3D> descriptors,
    Aevrynd::StableObjectId3D worldId) {
  if (state_ || !assets.ProductionActive())
    return false;
  auto state = std::make_unique<State>();
  state->engine = &engine;
  state->registry = &registry;
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
  state->source = std::make_unique<SingleGameplayHlodChunkSource3D>(state->payload);
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
          engine, registry, state->vegetation, library, nullptr,
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
  state_ = std::move(state);
  return true;
}

bool GameplayHlodWorldRuntime3D::CollectFrame(
    std::uint64_t frameId, const Aevrynd::Camera3D &camera,
    Aevrynd::Vec2 viewport, Aevrynd::Scene3D &scene,
    Aevrynd::DrawList &draw) {
  if (!state_ || !frameId)
    return false;
  state_->partition->Update({});
  state_->source->WaitIdle();
  state_->partition->Update({});
  state_->receipt.partition = state_->partition->Stats();
  state_->receipt.worldPartitionResident = state_->receipt.partition.resident == 1;
  if (!state_->receipt.worldPartitionResident)
    return false;
  Aevrynd::HlodRuntimeSelectionSettings3D selection{};
  selection.quality.maximumScreenError = .1f;
  selection.maximumVisibilityTransitionsPerFrame = state_->receipt.build.clusters;
  selection.cameraTeleportDistance = 128.f;
  Aevrynd::HlodSelectionView3D view{camera, viewport, {}, 512.f, false, false};
  state_->receipt.selected = state_->target->SelectHlodFrame3D(frameId, view, selection);
  if (!state_->receipt.selected)
    return false;
  const auto token = state_->target->FreezeHlodFrame3D(frameId);
  state_->receipt.frozen = token.has_value();
  if (!token)
    return false;
  const auto collection =
      state_->target->CollectFrozenHlodFrame3D(*token, scene, draw);
  state_->receipt.collected = collection.has_value();
  if (!collection)
    return false;
  state_->receipt.collection = *collection;
  state_->receipt.frozenFrame = token->frameId;
  state_->receipt.integration = state_->target->HlodStats();
  state_->receipt.frame = state_->target->HlodRuntimeStats();
  state_->receipt.aggregateProxyBuilt = state_->receipt.build.aggregateProxyBuilt;
  state_->receipt.gameplayLabOnRenderConnected = false;
  state_->receipt.allDescriptorKindsCovered = false;
  return state_->receipt.SeamComplete();
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
  return state_ ? state_->receipt : GameplayHlodWorldRuntimeReceipt3D{};
}

} // GameplayLab 命名空間
