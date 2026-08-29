#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <variant>
#include <vector>

using namespace Aevrynd;

namespace {
StableObjectId3D Id(std::uint64_t value) {
  return {0x9e3779b97f4a7c15ull, value};
}

WorldChunkDeltaSnapshot3D Snapshot() {
  WorldChunkDeltaSnapshot3D snapshot;
  snapshot.key = {
      Id(1), 7, 3, {-17, 4, (std::numeric_limits<std::int64_t>::max)()}};
  snapshot.base = {91, 2, 0x123456789abcdef0ull, 44, {}};
  for (std::size_t index = 0; index < snapshot.base.contentHash.size();
       ++index) {
    snapshot.base.contentHash[index] = std::byte(index);
  }
  snapshot.durableGeneration = 14;
  snapshot.lastSequence = 4;

  PersistentBuilding3D building;
  building.id = Id(20);
  building.assetId = 300;
  building.materialId = 400;
  building.position = {{-17, 4, (std::numeric_limits<std::int64_t>::max)()},
                       {-15.5f, 2.25f, 15.5f}};
  building.rotation = {0.1f, 0.2f, 0.3f};
  building.scale = {1.0f, 2.0f, 3.0f};
  building.collisionPreset = 5;
  snapshot.operations = {
      {1, 11, 101, Id(101), PlaceBuildingDelta3D{building}},
      {2, 12, 102, Id(102), RemoveBuildingDelta3D{Id(21)}},
      {3, 13, 103, Id(103), RemoveVegetationDelta3D{Id(22)}},
      {4, 14, 104, Id(104), SetTerrainHeightDelta3D{15, 31, 123.25f}},
  };
  return snapshot;
}

bool SameOperation(const WorldChunkDeltaOperation3D &left,
                   const WorldChunkDeltaOperation3D &right) {
  if (left.sequence != right.sequence ||
      left.dirtyGeneration != right.dirtyGeneration ||
      left.serverTick != right.serverTick ||
      left.operationId != right.operationId ||
      left.payload.index() != right.payload.index()) {
    return false;
  }
  if (const auto *value = std::get_if<PlaceBuildingDelta3D>(&left.payload)) {
    const auto *other = std::get_if<PlaceBuildingDelta3D>(&right.payload);
    const auto &a = value->building;
    const auto &b = other->building;
    return other && a.id == b.id && a.assetId == b.assetId &&
           a.materialId == b.materialId && a.position == b.position &&
           a.rotation.x == b.rotation.x && a.rotation.y == b.rotation.y &&
           a.rotation.z == b.rotation.z && a.scale.x == b.scale.x &&
           a.scale.y == b.scale.y && a.scale.z == b.scale.z &&
           a.collisionPreset == b.collisionPreset;
  }
  if (const auto *value = std::get_if<RemoveBuildingDelta3D>(&left.payload)) {
    const auto *other = std::get_if<RemoveBuildingDelta3D>(&right.payload);
    return other && value->id == other->id;
  }
  if (const auto *value = std::get_if<RemoveVegetationDelta3D>(&left.payload)) {
    const auto *other = std::get_if<RemoveVegetationDelta3D>(&right.payload);
    return other && value->id == other->id;
  }
  const auto *value = std::get_if<SetTerrainHeightDelta3D>(&left.payload);
  const auto *other = std::get_if<SetTerrainHeightDelta3D>(&right.payload);
  return value && other && value->x == other->x && value->z == other->z &&
         value->height == other->height;
}
} // namespace

int main() {
  const WorldChunkDeltaSnapshot3D source = Snapshot();
  std::vector<std::byte> encoded;
  if (!EncodeWorldChunkDeltaSnapshot3D(source, encoded) ||
      encoded.size() < 32) {
    std::cerr << "Encoding all four delta operations failed\n";
    return 1;
  }
  std::vector<std::byte> repeated;
  if (!EncodeWorldChunkDeltaSnapshot3D(source, repeated) ||
      repeated != encoded) {
    std::cerr << "Canonical encoding was not byte deterministic\n";
    return 2;
  }
  WorldChunkDeltaSnapshot3D decoded;
  if (!DecodeWorldChunkDeltaSnapshot3D(encoded, decoded) ||
      decoded.key != source.key || !(decoded.base == source.base) ||
      decoded.durableGeneration != source.durableGeneration ||
      decoded.lastSequence != source.lastSequence ||
      decoded.operations.size() != source.operations.size()) {
    std::cerr << "Version one round trip failed\n";
    return 3;
  }
  for (std::size_t index = 0; index < source.operations.size(); ++index) {
    if (!SameOperation(source.operations[index], decoded.operations[index])) {
      std::cerr << "Typed operation round trip failed\n";
      return 4;
    }
  }

  const auto stable = DeriveStableObjectId3D(
      source.key.world, source.key.chunk, source.base.contentRevision, 2, 99);
  if (stable != DeriveStableObjectId3D(source.key.world, source.key.chunk,
                                       source.base.contentRevision, 2, 99) ||
      stable == DeriveStableObjectId3D(source.key.world, source.key.chunk,
                                       source.base.contentRevision, 2, 100)) {
    std::cerr << "Stable object id determinism failed\n";
    return 5;
  }

  auto invalid = source;
  std::get<SetTerrainHeightDelta3D>(invalid.operations.back().payload).height =
      (std::numeric_limits<float>::quiet_NaN)();
  std::vector<std::byte> preserved{std::byte{0x5a}};
  if (EncodeWorldChunkDeltaSnapshot3D(invalid, preserved)) {
    std::cerr << "Non-finite terrain height was accepted\n";
    return 6;
  }
  invalid = source;
  invalid.operations[1].sequence = invalid.operations[0].sequence;
  if (EncodeWorldChunkDeltaSnapshot3D(invalid, preserved)) {
    std::cerr << "Duplicate sequence was accepted\n";
    return 7;
  }
  invalid = source;
  invalid.operations[2].sequence = 1;
  if (EncodeWorldChunkDeltaSnapshot3D(invalid, preserved)) {
    std::cerr << "Out-of-order sequence was accepted\n";
    return 8;
  }
  if (EncodeWorldChunkDeltaSnapshot3D(source, preserved, encoded.size() - 1)) {
    std::cerr << "Oversized snapshot length was accepted\n";
    return 9;
  }

  auto corrupted = encoded;
  corrupted.front() ^= std::byte{1};
  if (DecodeWorldChunkDeltaSnapshot3D(corrupted, decoded)) {
    std::cerr << "Corrupt magic was accepted\n";
    return 10;
  }
  corrupted = encoded;
  corrupted.back() ^= std::byte{1};
  if (DecodeWorldChunkDeltaSnapshot3D(corrupted, decoded)) {
    std::cerr << "CRC corruption was accepted\n";
    return 11;
  }
  corrupted.assign(encoded.begin(), encoded.end() - 1);
  if (DecodeWorldChunkDeltaSnapshot3D(corrupted, decoded)) {
    std::cerr << "Truncated length was accepted\n";
    return 12;
  }
  if (DecodeWorldChunkDeltaSnapshot3D(encoded, decoded, encoded.size() - 1)) {
    std::cerr << "Decode maximum length was not enforced\n";
    return 13;
  }

  const std::array<std::byte, 4> crcFixture{std::byte{'1'}, std::byte{'2'},
                                            std::byte{'3'}, std::byte{'4'}};
  if (WorldDeltaCrc32C3D(crcFixture) != 0xf63af4eeu) {
    std::cerr << "CRC32C golden value failed\n";
    return 14;
  }
  std::cout
      << "ops=4 canonical=1 corruption=magic+crc+length stable_id=1 bounds=1\n";
  return 0;
}
