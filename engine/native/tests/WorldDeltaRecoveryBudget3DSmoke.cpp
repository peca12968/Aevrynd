#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <Windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace Aevrynd;

namespace {
StableObjectId3D Id(std::uint64_t value) {
  return {0xbb67ae8584caa73bull, value};
}

WorldChunkBaseRevision3D Base(std::uint64_t revision = 7) {
  WorldChunkBaseRevision3D base{11, 2, 0x12345678, revision, {}};
  for (std::size_t index = 0; index < base.contentHash.size(); ++index)
    base.contentHash[index] = std::byte(index * 5);
  return base;
}

WorldDeltaChunkKey3D Key(std::int64_t x = 0, std::int64_t z = 0) {
  return {Id(1), 3, 2, {x, 0, z}};
}

WorldChunkDeltaSnapshot3D Snapshot(std::uint64_t generation,
                                   WorldDeltaChunkKey3D key = Key(),
                                   WorldChunkBaseRevision3D base = Base()) {
  WorldChunkDeltaSnapshot3D snapshot{key, base, generation, 1, {}};
  snapshot.operations.push_back(
      {1, generation, generation, Id(100 + generation),
       SetTerrainHeightDelta3D{4, 9, float(generation)}});
  return snapshot;
}

WorldDeltaStoreSettings3D Settings(const std::filesystem::path &root) {
  WorldDeltaStoreSettings3D settings;
  settings.root = root;
  settings.maximumQueuedRequests = 16;
  settings.maximumQueuedBytes = 16 * 1024 * 1024;
  settings.maximumSnapshotBytes = 1024 * 1024;
  settings.maximumCompletedResults = 16;
  settings.maximumCompletedBytes = 20 * 1024 * 1024;
  return settings;
}

struct Files {
  std::filesystem::path snapshot, journal;
};

Files FindFiles(const std::filesystem::path &root) {
  Files files;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file())
      continue;
    if (entry.path().filename() == "snapshot.aewdelta")
      files.snapshot = entry.path();
    else if (entry.path().filename() == "journal.aewdelta")
      files.journal = entry.path();
  }
  return files;
}

std::vector<std::byte> Read(const std::filesystem::path &path) {
  std::vector<std::byte> bytes(
      static_cast<std::size_t>(std::filesystem::file_size(path)));
  std::ifstream stream(path, std::ios::binary);
  stream.read(reinterpret_cast<char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  return bytes;
}

void Write(const std::filesystem::path &path,
           const std::vector<std::byte> &bytes) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

void Put32(std::vector<std::byte> &bytes, std::size_t at,
           std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes[at++] = std::byte((value >> shift) & 0xff);
}

std::vector<std::byte> CurrentFrame(const WorldChunkDeltaSnapshot3D &snapshot) {
  std::vector<std::byte> payload;
  if (!EncodeWorldChunkDeltaSnapshot3D(snapshot, payload))
    return {};
  std::vector<std::byte> frame{
      std::byte{'A'}, std::byte{'E'}, std::byte{'W'}, std::byte{'D'},
      std::byte{'L'}, std::byte{'T'}, std::byte{'A'}, std::byte{'2'},
  };
  auto append64 = [&](std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8)
      frame.push_back(std::byte((value >> shift) & 0xff));
  };
  append64(payload.size());
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

std::vector<std::byte> LegacyFrame(const WorldChunkDeltaSnapshot3D &snapshot) {
  std::vector<std::byte> payload;
  if (!EncodeWorldChunkDeltaSnapshot3D(snapshot, payload))
    return {};
  payload[7] = std::byte{'1'};
  Put32(payload, payload.size() - 4,
        WorldDeltaCrc32C3D(std::span(payload).first(payload.size() - 4)));
  std::vector<std::byte> frame{
      std::byte{'A'}, std::byte{'E'}, std::byte{'W'}, std::byte{'D'},
      std::byte{'L'}, std::byte{'T'}, std::byte{'A'}, std::byte{'1'},
  };
  const auto length = std::uint64_t(payload.size());
  for (unsigned shift = 0; shift < 64; shift += 8)
    frame.push_back(std::byte((length >> shift) & 0xff));
  const auto crc = WorldDeltaCrc32C3D(payload);
  for (unsigned shift = 0; shift < 32; shift += 8)
    frame.push_back(std::byte((crc >> shift) & 0xff));
  frame.insert(frame.end(), payload.begin(), payload.end());
  return frame;
}

bool Save(WorldChunkDeltaStore3D &store,
          const WorldChunkDeltaSnapshot3D &snapshot,
          std::uint64_t request = 1) {
  if (!store.EnqueueSave(request, 1, snapshot))
    return false;
  store.WaitIdle();
  WorldDeltaSaveResult3D result;
  return store.TryPopSave(result) && result.durable;
}

WorldDeltaLoadResult3D Load(WorldChunkDeltaStore3D &store,
                            WorldDeltaChunkKey3D key = Key(),
                            WorldChunkBaseRevision3D base = Base(),
                            std::uint64_t request = 1) {
  WorldDeltaLoadResult3D result;
  if (!store.EnqueueLoad(request, 1, key, base)) {
    result.status = WorldDeltaLoadStatus3D::Cancelled;
    return result;
  }
  store.WaitIdle();
  store.TryPopLoad(result);
  return result;
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
                    ("aevrynd-delta-recovery-budget-" +
                     std::to_string(GetCurrentProcessId()));
  TemporaryRoot cleanup{root};
  std::error_code error;
  std::filesystem::remove_all(root, error);

  for (int mode = 0; mode < 5; ++mode) {
    const auto caseRoot = root / ("files-" + std::to_string(mode));
    WorldChunkDeltaStore3D store(Settings(caseRoot));
    if (mode != 2 && !Save(store, Snapshot(1)))
      return 1;
    const auto files = mode == 2 ? Files{} : FindFiles(caseRoot);
    if (mode == 0)
      std::filesystem::remove(files.journal);
    else if (mode == 1)
      std::filesystem::remove(files.snapshot);
    else if (mode == 3) {
      auto bytes = Read(files.snapshot);
      bytes[52] ^= std::byte{1};
      Write(files.snapshot, bytes);
    } else if (mode == 4) {
      auto snapshotBytes = Read(files.snapshot);
      auto journalBytes = Read(files.journal);
      snapshotBytes[52] ^= std::byte{1};
      journalBytes[52] ^= std::byte{1};
      Write(files.snapshot, snapshotBytes);
      Write(files.journal, journalBytes);
    }
    const auto loaded = Load(store);
    const auto expected = mode == 2 ? WorldDeltaLoadStatus3D::Clean
                          : mode == 4 ? WorldDeltaLoadStatus3D::Corrupt
                                      : WorldDeltaLoadStatus3D::Loaded;
    if (loaded.status != expected ||
        (expected == WorldDeltaLoadStatus3D::Loaded &&
         loaded.snapshot.durableGeneration != 1)) {
      std::cerr << "Snapshot/journal recovery matrix failed mode=" << mode
                << '\n';
      return 2;
    }
  }

  {
    const auto caseRoot = root / "restart";
    WorldChunkDeltaStore3D store(Settings(caseRoot));
    if (!Save(store, Snapshot(1)))
      return 3;
    const auto files = FindFiles(caseRoot);
    auto journal = Read(files.journal);
    journal[52] ^= std::byte{1};
    Write(files.journal, journal);
    for (std::uint64_t restart = 1; restart <= 3; ++restart) {
      const auto loaded = Load(store, Key(), Base(), restart);
      if (loaded.status != WorldDeltaLoadStatus3D::Loaded ||
          loaded.snapshot.durableGeneration != 1)
        return 4;
    }
  }

  {
    const auto caseRoot = root / "rename-failure";
    {
      WorldChunkDeltaStore3D store(Settings(caseRoot));
      if (!Save(store, Snapshot(1)))
        return 5;
      const auto files = FindFiles(caseRoot);
      if (!SetFileAttributesW(files.snapshot.c_str(), FILE_ATTRIBUTE_READONLY))
        return 6;
      if (!store.EnqueueSave(2, 1, Snapshot(2)))
        return 7;
      store.WaitIdle();
      WorldDeltaSaveResult3D result;
      if (!store.TryPopSave(result) || result.durable) {
        SetFileAttributesW(files.snapshot.c_str(), FILE_ATTRIBUTE_NORMAL);
        return 8;
      }
      SetFileAttributesW(files.snapshot.c_str(), FILE_ATTRIBUTE_NORMAL);
    }
    WorldChunkDeltaStore3D restarted(Settings(caseRoot));
    const auto loaded = Load(restarted);
    if (loaded.status != WorldDeltaLoadStatus3D::Loaded ||
        loaded.snapshot.durableGeneration != 1)
      return 9;
  }

  {
    const auto caseRoot = root / "legacy";
    WorldChunkDeltaStore3D seed(Settings(caseRoot));
    if (!Save(seed, Snapshot(1)))
      return 10;
    const auto files = FindFiles(caseRoot);
    const auto legacy = LegacyFrame(Snapshot(1));
    Write(files.snapshot, legacy);
    Write(files.journal, legacy);
    const auto loaded = Load(seed);
    if (loaded.status != WorldDeltaLoadStatus3D::Loaded ||
        !Save(seed, Snapshot(2), 2) || Read(files.snapshot)[7] != std::byte{'2'})
      return 11;
  }

  {
    const auto caseRoot = root / "identity";
    WorldChunkDeltaStore3D store(Settings(caseRoot));
    if (!Save(store, Snapshot(1)))
      return 12;
    const auto files = FindFiles(caseRoot);
    const auto wrongKey = CurrentFrame(Snapshot(1, Key(99, 99)));
    Write(files.snapshot, wrongKey);
    Write(files.journal, wrongKey);
    if (Load(store).status != WorldDeltaLoadStatus3D::Corrupt)
      return 13;
  }

  {
    const auto caseRoot = root / "base-identity";
    WorldChunkDeltaStore3D store(Settings(caseRoot));
    if (!Save(store, Snapshot(1)))
      return 14;
    const auto files = FindFiles(caseRoot);
    const auto wrongBase = CurrentFrame(Snapshot(1, Key(), Base(99)));
    Write(files.snapshot, wrongBase);
    Write(files.journal, wrongBase);
    if (Load(store).status != WorldDeltaLoadStatus3D::BaseMismatch)
      return 15;
  }

  {
    const auto caseRoot = root / "digest";
    WorldChunkDeltaStore3D store(Settings(caseRoot));
    if (!Save(store, Snapshot(1)))
      return 16;
    const auto files = FindFiles(caseRoot);
    std::filesystem::remove(files.snapshot);
    auto bytes = Read(files.journal);
    bytes.back() ^= std::byte{1};
    const auto payload = std::span(bytes).subspan(84);
    Put32(bytes, 16, WorldDeltaCrc32C3D(payload));
    Write(files.journal, bytes);
    if (Load(store).status != WorldDeltaLoadStatus3D::Corrupt)
      return 17;
  }

  {
    const auto caseRoot = root / "same-generation-divergence";
    WorldChunkDeltaStore3D store(Settings(caseRoot));
    if (!Save(store, Snapshot(5)))
      return 18;
    const auto files = FindFiles(caseRoot);
    auto divergent = Snapshot(5);
    std::get<SetTerrainHeightDelta3D>(divergent.operations.front().payload)
        .height += 1.f;
    Write(files.journal, CurrentFrame(divergent));
    if (Load(store).status != WorldDeltaLoadStatus3D::Corrupt)
      return 19;
  }

  {
    WorldDeltaStoreSettings3D settings;
    settings.root = root / "default-four-loads";
    WorldChunkDeltaStore3D store(settings);
    for (std::uint64_t index = 0; index < 4; ++index)
      if (!store.EnqueueLoad(index + 1, 1, Key(index, 0), Base()))
        return 20;
    store.WaitIdle();
    WorldDeltaLoadResult3D result;
    for (int index = 0; index < 4; ++index)
      if (!store.TryPopLoad(result) ||
          result.status != WorldDeltaLoadStatus3D::Clean)
        return 20;
  }

  {
    const auto caseRoot = root / "journal-rotation";
    WorldChunkDeltaStore3D store(Settings(caseRoot));
    for (std::uint64_t generation = 1; generation <= 20; ++generation)
      if (!Save(store, Snapshot(generation), generation))
        return 21;
    const auto files = FindFiles(caseRoot);
    if (std::filesystem::file_size(files.journal) !=
            std::filesystem::file_size(files.snapshot) ||
        Load(store).snapshot.durableGeneration != 20)
      return 22;
  }

  {
    auto settings = Settings(root / "stall");
    settings.maximumCompletedResults = 2;
    settings.minimumConcurrentLoadCompletions = 2;
    WorldChunkDeltaStore3D store(settings);
    if (!store.EnqueueLoad(1, 1, Key(1, 0), Base()) ||
        !store.EnqueueLoad(2, 1, Key(2, 0), Base()) ||
        store.EnqueueLoad(3, 1, Key(3, 0), Base()))
      return 23;
    store.WaitIdle();
    auto stats = store.Stats();
    if (stats.completedResults != 2 || stats.completedBytes == 0 ||
        stats.completedBytes > settings.maximumCompletedBytes)
      return 24;
    WorldDeltaLoadResult3D result;
    if (!store.TryPopLoad(result) ||
        !store.EnqueueLoad(3, 1, Key(3, 0), Base()))
      return 25;
    store.WaitIdle();
    while (store.TryPopLoad(result)) {
    }
    store.Reset();
    if (store.Stats().retainedBytes || store.TrimUnusedMemory() != 0)
      return 26;
  }

  {
    auto undersized = Settings(root / "undersized-completions");
    undersized.maximumCompletedResults = 3;
    WorldChunkDeltaStore3D resultBounded(undersized);
    if (resultBounded.EnqueueLoad(1, 1, Key(), Base()))
      return 27;

    undersized = Settings(root / "undersized-completion-bytes");
    undersized.maximumCompletedBytes =
        undersized.minimumConcurrentLoadCompletions *
            (undersized.maximumSnapshotBytes +
             sizeof(WorldDeltaLoadResult3D) + 256) -
        1;
    WorldChunkDeltaStore3D byteBounded(undersized);
    if (byteBounded.EnqueueLoad(1, 1, Key(), Base()))
      return 28;

    auto overflow = Settings(root / "overflow-completions");
    overflow.maximumSnapshotBytes =
        (std::numeric_limits<std::size_t>::max)();
    overflow.maximumCompletedBytes =
        (std::numeric_limits<std::size_t>::max)();
    WorldChunkDeltaStore3D overflowBounded(overflow);
    if (overflowBounded.EnqueueLoad(1, 1, Key(), Base()))
      return 29;
  }

  std::cout << "files=snapshot+journal+missing recovery=restart3 legacy=v1-to-v2 "
               "commit_failure=old digest_chain=1 identity=closed "
               "same_generation=closed default_loads=4 journal=rotated "
               "completion_stall=bounded constructor_budget=checked trim=0\n";
  return 0;
}
