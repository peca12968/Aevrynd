#pragma once
#include "WorldOrigin3D.h"
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace Aevrynd {
struct StableObjectId3D {
  std::uint64_t high{}, low{};
  constexpr auto operator<=>(const StableObjectId3D &) const = default;
};
struct WorldDeltaChunkKey3D {
  StableObjectId3D world{};
  std::uint32_t layer{}, chunkSizeRevision{1};
  GlobalChunkCoordinate3D chunk{};
  constexpr auto operator<=>(const WorldDeltaChunkKey3D &) const = default;
};
struct WorldChunkBaseRevision3D {
  std::uint64_t generatorId{}, generatorVersion{}, worldSeed{},
      contentRevision{};
  std::array<std::byte, 32> contentHash{};
  constexpr bool operator==(const WorldChunkBaseRevision3D &) const = default;
};
struct PersistentBuilding3D {
  StableObjectId3D id{};
  std::uint64_t assetId{}, materialId{};
  GlobalPosition3D position{};
  Vec3 rotation{}, scale{1, 1, 1};
  std::uint32_t collisionPreset{};
};
struct PlaceBuildingDelta3D {
  PersistentBuilding3D building{};
};
struct RemoveBuildingDelta3D {
  StableObjectId3D id{};
};
struct RemoveVegetationDelta3D {
  StableObjectId3D id{};
};
struct SetTerrainHeightDelta3D {
  std::uint32_t x{}, z{};
  float height{};
};
using WorldChunkDeltaPayload3D =
    std::variant<PlaceBuildingDelta3D, RemoveBuildingDelta3D,
                 RemoveVegetationDelta3D, SetTerrainHeightDelta3D>;
struct WorldChunkDeltaOperation3D {
  std::uint64_t sequence{}, dirtyGeneration{}, serverTick{};
  StableObjectId3D operationId{};
  WorldChunkDeltaPayload3D payload{};
};
struct WorldChunkDeltaSnapshot3D {
  WorldDeltaChunkKey3D key{};
  WorldChunkBaseRevision3D base{};
  std::uint64_t durableGeneration{}, lastSequence{};
  std::vector<WorldChunkDeltaOperation3D> operations{};
};
enum class WorldDeltaLoadStatus3D : std::uint8_t {
  Clean,
  Loaded,
  BaseMismatch,
  Corrupt,
  IoError,
  Cancelled
};
struct WorldDeltaLoadResult3D {
  std::uint64_t requestId{}, epoch{};
  WorldDeltaChunkKey3D key{};
  WorldDeltaLoadStatus3D status{WorldDeltaLoadStatus3D::Clean};
  WorldChunkDeltaSnapshot3D snapshot{};
  std::string diagnostic{};
};
struct WorldDeltaSaveResult3D {
  std::uint64_t requestId{}, epoch{}, generation{};
  WorldDeltaChunkKey3D key{};
  bool durable{};
  std::string diagnostic{};
};
struct WorldDeltaStoreSettings3D {
  std::filesystem::path root{};
  std::size_t maximumQueuedRequests{256}, maximumQueuedBytes{16 * 1024 * 1024},
      maximumSnapshotBytes{8 * 1024 * 1024};
  bool enabled{true};
};
struct WorldDeltaStoreStats3D {
  std::size_t queuedRequests{}, queuedBytes{}, peakQueuedBytes{},
      retainedBytes{};
  std::uint64_t loads{}, saves{}, durableSaves{}, rejected{}, corruptions{},
      tornTails{}, truncatedBytes{}, trimmedBytes{};
};

std::uint32_t WorldDeltaCrc32C3D(std::span<const std::byte> bytes) noexcept;
StableObjectId3D DeriveStableObjectId3D(StableObjectId3D world,
                                        GlobalChunkCoordinate3D chunk,
                                        std::uint64_t revision,
                                        std::uint64_t type,
                                        std::uint64_t ordinal) noexcept;
bool EncodeWorldChunkDeltaSnapshot3D(const WorldChunkDeltaSnapshot3D &snapshot,
                                     std::vector<std::byte> &bytes,
                                     std::size_t maximumBytes = 8 * 1024 *
                                                                1024);
bool DecodeWorldChunkDeltaSnapshot3D(std::span<const std::byte> bytes,
                                     WorldChunkDeltaSnapshot3D &snapshot,
                                     std::size_t maximumBytes = 8 * 1024 *
                                                                1024);

class WorldChunkDeltaStore3D {
public:
  explicit WorldChunkDeltaStore3D(WorldDeltaStoreSettings3D settings = {});
  ~WorldChunkDeltaStore3D();
  WorldChunkDeltaStore3D(const WorldChunkDeltaStore3D &) = delete;
  WorldChunkDeltaStore3D &operator=(const WorldChunkDeltaStore3D &) = delete;
  bool EnqueueLoad(std::uint64_t requestId, std::uint64_t epoch,
                   WorldDeltaChunkKey3D key, WorldChunkBaseRevision3D base);
  bool EnqueueSave(std::uint64_t requestId, std::uint64_t epoch,
                   const WorldChunkDeltaSnapshot3D &snapshot);
  bool TryPopLoad(WorldDeltaLoadResult3D &result);
  bool TryPopSave(WorldDeltaSaveResult3D &result);
  void WaitIdle();
  void Reset();
  std::size_t TrimUnusedMemory();
  WorldDeltaStoreStats3D Stats() const;
  static bool RecoverFileForTesting(const std::filesystem::path &journal,
                                    WorldChunkDeltaSnapshot3D &snapshot,
                                    std::size_t maximumBytes = 8 * 1024 * 1024);

private:
  struct Request {
    enum class Kind { Load, Save };
    Kind kind{};
    std::uint64_t requestId{}, epoch{};
    WorldDeltaChunkKey3D key{};
    WorldChunkBaseRevision3D base{};
    WorldChunkDeltaSnapshot3D snapshot{};
    std::size_t bytes{};
  };
  void Worker(std::stop_token stop);
  WorldDeltaStoreSettings3D settings_{};
  mutable std::mutex mutex_{};
  std::condition_variable_any wake_{}, idle_{};
  std::deque<Request> queue_{};
  std::deque<WorldDeltaLoadResult3D> loads_{};
  std::deque<WorldDeltaSaveResult3D> saves_{};
  std::jthread worker_{};
  std::size_t active_{};
  WorldDeltaStoreStats3D stats_{};
  std::uint64_t resetEpoch_{1};
};
} // namespace Aevrynd
