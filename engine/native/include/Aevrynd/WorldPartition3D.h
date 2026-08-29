#pragma once
#include "WorldDeltaPersistence3D.h"
#include "WorldOrigin3D.h"
#include "WorldStreamer3D.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stop_token>

namespace Aevrynd {
enum class WorldChunkState3D : uint8_t {
  Missing,
  Queued,
  Loading,
  LoadingDelta,
  CpuReady,
  Activating,
  Resident,
  SavingDelta,
  Retiring,
  Failed
};
enum class WorldPartitionFailureReason3D : uint8_t {
  None,
  SourceRejected,
  LoadFailed,
  CpuBudget,
  PreparationBudget,
  ActivationBudget,
  GpuBudget,
  ActivationFailed,
  ActivationExceededReservation
};

struct WorldChunkRequest3D {
  WorldChunkCoordinate3D coordinate{};
  uint32_t seed{};
  uint64_t epoch{}, requestId{}, revision{};
};
struct WorldChunkPayload3D {
  std::shared_ptr<const void> data;
  std::size_t cpuBytes{}, gpuBytes{};
  explicit operator bool() const { return bool(data); }
};
struct WorldChunkLoadCompletion3D {
  WorldChunkRequest3D request{};
  WorldChunkPayload3D payload{};
  bool succeeded{};
};
struct WorldChunkActivation3D {
  uint64_t value{};
  std::size_t gpuBytes{};
  explicit operator bool() const { return value != 0; }
};

class IWorldChunkSource3D {
public:
  virtual ~IWorldChunkSource3D() = default;
  virtual bool BeginLoad(WorldChunkRequest3D request,
                         std::stop_token cancellation) = 0;
  virtual bool TryPopCompletion(WorldChunkLoadCompletion3D &completion) = 0;
  virtual void WaitIdle() = 0;
  virtual std::size_t TrimUnusedMemory() = 0;
};

class IWorldChunkActivationTarget3D {
public:
  virtual ~IWorldChunkActivationTarget3D() = default;
  virtual std::size_t
  PreparationBytes(WorldChunkRequest3D request,
                   const WorldChunkPayload3D &payload) const = 0;
  virtual std::size_t
  PhysicalAllocationBytes(WorldChunkRequest3D request,
                          const WorldChunkPayload3D &payload) const = 0;
  virtual WorldChunkActivation3D
  Activate(WorldChunkRequest3D request, const WorldChunkPayload3D &payload) = 0;
  virtual WorldChunkActivation3D
  ActivatePersistent(WorldChunkRequest3D request,
                     const WorldChunkPayload3D &payload,
                     const WorldChunkDeltaSnapshot3D *delta) {
    (void)delta;
    return Activate(request, payload);
  }
  virtual bool
  IsActivationComplete(WorldChunkActivation3D activation) const = 0;
  virtual void Rollback(WorldChunkActivation3D activation) = 0;
  virtual WorldChunkActivation3D
  BeginRetire(WorldChunkActivation3D activation) = 0;
  virtual bool
  IsRetirementComplete(WorldChunkActivation3D retirement) const = 0;
  virtual void WaitIdle() = 0;
  virtual std::size_t TrimUnusedMemory() = 0;
};

struct WorldPartitionSettings3D {
  WorldStreamingSettings3D spatial{};
  uint32_t workerCount{2};
  std::size_t maximumInflightLoads{2}, maximumCpuReadyBytes{64 * 1024 * 1024},
      maximumGpuResidentBytes{256 * 1024 * 1024},
      maximumPreparationBytesPerUpdate{8 * 1024 * 1024},
      maximumActivationBytesPerUpdate{8 * 1024 * 1024};
  uint32_t fairnessAgeUpdates{8};
  WorldChunkDeltaStore3D *persistence{};
  StableObjectId3D persistenceWorld{};
  std::uint32_t persistenceLayer{}, persistenceChunkSizeRevision{1};
  WorldChunkBaseRevision3D persistenceBase{};
};

struct WorldPartitionStats3D {
  WorldChunkCoordinate3D observerChunk{};
  uint64_t epoch{}, updates{}, nextRequestId{1}, totalQueued{}, totalStarted{},
      totalCompleted{}, totalActivated{}, totalRetired{}, totalFailed{},
      totalCancelled{}, staleCompletions{};
  uint64_t sourceFailures{}, loadFailures{}, cpuBudgetFailures{},
      preparationBudgetFailures{}, activationBudgetFailures{},
      gpuBudgetFailures{}, activationFailures{};
  WorldPartitionFailureReason3D lastFailure{
      WorldPartitionFailureReason3D::None};
  std::size_t queued{}, loading{}, loadingDelta{}, cpuReady{}, activating{},
      resident{}, savingDelta{}, retiring{}, failed{}, inflightLoads{},
      peakInflightLoads{}, cpuReadyBytes{}, residentGpuBytes{},
      retiringGpuBytes{}, lastPreparedBytes{}, lastActivatedBytes{},
      partitionOwnedWorkers{}, requestedSourceWorkers{};
  std::uint64_t deltaLoads{}, deltaLoadFailures{}, deltaSaves{},
      deltaSaveFailures{}, staleDeltaCompletions{};
};

struct WorldPartitionChunkStatus3D {
  WorldChunkCoordinate3D coordinate{};
  WorldChunkState3D state{WorldChunkState3D::Missing};
  uint64_t requestId{}, revision{};
};

class WorldPartition3D {
public:
  WorldPartition3D(IWorldChunkSource3D &source,
                   IWorldChunkActivationTarget3D &activation,
                   WorldPartitionSettings3D settings = {});
  ~WorldPartition3D();
  WorldPartition3D(const WorldPartition3D &) = delete;
  WorldPartition3D &operator=(const WorldPartition3D &) = delete;
  void Update(Vec3 observerPosition);
  // 全域觀測者不依賴目前 render origin，因此 rebase 不會取消或重建載入請求。
  bool UpdateGlobal(GlobalPosition3D observerPosition,
                    float globalChunkSize = 32.f);
  void Reset();
  void WaitIdle();
  std::size_t TrimUnusedMemory();
  WorldPartitionStats3D Stats() const { return stats_; }
  std::span<const WorldPartitionChunkStatus3D> Chunks() const { return view_; }
  bool SetChunkDelta(WorldChunkCoordinate3D coordinate,
                     WorldChunkDeltaSnapshot3D snapshot);

private:
  void UpdateCoordinate(WorldChunkCoordinate3D observer);
  struct Impl;
  std::unique_ptr<Impl> impl_;
  WorldPartitionStats3D stats_{};
  std::vector<WorldPartitionChunkStatus3D> view_;
};
} // namespace Aevrynd
