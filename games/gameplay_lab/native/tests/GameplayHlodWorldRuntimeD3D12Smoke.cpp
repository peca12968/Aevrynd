#include "GameplayHlodWorldRuntime3D.h"
#include "GameplayShowcaseEnvironment3D.h"

#include <Aevrynd/Engine.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

using namespace Aevrynd;
using namespace GameplayLab;

namespace {
int Result{};

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

class Game final : public IGame {
public:
  void OnStart(Engine &engine) override {
    baseline_ = engine.GetRendererStats();
    if (!environment_.Initialize(0x4a17u) ||
        environment_.Descriptors().size() != 464) {
      Fail("Gameplay production descriptor initialization failed", engine);
      return;
    }
    const auto manifest = std::filesystem::path(AEVRYND_SOURCE_DIR) /
        "engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json";
    if (!assets_.LoadProduction(engine, registry_, manifest,
                                environment_.Descriptors(), load_) ||
        !runtime_.Initialize(engine, registry_, assets_,
                             environment_.Descriptors(),
                             {0x2ec047454e560001ull, 0x484c4f44574f524cull})) {
      Fail("Gameplay production HLOD runtime initialization failed", engine);
      return;
    }
  }

  void OnUpdate(Engine &, float) override {}

  void OnRender(Engine &engine, float) override {
    if (Result)
      return;
    if (++attempts_ > 600) {
      Fail("Gameplay production HLOD runtime timed out", engine);
      return;
    }
    auto &draw = engine.Draw();
    draw.Clear();
    scene_.Clear();
    const Camera3D camera = stage_ == 0
        ? Camera3D{{0, 80, -300}, {0, 0, 0}, {0, 1, 0},
                   {ProjectionMode::Perspective, 1.0471976f, 0, .1f, 10000.f}}
        : stage_ == 1
            ? Camera3D{{0, 4000, -50000}, {0, 0, 0}, {0, 1, 0},
                       {ProjectionMode::Perspective, 1.0471976f, 0, .1f,
                        100000.f}}
            : Camera3D{{0, 400, -5000}, {0, 0, 0}, {0, 1, 0},
                       {ProjectionMode::Orthographic, 1.0471976f, 100000.f,
                        .1f, 10000.f}};
    if (!runtime_.CollectFrame(nextFrame_, camera, {640, 360}, scene_, draw))
      return;
    const auto runtimeReceipt = runtime_.Receipt();
    Scene3DSubmissionOptions options{};
    options.frustumCulling = false;
    const auto submission = scene_.Submit(draw, camera, {640, 360}, options);
    if (!submission.visibleMeshInstances || !runtimeReceipt.SeamComplete()) {
      Fail("Gameplay production HLOD frozen Scene submission failed", engine);
      return;
    }
    if (stage_ == 0) {
      near_ = runtimeReceipt;
      if (near_.build.inputDescriptors != 464 ||
          near_.build.eligibleRockDescriptors != 30 ||
          near_.build.sourceInstances != 30 || near_.build.clusters != 6 ||
          near_.integration.residentSourceEntities != 30 ||
          near_.integration.residentProxyEntities != 6 ||
          !near_.frame.sourceClusters || near_.frame.transitionClusters ||
          near_.frame.sourceClusters + near_.frame.proxyClusters != 6) {
        std::cerr << "near descriptors=" << near_.build.inputDescriptors
                  << " rocks=" << near_.build.eligibleRockDescriptors
                  << " sources=" << near_.build.sourceInstances
                  << " clusters=" << near_.build.clusters
                  << " residentSources=" << near_.integration.residentSourceEntities
                  << " residentProxies=" << near_.integration.residentProxyEntities
                  << " sourceMode=" << near_.frame.sourceClusters
                  << " transitionMode=" << near_.frame.transitionClusters
                  << " proxyMode=" << near_.frame.proxyClusters << '\n';
        Fail("Gameplay production HLOD near-source contract failed", engine);
        return;
      }
      ++stage_;
      ++nextFrame_;
      return;
    }
    if (stage_ == 1) {
      transition_ = runtimeReceipt;
      std::array<std::size_t, 8> sourceRowsBySeed{};
      std::array<std::size_t, 8> proxyRowsBySeed{};
      std::array<float, 8> sourceCoverageBySeed{};
      std::array<float, 8> proxyCoverageBySeed{};
      sourceCoverageBySeed.fill(-1.f);
      proxyCoverageBySeed.fill(-1.f);
      for (const auto &instance : draw.meshInstances) {
        if (instance.lodCoverage <= 0.f || instance.lodCoverage >= 1.f)
          continue;
        const auto seed = static_cast<std::size_t>(instance.lodSeed & 7u);
        if (instance.identity.id.high & HlodProxyIdentityDomainBit3D) {
          ++transitionProxyRows_;
          ++proxyRowsBySeed[seed];
          proxyCoverageBySeed[seed] = instance.lodCoverage;
          transitionProxyInvert_ =
              transitionProxyInvert_ && ((instance.lodSeed & 8u) != 0);
        } else {
          ++transitionSourceRows_;
          ++sourceRowsBySeed[seed];
          sourceCoverageBySeed[seed] = instance.lodCoverage;
          transitionSourceInvert_ =
              transitionSourceInvert_ || ((instance.lodSeed & 8u) != 0);
        }
      }
      std::size_t matchedTransitionClusters{};
      transitionSharedThreshold_ = true;
      for (std::size_t seed{}; seed < sourceRowsBySeed.size(); ++seed) {
        if (!sourceRowsBySeed[seed] && !proxyRowsBySeed[seed])
          continue;
        const bool clusterMatched = sourceRowsBySeed[seed] == 5 &&
            proxyRowsBySeed[seed] == 1 && sourceCoverageBySeed[seed] > 0.f &&
            proxyCoverageBySeed[seed] > 0.f &&
            std::abs(sourceCoverageBySeed[seed] - proxyCoverageBySeed[seed]) <=
                .00001f;
        transitionSharedThreshold_ =
            transitionSharedThreshold_ && clusterMatched;
        matchedTransitionClusters += clusterMatched ? 1 : 0;
      }
      transitionCoverageComplement_ = transitionSharedThreshold_ &&
          transitionProxyInvert_ && !transitionSourceInvert_ &&
          matchedTransitionClusters == transition_.frame.transitionClusters;
      if (!transition_.frame.transitionClusters ||
          transition_.frame.sourceClusters + transition_.frame.transitionClusters +
                  transition_.frame.proxyClusters !=
              6 ||
          transitionProxyRows_ != transition_.frame.transitionClusters ||
          transitionSourceRows_ != transition_.frame.transitionClusters * 5 ||
          !transitionProxyInvert_ || transitionSourceInvert_ ||
          !transitionCoverageComplement_ ||
          transition_.collection.submittedRenderables >=
              near_.collection.submittedRenderables) {
        Fail("Gameplay production HLOD transition coverage contract failed",
             engine);
        return;
      }
      ++stage_;
      ++nextFrame_;
      return;
    }
    far_ = runtimeReceipt;
    if (far_.frame.proxyClusters != 6 || far_.frame.sourceClusters ||
        far_.frame.transitionClusters ||
        far_.frame.appliedTransitions != transition_.frame.transitionClusters ||
        far_.collection.submittedRenderables >=
            transition_.collection.submittedRenderables) {
      std::cerr << "far sourceMode=" << far_.frame.sourceClusters
                << " transitionMode=" << far_.frame.transitionClusters
                << " proxyMode=" << far_.frame.proxyClusters
                << " applied=" << far_.frame.appliedTransitions
                << " priorTransition=" << transition_.frame.transitionClusters
                << " nearSubmitted=" << near_.collection.submittedRenderables
                << " farSubmitted=" << far_.collection.submittedRenderables << '\n';
      Fail("Gameplay production HLOD far-proxy contract failed", engine);
      return;
    }
    engine.WaitRendererIdle3D();
    const auto validation = engine.GetRendererStats();
    if (!Validation(validation)) {
      Fail("Gameplay production HLOD GPU validation contract failed", engine);
      return;
    }
    WriteReceipt(validation);
    complete_ = true;
    engine.Quit();
  }

  void OnStop(Engine &engine) override {
    engine.Draw().Clear();
    scene_.Clear();
    runtime_.Shutdown();
    const auto release = assets_.ReleaseProduction(engine, scene_);
    engine.WaitRendererIdle3D();
    const auto after = engine.GetRendererStats();
    if (complete_ && (!release.rendererBaselineRestored ||
                      after.residentMeshes != baseline_.residentMeshes ||
                      after.residentMaterialTextureSets !=
                          baseline_.residentMaterialTextureSets ||
                      after.pendingGpuReleaseBytes))
      Result = 2;
  }

private:
  void Fail(const char *message, Engine &engine) {
    if (!Result) {
      std::cerr << message << '\n';
      Result = 1;
    }
    engine.Quit();
  }

  void WriteReceipt(const RendererStats &stats) const {
    std::ofstream out(AEVRYND_GAMEPLAY_HLOD_RUNTIME_RECEIPT,
                      std::ios::binary | std::ios::trunc);
    if (!out) {
      Result = 3;
      return;
    }
    out << "{\"source\":\"GameplayHlodWorldRuntime3D\","
           "\"status\":\"SEAM-GO\",\"syntheticSubstitution\":0,"
           "\"performanceMeasured\":0,\"inputDescriptors\":"
        << near_.build.inputDescriptors
        << ",\"eligibleRockDescriptors\":" << near_.build.eligibleRockDescriptors
        << ",\"excludedTerrain\":" << near_.build.excludedTerrain
        << ",\"excludedWater\":" << near_.build.excludedWater
        << ",\"excludedTrees\":" << near_.build.excludedTrees
        << ",\"excludedGrass\":" << near_.build.excludedGrass
        << ",\"excludedBuildings\":" << near_.build.excludedBuildings
        << ",\"clusters\":" << near_.build.clusters
        << ",\"sourceInstances\":" << near_.build.sourceInstances
        << ",\"hlodDataHash\":" << near_.build.hlodDataHash
        << ",\"hlodManifestHash\":" << near_.build.hlodManifestHash
        << ",\"worldPartitionResident\":1,\"sceneEcsActivated\":1,"
           "\"selectFreezeCollect\":1,\"nearSourceClusters\":"
        << near_.frame.sourceClusters << ",\"farProxyClusters\":"
        << far_.frame.proxyClusters << ",\"nearSubmittedRenderables\":"
        << near_.collection.submittedRenderables
        << ",\"transitionClusters\":" << transition_.frame.transitionClusters
        << ",\"transitionSourceRows\":" << transitionSourceRows_
        << ",\"transitionProxyRows\":" << transitionProxyRows_
        << ",\"transitionSharedThreshold\":"
        << (transitionSharedThreshold_ ? 1 : 0)
        << ",\"transitionCoverageComplement\":"
        << (transitionCoverageComplement_ ? 1 : 0)
        << ",\"farSubmittedRenderables\":"
        << far_.collection.submittedRenderables
        << ",\"representativeLowLodProxy\":1,\"aggregateProxyBuilt\":0,"
           "\"gameplayLabOnRenderConnected\":0,"
           "\"gameplayIdentityDomainMigrated\":0,"
           "\"allDescriptorKindsCovered\":0,\"gpuValidationRequested\":"
        << (stats.gpuValidationRequested ? 1 : 0)
        << ",\"gpuValidationAvailable\":"
        << (stats.gpuValidationAvailable ? 1 : 0)
        << ",\"gpuValidationExecuted\":"
        << (stats.gpuValidationExecuted ? 1 : 0)
        << ",\"gpuValidationPassed\":"
        << (stats.gpuValidationPassed ? 1 : 0)
        << ",\"debugErrors\":" << stats.debugErrorCount
        << ",\"debugWarnings\":" << stats.debugWarningCount
        << ",\"remaining\":\"Connect GameplayLabGame OnRender, migrate EnvironmentPartitionId away from the reserved proxy bit, build offline aggregate proxies, and cover masked trees, grass, terrain, water, and buildings.\"}";
  }

  SceneECSRegistry3D registry_{};
  Scene3D scene_{};
  GameplayShowcaseEnvironment3D environment_{};
  ShowcaseRenderAssetBindings3D assets_{};
  ShowcaseProductionRenderReceipt3D load_{};
  GameplayHlodWorldRuntime3D runtime_{};
  GameplayHlodWorldRuntimeReceipt3D near_{}, transition_{}, far_{};
  RendererStats baseline_{};
  std::uint64_t nextFrame_{1};
  unsigned attempts_{}, stage_{};
  std::size_t transitionSourceRows_{}, transitionProxyRows_{};
  bool transitionProxyInvert_{true}, transitionSourceInvert_{},
      transitionSharedThreshold_{}, transitionCoverageComplement_{};
  bool complete_{};
};
} // 命名空間

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, wchar_t *, int) {
  try {
    EngineConfig config{};
    config.title = L"Aevrynd Gameplay Production HLOD Runtime";
    config.width = 640;
    config.height = 360;
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
