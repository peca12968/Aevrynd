#include <Aevrynd/WorldPartitionTerrainVegetationAdapter3D.h>

#include <iostream>

using namespace Aevrynd;

namespace {
int Result{};
void Fail(const char *message) {
  if (!Result) {
    std::cerr << message << '\n';
    Result = 1;
  }
}

bool Validation(const RendererStats &stats) {
  wchar_t expected[8]{};
  const DWORD count = GetEnvironmentVariableW(
      L"AEVRYND_EXPECT_GPU_VALIDATION_MODE", expected,
      static_cast<DWORD>(std::size(expected)));
  if (count == 6 && lstrcmpW(expected, L"normal") == 0)
    return !stats.gpuValidationRequested && !stats.gpuValidationAvailable &&
           !stats.gpuValidationExecuted && !stats.debugLayerEnabled &&
           !stats.gpuValidationPassed && !stats.debugErrorCount &&
           !stats.debugWarningCount;
  if (count == 3 && lstrcmpW(expected, L"gbv") == 0)
    return stats.gpuValidationRequested && stats.gpuValidationAvailable &&
           stats.gpuValidationExecuted && stats.debugLayerEnabled &&
           stats.gpuValidationPassed && !stats.debugErrorCount &&
           !stats.debugWarningCount;
  return false;
}

MaterialAssetDefinition3D Material() {
  MaterialAssetDefinition3D value;
  value.textures = {
      "engine/assets/material-packs/ambientcg/ground037/1k/"
      "Ground037_1K-JPG_Color.jpg",
      "engine/assets/material-packs/ambientcg/ground037/1k/"
      "Ground037_1K-JPG_Roughness.jpg",
      64,
      "engine/assets/material-packs/ambientcg/ground037/1k/"
      "Ground037_1K-JPG_NormalDX.jpg"};
  value.material.useUv = true;
  return value;
}

StaticMeshAssetDefinition3D Asset(std::string name,
                                  std::filesystem::path path) {
  StaticMeshAssetDefinition3D value;
  value.name = std::move(name);
  value.meshPath = std::move(path);
  value.material = Material();
  value.generateLods = false;
  return value;
}

struct Fixture {
  std::shared_ptr<TerrainVegetationChunkPayload3D> payload{};
  std::string proxyName{};
};

Fixture MakeFixture(StaticMeshAssetLibrary3D &library,
                    TerrainVegetationChunkSettings3D settings) {
  const GlobalChunkCoordinate3D chunk{0, 0, 0};
  const HlodClusterId3D clusterId{91, 92};
  auto proxy = library.PrepareHlodProxyCpu3D(
      settings.worldId, chunk, clusterId, 3,
      Asset({}, "engine/assets/model-packs/poly-haven/rock_09/rock_09.aemesh"));
  if (!proxy)
    return {};
  const std::array<HlodSourceBuildInput3D, 2> sources{{
      {{{101, 102}}, 201, 1, 1, 1, 1, {}, {1, 1, 1}, {-2, 0, 0}, {},
       {1, 1, 1}},
      {{{103, 104}}, 202, 1, 1, 1, 1, {}, {1, 1, 1}, {2, 0, 0}, {},
       {1, 1, 1}}}};
  HlodClusterBuildInput3D cluster{};
  cluster.asset = {clusterId, 3, {}, 5, 1, .8f, 1.2f, 2,
                   HlodCompatibility3D::OpaqueCompatible};
  cluster.resource = {1, 1, proxy->reservedPhysicalBytes, 1, 1, 100, 20};
  cluster.instance = {clusterId, chunk, {}, {1, 1, 1}, {}};
  cluster.sources = sources;
  const std::array clusters{cluster};
  auto data = BuildHlodData3D(clusters);
  if (!data)
    return {};
  TerrainVegetationHlodCluster3D manifestCluster{};
  manifestCluster.id = clusterId;
  manifestCluster.generation = 3;
  manifestCluster.proxy = *proxy;
  manifestCluster.sources = {
      {sources[0].reference, sources[0].generation, "hlod-source",
       {chunk, {-2, 0, 0}}, {}, {1, 1, 1}},
      {sources[1].reference, sources[1].generation, "hlod-source",
       {chunk, {2, 0, 0}}, {}, {1, 1, 1}}};
  const std::array manifestClusters{manifestCluster};
  auto manifest = BuildTerrainVegetationHlodManifest3D(manifestClusters, *data);
  if (!manifest)
    return {};
  auto payload = std::make_shared<TerrainVegetationChunkPayload3D>();
  payload->globalChunk = chunk;
  HeightField3D field({{}, 5, 5, 2});
  field.Generate(7, 1, .05f, 2);
  payload->heightField = field;
  payload->terrain = field.CreateMesh();
  payload->hlod = std::move(data);
  payload->hlodManifest = std::move(manifest);
  payload->cpuBytes = payload->terrain.vertices.size() * sizeof(Vertex3D) +
                      payload->terrain.indices.size() * sizeof(std::uint32_t);
  return {std::move(payload), proxy->logicalName};
}

class Game final : public IGame {
public:
  void OnStart(Engine &engine) override {
    baseline_ = engine.GetRendererStats();
    TerrainVegetationChunkSettings3D settings{};
    settings.chunkSize = 16;
    settings.terrainCells = 4;
    settings.vegetationPerChunk = 0;
    settings.vegetationLods = {Mesh3D::CreateUnitPyramid(),
                               Mesh3D::CreateUnitPyramid(),
                               Mesh3D::CreateUnitPyramid()};
    if (!library_.Initialize(engine, scene_, AEVRYND_SOURCE_DIR) ||
        !library_.Load(
            Asset("hlod-source", "games/engine_lab/assets/engine_probe.obj"))) {
      Fail("Real receipt fixture initialization failed");
      engine.Quit();
      return;
    }
    target_ = std::make_unique<TerrainVegetationWorldChunkActivationTarget3D>(
        engine, scene_, vegetation_, &library_, nullptr, settings);
    fixture_ = MakeFixture(library_, settings);
    if (!fixture_.payload) {
      Fail("Real receipt fixture creation failed");
      engine.Quit();
      return;
    }
    const WorldChunkPayload3D payload{fixture_.payload, fixture_.payload->cpuBytes,
                                      fixture_.payload->cpuBytes};
    const auto preparation = target_->PreparationBytes({{0, 0}, 1, 1, 1, 1}, payload);
    const auto reservation =
        target_->PhysicalAllocationBytes({{0, 0}, 1, 1, 1, 1}, payload);
    activation_ = target_->Activate({{0, 0}, 1, 1, 1, 1}, payload);
    if (preparation <= payload.cpuBytes || reservation == SIZE_MAX ||
        !activation_ || !activation_.gpuBytes ||
        activation_.gpuBytes > reservation ||
        !target_->BeginRetire(activation_)) {
      Fail("Exact preparation/reservation or real retirement capture failed");
      engine.Quit();
      return;
    }
  }

  void OnUpdate(Engine &, float) override {}

  void OnRender(Engine &engine, float) override {
    if (Result)
      return;
    if (activation_) {
      if (!target_->IsRetirementComplete(activation_)) {
        if (++polls_ > 600) {
          Fail("Real graphics queue retirement receipt timed out");
          engine.Quit();
        }
        return;
      }
      activation_ = {};
      target_.reset();
      if (library_.Contains(fixture_.proxyName) ||
          !library_.Unload("hlod-source") || !library_.Shutdown()) {
        Fail("Real receipt completion left HLOD aliases or source assets");
        engine.Quit();
        return;
      }
      // 收據完成後才允許 teardown wait；一般 retirement polling 全程不呼叫全域 wait。
      engine.WaitRendererIdle3D();
      engine.TrimUnusedMemory();
      cleanupComplete_ = true;
      return;
    }
    if (cleanupComplete_ && validationFrames_++ > 0) {
      const auto stats = engine.GetRendererStats();
      if (!Validation(stats) || stats.residentMeshBytes != baseline_.residentMeshBytes ||
          stats.residentTextureAllocationBytes !=
              baseline_.residentTextureAllocationBytes ||
          stats.pendingGpuReleaseBytes) {
        std::cerr << "real baseline mesh=" << stats.residentMeshBytes << '/'
                  << baseline_.residentMeshBytes << " texture="
                  << stats.residentTextureAllocationBytes << '/'
                  << baseline_.residentTextureAllocationBytes << " pending="
                  << stats.pendingGpuReleaseBytes << " validation="
                  << Validation(stats) << " polls=" << polls_ << '\n';
        Fail("Real receipt D3D12/GBV resource baseline failed");
      } else
        std::cout << "realRetirementReceipt=passed polls=" << polls_
                  << " globalWaitBeforeReceipt=0 validation=passed\n";
      engine.Quit();
    }
  }

  void OnStop(Engine &) override {
    target_.reset();
    if (library_.Initialized()) {
      library_.ClearInstances();
      library_.Shutdown();
    }
  }

private:
  SceneECSRegistry3D scene_{};
  ChunkedVegetationWorld3D vegetation_{};
  StaticMeshAssetLibrary3D library_{};
  std::unique_ptr<TerrainVegetationWorldChunkActivationTarget3D> target_{};
  Fixture fixture_{};
  WorldChunkActivation3D activation_{};
  RendererStats baseline_{};
  unsigned polls_{}, validationFrames_{};
  bool cleanupComplete_{};
};
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, wchar_t *, int) {
  try {
    EngineConfig config{};
    config.title = L"Aevrynd Real HLOD Retirement Receipt";
    config.width = 320;
    config.height = 180;
    config.renderQuality = RenderQuality3D::Low;
    config.enableAutomaticMemoryTrim = false;
    Engine engine(config);
    engine.Run(std::make_unique<Game>(), instance, SW_HIDE);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 20;
  }
  return Result;
}
