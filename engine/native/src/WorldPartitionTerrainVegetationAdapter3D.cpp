#include <Aevrynd/WorldPartitionTerrainVegetationAdapter3D.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Aevrynd {
namespace {
constexpr std::size_t GpuResourceAlignment = 64u * 1024u;
constexpr std::size_t MaximumHlodProxyBytesPerCluster=4u*1024u*1024u,MaximumHlodProxyBytes=256u*1024u*1024u;
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
bool AddStringCapacity(std::size_t &bytes, const std::string &value) {
  std::size_t amount{};
  return Multiply(value.capacity(), sizeof(char), amount) && Add(bytes, amount);
}
bool AddPathCapacity(std::size_t &bytes, const std::filesystem::path &value) {
  std::size_t amount{};
  return Multiply(value.native().capacity(),
                  sizeof(std::filesystem::path::value_type), amount) &&
         Add(bytes, amount);
}
bool AddTextureDefinitionCapacity(
    std::size_t &bytes, const MaterialTextureSetDefinition3D &value) {
  if (!AddPathCapacity(bytes, value.baseColorPath) ||
      !AddPathCapacity(bytes, value.roughnessPath) ||
      !AddPathCapacity(bytes, value.normalPath))
    return false;
  for (const auto &integrity : value.integrity)
    if (!AddStringCapacity(bytes, integrity.expectedSha256))
      return false;
  for (const auto &layer : value.additionalLayers) {
    if (!AddPathCapacity(bytes, layer.baseColorPath) ||
        !AddPathCapacity(bytes, layer.roughnessPath) ||
        !AddPathCapacity(bytes, layer.normalPath))
      return false;
    for (const auto &integrity : layer.integrity)
      if (!AddStringCapacity(bytes, integrity.expectedSha256))
        return false;
  }
  return true;
}
bool AddPreparedProxyCapacity(std::size_t &bytes,
                              const PreparedHlodProxyAsset3D &value) {
  if (!AddStringCapacity(bytes, value.logicalName) ||
      !AddStringCapacity(bytes, value.prepared.definition.name) ||
      !AddPathCapacity(bytes, value.prepared.definition.meshPath) ||
      !AddStringCapacity(bytes, value.prepared.definition.material.name) ||
      !AddTextureDefinitionCapacity(bytes,
                                    value.prepared.definition.material.textures) ||
      !AddStringCapacity(bytes, value.prepared.assetKey.canonical))
    return false;
  for (const auto &lod : value.prepared.definition.importedLods)
    if (!AddPathCapacity(bytes, lod.meshPath))
      return false;
  for (std::uint32_t index = 0; index < value.prepared.levelCount; ++index) {
    std::size_t amount{};
    if (!Multiply(value.prepared.cpuMeshes[index].vertices.capacity(),
                  sizeof(Vertex3D), amount) ||
        !Add(bytes, amount) ||
        !Multiply(value.prepared.cpuMeshes[index].indices.capacity(),
                  sizeof(std::uint32_t), amount) ||
        !Add(bytes, amount))
      return false;
  }
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
         value.vegetationPerChunk <= MaximumMeshInstancesPerFrame3D &&
         value.maximumHlodProxyBytes <= MaximumHlodProxyBytes;
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

void TerrainVegetationWorldChunkActivationTarget3D::InvalidateFrozenHlodFrame3D(
    bool activationMutation) noexcept {
  hlodFrameFrozen_ = false;
  frozenHlodToken_ = {};
  hlodRuntimeStats_.frozen = false;
  const auto advance = [&](std::uint64_t &value) {
    if (value == (std::numeric_limits<std::uint64_t>::max)()) {
      hlodTokenExhausted_ = true;
      return;
    }
    ++value;
  };
  advance(hlodWorldEpoch_);
  if (activationMutation) {
    advance(hlodActivationIdentity_);
  }
}

std::size_t TerrainVegetationWorldChunkActivationTarget3D::PreparationBytes(
    WorldChunkRequest3D, const WorldChunkPayload3D &value) const {
  std::size_t bytes = value.cpuBytes;
  const auto *payload = Payload(value);
  if (!payload || !payload->hlodManifest)
    return bytes;
  std::size_t amount{};
  if (!Multiply(payload->hlodManifest->Clusters().size(),
                sizeof(TerrainVegetationHlodCluster3D), amount) ||
      !Add(bytes, amount))
    return SIZE_MAX;
  for (const auto &cluster : payload->hlodManifest->Clusters()) {
    if (!Multiply(cluster.sources.capacity(),
                  sizeof(TerrainVegetationHlodSource3D), amount) ||
        !Add(bytes, amount))
      return SIZE_MAX;
    if (!AddPreparedProxyCapacity(bytes, cluster.proxy))
      return SIZE_MAX;
    for (const auto &source : cluster.sources)
      if (!Add(bytes, source.logicalName.capacity()))
        return SIZE_MAX;
  }
  return bytes;
}

std::shared_ptr<const TerrainVegetationChunkPayload3D::HlodManifest>
BuildTerrainVegetationHlodManifest3D(
    std::span<const TerrainVegetationHlodCluster3D> clusters,
    const HlodData3D &data) {
  try {
    if (clusters.size() != data.Clusters().size() || clusters.empty() ||
        !data.ContentHash())
      return {};
    auto result = std::shared_ptr<TerrainVegetationChunkPayload3D::HlodManifest>(
        new TerrainVegetationChunkPayload3D::HlodManifest);
    result->clusters_.reserve(clusters.size());
    std::uint64_t hash = 1469598103934665603ull;
    const auto byte = [&](std::uint8_t value) {
      hash ^= value;
      hash *= 1099511628211ull;
    };
    const auto scalar = [&](std::uint64_t value) {
      for (unsigned index = 0; index < 8; ++index)
        byte(static_cast<std::uint8_t>(value >> (index * 8)));
    };
    const auto text = [&](std::string_view value) {
      scalar(value.size());
      for (const unsigned char character : value)
        byte(character);
    };
    const auto floating = [&](float value) {
      if (value == 0.f)
        value = 0.f;
      scalar(std::bit_cast<std::uint32_t>(value));
    };
    const auto vector = [&](Vec3 value) {
      floating(value.x);
      floating(value.y);
      floating(value.z);
    };
    scalar(data.ContentHash());
    StableObjectId3D worldId{};
    for (std::size_t index = 0; index < clusters.size(); ++index) {
      const auto &value = clusters[index];
      const auto &asset = data.Clusters()[index];
      const auto &instance = data.Instances()[index];
      const auto expectedSources = data.Sources(index);
      const auto expectedContracts = data.SourceResourceContracts(index);
      if (value.id != asset.id || value.generation != asset.generation ||
          !ValidatePreparedHlodProxyDescriptor3D(value.proxy) ||
          value.proxy.clusterId != value.id ||
          value.proxy.clusterGeneration != value.generation ||
          value.proxy.chunk != instance.globalChunk ||
          value.sources.size() != expectedSources.size() ||
          value.sources.size() != expectedContracts.size())
        return {};
      if (!index)
        worldId = value.proxy.worldId;
      else if (value.proxy.worldId != worldId)
        return {};
      scalar(value.id.high);
      scalar(value.id.low);
      scalar(value.generation);
      scalar(value.proxy.worldId.high);
      scalar(value.proxy.worldId.low);
      scalar(static_cast<std::uint64_t>(value.proxy.chunk.x));
      scalar(static_cast<std::uint64_t>(value.proxy.chunk.y));
      scalar(static_cast<std::uint64_t>(value.proxy.chunk.z));
      text(value.proxy.logicalName);
      scalar(value.proxy.prepared.assetKey.contentIdentity);
      text(value.proxy.prepared.assetKey.canonical);
      scalar(value.proxy.reservedPhysicalBytes);
      TerrainVegetationHlodCluster3D ownedValue = value;
      for (std::size_t source = 0; source < value.sources.size(); ++source) {
        const auto &item = value.sources[source];
        const auto &expected = expectedContracts[source];
        if (item.reference != expectedSources[source] || !item.generation ||
            item.reference != expected.reference ||
            item.generation != expected.generation ||
            item.logicalName.empty() ||
            item.logicalName.size() > MaximumHlodProxyLogicalNameBytes3D ||
            item.logicalName.find("..") != std::string::npos ||
            item.logicalName.find_first_of("/\\\r\n\t") !=
                std::string::npos ||
            !std::isfinite(item.position.offset.x) ||
            !std::isfinite(item.position.offset.y) ||
            !std::isfinite(item.position.offset.z) ||
            !std::isfinite(item.rotation.x) ||
            !std::isfinite(item.rotation.y) ||
            !std::isfinite(item.rotation.z) ||
            !std::isfinite(item.scale.x) || !std::isfinite(item.scale.y) ||
            !std::isfinite(item.scale.z) || item.scale.x == 0 ||
            item.scale.y == 0 || item.scale.z == 0)
          return {};
        const bool suppliedResourceContract = item.meshHandle || item.materialHandle ||
            item.meshGeneration || item.materialGeneration ||
            item.resourceFingerprint;
        if (suppliedResourceContract &&
            (item.meshHandle != expected.meshHandle ||
             item.materialHandle != expected.materialHandle ||
             item.meshGeneration != expected.meshGeneration ||
             item.materialGeneration != expected.materialGeneration ||
             item.resourceFingerprint != expected.resourceFingerprint ||
             CanonicalHlodShadowSemantic3D(item.shadow) != expected.shadow))
          return {};
        auto &ownedSource = ownedValue.sources[source];
        ownedSource.meshHandle = expected.meshHandle;
        ownedSource.materialHandle = expected.materialHandle;
        ownedSource.meshGeneration = expected.meshGeneration;
        ownedSource.materialGeneration = expected.materialGeneration;
        ownedSource.resourceFingerprint = expected.resourceFingerprint;
        ownedSource.shadow = expected.shadow;
        scalar(item.reference.globalObjectId.high);
        scalar(item.reference.globalObjectId.low);
        scalar(item.generation);
        text(item.logicalName);
        scalar(expected.meshHandle);
        scalar(expected.materialHandle);
        scalar(expected.meshGeneration);
        scalar(expected.materialGeneration);
        scalar(expected.resourceFingerprint);
        scalar(HlodShadowSemanticFingerprint3D(expected.shadow));
        scalar(static_cast<std::uint64_t>(item.position.chunk.x));
        scalar(static_cast<std::uint64_t>(item.position.chunk.y));
        scalar(static_cast<std::uint64_t>(item.position.chunk.z));
        vector(item.position.offset);
        vector(item.rotation);
        vector(item.scale);
      }
      result->clusters_.push_back(std::move(ownedValue));
    }
    result->hlodDataHash_ = data.ContentHash();
    result->contentHash_ = hash ? hash : 1;
    return result;
  } catch (...) {
    return {};
  }
}

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
          engine, scene, vegetation, nullptr, nullptr, std::move(settings)) {}
TerrainVegetationWorldChunkActivationTarget3D::
    TerrainVegetationWorldChunkActivationTarget3D(
        Engine &engine, SceneECSRegistry3D &scene,
        ChunkedVegetationWorld3D &vegetation, PhysicsWorld3D *physics,
        TerrainVegetationChunkSettings3D settings)
    : TerrainVegetationWorldChunkActivationTarget3D(engine,scene,vegetation,nullptr,physics,std::move(settings)){}
TerrainVegetationWorldChunkActivationTarget3D::TerrainVegetationWorldChunkActivationTarget3D(Engine& engine,SceneECSRegistry3D& scene,ChunkedVegetationWorld3D& vegetation,StaticMeshAssetLibrary3D* assets,PhysicsWorld3D* physics,TerrainVegetationChunkSettings3D settings)
    : engine_(engine), scene_(scene), vegetation_(vegetation),
      physics_(physics), assets_(assets), settings_(std::move(settings)),
      ownerThread_(std::this_thread::get_id()), engineRetirementSource_(engine),
      retirementSource_(&engineRetirementSource_) {}
TerrainVegetationWorldChunkActivationTarget3D::
    TerrainVegetationWorldChunkActivationTarget3D(
        Engine &engine, SceneECSRegistry3D &scene,
        ChunkedVegetationWorld3D &vegetation,
        StaticMeshAssetLibrary3D *assets, PhysicsWorld3D *physics,
        IGpuRetirementReceiptSource3D &retirementSource,
        TerrainVegetationChunkSettings3D settings)
    : engine_(engine), scene_(scene), vegetation_(vegetation), physics_(physics),
      assets_(assets), settings_(std::move(settings)),
      ownerThread_(std::this_thread::get_id()), engineRetirementSource_(engine),
      retirementSource_(&retirementSource) {}
bool TerrainVegetationWorldChunkActivationTarget3D::SetWorldOriginSnapshot3D(
    WorldOrigin3D origin) noexcept {
  if (!OnOwnerThread() || !origin.generation ||
      !NormalizeGlobalPosition3D(origin.position, settings_.chunkSize))
    return false;
  origin_ = origin;
  hasLastHlodCameraPosition_ = false;
  InvalidateFrozenHlodFrame3D();
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
  if (OnOwnerThread() && preparedOrigin_ == event.current) {
    origin_ = preparedOrigin_;
    hasLastHlodCameraPosition_ = false;
    InvalidateFrozenHlodFrame3D();
  }
  preparedOrigin_ = {};
}
TerrainVegetationWorldChunkActivationTarget3D::
    ~TerrainVegetationWorldChunkActivationTarget3D() {
  engine_.WaitRendererIdle3D();
  while (!records_.empty()) {
    const auto before = records_.size();
    for (auto iterator = records_.begin(); iterator != records_.end();) {
      if (!ReleaseHlod(iterator->second)) {
        ++iterator;
        continue;
      }
      ReleaseRecord(iterator->second);
      iterator = records_.erase(iterator);
    }
    if (records_.size() == before)
      std::terminate();
  }
  engine_.WaitRendererIdle3D();
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
  if (assets_ && (payload->hlod || payload->hlodManifest)) {
    if (!payload->hlod || !payload->hlodManifest ||
        !payload->hlodManifest->ContentHash() ||
        payload->hlodManifest->HlodDataHash() !=
            payload->hlod->ContentHash() ||
        payload->hlodManifest->Clusters().size() !=
            payload->hlod->Clusters().size())
      return SIZE_MAX;
    std::size_t hlodBytes{};
    for (std::size_t index = 0;
         index < payload->hlodManifest->Clusters().size(); ++index) {
      const auto &cluster = payload->hlodManifest->Clusters()[index];
      const auto &asset = payload->hlod->Clusters()[index];
      const auto &instance = payload->hlod->Instances()[index];
      if (cluster.id != asset.id || cluster.generation != asset.generation ||
          cluster.proxy.worldId != settings_.worldId ||
          cluster.proxy.chunk != instance.globalChunk ||
          instance.globalChunk != payload->globalChunk)
        return SIZE_MAX;
      if (asset.compatibility != HlodCompatibility3D::OpaqueCompatible)
        continue;
      const auto reservation = cluster.proxy.reservedPhysicalBytes;
      if (reservation > MaximumHlodProxyBytesPerCluster ||
          reservation > settings_.maximumHlodProxyBytes - hlodBytes)
        continue;
      hlodBytes += reservation;
    }
    if (!Add(bytes, hlodBytes))
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
  if (assets_ && (payload->hlod || payload->hlodManifest)) {
    if (!payload->hlod || !payload->hlodManifest ||
        !payload->hlodManifest->ContentHash() ||
        payload->hlodManifest->HlodDataHash() !=
            payload->hlod->ContentHash() ||
        payload->hlodManifest->Clusters().size() !=
            payload->hlod->Clusters().size())
      return {};
    for (std::size_t index = 0;
         index < payload->hlodManifest->Clusters().size(); ++index) {
      const auto &manifest = payload->hlodManifest->Clusters()[index];
      const auto &asset = payload->hlod->Clusters()[index];
      const auto &instance = payload->hlod->Instances()[index];
      if (manifest.id != asset.id ||
          manifest.generation != asset.generation ||
          manifest.proxy.worldId != settings_.worldId ||
          manifest.proxy.chunk != instance.globalChunk ||
          instance.globalChunk != payload->globalChunk)
        return {};
    }
  }
  Vec3 chunkOrigin{};
  if (!ToLocalPosition3D({payload->globalChunk, {}}, origin_, chunkOrigin,
                         settings_.chunkSize))
    return {};
  Record record{};
  record.request = request;
  const HlodIntegrationStats3D hlodStatsBefore=hlodStats_;
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
    auto &terrainDefinition = definitions.front();
    terrainDefinition.transform = SceneTransformComponent3D{};
    terrainDefinition.transform->value.position = chunkOrigin;
    terrainDefinition.renderable =
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
    if (assets_ && payload->hlod && payload->hlodManifest) {
      if (payload->hlodManifest->HlodDataHash() != payload->hlod->ContentHash())
        throw std::runtime_error("HLOD manifest mismatch");
      record.hlod.emplace();
      auto &attachment = *record.hlod;
      attachment.data = payload->hlod;
      attachment.manifest = payload->hlodManifest;
      attachment.clusters.reserve(payload->hlodManifest->Clusters().size());
      hlodStats_.inputClusters += payload->hlodManifest->Clusters().size();
      std::size_t admittedReservation{};
      for (std::size_t clusterIndex = 0;
           clusterIndex < payload->hlodManifest->Clusters().size();
           ++clusterIndex) {
        const auto &manifest = payload->hlodManifest->Clusters()[clusterIndex];
        const auto &asset = payload->hlod->Clusters()[clusterIndex];
        const auto &shadowContract = payload->hlod->ShadowContracts()[clusterIndex];
        const bool sourceOnly =
            asset.compatibility != HlodCompatibility3D::OpaqueCompatible ||
            !shadowContract.sourceProxyCompatible;
        if (sourceOnly) {
          ++hlodStats_.incompatibleFallbacks;
          if (!shadowContract.sourceProxyCompatible)
            ++hlodStats_.shadowContractRejects;
          ++hlodStats_.sourceOnlyFallbacks;
          continue;
        }
        if (manifest.proxy.reservedPhysicalBytes >
                MaximumHlodProxyBytesPerCluster ||
            manifest.proxy.reservedPhysicalBytes >
                settings_.maximumHlodProxyBytes - admittedReservation) {
          ++hlodStats_.budgetFallbacks;
          ++hlodStats_.sourceOnlyFallbacks;
          continue;
        }
        Record::HlodClusterAttachment cluster{};
        cluster.id = manifest.id;
        cluster.generation = manifest.generation;
        cluster.dataIndex = clusterIndex;
        cluster.reservedBytes = manifest.proxy.reservedPhysicalBytes;
        cluster.sources.reserve(manifest.sources.size());
        std::vector<SceneEntityDefinition3D> sourceDefinitions;
        sourceDefinitions.reserve(manifest.sources.size());
        bool sourcesValid = true;
        bool shadowSemanticMismatch = false;
        bool sourceResourceMismatch = false;
        const auto expectedReferences = payload->hlod->Sources(clusterIndex);
        const auto expectedShadowSemantics =
            payload->hlod->SourceShadowSemantics(clusterIndex);
        const auto expectedResourceContracts =
            payload->hlod->SourceResourceContracts(clusterIndex);
        if (expectedReferences.size() != manifest.sources.size() ||
            expectedShadowSemantics.size() != manifest.sources.size() ||
            expectedResourceContracts.size() != manifest.sources.size()) {
          ++hlodStats_.resourceContractRejects;
          ++hlodStats_.sourceOnlyFallbacks;
          continue;
        }
        for (const auto &source : manifest.sources) {
          const auto sourceIndex = sourceDefinitions.size();
          const auto &expectedResource = expectedResourceContracts[sourceIndex];
          if (source.reference != expectedResource.reference ||
              source.generation != expectedResource.generation ||
              source.meshHandle != expectedResource.meshHandle ||
              source.materialHandle != expectedResource.materialHandle ||
              source.meshGeneration != expectedResource.meshGeneration ||
              source.materialGeneration != expectedResource.materialGeneration ||
              source.resourceFingerprint != expectedResource.resourceFingerprint ||
              source.shadow != expectedResource.shadow) {
            sourcesValid = false;
            sourceResourceMismatch = true;
            break;
          }
          auto lease = assets_->ResolveRenderable(source.logicalName, 1, true);
          Vec3 position{};
          if (!lease || !lease->Valid() ||
              !ToLocalPosition3D(source.position, origin_, position,
                                 settings_.chunkSize)) {
            sourcesValid = false;
            break;
          }
          if (!HlodSourceResourceContractMatchesRenderable3D(
                  expectedResource, lease->Renderable())) {
            sourcesValid = false;
            sourceResourceMismatch = true;
            break;
          }
          SceneEntityDefinition3D definition{};
          definition.transform = SceneTransformComponent3D{
              Transform3D{position, source.rotation, source.scale}};
          definition.renderable = lease->Renderable();
          definition.renderable->identity =
              {source.reference.globalObjectId, source.generation};
          const auto actualShadow = HlodShadowSemanticFromRenderable3D(*definition.renderable);
          if (sourceIndex >= expectedShadowSemantics.size() || !actualShadow ||
              *actualShadow != expectedShadowSemantics[sourceIndex]) {
            sourcesValid = false;
            shadowSemanticMismatch = true;
            break;
          }
          sourceDefinitions.push_back(std::move(definition));
          cluster.sources.push_back(std::move(*lease));
        }
        if (!sourcesValid) {
          if (shadowSemanticMismatch)
            ++hlodStats_.shadowContractRejects;
          else if (sourceResourceMismatch)
            ++hlodStats_.resourceContractRejects;
          else
            ++hlodStats_.staleGenerationRejects;
          ++hlodStats_.sourceOnlyFallbacks;
          continue;
        }
        if (!assets_->AcquireHlodProxyAtomic3D(manifest.proxy, cluster.proxy)) {
          ++hlodStats_.sourceOnlyFallbacks;
          continue;
        }
        const auto rollbackCluster = [&]() noexcept {
          if (cluster.entities)
            (void)scene_.DestroyHlodBatchAtomic3D(cluster.entities);
          for (auto &source : cluster.sources)
            source.Reset();
          if (cluster.proxy)
            (void)assets_->ReleaseHlodProxyAlias3D(cluster.proxy);
        };
        if (FailCheckpoint(failureCheckpoint_)) {
          rollbackCluster();
          throw std::runtime_error("injected HLOD acquire rollback");
        }
        cluster.actualBytes = cluster.proxy.actualPhysicalBytes;
        cluster.aliasBytes = cluster.proxy.logicalName.capacity();
        if (cluster.actualBytes > cluster.reservedBytes) {
          rollbackCluster();
          throw std::runtime_error("HLOD proxy reservation exceeded");
        }
        admittedReservation += cluster.reservedBytes;
        const auto &instance = payload->hlod->Instances()[clusterIndex];
        Vec3 proxyPosition{};
        if (!ToLocalPosition3D(
                {instance.globalChunk, instance.chunkLocalPosition}, origin_,
                proxyPosition, settings_.chunkSize)) {
          rollbackCluster();
          throw std::runtime_error("HLOD proxy transform invalid");
        }
        SceneEntityDefinition3D proxyDefinition{};
        proxyDefinition.transform = SceneTransformComponent3D{
            Transform3D{proxyPosition, instance.rotation, instance.scale}};
        proxyDefinition.renderable = cluster.proxy.renderable.Renderable();
        proxyDefinition.renderable->identity = {
            HlodProxyRenderObjectId3D(cluster.id), cluster.generation};
        const auto actualProxyShadow =
            HlodShadowSemanticFromRenderable3D(*proxyDefinition.renderable);
        if (!actualProxyShadow ||
            *actualProxyShadow != shadowContract.proxy) {
          ++hlodStats_.shadowContractRejects;
          ++hlodStats_.sourceOnlyFallbacks;
          rollbackCluster();
          continue;
        }
        if (!scene_.CreateHlodBatchAtomic3D(cluster.id, cluster.generation,
                                            sourceDefinitions, proxyDefinition,
                                            expectedResourceContracts,
                                            payload->hlod->Resources()[clusterIndex]
                                                .proxyShadow,
                                            shadowContract,
                                            cluster.entities)) {
          rollbackCluster();
          throw std::runtime_error("HLOD Scene publish failed");
        }
        cluster.activeSelection.clusterId = cluster.id;
        cluster.activeSelection.proxyInvert = true;
        cluster.pendingSelection = cluster.activeSelection;
        // Scene 已取得自己的 leases；在任何 publish 後 rollback checkpoint 前先
        // 釋放暫存 definitions，否則 proxyDefinition 仍是 borrower，alias Unload
        // 會失敗並把 proxy/GPU resource 洩漏在失敗路徑。
        sourceDefinitions.clear();
        proxyDefinition.renderable.reset();
        if (FailCheckpoint(failureCheckpoint_)) {
          rollbackCluster();
          throw std::runtime_error("injected HLOD publish rollback");
        }
        if (!cluster.actualBytes)
          ++hlodStats_.sharedProxyDedupHits;
        hlodStats_.proxyReservedBytes += cluster.reservedBytes;
        hlodStats_.proxyActualResidentBytes += cluster.actualBytes;
        hlodStats_.aliasBytes += cluster.aliasBytes;
        hlodStats_.residentSourceEntities += cluster.sources.size();
        ++hlodStats_.residentProxyEntities;
        ++hlodStats_.attachedClusters;
        attachment.clusters.push_back(std::move(cluster));
      }
      if (attachment.clusters.empty())
        record.hlod.reset();
      else {
        attachment.selector = std::make_unique<HlodReferenceSelector3D>();
        if (!attachment.selector->Reserve(attachment.data->Clusters().size()))
          throw std::runtime_error("HLOD runtime selector reserve failed");
      }
    }
    record.receipt = engine_.SubmitPendingUploads3D();
    record.heightField = payload->heightField;
    const auto after = engine_.GetRendererStats();
    record.physicalBytes = sharedBytes;
    if (!Add(record.physicalBytes,
             Delta(after.residentMeshBytes, before.residentMeshBytes)) ||
        !Add(record.physicalBytes,
             Delta(after.residentTextureAllocationBytes,
                   before.residentTextureAllocationBytes)))
      throw std::overflow_error("activation physical byte overflow");
    const auto id = next_;
    const auto [inserted,created]=records_.emplace(id, std::move(record));
    if(!created)throw std::runtime_error("activation record publish failed");
    InvalidateFrozenHlodFrame3D(true);
    ++next_;
    return {id, inserted->second.physicalBytes};
  } catch (...) {
    if(record.hlod)for(auto iterator=record.hlod->clusters.rbegin();iterator!=record.hlod->clusters.rend();++iterator){scene_.DestroyHlodBatchAtomic3D(iterator->entities);for(auto& source:iterator->sources)source.Reset();assets_->ReleaseHlodProxyAlias3D(iterator->proxy);}
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
    hlodStats_=hlodStatsBefore;++hlodStats_.rollbackCount;
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
bool TerrainVegetationWorldChunkActivationTarget3D::HasSharedProxyBorrower(
    const Record::HlodClusterAttachment &cluster) const noexcept {
  const auto owner = cluster.proxy.renderable.Renderable().assetLease.get();
  if (!owner)
    return false;
  for (const auto &[id, record] : records_) {
    (void)id;
    if (!record.hlod)
      continue;
    for (const auto &candidate : record.hlod->clusters) {
      if (&candidate != &cluster && !candidate.aliasReleased &&
          candidate.proxy.renderable.Renderable().assetLease.get() == owner)
        return true;
    }
  }
  return false;
}

bool TerrainVegetationWorldChunkActivationTarget3D::ReleaseHlod(Record &record) {
  if (!record.hlod)
    return true;
  for (auto iterator = record.hlod->clusters.rbegin();
       iterator != record.hlod->clusters.rend(); ++iterator) {
    if (!iterator->sceneDestroyed) {
      if (!scene_.DestroyHlodBatchAtomic3D(iterator->entities))
        return false;
      iterator->sceneDestroyed = true;
      hlodStats_.residentSourceEntities -= iterator->entities.sourceCount;
      --hlodStats_.residentProxyEntities;
      --hlodStats_.attachedClusters;
    }
    if (!iterator->sourcesReset) {
      for (auto &source : iterator->sources)
        source.Reset();
      iterator->sourcesReset = true;
    }
    // 實際物理 delta 的擁有者必須保留退休費用，直到零 delta 的共享 alias 離開。
    // 否則 WorldPartition 會在物理資產仍常駐時提早釋放唯一的 GPU budget charge。
    if (!iterator->aliasReleased && iterator->actualBytes &&
        HasSharedProxyBorrower(*iterator))
      return false;
    if (!iterator->aliasReleased) {
      if (!assets_ || !assets_->ReleaseHlodProxyAlias3D(iterator->proxy))
        return false;
      iterator->aliasReleased = true;
      hlodStats_.proxyReservedBytes -= iterator->reservedBytes;
      hlodStats_.proxyActualResidentBytes -= iterator->actualBytes;
      hlodStats_.aliasBytes -= iterator->aliasBytes;
    }
  }
  record.hlod.reset();
  return true;
}
void TerrainVegetationWorldChunkActivationTarget3D::Rollback(
    WorldChunkActivation3D activation) {
  if (!OnOwnerThread())
    return;
  const auto found = records_.find(activation.value);
  if (found == records_.end())
    return;
  InvalidateFrozenHlodFrame3D(true);
  ReleaseHlod(found->second);
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
  if(found->second.retiring)return activation;
  found->second.retiring = true;
  InvalidateFrozenHlodFrame3D(true);
  if (found->second.hlod && !HideHlodForRetirement(found->second))
    return activation;
  ReleaseRecord(found->second);
  return activation;
}

bool TerrainVegetationWorldChunkActivationTarget3D::HideHlodForRetirement(
    Record &record) {
  if (!record.hlod || record.hlodHidden)
    return true;
    // Scene HLOD 擁有者記錄不可繞過批次 API 修改；第一次可見性寫入前先驗證整批，
    // 避免多叢集退役只隱藏前段後便中止。
    const auto& clusters=record.hlod->clusters;
    for(std::size_t clusterIndex=0;clusterIndex<clusters.size();++clusterIndex){
      if(!scene_.CanSetHlodSelection3D(clusters[clusterIndex].entities,
                                       clusters[clusterIndex].activeSelection))
        return false;
      for(std::size_t entityIndex=0;entityIndex<clusters[clusterIndex].entities.entities.size();++entityIndex){
        const auto entity=clusters[clusterIndex].entities.entities[entityIndex];
        for(std::size_t previousCluster=0;previousCluster<=clusterIndex;++previousCluster){
          const auto limit=previousCluster==clusterIndex?entityIndex:clusters[previousCluster].entities.entities.size();
          for(std::size_t previousEntity=0;previousEntity<limit;++previousEntity)
            if(clusters[previousCluster].entities.entities[previousEntity]==entity)return{};
        }
      }
    }
    // 收據先涵蓋目前所有已提交影格；取得失敗時不可先改變可見性。
    if(!record.retirementGate.Begin(*retirementSource_))return false;
    for(const auto& cluster:clusters)if(!scene_.HideHlodBatchAtomic3D(cluster.entities))std::terminate();
    record.hlodHidden=true;hlodStats_.retiringClusters+=clusters.size();hlodStats_.pendingRetirementReceipts++;for(const auto& cluster:clusters)hlodStats_.retiringProxyBytes+=cluster.actualBytes;
    return true;
}
bool TerrainVegetationWorldChunkActivationTarget3D::IsRetirementComplete(
    WorldChunkActivation3D retirement) const {
  if (!OnOwnerThread())
    return false;
  const auto found = records_.find(retirement.value);
  if (found == records_.end())
    return true;
  if (!found->second.retiring)
    return false;
  if (found->second.hlod && !found->second.hlodHidden) {
    auto &mutableRecord = const_cast<Record &>(found->second);
    auto *self = const_cast<TerrainVegetationWorldChunkActivationTarget3D *>(this);
    if (!self->HideHlodForRetirement(mutableRecord))
      return false;
    self->ReleaseRecord(mutableRecord);
  }
  if (!found->second.hlod &&
      engine_.GetRendererStats().pendingGpuReleaseBytes)
    return false;
  if(found->second.hlod&&!found->second.retirementGate.Complete(*retirementSource_))return false;
  if(found->second.hlod){const auto count=found->second.hlod->clusters.size();std::size_t retiringBytes{};for(const auto& cluster:found->second.hlod->clusters)retiringBytes+=cluster.actualBytes;if(!const_cast<TerrainVegetationWorldChunkActivationTarget3D*>(this)->ReleaseHlod(found->second))return false;hlodStats_.retiringClusters-=count;hlodStats_.retiringProxyBytes-=retiringBytes;--hlodStats_.pendingRetirementReceipts;}
  records_.erase(found);
  return true;
}
void TerrainVegetationWorldChunkActivationTarget3D::WaitIdle() {
  if (!OnOwnerThread())
    return;
  engine_.WaitRendererIdle3D();
  bool progress{};
  do {
    progress = false;
    for (auto it = records_.begin(); it != records_.end();) {
      if (!it->second.retiring) {
        ++it;
        continue;
      }
      std::size_t count{}, bytes{};
      if (it->second.hlod) {
        if (!it->second.hlodHidden &&
            !HideHlodForRetirement(it->second)) {
          ++it;
          continue;
        }
        count = it->second.hlod->clusters.size();
        for (const auto &cluster : it->second.hlod->clusters)
          bytes += cluster.actualBytes;
        if (!ReleaseHlod(it->second)) {
          ++it;
          continue;
        }
        hlodStats_.retiringClusters -= count;
        hlodStats_.retiringProxyBytes -= bytes;
        if (hlodStats_.pendingRetirementReceipts)
          --hlodStats_.pendingRetirementReceipts;
      }
      ReleaseRecord(it->second);
      it = records_.erase(it);
      progress = true;
    }
  } while (progress);
  engine_.WaitRendererIdle3D();
}
std::size_t TerrainVegetationWorldChunkActivationTarget3D::TrimUnusedMemory() {
  if (!OnOwnerThread())
    return 0;
  const auto before = HlodStats().metadataCapacityBytes;
  for (auto &[id, record] : records_) {
    (void)id;
    if (!record.hlod)
      continue;
    for (auto &cluster : record.hlod->clusters) {
      cluster.sources.shrink_to_fit();
      cluster.entities.entities.shrink_to_fit();
    }
    record.hlod->clusters.shrink_to_fit();
  }
  records_.rehash(records_.size());
  const auto after = HlodStats().metadataCapacityBytes;
  hlodStats_.lastReleasedBytes = before > after ? before - after : 0;
  ++hlodStats_.trimCount;
  return hlodStats_.lastReleasedBytes;
}
bool TerrainVegetationWorldChunkActivationTarget3D::SelectHlodFrame3D(
    std::uint64_t frameId, HlodSelectionView3D view,
    const HlodRuntimeSelectionSettings3D &settings) noexcept {
  const auto finite = [](Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
  };
  if (!OnOwnerThread() || hlodTokenExhausted_ || hlodSelectionGeneration_ == (std::numeric_limits<std::uint64_t>::max)() || !frameId || frameId <= hlodRuntimeStats_.frameId ||
      !std::isfinite(settings.cameraTeleportDistance) ||
      settings.cameraTeleportDistance <= 0.f || !finite(view.camera.position))
    return false;
  const bool teleported =
      hasLastHlodCameraPosition_ &&
      Length(view.camera.position - lastHlodCameraPosition_) >=
          settings.cameraTeleportDistance;
  const bool reset = view.cameraCut || view.historyInvalid || teleported;
  view.historyInvalid = reset;
  // Scene HLOD transform 使用 target 的權威 origin；呼叫端不可用不同 origin 產生錯誤選擇。
  view.origin = origin_;
  view.chunkSize = settings_.chunkSize;
  HlodRuntimeFrameStats3D next{};
  next.frameId = frameId;
  next.cameraHistoryReset = reset;

  // 先驗證所有 ownership；任何一般失敗都發生在 selector history 與 Scene 改變前。
  for (const auto &[id, record] : records_) {
    (void)id;
    if (record.retiring || !record.hlod)
      continue;
    const auto &attachment = *record.hlod;
    if (!attachment.selector || !attachment.data)
      return false;
    for (const auto &cluster : attachment.clusters)
      if (cluster.dataIndex >= attachment.data->Clusters().size() ||
          attachment.data->Clusters()[cluster.dataIndex].id != cluster.id ||
          !scene_.MatchesHlodSelection3D(cluster.entities,
                                         cluster.activeSelection))
        return false;
  }

  // 第一遍只計算；在所有批次可安全切換前不改變 Scene 可見性。
  for (std::uint64_t recordId = 1; recordId < next_; ++recordId) {
    const auto found = records_.find(recordId);
    if (found == records_.end() || found->second.retiring ||
        !found->second.hlod)
      continue;
    auto &attachment = *found->second.hlod;
    const auto selected =
        attachment.selector->Select(*attachment.data, view, settings.quality);
    if (selected.size() != attachment.data->Clusters().size())
      std::terminate();
    for (auto &cluster : attachment.clusters) {
      if (selected[cluster.dataIndex].clusterId != cluster.id)
        std::terminate();
      cluster.pendingSelection = selected[cluster.dataIndex];
      if (!cluster.proxy.Valid()) {
        cluster.pendingSelection.mode = HlodSelectionMode3D::Source;
        cluster.pendingSelection.sourceCoverage = 1.f;
        cluster.pendingSelection.proxyCoverage = 0.f;
        cluster.pendingSelection.sourceCoverage8 = 255;
        cluster.pendingSelection.proxyCoverage8 = 0;
      }
      ++next.evaluatedClusters;
      if (cluster.pendingSelection.mode != cluster.activeSelection.mode)
        ++next.requestedTransitions;
    }
  }

  // 第二遍在任何 Scene 寫入前驗證所有目標批次；之後 owner thread 上的提交不會
  // 因 stale handle 留下只切換一半的跨-cluster visibility。
  for (const auto &[id, record] : records_) {
    (void)id;
    if (record.retiring || !record.hlod)
      continue;
    for (const auto &cluster : record.hlod->clusters)
      if (!scene_.CanSetHlodSelection3D(cluster.entities,
                                        cluster.pendingSelection))
        return false;
  }

  // 已啟用模式內的 raster coverage 更新不是可見模式切換，不能消耗公平性
  // 預算，也不能因為預算耗盡而停滯。
  for (auto &[id, record] : records_) {
    (void)id;
    if (record.retiring || !record.hlod)
      continue;
    for (auto &cluster : record.hlod->clusters) {
      if (cluster.pendingSelection.mode != cluster.activeSelection.mode)
        continue;
      if (!scene_.SetHlodSelectionAtomic3D(cluster.entities,
                                           cluster.pendingSelection))
        std::terminate();
      cluster.activeSelection = cluster.pendingSelection;
    }
  }

  std::size_t remaining = settings.maximumVisibilityTransitionsPerFrame;
  const std::uint64_t startActivation =
      nextTransitionActivation_ && nextTransitionActivation_ < next_
          ? nextTransitionActivation_
          : 1;
  const auto applyRange = [&](std::uint64_t first,
                              std::uint64_t last) noexcept {
    for (std::uint64_t recordId = first; recordId < last; ++recordId) {
      const auto found = records_.find(recordId);
      if (found == records_.end() || found->second.retiring ||
          !found->second.hlod)
        continue;
      auto &attachment = *found->second.hlod;
      const std::size_t count = attachment.clusters.size();
      const std::size_t firstCluster =
          recordId == startActivation && attachment.transitionCursor < count
              ? attachment.transitionCursor
              : 0;
      for (std::size_t offset = 0; offset < count; ++offset) {
        auto &cluster = attachment.clusters[(firstCluster + offset) % count];
        if (cluster.pendingSelection.mode == cluster.activeSelection.mode)
          continue;
        if (!remaining)
          continue;
        if (!scene_.SetHlodSelectionAtomic3D(cluster.entities,
                                            cluster.pendingSelection))
          std::terminate();
        cluster.activeSelection = cluster.pendingSelection;
        --remaining;
        ++next.appliedTransitions;
        next.lastAppliedActivation = recordId;
        const std::size_t selectedIndex = (firstCluster + offset) % count;
        attachment.transitionCursor = (selectedIndex + 1) % count;
        nextTransitionActivation_ =
            attachment.transitionCursor ? recordId : recordId + 1;
        if (nextTransitionActivation_ >= next_)
          nextTransitionActivation_ = 1;
      }
    }
  };
  applyRange(startActivation, next_);
  if (startActivation > 1)
    applyRange(1, startActivation);

  for (std::uint64_t recordId = 1; recordId < next_; ++recordId) {
    const auto found = records_.find(recordId);
    if (found == records_.end() || found->second.retiring ||
        !found->second.hlod)
      continue;
    for (auto &cluster : found->second.hlod->clusters) {
      if (cluster.pendingSelection.mode != cluster.activeSelection.mode) {
        cluster.pendingSelection = cluster.activeSelection;
        ++next.deferredTransitions;
      }
      switch (cluster.activeSelection.mode) {
      case HlodSelectionMode3D::Source: ++next.sourceClusters; break;
      case HlodSelectionMode3D::Transition: ++next.transitionClusters; break;
      case HlodSelectionMode3D::Proxy: ++next.proxyClusters; break;
      }
    }
  }
  lastHlodCameraPosition_ = view.camera.position;
  hasLastHlodCameraPosition_ = true;
  InvalidateFrozenHlodFrame3D();
  ++hlodSelectionGeneration_;
  hlodRuntimeStats_ = next;
  return true;
}

std::optional<HlodFrozenFrameToken3D>
TerrainVegetationWorldChunkActivationTarget3D::FreezeHlodFrame3D(
    std::uint64_t frameId) noexcept {
  if (!OnOwnerThread() || hlodTokenExhausted_ || !frameId ||
      frameId != hlodRuntimeStats_.frameId)
    return std::nullopt;
  for (const auto &[id, record] : records_) {
    (void)id;
    if (record.retiring || !record.hlod)
      continue;
    for (const auto &cluster : record.hlod->clusters)
      if (!scene_.MatchesHlodSelection3D(cluster.entities,
                                        cluster.activeSelection))
        return std::nullopt;
  }
  const auto rendererEpoch = engine_.GetRendererRuntimeEpoch3D();
  if (!rendererEpoch)
    return std::nullopt;
  frozenHlodToken_ = {frameId, hlodActivationIdentity_, hlodWorldEpoch_,
                      hlodSelectionGeneration_, rendererEpoch.deviceEpoch,
                      rendererEpoch.resizeEpoch};
  hlodFrameFrozen_ = true;
  hlodRuntimeStats_.frozen = true;
  return frozenHlodToken_;
}

std::optional<SceneECSCollectionStats3D>
TerrainVegetationWorldChunkActivationTarget3D::CollectFrozenHlodFrame3D(
    HlodFrozenFrameToken3D token, Scene3D &scene, DrawList &draw,
    std::uint32_t visibilityMask) const {
  if (!OnOwnerThread())
    return std::nullopt;
  const auto rendererEpoch = engine_.GetRendererRuntimeEpoch3D();
  if (hlodTokenExhausted_ || !hlodFrameFrozen_ || !token ||
      token != frozenHlodToken_ || token.frameId != hlodRuntimeStats_.frameId ||
      token.activationIdentity != hlodActivationIdentity_ ||
      token.worldEpoch != hlodWorldEpoch_ ||
      token.selectionGeneration != hlodSelectionGeneration_ || !rendererEpoch ||
      token.deviceEpoch != rendererEpoch.deviceEpoch ||
      token.resizeEpoch != rendererEpoch.resizeEpoch)
    return std::nullopt;
  for (const auto &[id, record] : records_) {
    (void)id;
    if (record.retiring || !record.hlod)
      continue;
    for (const auto &cluster : record.hlod->clusters)
      if (!scene_.MatchesHlodSelection3D(cluster.entities,
                                        cluster.activeSelection))
        return std::nullopt;
  }
  return scene_.Collect(scene, draw, visibilityMask);
}
HlodIntegrationStats3D
TerrainVegetationWorldChunkActivationTarget3D::HlodStats() const noexcept {
  HlodIntegrationStats3D result = hlodStats_;
  result.metadataSizeBytes = 0;
  result.metadataCapacityBytes = 0;
  std::size_t amount{};
  if (!Multiply(records_.size(), sizeof(Record), amount) ||
      !Add(result.metadataSizeBytes, amount) ||
      !Multiply(records_.bucket_count(), sizeof(void *), amount) ||
      !Add(result.metadataCapacityBytes, amount)) {
    result.metadataSizeBytes = SIZE_MAX;
    result.metadataCapacityBytes = SIZE_MAX;
    return result;
  }
  for (const auto &[id, record] : records_) {
    (void)id;
    if (!record.hlod)
      continue;
    if (!Add(result.metadataSizeBytes, sizeof(Record::HlodAttachment)) ||
        !Multiply(record.hlod->clusters.size(),
                  sizeof(Record::HlodClusterAttachment), amount) ||
        !Add(result.metadataSizeBytes, amount) ||
        !Add(result.metadataCapacityBytes, sizeof(Record::HlodAttachment)) ||
        !Multiply(record.hlod->clusters.capacity(),
                  sizeof(Record::HlodClusterAttachment), amount) ||
        !Add(result.metadataCapacityBytes, amount)) {
      result.metadataSizeBytes = SIZE_MAX;
      result.metadataCapacityBytes = SIZE_MAX;
      return result;
    }
    for (const auto &cluster : record.hlod->clusters) {
      if (!Multiply(cluster.sources.size(),
                    sizeof(StaticMeshRenderableLease3D), amount) ||
          !Add(result.metadataSizeBytes, amount) ||
          !Multiply(cluster.entities.entities.size(), sizeof(SceneEntity3D),
                    amount) ||
          !Add(result.metadataSizeBytes, amount) ||
          !Multiply(cluster.sources.capacity(),
                    sizeof(StaticMeshRenderableLease3D), amount) ||
          !Add(result.metadataCapacityBytes, amount) ||
          !Multiply(cluster.entities.entities.capacity(),
                    sizeof(SceneEntity3D), amount) ||
          !Add(result.metadataCapacityBytes, amount)) {
        result.metadataSizeBytes = SIZE_MAX;
        result.metadataCapacityBytes = SIZE_MAX;
        return result;
      }
    }
  }
  return result;
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
