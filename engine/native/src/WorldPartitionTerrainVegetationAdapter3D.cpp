#include <Aevrynd/WorldPartitionTerrainVegetationAdapter3D.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Aevrynd {
namespace {
constexpr std::size_t GpuResourceAlignment = 64u * 1024u;
bool Add(std::size_t &value, std::size_t amount) {
  if (value > SIZE_MAX - amount)
    return false;
  value += amount;
  return true;
}
bool Multiply(std::size_t left, std::size_t right, std::size_t &result) {
  if (left && right > SIZE_MAX / left)
    return false;
  result = left * right;
  return true;
}
std::size_t MeshCpuBytes(const Mesh3D &mesh) {
  return mesh.vertices.size() * sizeof(Vertex3D) +
         mesh.indices.size() * sizeof(std::uint32_t);
}
std::size_t MeshGpuReservation(const Mesh3D &mesh) {
  const auto bytes = MeshCpuBytes(mesh);
  if (!bytes)
    return 0;
  if (bytes > SIZE_MAX - (GpuResourceAlignment - 1))
    return SIZE_MAX;
  return ((bytes + GpuResourceAlignment - 1) / GpuResourceAlignment) *
         GpuResourceAlignment;
}
bool HasTextures(const MaterialTextureSetDefinition3D &value) {
  return !value.baseColorPath.empty() || !value.roughnessPath.empty() ||
         !value.normalPath.empty();
}
std::size_t Delta(std::size_t after, std::size_t before) {
  return after >= before ? after - before : 0;
}
bool ValidSettings(const TerrainVegetationChunkSettings3D &value) {
  return std::isfinite(value.chunkSize) && value.chunkSize > 0 &&
         std::isfinite(value.terrainAmplitude) && value.terrainAmplitude >= 0 &&
         value.terrainCells > 0 && value.terrainCells <= 2048 &&
         value.vegetationPerChunk <= MaximumMeshInstancesPerFrame3D;
}
bool FailCheckpoint(int &checkpoint) {
  if (checkpoint < 0)
    return false;
  if (checkpoint == 0) {
    checkpoint = -1;
    return true;
  }
  --checkpoint;
  return false;
}
std::uint64_t Hash(std::uint64_t value) {
  value ^= value >> 30;
  value *= 0xbf58476d1ce4e5b9ull;
  value ^= value >> 27;
  value *= 0x94d049bb133111ebull;
  return value ^ (value >> 31);
}
const TerrainVegetationChunkPayload3D *
Payload(const WorldChunkPayload3D &value) {
  return static_cast<const TerrainVegetationChunkPayload3D *>(value.data.get());
}
} // namespace

GlobalChunkCoordinate3D
CanonicalTerrainSampleOwner3D(GlobalChunkCoordinate3D chunk, std::uint32_t x,
                              std::uint32_t z, std::uint32_t cells) noexcept {
  if (!cells || x > cells || z > cells)
    return {};
  // 共用西、北邊界由座標較小的區塊持有，避免同一 sample 產生兩份真相。
  if (x == 0) {
    if (chunk.x == (std::numeric_limits<std::int64_t>::min)())
      return {};
    --chunk.x;
  }
  if (z == 0) {
    if (chunk.z == (std::numeric_limits<std::int64_t>::min)())
      return {};
    --chunk.z;
  }
  return chunk;
}

bool ComposeTerrainVegetationChunkDelta3D(
    const TerrainVegetationChunkPayload3D &base,
    const WorldChunkDeltaSnapshot3D *delta,
    ComposedTerrainVegetationChunk3D &output) {
  if (!base.terrain.Valid() ||
      base.vegetation.size() != base.vegetationIds.size())
    return false;
  ComposedTerrainVegetationChunk3D composed{};
  composed.payload = base;
  if (!delta) {
    output = std::move(composed);
    return true;
  }
  if (!(delta->base == base.baseRevision) ||
      delta->key.chunk != base.globalChunk)
    return false;
  for (const auto &operation : delta->operations) {
    if (const auto *edit =
            std::get_if<SetTerrainHeightDelta3D>(&operation.payload)) {
      if (!std::isfinite(edit->height) ||
          edit->x >= base.heightField.Settings().width ||
          edit->z >= base.heightField.Settings().depth ||
          !composed.payload.heightField.SetHeight(edit->x, edit->z,
                                                  edit->height))
        return false;
    } else if (const auto *removed =
                   std::get_if<RemoveVegetationDelta3D>(&operation.payload)) {
      const auto found =
          std::find(composed.payload.vegetationIds.begin(),
                    composed.payload.vegetationIds.end(), removed->id);
      if (found == composed.payload.vegetationIds.end())
        return false;
      const auto index =
          std::size_t(found - composed.payload.vegetationIds.begin());
      composed.payload.vegetationIds.erase(found);
      composed.payload.vegetation.erase(composed.payload.vegetation.begin() +
                                        std::ptrdiff_t(index));
    } else if (const auto *placed =
                   std::get_if<PlaceBuildingDelta3D>(&operation.payload)) {
      const auto found =
          std::find_if(composed.buildings.begin(), composed.buildings.end(),
                       [&](const auto &building) {
                         return building.id == placed->building.id;
                       });
      if (found == composed.buildings.end())
        composed.buildings.push_back(placed->building);
      else
        *found = placed->building;
    } else if (const auto *removedBuilding =
                   std::get_if<RemoveBuildingDelta3D>(&operation.payload)) {
      const auto found = std::find_if(
          composed.buildings.begin(), composed.buildings.end(),
          [&](const auto &building) { return building.id == removedBuilding->id; });
      if (found == composed.buildings.end())
        return false;
      composed.buildings.erase(found);
    }
  }
  composed.payload.heightField.RecalculateRange();
  composed.payload.terrain = composed.payload.heightField.CreateMesh();
  if (!composed.payload.terrain.Valid())
    return false;
  output = std::move(composed);
  return true;
}

TerrainVegetationWorldChunkSource3D::TerrainVegetationWorldChunkSource3D(
    TerrainVegetationChunkSettings3D settings, std::uint32_t workers)
    : settings_(std::move(settings)) {
  workers = (std::max)(1u, workers);
  workers_.reserve(workers);
  for (std::uint32_t i = 0; i < workers; ++i)
    workers_.emplace_back([this](std::stop_token stop) { Worker(stop); });
}
TerrainVegetationWorldChunkSource3D::~TerrainVegetationWorldChunkSource3D() {
  for (auto &worker : workers_)
    worker.request_stop();
  wake_.notify_all();
  workers_.clear();
}
bool TerrainVegetationWorldChunkSource3D::BeginLoad(
    WorldChunkRequest3D request, std::stop_token cancellation) {
  if (cancellation.stop_requested() || !ValidSettings(settings_))
    return false;
  const double worldX =
                   double(request.coordinate.x) * double(settings_.chunkSize),
               worldZ =
                   double(request.coordinate.z) * double(settings_.chunkSize);
  if (!std::isfinite(worldX) || !std::isfinite(worldZ) ||
      std::abs(worldX) > std::numeric_limits<float>::max() ||
      std::abs(worldZ) > std::numeric_limits<float>::max())
    return false;
  {
    std::lock_guard lock(mutex_);
    work_.push_back({request, cancellation});
  }
  wake_.notify_one();
  return true;
}
void TerrainVegetationWorldChunkSource3D::Worker(std::stop_token stop) {
  for (;;) {
    Work work{};
    {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, stop, [&] { return !work_.empty(); });
      if (stop.stop_requested())
        return;
      work = work_.front();
      work_.pop_front();
      ++active_;
    }
    WorldChunkLoadCompletion3D result{work.request, {}, false};
    if (!work.cancellation.stop_requested())
      try {
        auto payload = std::make_shared<TerrainVegetationChunkPayload3D>();
        payload->globalChunk = {work.request.coordinate.x, 0,
                                work.request.coordinate.z};
        payload->baseRevision = settings_.baseRevision;
        const auto cells = settings_.terrainCells;
        const float cell = settings_.chunkSize / float(cells);
        payload->heightField.Configure({{}, cells + 1, cells + 1, cell});
        payload->heightField.Generate(work.request.seed,
                                      settings_.terrainAmplitude, .02f, 5);
        payload->terrain = payload->heightField.CreateMesh();
        payload->vegetation.reserve(settings_.vegetationPerChunk);
        payload->vegetationIds.reserve(settings_.vegetationPerChunk);
        for (std::uint32_t i = 0; i < settings_.vegetationPerChunk &&
                                  !work.cancellation.stop_requested();
             ++i) {
          const auto h =
              Hash(std::uint64_t(work.request.seed) ^
                   std::uint64_t(std::uint32_t(work.request.coordinate.x)) *
                       0x9e3779b9u ^
                   std::uint64_t(std::uint32_t(work.request.coordinate.z)) *
                       0x85ebca6bu ^
                   i);
          const float x =
              (float(h & 0xffffu) + .5f) / 65536.f * settings_.chunkSize;
          const float z = (float((h >> 16) & 0xffffu) + .5f) / 65536.f *
                          settings_.chunkSize;
          Transform3D transform{};
          transform.position = {x, payload->heightField.SampleHeight(x, z), z};
          const float scale = .75f + float((h >> 32) & 255u) / 512.f;
          transform.scale = {scale, scale, scale};
          const Matrix4 worldTransform = transform.Matrix();
          const MeshBounds3D worldBounds = TransformSphereBounds(
              {{0, 1, 0}, 2.25f, {1, 2.25f, 1}}, transform);
          const auto stable = DeriveStableObjectId3D(
              settings_.worldId, payload->globalChunk,
              payload->baseRevision.contentRevision, 3, i);
          payload->vegetationIds.push_back(stable);
          payload->vegetation.push_back(
              {stable.low ? stable.low : 1u,
               0,
               worldTransform,
               worldBounds,
               {.8f + .2f * float((h >> 40) & 255u) / 255.f, 1, .8f, 1}});
        }
        std::size_t vegetationBytes{}, vegetationIdBytes{}, heightBytes{};
        if (!Multiply(payload->vegetation.capacity(),
                      sizeof(VegetationInstance3D), vegetationBytes) ||
            !Multiply(payload->vegetationIds.capacity(),
                      sizeof(StableObjectId3D), vegetationIdBytes) ||
            !Multiply(payload->heightField.SampleCount(), sizeof(float),
                      heightBytes)) {
          result.succeeded = false;
          throw std::overflow_error("vegetation cpu byte overflow");
        }
        payload->cpuBytes = MeshCpuBytes(payload->terrain);
        if (!Add(payload->cpuBytes, vegetationBytes) ||
            !Add(payload->cpuBytes, vegetationIdBytes) ||
            !Add(payload->cpuBytes, heightBytes)) {
          result.succeeded = false;
          throw std::overflow_error("chunk cpu byte overflow");
        }
        payload->conservativeGpuBytes = MeshGpuReservation(payload->terrain);
        for (const auto &mesh : settings_.vegetationLods)
          if (!Add(payload->conservativeGpuBytes, MeshGpuReservation(mesh)))
            payload->conservativeGpuBytes = SIZE_MAX;
        if (payload->terrain.Valid() && !work.cancellation.stop_requested()) {
          result.payload = {payload, payload->cpuBytes,
                            payload->conservativeGpuBytes};
          result.succeeded = true;
        }
      } catch (...) {
      }
    {
      std::lock_guard lock(mutex_);
      completions_.push_back(std::move(result));
      --active_;
      if (work_.empty() && !active_)
        idle_.notify_all();
    }
  }
}
bool TerrainVegetationWorldChunkSource3D::TryPopCompletion(
    WorldChunkLoadCompletion3D &completion) {
  std::lock_guard lock(mutex_);
  if (completions_.empty())
    return false;
  completion = std::move(completions_.front());
  completions_.pop_front();
  return true;
}
void TerrainVegetationWorldChunkSource3D::WaitIdle() {
  std::unique_lock lock(mutex_);
  idle_.wait(lock, [&] { return work_.empty() && !active_; });
}
std::size_t TerrainVegetationWorldChunkSource3D::TrimUnusedMemory() {
  std::lock_guard lock(mutex_);
  const auto before = work_.size() * sizeof(Work) +
                      completions_.size() * sizeof(WorldChunkLoadCompletion3D);
  if (work_.empty())
    decltype(work_){}.swap(work_);
  if (completions_.empty())
    decltype(completions_){}.swap(completions_);
  return before;
}

TerrainVegetationWorldChunkActivationTarget3D::
    TerrainVegetationWorldChunkActivationTarget3D(
        Engine &engine, SceneECSRegistry3D &scene,
        ChunkedVegetationWorld3D &vegetation,
        TerrainVegetationChunkSettings3D settings)
    : TerrainVegetationWorldChunkActivationTarget3D(
          engine, scene, vegetation, nullptr, std::move(settings)) {}
TerrainVegetationWorldChunkActivationTarget3D::
    TerrainVegetationWorldChunkActivationTarget3D(
        Engine &engine, SceneECSRegistry3D &scene,
        ChunkedVegetationWorld3D &vegetation, PhysicsWorld3D *physics,
        TerrainVegetationChunkSettings3D settings)
    : engine_(engine), scene_(scene), vegetation_(vegetation),
      physics_(physics), settings_(std::move(settings)),
      ownerThread_(std::this_thread::get_id()) {}
bool TerrainVegetationWorldChunkActivationTarget3D::SetWorldOriginSnapshot3D(
    WorldOrigin3D origin) noexcept {
  if (!OnOwnerThread() || !origin.generation ||
      !NormalizeGlobalPosition3D(origin.position, settings_.chunkSize))
    return false;
  origin_ = origin;
  return true;
}
bool TerrainVegetationWorldChunkActivationTarget3D::PrepareWorldOriginRebase3D(
    const WorldOriginRebaseEvent3D &event) {
  if (!OnOwnerThread() || !event || event.previous != origin_ ||
      !event.current.generation)
    return false;
  preparedOrigin_ = event.current;
  return NormalizeGlobalPosition3D(preparedOrigin_.position,
                                   settings_.chunkSize);
}
void TerrainVegetationWorldChunkActivationTarget3D::CommitWorldOriginRebase3D(
    const WorldOriginRebaseEvent3D &event) noexcept {
  if (OnOwnerThread() && preparedOrigin_ == event.current)
    origin_ = preparedOrigin_;
  preparedOrigin_ = {};
}
TerrainVegetationWorldChunkActivationTarget3D::
    ~TerrainVegetationWorldChunkActivationTarget3D() {
  for (auto &[id, record] : records_) {
    (void)id;
    ReleaseRecord(record);
  }
  engine_.WaitRendererIdle3D();
  records_.clear();
}
std::size_t
TerrainVegetationWorldChunkActivationTarget3D::PhysicalAllocationBytes(
    WorldChunkRequest3D, const WorldChunkPayload3D &value) const {
  if (!OnOwnerThread() || !ValidSettings(settings_))
    return SIZE_MAX;
  const auto *payload = Payload(value);
  if (!payload)
    return SIZE_MAX;
  std::size_t bytes = MeshGpuReservation(payload->terrain);
  if (!shared_.references) {
    for (const auto &mesh : settings_.vegetationLods)
      if (!Add(bytes, MeshGpuReservation(mesh)))
        return SIZE_MAX;
    if (HasTextures(settings_.vegetationTextures)) {
      const auto estimate = engine_.EstimateMaterialTextureSetAllocationBytes3D(
          settings_.vegetationTextures);
      if (!estimate || !Add(bytes, *estimate))
        return SIZE_MAX;
    }
    if (settings_.buildingMesh.Valid() &&
        !Add(bytes, MeshGpuReservation(settings_.buildingMesh)))
      return SIZE_MAX;
    if (HasTextures(settings_.buildingTextures)) {
      const auto estimate = engine_.EstimateMaterialTextureSetAllocationBytes3D(
          settings_.buildingTextures);
      if (!estimate || !Add(bytes, *estimate))
        return SIZE_MAX;
    }
  }
  if (HasTextures(settings_.terrainTextures)) {
    const auto estimate = engine_.EstimateMaterialTextureSetAllocationBytes3D(
        settings_.terrainTextures);
    if (!estimate || !Add(bytes, *estimate))
      return SIZE_MAX;
  }
  return bytes;
}
bool TerrainVegetationWorldChunkActivationTarget3D::EnsureShared(
    const TerrainVegetationChunkPayload3D &, bool needsBuilding,
    std::size_t &physical) {
  if (shared_.references) {
    if (shared_.references == SIZE_MAX)
      return false;
    if (needsBuilding && !shared_.buildingMesh) {
      const auto before = engine_.GetRendererStats();
      MeshResource3D buildingMesh{};
      MaterialTextureSetHandle3D buildingTextures{};
      try {
        buildingMesh = engine_.CreateMeshResource3D(
            settings_.buildingMesh, MeshCpuRetention3D::Discard);
        if (!buildingMesh)
          throw std::runtime_error("building mesh upload failed");
        if (HasTextures(settings_.buildingTextures)) {
          buildingTextures =
              engine_.CreateMaterialTextureSet3D(settings_.buildingTextures);
          if (!buildingTextures)
            throw std::runtime_error("building texture upload failed");
        }
      } catch (...) {
        if (buildingTextures)
          engine_.DestroyMaterialTextureSet3D(buildingTextures);
        if (buildingMesh.handle)
          engine_.DestroyMesh3D(buildingMesh.handle);
        return false;
      }
      shared_.buildingMesh = buildingMesh;
      shared_.buildingTextures = buildingTextures;
      const auto after = engine_.GetRendererStats();
      physical += Delta(after.residentMeshBytes, before.residentMeshBytes) +
                  Delta(after.residentTextureAllocationBytes,
                        before.residentTextureAllocationBytes);
      shared_.physicalBytes += physical;
    }
    ++shared_.references;
    return true;
  }
  const auto before = engine_.GetRendererStats();
  try {
    for (std::size_t i = 0; i < 3; ++i) {
      shared_.meshes[i] = engine_.CreateMeshResource3D(
          settings_.vegetationLods[i], MeshCpuRetention3D::Discard);
      if (!shared_.meshes[i])
        throw std::runtime_error("vegetation mesh upload failed");
    }
    if (HasTextures(settings_.vegetationTextures)) {
      shared_.textures =
          engine_.CreateMaterialTextureSet3D(settings_.vegetationTextures);
      if (!shared_.textures)
        throw std::runtime_error("vegetation texture upload failed");
    }
    if (needsBuilding && settings_.buildingMesh.Valid()) {
      shared_.buildingMesh = engine_.CreateMeshResource3D(
          settings_.buildingMesh, MeshCpuRetention3D::Discard);
      if (!shared_.buildingMesh)
        throw std::runtime_error("building mesh upload failed");
    }
    if (needsBuilding && HasTextures(settings_.buildingTextures)) {
      shared_.buildingTextures =
          engine_.CreateMaterialTextureSet3D(settings_.buildingTextures);
      if (!shared_.buildingTextures)
        throw std::runtime_error("building texture upload failed");
    }
    shared_.lease =
        std::make_shared<AssetLeaseState3D>(AssetLeaseState3D{Hash(next_)});
    shared_.references = 1;
    const auto after = engine_.GetRendererStats();
    physical = Delta(after.residentMeshBytes, before.residentMeshBytes) +
               Delta(after.residentTextureAllocationBytes,
                     before.residentTextureAllocationBytes);
    shared_.physicalBytes = physical;
    return true;
  } catch (...) {
    for (auto &mesh : shared_.meshes)
      if (mesh.handle)
        engine_.DestroyMesh3D(mesh.handle);
    if (shared_.textures)
      engine_.DestroyMaterialTextureSet3D(shared_.textures);
    if (shared_.buildingMesh.handle)
      engine_.DestroyMesh3D(shared_.buildingMesh.handle);
    if (shared_.buildingTextures)
      engine_.DestroyMaterialTextureSet3D(shared_.buildingTextures);
    shared_ = {};
    return false;
  }
}
WorldChunkActivation3D TerrainVegetationWorldChunkActivationTarget3D::Activate(
    WorldChunkRequest3D request, const WorldChunkPayload3D &value) {
  return ActivatePersistent(request, value, nullptr);
}
WorldChunkActivation3D
TerrainVegetationWorldChunkActivationTarget3D::ActivatePersistent(
    WorldChunkRequest3D request, const WorldChunkPayload3D &value,
    const WorldChunkDeltaSnapshot3D *delta) {
  return ActivatePersistentDependencies(request, value, delta, {});
}

std::size_t TerrainVegetationWorldChunkActivationTarget3D::
    PersistentDependencyCoordinates(
        WorldChunkRequest3D request,
        std::span<WorldChunkCoordinate3D> output) const noexcept {
  if (output.size() < 3 ||
      request.coordinate.x == (std::numeric_limits<int>::min)() ||
      request.coordinate.z == (std::numeric_limits<int>::min)())
    return output.size() + 1;
  output[0] = {request.coordinate.x - 1, request.coordinate.z};
  output[1] = {request.coordinate.x, request.coordinate.z - 1};
  output[2] = {request.coordinate.x - 1, request.coordinate.z - 1};
  return 3;
}

WorldChunkActivation3D TerrainVegetationWorldChunkActivationTarget3D::
    ActivatePersistentDependencies(
        WorldChunkRequest3D request, const WorldChunkPayload3D &value,
        const WorldChunkDeltaSnapshot3D *delta,
        std::span<const WorldChunkDeltaSnapshot3D> dependencies) {
  if (!OnOwnerThread() || !ValidSettings(settings_))
    return {};
  const auto *basePayload = Payload(value);
  ComposedTerrainVegetationChunk3D composed{};
  if (!basePayload ||
      !ComposeTerrainVegetationChunkDelta3D(*basePayload, delta, composed))
    return {};
  for (const auto &dependency : dependencies) {
    const auto dx = dependency.key.chunk.x - composed.payload.globalChunk.x;
    const auto dz = dependency.key.chunk.z - composed.payload.globalChunk.z;
    if ((dx != -1 && dx != 0) || (dz != -1 && dz != 0) ||
        (dx == 0 && dz == 0))
      return {};
    for (const auto &operation : dependency.operations) {
      const auto *edit = std::get_if<SetTerrainHeightDelta3D>(&operation.payload);
      if (!edit)
        continue;
      std::uint32_t localX{}, localZ{};
      if (dx == -1 && dz == 0 && edit->x == settings_.terrainCells) {
        localX = 0;
        localZ = edit->z;
      } else if (dx == 0 && dz == -1 &&
                 edit->z == settings_.terrainCells) {
        localX = edit->x;
        localZ = 0;
      } else if (dx == -1 && dz == -1 &&
                 edit->x == settings_.terrainCells &&
                 edit->z == settings_.terrainCells) {
        localX = localZ = 0;
      } else {
        continue;
      }
      if (!composed.payload.heightField.SetHeight(localX, localZ, edit->height))
        return {};
    }
  }
  composed.payload.heightField.RecalculateRange();
  composed.payload.terrain = composed.payload.heightField.CreateMesh();
  if (!composed.payload.terrain.Valid())
    return {};
  const auto *payload = &composed.payload;
  if (!payload ||
      payload->globalChunk != GlobalChunkCoordinate3D{request.coordinate.x, 0,
                                                      request.coordinate.z} ||
      !next_ || next_ == (std::numeric_limits<std::uint64_t>::max)())
    return {};
  Vec3 chunkOrigin{};
  if (!ToLocalPosition3D({payload->globalChunk, {}}, origin_, chunkOrigin,
                         settings_.chunkSize))
    return {};
  Record record{};
  record.request = request;
  std::size_t sharedBytes{};
  bool sharedAcquired{};
  try {
    if (!EnsureShared(*payload, !composed.buildings.empty(), sharedBytes))
      return {};
    sharedAcquired = true;
    if (FailCheckpoint(failureCheckpoint_))
      throw std::runtime_error("injected");
    const auto before = engine_.GetRendererStats();
    record.terrain = engine_.CreateMeshResource3D(payload->terrain,
                                                  MeshCpuRetention3D::Discard);
    if (!record.terrain)
      throw std::runtime_error("terrain upload failed");
    if (HasTextures(settings_.terrainTextures)) {
      record.terrainTextures =
          engine_.CreateMaterialTextureSet3D(settings_.terrainTextures);
      if (!record.terrainTextures)
        throw std::runtime_error("terrain textures failed");
    }
    if (FailCheckpoint(failureCheckpoint_))
      throw std::runtime_error("injected");
    std::vector<SceneEntityDefinition3D> definitions(composed.buildings.size() +
                                                     1);
    auto &definition = definitions.front();
    definition.transform = SceneTransformComponent3D{};
    definition.transform->value.position = chunkOrigin;
    definition.renderable =
        SceneMeshRenderable3D{{},
                              record.terrain,
                              {},
                              40,
                              0,
                              MeshLodDistanceMetric3D::Center,
                              Material3D{{1, 1, 1, 1},
                                         {},
                                         true,
                                         record.terrainTextures,
                                         .25f,
                                         true,
                                         MaterialClass3D::Terrain},
                              {},
                              1,
                              true};
    for (std::size_t index = 0; index < composed.buildings.size(); ++index) {
      const auto &building = composed.buildings[index];
      if (building.assetId != settings_.buildingAssetId ||
          !shared_.buildingMesh)
        throw std::runtime_error("building asset mismatch");
      Vec3 position{};
      if (!ToLocalPosition3D(building.position, origin_, position,
                             settings_.chunkSize))
        throw std::runtime_error("building global position invalid");
      auto &buildingDefinition = definitions[index + 1];
      buildingDefinition.transform = SceneTransformComponent3D{};
      buildingDefinition.transform->value = {position, building.rotation,
                                             building.scale};
      buildingDefinition.renderable = SceneMeshRenderable3D{
          shared_.lease,
          shared_.buildingMesh,
          {},
          80,
          0,
          MeshLodDistanceMetric3D::Center,
          Material3D{{1, 1, 1, 1}, {}, true, shared_.buildingTextures, .25f},
          {},
          1,
          true};
    }
    if (!scene_.CreateBatchAtomic(definitions, record.entities))
      throw std::runtime_error("terrain and building publish failed");
    if (physics_) {
      for (std::size_t index = 0; index < composed.buildings.size(); ++index) {
        const auto *transform = scene_.Transform(record.entities[index + 1]);
        if (!transform)
          throw std::runtime_error("building transform missing");
        RigidBodySettings3D body{};
        body.type = RigidBodyType3D::Static;
        body.transform = transform->value;
        body.colliderExtents =
            TransformBounds(shared_.buildingMesh.bounds, body.transform)
                .extents;
        const auto handle = physics_->CreateBody(body);
        if (!handle)
          throw std::runtime_error("building physics publish failed");
        record.buildingBodies.push_back(handle);
      }
    }
    if (FailCheckpoint(failureCheckpoint_))
      throw std::runtime_error("injected");
    std::vector<VegetationInstance3D> localVegetation = payload->vegetation;
    const Matrix4 translation = CreateTranslationMatrix(chunkOrigin);
    for (auto &instance : localVegetation) {
      instance.transform = translation * instance.transform;
      instance.bounds.center = instance.bounds.center + chunkOrigin;
    }
    std::array<VegetationAsset3D, 1> assets{
        {{{shared_.meshes[0].handle, shared_.meshes[1].handle,
           shared_.meshes[2].handle}}}};
    if (!vegetation_.AttachChunkAtomic(request.requestId, assets,
                                       localVegetation, &record.vegetation))
      throw std::runtime_error("vegetation publish failed");
    if (FailCheckpoint(failureCheckpoint_))
      throw std::runtime_error("injected");
    record.receipt = engine_.SubmitPendingUploads3D();
    record.heightField = payload->heightField;
    const auto after = engine_.GetRendererStats();
    record.physicalBytes =
        sharedBytes + Delta(after.residentMeshBytes, before.residentMeshBytes) +
        Delta(after.residentTextureAllocationBytes,
              before.residentTextureAllocationBytes);
    const auto id = next_++;
    records_.emplace(id, std::move(record));
    return {id, records_.at(id).physicalBytes};
  } catch (...) {
    if (record.vegetation)
      vegetation_.DetachChunk(record.vegetation);
    for (const auto body : record.buildingBodies)
      if (physics_)
        physics_->DestroyBody(body);
    if (!record.entities.empty())
      scene_.DestroyBatchAtomic(record.entities);
    if (record.terrainTextures)
      engine_.DestroyMaterialTextureSet3D(record.terrainTextures);
    if (record.terrain.handle)
      engine_.DestroyMesh3D(record.terrain.handle);
    if (sharedAcquired && shared_.references && !--shared_.references) {
      for (auto &mesh : shared_.meshes)
        if (mesh.handle)
          engine_.DestroyMesh3D(mesh.handle);
      if (shared_.textures)
        engine_.DestroyMaterialTextureSet3D(shared_.textures);
      if (shared_.buildingMesh.handle)
        engine_.DestroyMesh3D(shared_.buildingMesh.handle);
      if (shared_.buildingTextures)
        engine_.DestroyMaterialTextureSet3D(shared_.buildingTextures);
      shared_ = {};
    }
    return {};
  }
}
bool TerrainVegetationWorldChunkActivationTarget3D::IsActivationComplete(
    WorldChunkActivation3D activation) const {
  if (!OnOwnerThread())
    return false;
  const auto found = records_.find(activation.value);
  return found != records_.end() && !found->second.retiring &&
         engine_.IsUploadComplete3D(found->second.receipt);
}
void TerrainVegetationWorldChunkActivationTarget3D::ReleaseRecord(
    Record &record) {
  if (record.released)
    return;
  if (record.vegetation)
    vegetation_.DetachChunk(record.vegetation);
  for (const auto body : record.buildingBodies)
    if (physics_)
      physics_->DestroyBody(body);
  if (!record.entities.empty())
    scene_.DestroyBatchAtomic(record.entities);
  if (record.terrainTextures)
    engine_.DestroyMaterialTextureSet3D(record.terrainTextures);
  if (record.terrain.handle)
    engine_.DestroyMesh3D(record.terrain.handle);
  if (shared_.references && !--shared_.references) {
    for (auto &mesh : shared_.meshes)
      if (mesh.handle)
        engine_.DestroyMesh3D(mesh.handle);
    if (shared_.textures)
      engine_.DestroyMaterialTextureSet3D(shared_.textures);
    if (shared_.buildingMesh.handle)
      engine_.DestroyMesh3D(shared_.buildingMesh.handle);
    if (shared_.buildingTextures)
      engine_.DestroyMaterialTextureSet3D(shared_.buildingTextures);
    shared_ = {};
  }
  record.released = true;
}
void TerrainVegetationWorldChunkActivationTarget3D::Rollback(
    WorldChunkActivation3D activation) {
  if (!OnOwnerThread())
    return;
  const auto found = records_.find(activation.value);
  if (found == records_.end())
    return;
  ReleaseRecord(found->second);
  records_.erase(found);
}
WorldChunkActivation3D
TerrainVegetationWorldChunkActivationTarget3D::BeginRetire(
    WorldChunkActivation3D activation) {
  if (!OnOwnerThread())
    return {};
  const auto found = records_.find(activation.value);
  if (found == records_.end())
    return {};
  ReleaseRecord(found->second);
  found->second.retiring = true;
  return activation;
}
bool TerrainVegetationWorldChunkActivationTarget3D::IsRetirementComplete(
    WorldChunkActivation3D retirement) const {
  if (!OnOwnerThread())
    return false;
  const auto found = records_.find(retirement.value);
  if (found == records_.end())
    return true;
  if (!found->second.retiring ||
      engine_.GetRendererStats().pendingGpuReleaseBytes)
    return false;
  records_.erase(found);
  return true;
}
void TerrainVegetationWorldChunkActivationTarget3D::WaitIdle() {
  if (!OnOwnerThread())
    return;
  engine_.WaitRendererIdle3D();
  for (auto it = records_.begin(); it != records_.end();)
    if (it->second.retiring) {
      ReleaseRecord(it->second);
      it = records_.erase(it);
    } else
      ++it;
}
std::size_t TerrainVegetationWorldChunkActivationTarget3D::TrimUnusedMemory() {
  if (!OnOwnerThread())
    return 0;
  const auto a = records_.bucket_count();
  records_.rehash(records_.size());
  return a > records_.bucket_count()
             ? (a - records_.bucket_count()) * sizeof(void *)
             : 0;
}
std::size_t TerrainVegetationWorldChunkActivationTarget3D::PendingRetirements()
    const noexcept {
  std::size_t n{};
  for (const auto &[id, r] : records_) {
    (void)id;
    if (r.retiring)
      ++n;
  }
  return n;
}

bool TerrainVegetationWorldChunkActivationTarget3D::ApplyTerrainHeightEditAtomic(
    WorldPartition3D &partition, GlobalChunkCoordinate3D chunk,
    std::uint32_t x, std::uint32_t z, float height,
    WorldChunkDeltaSnapshot3D ownerSnapshot) {
  if (!OnOwnerThread() || !std::isfinite(height) ||
      x > settings_.terrainCells || z > settings_.terrainCells ||
      (x == 0 && chunk.x == (std::numeric_limits<std::int64_t>::min)()) ||
      (z == 0 && chunk.z == (std::numeric_limits<std::int64_t>::min)()))
    return false;
  const auto owner = CanonicalTerrainSampleOwner3D(
      chunk, x, z, settings_.terrainCells);
  if (owner.y != 0 || owner.x < (std::numeric_limits<int>::min)() ||
      owner.x > (std::numeric_limits<int>::max)() ||
      owner.z < (std::numeric_limits<int>::min)() ||
      owner.z > (std::numeric_limits<int>::max)())
    return false;

  const std::uint32_t ownerX = x == 0 ? settings_.terrainCells : x;
  const std::uint32_t ownerZ = z == 0 ? settings_.terrainCells : z;
  struct Replacement {
    Record *record{};
    HeightField3D heightField{};
    MeshResource3D mesh{};
    SceneMeshRenderable3D oldRenderable{}, newRenderable{};
  };
  std::vector<Replacement> replacements;
  replacements.reserve(4);
  const auto addReplacement = [&](GlobalChunkCoordinate3D coordinate,
                                  std::uint32_t localX,
                                  std::uint32_t localZ) -> bool {
    for (auto &[id, record] : records_) {
      (void)id;
      if (record.retiring || record.request.coordinate.x != coordinate.x ||
          record.request.coordinate.z != coordinate.z)
        continue;
      if (record.entities.empty() ||
          !scene_.Renderable(record.entities.front()))
        return false;
      Replacement replacement{};
      replacement.record = &record;
      replacement.heightField = record.heightField;
      if (!replacement.heightField.SetHeight(localX, localZ, height))
        return false;
      replacement.heightField.RecalculateRange();
      replacement.oldRenderable = *scene_.Renderable(record.entities.front());
      replacements.push_back(std::move(replacement));
      return true;
    }
    return true;
  };
  if (!addReplacement(owner, ownerX, ownerZ))
    return false;
  if (ownerX == settings_.terrainCells &&
      !addReplacement({owner.x + 1, 0, owner.z}, 0, ownerZ))
    return false;
  if (ownerZ == settings_.terrainCells &&
      !addReplacement({owner.x, 0, owner.z + 1}, ownerX, 0))
    return false;
  if (ownerX == settings_.terrainCells &&
      ownerZ == settings_.terrainCells &&
      !addReplacement({owner.x + 1, 0, owner.z + 1}, 0, 0))
    return false;
  // owner 不在常駐集合時，不能建立只有日誌而沒有即時世界更新的半提交。
  const bool hasOwner = std::ranges::any_of(
      replacements, [&](const Replacement &replacement) {
        return replacement.record->request.coordinate.x == owner.x &&
               replacement.record->request.coordinate.z == owner.z;
      });
  if (!hasOwner)
    return false;

  for (auto &replacement : replacements) {
    replacement.mesh = engine_.CreateMeshResource3D(
        replacement.heightField.CreateMesh(), MeshCpuRetention3D::Discard);
    if (!replacement.mesh) {
      for (auto &prepared : replacements)
        if (prepared.mesh.handle)
          engine_.DestroyMesh3D(prepared.mesh.handle);
      return false;
    }
    replacement.newRenderable = replacement.oldRenderable;
    replacement.newRenderable.nearMesh = replacement.mesh;
  }

  if (ownerSnapshot.lastSequence ==
          (std::numeric_limits<std::uint64_t>::max)() ||
      !ownerSnapshot.durableGeneration) {
    for (auto &replacement : replacements)
      engine_.DestroyMesh3D(replacement.mesh.handle);
    return false;
  }
  const auto sequence = ++ownerSnapshot.lastSequence;
  WorldChunkDeltaOperation3D operation{};
  operation.sequence = sequence;
  operation.dirtyGeneration = ownerSnapshot.durableGeneration;
  operation.operationId = {Hash(static_cast<std::uint64_t>(owner.x) ^ sequence),
                           Hash(static_cast<std::uint64_t>(owner.z) ^
                                (sequence << 1))};
  operation.payload = SetTerrainHeightDelta3D{ownerX, ownerZ, height};
  ownerSnapshot.operations.push_back(std::move(operation));
  if (!partition.SetChunkDelta(
          {static_cast<int>(owner.x), static_cast<int>(owner.z)},
          std::move(ownerSnapshot))) {
    for (auto &replacement : replacements)
      engine_.DestroyMesh3D(replacement.mesh.handle);
    return false;
  }

  for (auto &replacement : replacements) {
    // 上方已驗證 handle 與 component；此提交不配置新 ECS slot。
    if (!scene_.SetRenderable(replacement.record->entities.front(),
                              replacement.newRenderable))
      std::terminate();
    engine_.DestroyMesh3D(replacement.record->terrain.handle);
    replacement.record->terrain = replacement.mesh;
    replacement.record->heightField = std::move(replacement.heightField);
  }
  return true;
}

std::optional<float>
TerrainVegetationWorldChunkActivationTarget3D::QueryTerrainHeight(
    GlobalChunkCoordinate3D chunk, std::uint32_t x,
    std::uint32_t z) const noexcept {
  if (!OnOwnerThread())
    return std::nullopt;
  for (const auto &[id, record] : records_) {
    (void)id;
    if (!record.retiring && record.request.coordinate.x == chunk.x &&
        record.request.coordinate.z == chunk.z &&
        x < record.heightField.Settings().width &&
        z < record.heightField.Settings().depth)
      return record.heightField.Height(x, z);
  }
  return std::nullopt;
}
} // namespace Aevrynd
