#include "WorldPartition3DFakeFixtures.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace Aevrynd;
using namespace Aevrynd::Tests;

namespace {
StableObjectId3D Id(std::uint64_t value) {
  return {0x6a09e667f3bcc909ull, value};
}

WorldChunkBaseRevision3D Base() {
  WorldChunkBaseRevision3D base{17, 3, 0x12345678, 9, {}};
  for (std::size_t index = 0; index < base.contentHash.size(); ++index)
    base.contentHash[index] = std::byte(index + 1);
  return base;
}

WorldDeltaChunkKey3D Key(WorldChunkCoordinate3D coordinate) {
  return {Id(1), 2, 4, {coordinate.x, 0, coordinate.z}};
}

WorldChunkDeltaSnapshot3D Snapshot(WorldChunkCoordinate3D coordinate,
                                   std::uint64_t generation) {
  WorldChunkDeltaSnapshot3D snapshot;
  snapshot.key = Key(coordinate);
  snapshot.base = Base();
  snapshot.durableGeneration = generation;
  snapshot.lastSequence = 1;
  snapshot.operations.push_back(
      {1, generation, 100 + generation, Id(1000 + generation),
       SetTerrainHeightDelta3D{3, 5, float(generation)}});
  return snapshot;
}

WorldDeltaStoreSettings3D StoreSettings(const std::filesystem::path &root,
                                        std::size_t maximumSnapshotBytes =
                                            256 * 1024) {
  WorldDeltaStoreSettings3D settings;
  settings.root = root;
  settings.maximumQueuedRequests = 16;
  settings.maximumQueuedBytes = 1024 * 1024;
  settings.maximumSnapshotBytes = maximumSnapshotBytes;
  return settings;
}

WorldPartitionSettings3D PartitionSettings(WorldChunkDeltaStore3D &store) {
  WorldPartitionSettings3D settings;
  settings.spatial = {32, 0, 0, 1, 1, 91};
  settings.workerCount = 1;
  settings.maximumInflightLoads = 1;
  settings.maximumCpuReadyBytes = 4096;
  settings.maximumGpuResidentBytes = 4096;
  settings.maximumPreparationBytesPerUpdate = 4096;
  settings.maximumActivationBytesPerUpdate = 4096;
  settings.persistence = &store;
  settings.persistenceWorld = Id(1);
  settings.persistenceLayer = 2;
  settings.persistenceChunkSizeRevision = 4;
  settings.persistenceBase = Base();
  return settings;
}

class PersistentActivationTarget final : public IWorldChunkActivationTarget3D {
public:
  std::size_t PreparationBytes(WorldChunkRequest3D request,
                               const WorldChunkPayload3D &payload) const override {
    return fake.PreparationBytes(request, payload);
  }
  std::size_t
  PhysicalAllocationBytes(WorldChunkRequest3D request,
                          const WorldChunkPayload3D &payload) const override {
    return fake.PhysicalAllocationBytes(request, payload);
  }
  WorldChunkActivation3D
  Activate(WorldChunkRequest3D request,
           const WorldChunkPayload3D &payload) override {
    return fake.Activate(request, payload);
  }
  WorldChunkActivation3D
  ActivatePersistent(WorldChunkRequest3D request,
                     const WorldChunkPayload3D &payload,
                     const WorldChunkDeltaSnapshot3D *delta) override {
    ++persistentCalls;
    receivedDelta = delta != nullptr;
    if (delta)
      lastDelta = *delta;
    return fake.Activate(request, payload);
  }
  bool IsActivationComplete(WorldChunkActivation3D activation) const override {
    return fake.IsActivationComplete(activation);
  }
  void Rollback(WorldChunkActivation3D activation) override {
    fake.Rollback(activation);
  }
  WorldChunkActivation3D
  BeginRetire(WorldChunkActivation3D activation) override {
    if (requireDurableBeforeRetire && durabilityProbe &&
        durabilityProbe->Stats().durableSaves < minimumDurableSaves)
      retiredBeforeDurable = true;
    return fake.BeginRetire(activation);
  }
  bool IsRetirementComplete(WorldChunkActivation3D retirement) const override {
    return fake.IsRetirementComplete(retirement);
  }
  void WaitIdle() override { fake.WaitIdle(); }
  std::size_t TrimUnusedMemory() override {
    return fake.TrimUnusedMemory();
  }
  void CompleteActivations() { fake.CompleteActivations(); }
  void CompleteRetirements() { fake.CompleteRetirements(); }
  void HoldRetirements() { fake.HoldRetirements(); }

  FakeActivationTarget3D fake;
  std::size_t persistentCalls{};
  bool receivedDelta{};
  bool requireDurableBeforeRetire{}, retiredBeforeDurable{};
  std::uint64_t minimumDurableSaves{};
  WorldChunkDeltaStore3D *durabilityProbe{};
  WorldChunkDeltaSnapshot3D lastDelta{};
};

std::filesystem::path FindSnapshotFile(const std::filesystem::path &root) {
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file() &&
        entry.path().filename() == "snapshot.aewdelta")
      return entry.path();
  return {};
}

bool LoadToResident(WorldPartition3D &partition, FakeChunkSource3D &source,
                    PersistentActivationTarget &target,
                    WorldChunkDeltaStore3D &store, bool expectedDelta) {
  partition.Update({0, 0, 0});
  source.CompleteAll();
  partition.Update({0, 0, 0});
  if (partition.Stats().loadingDelta != 1 || target.persistentCalls)
    return false;
  store.WaitIdle();
  partition.Update({0, 0, 0});
  if (target.persistentCalls != 1 || target.receivedDelta != expectedDelta)
    return false;
  target.CompleteActivations();
  partition.Update({0, 0, 0});
  return partition.Stats().resident == 1;
}

struct TemporaryRoot {
  std::filesystem::path path;
  ~TemporaryRoot() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }
};
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("aevrynd-world-partition-delta-gate-" +
                     std::to_string(GetCurrentProcessId()));
  TemporaryRoot cleanup{root};
  std::error_code error;
  std::filesystem::remove_all(root, error);

  {
    WorldChunkDeltaStore3D store(StoreSettings(root / "loaded"));
    const auto persisted = Snapshot({0, 0}, 7);
    if (!store.EnqueueSave(99, 99, persisted))
      return 1;
    store.WaitIdle();
    WorldDeltaSaveResult3D receipt;
    if (!store.TryPopSave(receipt) || !receipt.durable)
      return 2;
    FakeChunkSource3D source;
    PersistentActivationTarget target;
    WorldPartition3D partition(source, target, PartitionSettings(store));
    if (!LoadToResident(partition, source, target, store, true) ||
        target.lastDelta.durableGeneration != 7 ||
        target.lastDelta.operations.size() != 1 ||
        partition.Stats().deltaLoads != 1) {
      std::cerr << "Delta load did not gate activation\n";
      return 3;
    }
  }

  {
    WorldChunkDeltaStore3D store(StoreSettings(root / "clean"));
    FakeChunkSource3D source;
    PersistentActivationTarget target;
    target.CompleteRetirements();
    WorldPartition3D partition(source, target, PartitionSettings(store));
    if (!LoadToResident(partition, source, target, store, false))
      return 4;
    partition.Update({64, 0, 0});
    if (target.fake.retires != 1 || store.Stats().saves ||
        partition.Stats().deltaSaves) {
      std::cerr << "Clean chunk unexpectedly entered delta save\n";
      return 5;
    }
  }

  {
    WorldChunkDeltaStore3D store(StoreSettings(root / "dirty"));
    FakeChunkSource3D source;
    PersistentActivationTarget target;
    target.HoldRetirements();
    WorldPartition3D partition(source, target, PartitionSettings(store));
    if (!LoadToResident(partition, source, target, store, false) ||
        !partition.SetChunkDelta({0, 0}, Snapshot({0, 0}, 2)))
      return 6;
    partition.Update({64, 0, 0});
    if (partition.Stats().savingDelta != 1 || target.fake.retires) {
      std::cerr << "Dirty chunk retired before a durable receipt\n";
      return 7;
    }
    store.WaitIdle();
    partition.Update({64, 0, 0});
    if (target.fake.retires != 1 || partition.Stats().deltaSaves != 1 ||
        store.Stats().durableSaves != 1) {
      std::cerr << "Durable receipt did not release the retirement gate\n";
      return 8;
    }
  }

  {
    WorldChunkDeltaStore3D store(StoreSettings(root / "failed", 128));
    FakeChunkSource3D source;
    PersistentActivationTarget target;
    WorldPartition3D partition(source, target, PartitionSettings(store));
    if (!LoadToResident(partition, source, target, store, false))
      return 9;
    if (partition.SetChunkDelta({0, 0}, Snapshot({0, 0}, 3)))
      return 10;
    partition.Update({64, 0, 0});
    if (!store.Stats().rejected || target.fake.retires != 1 ||
        partition.Stats().savingDelta) {
      std::cerr << "Backpressure changed gameplay state or blocked clean retire\n";
      return 11;
    }
  }

  {
    WorldChunkDeltaStore3D store(StoreSettings(root / "stale"));
    FakeChunkSource3D source;
    PersistentActivationTarget target;
    WorldPartition3D partition(source, target, PartitionSettings(store));
    partition.Update({0, 0, 0});
    source.CompleteAll();
    partition.Update({0, 0, 0});
    if (partition.Stats().loadingDelta != 1)
      return 11;
    store.WaitIdle();
    partition.Reset();
    partition.Update({64, 0, 0});
    if (partition.Stats().staleDeltaCompletions != 1 ||
        target.persistentCalls) {
      std::cerr << "Stale epoch/request delta completion was accepted\n";
      return 12;
    }
  }

  {
    WorldChunkDeltaStore3D store(StoreSettings(root / "dirty-reset"));
    FakeChunkSource3D source;
    PersistentActivationTarget target;
    target.durabilityProbe = &store;
    target.requireDurableBeforeRetire = true;
    target.minimumDurableSaves = 1;
    WorldPartition3D partition(source, target, PartitionSettings(store));
    if (!LoadToResident(partition, source, target, store, false) ||
        !partition.SetChunkDelta({0, 0}, Snapshot({0, 0}, 4)))
      return 13;
    partition.Reset();
    if (target.fake.retires != 1 || target.retiredBeforeDurable ||
        store.Stats().durableSaves != 1 || !partition.Chunks().empty()) {
      std::cerr << "Dirty Reset retired before durable completion retires="
                << target.fake.retires
                << " before=" << target.retiredBeforeDurable
                << " saves=" << store.Stats().saves
                << " durable=" << store.Stats().durableSaves
                << " failures=" << partition.Stats().deltaSaveFailures
                << " saving=" << partition.Stats().savingDelta
                << " chunks=" << partition.Chunks().size() << '\n';
      return 14;
    }
  }

  {
    const auto failureRoot = root / "reset-failure";
    WorldChunkDeltaStore3D store(StoreSettings(failureRoot));
    if (!store.EnqueueSave(91, 91, Snapshot({0, 0}, 1)))
      return 15;
    store.WaitIdle();
    WorldDeltaSaveResult3D receipt;
    if (!store.TryPopSave(receipt) || !receipt.durable)
      return 16;
    const auto snapshotFile = FindSnapshotFile(failureRoot);
    if (snapshotFile.empty() ||
        !SetFileAttributesW(snapshotFile.c_str(), FILE_ATTRIBUTE_READONLY))
      return 17;
    FakeChunkSource3D source;
    PersistentActivationTarget target;
    WorldPartition3D partition(source, target, PartitionSettings(store));
    if (!LoadToResident(partition, source, target, store, true) ||
        !partition.SetChunkDelta({0, 0}, Snapshot({0, 0}, 2))) {
      SetFileAttributesW(snapshotFile.c_str(), FILE_ATTRIBUTE_NORMAL);
      return 18;
    }
    partition.Reset();
    if (partition.Stats().savingDelta != 1 ||
        partition.Chunks().size() != 1 || target.fake.retires) {
      SetFileAttributesW(snapshotFile.c_str(), FILE_ATTRIBUTE_NORMAL);
      std::cerr << "Failed Reset did not retain the SavingDelta record\n";
      return 19;
    }
    bool waitThrew{};
    try {
      partition.WaitIdle();
    } catch (const std::runtime_error &) {
      waitThrew = true;
    }
    if (!waitThrew || partition.Stats().savingDelta != 1 ||
        partition.Chunks().size() != 1 || target.fake.retires) {
      SetFileAttributesW(snapshotFile.c_str(), FILE_ATTRIBUTE_NORMAL);
      std::cerr << "Permanent save failure did not fail closed\n";
      return 20;
    }
    SetFileAttributesW(snapshotFile.c_str(), FILE_ATTRIBUTE_NORMAL);
    partition.Reset();
    if (!partition.Chunks().empty() || target.fake.retires != 1) {
      std::cerr << "Recovered save failure did not finish Reset\n";
      return 21;
    }
  }

  {
    WorldChunkDeltaStore3D store(StoreSettings(root / "clean-reset"));
    FakeChunkSource3D source;
    PersistentActivationTarget target;
    WorldPartition3D partition(source, target, PartitionSettings(store));
    if (!LoadToResident(partition, source, target, store, false))
      return 22;
    partition.Reset();
    if (target.fake.retires != 1 || !partition.Chunks().empty() ||
        store.Stats().saves) {
      std::cerr << "Clean resident Reset failed\n";
      return 23;
    }
  }

  {
    WorldChunkDeltaStore3D store(StoreSettings(root / "clean-retiring"));
    FakeChunkSource3D source;
    PersistentActivationTarget target;
    target.HoldRetirements();
    WorldPartition3D partition(source, target, PartitionSettings(store));
    if (!LoadToResident(partition, source, target, store, false))
      return 24;
    partition.Update({64, 0, 0});
    if (partition.Stats().retiring != 1 || target.fake.retires != 1)
      return 25;
    partition.Reset();
    if (!partition.Chunks().empty()) {
      std::cerr << "Existing clean retirement did not finish during Reset\n";
      return 26;
    }
  }

  {
    WorldChunkDeltaStore3D store(StoreSettings(root / "reset-cycles"));
    for (std::uint64_t cycle = 1; cycle <= 128; ++cycle) {
      FakeChunkSource3D source;
      PersistentActivationTarget target;
      target.durabilityProbe = &store;
      target.requireDurableBeforeRetire = true;
      target.minimumDurableSaves = cycle;
      WorldPartition3D partition(source, target, PartitionSettings(store));
      if (!LoadToResident(partition, source, target, store, cycle != 1) ||
          !partition.SetChunkDelta({0, 0}, Snapshot({0, 0}, cycle))) {
        std::cerr << "Dirty Reset cycle setup failed at cycle=" << cycle
                  << '\n';
        return 27;
      }
      partition.Reset();
      if (target.fake.retires != 1 || target.retiredBeforeDurable ||
          !partition.Chunks().empty() ||
          store.Stats().durableSaves != cycle) {
        std::cerr << "Dirty Reset cycle failed at cycle=" << cycle << '\n';
        return 28;
      }
    }
  }

  std::cout << "load_before_activate=1 clean_no_save=1 durable_before_retire=1 "
               "failed_save_closed=1 stale_epoch_request=1 reset_dirty=durable "
               "reset_failure=closed wait_idle=throws clean_reset=1 "
               "dirty_reset_cycles=128\n";
  return 0;
}
