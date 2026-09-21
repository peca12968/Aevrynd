#include "GameplayHlodWorldRuntime3D.h"
#include "GameplayShowcaseEnvironment3D.h"

#include <Aevrynd/Engine.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <tuple>

using namespace Aevrynd;
using namespace GameplayLab;

namespace {
int Result{};
constexpr StableObjectId3D HostPaletteIdentity3D{
    0x50414c4554544533ull, 1};

constexpr auto FrameNegativeDecision = EvaluateGameplayHlodWorldContract3D(
    {1, 1, true, true});
constexpr auto IdentityNegativeDecision = EvaluateGameplayHlodWorldContract3D(
    {1, 0, false, true});
constexpr auto DataNegativeDecision = EvaluateGameplayHlodWorldContract3D(
    {1, 0, true, false});
constexpr auto RetryLimitDecision = EvaluateGameplayHlodWorldRetry3D(8, 8);
static_assert(FrameNegativeDecision.readiness ==
                  GameplayHlodWorldReadiness3D::Fatal &&
              FrameNegativeDecision.reason ==
                  GameplayHlodWorldTerminalReason3D::FrameInvariantViolation);
static_assert(IdentityNegativeDecision.readiness ==
                  GameplayHlodWorldReadiness3D::Fatal &&
              IdentityNegativeDecision.reason ==
                  GameplayHlodWorldTerminalReason3D::IdentityInvariantViolation);
static_assert(DataNegativeDecision.readiness ==
                  GameplayHlodWorldReadiness3D::Fatal &&
              DataNegativeDecision.reason ==
                  GameplayHlodWorldTerminalReason3D::DataInvariantViolation);
static_assert(RetryLimitDecision.readiness ==
                  GameplayHlodWorldReadiness3D::Fatal &&
              RetryLimitDecision.reason ==
                  GameplayHlodWorldTerminalReason3D::RetryLimitExceeded);

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

void MixBytes(std::uint64_t &hash, const void *data, std::size_t size) {
  const auto *bytes = static_cast<const std::uint8_t *>(data);
  for (std::size_t index{}; index < size; ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ull;
  }
}

template <class Value>
void MixValue(std::uint64_t &hash, const Value &value) {
  static_assert(std::is_trivially_copyable_v<Value>);
  MixBytes(hash, &value, sizeof(value));
}

template <class Value>
void MixValues(std::uint64_t &hash, std::span<const Value> values) {
  if (!values.empty())
    MixBytes(hash, values.data(), values.size_bytes());
}

std::uint64_t DrawFingerprint(const DrawList &draw) {
  std::uint64_t hash{14695981039346656037ull};
  const auto mixMatrix = [&](const Matrix4 &matrix) {
    for (const auto &row : matrix.values)
      for (const float value : row)
        MixValue(hash, value);
  };
  const auto mixIdentity = [&](const RenderObjectIdentity3D &identity) {
    MixValue(hash, identity.id.high);
    MixValue(hash, identity.id.low);
    MixValue(hash, identity.generation);
    MixValue(hash, identity.hlodClusterId.high);
    MixValue(hash, identity.hlodClusterId.low);
    MixValue(hash, identity.sharedLodDither);
    MixValue(hash, identity.hlodProxy);
  };
  const auto vertices2D = draw.vertices.size();
  const auto skinned = draw.skinnedMeshInstances.size();
  const auto joints = draw.skinPaletteJoints.size();
  MixValue(hash, vertices2D);
  MixValue(hash, skinned);
  MixValue(hash, joints);
  for (const auto &vertex : draw.worldVertices) {
    MixValue(hash, vertex.x);
    MixValue(hash, vertex.y);
    MixValue(hash, vertex.z);
    MixValue(hash, vertex.normal);
    MixValue(hash, vertex.color);
    MixValue(hash, vertex.u);
    MixValue(hash, vertex.v);
  }
  MixValues(hash, std::span{draw.worldIndices});
  for (const auto &instance : draw.meshInstances) {
    MixValue(hash, instance.mesh.value);
    MixValue(hash, instance.mesh.contentPrimary);
    MixValue(hash, instance.mesh.contentSecondary);
    MixValue(hash, instance.mesh.vertexCount);
    MixValue(hash, instance.mesh.indexCount);
    mixMatrix(instance.transform);
    mixMatrix(instance.normalTransform);
    MixValue(hash, instance.tint);
    MixValue(hash, instance.surface);
    MixValue(hash, instance.layer);
    MixValue(hash, instance.textures.value);
    MixValue(hash, instance.textures.semanticIdentity);
    MixValue(hash, instance.textureScale);
    MixValue(hash, instance.normalScale);
    MixValue(hash, instance.useUv);
    MixValue(hash, instance.materialClass);
    MixValue(hash, instance.cameraDepth);
    MixValue(hash, instance.castsShadow);
    MixValue(hash, instance.lodCoverage);
    MixValue(hash, instance.lodSeed);
    MixValue(hash, instance.shadowCascadeMask);
    MixValue(hash, instance.shadowCascadeSelection);
    mixMatrix(instance.previousTransform);
    MixValue(hash, instance.motionValid);
    MixValue(hash, instance.alphaMode);
    MixValue(hash, instance.alphaCutoff);
    MixValue(hash, instance.doubleSided);
    mixIdentity(instance.identity);
    MixValue(hash, instance.occlusionOpacity);
  }
  for (const auto &light : draw.PointLights()) {
    MixValue(hash, light.position.x);
    MixValue(hash, light.position.y);
    MixValue(hash, light.position.z);
    MixValue(hash, light.radius);
    MixValue(hash, light.color.r);
    MixValue(hash, light.color.g);
    MixValue(hash, light.color.b);
    MixValue(hash, light.color.a);
    MixValue(hash, light.intensity);
  }
  mixMatrix(draw.ViewProjection());
  const auto &directional = draw.DirectionalLight();
  MixValue(hash, directional.direction.x);
  MixValue(hash, directional.direction.y);
  MixValue(hash, directional.direction.z);
  MixValue(hash, directional.ambient);
  MixValue(hash, directional.color.r);
  MixValue(hash, directional.color.g);
  MixValue(hash, directional.color.b);
  MixValue(hash, directional.color.a);
  MixValue(hash, directional.intensity);
  const auto leaseCount = draw.AssetLeaseCount();
  const auto originGeneration = draw.OriginGeneration3D();
  MixValue(hash, leaseCount);
  MixValue(hash, originGeneration);
  return hash;
}

std::uint64_t SceneFingerprint(const Scene3D &scene) {
  Scene3D copy = scene;
  DrawList draw;
  const Camera3D camera{{0, 3, -12}, {0, 0, 0}, {0, 1, 0},
                        {ProjectionMode::Perspective, 1.0471976f, 0, .1f,
                         1000.f}};
  const auto stats = copy.Submit(draw, camera, {640, 360},
                                 {.frustumCulling = false});
  std::uint64_t hash = DrawFingerprint(draw);
  MixValue(hash, stats.inputTriangles);
  MixValue(hash, stats.inputLines);
  MixValue(hash, stats.meshInstances);
  MixValue(hash, stats.visibleMeshInstances);
  MixValue(hash, stats.drawListRevision);
  MixValue(hash, stats.acceptedIdentityOrderFingerprint);
  const auto triangles = scene.TriangleCount();
  const auto lines = scene.LineCount();
  const auto meshes = scene.MeshCount();
  MixValue(hash, triangles);
  MixValue(hash, lines);
  MixValue(hash, meshes);
  return hash;
}

struct HostOutputFingerprint {
  std::uint64_t scene{}, draw{};
  [[nodiscard]] bool operator==(const HostOutputFingerprint &) const = default;
};

HostOutputFingerprint HostFingerprint(const Scene3D &scene,
                                      const DrawList &draw) {
  return {SceneFingerprint(scene), DrawFingerprint(draw)};
}

struct RuntimeNegativeEvidence {
  GameplayHlodWorldRuntimeReceipt3D receipt{};
  bool failClosed{};
};

class Game final : public IGame {
public:
  void OnStart(Engine &engine) override {
    baseline_ = engine.GetRendererStats();
    if (!environment_.Initialize(0x4a17u) ||
        environment_.Descriptors().size() != 472 ||
        !GameplayEnvironmentIdentityContract3D(
            GameplayEnvironmentObjectIdentity3D(1)) ||
        (GameplayHlodWorldIdentity3D.high & HlodProxyIdentityDomainBit3D)) {
      Fail("Gameplay production descriptor initialization failed", engine);
      return;
    }
    const auto manifest = std::filesystem::path(AEVRYND_SOURCE_DIR) /
        "engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json";
    if (!assets_.LoadProduction(engine, registry_, manifest,
                                environment_.Descriptors(), load_)) {
      Fail("Gameplay production HLOD runtime initialization failed", engine);
      return;
    }
    const auto close = [](float left, float right) {
      return std::abs(left - right) <= .0001f;
    };
    const auto *islandTrunk =
        assets_.ProductionMaterialById("island_tree_01:trunk");
    const auto *smallBranches =
        assets_.ProductionMaterialById("tree_small_02:branches");
    const auto *islandLeaves =
        assets_.ProductionMaterialById("island_tree_01:leaves");
    const auto *deadTree =
        assets_.ProductionMaterialById("dead_tree_trunk_02:slot-0");
    const auto *boulder =
        assets_.ProductionMaterialById("boulder_01:slot-0");
    if (load_.barkMaterialResponses != 4 ||
        !load_.barkMaterialResponseFingerprint ||
        load_.weatheredPropMaterialResponses != 2 ||
        !load_.weatheredPropMaterialResponseFingerprint || !islandTrunk ||
        !smallBranches || !islandLeaves || !deadTree || !boulder ||
        !close(islandTrunk->tint.r, 1.f) ||
        !close(islandTrunk->tint.g, .995f) ||
        !close(islandTrunk->tint.b, .98f) ||
        !close(islandTrunk->surface.roughness, .68f) ||
        !close(islandTrunk->normalScale, 1.12f) ||
        !close(smallBranches->tint.g, .997f) ||
        !close(smallBranches->tint.b, .985f) ||
        !close(smallBranches->surface.roughness, .74f) ||
        !close(smallBranches->normalScale, 1.06f) ||
        islandLeaves->materialClass != MaterialClass3D::Foliage ||
        islandLeaves->alphaMode != MaterialAlphaMode3D::Mask ||
        !close(islandLeaves->tint.r, .96f) ||
        !close(islandLeaves->tint.g, 1.f) ||
        !close(islandLeaves->tint.b, .93f) ||
        !close(islandLeaves->surface.roughness, .76f) ||
        !close(islandLeaves->normalScale, 1.08f) ||
        !close(islandLeaves->alphaCutoff, .42f) ||
        !close(deadTree->tint.r, .58f) || !close(deadTree->tint.g, .47f) ||
        !close(deadTree->tint.b, .34f) ||
        !close(deadTree->surface.roughness, .82f) ||
        !close(deadTree->normalScale, 1.15f) ||
        !close(boulder->tint.r, .62f) || !close(boulder->tint.g, .66f) ||
        !close(boulder->tint.b, .58f) ||
        !close(boulder->surface.roughness, .86f) ||
        !close(boulder->normalScale, 1.20f)) {
      Fail("Gameplay production bark/weathered-prop response contract failed",
           engine);
      return;
    }
    if (!runtime_.Initialize(engine, assets_, environment_.Descriptors(),
                             GameplayHlodWorldIdentity3D)) {
      Fail("Gameplay production HLOD runtime initialization failed", engine);
      return;
    }
    paletteMesh_ = engine.CreateMeshResource3D(Mesh3D::CreateUnitBox());
    SceneMeshRenderable3D paletteRenderable{};
    paletteRenderable.nearMesh = paletteMesh_;
    paletteRenderable.nearMaterial.tint = {.2f, .8f, .9f, 1.f};
    paletteRenderable.identity = {HostPaletteIdentity3D, 1};
    const SceneEntityDefinition3D paletteDefinition{
        SceneTransformComponent3D{{{0, 2, -8}, {}, {1, 1, 1}}},
        paletteRenderable, {}};
    std::vector<SceneEntity3D> created;
    if (!paletteMesh_ ||
        !registry_.CreateBatchAtomic(std::span(&paletteDefinition, 1), created) ||
        created.size() != 1) {
      Fail("Host palette SceneECS fixture initialization failed", engine);
      return;
    }
    paletteEntity_ = created.front();
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
    const auto hostCollection = registry_.Collect(scene_, draw);
    if (hostCollection.aliveEntities != 1 ||
        hostCollection.submittedRenderables != 1) {
      Fail("Host palette SceneECS ownership collection failed", engine);
      return;
    }
    // The source-only phase must remain a genuinely near view even when the
    // production rock layout changes a cluster's aggregate bounds.  Keeping
    // this camera inside the authored world tests Source selection without
    // weakening the production screen-error or transition thresholds.
    const Camera3D camera = stage_ == 0
        ? Camera3D{{0, 100, -20}, {0, 0, 0}, {0, 1, 0},
                   {ProjectionMode::Perspective, 1.0471976f, 0, .1f, 10000.f}}
        : stage_ == 1
            ? Camera3D{{0, 4000, -50000}, {0, 0, 0}, {0, 1, 0},
                       {ProjectionMode::Perspective, 1.0471976f, 0, .1f,
                        100000.f}}
            : Camera3D{{0, 400, -5000}, {0, 0, 0}, {0, 1, 0},
                       {ProjectionMode::Orthographic, 1.0471976f, 100000.f,
                        .1f, 10000.f}};
    const auto readiness =
        runtime_.CollectFrame(nextFrame_, camera, {640, 360}, scene_, draw);
    if (readiness == GameplayHlodWorldReadiness3D::RetryableNotReady) {
      if (stage_ != 0) {
        Fail("Gameplay HLOD became retryable after reaching Ready", engine);
      }
      return;
    }
    if (readiness == GameplayHlodWorldReadiness3D::Fatal) {
      Fail("Gameplay HLOD runtime returned Fatal", engine);
      return;
    }
    const auto runtimeReceipt = runtime_.Receipt();
    const auto hostCollectionAfterHlod = registry_.Stats();
    if (!runtimeReceipt.ownership.independentRegistry ||
        runtimeReceipt.ownership.owner !=
            GameplaySceneEcsSubmissionOwner3D::HlodWorldRuntime ||
        hostCollectionAfterHlod.collectionPasses !=
            hostCollection.collectionPasses ||
        hostCollectionAfterHlod.aliveEntities != hostCollection.aliveEntities ||
        hostCollectionAfterHlod.submittedRenderables !=
            hostCollection.submittedRenderables ||
        runtimeReceipt.collection.aliveEntities <
            runtimeReceipt.integration.residentSourceEntities +
                runtimeReceipt.integration.residentProxyEntities ||
        scene_.MeshCount() <= hostCollection.submittedRenderables) {
      std::cerr << "ownership independent="
                << runtimeReceipt.ownership.independentRegistry
                << " owner=" << unsigned(runtimeReceipt.ownership.owner)
                << " hostAlive=" << hostCollection.aliveEntities
                << " hostSubmitted=" << hostCollection.submittedRenderables
                << " hlodAlive=" << runtimeReceipt.collection.aliveEntities
                << " sources="
                << runtimeReceipt.integration.residentSourceEntities
                << " proxies="
                << runtimeReceipt.integration.residentProxyEntities
                << " hlodSubmitted="
                << runtimeReceipt.collection.submittedRenderables
                << " sceneMeshes=" << scene_.MeshCount()
                << " hostPassBefore=" << hostCollection.collectionPasses
                << " hostPassAfter="
                << hostCollectionAfterHlod.collectionPasses << '\n';
      Fail("HLOD and host SceneECS ownership was not isolated", engine);
      return;
    }
    Scene3DSubmissionOptions options{};
    options.frustumCulling = false;
    const auto submission = scene_.Submit(draw, camera, {640, 360}, options);
    if (!submission.visibleMeshInstances || !runtimeReceipt.SeamComplete()) {
      Fail("Gameplay production HLOD frozen Scene submission failed", engine);
      return;
    }
    std::set<std::tuple<std::uint64_t, std::uint64_t, std::uint64_t>>
        submittedIdentities;
    std::size_t hostPaletteRows{};
    for (const auto &instance : draw.meshInstances) {
      const auto identity = instance.identity;
      if (!identity)
        continue;
      const auto key =
          std::tuple{identity.id.high, identity.id.low, identity.generation};
      if (!submittedIdentities.insert(key).second) {
        Fail("SceneECS ownership emitted a duplicate render identity", engine);
        return;
      }
      hostPaletteRows += identity.id == HostPaletteIdentity3D ? 1u : 0u;
    }
    if (hostPaletteRows != 1) {
      Fail("Host palette submission did not retain unique ownership", engine);
      return;
    }
    if (stage_ == 0) {
      near_ = runtime_.Receipt();
      if (near_.ownership.rejectedDuplicateFrames != 0 ||
          near_.ownership.acceptedSubmissions != 1 ||
          near_.ownership.submittedFrame != nextFrame_) {
        Fail("Initial HLOD frame ownership receipt failed", engine);
        return;
      }
      const bool identityDomainSeparated = std::ranges::all_of(
          draw.meshInstances, [](const auto &instance) {
            if (instance.identity.id == HostPaletteIdentity3D)
              return true;
            return instance.identity.id.high & HlodProxyIdentityDomainBit3D
                ? instance.identity.id.high != GameplayEnvironmentIdentityDomain3D
                : GameplayEnvironmentIdentityContract3D(instance.identity.id);
          });
      if (near_.build.inputDescriptors != 472 ||
          near_.build.eligibleRockDescriptors != 30 ||
          near_.build.sourceInstances != 30 || near_.build.clusters != 6 ||
          !near_.aggregateProxyBuilt || !near_.allDescriptorKindsCovered ||
          near_.build.representativeLowLodProxy ||
          near_.integration.residentSourceEntities != 30 ||
          near_.integration.residentProxyEntities != 6 ||
          !near_.frame.sourceClusters || near_.frame.transitionClusters ||
          near_.frame.sourceClusters + near_.frame.proxyClusters != 6 ||
          !identityDomainSeparated) {
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
        if (instance.identity.id == HostPaletteIdentity3D)
          continue;
        if (instance.lodCoverage <= 0.f || instance.lodCoverage >= 1.f)
          continue;
        const auto seed = static_cast<std::size_t>(instance.lodSeed & 7u);
        if (instance.identity.id.high & HlodProxyIdentityDomainBit3D) {
          transitionIdentitySeparated_ = transitionIdentitySeparated_ &&
              instance.identity.id.high != GameplayEnvironmentIdentityDomain3D;
          ++transitionProxyRows_;
          ++proxyRowsBySeed[seed];
          proxyCoverageBySeed[seed] = instance.lodCoverage;
          transitionProxyInvert_ =
              transitionProxyInvert_ && ((instance.lodSeed & 8u) != 0);
        } else {
          transitionIdentitySeparated_ = transitionIdentitySeparated_ &&
              GameplayEnvironmentIdentityContract3D(instance.identity.id);
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
          !transitionCoverageComplement_ || !transitionIdentitySeparated_ ||
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
    const auto residentTerrainRows = std::ranges::count_if(
        draw.meshInstances, [](const auto &instance) {
          return instance.materialClass == MaterialClass3D::Terrain &&
              GameplayEnvironmentIdentityContract3D(instance.identity.id);
        });
    const bool proxyIdentityDomain = std::ranges::all_of(
        draw.meshInstances, [](const auto &instance) {
          if (instance.identity.id == HostPaletteIdentity3D)
            return true;
          // Terrain is partition-owned and remains resident at every HLOD
          // distance.  Only the clustered authored sources transition to the
          // proxy identity domain.
          if (instance.materialClass == MaterialClass3D::Terrain)
            return GameplayEnvironmentIdentityContract3D(instance.identity.id);
          return instance.identity.id &&
              (instance.identity.id.high & HlodProxyIdentityDomainBit3D) &&
              instance.identity.id.high != GameplayEnvironmentIdentityDomain3D;
        });
    if (far_.frame.proxyClusters != 6 || far_.frame.sourceClusters ||
        far_.frame.transitionClusters ||
        far_.frame.appliedTransitions != transition_.frame.transitionClusters ||
        far_.collection.submittedRenderables >=
            transition_.collection.submittedRenderables ||
        residentTerrainRows != 1 ||
        !proxyIdentityDomain) {
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
      std::cerr << "gpuValidation requested="
                << validation.gpuValidationRequested
                << " available=" << validation.gpuValidationAvailable
                << " executed=" << validation.gpuValidationExecuted
                << " debugLayer=" << validation.debugLayerEnabled
                << " passed=" << validation.gpuValidationPassed
                << " errors=" << validation.debugErrorCount
                << " warnings=" << validation.debugWarningCount << '\n';
      Fail("Gameplay production HLOD GPU validation contract failed", engine);
      return;
    }
    validation_ = validation;
    if (!RunHostileReview(engine, camera)) {
      Fail("Gameplay HLOD hostile runtime review failed", engine);
      return;
    }
    Scene3D duplicateScene;
    DrawList duplicateDraw;
    const auto duplicate = runtime_.CollectFrame(
        nextFrame_, camera, {640, 360}, duplicateScene, duplicateDraw);
    frameNegative_ = runtime_.Receipt();
    frameNegativeFailClosed_ =
        duplicate == GameplayHlodWorldReadiness3D::Fatal &&
        frameNegative_.readiness == GameplayHlodWorldReadiness3D::Fatal &&
        frameNegative_.terminalReason ==
            GameplayHlodWorldTerminalReason3D::FrameInvariantViolation &&
        frameNegative_.ownership.rejectedDuplicateFrames == 1 &&
        !duplicateScene.MeshCount() && duplicateDraw.meshInstances.empty();
    if (!frameNegativeFailClosed_) {
      Fail("Duplicate HLOD frame was not Fatal and fail-closed", engine);
      return;
    }
    complete_ = true;
    engine.Quit();
  }

  void OnStop(Engine &engine) override {
    engine.Draw().Clear();
    scene_.Clear();
    runtime_.Shutdown();
    if (paletteEntity_)
      registry_.Destroy(paletteEntity_);
    paletteEntity_ = {};
    if (paletteMesh_)
      engine.DestroyMesh3D(paletteMesh_.handle);
    paletteMesh_ = {};
    const auto release = assets_.ReleaseProduction(engine, scene_);
    engine.WaitRendererIdle3D();
    const auto after = engine.GetRendererStats();
    if (complete_) {
      cleanupComplete_ = release.rendererBaselineRestored &&
          after.residentMeshes == baseline_.residentMeshes &&
          after.residentMaterialTextureSets ==
              baseline_.residentMaterialTextureSets &&
          !after.pendingGpuReleaseBytes;
      WriteReceipt(validation_, release, after, cleanupComplete_);
      if (!cleanupComplete_)
        Result = 2;
    }
  }

private:
  bool SeedHostOutputs(Scene3D &scene, DrawList &draw) const {
    const auto lease =
        std::make_shared<AssetLeaseState3D>(AssetLeaseState3D{0x7a11ce55ull});
    const RenderObjectIdentity3D identity{{0x484f53544f555450ull,
                                           0x555453454e54494eull},
                                          7};
    if (!scene.Mesh(paletteMesh_, {{3, 2, 1}, {}, {1, 1, 1}}, {}, lease,
                    identity) ||
        !draw.MeshInstance(paletteMesh_.handle,
                           CreateTranslationMatrix({-3, 1, 2}),
                           {.31f, .47f, .83f, 1.f}, {},
                           MeshRenderLayer3D::Opaque, {}, .25f, false,
                           MaterialClass3D::Surface,
                           std::numeric_limits<float>::quiet_NaN(), true, 1.f,
                           3, false, DirectionalShadowAllCascades3D,
                           ShadowCascadeSelection3D::Automatic,
                           Matrix4::Identity(), false, 1.f,
                           MaterialAlphaMode3D::Opaque, .5f, false, identity))
      return false;
    scene.Triangle({-1, 0, 0}, {0, 1, 0}, {1, 0, 0}, {.2f, .3f, .4f, 1.f});
    scene.Line({-2, 0, 0}, {2, 0, 0}, .25f, {.9f, .1f, .2f, 1.f});
    draw.WorldTriangle({-1, -1, 0}, {0, 1, 0}, {1, -1, 0},
                       {.7f, .4f, .2f, 1.f});
    draw.SetDirectionalLight({{.2f, -1.f, .4f}, .37f,
                              {.8f, .9f, 1.f, 1.f}, 1.25f});
    draw.AddPointLight({{3, 4, 5}, 9.f, {.3f, .8f, .6f, 1.f}, 2.f});
    draw.SetOriginGeneration3D(19);
    draw.RetainAssetLease(lease);
    return true;
  }

  GameplayHlodWorldReadiness3D TimedCollect(
      GameplayHlodWorldRuntime3D &runtime, std::uint64_t frame,
      const Camera3D &camera, Scene3D &scene, DrawList &draw) {
    const auto begin = std::chrono::steady_clock::now();
    const auto result = runtime.CollectFrame(frame, camera, {640, 360}, scene,
                                             draw);
    const auto elapsed = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - begin).count());
    hostileMaxCollectWallNanoseconds_ =
        (std::max)(hostileMaxCollectWallNanoseconds_, elapsed);
    if (elapsed > HostileCollectWallLimitNanoseconds)
      hostileWallLimitPassed_ = false;
    return result;
  }

  bool InitializeHostile(GameplayHlodWorldRuntime3D &runtime,
                         Engine &engine,
                         GameplayHlodWorldRuntimeOptions3D options,
                         std::uint64_t worldOrdinal) {
    return runtime.Initialize(
        engine, assets_, environment_.Descriptors(),
        {GameplayEnvironmentIdentityDomain3D, worldOrdinal}, options);
  }

  bool RunHostileReview(Engine &engine, const Camera3D &camera) {
    constexpr auto retry = GameplayHlodWorldReadiness3D::RetryableNotReady;
    constexpr auto ready = GameplayHlodWorldReadiness3D::Ready;
    constexpr auto fatal = GameplayHlodWorldReadiness3D::Fatal;
    bool passed = true;

    {
      GameplayHlodWorldRuntime3D runtime;
      Scene3D scene;
      DrawList draw;
      const bool seeded = SeedHostOutputs(scene, draw);
      const auto before = HostFingerprint(scene, draw);
      const auto result = TimedCollect(runtime, 1, camera, scene, draw);
      runtimeUnavailableNegative_.receipt = runtime.Receipt();
      runtimeUnavailableNegative_.failClosed = seeded && result == fatal &&
          runtimeUnavailableNegative_.receipt.readiness == fatal &&
          runtimeUnavailableNegative_.receipt.terminalReason ==
              GameplayHlodWorldTerminalReason3D::RuntimeUnavailable &&
          HostFingerprint(scene, draw) == before;
      passed = passed && runtimeUnavailableNegative_.failClosed;
      if (!runtimeUnavailableNegative_.failClosed)
        std::cerr << "hostile case runtime-unavailable failed\n";
    }

    {
      GameplayHlodWorldRuntime3D runtime;
      GameplayHlodWorldRuntimeOptions3D options{};
      options.prewarmUpdates = 3;
      Scene3D scene;
      DrawList draw;
      passed = passed && InitializeHostile(runtime, engine, options, 0x101) &&
          SeedHostOutputs(scene, draw);
      const auto before = HostFingerprint(scene, draw);
      const auto result = passed ? TimedCollect(runtime, 1, camera, scene, draw)
                                 : fatal;
      const auto receipt = runtime.Receipt();
      firstReadyReceipt_ = receipt;
      firstReadyPassed_ = result == ready && receipt.SeamComplete() &&
          !receipt.firstFrameRetryObserved && !receipt.firstFrameRetryCount &&
          !receipt.sourceWaitIdleCalls && !(HostFingerprint(scene, draw) == before);
      passed = passed && firstReadyPassed_;
      if (!firstReadyPassed_)
        std::cerr << "hostile first-ready result=" << unsigned(result)
                  << " resident=" << receipt.partition.resident
                  << " pending=" << receipt.partitionPending
                  << " retries=" << receipt.firstFrameRetryCount
                  << " lifecycle=" << receipt.lifecycleInvariantPassed
                  << " seam=" << receipt.SeamComplete() << '\n';
      hostileStagingCopyNanoseconds_ = receipt.stagingCopyNanoseconds;
      hostileStagingCopyBytesEstimate_ = receipt.stagingCopyBytesEstimate;
      if (!passed)
        std::cerr << "hostile case first-ready failed\n";
      runtime.Shutdown();
    }

    {
      GameplayHlodWorldRuntime3D runtime;
      GameplayHlodWorldRuntimeOptions3D options{};
      options.sourceCompletionDelayPolls = 100;
      Scene3D scene;
      DrawList draw;
      passed = passed && InitializeHostile(runtime, engine, options, 0x102) &&
          SeedHostOutputs(scene, draw);
      const auto before = HostFingerprint(scene, draw);
      for (std::uint32_t attempt = 1; passed && attempt <= 8; ++attempt) {
        const auto result = TimedCollect(runtime, 1, camera, scene, draw);
        passed = result == (attempt < 8 ? retry : fatal) &&
            HostFingerprint(scene, draw) == before;
      }
      const auto latched = runtime.Receipt();
      const auto result = TimedCollect(runtime, 1, camera, scene, draw);
      const auto afterLatch = runtime.Receipt();
      retryLimitNegative_.receipt = latched;
      retryLimitNegative_.failClosed = latched.readiness == fatal &&
          latched.firstFrameRetryCount == 8 &&
          latched.terminalReason ==
              GameplayHlodWorldTerminalReason3D::RetryLimitExceeded &&
          latched.attemptRetryCounted && !latched.sourceWaitIdleCalls &&
          result == fatal &&
          afterLatch.firstFrameRetryCount == latched.firstFrameRetryCount &&
          afterLatch.ownership.frameRequests == latched.ownership.frameRequests &&
          afterLatch.ownership.rejectedDuplicateFrames ==
              latched.ownership.rejectedDuplicateFrames &&
          HostFingerprint(scene, draw) == before;
      passed = passed && retryLimitNegative_.failClosed;
      if (!retryLimitNegative_.failClosed)
        std::cerr << "hostile case retry-limit failed\n";
      runtime.Shutdown();
    }

    {
      GameplayHlodWorldRuntime3D runtime;
      GameplayHlodWorldRuntimeOptions3D options{};
      options.sourceCompletionDelayPolls = 5;
      Scene3D scene;
      DrawList draw;
      passed = passed && InitializeHostile(runtime, engine, options, 0x103) &&
          SeedHostOutputs(scene, draw);
      const auto before = HostFingerprint(scene, draw);
      for (std::uint32_t attempt = 1; passed && attempt <= 8; ++attempt) {
        const auto result = TimedCollect(runtime, 1, camera, scene, draw);
        passed = result == (attempt < 8 ? retry : ready) &&
            (attempt == 8 ? !(HostFingerprint(scene, draw) == before)
                          : HostFingerprint(scene, draw) == before);
      }
      const auto receipt = runtime.Receipt();
      completeBeforeLimitReceipt_ = receipt;
      completeBeforeLimitPassed_ = receipt.readiness == ready &&
          receipt.firstFrameRetryCount == 7 &&
          receipt.SeamComplete() && !receipt.sourceWaitIdleCalls;
      passed = passed && completeBeforeLimitPassed_;
      if (!completeBeforeLimitPassed_)
        std::cerr << "hostile case complete-before-limit failed\n";
      runtime.Shutdown();
    }

    {
      GameplayHlodWorldRuntime3D runtime;
      GameplayHlodWorldRuntimeOptions3D options{};
      options.prewarmUpdates = 3;
      Scene3D committedScene;
      DrawList committedDraw;
      passed = passed && InitializeHostile(runtime, engine, options, 0x104) &&
          TimedCollect(runtime, 1, camera, committedScene, committedDraw) == ready;
      Scene3D scene;
      DrawList draw;
      passed = passed && SeedHostOutputs(scene, draw);
      const auto before = HostFingerprint(scene, draw);
      const auto result = TimedCollect(runtime, 1, camera, scene, draw);
      const auto receipt = runtime.Receipt();
      passed = passed && result == fatal &&
          receipt.terminalReason ==
              GameplayHlodWorldTerminalReason3D::FrameInvariantViolation &&
          receipt.ownership.rejectedDuplicateFrames == 1 &&
          HostFingerprint(scene, draw) == before;
      if (!passed)
        std::cerr << "hostile case duplicate failed\n";
      runtime.Shutdown();
    }

    {
      GameplayHlodWorldRuntime3D runtime;
      GameplayHlodWorldRuntimeOptions3D options{};
      options.prewarmUpdates = 3;
      options.failureFrame = 1;
      options.failureInjection =
          GameplayHlodWorldFailureInjection3D::IdentityInvariant;
      Scene3D scene;
      DrawList draw;
      passed = passed && InitializeHostile(runtime, engine, options, 0x105) &&
          SeedHostOutputs(scene, draw);
      const auto before = HostFingerprint(scene, draw);
      const auto result = TimedCollect(runtime, 1, camera, scene, draw);
      const auto receipt = runtime.Receipt();
      identityNegative_.receipt = receipt;
      identityNegative_.failClosed = result == fatal &&
          receipt.readiness == fatal &&
          receipt.terminalReason ==
              GameplayHlodWorldTerminalReason3D::IdentityInvariantViolation &&
          !receipt.selected && !receipt.frozen && !receipt.collected &&
          HostFingerprint(scene, draw) == before;
      passed = passed && identityNegative_.failClosed;
      if (!identityNegative_.failClosed)
        std::cerr << "hostile case identity failed\n";
      runtime.Shutdown();
    }

    {
      GameplayHlodWorldRuntime3D runtime;
      GameplayHlodWorldRuntimeOptions3D options{};
      options.prewarmUpdates = 3;
      options.failureFrame = 2;
      options.failureInjection =
          GameplayHlodWorldFailureInjection3D::DataInvariant;
      Scene3D firstScene;
      DrawList firstDraw;
      passed = passed && InitializeHostile(runtime, engine, options, 0x106) &&
          TimedCollect(runtime, 1, camera, firstScene, firstDraw) == ready;
      Scene3D scene;
      DrawList draw;
      passed = passed && SeedHostOutputs(scene, draw);
      const auto before = HostFingerprint(scene, draw);
      const auto result = TimedCollect(runtime, 2, camera, scene, draw);
      const auto receipt = runtime.Receipt();
      dataNegative_.receipt = receipt;
      dataNegative_.failClosed = result == fatal && receipt.readiness == fatal &&
          receipt.attemptFrame == 2 &&
          receipt.terminalReason ==
              GameplayHlodWorldTerminalReason3D::DataInvariantViolation &&
          !receipt.selected && !receipt.frozen && !receipt.collected &&
          !receipt.frameInvariantPassed && !receipt.identityInvariantPassed &&
          !receipt.dataInvariantPassed && HostFingerprint(scene, draw) == before;
      passed = passed && dataNegative_.failClosed;
      if (!dataNegative_.failClosed)
        std::cerr << "hostile case next-frame-data failed\n";
      runtime.Shutdown();
    }

    {
      GameplayHlodWorldRuntime3D runtime;
      GameplayHlodWorldRuntimeOptions3D options{};
      options.prewarmUpdates = 3;
      options.failureFrame = 1;
      options.failureInjection =
          GameplayHlodWorldFailureInjection3D::LateDataInvariant;
      Scene3D scene;
      DrawList draw;
      passed = passed && InitializeHostile(runtime, engine, options, 0x107) &&
          SeedHostOutputs(scene, draw);
      const auto before = HostFingerprint(scene, draw);
      const auto result = TimedCollect(runtime, 1, camera, scene, draw);
      const auto receipt = runtime.Receipt();
      lateDataNegative_.receipt = receipt;
      lateDataNegative_.failClosed = result == fatal &&
          receipt.readiness == fatal && receipt.selected && receipt.frozen &&
          receipt.collected && !receipt.dataInvariantPassed &&
          receipt.transactionalStagingWip && receipt.stagingCopyNanoseconds &&
          HostFingerprint(scene, draw) == before;
      passed = passed && lateDataNegative_.failClosed;
      hostileStagingCopyNanoseconds_ =
          (std::max)(hostileStagingCopyNanoseconds_,
                     receipt.stagingCopyNanoseconds);
      hostileStagingCopyBytesEstimate_ =
          (std::max)(hostileStagingCopyBytesEstimate_,
                     receipt.stagingCopyBytesEstimate);
      if (!lateDataNegative_.failClosed)
        std::cerr << "hostile case late-data failed result=" << unsigned(result)
                  << " selected=" << receipt.selected
                  << " frozen=" << receipt.frozen
                  << " collected=" << receipt.collected
                  << " data=" << receipt.dataInvariantPassed
                  << " staging=" << receipt.transactionalStagingWip
                  << " copyNs=" << receipt.stagingCopyNanoseconds
                  << " unchanged=" << (HostFingerprint(scene, draw) == before)
                  << '\n';
      runtime.Shutdown();
    }

    hostileReviewPassed_ = passed && hostileWallLimitPassed_;
    return hostileReviewPassed_;
  }

  void Fail(const char *message, Engine &engine) {
    if (!Result) {
      std::cerr << message << '\n';
      Result = 1;
    }
    engine.Quit();
  }

  void WriteReceipt(const RendererStats &stats,
                    const ShowcaseProductionReleaseReceipt3D &release,
                    const RendererStats &after,
                    bool cleanupComplete) const {
    std::ofstream out(AEVRYND_GAMEPLAY_HLOD_RUNTIME_RECEIPT,
                      std::ios::binary | std::ios::trunc);
    if (!out) {
      Result = 3;
      return;
    }
    const bool sceneEcsActivated = far_.ownership.Complete() &&
        far_.collection.aliveEntities && far_.collection.submittedRenderables;
    const bool selectFreezeCollect =
        far_.selected && far_.frozen && far_.collected;
    const bool transactionalStagingWip = far_.transactionalStagingWip &&
        lateDataNegative_.receipt.transactionalStagingWip;
    const bool callerOutputsBitExact =
        runtimeUnavailableNegative_.failClosed && frameNegativeFailClosed_ &&
        identityNegative_.failClosed && dataNegative_.failClosed &&
        lateDataNegative_.failClosed && retryLimitNegative_.failClosed;
    const auto collectWaitIdleCalls = far_.sourceWaitIdleCalls +
        runtimeUnavailableNegative_.receipt.sourceWaitIdleCalls +
        retryLimitNegative_.receipt.sourceWaitIdleCalls +
        identityNegative_.receipt.sourceWaitIdleCalls +
        dataNegative_.receipt.sourceWaitIdleCalls +
        lateDataNegative_.receipt.sourceWaitIdleCalls;
    out << "{\"source\":\"GameplayHlodWorldRuntime3D\","
           "\"status\":\"NO-GO\",\"coreSemantics\":\"GO\",\"readiness\":\""
        << GameplayHlodWorldReadinessName3D(far_.readiness)
        << "\",\"terminalReason\":\""
        << GameplayHlodWorldTerminalReasonName3D(far_.terminalReason)
        << "\",\"firstFrameRetryCount\":" << far_.firstFrameRetryCount
        << ",\"firstFrameRetryLimit\":" << far_.firstFrameRetryLimit
        << ",\"firstFrameRetryObserved\":"
        << (far_.firstFrameRetryObserved ? 1 : 0)
        << ",\"firstFrameRetryReason\":\""
        << GameplayHlodWorldTerminalReasonName3D(far_.lastRetryReason) << "\""
        << ",\"frameInvariantPassed\":"
        << (far_.frameInvariantPassed ? 1 : 0)
        << ",\"identityInvariantPassed\":"
        << (far_.identityInvariantPassed ? 1 : 0)
        << ",\"dataInvariantPassed\":"
        << (far_.dataInvariantPassed ? 1 : 0)
        << ",\"syntheticSubstitution\":0,"
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
        << ",\"worldPartitionResident\":"
        << (far_.worldPartitionResident ? 1 : 0)
        << ",\"sceneEcsActivated\":" << (sceneEcsActivated ? 1 : 0)
        << ",\"selectFreezeCollect\":" << (selectFreezeCollect ? 1 : 0)
        << ",\"nearSourceClusters\":"
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
        << ",\"sceneEcsRegistryIndependent\":"
        << (far_.ownership.independentRegistry ? 1 : 0)
        << ",\"submissionOwner\":\""
        << GameplaySceneEcsSubmissionOwnerName3D(far_.ownership.owner)
        << "\","
           "\"acceptedHlodSubmissions\":"
        << far_.ownership.acceptedSubmissions
        << ",\"duplicateFrameRejections\":"
        << frameNegative_.ownership.rejectedDuplicateFrames
        << ",\"worldPartitionRequests\":" << far_.partitionRequests
        << ",\"worldPartitionDispatches\":" << far_.partitionDispatches
        << ",\"worldPartitionCompletions\":" << far_.partitionCompletions
         << ",\"worldPartitionPending\":"
         << (far_.partitionPending ? 1 : 0)
        << ",\"worldPartitionLifecycleInvariant\":"
        << (far_.lifecycleInvariantPassed ? 1 : 0)
        << ",\"hostileReviewPassed\":" << (hostileReviewPassed_ ? 1 : 0)
        << ",\"hostileFirstCall\":{\"readiness\":\""
        << GameplayHlodWorldReadinessName3D(firstReadyReceipt_.readiness)
        << "\",\"terminalReason\":\""
        << GameplayHlodWorldTerminalReasonName3D(
               firstReadyReceipt_.terminalReason)
        << "\",\"retryCount\":" << firstReadyReceipt_.firstFrameRetryCount
        << ",\"passed\":" << (firstReadyPassed_ ? 1 : 0) << "}"
        << ",\"completedBeforeRetryLimit\":{\"readiness\":\""
        << GameplayHlodWorldReadinessName3D(
               completeBeforeLimitReceipt_.readiness)
        << "\",\"terminalReason\":\""
        << GameplayHlodWorldTerminalReasonName3D(
               completeBeforeLimitReceipt_.terminalReason)
        << "\",\"retryCount\":"
        << completeBeforeLimitReceipt_.firstFrameRetryCount
        << ",\"passed\":" << (completeBeforeLimitPassed_ ? 1 : 0) << "}"
        << ",\"callerOutputsBitExact\":"
        << (callerOutputsBitExact ? 1 : 0)
        << ",\"collectWaitIdleCalls\":" << collectWaitIdleCalls
        << ",\"hostileMaxCollectWallNanoseconds\":"
        << hostileMaxCollectWallNanoseconds_
        << ",\"hostileCollectWallLimitNanoseconds\":"
        << HostileCollectWallLimitNanoseconds
        << ",\"transactionalStagingWip\":"
        << (transactionalStagingWip ? 1 : 0)
        << ",\"stagingCopyNanoseconds\":"
        << hostileStagingCopyNanoseconds_
        << ",\"stagingCopyBytesEstimate\":"
        << hostileStagingCopyBytesEstimate_
        << ",\"hostPaletteRows\":1,\"identityCollisions\":0"
        << ",\"representativeLowLodProxy\":"
        << (far_.build.representativeLowLodProxy ? 1 : 0)
        << ",\"aggregateProxyBuilt\":"
        << (far_.aggregateProxyBuilt ? 1 : 0)
        << ",\"gameplayLabOnRenderConnected\":0,"
           "\"gameplayIdentityDomainMigrated\":1,"
           "\"sourceProxyIdentitySeparated\":1,"
           "\"allDescriptorKindsCovered\":"
        << (far_.allDescriptorKindsCovered ? 1 : 0)
        << ",\"gpuValidationRequested\":"
        << (stats.gpuValidationRequested ? 1 : 0)
        << ",\"gpuValidationAvailable\":"
        << (stats.gpuValidationAvailable ? 1 : 0)
        << ",\"gpuValidationExecuted\":"
        << (stats.gpuValidationExecuted ? 1 : 0)
        << ",\"gpuValidationPassed\":"
        << (stats.gpuValidationPassed ? 1 : 0)
        << ",\"debugErrors\":" << stats.debugErrorCount
        << ",\"debugWarnings\":" << stats.debugWarningCount
        << ",\"gpuHlodSelectionRequests\":"
        << stats.gpuHlodSelectionRequests
        << ",\"gpuHlodSelectionDispatches\":"
        << stats.gpuHlodSelectionDispatches
        << ",\"gpuHlodSelectionCompletions\":"
        << stats.gpuHlodSelectionCompletions
        << ",\"gpuHlodSelectionPending\":"
        << (stats.gpuHlodSelectionPending ? 1 : 0)
        << ",\"negativeCases\":{\"runtimeUnavailable\":{\"readiness\":\""
        << GameplayHlodWorldReadinessName3D(
               runtimeUnavailableNegative_.receipt.readiness)
        << "\",\"terminalReason\":\""
        << GameplayHlodWorldTerminalReasonName3D(
               runtimeUnavailableNegative_.receipt.terminalReason)
        << "\",\"failClosed\":"
        << (runtimeUnavailableNegative_.failClosed ? 1 : 0)
        << "},\"frame\":{\"readiness\":\""
        << GameplayHlodWorldReadinessName3D(frameNegative_.readiness)
        << "\",\"terminalReason\":\""
        << GameplayHlodWorldTerminalReasonName3D(
               frameNegative_.terminalReason)
        << "\",\"failClosed\":" << (frameNegativeFailClosed_ ? 1 : 0)
        << "},\"identity\":{\"readiness\":\""
        << GameplayHlodWorldReadinessName3D(
               identityNegative_.receipt.readiness)
        << "\",\"terminalReason\":\""
        << GameplayHlodWorldTerminalReasonName3D(
               identityNegative_.receipt.terminalReason)
        << "\",\"failClosed\":"
        << (identityNegative_.failClosed ? 1 : 0)
        << "},\"data\":{\"readiness\":\""
        << GameplayHlodWorldReadinessName3D(dataNegative_.receipt.readiness)
        << "\",\"terminalReason\":\""
        << GameplayHlodWorldTerminalReasonName3D(
               dataNegative_.receipt.terminalReason)
        << "\",\"failClosed\":" << (dataNegative_.failClosed ? 1 : 0)
        << "},\"lateData\":{\"readiness\":\""
        << GameplayHlodWorldReadinessName3D(
               lateDataNegative_.receipt.readiness)
        << "\",\"terminalReason\":\""
        << GameplayHlodWorldTerminalReasonName3D(
               lateDataNegative_.receipt.terminalReason)
        << "\",\"failClosed\":" << (lateDataNegative_.failClosed ? 1 : 0)
        << "},\"retryLimit\":{\"readiness\":\""
        << GameplayHlodWorldReadinessName3D(
               retryLimitNegative_.receipt.readiness)
        << "\",\"terminalReason\":\""
        << GameplayHlodWorldTerminalReasonName3D(
               retryLimitNegative_.receipt.terminalReason)
        << "\",\"failClosed\":"
        << (retryLimitNegative_.failClosed ? 1 : 0)
        << "}},\"postCleanup\":{"
           "\"rendererBaselineRestored\":"
        << (release.rendererBaselineRestored ? 1 : 0)
        << ",\"baselineMeshes\":" << baseline_.residentMeshes
        << ",\"residentMeshes\":" << after.residentMeshes
        << ",\"baselineMaterialTextureSets\":"
        << baseline_.residentMaterialTextureSets
        << ",\"residentMaterialTextureSets\":"
        << after.residentMaterialTextureSets
        << ",\"pendingGpuReleaseBytes\":" << after.pendingGpuReleaseBytes
        << ",\"leasesReleased\":" << release.leasesReleased
        << ",\"meshesReleased\":" << release.meshesReleased
        << ",\"materialsReleased\":" << release.materialsReleased
        << ",\"complete\":" << (cleanupComplete ? 1 : 0) << "}"
        << ",\"remaining\":\"Connect GameplayLabGame OnRender without duplicate SceneECS ownership, then validate GPU-side selection, gameplay transition pixels, and raw CPU/GPU performance gates.\"}";
  }

  SceneECSRegistry3D registry_{};
  SceneEntity3D paletteEntity_{};
  MeshResource3D paletteMesh_{};
  Scene3D scene_{};
  GameplayShowcaseEnvironment3D environment_{};
  ShowcaseRenderAssetBindings3D assets_{};
  ShowcaseProductionRenderReceipt3D load_{};
  GameplayHlodWorldRuntime3D runtime_{};
  GameplayHlodWorldRuntimeReceipt3D near_{}, transition_{}, far_{},
      frameNegative_{}, firstReadyReceipt_{}, completeBeforeLimitReceipt_{};
  RuntimeNegativeEvidence runtimeUnavailableNegative_{},
      retryLimitNegative_{}, identityNegative_{}, dataNegative_{},
      lateDataNegative_{};
  RendererStats baseline_{}, validation_{};
  std::uint64_t nextFrame_{1};
  unsigned attempts_{}, stage_{};
  std::size_t transitionSourceRows_{}, transitionProxyRows_{};
  bool transitionProxyInvert_{true}, transitionSourceInvert_{},
      transitionSharedThreshold_{}, transitionCoverageComplement_{},
      transitionIdentitySeparated_{true};
  static constexpr std::uint64_t HostileCollectWallLimitNanoseconds =
      100000000ull;
  std::uint64_t hostileMaxCollectWallNanoseconds_{},
      hostileStagingCopyNanoseconds_{}, hostileStagingCopyBytesEstimate_{};
  bool hostileWallLimitPassed_{true}, hostileReviewPassed_{},
      frameNegativeFailClosed_{}, firstReadyPassed_{},
      completeBeforeLimitPassed_{}, complete_{}, cleanupComplete_{};
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
