#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace Aevrynd;
static_assert(sizeof(StableObjectId3D) == 16);
static_assert(StableObjectId3D{1, 8} < StableObjectId3D{1, 9});
static_assert(StableObjectId3D{1, 9} < StableObjectId3D{2, 0});
static_assert(StableObjectIdScalarHash3D({1, 9}) ==
              StableObjectIdScalarHash3D(StableObjectId3D{1, 9}));

namespace {
StableObjectId3D Id(std::uint64_t value) {
  return {0x243f6a8885a308d3ull, value};
}

WorldChunkBaseRevision3D Base(std::uint64_t revision = 8) {
  WorldChunkBaseRevision3D base{3, 2, 0xabcdef, revision, {}};
  for (std::size_t index = 0; index < base.contentHash.size(); ++index) {
    base.contentHash[index] = std::byte(index * 3);
  }
  return base;
}

WorldChunkDeltaSnapshot3D Snapshot(WorldDeltaChunkKey3D key,
                                   std::uint64_t generation) {
  WorldChunkDeltaSnapshot3D snapshot{key, Base(), generation, 4, {}};
  PersistentBuilding3D building{
      Id(100 + generation), 7, 9, {key.chunk, {1, 2, 3}}, {}, {1, 1, 1}, 2};
  snapshot.operations = {
      {1, generation, 1, Id(generation * 10 + 1),
       PlaceBuildingDelta3D{building}},
      {2, generation, 2, Id(generation * 10 + 2),
       RemoveBuildingDelta3D{Id(200)}},
      {3, generation, 3, Id(generation * 10 + 3),
       RemoveVegetationDelta3D{Id(300)}},
      {4, generation, 4, Id(generation * 10 + 4),
       SetTerrainHeightDelta3D{2, 5, float(generation)}},
  };
  return snapshot;
}

template <class Result, class Pop> bool PopOne(Pop &&pop, Result &result) {
  return pop(result);
}

bool IsBelowRoot(const std::filesystem::path &root,
                 const std::filesystem::path &candidate) {
  const auto normalizedRoot = std::filesystem::weakly_canonical(root).wstring();
  const auto normalizedCandidate =
      std::filesystem::weakly_canonical(candidate).wstring();
  return normalizedCandidate.size() >= normalizedRoot.size() &&
         std::equal(normalizedRoot.begin(), normalizedRoot.end(),
                    normalizedCandidate.begin(),
                    [](wchar_t left, wchar_t right) {
                      return towlower(left) == towlower(right);
                    });
}

std::filesystem::path FindJournal(const std::filesystem::path &root,
                                  const WorldDeltaChunkKey3D &key) {
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file() ||
        entry.path().filename() != "journal.aewdelta")
      continue;
    WorldChunkDeltaSnapshot3D recovered;
    if (WorldChunkDeltaStore3D::RecoverFileForTesting(entry.path(),
                                                      recovered) &&
        recovered.key == key) {
      return entry.path();
    }
  }
  return {};
}

std::filesystem::path FindSnapshotFile(const std::filesystem::path &root,
                                       const WorldDeltaChunkKey3D &key) {
  const auto journal = FindJournal(root, key);
  return journal.empty() ? std::filesystem::path{}
                         : journal.parent_path() / "snapshot.aewdelta";
}

std::vector<std::byte> ReadBytes(const std::filesystem::path &path) {
  const auto size = std::filesystem::file_size(path);
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  std::ifstream input(path, std::ios::binary);
  input.read(reinterpret_cast<char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  return bytes;
}

void WriteBytes(const std::filesystem::path &path,
                const std::vector<std::byte> &bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::byte>
FrameSnapshot(const WorldChunkDeltaSnapshot3D &snapshot) {
  std::vector<std::byte> payload;
  if (!EncodeWorldChunkDeltaSnapshot3D(snapshot, payload))
    return {};
  std::vector<std::byte> frame{
      std::byte{'A'}, std::byte{'E'}, std::byte{'W'}, std::byte{'D'},
      std::byte{'L'}, std::byte{'T'}, std::byte{'A'}, std::byte{'2'},
  };
  const std::uint64_t length = payload.size();
  for (unsigned shift = 0; shift < 64; shift += 8)
    frame.push_back(std::byte((length >> shift) & 0xff));
  const std::uint32_t crc = WorldDeltaCrc32C3D(payload);
  for (unsigned shift = 0; shift < 32; shift += 8)
    frame.push_back(std::byte((crc >> shift) & 0xff));
  const std::array<std::byte, 32> parent{};
  frame.insert(frame.end(), parent.begin(), parent.end());
  std::vector<std::byte> chained(parent.begin(), parent.end());
  chained.insert(chained.end(), payload.begin(), payload.end());
  std::array<std::byte, 32> digest{};
  if (!WorldDeltaSha2563D(chained, digest))
    return {};
  frame.insert(frame.end(), digest.begin(), digest.end());
  frame.insert(frame.end(), payload.begin(), payload.end());
  return frame;
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
  if (StableObjectIdScalarHash3D({1, 8}) ==
          StableObjectIdScalarHash3D({1, 9}) ||
      StableObjectIdHash3D{}({1, 9}) !=
          static_cast<std::size_t>(StableObjectIdScalarHash3D({1, 9}))) {
    std::cerr << "Stable ID scalar hash contract failed\n";
    return 90;
  }
  const auto root =
      std::filesystem::temp_directory_path() /
      ("aevrynd-world-delta-" + std::to_string(GetCurrentProcessId()));
  TemporaryRoot cleanup{root};
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);
  if (error)
    return 1;

  const WorldDeltaChunkKey3D keyA{Id(1),
                                  0,
                                  1,
                                  {-(std::numeric_limits<std::int64_t>::max)(),
                                   0,
                                   (std::numeric_limits<std::int64_t>::max)()}};
  const WorldDeltaChunkKey3D keyB{Id(1), 0, 1, {42, -7, -99}};
  {
    WorldDeltaStoreSettings3D settings;
    settings.root = root;
    settings.maximumQueuedRequests = 8;
    settings.maximumQueuedBytes = 1024 * 1024;
    settings.maximumSnapshotBytes = 256 * 1024;
    WorldChunkDeltaStore3D store(settings);

    if (!store.EnqueueSave(10, 1, Snapshot(keyA, 1)) ||
        !store.EnqueueSave(11, 1, Snapshot(keyB, 1))) {
      std::cerr << "Two chunk save admission failed\n";
      return 2;
    }
    store.WaitIdle();
    WorldDeltaSaveResult3D savedA, savedB;
    if (!PopOne([&](auto &value) { return store.TryPopSave(value); }, savedA) ||
        !PopOne([&](auto &value) { return store.TryPopSave(value); }, savedB) ||
        !savedA.durable || !savedB.durable || savedA.key == savedB.key) {
      std::cerr << "Two chunk durable receipts failed\n";
      return 3;
    }

    if (!store.EnqueueLoad(20, 2, keyA, Base()))
      return 4;
    store.WaitIdle();
    WorldDeltaLoadResult3D loaded;
    if (!store.TryPopLoad(loaded) ||
        loaded.status != WorldDeltaLoadStatus3D::Loaded ||
        loaded.snapshot.durableGeneration != 1 ||
        loaded.snapshot.operations.size() != 4) {
      std::cerr << "Durable file reload failed\n";
      return 5;
    }
    const auto journalA = FindJournal(root, keyA);
    if (journalA.empty()) {
      std::cerr << "Chunk journal was not created\n";
      return 15;
    }
    const auto validJournalBytes = std::filesystem::file_size(journalA);
    {
      std::ofstream tail(journalA, std::ios::binary | std::ios::app);
      const std::array<char, 7> partial{'A', 'E', 'W', 'D', 'L', 'T', 'A'};
      tail.write(partial.data(), static_cast<std::streamsize>(partial.size()));
    }
    if (!store.EnqueueLoad(22, 2, keyA, Base()))
      return 16;
    store.WaitIdle();
    if (!store.TryPopLoad(loaded) ||
        loaded.status != WorldDeltaLoadStatus3D::Loaded ||
        std::filesystem::file_size(journalA) != validJournalBytes ||
        store.Stats().tornTails != 1 || store.Stats().truncatedBytes != 7) {
      std::cerr
          << "Truncated journal tail was not recovered to the valid prefix\n";
      return 17;
    }

    const auto frame = ReadBytes(journalA);
    if (frame.size() < 20)
      return 18;
    auto corrupt = frame;
    corrupt.front() ^= std::byte{1};
    const auto badMagic = root / "bad-magic.aewdelta";
    WriteBytes(badMagic, corrupt);
    WorldChunkDeltaSnapshot3D recovered;
    if (WorldChunkDeltaStore3D::RecoverFileForTesting(badMagic, recovered)) {
      std::cerr << "Journal magic corruption was accepted\n";
      return 19;
    }
    corrupt = frame;
    std::fill(corrupt.begin() + 8, corrupt.begin() + 16, std::byte{0xff});
    const auto badLength = root / "bad-length.aewdelta";
    WriteBytes(badLength, corrupt);
    if (WorldChunkDeltaStore3D::RecoverFileForTesting(badLength, recovered)) {
      std::cerr << "Journal length corruption was accepted\n";
      return 20;
    }
    corrupt = frame;
    corrupt[16] ^= std::byte{1};
    const auto badCrc = root / "bad-crc.aewdelta";
    WriteBytes(badCrc, corrupt);
    if (WorldChunkDeltaStore3D::RecoverFileForTesting(badCrc, recovered)) {
      std::cerr << "Journal CRC corruption was accepted\n";
      return 21;
    }

    const auto tornMatrix = root / "torn-matrix.aewdelta";
    for (std::size_t cut = 0; cut < frame.size(); ++cut) {
      const std::vector<std::byte> incomplete(
          frame.begin(), frame.begin() + static_cast<std::ptrdiff_t>(cut));
      WriteBytes(tornMatrix, incomplete);
      WorldChunkDeltaSnapshot3D incompleteRecovered;
      const bool incompleteAccepted =
          WorldChunkDeltaStore3D::RecoverFileForTesting(tornMatrix,
                                                        incompleteRecovered);
      if ((cut == 0) != incompleteAccepted) {
        std::cerr << "Incomplete first frame recovery failed at cut=" << cut
                  << '\n';
        return 22;
      }
      auto prefix = frame;
      prefix.insert(prefix.end(), frame.begin(),
                    frame.begin() + static_cast<std::ptrdiff_t>(cut));
      WriteBytes(tornMatrix, prefix);
      WorldChunkDeltaSnapshot3D matrixRecovered;
      if (!WorldChunkDeltaStore3D::RecoverFileForTesting(tornMatrix,
                                                         matrixRecovered) ||
          matrixRecovered.key != keyA ||
          matrixRecovered.durableGeneration != 1) {
        std::cerr << "Per-byte torn tail recovery failed at cut=" << cut
                  << '\n';
        return 31;
      }
    }
    auto duplicateFrames = frame;
    duplicateFrames.insert(duplicateFrames.end(), frame.begin(), frame.end());
    const auto duplicateJournal = root / "duplicate-frame.aewdelta";
    WriteBytes(duplicateJournal, duplicateFrames);
    if (WorldChunkDeltaStore3D::RecoverFileForTesting(duplicateJournal,
                                                      recovered)) {
      std::cerr << "Duplicate frame bypassed the digest chain\n";
      return 23;
    }
    auto older = Snapshot(keyA, 2);
    older.operations.resize(3);
    older.lastSequence = 3;
    const auto olderFrame = FrameSnapshot(older);
    auto outOfOrderFrames = frame;
    outOfOrderFrames.insert(outOfOrderFrames.end(), olderFrame.begin(),
                            olderFrame.end());
    const auto outOfOrderJournal = root / "out-of-order-frame.aewdelta";
    WriteBytes(outOfOrderJournal, outOfOrderFrames);
    if (WorldChunkDeltaStore3D::RecoverFileForTesting(outOfOrderJournal,
                                                      recovered)) {
      std::cerr << "Out-of-order frame replay was accepted\n";
      return 24;
    }

    auto mismatched = Base();
    ++mismatched.contentRevision;
    if (!store.EnqueueLoad(21, 2, keyA, mismatched))
      return 6;
    store.WaitIdle();
    if (!store.TryPopLoad(loaded) ||
        loaded.status != WorldDeltaLoadStatus3D::BaseMismatch) {
      std::cerr << "Base mismatch did not fail closed\n";
      return 7;
    }

    for (std::uint64_t cycle = 2; cycle <= 129; ++cycle) {
      if (!store.EnqueueSave(1000 + cycle, 3, Snapshot(keyB, cycle))) {
        std::cerr << "Repeated save cycle was rejected\n";
        return 8;
      }
      store.WaitIdle();
      WorldDeltaSaveResult3D receipt;
      if (!store.TryPopSave(receipt) || !receipt.durable ||
          receipt.generation != cycle) {
        std::cerr << "Repeated durable save cycle failed\n";
        return 9;
      }
    }
    const auto beforeTrim = store.Stats();
    store.TrimUnusedMemory();
    store.TrimUnusedMemory();
    const auto afterTrim = store.Stats();
    if (!beforeTrim.peakQueuedBytes ||
        beforeTrim.peakQueuedBytes > settings.maximumQueuedBytes ||
        afterTrim.queuedBytes || afterTrim.queuedRequests ||
        afterTrim.retainedBytes > beforeTrim.retainedBytes) {
      std::cerr << "Trim did not preserve idle bounded accounting\n";
      return 10;
    }
  }

  {
    WorldDeltaStoreSettings3D settings;
    settings.root = root / "owner-namespace-wrap";
    settings.maximumQueuedRequests = 8;
    settings.maximumQueuedBytes = 1024 * 1024;
    settings.maximumSnapshotBytes = 256 * 1024;
    WorldChunkDeltaStore3D store(settings);
    if (!store.SetNextSaveOwnerNamespaceForTesting(
            (std::numeric_limits<std::uint64_t>::max)()))
      return 42;
    auto last = store.AcquireSaveOwner();
    if (!last ||
        last.OwnerNamespace() !=
            (std::numeric_limits<std::uint64_t>::max)() ||
        store.AcquireSaveOwner()) {
      std::cerr << "Owner namespace wrap did not fail closed\n";
      return 43;
    }
  }

  {
    WorldDeltaStoreSettings3D settings;
    settings.root = root / "owner-callback";
    settings.maximumQueuedRequests = 8;
    settings.maximumQueuedBytes = 1024 * 1024;
    settings.maximumSnapshotBytes = 256 * 1024;
    WorldChunkDeltaStore3D store(settings);
    struct CallbackState {
      std::atomic<unsigned> calls{};
      std::atomic<std::uint64_t> token{};
    };
    auto callback = std::make_shared<CallbackState>();
    auto late = store.AcquireSaveOwner(
        {callback, [](void *context,
                       const WorldDeltaSaveResult3D &receipt) noexcept {
           auto &state = *static_cast<CallbackState *>(context);
           state.token.store(receipt.requestId);
           state.calls.fetch_add(1);
         }});
    if (!late || !store.EnqueueSave(late, 71, 11, Snapshot(keyA, 301)))
      return 44;
    late = {};
    store.WaitIdle();
    if (callback->calls.load() != 1 || callback->token.load() != 71 ||
        store.Stats().reservedCompletedResults != 0) {
      std::cerr << "Detached late completion callback was not terminal\n";
      return 45;
    }

  }

  {
    WorldDeltaStoreSettings3D settings;
    settings.root = root / "owner-token-bounded";
    settings.maximumQueuedRequests = 8;
    settings.maximumQueuedBytes = 1024 * 1024;
    settings.maximumSnapshotBytes = 256 * 1024;
    WorldChunkDeltaStore3D store(settings);
    auto owner = store.AcquireSaveOwner();
    const auto boundedSnapshot = Snapshot(keyA, 304);
    if (!owner || !owner.EnqueueSave(100001, 13, boundedSnapshot))
      return 46;
    owner.WaitIdle();
    const auto retained = store.Stats().retainedBytes;
    for (std::uint64_t token = 1; token <= 100000; ++token)
      if (owner.EnqueueSave(token, 13, boundedSnapshot))
        return 47;
    if (store.Stats().retainedBytes != retained || store.TryReset()) {
      std::cerr << "Owner tokens grew memory or live-owner reset succeeded\n";
      return 48;
    }
    WorldDeltaSaveResult3D receipt;
    if (!owner.TryPopSave(receipt) || store.TryPopSave(receipt)) {
      std::cerr << "Owner/global completion isolation failed\n";
      return 49;
    }
  }

  {
    WorldDeltaStoreSettings3D settings;
    settings.root = root / "owner-completion-cap";
    settings.maximumQueuedRequests = 8;
    settings.maximumQueuedBytes = 1024 * 1024;
    settings.maximumSnapshotBytes = 256 * 1024;
    settings.maximumCompletedResults = 4;
    WorldChunkDeltaStore3D store(settings);
    auto owner = store.AcquireSaveOwner();
    if (!owner)
      return 37;
    for (std::uint64_t token = 1; token <= 4; ++token) {
      if (!store.EnqueueSave(owner, token, 10, Snapshot(keyA, 200 + token)))
        return 38;
    }
    store.WaitOwnerIdle(owner);
    if (store.EnqueueSave(owner, 5, 10, Snapshot(keyA, 205)) ||
        store.Stats().completedResults != 4 ||
        store.Stats().reservedCompletedResults != 4) {
      std::cerr << "Owner completion cap overflow was admitted\n";
      return 39;
    }
    WorldDeltaSaveResult3D receipt;
    if (!store.TryPopSave(owner, receipt) ||
        !store.EnqueueSave(owner, 5, 10, Snapshot(keyA, 205))) {
      std::cerr << "Owner completion capacity was not released\n";
      return 40;
    }
    store.WaitOwnerIdle(owner);
    while (store.TryPopSave(owner, receipt)) {}
  }

  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(root)) {
    if (!IsBelowRoot(root, entry.path())) {
      std::cerr << "Chunk path escaped the save root\n";
      return 11;
    }
  }

  {
    WorldDeltaStoreSettings3D bounded;
    bounded.root = root / "bounded";
    bounded.maximumQueuedRequests = 0;
    bounded.maximumQueuedBytes = 1;
    bounded.maximumSnapshotBytes = 1024;
    WorldChunkDeltaStore3D store(bounded);
    if (store.EnqueueSave(1, 1, Snapshot(keyA, 1)) ||
        store.Stats().rejected != 1 || store.Stats().queuedBytes ||
        store.Stats().queuedRequests) {
      std::cerr << "Backpressure did not reject atomically\n";
      return 12;
    }
  }

  {
    WorldDeltaStoreSettings3D disabled;
    disabled.root = root / "disabled";
    disabled.enabled = false;
    WorldChunkDeltaStore3D store(disabled);
    if (store.EnqueueLoad(1, 1, keyA, Base())) {
      std::cerr << "Disabled store admitted asynchronous work\n";
      return 13;
    }
    store.WaitIdle();
    WorldDeltaLoadResult3D result;
    if (store.TryPopLoad(result) || store.Stats().queuedBytes ||
        store.Stats().queuedRequests) {
      std::cerr << "Disabled store was not allocation-free\n";
      return 14;
    }
  }

  {
    const auto failureRoot = root / "rename-failure";
    WorldDeltaStoreSettings3D settings;
    settings.root = failureRoot;
    settings.maximumQueuedRequests = 2;
    settings.maximumQueuedBytes = 1024 * 1024;
    settings.maximumSnapshotBytes = 256 * 1024;
    WorldChunkDeltaStore3D store(settings);
    if (!store.EnqueueSave(1, 1, Snapshot(keyA, 1)))
      return 25;
    store.WaitIdle();
    WorldDeltaSaveResult3D receipt;
    if (!store.TryPopSave(receipt) || !receipt.durable)
      return 26;
    const auto snapshotFile = FindSnapshotFile(failureRoot, keyA);
    WorldChunkDeltaSnapshot3D oldSnapshot;
    if (snapshotFile.empty() ||
        !WorldChunkDeltaStore3D::RecoverFileForTesting(snapshotFile,
                                                       oldSnapshot) ||
        oldSnapshot.durableGeneration != 1 ||
        !SetFileAttributesW(snapshotFile.c_str(), FILE_ATTRIBUTE_READONLY)) {
      std::cerr << "Readonly snapshot fixture setup failed\n";
      return 27;
    }
    if (!store.EnqueueSave(2, 1, Snapshot(keyA, 2)))
      return 28;
    store.WaitIdle();
    if (!store.TryPopSave(receipt) || receipt.durable) {
      SetFileAttributesW(snapshotFile.c_str(), FILE_ATTRIBUTE_NORMAL);
      std::cerr << "Readonly snapshot rename failure reported durable\n";
      return 29;
    }
    WorldChunkDeltaSnapshot3D afterFailure;
    const bool oldPreserved =
        WorldChunkDeltaStore3D::RecoverFileForTesting(snapshotFile,
                                                      afterFailure) &&
        afterFailure.durableGeneration == 1;
    SetFileAttributesW(snapshotFile.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (!oldPreserved ||
        std::filesystem::exists(snapshotFile.wstring() + L".tmp")) {
      std::cerr << "Atomic rename failure did not preserve the old snapshot\n";
      return 30;
    }
  }

  {
    WorldDeltaStoreSettings3D settings;
    settings.root = root / "owner-routing";
    settings.maximumQueuedRequests = 8;
    settings.maximumQueuedBytes = 1024 * 1024;
    settings.maximumSnapshotBytes = 256 * 1024;
    WorldChunkDeltaStore3D store(settings);
    auto first = store.AcquireSaveOwner();
    auto second = store.AcquireSaveOwner();
    if (!first || !second ||
        first.OwnerNamespace() == second.OwnerNamespace() ||
        !store.EnqueueSave(first, 1, 7, Snapshot(keyA, 140)) ||
        !store.EnqueueSave(second, 1, 8, Snapshot(keyB, 141)) ||
        store.EnqueueSave(first, 1, 7, Snapshot(keyA, 142))) {
      std::cerr << "Save owner admission or lifetime token dedup failed\n";
      return 32;
    }
    store.WaitOwnerIdle(first);
    WorldDeltaSaveResult3D receipt;
    if (!store.TryPopSave(first, receipt) || receipt.requestId != 1 ||
        receipt.epoch != 7 || receipt.key != keyA ||
        store.TryPopSave(first, receipt) || store.TryPopSave(receipt)) {
      std::cerr << "First owner completion escaped its route\n";
      return 33;
    }
    store.WaitOwnerIdle(second);
    if (!store.TryPopSave(second, receipt) || receipt.requestId != 1 ||
        receipt.epoch != 8 || receipt.key != keyB ||
        store.EnqueueSave(second, 1, 8, Snapshot(keyB, 143))) {
      std::cerr << "Second owner completion or token namespace failed\n";
      return 34;
    }

    auto detached = store.AcquireSaveOwner();
    if (!detached ||
        !store.EnqueueSave(detached, 9, 9, Snapshot(keyA, 144)))
      return 35;
    detached = {};
    auto replacement = store.AcquireSaveOwner();
    if (!replacement ||
        !store.EnqueueSave(replacement, 9, 10, Snapshot(keyB, 145))) {
      std::cerr << "Detached owner namespace was not reusable\n";
      return 36;
    }
    store.WaitIdle();
    store.WaitOwnerIdle(replacement);
    if (!store.TryPopSave(replacement, receipt) || receipt.epoch != 10 ||
        receipt.key != keyB || store.Stats().completedResults != 0 ||
        store.Stats().reservedCompletedResults != 0) {
      std::cerr << "Detached late completion retained budget or namespace\n";
      return 41;
    }
  }

  std::filesystem::remove_all(root, error);
  std::cout << "chunks=2 ops=4 cycles=128 torn_tail=every_byte "
               "corruption=magic+crc+length duplicate=idempotent "
               "out_of_order=rejected atomic_rename=old_preserved "
               "base_mismatch=1 backpressure=1 trim=1 path_bounds=1 "
               "owner_routing=2 token_dedup=1 detach=1 owner_cap=4 "
               "owner_namespace_wrap=closed callback_late=1 sink=noexcept "
               "tokens=100000_bounded live_reset=closed\n";
  return 0;
}
