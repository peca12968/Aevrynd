#include <Aevrynd/WorldPartition3D.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace Aevrynd {
namespace {
struct Hash {
  size_t operator()(WorldChunkCoordinate3D c) const noexcept {
    return size_t(uint32_t(c.x)) * 0x9e3779b1u ^
           size_t(uint32_t(c.z)) * 0x85ebca77u;
  }
};
unsigned Distance(WorldChunkCoordinate3D a, WorldChunkCoordinate3D b) {
  return unsigned((std::max)(std::abs(a.x - b.x), std::abs(a.z - b.z)));
}
bool Fits(size_t used, size_t add, size_t limit) {
  return used <= limit && add <= limit - used;
}
WorldDeltaChunkKey3D DeltaKey(const WorldPartitionSettings3D &settings,
                              WorldChunkCoordinate3D coordinate) {
  return {settings.persistenceWorld,
          settings.persistenceLayer,
          settings.persistenceChunkSizeRevision,
          {coordinate.x, 0, coordinate.z}};
}
} // namespace
struct WorldPartition3D::Impl {
  struct Entry {
    WorldChunkRequest3D request{};
    WorldChunkState3D state{WorldChunkState3D::Missing};
    WorldChunkPayload3D payload{};
    WorldChunkDeltaSnapshot3D delta{};
    std::vector<WorldChunkDeltaSnapshot3D> deltaDependencies{};
    WorldChunkActivation3D activation{}, retirement{};
    std::stop_source stop;
    uint64_t queuedAt{}, deltaIoRequest{};
    std::size_t pendingDeltaLoads{};
    bool desired{true}, dirty{};
  };
  IWorldChunkSource3D &source;
  IWorldChunkActivationTarget3D &target;
  WorldPartitionSettings3D settings;
  std::unordered_map<WorldChunkCoordinate3D, Entry, Hash> chunks;
  Vec3 lastObserver{};
  bool hasObserver{};
  Impl(IWorldChunkSource3D &s, IWorldChunkActivationTarget3D &t,
       WorldPartitionSettings3D v)
      : source(s), target(t), settings(v) {
    settings.spatial.chunkSize = (std::max)(1.f, settings.spatial.chunkSize);
    settings.spatial.retainRadius =
        (std::max)(settings.spatial.loadRadius, settings.spatial.retainRadius);
    settings.maximumInflightLoads =
        settings.workerCount
            ? (std::max)(size_t(1), settings.maximumInflightLoads)
            : 0;
    settings.fairnessAgeUpdates = (std::max)(1u, settings.fairnessAgeUpdates);
    if (!settings.persistenceChunkSizeRevision)
      settings.persistence = nullptr;
  }
};
WorldPartition3D::WorldPartition3D(IWorldChunkSource3D &s,
                                   IWorldChunkActivationTarget3D &t,
                                   WorldPartitionSettings3D v)
    : impl_(std::make_unique<Impl>(s, t, v)) {
  stats_.epoch = 1;
  stats_.requestedSourceWorkers = v.workerCount;
}
WorldPartition3D::~WorldPartition3D() {
  Reset();
  try {
    WaitIdle();
  } catch (...) {
    // 永久 I/O 失敗時保留 fail-closed 診斷；析構不可終止程序。
  }
}
void WorldPartition3D::Update(Vec3 position) {
  if (!impl_)
    return;
  const auto coordinate = WorldStreamer3D::CoordinateAt(
      position, impl_->settings.spatial.chunkSize);
  UpdateCoordinate(coordinate);
}
bool WorldPartition3D::UpdateGlobal(GlobalPosition3D position,
                                    float globalChunkSize) {
  if (!impl_ || !std::isfinite(globalChunkSize) || globalChunkSize <= 0 ||
      !NormalizeGlobalPosition3D(position, globalChunkSize))
    return false;
  const double x = double(position.chunk.x) * globalChunkSize +
                   position.offset.x,
               z = double(position.chunk.z) * globalChunkSize +
                   position.offset.z,
               partitionSize = impl_->settings.spatial.chunkSize,
               cx = std::floor(x / partitionSize),
               cz = std::floor(z / partitionSize);
  if (!std::isfinite(cx) || !std::isfinite(cz) ||
      cx < std::numeric_limits<int>::min() ||
      cx > std::numeric_limits<int>::max() ||
      cz < std::numeric_limits<int>::min() ||
      cz > std::numeric_limits<int>::max())
    return false;
  UpdateCoordinate({int(cx), int(cz)});
  return true;
}
void WorldPartition3D::UpdateCoordinate(WorldChunkCoordinate3D observer) {
  auto &d = *impl_;
  if (!d.settings.workerCount)
    return;
  d.hasObserver = true;
  stats_.observerChunk = observer;
  ++stats_.updates;
  stats_.lastPreparedBytes = stats_.lastActivatedBytes = 0;
  if (d.settings.persistence) {
    WorldDeltaLoadResult3D loaded{};
    while (d.settings.persistence->TryPopLoad(loaded)) {
      auto it = std::find_if(d.chunks.begin(), d.chunks.end(), [&](auto &pair) {
        return pair.second.state == WorldChunkState3D::LoadingDelta &&
               pair.second.deltaIoRequest == loaded.requestId &&
               pair.second.request.epoch == loaded.epoch;
      });
      if (it == d.chunks.end()) {
        ++stats_.staleDeltaCompletions;
        continue;
      }
      auto &e = it->second;
      if (loaded.status == WorldDeltaLoadStatus3D::Clean ||
          loaded.status == WorldDeltaLoadStatus3D::Loaded) {
        if (loaded.status == WorldDeltaLoadStatus3D::Clean) {
          loaded.snapshot.key = loaded.key;
          loaded.snapshot.base = d.settings.persistenceBase;
        }
        const auto primary = DeltaKey(d.settings, e.request.coordinate);
        if (loaded.key == primary)
          e.delta = std::move(loaded.snapshot);
        else
          e.deltaDependencies.push_back(std::move(loaded.snapshot));
        if (e.pendingDeltaLoads)
          --e.pendingDeltaLoads;
        ++stats_.deltaLoads;
        if (!e.pendingDeltaLoads) {
          e.deltaIoRequest = 0;
          e.state = WorldChunkState3D::CpuReady;
        }
      } else {
        e.deltaIoRequest = 0;
        e.pendingDeltaLoads = 0;
        stats_.cpuReadyBytes = stats_.cpuReadyBytes >= e.payload.cpuBytes
                                   ? stats_.cpuReadyBytes - e.payload.cpuBytes
                                   : 0;
        e.payload = {};
        e.state = WorldChunkState3D::Failed;
        ++stats_.deltaLoadFailures;
        ++stats_.totalFailed;
      }
    }
    WorldDeltaSaveResult3D saved{};
    while (d.settings.persistence->TryPopSave(saved)) {
      auto it = d.chunks.find({int(saved.key.chunk.x), int(saved.key.chunk.z)});
      if (it == d.chunks.end() ||
          (it->second.state != WorldChunkState3D::SavingDelta &&
           it->second.state != WorldChunkState3D::Resident) ||
          it->second.deltaIoRequest != saved.requestId ||
          it->second.request.epoch != saved.epoch) {
        ++stats_.staleDeltaCompletions;
        continue;
      }
      auto &e = it->second;
      e.deltaIoRequest = 0;
      if (!saved.durable || saved.generation < e.delta.durableGeneration) {
        ++stats_.deltaSaveFailures;
        continue;
      }
      e.dirty = false;
      ++stats_.deltaSaves;
      if (e.state == WorldChunkState3D::Resident &&
          Distance(e.request.coordinate, observer) <=
              d.settings.spatial.retainRadius)
        continue;
      if (Distance(e.request.coordinate, observer) <=
          d.settings.spatial.retainRadius) {
        e.desired = true;
        e.state = WorldChunkState3D::Resident;
        continue;
      }
      e.retirement = d.target.BeginRetire(e.activation);
      e.state = WorldChunkState3D::Retiring;
      stats_.residentGpuBytes =
          stats_.residentGpuBytes >= e.activation.gpuBytes
              ? stats_.residentGpuBytes - e.activation.gpuBytes
              : 0;
      stats_.retiringGpuBytes += e.activation.gpuBytes;
      ++stats_.totalRetired;
    }
    for (auto &[coordinate, e] : d.chunks)
      if ((e.state == WorldChunkState3D::SavingDelta ||
           (e.state == WorldChunkState3D::Resident && e.dirty)) &&
          !e.deltaIoRequest) {
        e.deltaIoRequest = e.request.requestId;
        if (!d.settings.persistence->EnqueueSave(e.deltaIoRequest,
                                                 e.request.epoch, e.delta)) {
          e.deltaIoRequest = 0;
          ++stats_.deltaSaveFailures;
        }
      }
  }
  WorldChunkLoadCompletion3D done{};
  while (d.source.TryPopCompletion(done)) {
    auto it = d.chunks.find(done.request.coordinate);
    if (it == d.chunks.end() ||
        it->second.request.epoch != done.request.epoch ||
        it->second.request.requestId != done.request.requestId ||
        it->second.request.revision != done.request.revision ||
        it->second.state != WorldChunkState3D::Loading) {
      ++stats_.staleCompletions;
      continue;
    }
    auto &e = it->second;
    if (stats_.inflightLoads)
      --stats_.inflightLoads;
    ++stats_.totalCompleted;
    if (!e.desired) {
      ++stats_.staleCompletions;
      d.chunks.erase(it);
      continue;
    }
    if (!done.succeeded || !done.payload) {
      e.state = WorldChunkState3D::Failed;
      ++stats_.totalFailed;
      ++stats_.loadFailures;
      stats_.lastFailure = WorldPartitionFailureReason3D::LoadFailed;
      continue;
    }
    if (!Fits(stats_.cpuReadyBytes, done.payload.cpuBytes,
              d.settings.maximumCpuReadyBytes)) {
      e.state = WorldChunkState3D::Failed;
      ++stats_.totalFailed;
      ++stats_.cpuBudgetFailures;
      stats_.lastFailure = WorldPartitionFailureReason3D::CpuBudget;
      continue;
    }
    e.payload = std::move(done.payload);
    stats_.cpuReadyBytes += e.payload.cpuBytes;
    if (d.settings.persistence) {
      e.deltaIoRequest = e.request.requestId;
      std::array<WorldChunkCoordinate3D, 8> dependencies{};
      const auto dependencyCount = d.target.PersistentDependencyCoordinates(
          e.request, dependencies);
      bool admitted = dependencyCount <= dependencies.size() &&
                      d.settings.persistence->EnqueueLoad(
                          e.deltaIoRequest, e.request.epoch,
                          DeltaKey(d.settings, e.request.coordinate),
                          d.settings.persistenceBase);
      e.pendingDeltaLoads = admitted ? 1 : 0;
      for (std::size_t index = 0; admitted && index < dependencyCount;
           ++index) {
        admitted = d.settings.persistence->EnqueueLoad(
            e.deltaIoRequest, e.request.epoch,
            DeltaKey(d.settings, dependencies[index]),
            d.settings.persistenceBase);
        if (admitted)
          ++e.pendingDeltaLoads;
      }
      if (admitted)
        e.state = WorldChunkState3D::LoadingDelta;
      else {
        e.pendingDeltaLoads = 0;
        stats_.cpuReadyBytes -= e.payload.cpuBytes;
        e.payload = {};
        e.state = WorldChunkState3D::Failed;
        ++stats_.deltaLoadFailures;
        ++stats_.totalFailed;
      }
    } else
      e.state = WorldChunkState3D::CpuReady;
  }
  for (auto it = d.chunks.begin(); it != d.chunks.end();) {
    auto &e = it->second;
    if (Distance(it->first, observer) <= d.settings.spatial.retainRadius) {
      e.desired = true;
      ++it;
      continue;
    }
    e.desired = false;
    e.stop.request_stop();
    if (e.state == WorldChunkState3D::Loading) {
      ++stats_.totalCancelled;
      ++it;
      continue;
    }
    if (e.state == WorldChunkState3D::LoadingDelta) {
      stats_.cpuReadyBytes = stats_.cpuReadyBytes >= e.payload.cpuBytes
                                 ? stats_.cpuReadyBytes - e.payload.cpuBytes
                                 : 0;
      it = d.chunks.erase(it);
      continue;
    }
    if (e.state == WorldChunkState3D::SavingDelta) {
      ++it;
      continue;
    }
    if (e.state == WorldChunkState3D::Activating) {
      d.target.Rollback(e.activation);
      stats_.cpuReadyBytes = stats_.cpuReadyBytes >= e.payload.cpuBytes
                                 ? stats_.cpuReadyBytes - e.payload.cpuBytes
                                 : 0;
      stats_.residentGpuBytes =
          stats_.residentGpuBytes >= e.activation.gpuBytes
              ? stats_.residentGpuBytes - e.activation.gpuBytes
              : 0;
      it = d.chunks.erase(it);
      continue;
    }
    if (e.state == WorldChunkState3D::Resident) {
      if (d.settings.persistence && e.dirty) {
        e.delta.key = DeltaKey(d.settings, e.request.coordinate);
        e.delta.base = d.settings.persistenceBase;
        if (e.deltaIoRequest)
          e.state = WorldChunkState3D::SavingDelta;
        else {
          e.deltaIoRequest = e.request.requestId;
          if (d.settings.persistence->EnqueueSave(
                  e.deltaIoRequest, e.request.epoch, e.delta))
            e.state = WorldChunkState3D::SavingDelta;
          else {
            e.deltaIoRequest = 0;
            ++stats_.deltaSaveFailures;
          }
        }
        ++it;
        continue;
      }
      e.retirement = d.target.BeginRetire(e.activation);
      e.state = WorldChunkState3D::Retiring;
      stats_.residentGpuBytes =
          stats_.residentGpuBytes >= e.activation.gpuBytes
              ? stats_.residentGpuBytes - e.activation.gpuBytes
              : 0;
      stats_.retiringGpuBytes += e.activation.gpuBytes;
      ++stats_.totalRetired;
      ++it;
      continue;
    }
    if (e.state == WorldChunkState3D::Retiring) {
      ++it;
      continue;
    }
    if (e.state == WorldChunkState3D::CpuReady)
      stats_.cpuReadyBytes = stats_.cpuReadyBytes >= e.payload.cpuBytes
                                 ? stats_.cpuReadyBytes - e.payload.cpuBytes
                                 : 0;
    it = d.chunks.erase(it);
  }
  for (auto it = d.chunks.begin(); it != d.chunks.end();) {
    auto &e = it->second;
    if (e.state == WorldChunkState3D::Activating &&
        d.target.IsActivationComplete(e.activation)) {
      e.state = WorldChunkState3D::Resident;
      stats_.cpuReadyBytes = stats_.cpuReadyBytes >= e.payload.cpuBytes
                                 ? stats_.cpuReadyBytes - e.payload.cpuBytes
                                 : 0;
      e.payload = {};
      ++stats_.totalActivated;
    }
    if (e.state == WorldChunkState3D::Retiring &&
        d.target.IsRetirementComplete(e.retirement)) {
      stats_.retiringGpuBytes =
          stats_.retiringGpuBytes >= e.activation.gpuBytes
              ? stats_.retiringGpuBytes - e.activation.gpuBytes
              : 0;
      it = d.chunks.erase(it);
      continue;
    }
    ++it;
  }
  const int r = int(d.settings.spatial.loadRadius);
  for (int z = -r; z <= r; ++z)
    for (int x = -r; x <= r; ++x) {
      WorldChunkCoordinate3D c{observer.x + x, observer.z + z};
      if (d.chunks.contains(c))
        continue;
      Impl::Entry e{};
      e.state = WorldChunkState3D::Queued;
      e.queuedAt = stats_.updates;
      e.request = {c,
                   WorldStreamer3D::ChunkSeed(c, d.settings.spatial.worldSeed),
                   stats_.epoch, stats_.nextRequestId++, 1};
      d.chunks.emplace(c, std::move(e));
      ++stats_.totalQueued;
    }
  std::vector<Impl::Entry *> queue;
  for (auto &[c, e] : d.chunks)
    if (e.state == WorldChunkState3D::Queued)
      queue.push_back(&e);
  std::sort(queue.begin(), queue.end(), [&](auto *a, auto *b) {
    const auto aa = stats_.updates - a->queuedAt,
               ab = stats_.updates - b->queuedAt;
    const bool fa = aa >= d.settings.fairnessAgeUpdates,
               fb = ab >= d.settings.fairnessAgeUpdates;
    if (fa != fb)
      return fa;
    if (fa && aa != ab)
      return aa > ab;
    const auto da = Distance(a->request.coordinate, observer),
               db = Distance(b->request.coordinate, observer);
    if (da != db)
      return da < db;
    if (a->request.coordinate.z != b->request.coordinate.z)
      return a->request.coordinate.z < b->request.coordinate.z;
    return a->request.coordinate.x < b->request.coordinate.x;
  });
  for (auto *e : queue) {
    if (stats_.inflightLoads >= d.settings.maximumInflightLoads)
      break;
    bool started{};
    try {
      started = d.source.BeginLoad(e->request, e->stop.get_token());
    } catch (...) {
      started = false;
    }
    if (!started) {
      e->state = WorldChunkState3D::Failed;
      ++stats_.totalFailed;
      continue;
    }
    e->state = WorldChunkState3D::Loading;
    ++stats_.inflightLoads;
    ++stats_.totalStarted;
    stats_.peakInflightLoads =
        (std::max)(stats_.peakInflightLoads, stats_.inflightLoads);
  }
  std::vector<Impl::Entry *> ready;
  for (auto &[c, e] : d.chunks)
    if (e.state == WorldChunkState3D::CpuReady && e.desired)
      ready.push_back(&e);
  std::sort(ready.begin(), ready.end(), [&](auto *a, auto *b) {
    const auto da = Distance(a->request.coordinate, observer),
               db = Distance(b->request.coordinate, observer);
    if (da != db)
      return da < db;
    if (a->request.coordinate.z != b->request.coordinate.z)
      return a->request.coordinate.z < b->request.coordinate.z;
    return a->request.coordinate.x < b->request.coordinate.x;
  });
  for (auto *e : ready) {
    const size_t prepare = d.target.PreparationBytes(e->request, e->payload),
                 reserved =
                     d.target.PhysicalAllocationBytes(e->request, e->payload);
    const auto fail = [&](WorldPartitionFailureReason3D reason,
                          uint64_t &counter) {
      e->state = WorldChunkState3D::Failed;
      stats_.cpuReadyBytes = stats_.cpuReadyBytes >= e->payload.cpuBytes
                                 ? stats_.cpuReadyBytes - e->payload.cpuBytes
                                 : 0;
      e->payload = {};
      ++stats_.totalFailed;
      ++counter;
      stats_.lastFailure = reason;
    };
    if (prepare > d.settings.maximumPreparationBytesPerUpdate) {
      fail(WorldPartitionFailureReason3D::PreparationBudget,
           stats_.preparationBudgetFailures);
      continue;
    }
    if (reserved > d.settings.maximumActivationBytesPerUpdate) {
      fail(WorldPartitionFailureReason3D::ActivationBudget,
           stats_.activationBudgetFailures);
      continue;
    }
    if (reserved > d.settings.maximumGpuResidentBytes) {
      fail(WorldPartitionFailureReason3D::GpuBudget, stats_.gpuBudgetFailures);
      continue;
    }
    if (!Fits(stats_.residentGpuBytes, stats_.retiringGpuBytes,
              d.settings.maximumGpuResidentBytes))
      continue;
    const size_t physical = stats_.residentGpuBytes + stats_.retiringGpuBytes;
    if (!Fits(stats_.lastPreparedBytes, prepare,
              d.settings.maximumPreparationBytesPerUpdate) ||
        !Fits(stats_.lastActivatedBytes, reserved,
              d.settings.maximumActivationBytesPerUpdate) ||
        !Fits(physical, reserved, d.settings.maximumGpuResidentBytes))
      continue;
    auto activation = d.target.ActivatePersistentDependencies(
        e->request, e->payload,
        e->delta.operations.empty() ? nullptr : &e->delta,
        e->deltaDependencies);
    e->deltaDependencies.clear();
    if (!activation) {
      fail(WorldPartitionFailureReason3D::ActivationFailed,
           stats_.activationFailures);
      continue;
    }
    if (activation.gpuBytes > reserved) {
      d.target.Rollback(activation);
      fail(WorldPartitionFailureReason3D::ActivationExceededReservation,
           stats_.activationFailures);
      continue;
    }
    e->activation = activation;
    e->state = WorldChunkState3D::Activating;
    stats_.residentGpuBytes += activation.gpuBytes;
    stats_.lastPreparedBytes += prepare;
    stats_.lastActivatedBytes += activation.gpuBytes;
  }
  stats_.queued = stats_.loading = stats_.loadingDelta = stats_.cpuReady =
      stats_.activating = stats_.resident = stats_.savingDelta =
          stats_.retiring = stats_.failed = 0;
  view_.clear();
  for (const auto &[c, e] : d.chunks) {
    switch (e.state) {
    case WorldChunkState3D::Queued:
      ++stats_.queued;
      break;
    case WorldChunkState3D::Loading:
      ++stats_.loading;
      break;
    case WorldChunkState3D::LoadingDelta:
      ++stats_.loadingDelta;
      break;
    case WorldChunkState3D::CpuReady:
      ++stats_.cpuReady;
      break;
    case WorldChunkState3D::Activating:
      ++stats_.activating;
      break;
    case WorldChunkState3D::Resident:
      ++stats_.resident;
      break;
    case WorldChunkState3D::SavingDelta:
      ++stats_.savingDelta;
      break;
    case WorldChunkState3D::Retiring:
      ++stats_.retiring;
      break;
    case WorldChunkState3D::Failed:
      ++stats_.failed;
      break;
    default:
      break;
    }
    view_.push_back({c, e.state, e.request.requestId, e.request.revision});
  }
  std::sort(view_.begin(), view_.end(), [](auto &a, auto &b) {
    return a.coordinate.z != b.coordinate.z ? a.coordinate.z < b.coordinate.z
                                            : a.coordinate.x < b.coordinate.x;
  });
}
bool WorldPartition3D::SetChunkDelta(WorldChunkCoordinate3D coordinate,
                                     WorldChunkDeltaSnapshot3D snapshot) {
  if (!impl_ || !impl_->settings.persistence || !snapshot.durableGeneration)
    return false;
  auto &d = *impl_;
  const auto found = d.chunks.find(coordinate);
  if (found == d.chunks.end() ||
      (found->second.state != WorldChunkState3D::Resident &&
       found->second.state != WorldChunkState3D::Activating))
    return false;
  const auto expected = DeltaKey(d.settings, coordinate);
  if (snapshot.key != WorldDeltaChunkKey3D{} && snapshot.key != expected)
    return false;
  if (snapshot.base != WorldChunkBaseRevision3D{} &&
      !(snapshot.base == d.settings.persistenceBase))
    return false;
  snapshot.key = expected;
  snapshot.base = d.settings.persistenceBase;
  if (found->second.deltaIoRequest ||
      !d.settings.persistence->EnqueueSave(found->second.request.requestId,
                                           found->second.request.epoch,
                                           snapshot))
    return false;
  found->second.delta = std::move(snapshot);
  found->second.dirty = true;
  found->second.deltaIoRequest = found->second.request.requestId;
  return true;
}
void WorldPartition3D::Reset() {
  if (!impl_)
    return;
  auto &d = *impl_;
  d.hasObserver = false;
  if (stats_.epoch == (std::numeric_limits<std::uint64_t>::max)())
    return;
  const uint64_t epoch = stats_.epoch + 1;
  for (auto &[c, e] : d.chunks) {
    (void)c;
    e.desired = false;
    e.stop.request_stop();
    if (e.state == WorldChunkState3D::Activating) {
      d.target.Rollback(e.activation);
      e.state = WorldChunkState3D::Missing;
    } else if (e.state == WorldChunkState3D::Resident) {
      if (d.settings.persistence && e.dirty) {
        e.delta.key = DeltaKey(d.settings, e.request.coordinate);
        e.delta.base = d.settings.persistenceBase;
        if (!e.deltaIoRequest) {
          e.deltaIoRequest = e.request.requestId;
          if (!d.settings.persistence->EnqueueSave(
                  e.deltaIoRequest, e.request.epoch, e.delta)) {
            e.deltaIoRequest = 0;
            ++stats_.deltaSaveFailures;
          }
        }
        e.state = WorldChunkState3D::SavingDelta;
      } else {
        e.retirement = d.target.BeginRetire(e.activation);
        e.state = WorldChunkState3D::Retiring;
        stats_.residentGpuBytes =
            stats_.residentGpuBytes >= e.activation.gpuBytes
                ? stats_.residentGpuBytes - e.activation.gpuBytes
                : 0;
        stats_.retiringGpuBytes += e.activation.gpuBytes;
      }
    } else if (e.state == WorldChunkState3D::SavingDelta &&
               d.settings.persistence && !e.deltaIoRequest) {
      e.deltaIoRequest = e.request.requestId;
      if (!d.settings.persistence->EnqueueSave(
              e.deltaIoRequest, e.request.epoch, e.delta)) {
        e.deltaIoRequest = 0;
        ++stats_.deltaSaveFailures;
      }
    }
  }
  d.source.WaitIdle();
  if (d.settings.persistence)
    d.settings.persistence->WaitIdle();
  d.target.WaitIdle();
  WorldChunkLoadCompletion3D ignored{};
  while (d.source.TryPopCompletion(ignored))
    ++stats_.staleCompletions;
  if (d.settings.persistence) {
    WorldDeltaLoadResult3D ignoredLoad{};
    while (d.settings.persistence->TryPopLoad(ignoredLoad))
      ++stats_.staleDeltaCompletions;
    WorldDeltaSaveResult3D saved{};
    while (d.settings.persistence->TryPopSave(saved)) {
      auto found = d.chunks.find(
          {static_cast<int>(saved.key.chunk.x),
           static_cast<int>(saved.key.chunk.z)});
      if (found == d.chunks.end() ||
          found->second.state != WorldChunkState3D::SavingDelta ||
          found->second.deltaIoRequest != saved.requestId ||
          found->second.request.epoch != saved.epoch) {
        ++stats_.staleDeltaCompletions;
        continue;
      }
      auto &entry = found->second;
      entry.deltaIoRequest = 0;
      if (!saved.durable ||
          saved.generation < entry.delta.durableGeneration) {
        ++stats_.deltaSaveFailures;
        continue;
      }
      entry.dirty = false;
      entry.retirement = d.target.BeginRetire(entry.activation);
      entry.state = WorldChunkState3D::Retiring;
      stats_.residentGpuBytes =
          stats_.residentGpuBytes >= entry.activation.gpuBytes
              ? stats_.residentGpuBytes - entry.activation.gpuBytes
              : 0;
      stats_.retiringGpuBytes += entry.activation.gpuBytes;
      ++stats_.deltaSaves;
    }
  }
  for (auto iterator = d.chunks.begin(); iterator != d.chunks.end();) {
    auto &entry = iterator->second;
    if (entry.state == WorldChunkState3D::Resident ||
        entry.state == WorldChunkState3D::SavingDelta) {
      ++iterator;
      continue;
    }
    if (entry.state == WorldChunkState3D::Retiring) {
      if (!d.target.IsRetirementComplete(entry.retirement)) {
        ++iterator;
        continue;
      }
      stats_.retiringGpuBytes =
          stats_.retiringGpuBytes >= entry.activation.gpuBytes
              ? stats_.retiringGpuBytes - entry.activation.gpuBytes
              : 0;
    }
    iterator = d.chunks.erase(iterator);
  }
  if (!d.chunks.empty()) {
    stats_.queued = stats_.loading = stats_.loadingDelta = stats_.cpuReady =
        stats_.activating = stats_.resident = stats_.savingDelta =
            stats_.retiring = stats_.failed = 0;
    stats_.inflightLoads = 0;
    view_.clear();
    for (const auto &[coordinate, entry] : d.chunks) {
      if (entry.state == WorldChunkState3D::Resident)
        ++stats_.resident;
      else if (entry.state == WorldChunkState3D::SavingDelta)
        ++stats_.savingDelta;
      else if (entry.state == WorldChunkState3D::Retiring)
        ++stats_.retiring;
      view_.push_back({coordinate, entry.state, entry.request.requestId,
                       entry.request.revision});
    }
    return;
  }
  const auto staleDeltaCompletions = stats_.staleDeltaCompletions;
  stats_ = {};
  stats_.epoch = epoch;
  stats_.nextRequestId = 1;
  stats_.requestedSourceWorkers = d.settings.workerCount;
  stats_.staleDeltaCompletions = staleDeltaCompletions;
  view_.clear();
}
void WorldPartition3D::WaitIdle() {
  if (!impl_)
    return;
  auto &d = *impl_;
  if (!d.hasObserver) {
    for (std::size_t iteration = 0; iteration < 64; ++iteration) {
      if (d.chunks.empty())
        return;
      Reset();
    }
    throw std::runtime_error(
        "World partition has unfinished durable retirements");
  }
  for (size_t iteration = 0; iteration < 4096; ++iteration) {
    d.source.WaitIdle();
    if (d.settings.persistence)
      d.settings.persistence->WaitIdle();
    d.target.WaitIdle();
    Update(d.lastObserver);
    if (!stats_.queued && !stats_.loading && !stats_.loadingDelta &&
        !stats_.cpuReady && !stats_.activating && !stats_.savingDelta &&
        !stats_.inflightLoads)
      return;
  }
  throw std::runtime_error("World partition failed to become idle");
}
std::size_t WorldPartition3D::TrimUnusedMemory() {
  if (!impl_)
    return 0;
  const size_t persistenceBytes =
      impl_->settings.persistence
          ? impl_->settings.persistence->TrimUnusedMemory()
          : 0;
  const size_t bytes = impl_->source.TrimUnusedMemory() +
                       impl_->target.TrimUnusedMemory() + persistenceBytes;
  std::vector<WorldPartitionChunkStatus3D>(view_).swap(view_);
  return bytes;
}
} // namespace Aevrynd
