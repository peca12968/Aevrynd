#include <Aevrynd/WorldPartitionTerrainVegetationAdapter3D.h>
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace Aevrynd;

namespace {
StableObjectId3D Id(std::uint64_t low) {
  return {0x6a09e667f3bcc909ull, low};
}

WorldChunkBaseRevision3D BaseRevision() {
  WorldChunkBaseRevision3D base{21, 4, 0x12345678, 12, {}};
  for (std::size_t index = 0; index < base.contentHash.size(); ++index)
    base.contentHash[index] = std::byte(index + 3);
  return base;
}

TerrainVegetationChunkSettings3D Settings() {
  TerrainVegetationChunkSettings3D settings;
  settings.chunkSize = 32;
  settings.terrainCells = 8;
  settings.terrainAmplitude = 5;
  settings.vegetationPerChunk = 12;
  settings.worldId = Id(77);
  settings.baseRevision = BaseRevision();
  return settings;
}

bool Load(TerrainVegetationWorldChunkSource3D &source,
          WorldChunkRequest3D request,
          TerrainVegetationChunkPayload3D &payload) {
  if (!source.BeginLoad(request, {}))
    return false;
  source.WaitIdle();
  WorldChunkLoadCompletion3D completion;
  if (!source.TryPopCompletion(completion) || !completion.succeeded ||
      !completion.payload.data)
    return false;
  const auto *typed = static_cast<const TerrainVegetationChunkPayload3D *>(
      completion.payload.data.get());
  if (!typed)
    return false;
  payload = *typed;
  return true;
}

WorldChunkDeltaOperation3D Operation(std::uint64_t sequence,
                                     WorldChunkDeltaPayload3D payload) {
  return {sequence, 1, 100 + sequence, Id(1000 + sequence),
          std::move(payload)};
}

WorldChunkDeltaSnapshot3D Snapshot(
    const TerrainVegetationChunkPayload3D &base) {
  WorldChunkDeltaSnapshot3D snapshot;
  snapshot.key = {Id(77), 2, 1, base.globalChunk};
  snapshot.base = base.baseRevision;
  snapshot.durableGeneration = 1;
  return snapshot;
}

PersistentBuilding3D Building(std::uint64_t id, float localX) {
  PersistentBuilding3D building;
  building.id = Id(id);
  building.assetId = 1;
  building.materialId = 2;
  building.position = {{-7, 0, 5}, {localX, 3, 6}};
  building.rotation = {0, .25f, 0};
  building.scale = {1, 2, 1};
  building.collisionPreset = 3;
  return building;
}

bool SameObservableOutput(const ComposedTerrainVegetationChunk3D &left,
                          const ComposedTerrainVegetationChunk3D &right) {
  if (left.payload.globalChunk != right.payload.globalChunk ||
      left.payload.vegetationIds != right.payload.vegetationIds ||
      left.payload.vegetation.size() != right.payload.vegetation.size() ||
      left.payload.terrain.vertices.size() !=
          right.payload.terrain.vertices.size() ||
      left.buildings.size() != right.buildings.size())
    return false;
  for (std::size_t index = 0;
       index < left.payload.terrain.vertices.size(); ++index) {
    const auto &a = left.payload.terrain.vertices[index];
    const auto &b = right.payload.terrain.vertices[index];
    if (a.x != b.x || a.y != b.y || a.z != b.z)
      return false;
  }
  const auto &leftField = left.payload.heightField;
  const auto &rightField = right.payload.heightField;
  if (leftField.Settings().width != rightField.Settings().width ||
      leftField.Settings().depth != rightField.Settings().depth)
    return false;
  for (unsigned z = 0; z < leftField.Settings().depth; ++z)
    for (unsigned x = 0; x < leftField.Settings().width; ++x)
      if (leftField.Height(x, z) != rightField.Height(x, z))
        return false;
  for (std::size_t index = 0; index < left.buildings.size(); ++index)
    if (left.buildings[index].id != right.buildings[index].id)
      return false;
  return true;
}
} // namespace

int main() {
  const auto settings = Settings();
  const WorldChunkCoordinate3D coordinate{-7, 5};
  const std::uint32_t seed = 0x31415926;
  TerrainVegetationChunkPayload3D first, second, reloaded;
  {
    TerrainVegetationWorldChunkSource3D source(settings, 1);
    if (!Load(source, {coordinate, seed, 1, 11, 1}, first) ||
        !Load(source, {coordinate, seed, 1, 22, 2}, second)) {
      std::cerr << "Stable vegetation fixture load failed\n";
      return 1;
    }
  }
  {
    TerrainVegetationWorldChunkSource3D source(settings, 1);
    if (!Load(source, {coordinate, seed, 1, 33, 1}, reloaded)) {
      std::cerr << "Reloaded vegetation fixture load failed\n";
      return 2;
    }
  }
  if (first.vegetationIds.empty() ||
      first.vegetationIds != second.vegetationIds ||
      first.vegetationIds != reloaded.vegetationIds) {
    std::cerr << "Vegetation stable IDs changed with requestId or reload\n";
    return 3;
  }
  for (std::size_t index = 0; index < first.vegetation.size(); ++index)
    if (first.vegetation[index].key != first.vegetationIds[index].low) {
      std::cerr << "Runtime vegetation key does not use the stable ID\n";
      return 4;
    }

  auto delta = Snapshot(first);
  delta.operations.push_back(
      Operation(1, RemoveVegetationDelta3D{first.vegetationIds[3]}));
  delta.operations.push_back(
      Operation(2, SetTerrainHeightDelta3D{2, 3, 91.25f}));
  const auto placedThenRemoved = Building(501, 4);
  const auto retainedBuilding = Building(502, 12);
  delta.operations.push_back(
      Operation(3, PlaceBuildingDelta3D{placedThenRemoved}));
  delta.operations.push_back(
      Operation(4, PlaceBuildingDelta3D{retainedBuilding}));
  delta.operations.push_back(
      Operation(5, RemoveBuildingDelta3D{placedThenRemoved.id}));
  delta.lastSequence = 5;

  ComposedTerrainVegetationChunk3D composed;
  if (!ComposeTerrainVegetationChunkDelta3D(first, &delta, composed) ||
      composed.payload.vegetation.size() + 1 != first.vegetation.size() ||
      composed.payload.vegetationIds.size() + 1 !=
          first.vegetationIds.size() ||
      std::find(composed.payload.vegetationIds.begin(),
                composed.payload.vegetationIds.end(),
                first.vegetationIds[3]) !=
          composed.payload.vegetationIds.end()) {
    std::cerr << "Persistent vegetation removal was not composed\n";
    return 5;
  }
  const std::size_t editedVertex = 3 * (settings.terrainCells + 1) + 2;
  if (editedVertex >= composed.payload.terrain.vertices.size() ||
      std::abs(composed.payload.terrain.vertices[editedVertex].y - 91.25f) >
          .0001f) {
    std::cerr << "Absolute terrain height did not update mesh vertex Y\n";
    return 6;
  }
  if (composed.buildings.size() != 1 ||
      composed.buildings.front().id != retainedBuilding.id) {
    std::cerr << "Building place/remove fold was not atomic\n";
    return 7;
  }

  const auto west =
      CanonicalTerrainSampleOwner3D({-7, 0, -5}, 0, 4, 8);
  const auto north =
      CanonicalTerrainSampleOwner3D({-7, 0, -5}, 4, 0, 8);
  const auto corner =
      CanonicalTerrainSampleOwner3D({-7, 0, -5}, 0, 0, 8);
  const auto interior =
      CanonicalTerrainSampleOwner3D({-7, 0, -5}, 8, 8, 8);
  if (west != GlobalChunkCoordinate3D{-8, 0, -5} ||
      north != GlobalChunkCoordinate3D{-7, 0, -6} ||
      corner != GlobalChunkCoordinate3D{-8, 0, -6} ||
      interior != GlobalChunkCoordinate3D{-7, 0, -5}) {
    std::cerr << "Negative-coordinate canonical boundary owner failed\n";
    return 8;
  }

  ComposedTerrainVegetationChunk3D baseline;
  if (!ComposeTerrainVegetationChunkDelta3D(first, nullptr, baseline))
    return 9;
  baseline.buildings.push_back(Building(900, 1));
  baseline.payload.terrain.vertices.front().y = -4321;
  for (int checkpoint = 0; checkpoint < 4; ++checkpoint) {
    auto failing = Snapshot(first);
    if (checkpoint >= 1)
      failing.operations.push_back(
          Operation(1, SetTerrainHeightDelta3D{1, 1, 45}));
    if (checkpoint >= 2)
      failing.operations.push_back(
          Operation(2, RemoveVegetationDelta3D{first.vegetationIds[0]}));
    if (checkpoint >= 3)
      failing.operations.push_back(
          Operation(3, PlaceBuildingDelta3D{Building(700, 2)}));
    if (checkpoint == 0)
      failing.operations.push_back(Operation(
          1, SetTerrainHeightDelta3D{settings.terrainCells + 1, 0, 5}));
    else if (checkpoint == 1)
      failing.operations.push_back(
          Operation(2, RemoveVegetationDelta3D{Id(0xdead)}));
    else
      failing.operations.push_back(
          Operation(std::uint64_t(checkpoint + 1),
                    RemoveBuildingDelta3D{Id(0xbeef)}));
    auto output = baseline;
    if (ComposeTerrainVegetationChunkDelta3D(first, &failing, output) ||
        !SameObservableOutput(output, baseline)) {
      std::cerr << "Compose rollback did not restore baseline checkpoint="
                << checkpoint << '\n';
      return 10;
    }
  }

  std::cout << "stable_vegetation_ids=request_reload removed_vegetation=filtered "
               "terrain_height=absolute building_fold=atomic "
               "negative_boundary=canonical rollback_checkpoints=4\n";
  return 0;
}
