#include <Aevrynd/Scene3D.h>
#include <Aevrynd/WorldPartitionTerrainVegetationAdapter3D.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <windows.h>

using namespace Aevrynd;

namespace {
constexpr WorldChunkCoordinate3D Chunk{};
constexpr std::uint32_t PersistenceLayer = 7;
constexpr std::uint32_t ChunkSizeRevision = 3;
constexpr float EditedHeight = 11.75f;
constexpr float BoundaryHeight = 7.5f;
constexpr float NegativeBoundaryHeight = 6.25f;
constexpr float CornerHeight = 9.125f;

StableObjectId3D Id(std::uint64_t low) {
  return {0xbb67ae8584caa73bull, low};
}

WorldChunkBaseRevision3D BaseRevision() {
  WorldChunkBaseRevision3D base{31, 6, 0x5eed1234, 19, {}};
  for (std::size_t index = 0; index < base.contentHash.size(); ++index)
    base.contentHash[index] = std::byte(0x40 + index);
  return base;
}

TerrainVegetationChunkSettings3D FixtureSettings() {
  TerrainVegetationChunkSettings3D settings;
  settings.chunkSize = 16;
  settings.terrainCells = 8;
  settings.terrainAmplitude = 3;
  settings.vegetationPerChunk = 8;
  settings.vegetationLods = {Mesh3D::CreateUnitCone(12),
                            Mesh3D::CreateUnitCone(8),
                            Mesh3D::CreateUnitPyramid()};
  settings.worldId = Id(77);
  settings.baseRevision = BaseRevision();
  settings.buildingAssetId = 0xc001;
  settings.buildingMesh = Mesh3D::CreateUnitBox();
  return settings;
}

PersistentBuilding3D Building(
    const TerrainVegetationChunkSettings3D &settings,
    WorldChunkCoordinate3D coordinate) {
  PersistentBuilding3D building;
  building.id = Id(501);
  building.assetId = settings.buildingAssetId;
  building.materialId = 2;
  building.position = {{coordinate.x, 0, coordinate.z}, {8, 4, 8}};
  building.rotation = {0, .3f, 0};
  building.scale = {2, 3, 2};
  building.collisionPreset = 1;
  return building;
}

WorldChunkDeltaSnapshot3D Snapshot(
    const TerrainVegetationChunkSettings3D &settings,
    WorldChunkCoordinate3D coordinate) {
  const GlobalChunkCoordinate3D global{coordinate.x, 0, coordinate.z};
  WorldChunkDeltaSnapshot3D snapshot;
  snapshot.key = {settings.worldId, PersistenceLayer, ChunkSizeRevision,
                  global};
  snapshot.base = settings.baseRevision;
  snapshot.durableGeneration = 1;
  snapshot.lastSequence = 3;
  snapshot.operations = {
      {1, 1, 101, Id(1001), SetTerrainHeightDelta3D{4, 4, EditedHeight}},
      {2, 1, 102, Id(1002),
       RemoveVegetationDelta3D{DeriveStableObjectId3D(
           settings.worldId, global, settings.baseRevision.contentRevision, 3,
           2)}},
      {3, 1, 103, Id(1003), PlaceBuildingDelta3D{Building(settings, coordinate)}}};
  return snapshot;
}

WorldChunkDeltaSnapshot3D TerrainEditSnapshot(
    const TerrainVegetationChunkSettings3D &settings,
    WorldChunkCoordinate3D coordinate, std::uint32_t x, std::uint32_t z,
    float height, std::uint64_t operation) {
  WorldChunkDeltaSnapshot3D snapshot;
  snapshot.key = {settings.worldId, PersistenceLayer, ChunkSizeRevision,
                  {coordinate.x, 0, coordinate.z}};
  snapshot.base = settings.baseRevision;
  snapshot.durableGeneration = 1;
  snapshot.lastSequence = 1;
  snapshot.operations.push_back(
      {1, 1, 200 + operation, Id(operation),
       SetTerrainHeightDelta3D{x, z, height}});
  return snapshot;
}

WorldDeltaStoreSettings3D StoreSettings(const std::filesystem::path &root) {
  WorldDeltaStoreSettings3D settings;
  settings.root = root;
  settings.maximumQueuedRequests = 8;
  settings.maximumQueuedBytes = 1024 * 1024;
  settings.maximumSnapshotBytes = 256 * 1024;
  return settings;
}

std::filesystem::path FindSnapshotFile(const std::filesystem::path &root) {
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file() &&
        entry.path().filename() == "snapshot.aewdelta")
      return entry.path();
  return {};
}

std::filesystem::path FindJournalFile(const std::filesystem::path &root) {
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file() &&
        entry.path().filename() == "journal.aewdelta")
      return entry.path();
  return {};
}

WorldPartitionSettings3D PartitionSettings(
    WorldChunkDeltaStore3D &store,
    const TerrainVegetationChunkSettings3D &sourceSettings,
    std::uint32_t retainRadius = 1) {
  WorldPartitionSettings3D settings;
  settings.spatial = {sourceSettings.chunkSize, 0, retainRadius, 1, 1,
                      0x73451u};
  settings.workerCount = 1;
  settings.maximumInflightLoads = 1;
  settings.maximumCpuReadyBytes = 8 * 1024 * 1024;
  settings.maximumGpuResidentBytes = 8 * 1024 * 1024;
  settings.maximumPreparationBytesPerUpdate = 8 * 1024 * 1024;
  settings.maximumActivationBytesPerUpdate = 8 * 1024 * 1024;
  settings.persistence = &store;
  settings.persistenceWorld = sourceSettings.worldId;
  settings.persistenceLayer = PersistenceLayer;
  settings.persistenceChunkSizeRevision = ChunkSizeRevision;
  settings.persistenceBase = sourceSettings.baseRevision;
  return settings;
}

bool LoadPayload(const TerrainVegetationChunkSettings3D &settings,
                 WorldChunkCoordinate3D coordinate,
                 TerrainVegetationChunkPayload3D &payload) {
  TerrainVegetationWorldChunkSource3D source(settings, 1);
  const WorldChunkRequest3D request{coordinate, 0x73451u, 1, 91, 1};
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

struct TemporaryFiles {
  std::filesystem::path root;
  std::filesystem::path capture;
  ~TemporaryFiles() {
    std::error_code ignored;
    std::filesystem::remove(capture, ignored);
    std::filesystem::remove_all(root, ignored);
  }
};
} // namespace

class TerrainVegetationDeltaD3D12Game final : public IGame {
public:
  explicit TerrainVegetationDeltaD3D12Game(int &result) : result_(result) {}

  void OnStart(Engine &engine) override {
    const auto suffix = std::to_string(GetCurrentProcessId());
    files_ = std::make_unique<TemporaryFiles>();
    files_->root = std::filesystem::temp_directory_path() /
                   ("aevrynd-world-partition-milestone-c-" + suffix);
    files_->capture = std::filesystem::temp_directory_path() /
                      ("aevrynd-world-partition-milestone-c-" + suffix +
                       ".png");
    std::error_code ignored;
    std::filesystem::remove_all(files_->root, ignored);
    std::filesystem::remove(files_->capture, ignored);

    settings_ = FixtureSettings();
    snapshot_ = Snapshot(settings_, Chunk);
    negativeSnapshot_ = TerrainEditSnapshot(
        settings_, {-2, 0}, settings_.terrainCells, 4,
        NegativeBoundaryHeight, 2001);
    if (!VerifyAbsoluteHeightAndRollback(engine) ||
        !VerifyCornerDependencyComposition(engine)) {
      Fail(engine, 2, "absolute terrain edit or activation rollback failed");
      return;
    }

    store_ = std::make_unique<WorldChunkDeltaStore3D>(
        StoreSettings(files_->root));
    if (!store_->EnqueueSave(70, 1, snapshot_) ||
        !store_->EnqueueSave(71, 1, negativeSnapshot_)) {
      Fail(engine, 3, "delta fixture enqueue failed");
      return;
    }
    store_->WaitIdle();
    WorldDeltaSaveResult3D firstSaved, secondSaved;
    if (!store_->TryPopSave(firstSaved) || !store_->TryPopSave(secondSaved) ||
        !firstSaved.durable || !secondSaved.durable ||
        firstSaved.generation != snapshot_.durableGeneration ||
        secondSaved.generation != negativeSnapshot_.durableGeneration) {
      Fail(engine, 4, "delta fixture was not durable");
      return;
    }
    if (!StartRuntime(engine, false)) {
      Fail(engine, 5, "initial load-before-activate gate failed");
      return;
    }
  }

  void OnUpdate(Engine &engine, float) override {
    if (result_ || !partition_)
      return;
    partition_->Update({});
    const auto partitionStats = partition_->Stats();
    if (partitionStats.failed) {
      Fail(engine, 6, "persistent partition entered Failed");
      return;
    }

    if ((stage_ == 0 || stage_ == 2) && partitionStats.resident == 2) {
      if (!VerifyResident(engine)) {
        Fail(engine, stage_ ? 12 : 7,
             stage_ ? "restart state changed" : "initial composed state invalid");
        return;
      }
      if (stage_ == 0) {
        initialSceneEntities_ = 3;
        initialVegetationInstances_ = settings_.vegetationPerChunk * 2 - 1;
        initialPhysicsBodies_ = 1;
        if (!engine.CaptureNextFrame3D(files_->capture)) {
          Fail(engine, 8, "capture request failed");
          return;
        }
        stage_ = 1;
        frames_ = 0;
      } else {
        if (!StopRuntime(engine) || !Released(engine)) {
          Fail(engine, 13, "final retirement leaked references");
          return;
        }
        if (!VerifyNeighborOnlyDependency(engine, false) ||
            !VerifyNeighborOnlyDependency(engine, true) ||
            !VerifyBaseMismatchDependencyFailsClosed(engine)) {
          Fail(engine, 15, "neighbor-only dependency activation failed");
          return;
        }
        if (!VerifyCorruptDependencyFailsClosed(engine)) {
          Fail(engine, 16, "corrupt dependency did not fail closed");
          return;
        }
        if (!VerifyStaleDependencyLoads(engine)) {
          Fail(engine, 17, "stale dependency completion rebound");
          return;
        }
        if (!Released(engine)) {
          Fail(engine, 18, "dependency gates leaked renderer references");
          return;
        }
        std::cout
            << "load_before_activate=" << loadGates_
            << " absolute_height=" << EditedHeight
            << " removed_vegetation=1 building_entities=1 physics_bodies=1 "
               "boundary_atomic=2 boundary_height="
            << BoundaryHeight
            << " backpressure_rollback=1 owner_journal_only=1 "
               "restart_consistent=1 neighbor_only_dependency=2 "
               "negative_dependency=1 nw_corner_dependencies=4 "
               "dependency_base_mismatch_closed=1 "
               "dependency_mid_corrupt_closed=1 "
               "stale_dependency_completions=4 "
               "rollback_checkpoint=2 capture_bytes="
            << captureBytes_ << " final_refs=0 pending_gpu_release=0\n";
        engine.Quit();
      }
      return;
    }

    if (stage_ == 1 && ++frames_ > 4) {
      engine.WaitRendererIdle3D();
      if (!std::filesystem::exists(files_->capture) ||
          !(captureBytes_ = std::filesystem::file_size(files_->capture)) ||
          !engine.GetRendererStats().successfulFrameCaptures ||
          !engine.GetRendererStats().lastSubmittedMeshInstances) {
        Fail(engine, 9, "render or frame capture was empty");
        return;
      }
      if (!StopRuntime(engine) || !Released(engine)) {
        Fail(engine, 10, "unload leaked resources");
        return;
      }
      store_.reset();
      store_ = std::make_unique<WorldChunkDeltaStore3D>(
          StoreSettings(files_->root));
      if (!StartRuntime(engine, true)) {
        Fail(engine, 11, "restart load-before-activate gate failed");
        return;
      }
      stage_ = 2;
    }
  }

  void OnRender(Engine &engine, float) override {
    if (++renderFrames_ > 360) {
      Fail(engine, 14, "D3D smoke timed out");
      return;
    }
    Scene3D scene;
    scene_.Collect(scene, engine.Draw());
    Camera3D camera{{16, 14, -22}, {16, 1, 8}, {0, 1, 0}, {}};
    scene.Submit(engine.Draw(), camera,
                 {float(engine.Width()), float(engine.Height())});
    VegetationView3D view{camera, float(engine.Width()) / float(engine.Height()),
                          120, true, true};
    for (const auto &draw : vegetation_.Query(view))
      engine.Draw().MeshInstance(draw.mesh, draw.transform, draw.tint, {},
                                 MeshRenderLayer3D::Opaque, {}, .25f, false,
                                 MaterialClass3D::Foliage);
  }

  void OnStop(Engine &engine) override {
    StopRuntime(engine);
    store_.reset();
    files_.reset();
  }

private:
  void Fail(Engine &engine, int code, const char *message) {
    if (!result_) {
      result_ = code;
      std::cerr << message << '\n';
    }
    engine.Quit();
  }

  bool VerifyAbsoluteHeightAndRollback(Engine &engine) {
    TerrainVegetationChunkPayload3D payload;
    if (!LoadPayload(settings_, Chunk, payload))
      return false;
    ComposedTerrainVegetationChunk3D composed;
    if (!ComposeTerrainVegetationChunkDelta3D(payload, &snapshot_, composed) ||
        std::abs(composed.payload.heightField.Height(4, 4) - EditedHeight) >
            .0001f ||
        composed.payload.vegetation.size() + 1 != payload.vegetation.size() ||
        composed.buildings.size() != 1 ||
        composed.buildings.front().assetId != settings_.buildingAssetId)
      return false;

    const auto rendererBaseline = engine.GetRendererStats();
    TerrainVegetationWorldChunkActivationTarget3D failureTarget(
        engine, scene_, vegetation_, &physics_, settings_);
    failureTarget.SetActivationFailureCheckpointForTesting(2);
    const WorldChunkPayload3D opaque{std::make_shared<
                                         TerrainVegetationChunkPayload3D>(
                                         std::move(payload)),
                                     composed.payload.cpuBytes,
                                     composed.payload.conservativeGpuBytes};
    if (failureTarget.ActivatePersistent({Chunk, 0x73451u, 1, 90, 1}, opaque,
                                         &snapshot_))
      return false;
    engine.WaitRendererIdle3D();
    Scene3D scene;
    DrawList draw;
    const auto sceneStats = scene_.Collect(scene, draw);
    const auto rendererAfter = engine.GetRendererStats();
    return !sceneStats.aliveEntities && !vegetation_.Stats().chunks &&
           !vegetation_.Stats().instances && !physics_.Stats().bodies &&
           rendererAfter.residentMeshReferences ==
               rendererBaseline.residentMeshReferences &&
           rendererAfter.residentMaterialTextureReferences ==
               rendererBaseline.residentMaterialTextureReferences &&
           rendererAfter.residentMeshBytes == rendererBaseline.residentMeshBytes &&
           !rendererAfter.pendingGpuReleaseBytes;
  }

  bool VerifyCornerDependencyComposition(Engine &engine) {
    constexpr std::array<WorldChunkCoordinate3D, 4> coordinates{
        WorldChunkCoordinate3D{-1, -1}, WorldChunkCoordinate3D{0, -1},
        WorldChunkCoordinate3D{-1, 0}, WorldChunkCoordinate3D{0, 0}};
    std::array<std::shared_ptr<TerrainVegetationChunkPayload3D>, 4> payloads;
    for (std::size_t index = 0; index < coordinates.size(); ++index) {
      TerrainVegetationChunkPayload3D payload;
      if (!LoadPayload(settings_, coordinates[index], payload))
        return false;
      payloads[index] =
          std::make_shared<TerrainVegetationChunkPayload3D>(std::move(payload));
    }
    const auto corner = TerrainEditSnapshot(
        settings_, coordinates[0], settings_.terrainCells,
        settings_.terrainCells, CornerHeight, 3001);
    const auto rendererBaseline = engine.GetRendererStats();
    TerrainVegetationWorldChunkActivationTarget3D target(
        engine, scene_, vegetation_, &physics_, settings_);
    std::array<WorldChunkActivation3D, 4> activations{};
    for (std::size_t index = 0; index < coordinates.size(); ++index) {
      const WorldChunkPayload3D opaque{
          payloads[index], payloads[index]->cpuBytes,
          payloads[index]->conservativeGpuBytes};
      const auto request = WorldChunkRequest3D{
          coordinates[index], std::uint32_t(0x8000 + index), 1,
          std::uint64_t(400 + index), 1};
      const std::span<const WorldChunkDeltaSnapshot3D> dependencies =
          index ? std::span<const WorldChunkDeltaSnapshot3D>(&corner, 1)
                : std::span<const WorldChunkDeltaSnapshot3D>{};
      activations[index] = target.ActivatePersistentDependencies(
          request, opaque, index ? nullptr : &corner, dependencies);
      if (!activations[index])
        return false;
    }
    engine.WaitRendererIdle3D();
    const std::array<std::optional<float>, 4> heights{
        target.QueryTerrainHeight({-1, 0, -1}, settings_.terrainCells,
                                  settings_.terrainCells),
        target.QueryTerrainHeight({0, 0, -1}, 0, settings_.terrainCells),
        target.QueryTerrainHeight({-1, 0, 0}, settings_.terrainCells, 0),
        target.QueryTerrainHeight({0, 0, 0}, 0, 0)};
    for (const auto height : heights)
      if (!height || std::abs(*height - CornerHeight) > .0001f)
        return false;
    Scene3D scene;
    DrawList draw;
    if (scene_.Collect(scene, draw).aliveEntities != 4 ||
        vegetation_.Stats().chunks != 4 ||
        vegetation_.Stats().instances != settings_.vegetationPerChunk * 4 ||
        physics_.Stats().bodies)
      return false;
    for (const auto activation : activations)
      target.BeginRetire(activation);
    target.WaitIdle();
    engine.WaitRendererIdle3D();
    Scene3D releasedScene;
    DrawList releasedDraw;
    const auto released = scene_.Collect(releasedScene, releasedDraw);
    const auto rendererAfter = engine.GetRendererStats();
    return !released.aliveEntities && !vegetation_.Stats().chunks &&
           !vegetation_.Stats().instances && !physics_.Stats().bodies &&
           rendererAfter.residentMeshReferences ==
               rendererBaseline.residentMeshReferences &&
           !rendererAfter.pendingGpuReleaseBytes;
  }

  bool StartRuntime(Engine &engine, bool restart) {
    runtimeGates_ = 0;
    source_ = std::make_unique<TerrainVegetationWorldChunkSource3D>(settings_, 1);
    target_ = std::make_unique<TerrainVegetationWorldChunkActivationTarget3D>(
        engine, scene_, vegetation_, &physics_, settings_);
    partition_ = std::make_unique<WorldPartition3D>(
        *source_, *target_, PartitionSettings(*store_, settings_));
    if (!GateCoordinate(engine, {}, 0, 0, 0, 0))
      return false;
    engine.WaitRendererIdle3D();
    partition_->Update({});
    if (partition_->Stats().resident != 1)
      return false;
    if (!GateCoordinate(engine, {settings_.chunkSize, 0, 0}, 2,
                        settings_.vegetationPerChunk - 1, 1, 1))
      return false;
    if (restart)
      ++restartGates_;
    return true;
  }

  bool GateCoordinate(Engine &, Vec3 observer, std::size_t expectedEntities,
                      std::size_t expectedVegetation,
                      std::size_t expectedPhysics,
                      std::size_t expectedResident) {
    partition_->Update(observer);
    source_->WaitIdle();
    partition_->Update(observer);
    Scene3D hiddenScene;
    DrawList hiddenDraw;
    const auto hidden = scene_.Collect(hiddenScene, hiddenDraw);
    if (partition_->Stats().loadingDelta != 1 ||
        partition_->Stats().resident != expectedResident ||
        hidden.aliveEntities != expectedEntities ||
        vegetation_.Stats().instances != expectedVegetation ||
        physics_.Stats().bodies != expectedPhysics)
      return false;
    store_->WaitIdle();
    partition_->Update(observer);
    ++runtimeGates_;
    ++loadGates_;
    return partition_->Stats().deltaLoads == std::uint64_t(runtimeGates_ * 4) &&
           !partition_->Stats().loadingDelta;
  }

  std::array<MeshHandle3D, 2> TerrainHandles(std::size_t &visible) {
    Scene3D scene;
    DrawList draw(DrawListCapacityProfile::Minimal2D);
    scene_.Collect(scene, draw);
    Camera3D camera{{16, 14, -22}, {16, 1, 8}, {0, 1, 0}, {}};
    Scene3DSubmissionOptions options;
    options.frustumCulling = false;
    const auto submitted = scene.Submit(draw, camera, {320, 180}, options);
    visible = submitted.visibleMeshInstances;
    std::array<MeshHandle3D, 2> handles{};
    for (const auto &instance : draw.meshInstances) {
      if (instance.materialClass != MaterialClass3D::Terrain)
        continue;
      const auto index = instance.transform[0][3] < settings_.chunkSize * .5f
                             ? std::size_t{0}
                             : std::size_t{1};
      handles[index] = instance.mesh;
    }
    return handles;
  }

  bool VerifyBoundaryAtomic(Engine &engine) {
    const auto rejected = [](const char *checkpoint) {
      std::cerr << "boundary checkpoint=" << checkpoint << '\n';
      return false;
    };
    const GlobalChunkCoordinate3D owner{0, 0, 0}, neighbor{1, 0, 0};
    const auto ownerBeforeHeight = target_->QueryTerrainHeight(
        owner, settings_.terrainCells, 4);
    const auto neighborBeforeHeight = target_->QueryTerrainHeight(neighbor, 0, 4);
    if (!ownerBeforeHeight || !neighborBeforeHeight)
      return rejected("query-before");
    std::size_t visibleBefore{}, visibleAfter{}, visibleRejected{};
    const auto handlesBefore = TerrainHandles(visibleBefore);
    const auto referencesBefore = engine.GetRendererStats().residentMeshReferences;
    if (!handlesBefore[0] || !handlesBefore[1] || visibleBefore < 3)
      return rejected("visible-before");
    if (!target_->ApplyTerrainHeightEditAtomic(
            *partition_, neighbor, 0, 4, BoundaryHeight, snapshot_)) {
      const auto renderer = engine.GetRendererStats();
      std::cerr << "apply stats storeRejected=" << store_->Stats().rejected
                << " queued=" << store_->Stats().queuedRequests
                << " deltaSaveFailures="
                << partition_->Stats().deltaSaveFailures
                << " meshBudgetRejections=" << renderer.meshBudgetRejections
                << " pending=" << renderer.pendingGpuReleaseBytes << '\n';
      return rejected("apply-returned-false");
    }
    const auto ownerAfter = target_->QueryTerrainHeight(
        owner, settings_.terrainCells, 4);
    const auto neighborAfter = target_->QueryTerrainHeight(neighbor, 0, 4);
    const auto handlesAfter = TerrainHandles(visibleAfter);
    if (!ownerAfter || !neighborAfter ||
        std::abs(*ownerAfter - BoundaryHeight) > .0001f ||
        std::abs(*neighborAfter - BoundaryHeight) > .0001f ||
        handlesAfter[0] == handlesBefore[0] ||
        handlesAfter[1] == handlesBefore[1] || visibleAfter != visibleBefore)
      return rejected("sync-observable");

    if (target_->ApplyTerrainHeightEditAtomic(
            *partition_, neighbor, 0, 4, BoundaryHeight + 2, snapshot_))
      return rejected("backpressure-accepted");
    const auto rejectedOwner = target_->QueryTerrainHeight(
        owner, settings_.terrainCells, 4);
    const auto rejectedNeighbor = target_->QueryTerrainHeight(neighbor, 0, 4);
    const auto handlesRejected = TerrainHandles(visibleRejected);
    if (!rejectedOwner || !rejectedNeighbor ||
        std::abs(*rejectedOwner - BoundaryHeight) > .0001f ||
        std::abs(*rejectedNeighbor - BoundaryHeight) > .0001f ||
        handlesRejected != handlesAfter || visibleRejected != visibleAfter ||
        engine.GetRendererStats().residentMeshReferences != referencesBefore)
      return rejected("backpressure-mutated");

    store_->WaitIdle();
    partition_->Update({settings_.chunkSize, 0, 0});
    const WorldDeltaChunkKey3D ownerKey{settings_.worldId, PersistenceLayer,
                                        ChunkSizeRevision, owner};
    const WorldDeltaChunkKey3D neighborKey{settings_.worldId, PersistenceLayer,
                                           ChunkSizeRevision, neighbor};
    if (!store_->EnqueueLoad(801, 88, ownerKey, settings_.baseRevision) ||
        !store_->EnqueueLoad(802, 88, neighborKey, settings_.baseRevision))
      return rejected("journal-enqueue");
    store_->WaitIdle();
    WorldDeltaLoadResult3D first, second;
    if (!store_->TryPopLoad(first) || !store_->TryPopLoad(second))
      return rejected("journal-pop");
    const auto &ownerLoad = first.key == ownerKey ? first : second;
    const auto &neighborLoad = first.key == neighborKey ? first : second;
    const auto *boundary = ownerLoad.snapshot.operations.empty()
                               ? nullptr
                               : std::get_if<SetTerrainHeightDelta3D>(
                                     &ownerLoad.snapshot.operations.back().payload);
    if (ownerLoad.status != WorldDeltaLoadStatus3D::Loaded ||
        ownerLoad.snapshot.operations.size() != 4 || !boundary ||
        boundary->x != settings_.terrainCells || boundary->z != 4 ||
        std::abs(boundary->height - BoundaryHeight) > .0001f ||
        neighborLoad.status != WorldDeltaLoadStatus3D::Clean ||
        !neighborLoad.snapshot.operations.empty())
      return rejected("journal-content");
    snapshot_ = ownerLoad.snapshot;
    engine.WaitRendererIdle3D();
    return !engine.GetRendererStats().pendingGpuReleaseBytes;
  }

  bool VerifyResident(Engine &engine) {
    engine.WaitRendererIdle3D();
    Scene3D scene;
    DrawList draw;
    const auto sceneStats = scene_.Collect(scene, draw);
    const auto renderer = engine.GetRendererStats();
    if (partition_->Stats().deltaLoads != 8 ||
        sceneStats.aliveEntities != 3 ||
        sceneStats.renderableComponents != 3 ||
        vegetation_.Stats().chunks != 2 ||
        vegetation_.Stats().instances != settings_.vegetationPerChunk * 2 - 1 ||
        physics_.Stats().bodies != 1 || physics_.Stats().staticBodies != 1 ||
        renderer.residentMeshReferences < 6 || renderer.residentMeshes < 6 ||
        (stage_ &&
         (sceneStats.aliveEntities != initialSceneEntities_ ||
          vegetation_.Stats().instances != initialVegetationInstances_ ||
          physics_.Stats().bodies != initialPhysicsBodies_ ||
          restartGates_ != 1)))
    {
      std::cerr << "resident diagnostics deltaLoads="
                << partition_->Stats().deltaLoads
                << " entities=" << sceneStats.aliveEntities
                << " renderables=" << sceneStats.renderableComponents
                << " chunks=" << vegetation_.Stats().chunks
                << " vegetation=" << vegetation_.Stats().instances
                << " bodies=" << physics_.Stats().bodies
                << " meshRefs=" << renderer.residentMeshReferences
                << " meshes=" << renderer.residentMeshes << '\n';
      return false;
    }
    if (!stage_ && !boundaryVerified_ && !VerifyBoundaryAtomic(engine))
    {
      std::cerr << "boundary atomic diagnostics failed\n";
      return false;
    }
    boundaryVerified_ = true;
    if (stage_) {
      const auto owner = target_->QueryTerrainHeight(
          {0, 0, 0}, settings_.terrainCells, 4);
      const auto neighbor = target_->QueryTerrainHeight({1, 0, 0}, 0, 4);
      if (!owner || !neighbor || std::abs(*owner - BoundaryHeight) > .0001f ||
          std::abs(*neighbor - BoundaryHeight) > .0001f)
      {
        std::cerr << "restart boundary owner="
                  << (owner ? *owner : -9999.f) << " neighbor="
                  << (neighbor ? *neighbor : -9999.f) << '\n';
        return false;
      }
    }
    return vegetation_.Stats().instances == settings_.vegetationPerChunk * 2 - 1 &&
           physics_.Stats().bodies == 1 && physics_.Stats().staticBodies == 1 &&
           scene_.Stats().aliveEntities == 3;
  }

  bool VerifyNeighborOnlyDependency(Engine &engine, bool negative) {
    runtimeGates_ = 0;
    source_ = std::make_unique<TerrainVegetationWorldChunkSource3D>(settings_, 1);
    target_ = std::make_unique<TerrainVegetationWorldChunkActivationTarget3D>(
        engine, scene_, vegetation_, &physics_, settings_);
    partition_ = std::make_unique<WorldPartition3D>(
        *source_, *target_, PartitionSettings(*store_, settings_, 0));
    const int neighborChunk = negative ? -1 : 1;
    const int ownerChunk = negative ? -2 : 0;
    const float expectedHeight =
        negative ? NegativeBoundaryHeight : BoundaryHeight;
    const Vec3 observer{float(neighborChunk) * settings_.chunkSize, 0, 0};
    if (!GateCoordinate(engine, observer, 0, 0, 0, 0))
      return false;
    engine.WaitRendererIdle3D();
    partition_->Update(observer);
    Scene3D scene;
    DrawList draw;
    const auto sceneStats = scene_.Collect(scene, draw);
    const auto neighbor =
        target_->QueryTerrainHeight({neighborChunk, 0, 0}, 0, 4);
    const auto owner = target_->QueryTerrainHeight(
        {ownerChunk, 0, 0}, settings_.terrainCells, 4);
    const bool isolated = partition_->Stats().resident == 1 &&
                          partition_->Chunks().size() == 1 &&
                          partition_->Chunks().front().coordinate ==
                              WorldChunkCoordinate3D{neighborChunk, 0} &&
                          sceneStats.aliveEntities == 1 &&
                          vegetation_.Stats().chunks == 1 &&
                          vegetation_.Stats().instances ==
                              settings_.vegetationPerChunk &&
                          !physics_.Stats().bodies && neighbor && !owner &&
                          std::abs(*neighbor - expectedHeight) < .0001f;
    return isolated && StopRuntime(engine);
  }

  bool VerifyBaseMismatchDependencyFailsClosed(Engine &engine) {
    const auto rendererBaseline = engine.GetRendererStats();
    auto mismatched = TerrainEditSnapshot(
        settings_, {0, 0}, settings_.terrainCells, 4, BoundaryHeight, 4001);
    ++mismatched.base.contentRevision;
    WorldChunkDeltaStore3D mismatchStore(
        StoreSettings(files_->root / "base-mismatch"));
    if (!mismatchStore.EnqueueSave(901, 1, mismatched))
      return false;
    mismatchStore.WaitIdle();
    WorldDeltaSaveResult3D saved;
    if (!mismatchStore.TryPopSave(saved) || !saved.durable)
      return false;
    SceneECSRegistry3D mismatchScene;
    ChunkedVegetationWorld3D mismatchVegetation;
    PhysicsWorld3D mismatchPhysics;
    TerrainVegetationWorldChunkSource3D mismatchSource(settings_, 1);
    TerrainVegetationWorldChunkActivationTarget3D mismatchTarget(
        engine, mismatchScene, mismatchVegetation, &mismatchPhysics, settings_);
    WorldPartition3D mismatchPartition(
        mismatchSource, mismatchTarget,
        PartitionSettings(mismatchStore, settings_, 0));
    const Vec3 observer{settings_.chunkSize, 0, 0};
    mismatchPartition.Update(observer);
    mismatchSource.WaitIdle();
    mismatchPartition.Update(observer);
    if (mismatchPartition.Stats().loadingDelta != 1)
      return false;
    mismatchStore.WaitIdle();
    mismatchPartition.Update(observer);
    Scene3D scene;
    DrawList draw;
    const auto sceneStats = mismatchScene.Collect(scene, draw);
    const bool closed = mismatchPartition.Stats().failed == 1 &&
                        mismatchPartition.Stats().deltaLoadFailures == 1 &&
                        !mismatchPartition.Stats().totalActivated &&
                        !sceneStats.aliveEntities &&
                        !mismatchVegetation.Stats().chunks &&
                        !mismatchVegetation.Stats().instances &&
                        !mismatchPhysics.Stats().bodies;
    mismatchPartition.Reset();
    engine.WaitRendererIdle3D();
    const auto rendererAfter = engine.GetRendererStats();
    return closed && rendererAfter.residentMeshReferences ==
                         rendererBaseline.residentMeshReferences &&
           !rendererAfter.pendingGpuReleaseBytes;
  }

  bool VerifyCorruptDependencyFailsClosed(Engine &engine) {
    const auto rendererBaseline = engine.GetRendererStats();
    const auto root = files_->root / "mid-corrupt";
    WorldChunkDeltaStore3D corruptStore(StoreSettings(root));
    const auto owner = TerrainEditSnapshot(
        settings_, {0, 0}, settings_.terrainCells, 4, BoundaryHeight, 5001);
    if (!corruptStore.EnqueueSave(911, 1, owner))
      return false;
    corruptStore.WaitIdle();
    WorldDeltaSaveResult3D saved;
    if (!corruptStore.TryPopSave(saved) || !saved.durable)
      return false;
    const auto snapshotFile = FindSnapshotFile(root);
    const auto journalFile = FindJournalFile(root);
    if (snapshotFile.empty() || journalFile.empty())
      return false;
    std::error_code removeError;
    if (!std::filesystem::remove(snapshotFile, removeError) || removeError)
      return false;
    std::fstream file(journalFile,
                      std::ios::in | std::ios::out | std::ios::binary);
    file.seekg(0, std::ios::end);
    const auto length = file.tellg();
    if (length < std::streamoff(16))
      return false;
    const auto offset = length - std::streamoff(1);
    file.seekg(offset);
    char byte{};
    file.read(&byte, 1);
    byte = char(static_cast<unsigned char>(byte) ^ 0x5au);
    file.seekp(offset);
    file.write(&byte, 1);
    file.flush();
    file.close();
    if (!file)
      return false;

    SceneECSRegistry3D corruptScene;
    ChunkedVegetationWorld3D corruptVegetation;
    PhysicsWorld3D corruptPhysics;
    TerrainVegetationWorldChunkSource3D corruptSource(settings_, 1);
    TerrainVegetationWorldChunkActivationTarget3D corruptTarget(
        engine, corruptScene, corruptVegetation, &corruptPhysics, settings_);
    WorldPartition3D corruptPartition(
        corruptSource, corruptTarget,
        PartitionSettings(corruptStore, settings_, 0));
    const Vec3 observer{settings_.chunkSize, 0, 0};
    corruptPartition.Update(observer);
    corruptSource.WaitIdle();
    corruptPartition.Update(observer);
    if (corruptPartition.Stats().loadingDelta != 1)
      return false;
    corruptStore.WaitIdle();
    corruptPartition.Update(observer);
    Scene3D scene;
    DrawList draw;
    const auto sceneStats = corruptScene.Collect(scene, draw);
    const bool closed = corruptPartition.Stats().failed == 1 &&
                        corruptPartition.Stats().deltaLoadFailures == 1 &&
                        !corruptPartition.Stats().totalActivated &&
                        corruptStore.Stats().corruptions == 1 &&
                        !sceneStats.aliveEntities &&
                        !corruptVegetation.Stats().chunks &&
                        !corruptPhysics.Stats().bodies;
    if (!closed)
      std::cerr << "corrupt diagnostics failed="
                << corruptPartition.Stats().failed
                << " deltaLoadFailures="
                << corruptPartition.Stats().deltaLoadFailures
                << " activated=" << corruptPartition.Stats().totalActivated
                << " corruptions=" << corruptStore.Stats().corruptions
                << " stale="
                << corruptPartition.Stats().staleDeltaCompletions
                << " entities=" << sceneStats.aliveEntities << '\n';
    corruptPartition.Reset();
    engine.WaitRendererIdle3D();
    const auto rendererAfter = engine.GetRendererStats();
    return closed && rendererAfter.residentMeshReferences ==
                         rendererBaseline.residentMeshReferences &&
           !rendererAfter.pendingGpuReleaseBytes;
  }

  bool VerifyStaleDependencyLoads(Engine &engine) {
    const auto rendererBaseline = engine.GetRendererStats();
    const auto root = files_->root / "stale-dependencies";
    WorldChunkDeltaStore3D staleStore(StoreSettings(root));
    const auto owner = TerrainEditSnapshot(
        settings_, {0, 0}, settings_.terrainCells, 4, BoundaryHeight, 6001);
    if (!staleStore.EnqueueSave(921, 1, owner))
      return false;
    staleStore.WaitIdle();
    WorldDeltaSaveResult3D saved;
    if (!staleStore.TryPopSave(saved) || !saved.durable)
      return false;
    SceneECSRegistry3D staleScene;
    ChunkedVegetationWorld3D staleVegetation;
    PhysicsWorld3D stalePhysics;
    TerrainVegetationWorldChunkSource3D staleSource(settings_, 1);
    TerrainVegetationWorldChunkActivationTarget3D staleTarget(
        engine, staleScene, staleVegetation, &stalePhysics, settings_);
    WorldPartition3D stalePartition(
        staleSource, staleTarget, PartitionSettings(staleStore, settings_, 0));
    const Vec3 observer{settings_.chunkSize, 0, 0};
    stalePartition.Update(observer);
    staleSource.WaitIdle();
    stalePartition.Update(observer);
    if (stalePartition.Stats().loadingDelta != 1 ||
        stalePartition.Chunks().size() != 1)
      return false;
    const auto oldRequest = stalePartition.Chunks().front().requestId;
    const auto oldEpoch = stalePartition.Stats().epoch;
    stalePartition.Reset();
    if (!stalePartition.Chunks().empty() ||
        stalePartition.Stats().staleDeltaCompletions != 4 ||
        stalePartition.Stats().epoch == oldEpoch)
      return false;

    stalePartition.Update(observer);
    staleSource.WaitIdle();
    stalePartition.Update(observer);
    if (stalePartition.Stats().loadingDelta != 1 ||
        stalePartition.Chunks().size() != 1 ||
        stalePartition.Chunks().front().requestId != oldRequest)
      return false;
    staleStore.WaitIdle();
    stalePartition.Update(observer);
    engine.WaitRendererIdle3D();
    stalePartition.Update(observer);
    const auto height = staleTarget.QueryTerrainHeight({1, 0, 0}, 0, 4);
    Scene3D scene;
    DrawList draw;
    const auto sceneStats = staleScene.Collect(scene, draw);
    const bool rebound = stalePartition.Stats().resident == 1 &&
                         stalePartition.Stats().deltaLoads == 4 &&
                         stalePartition.Stats().staleDeltaCompletions == 4 &&
                         stalePartition.Stats().totalActivated == 1 && height &&
                         std::abs(*height - BoundaryHeight) < .0001f &&
                         sceneStats.aliveEntities == 1 &&
                         staleVegetation.Stats().chunks == 1;
    stalePartition.Reset();
    engine.WaitRendererIdle3D();
    const auto rendererAfter = engine.GetRendererStats();
    return rebound && rendererAfter.residentMeshReferences ==
                          rendererBaseline.residentMeshReferences &&
           !rendererAfter.pendingGpuReleaseBytes;
  }

  bool StopRuntime(Engine &engine) {
    if (partition_)
      partition_->Reset();
    if (target_)
      target_->WaitIdle();
    engine.WaitRendererIdle3D();
    Scene3D scene;
    DrawList draw;
    const auto sceneStats = scene_.Collect(scene, draw);
    const bool empty = !sceneStats.aliveEntities &&
                       !vegetation_.Stats().chunks &&
                       !vegetation_.Stats().instances &&
                       !physics_.Stats().bodies &&
                       (!target_ || !target_->PendingRetirements()) &&
                       (!partition_ || partition_->Chunks().empty());
    partition_.reset();
    target_.reset();
    source_.reset();
    engine.WaitRendererIdle3D();
    return empty;
  }

  bool Released(Engine &engine) const {
    const auto renderer = engine.GetRendererStats();
    return !renderer.residentMeshReferences &&
           !renderer.residentMaterialTextureReferences &&
           !renderer.pendingGpuReleaseBytes;
  }

  int &result_;
  TerrainVegetationChunkSettings3D settings_{};
  WorldChunkDeltaSnapshot3D snapshot_{}, negativeSnapshot_{};
  SceneECSRegistry3D scene_{};
  ChunkedVegetationWorld3D vegetation_{};
  PhysicsWorld3D physics_{};
  std::unique_ptr<TemporaryFiles> files_{};
  std::unique_ptr<WorldChunkDeltaStore3D> store_{};
  std::unique_ptr<TerrainVegetationWorldChunkSource3D> source_{};
  std::unique_ptr<TerrainVegetationWorldChunkActivationTarget3D> target_{};
  std::unique_ptr<WorldPartition3D> partition_{};
  std::size_t initialSceneEntities_{}, initialVegetationInstances_{},
      initialPhysicsBodies_{}, captureBytes_{};
  int stage_{}, frames_{}, renderFrames_{}, restartGates_{}, runtimeGates_{},
      loadGates_{};
  bool boundaryVerified_{};
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, wchar_t *, int) {
  int result{};
  try {
    EngineConfig config{};
    config.title = L"Aevrynd World Partition Milestone C";
    config.width = 320;
    config.height = 180;
    config.renderQuality = RenderQuality3D::Low;
    config.enableAutomaticMemoryTrim = false;
    Engine engine(config);
    engine.Run(std::make_unique<TerrainVegetationDeltaD3D12Game>(result),
               instance, SW_HIDE);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 99;
  }
  std::cout << "result=" << result << '\n';
  return result;
}
