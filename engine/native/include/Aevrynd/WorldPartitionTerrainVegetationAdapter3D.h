#pragma once
#include "Engine.h"
#include "HeightField3D.h"
#include "PhysicsWorld3D.h"
#include "SceneECS3D.h"
#include "StaticMeshAssetLibrary3D.h"
#include "Vegetation3D.h"
#include "WorldPartition3D.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

namespace Aevrynd {
// 來源的生命週期必須涵蓋 activation target，且只能由 target owner thread 呼叫。
class IGpuRetirementReceiptSource3D {
public:
  virtual ~IGpuRetirementReceiptSource3D() = default;
  virtual GpuRetirementReceipt3D CaptureGraphicsRetirementReceipt3D() = 0;
  virtual bool IsGraphicsRetirementComplete3D(
      GpuRetirementReceipt3D receipt) const = 0;
protected:
  static constexpr GpuRetirementReceipt3D MakeReceipt(
      std::uint64_t deviceEpoch, std::uint64_t fenceValue) noexcept {
    return GpuRetirementReceipt3D(deviceEpoch, fenceValue);
  }
};
class EngineGpuRetirementReceiptSource3D final
    : public IGpuRetirementReceiptSource3D {
public:
  explicit EngineGpuRetirementReceiptSource3D(Engine &engine) noexcept
      : engine_(engine) {}
  GpuRetirementReceipt3D CaptureGraphicsRetirementReceipt3D() override {
    return engine_.CaptureRetirementFence3D();
  }
  bool IsGraphicsRetirementComplete3D(
      GpuRetirementReceipt3D receipt) const override {
    return engine_.IsRetirementComplete3D(receipt);
  }
private:
  Engine &engine_;
};
class HlodRetirementGate3D final {
public:
  bool Begin(IGpuRetirementReceiptSource3D &source) {
    if (receipt_)
      return true;
    receipt_ = source.CaptureGraphicsRetirementReceipt3D();
    return bool(receipt_);
  }
  bool Complete(const IGpuRetirementReceiptSource3D &source) const {
    return receipt_ && source.IsGraphicsRetirementComplete3D(receipt_);
  }
  GpuRetirementReceipt3D Receipt() const noexcept { return receipt_; }
private:
  GpuRetirementReceipt3D receipt_{};
};
struct TerrainVegetationChunkSettings3D {
  float chunkSize{32.f};
  unsigned terrainCells{16};
  float terrainAmplitude{10.f};
  std::uint32_t vegetationPerChunk{32};
  std::array<Mesh3D, 3> vegetationLods{};
  MaterialTextureSetDefinition3D terrainTextures{}, vegetationTextures{};
  StableObjectId3D worldId{1, 1};
  WorldChunkBaseRevision3D baseRevision{};
  std::uint64_t buildingAssetId{1};
  Mesh3D buildingMesh{Mesh3D::CreateUnitBox()};
  MaterialTextureSetDefinition3D buildingTextures{};
  std::size_t maximumHlodProxyBytes{256u * 1024u * 1024u};
};

struct TerrainVegetationChunkPayload3D {
  GlobalChunkCoordinate3D globalChunk{};
  Mesh3D terrain{};
  HeightField3D heightField{};
  std::vector<VegetationInstance3D> vegetation{};
  std::vector<StableObjectId3D> vegetationIds{};
  WorldChunkBaseRevision3D baseRevision{};
  std::size_t cpuBytes{}, conservativeGpuBytes{};
  std::shared_ptr<const HlodData3D> hlod{};
  struct HlodManifest;
  std::shared_ptr<const HlodManifest> hlodManifest{};
};
struct TerrainVegetationHlodSource3D {HlodSourceReference3D reference{};std::uint64_t generation{};std::string logicalName{};GlobalPosition3D position{};Vec3 rotation{},scale{1,1,1};};
struct TerrainVegetationHlodCluster3D {HlodClusterId3D id{};std::uint64_t generation{};PreparedHlodProxyAsset3D proxy{};std::vector<TerrainVegetationHlodSource3D> sources{};};
struct TerrainVegetationChunkPayload3D::HlodManifest final {HlodManifest(const HlodManifest&)=delete;HlodManifest& operator=(const HlodManifest&)=delete;std::span<const TerrainVegetationHlodCluster3D> Clusters()const noexcept{return clusters_;}std::uint64_t ContentHash()const noexcept{return contentHash_;}std::uint64_t HlodDataHash()const noexcept{return hlodDataHash_;}private:friend std::shared_ptr<const HlodManifest> BuildTerrainVegetationHlodManifest3D(std::span<const TerrainVegetationHlodCluster3D>,const HlodData3D&);HlodManifest()=default;std::vector<TerrainVegetationHlodCluster3D> clusters_{};std::uint64_t contentHash_{},hlodDataHash_{};};
std::shared_ptr<const TerrainVegetationChunkPayload3D::HlodManifest> BuildTerrainVegetationHlodManifest3D(std::span<const TerrainVegetationHlodCluster3D> clusters,const HlodData3D& data);
struct HlodIntegrationStats3D {std::size_t inputClusters{},attachedClusters{},sourceOnlyFallbacks{},incompatibleFallbacks{},budgetFallbacks{},staleGenerationRejects{},overlapRejects{},rollbackCount{},residentSourceEntities{},residentProxyEntities{},retiringClusters{},metadataSizeBytes{},metadataCapacityBytes{},aliasBytes{},proxyReservedBytes{},proxyActualResidentBytes{},retiringProxyBytes{},sharedProxyDedupHits{},pendingRetirementReceipts{},lastReleasedBytes{};std::uint64_t trimCount{};};
struct ComposedTerrainVegetationChunk3D {
  TerrainVegetationChunkPayload3D payload{};
  std::vector<PersistentBuilding3D> buildings{};
};
bool ComposeTerrainVegetationChunkDelta3D(
    const TerrainVegetationChunkPayload3D &base,
    const WorldChunkDeltaSnapshot3D *delta,
    ComposedTerrainVegetationChunk3D &output);
GlobalChunkCoordinate3D
CanonicalTerrainSampleOwner3D(GlobalChunkCoordinate3D chunk, std::uint32_t x,
                              std::uint32_t z, std::uint32_t cells) noexcept;

class TerrainVegetationWorldChunkSource3D final : public IWorldChunkSource3D {
public:
  explicit TerrainVegetationWorldChunkSource3D(
      TerrainVegetationChunkSettings3D settings = {},
      std::uint32_t workers = 2);
  ~TerrainVegetationWorldChunkSource3D() override;
  bool BeginLoad(WorldChunkRequest3D request,
                 std::stop_token cancellation) override;
  bool TryPopCompletion(WorldChunkLoadCompletion3D &completion) override;
  void WaitIdle() override;
  std::size_t TrimUnusedMemory() override;

private:
  struct Work {
    WorldChunkRequest3D request{};
    std::stop_token cancellation{};
  };
  void Worker(std::stop_token stop);
  TerrainVegetationChunkSettings3D settings_{};
  std::mutex mutex_{};
  std::condition_variable_any wake_{}, idle_{};
  std::deque<Work> work_{};
  std::deque<WorldChunkLoadCompletion3D> completions_{};
  std::vector<std::jthread> workers_{};
  std::size_t active_{};
};

class TerrainVegetationWorldChunkActivationTarget3D final
    : public IWorldChunkActivationTarget3D,
      public IWorldOriginParticipant3D {
public:
  TerrainVegetationWorldChunkActivationTarget3D(
      Engine &engine, SceneECSRegistry3D &scene,
      ChunkedVegetationWorld3D &vegetation,
      TerrainVegetationChunkSettings3D settings = {});
  TerrainVegetationWorldChunkActivationTarget3D(
      Engine &engine, SceneECSRegistry3D &scene,
      ChunkedVegetationWorld3D &vegetation, PhysicsWorld3D *physics,
      TerrainVegetationChunkSettings3D settings = {});
  TerrainVegetationWorldChunkActivationTarget3D(Engine& engine,SceneECSRegistry3D& scene,ChunkedVegetationWorld3D& vegetation,StaticMeshAssetLibrary3D* assets,PhysicsWorld3D* physics,TerrainVegetationChunkSettings3D settings={});
  TerrainVegetationWorldChunkActivationTarget3D(
      Engine &engine, SceneECSRegistry3D &scene,
      ChunkedVegetationWorld3D &vegetation, StaticMeshAssetLibrary3D *assets,
      PhysicsWorld3D *physics, IGpuRetirementReceiptSource3D &retirementSource,
      TerrainVegetationChunkSettings3D settings = {});
  ~TerrainVegetationWorldChunkActivationTarget3D() override;
  std::size_t PreparationBytes(WorldChunkRequest3D,
                               const WorldChunkPayload3D &payload) const override;
  std::size_t
  PhysicalAllocationBytes(WorldChunkRequest3D,
                          const WorldChunkPayload3D &payload) const override;
  WorldChunkActivation3D Activate(WorldChunkRequest3D request,
                                  const WorldChunkPayload3D &payload) override;
  WorldChunkActivation3D
  ActivatePersistent(WorldChunkRequest3D request,
                     const WorldChunkPayload3D &payload,
                     const WorldChunkDeltaSnapshot3D *delta) override;
  std::size_t PersistentDependencyCoordinates(
      WorldChunkRequest3D request,
      std::span<WorldChunkCoordinate3D> output) const noexcept override;
  WorldChunkActivation3D ActivatePersistentDependencies(
      WorldChunkRequest3D request, const WorldChunkPayload3D &payload,
      const WorldChunkDeltaSnapshot3D *delta,
      std::span<const WorldChunkDeltaSnapshot3D> dependencies) override;
  bool IsActivationComplete(WorldChunkActivation3D activation) const override;
  void Rollback(WorldChunkActivation3D activation) override;
  WorldChunkActivation3D
  BeginRetire(WorldChunkActivation3D activation) override;
  bool IsRetirementComplete(WorldChunkActivation3D retirement) const override;
  void WaitIdle() override;
  std::size_t TrimUnusedMemory() override;
  void SetActivationFailureCheckpointForTesting(int value) noexcept {
    failureCheckpoint_ = value;
  }
  std::size_t PendingRetirements() const noexcept;
  HlodIntegrationStats3D HlodStats() const noexcept;
  // 邊界樣本只寫入 canonical owner；所有目前常駐的共用邊界網格會在同一提交中更新。
  bool ApplyTerrainHeightEditAtomic(
      WorldPartition3D &partition, GlobalChunkCoordinate3D chunk,
      std::uint32_t x, std::uint32_t z, float height,
      WorldChunkDeltaSnapshot3D ownerSnapshot);
  std::optional<float> QueryTerrainHeight(
      GlobalChunkCoordinate3D chunk, std::uint32_t x,
      std::uint32_t z) const noexcept;
  bool SetWorldOriginSnapshot3D(WorldOrigin3D origin) noexcept;
  WorldOrigin3D WorldOriginSnapshot3D() const noexcept { return origin_; }
  bool
  PrepareWorldOriginRebase3D(const WorldOriginRebaseEvent3D &event) override;
  void CommitWorldOriginRebase3D(
      const WorldOriginRebaseEvent3D &event) noexcept override;
  void AbortWorldOriginRebase3D() noexcept override { preparedOrigin_ = {}; }

private:
  // GPU、ECS 與植被發布只能由建立 target 的主執行緒操作；跨執行緒呼叫一律
  // fail-closed。
  bool OnOwnerThread() const noexcept {
    return std::this_thread::get_id() == ownerThread_;
  }
  struct SharedVegetation {
    std::array<MeshResource3D, 3> meshes{};
    MaterialTextureSetHandle3D textures{};
    MeshResource3D buildingMesh{};
    MaterialTextureSetHandle3D buildingTextures{};
    AssetLease3D lease{};
    std::size_t references{}, physicalBytes{};
  };
  struct Record {
    struct HlodClusterAttachment {HlodClusterId3D id{};std::uint64_t generation{};HlodProxyLease3D proxy{};std::vector<StaticMeshRenderableLease3D> sources{};SceneHlodEntityBatch3D entities{};std::size_t reservedBytes{},actualBytes{},aliasBytes{};bool sceneDestroyed{},sourcesReset{},aliasReleased{};};
    struct HlodAttachment {std::shared_ptr<const HlodData3D> data{};std::shared_ptr<const TerrainVegetationChunkPayload3D::HlodManifest> manifest{};std::vector<HlodClusterAttachment> clusters{};};
    WorldChunkRequest3D request{};
    MeshResource3D terrain{};
    MaterialTextureSetHandle3D terrainTextures{};
    std::vector<SceneEntity3D> entities{};
    std::vector<RigidBodyHandle3D> buildingBodies{};
    VegetationChunkHandle3D vegetation{};
    HeightField3D heightField{};
    GpuUploadReceipt3D receipt{};
    HlodRetirementGate3D retirementGate{};
    std::optional<HlodAttachment> hlod{};
    std::size_t physicalBytes{};
    bool retiring{}, released{},hlodHidden{};
  };
  bool EnsureShared(const TerrainVegetationChunkPayload3D &payload,
                    bool needsBuilding, std::size_t &physicalBytes);
  void ReleaseRecord(Record &record);
  bool HideHlodForRetirement(Record &record);
  bool HasSharedProxyBorrower(
      const Record::HlodClusterAttachment &cluster) const noexcept;
  bool ReleaseHlod(Record& record);
  Engine &engine_;
  SceneECSRegistry3D &scene_;
  ChunkedVegetationWorld3D &vegetation_;
  PhysicsWorld3D *physics_{};
  StaticMeshAssetLibrary3D* assets_{};
  TerrainVegetationChunkSettings3D settings_{};
  SharedVegetation shared_{};
  mutable std::unordered_map<std::uint64_t, Record> records_{};
  std::thread::id ownerThread_{};
  std::uint64_t next_{1};
  int failureCheckpoint_{-1};
  WorldOrigin3D origin_{};
  WorldOrigin3D preparedOrigin_{};
  mutable HlodIntegrationStats3D hlodStats_{};
  EngineGpuRetirementReceiptSource3D engineRetirementSource_;
  IGpuRetirementReceiptSource3D *retirementSource_{};
};
} // namespace Aevrynd
