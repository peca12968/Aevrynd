#include "FrontierWorldGame.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string_view>

using namespace Aevrynd;
using namespace FrontierWorldRules;
namespace {
constexpr std::size_t ShowcaseCpuWindowCount = 7;
constexpr uint64_t ShowcaseCpuBudgetNanoseconds = 8'000'000;
constexpr uint64_t ShowcaseCpuCatastrophicNanoseconds = 50'000'000;
struct ShowcaseCpuPerformanceReceipt {
  std::size_t sampleCount{};
  std::array<std::size_t, ShowcaseCpuWindowCount> windowSampleCounts{};
  std::array<uint64_t, ShowcaseCpuWindowCount> windowP95Nanoseconds{};
  uint64_t rawP95Nanoseconds{}, medianWindowP95Nanoseconds{}, maximumNanoseconds{};
  bool coverageComplete{}, passed{};
};
struct ThreadCycleCalibrationReceipt {std::array<uint64_t,7> cyclesPerSecond{};uint64_t medianCyclesPerSecond{};bool valid{};};
ThreadCycleCalibrationReceipt CalibrateCurrentThreadCycles(){ThreadCycleCalibrationReceipt result{};LARGE_INTEGER frequency{};if(!QueryPerformanceFrequency(&frequency)||frequency.QuadPart<=0)return result;for(std::size_t sample=0;sample<result.cyclesPerSecond.size();++sample){ULONG64 beginCycles{},endCycles{};LARGE_INTEGER begin{},now{};if(!QueryThreadCycleTime(GetCurrentThread(),&beginCycles)||!QueryPerformanceCounter(&begin))return result;do{YieldProcessor();QueryPerformanceCounter(&now);}while(now.QuadPart-begin.QuadPart<frequency.QuadPart/20);if(!QueryThreadCycleTime(GetCurrentThread(),&endCycles)||endCycles<=beginCycles||now.QuadPart<=begin.QuadPart)return result;const long double cyclesPerSecond=long double(endCycles-beginCycles)*long double(frequency.QuadPart)/long double(now.QuadPart-begin.QuadPart);if(!std::isfinite(double(cyclesPerSecond))||cyclesPerSecond<1||cyclesPerSecond>long double((std::numeric_limits<uint64_t>::max)()))return result;result.cyclesPerSecond[sample]=uint64_t(cyclesPerSecond);}auto sorted=result.cyclesPerSecond;std::ranges::sort(sorted);result.medianCyclesPerSecond=sorted[3];result.valid=result.medianCyclesPerSecond&&double(sorted.back()-sorted.front())/double(result.medianCyclesPerSecond)<=.10;return result;}
ShowcaseCpuPerformanceReceipt EvaluateShowcaseCpuPerformance(std::span<const uint64_t> samples,uint64_t medianBudgetNanoseconds=ShowcaseCpuBudgetNanoseconds,uint64_t maximumBudgetNanoseconds=ShowcaseCpuCatastrophicNanoseconds) {
  ShowcaseCpuPerformanceReceipt result{};
  result.sampleCount=samples.size();
  if(samples.size()<ShowcaseCpuWindowCount||std::ranges::any_of(samples,[](uint64_t value){return value==0;}))return result;
  const auto percentile95=[](std::vector<uint64_t> values){std::sort(values.begin(),values.end());return values[(values.size()-1)*95/100];};
  result.rawP95Nanoseconds=percentile95({samples.begin(),samples.end()});
  result.maximumNanoseconds=*std::ranges::max_element(samples);
  std::size_t covered=0;
  for(std::size_t window=0;window<ShowcaseCpuWindowCount;++window){const std::size_t begin=samples.size()*window/ShowcaseCpuWindowCount,end=samples.size()*(window+1)/ShowcaseCpuWindowCount;result.windowSampleCounts[window]=end-begin;if(begin==end)return result;result.windowP95Nanoseconds[window]=percentile95({samples.begin()+begin,samples.begin()+end});covered+=end-begin;}
  auto medians=result.windowP95Nanoseconds;std::ranges::sort(medians);result.medianWindowP95Nanoseconds=medians[ShowcaseCpuWindowCount/2];result.coverageComplete=covered==samples.size();result.passed=result.coverageComplete&&result.medianWindowP95Nanoseconds<=medianBudgetNanoseconds&&result.maximumNanoseconds<=maximumBudgetNanoseconds;return result;
}
constexpr uint32_t WorldLayer = 1, BuildingLayer = 2, ActorLayer = 4;
constexpr Color Grass{.19f, .39f, .18f, 1}, DarkGrass{.1f, .26f, .13f, 1},
    Wood{.42f, .21f, .08f, 1}, Stone{.4f, .44f, .45f, 1},
    Leaf{.1f, .34f, .13f, 1}, Cyan{.18f, .9f, .82f, 1},
    Gold{1.f, .72f, .22f, 1}, Ink{.78f, .9f, .88f, 1},
    Coral{.92f, .22f, .14f, 1};
constexpr Surface3D GroundSurface{.96f, 0, .12f, .92f},
    WaterSurface{.12f, .08f, .05f, 1}, WoodSurface{.82f, 0, .09f, .88f},
    FoliageSurface{.9f, 0, .12f, .86f}, StoneSurface{.88f, .04f, .07f, .8f},
    SkinSurface{.58f, 0, .04f, 1}, MetalSurface{.16f, .9f, .03f, 1};
constexpr std::array<const wchar_t *, 14> ShowcaseCaptureNames = {
    L"day-first-terrain.png",
    L"day-third-forest.png",
    L"day-god-map.png",
    L"day-river-water.png",
    L"dusk-first-road.png",
    L"dusk-third-settlement.png",
    L"dusk-god-map.png",
    L"dusk-river-reflection.png",
    L"night-first-settlement.png",
    L"night-third-forest.png",
    L"night-god-map.png",
    L"night-river-reflection.png",
    L"feature-off.png",
    L"feature-on.png"};
float RiverCenter(float z) {
  return 28.f + std::sin(z * .018f) * 13.f + std::sin(z * .051f) * 4.f;
}
float RiverWidth(float z) {
  return 10.f + 2.5f * (.5f + .5f * std::sin(z * .031f + 1.4f));
}
MeshLodPolicy3D WorldLod(float switchDistance) {
  return {switchDistance, (std::max)(3.f, switchDistance * .16f),
          MeshLodDistanceMetric3D::BoundsSurface};
}
JointTransform3D Joint(Vec3 translation, Vec3 rotation = {}) {
  return {translation, rotation, {1, 1, 1}};
}
AnimationTrack3D Track(std::size_t joint,
                       std::initializer_list<AnimationKeyframe3D> frames) {
  return {joint, frames};
}
void CreateAnimationLibrary(Skeleton3D &humanoid, Skeleton3D &creature,
                            AnimationClip3D &idle, AnimationClip3D &walk,
                            AnimationClip3D &attack,
                            AnimationClip3D &creatureWalk) {
  humanoid.joints = {{-1, Joint({0, 1.15f, 0})},
                     {0, Joint({0, .73f, 0})},
                     {0, Joint({-.48f, .18f, 0})},
                     {0, Joint({.48f, .18f, 0})},
                     {0, Joint({-.2f, -.77f, 0})},
                     {0, Joint({.2f, -.77f, 0})},
                     {3, Joint({0, -.48f, .45f}, {.18f, 0, 0})}};
  idle = {2,
          true,
          {Track(0, {{0, Joint({0, 1.15f, 0})},
                     {1, Joint({0, 1.19f, 0})},
                     {2, Joint({0, 1.15f, 0})}}),
           Track(1, {{0, Joint({0, .73f, 0}, {0, -.08f, 0})},
                     {1, Joint({0, .73f, 0}, {0, .08f, 0})},
                     {2, Joint({0, .73f, 0}, {0, -.08f, 0})}})}};
  walk = {.8f,
          true,
          {Track(0, {{0, Joint({0, 1.15f, 0})},
                     {.2f, Joint({0, 1.2f, 0})},
                     {.4f, Joint({0, 1.15f, 0})},
                     {.6f, Joint({0, 1.2f, 0})},
                     {.8f, Joint({0, 1.15f, 0})}}),
           Track(2, {{0, Joint({-.48f, .18f, 0}, {.65f, 0, 0})},
                     {.4f, Joint({-.48f, .18f, 0}, {-.65f, 0, 0})},
                     {.8f, Joint({-.48f, .18f, 0}, {.65f, 0, 0})}}),
           Track(3, {{0, Joint({.48f, .18f, 0}, {-.65f, 0, 0})},
                     {.4f, Joint({.48f, .18f, 0}, {.65f, 0, 0})},
                     {.8f, Joint({.48f, .18f, 0}, {-.65f, 0, 0})}}),
           Track(4, {{0, Joint({-.2f, -.77f, 0}, {-.55f, 0, 0})},
                     {.4f, Joint({-.2f, -.77f, 0}, {.55f, 0, 0})},
                     {.8f, Joint({-.2f, -.77f, 0}, {-.55f, 0, 0})}}),
           Track(5, {{0, Joint({.2f, -.77f, 0}, {.55f, 0, 0})},
                     {.4f, Joint({.2f, -.77f, 0}, {-.55f, 0, 0})},
                     {.8f, Joint({.2f, -.77f, 0}, {.55f, 0, 0})}})}};
  attack = {.48f,
            false,
            {Track(0, {{0, Joint({0, 1.15f, 0}, {0, -.18f, 0})},
                       {.18f, Joint({0, 1.15f, 0}, {0, .32f, 0})},
                       {.48f, Joint({0, 1.15f, 0})}}),
             Track(3, {{0, Joint({.48f, .18f, 0}, {-1.05f, 0, -.2f})},
                       {.18f, Joint({.48f, .18f, 0}, {.9f, 0, .5f})},
                       {.48f, Joint({.48f, .18f, 0}, {-.25f, 0, 0})}})}};
  creature.joints = {
      {-1, Joint({0, .62f, 0})},        {0, Joint({0, .16f, .58f})},
      {0, Joint({-.4f, -.32f, .28f})},  {0, Joint({.4f, -.32f, .28f})},
      {0, Joint({-.4f, -.32f, -.28f})}, {0, Joint({.4f, -.32f, -.28f})}};
  creatureWalk = {
      .65f,
      true,
      {Track(0, {{0, Joint({0, .62f, 0})},
                 {.325f, Joint({0, .68f, 0})},
                 {.65f, Joint({0, .62f, 0})}}),
       Track(1, {{0, Joint({0, .16f, .58f}, {.12f, 0, 0})},
                 {.325f, Joint({0, .16f, .58f}, {-.12f, 0, 0})},
                 {.65f, Joint({0, .16f, .58f}, {.12f, 0, 0})}}),
       Track(2, {{0, Joint({-.4f, -.32f, .28f}, {-.45f, 0, 0})},
                 {.325f, Joint({-.4f, -.32f, .28f}, {.45f, 0, 0})},
                 {.65f, Joint({-.4f, -.32f, .28f}, {-.45f, 0, 0})}}),
       Track(3, {{0, Joint({.4f, -.32f, .28f}, {.45f, 0, 0})},
                 {.325f, Joint({.4f, -.32f, .28f}, {-.45f, 0, 0})},
                 {.65f, Joint({.4f, -.32f, .28f}, {.45f, 0, 0})}}),
       Track(4, {{0, Joint({-.4f, -.32f, -.28f}, {.45f, 0, 0})},
                 {.325f, Joint({-.4f, -.32f, -.28f}, {-.45f, 0, 0})},
                 {.65f, Joint({-.4f, -.32f, -.28f}, {.45f, 0, 0})}}),
       Track(5, {{0, Joint({.4f, -.32f, -.28f}, {-.45f, 0, 0})},
                 {.325f, Joint({.4f, -.32f, -.28f}, {.45f, 0, 0})},
                 {.65f, Joint({.4f, -.32f, -.28f}, {-.45f, 0, 0})}})}};
}

Vec3 JointPosition(const AnimationPose3D &pose, std::size_t joint,
                   Vec3 local = {}) {
  return TransformPoint(pose.globalMatrices[joint], local);
}
uint32_t NextRandom(uint32_t &state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}
float RandomRange(uint32_t &state, float minimum, float maximum) {
  return minimum + (maximum - minimum) *
                       float(NextRandom(state) & 0x00ffffffu) /
                       float(0x01000000u);
}
Material3D MaterialVariant(Material3D material, Color tint, float scale,
                           bool castsShadow = true) {
  material.tint = tint;
  material.textureScale = (std::max)(.001f, scale);
  material.castsShadow = castsShadow;
  return material;
}
Material3D FoliageMaterial(Color tint, bool castsShadow = true) {
  Material3D material{tint, FoliageSurface, castsShadow};
  material.materialClass = MaterialClass3D::Foliage;
  return material;
}
Material3D WaterMaterial(Color tint, Surface3D surface = WaterSurface) {
  Material3D material{tint, surface, false};
  material.materialClass = MaterialClass3D::Water;
  return material;
}
std::array<uint64_t, 10> ShowcaseFeatureCounters(const RendererStats &stats) {
  uint64_t shadowCascades{};
  for (uint64_t value : stats.lastDirectionalShadowCascadeRendered)
    shadowCascades += value;
  return {shadowCascades,
          stats.softDirectionalShadowBlockerSamples,
          stats.contactShadowEvaluationPasses,
          stats.ambientOcclusionEvaluationPasses,
          stats.screenSpaceReflectionPasses,
          stats.bloomPasses,
          stats.volumetricCloudConfiguredFrames,
          stats.volumetricCloudShadowConfiguredFrames,
          stats.directionalLightShaftConfiguredFrames,
          stats.screenSpaceWaterRefractionPasses};
}
} // namespace

Aevrynd::ReusableMemoryStats FrontierWorldGame::GetReusableMemoryStats() const {
  const ReusableMemoryStats scene = scene_.MemoryStats(),
                            collision = collisionWorld_.MemoryStats(),
                            streaming = worldStreamer_.MemoryStats(),
                            navigation = navigationWorkspace_.MemoryStats();
  std::size_t retained = scene.retainedCapacityBytes +
                         collision.retainedCapacityBytes +
                         streaming.retainedCapacityBytes +
                         navigation.retainedCapacityBytes +
                         buildings_.capacity() * sizeof(Building) +
                         monsters_.capacity() * sizeof(Monster),
              reclaimable = scene.reclaimableCapacityBytes +
                            collision.reclaimableCapacityBytes +
                            streaming.reclaimableCapacityBytes +
                            navigation.reclaimableCapacityBytes;
  for (const Monster &monster : monsters_) {
    retained += monster.path.capacity() * sizeof(Vec3);
    if (monster.path.capacity() > monster.path.size())
      reclaimable +=
          (monster.path.capacity() - monster.path.size()) * sizeof(Vec3);
  }
  auto excess = [](std::size_t capacity, std::size_t size, std::size_t baseline,
                   std::size_t element) {
    const std::size_t target = (std::max)(size, baseline);
    return capacity > target ? (capacity - target) * element : std::size_t{};
  };
  reclaimable +=
      excess(buildings_.capacity(), buildings_.size(), 32, sizeof(Building)) +
      excess(monsters_.capacity(), monsters_.size(), 64, sizeof(Monster));
  return {retained, reclaimable, lastReusableMemoryReleasedBytes_,
          reusableMemoryTrimCount_};
}
std::size_t FrontierWorldGame::TrimReusableMemory() {
  const std::size_t before = GetReusableMemoryStats().retainedCapacityBytes;
  scene_.TrimUnusedMemory();
  collisionWorld_.TrimUnusedMemory();
  worldStreamer_.TrimUnusedMemory();
  navigationWorkspace_.TrimUnusedMemory(
      std::size_t(navigationGrid_.Settings().width) *
      navigationGrid_.Settings().depth);
  auto compact = []<class T>(std::vector<T> &values, std::size_t baseline) {
    std::vector<T> replacement;
    replacement.reserve((std::max)(values.size(), baseline));
    replacement.insert(replacement.end(),
                       std::make_move_iterator(values.begin()),
                       std::make_move_iterator(values.end()));
    values.swap(replacement);
  };
  for (Monster &monster : monsters_)
    compact(monster.path, monster.path.size());
  compact(buildings_, 32);
  compact(monsters_, 64);
  const std::size_t after = GetReusableMemoryStats().retainedCapacityBytes;
  lastReusableMemoryReleasedBytes_ = before > after ? before - after : 0;
  ++reusableMemoryTrimCount_;
  return lastReusableMemoryReleasedBytes_;
}

void FrontierWorldGame::OnStart(Engine &engine) {
  engine.SetRenderQuality(RenderQuality3D::Ultra);
  showcaseGpuSamples_.reserve(4096);
  showcaseCpuSamples_.reserve(4096);
  showcaseCpuWorkSamples_.reserve(4096);
  for(auto& samples:showcaseCpuStageSamples_)samples.reserve(4096);
  if(!captureDirectory_.empty()){const ThreadCycleCalibrationReceipt calibration=CalibrateCurrentThreadCycles();showcaseThreadCycleCalibration_=calibration.cyclesPerSecond;showcaseThreadCyclesPerSecond_=calibration.medianCyclesPerSecond;showcaseThreadCycleCalibrationValid_=calibration.valid;ULONG64 cycles{};if(showcaseThreadCycleCalibrationValid_&&QueryThreadCycleTime(GetCurrentThread(),&cycles))showcasePreviousThreadCycles_=cycles;else showcaseThreadCycleCalibrationValid_=false;}
  DrawList &high = engine.Draw();
  high.SetDirectionalShadowCascades({4, 2048, .1f, 220.f, .7f, .1f});
  high.SetSoftDirectionalShadows({true, 24.f, 128.f, 36.f});
  high.SetContactShadows({true, 16, 1.8f, .045f, .85f});
  high.SetAmbientOcclusion({true, 1.5f, .03f, 1.15f});
  high.SetScreenSpaceReflections({true, .82f, 42.f, .7f, .32f, 16});
  high.SetBloom({true, 1.05f, .55f, .13f, 3.f});
  high.SetVolumetricClouds(
      {true, 1100.f, 1700.f, .48f, 1.08f, .28f, 14.f, 16, true, .7f});
  high.SetDirectionalLightShafts({true, .68f, .72f, 360.f, 6});
  high.SetTerrainDistanceMaterial(
      {true, 44.f, 196.f, .0065f, .16f, 1.f, .14f});
  high.SetWaterOptics({true, .022f, .42f, .04f});
  if(engine.RequestedRenderQuality()==RenderQuality3D::Ultra)high.SetDirectionalShadowCascades({4,4096,.1f,220.f,.70f,.10f});
  if (!captureDirectory_.empty()) {
    std::filesystem::create_directories(captureDirectory_);
    showcaseRunId_ =
        std::format("{:08x}-{:016x}", GetCurrentProcessId(), GetTickCount64());
    for (const wchar_t *name : ShowcaseCaptureNames) {
      std::error_code error;
      std::filesystem::remove(captureDirectory_ / name, error);
    }
    std::error_code error;
    std::filesystem::remove(
        captureDirectory_ / L"frontier-showcase-receipt.json", error);
  }
  boxMesh_ = engine.CreateMeshResource3D(Mesh3D::CreateUnitBox());
  pyramidMesh_ = engine.CreateMeshResource3D(Mesh3D::CreateUnitPyramid());
  cylinderMesh_ = engine.CreateMeshResource3D(Mesh3D::CreateUnitCylinder(12));
  coneHighMesh_ = engine.CreateMeshResource3D(Mesh3D::CreateUnitCone(14));
  coneLowMesh_ = engine.CreateMeshResource3D(Mesh3D::CreateUnitCone(6));
  sphereHighMesh_ =
      engine.CreateMeshResource3D(Mesh3D::CreateUnitSphere(12, 8));
  sphereLowMesh_ = engine.CreateMeshResource3D(Mesh3D::CreateUnitSphere(6, 3));
  engine.SetMaterialTextureMemoryBudget(96 * 1024 * 1024);
  materialLibrary_.Initialize(engine, engine.ExecutableDirectory() / "assets" /
                                          "material-packs");
  const std::array<MaterialAssetDefinition3D, 4> materials = {
      MaterialAssetDefinition3D{
          "ground",
          {"ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg",
           "ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg", 1024,
           "ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"},
          {{1, 1, 1, 1}, GroundSurface, false, {}, .085f}},
      MaterialAssetDefinition3D{
          "wood",
          {"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg",
           "ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg",
           1024,
           "ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg"},
          {{1, 1, 1, 1}, WoodSurface, true, {}, .48f}},
      MaterialAssetDefinition3D{"bark",
                                {"poly-haven/bark_brown_02/1k/base-color.jpg",
                                 "poly-haven/bark_brown_02/1k/roughness.jpg",
                                 1024,
                                 "poly-haven/bark_brown_02/1k/normal-dx.jpg"},
                                {{1, 1, 1, 1}, WoodSurface, true, {}, 1.f}},
      MaterialAssetDefinition3D{
          "rock",
          {"poly-haven/aerial_ground_rock/1k/base-color.jpg",
           "poly-haven/aerial_ground_rock/1k/roughness.jpg", 1024,
           "poly-haven/aerial_ground_rock/1k/normal-dx.jpg"},
          {{1, 1, 1, 1}, StoneSurface, true, {}, .5f}}};
  if (!materialLibrary_.LoadAll(materials))
    throw std::runtime_error("CC0 PBR material loading failed");
  materialLibraryStats_ = materialLibrary_.Stats();
  if (const Material3D *material = materialLibrary_.Find("ground"))
    groundMaterial_ = *material;
  if (const Material3D *material = materialLibrary_.Find("wood"))
    woodMaterial_ = *material;
  if (const Material3D *material = materialLibrary_.Find("bark"))
    barkMaterial_ = *material;
  if (const Material3D *material = materialLibrary_.Find("rock"))
    rockMaterial_ = *material;
  groundMaterial_.normalScale = .65f;
  woodMaterial_.normalScale = .8f;
  barkMaterial_.normalScale = 1.1f;
  rockMaterial_.normalScale = 1.2f;
  const std::filesystem::path modelRoot =
      engine.ExecutableDirectory() / "assets" / "model-packs";
  modelMaterialLibrary_.Initialize(engine, modelRoot);
  const std::array<MaterialAssetDefinition3D, 2> modelMaterials = {
      MaterialAssetDefinition3D{
          "quiver-tree-02",
          {"poly-haven/quiver_tree_02/1k/base-color.jpg",
           "poly-haven/quiver_tree_02/1k/roughness.jpg", 1024,
           "poly-haven/quiver_tree_02/1k/normal-dx.jpg"},
          {{1, 1, 1, 1}, {.82f, 0, 0, .92f}, true, {}, 1.f, true}},
      MaterialAssetDefinition3D{
          "rock-09",
          {"poly-haven/rock_09/1k/base-color.jpg",
           "poly-haven/rock_09/1k/roughness.jpg", 1024,
           "poly-haven/rock_09/1k/normal-dx.jpg"},
          {{1, 1, 1, 1}, StoneSurface, true, {}, 1.f, true}}};
  if (!modelMaterialLibrary_.LoadAll(modelMaterials))
    throw std::runtime_error("CC0 model material loading failed");
  if (const Material3D *material = modelMaterialLibrary_.Find("quiver-tree-02"))
    quiverTreeMaterial_ = *material;
  quiverTreeMaterial_.materialClass = MaterialClass3D::Foliage;
  quiverTreeMaterial_.normalScale = .9f;
  if (const Material3D *material = modelMaterialLibrary_.Find("rock-09"))
    rock09Material_ = *material;
  rock09Material_.normalScale = 1.15f;
  const MaterialLibraryStats3D modelStats = modelMaterialLibrary_.Stats();
  materialLibraryStats_.materials += modelStats.materials;
  materialLibraryStats_.uniqueTextureSets += modelStats.uniqueTextureSets;
  const MeshLoadResult3D quiverTree = LoadAevryndMeshFile(
                             modelRoot / "poly-haven" / "quiver_tree_02" /
                             "quiver_tree_02.aemesh"),
                         rock09 =
                             LoadAevryndMeshFile(modelRoot / "poly-haven" /
                                                 "rock_09" / "rock_09.aemesh");
  if (!quiverTree || !rock09)
    throw std::runtime_error("CC0 model mesh loading failed");
  quiverTreeMesh_ = engine.CreateMeshResource3D(quiverTree.mesh);
  rock09Mesh_ = engine.CreateMeshResource3D(rock09.mesh);
  if (!quiverTreeMesh_ || !rock09Mesh_)
    throw std::runtime_error("CC0 model GPU upload failed");
  terrain_.Configure({{-256, 0, -256}, 129, 129, 4});
  terrain_.GenerateNatural(0x4a17u, {24, .0115f, .62f, 28, 6});
  terrain_.FlattenCircle({0, 0, 0}, 20, 0, 16);
  for (unsigned z = 0; z < terrain_.Settings().depth; ++z)
    for (unsigned x = 0; x < terrain_.Settings().width; ++x) {
      const Vec3 sample = terrain_.SamplePosition(x, z);
      const float riverDistance = std::abs(sample.x - RiverCenter(sample.z)),
                  riverAmount =
                      Clamp((RiverWidth(sample.z) + 8.f - riverDistance) / 8.f,
                            0.f, 1.f);
      const float lakeDistance = Length(Vec2{sample.x - 40.f, sample.z - 72.f}),
                  lakeAmount = Clamp((30.f - lakeDistance) / 11.f, 0.f, 1.f);
      const float amount = (std::max)(riverAmount, lakeAmount);
      if (amount > 0)
        terrain_.SetHeight(x, z, sample.y + (-2.4f - sample.y) * amount);
    }
  terrainMesh_ =
      engine.CreateMeshResource3D(terrain_.CreateMesh({{.1f, .25f, .075f, 1},
                                                       {.42f, .55f, .27f, 1},
                                                       {.34f, .32f, .3f, 1},
                                                       .77f,
                                                       {.27f, .18f, .09f, 1},
                                                       -2.1f,
                                                       5.5f}));
  grassHighMesh_ = engine.CreateMeshResource3D(CreateGrassClumpMesh3D(11, 3));
  grassLowMesh_ = engine.CreateMeshResource3D(CreateGrassClumpMesh3D(4, 2));
  treeLowMesh_ = engine.CreateMeshResource3D(CreateTreeLodMesh3D());
  skyDomeMesh_ = engine.CreateMeshResource3D(CreateSkyDomeMesh3D());
  std::vector<Vec3> riverCenters;
  std::vector<float> riverWidths;
  riverCenters.reserve(513);
  riverWidths.reserve(513);
  // 河道每公尺取樣，讓曲線岸線在俯視與低角度觀察時保持平順。
  for (int index = 0; index < 513; ++index) {
    const float z = -256.f + float(index), x = RiverCenter(z),
                width = RiverWidth(z);
    riverCenters.push_back({x, -2.05f, z});
    riverWidths.push_back(width);
  }
  riverMesh_ = engine.CreateMeshResource3D(
      CreateCurvedRibbonMesh3D(riverCenters, riverWidths));
  scene_.SetBoxMesh(boxMesh_);
  scene_.Reserve({0, 16, 64, 4096, 2048});
  controller_.Reset({0, terrain_.SampleHeight(0, -8), -8});
  worldStreamer_.Configure({32, 3, 4, 7, 12, 0x4a17u});
  for (int index = 0; index < 8; ++index)
    worldStreamingStats_ = worldStreamer_.Update(controller_.Position());
  for (const Vec3 cabin : std::array<Vec3, 3>{
           Vec3{-12, 0, 17}, Vec3{-38, 0, -18}, Vec3{14, 0, 34}}) {
    const float base = terrain_.SampleHeight(cabin.x, cabin.z);
    collisionWorld_.CreateBox(
        {{{cabin.x, base + 1.8f, cabin.z}, {3.6f, 1.8f, 3.f}},
         BuildingLayer,
         CollisionLayerAll3D,
         0,
         false});
  }
  navigationGrid_.Configure({{-128, 0, -128}, 128, 128, 2, 4096, true});
  RebuildNavigation();
  swingSound_ = engine.Audio().CreateSound(GenerateTone(180, .1f, .18f));
  buildSound_ = engine.Audio().CreateSound(GenerateTone(620, .12f, .14f));
  CreateAnimationLibrary(humanoidSkeleton_, creatureSkeleton_, idleClip_,
                         walkClip_, attackClip_, creatureWalkClip_);
  playerAnimation_.Play(idleClip_, 0);
  PopulateWorld();
  if (CommandLineHasOption(GetCommandLineW(), L"--river-view")) {
    const float riverZ = 0.f,
                riverX = RiverCenter(riverZ) - RiverWidth(riverZ) - 10.f;
    controller_.Reset({riverX, terrain_.SampleHeight(riverX, riverZ), riverZ});
    yaw_ = -1.5707963f;
    viewMode_ = ViewMode::ThirdPerson;
    automatic_ = false;
    riverView_ = true;
  } else if (CommandLineHasOption(GetCommandLineW(), L"--first-person")) {
    viewMode_ = ViewMode::FirstPerson;
    automatic_ = false;
  } else if (CommandLineHasOption(GetCommandLineW(), L"--third-person")) {
    viewMode_ = ViewMode::ThirdPerson;
    automatic_ = false;
  } else if (CommandLineHasOption(GetCommandLineW(), L"--god-view")) {
    viewMode_ = ViewMode::GodView;
    automatic_ = false;
  }
  if (engine.DLSSOn())
    engine.ToggleDLSS();
}
void FrontierWorldGame::OnStop(Engine &engine) {
  scene_.SetBoxMesh({});
  for (MeshResource3D *mesh :
       {&boxMesh_, &pyramidMesh_, &cylinderMesh_, &coneHighMesh_, &coneLowMesh_,
        &sphereHighMesh_, &sphereLowMesh_, &terrainMesh_, &grassHighMesh_,
        &grassLowMesh_, &treeLowMesh_, &skyDomeMesh_, &riverMesh_,
        &quiverTreeMesh_, &rock09Mesh_}) {
    engine.DestroyMesh3D(mesh->handle);
    *mesh = {};
  }
  modelMaterialLibrary_.Shutdown();
  materialLibrary_.Shutdown();
  materialLibraryStats_ = {};
  engine.Audio().DestroySound(swingSound_);
  engine.Audio().DestroySound(buildSound_);
  collisionWorld_.Clear();
  groundMaterial_ = woodMaterial_ = barkMaterial_ = rockMaterial_ =
      quiverTreeMaterial_ = rock09Material_ = {};
}

void FrontierWorldGame::PopulateWorld() {
  monsters_ = {{{0, 0, -3}, 55, 55, 2.7f, 0, MonsterKind::Wolf, true},
               {{-8, 0, 4}, 55, 55, 2.5f, 0, MonsterKind::Wolf, true},
               {{10, 0, 10}, 130, 130, 1.35f, 0, MonsterKind::Golem, true},
               {{-14, 0, -10}, 130, 130, 1.25f, 0, MonsterKind::Golem, true}};
  for (Monster &monster : monsters_)
    monster.position.y =
        terrain_.SampleHeight(monster.position.x, monster.position.z);
}

bool FrontierWorldGame::Build(BuildingKind kind, Vec3 position, float yaw) {
  ++buildAttempts_;
  const ResourceCost cost = Cost(kind);
  if (!CanAfford(wood_, stone_, kind))
    return false;
  position.x = std::round(position.x / 2.5f) * 2.5f;
  position.z = std::round(position.z / 2.5f) * 2.5f;
  position.y = terrain_.SampleHeight(position.x, position.z) +
               (kind == BuildingKind::Wall   ? .4f
                : kind == BuildingKind::Roof ? 3.4f
                                             : 0.f);
  Vec3 size = kind == BuildingKind::Foundation ? Vec3{5, .4f, 5}
              : kind == BuildingKind::Wall     ? Vec3{5, 3, .35f}
                                               : Vec3{5.8f, .55f, 5.8f};
  if (std::abs(std::sin(yaw)) > .7f)
    std::swap(size.x, size.z);
  const Vec3 center = position + Vec3{0, size.y * .5f, 0};
  if (collisionWorld_.OverlapsBox(center, size * .35f,
                                  {BuildingLayer, ActorLayer, false}))
    return false;
  const ColliderHandle3D collider =
      collisionWorld_.CreateBox({{center, size * .5f},
                                 BuildingLayer,
                                 CollisionLayerAll3D,
                                 buildings_.size() + 1,
                                 false});
  buildings_.push_back({kind, position, yaw, collider});
  wood_ -= cost.wood;
  stone_ -= cost.stone;
  navigationDirty_ = true;
  return true;
}

void FrontierWorldGame::Attack() {
  if (attackCooldown_ > 0)
    return;
  attackCooldown_ = .48f;
  attackAnimationClock_ = .48f;
  ++attacks_;
  for (Monster &monster : monsters_) {
    if (!monster.alive ||
        !CanMeleeHit(controller_.Position() + Vec3{0, 1, 0}, yaw_,
                     monster.position + Vec3{0, .7f, 0}))
      continue;
    monster.health -= 24.f + level_ * 6.f;
    ++hits_;
    if (monster.health <= 0) {
      monster.alive = false;
      ++kills_;
      experience_ += monster.kind == MonsterKind::Golem ? 70 : 35;
      while (experience_ >= ExperienceForLevel(level_)) {
        experience_ -= ExperienceForLevel(level_);
        ++level_;
        health_ = (std::min)(100, health_ + 25);
      }
    }
  }
}

void FrontierWorldGame::RebuildNavigation() {
  navigationGrid_.Rasterize(collisionWorld_, {BuildingLayer, ActorLayer, false},
                            .55f, 1.8f);
  navigationBlockedCells_ =
      navigationGrid_.ApplyHeightField(terrain_, 48, 7, true);
  navigationDirty_ = false;
  for (Monster &monster : monsters_) {
    monster.path.clear();
    monster.pathIndex = 0;
    monster.repathClock = 0;
  }
}

void FrontierWorldGame::UpdateMonsters(float deltaTime) {
  if (navigationDirty_)
    RebuildNavigation();
  navigationPathPoints_ = 0;
  for (Monster &monster : monsters_) {
    if (!monster.alive)
      continue;
    monster.attackCooldown =
        (std::max)(0.f, monster.attackCooldown - deltaTime);
    monster.repathClock = (std::max)(0.f, monster.repathClock - deltaTime);
    const Vec3 delta = controller_.Position() - monster.position;
    const float distance = Length(Vec3{delta.x, 0, delta.z});
    if (distance < 1.55f) {
      if (monster.attackCooldown <= 0) {
        health_ = (std::max)(
            0, health_ - (monster.kind == MonsterKind::Golem ? 14 : 7));
        monster.attackCooldown = 1.1f;
      }
    } else if (distance < 90.f) {
      if (monster.repathClock <= 0 ||
          monster.pathIndex >= monster.path.size()) {
        const NavigationPathStats3D pathStats = navigationGrid_.FindPathInto(
            monster.position, controller_.Position(), navigationWorkspace_,
            monster.path);
        ++navigationSearches_;
        navigationVisited_ += pathStats.visitedNodes;
        monster.pathIndex = monster.path.size() > 1 ? 1 : 0;
        monster.repathClock =
            .4f +
            float((navigationSearches_ + std::size_t(monster.kind)) % 4) * .08f;
      }
      while (
          monster.pathIndex < monster.path.size() &&
          Length(Vec3{monster.path[monster.pathIndex].x - monster.position.x, 0,
                      monster.path[monster.pathIndex].z - monster.position.z}) <
              .55f)
        ++monster.pathIndex;
      Vec3 direction = Normalize(Vec3{delta.x, 0, delta.z});
      if (monster.pathIndex < monster.path.size()) {
        const Vec3 waypoint =
            monster.path[monster.pathIndex] - monster.position;
        direction = Normalize(Vec3{waypoint.x, 0, waypoint.z});
      }
      Vec3 proposed =
          monster.position + direction * (monster.speed * deltaTime);
      proposed.y = terrain_.SampleHeight(proposed.x, proposed.z);
      if (!collisionWorld_.OverlapsBox(proposed + Vec3{0, .65f, 0},
                                       {.45f, .65f, .45f},
                                       {BuildingLayer, ActorLayer, false}))
        monster.position = proposed;
    }
    navigationPathPoints_ += monster.path.size() > monster.pathIndex
                                 ? monster.path.size() - monster.pathIndex
                                 : 0;
  }
  if (health_ <= 0) {
    health_ = 100;
    controller_.Reset({0, terrain_.SampleHeight(0, -8), -8});
  }
}

void FrontierWorldGame::OnUpdate(Engine &engine, float deltaTime) {
  Input &input = engine.GetInput();
  if (input.Pressed(VK_ESCAPE))
    engine.Quit();
  if (input.Pressed(VK_F4))
    automatic_ = !automatic_;
  if (input.Pressed(VK_F5))
    engine.SetRenderQuality(NextRenderQuality3D(engine.RequestedRenderQuality()));
  if (input.Pressed(VK_F1)) {
    viewMode_ = ViewMode::FirstPerson;
    automatic_ = false;
  }
  if (input.Pressed(VK_F2)) {
    viewMode_ = ViewMode::ThirdPerson;
    automatic_ = false;
  }
  if (input.Pressed(VK_F3)) {
    viewMode_ = ViewMode::GodView;
    automatic_ = false;
  }
  if (input.Pressed('1'))
    selectedBuilding_ = BuildingKind::Foundation;
  if (input.Pressed('2'))
    selectedBuilding_ = BuildingKind::Wall;
  if (input.Pressed('3'))
    selectedBuilding_ = BuildingKind::Roof;
  if (!captureDirectory_.empty()) {
    const RendererStats rendererStats = engine.GetRendererStats();
    const EngineMemoryStats memory = engine.GetMemoryStats();
    if (rendererStats.lastGpuFrameNanoseconds) {
      showcaseMinimumGpuFrameNanoseconds_ =
          (std::min)(showcaseMinimumGpuFrameNanoseconds_,
                     rendererStats.lastGpuFrameNanoseconds);
      showcaseMaximumGpuFrameNanoseconds_ =
          (std::max)(showcaseMaximumGpuFrameNanoseconds_,
                     rendererStats.lastGpuFrameNanoseconds);
      if (showcaseGpuSamples_.size() < 4096)
        showcaseGpuSamples_.push_back(rendererStats.lastGpuFrameNanoseconds);
    }
    const uint64_t rendererCpuWorkNanoseconds =
        rendererStats.lastRenderPreparationCpuNanoseconds +
        rendererStats.lastRenderCommandRecordingCpuNanoseconds +
        rendererStats.lastRenderQueueExecuteCpuNanoseconds;
    uint64_t cpuFrameNanoseconds = rendererCpuWorkNanoseconds +
        rendererStats.lastRenderFenceWaitCpuNanoseconds +
        rendererStats.lastRenderPresentCpuNanoseconds;
    uint64_t cpuWorkNanoseconds{};
    if(showcaseThreadCycleCalibrationValid_&&showcaseThreadCyclesPerSecond_&&rendererStats.lastRenderThreadCpuQuerySucceeded&&rendererStats.lastRenderThreadCpuCycles){const long double nanoseconds=long double(rendererStats.lastRenderThreadCpuCycles)*1'000'000'000.0L/long double(showcaseThreadCyclesPerSecond_);if(std::isfinite(double(nanoseconds))&&nanoseconds>0&&nanoseconds<=long double((std::numeric_limits<uint64_t>::max)()))cpuWorkNanoseconds=uint64_t(nanoseconds);else showcaseThreadCycleCalibrationValid_=false;}else showcaseThreadCycleCalibrationValid_=false;
    if (cpuFrameNanoseconds && showcaseCpuSamples_.size() < 4096)
      showcaseCpuSamples_.push_back(cpuFrameNanoseconds);
    if (cpuWorkNanoseconds && showcaseCpuWorkSamples_.size() < 4096)
      showcaseCpuWorkSamples_.push_back(cpuWorkNanoseconds);
    const std::array<uint64_t,5> cpuStages={rendererStats.lastRenderPreparationCpuNanoseconds,rendererStats.lastRenderCommandRecordingCpuNanoseconds,rendererStats.lastRenderQueueExecuteCpuNanoseconds,rendererStats.lastRenderFenceWaitCpuNanoseconds,rendererStats.lastRenderPresentCpuNanoseconds};
    for(std::size_t stage=0;stage<cpuStages.size();++stage)if(cpuStages[stage]&&showcaseCpuStageSamples_[stage].size()<4096)showcaseCpuStageSamples_[stage].push_back(cpuStages[stage]);
    const uint64_t steadyAllocationTotal=rendererStats.performanceTelemetryMutationAllocations+rendererStats.hdrSceneColorAllocations+rendererStats.nativeDepthBufferAllocations+rendererStats.ambientOcclusionAllocations+rendererStats.waterOpaqueSceneColorAllocations+rendererStats.waterOpaqueSceneColorReleases+rendererStats.volumetricAtmosphereResourceCreates+rendererStats.volumetricAtmosphereResourceReleases+rendererStats.temporalHistoryAllocations+rendererStats.temporalHistoryReleases+rendererStats.multisampleAllocations+rendererStats.multisampleReleases+rendererStats.velocityAllocations+rendererStats.velocityReleases+rendererStats.directionalShadowMapAllocations+rendererStats.directionalShadowResourceGeneration+rendererStats.frameUploadBufferAllocations;
    const std::array<uint64_t,16> resourceCounters={rendererStats.performanceTelemetryMutationAllocations,rendererStats.hdrSceneColorAllocations,rendererStats.nativeDepthBufferAllocations,rendererStats.ambientOcclusionAllocations,rendererStats.waterOpaqueSceneColorAllocations,rendererStats.waterOpaqueSceneColorReleases,rendererStats.volumetricAtmosphereResourceCreates,rendererStats.volumetricAtmosphereResourceReleases,rendererStats.temporalHistoryAllocations,rendererStats.temporalHistoryReleases,rendererStats.multisampleAllocations,rendererStats.multisampleReleases,rendererStats.velocityAllocations,rendererStats.velocityReleases,rendererStats.directionalShadowMapAllocations,rendererStats.directionalShadowResourceGeneration};
    const bool ultraApplied=engine.DevelopmentUltraReceipt().rendererApplied;if(ultraApplied&&!showcaseStartupResourceCounterEnd_)showcaseStartupResourceCounterEnd_=steadyAllocationTotal;if(captureIndex_>=12&&!showcaseTransitionResourceCounterStart_)showcaseTransitionResourceCounterStart_=steadyAllocationTotal;if(ultraApplied&&captureIndex_<12){const std::size_t phase=captureIndex_;const bool changed=showcasePreviousSteadyAllocationTotal_&&steadyAllocationTotal!=showcasePreviousSteadyAllocationTotal_;const uint64_t phaseFrame=++showcasePhaseAllocationFrames_[phase];if(changed){showcasePhaseSteadyCoverage_[phase]=0;++showcaseWarmupResourceChanges_;}else if(phaseFrame>15){++showcasePhaseSteadyCoverage_[phase];++showcaseSteadyAllocationFrames_;}if(phaseFrame>15&&rendererStats.pendingGpuReleaseBytes)++showcaseSteadyPendingViolations_;}showcaseSteadyAllocationEstablished_=std::ranges::all_of(showcasePhaseSteadyCoverage_,[](uint64_t coverage){return coverage>=8;});showcasePreviousResourceCounters_=resourceCounters;showcasePreviousSteadyAllocationTotal_=steadyAllocationTotal;
    showcaseMaximumWorkingSetBytes_ = (std::max)(
        showcaseMaximumWorkingSetBytes_, memory.process.workingSetBytes);
    const std::size_t trackedVram =
        rendererStats.residentTextureAllocationBytes +
        rendererStats.residentMeshBytes +
        rendererStats.residentSkinnedMeshBytes +
        rendererStats.residentEnvironmentLightingBytes +
        rendererStats.screenSpaceReflectionResidentBytes +
        rendererStats.bloomResidentBytes +
        rendererStats.volumetricCloudResidentBytes +
        rendererStats.volumetricCloudShadowResidentBytes +
        rendererStats.directionalLightShaftResidentBytes +
        rendererStats.directionalShadowMapBytes +
        rendererStats.ambientOcclusionBytes +
        rendererStats.waterOpaqueSceneColorBytes;
    showcaseMaximumTrackedVramBytes_ =
        (std::max)(showcaseMaximumTrackedVramBytes_, trackedVram);
    if (capturePending_ &&
        rendererStats.successfulFrameCaptures > captureSuccessfulBaseline_) {
      const auto counters = ShowcaseFeatureCounters(rendererStats);
      ShowcaseSlotReceipt &slot = showcaseSlots_[captureIndex_];
      for (std::size_t counter = 0; counter < counters.size(); ++counter)
        slot.counterDelta[counter] =
            counters[counter] >= slot.counterBaseline[counter]
                ? counters[counter] - slot.counterBaseline[counter]
                : 0;
      capturePending_ = false;
      ++captureIndex_;
      capturePresentationFrames_ = 0;
    } else if (capturePending_ &&
               rendererStats.rejectedFrameCaptures > captureRejectedBaseline_) {
      capturePending_ = false;
      std::error_code error;
      std::filesystem::remove(
          captureDirectory_ / ShowcaseCaptureNames[captureIndex_], error);
      capturePresentationFrames_ = 0;
    }
    if (captureIndex_ == ShowcaseCaptureNames.size()) {
      bool capturesReady = true;
      for (const wchar_t *name : ShowcaseCaptureNames)
        capturesReady &=
            std::filesystem::is_regular_file(captureDirectory_ / name);
      if (capturesReady && !showcaseReceiptWritten_) {
        engine.WaitRendererIdle3D();
        WriteShowcaseReceipt(engine);
        showcaseReceiptWritten_ = true;
        engine.Quit();
        return;
      }
    } else {
      automatic_ = false;
      if (appliedCaptureIndex_ != captureIndex_) {
        lightingProfile_ = captureIndex_ < 4    ? LightingProfile::Day
                           : captureIndex_ < 8  ? LightingProfile::Dusk
                           : captureIndex_ < 12 ? LightingProfile::Night
                                                : LightingProfile::Day;
        const std::size_t view = captureIndex_ < 12 ? captureIndex_ % 4 : 1;
        riverView_ = view == 3;
        viewMode_ = view == 0   ? ViewMode::FirstPerson
                    : view == 2 ? ViewMode::GodView
                                : ViewMode::ThirdPerson;
        if (riverView_) {
          const float riverZ = 0.f,
                      riverX = RiverCenter(riverZ) - RiverWidth(riverZ) - 10.f;
          controller_.Reset(
              {riverX, terrain_.SampleHeight(riverX, riverZ), riverZ});
          yaw_ = -1.5707963f;
        } else {
          controller_.Reset({0, terrain_.SampleHeight(0, -8), -8});
          yaw_ = 0;
        }
        appliedCaptureIndex_ = captureIndex_;
        capturePresentationFrames_ = 0;
      }
      const bool featureEnabled = captureIndex_ != 12;
      DrawList &profile = engine.Draw();
      profile.SetAmbientOcclusion({featureEnabled, 1.5f, .03f, 1.15f});
      profile.SetScreenSpaceReflections(
          {featureEnabled, .82f, 42.f, .7f, .32f, 16});
      profile.SetBloom({featureEnabled, 1.05f, .55f, .13f, 3.f});
      profile.SetVolumetricClouds({featureEnabled, 1100.f, 1700.f, .48f, 1.08f,
                                  .28f, 14.f, 16, featureEnabled, .7f});
      profile.SetDirectionalLightShafts({featureEnabled, .68f, .72f, 360.f, 6});
      profile.SetSoftDirectionalShadows({featureEnabled, 24.f, 128.f, 36.f});
      profile.SetContactShadows({featureEnabled, 16, 1.8f, .045f, .85f});
      profile.SetWaterOptics({featureEnabled, .022f, .42f, .04f});
      profile.SetDirectionalShadowMapEnabled(featureEnabled);
      if (!capturePending_ && capturePresentationFrames_ >= 45) {
        captureSuccessfulBaseline_ = rendererStats.successfulFrameCaptures;
        captureRejectedBaseline_ = rendererStats.rejectedFrameCaptures;
        ShowcaseSlotReceipt &slot = showcaseSlots_[captureIndex_];
        slot.profile = lightingProfile_;
        slot.view = viewMode_;
        slot.featureRequested = featureEnabled;
        slot.featureNormalized = profile.AmbientOcclusion().enabled &&
                                 profile.ScreenSpaceReflections().enabled &&
                                 profile.Bloom().enabled &&
                                 profile.VolumetricClouds().enabled &&
                                 profile.DirectionalLightShafts().enabled &&
                                 profile.ContactShadows().enabled &&
                                 profile.WaterOptics().screenSpaceRefraction;
        slot.softShadowNormalized = profile.SoftDirectionalShadows().enabled;
        slot.worldStateHash = ShowcaseWorldStateHash();
        slot.counterBaseline = ShowcaseFeatureCounters(rendererStats);
        // Cascade與soft sample是per-frame值；以零為baseline保存capture frame實際值。
        slot.counterBaseline[0] = 0;
        slot.counterBaseline[1] = 0;
        capturePending_ = engine.CaptureNextFrame3D(
            captureDirectory_ / ShowcaseCaptureNames[captureIndex_]);
      }
    }
    // Capture模式完全凍結simulation，確保各slot與A/B只受profile設定影響。
    return;
  }
  attackCooldown_ = (std::max)(0.f, attackCooldown_ - deltaTime);
  attackAnimationClock_ = (std::max)(0.f, attackAnimationClock_ - deltaTime);
  automaticClock_ += deltaTime;
  CharacterControl3D control{};
  bool attack = false, build = false;
  if (automatic_) {
    viewMode_ = static_cast<ViewMode>(int(automaticClock_ / 6.f) % 3);
    const int phase = int(automaticClock_ / 12.f) % 4;
    const Vec3 directions[4] = {{0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {-1, 0, 0}};
    control.movement = directions[phase];
    yaw_ = std::atan2(control.movement.x, control.movement.z);
    attack = true;
    static constexpr std::array<Vec3, 6> positions = {
        Vec3{6, 0, 6},      Vec3{6, .4f, 3.5f}, Vec3{6, .4f, 8.5f},
        Vec3{3.5f, .4f, 6}, Vec3{8.5f, .4f, 6}, Vec3{6, 3.4f, 6}};
    const std::size_t next = buildings_.size();
    if (next < positions.size() && automaticClock_ > 1.f + float(next) * 1.2f) {
      const BuildingKind kind = next == 0   ? BuildingKind::Foundation
                                : next == 5 ? BuildingKind::Roof
                                            : BuildingKind::Wall;
      const float rotation = next == 3 || next == 4 ? 1.5707963f : 0;
      build = Build(kind, positions[next], rotation);
    }
  } else {
    yaw_ += (float(input.Down(VK_RIGHT)) - float(input.Down(VK_LEFT)) -
             input.MouseDelta().x * .02f) *
            deltaTime * 2.4f;
    pitch_ =
        Clamp(pitch_ + (float(input.Down(VK_UP)) - float(input.Down(VK_DOWN))) *
                           deltaTime * 1.5f,
              -.35f, .7f);
    const Vec3 forward{std::sin(yaw_), 0, std::cos(yaw_)},
        right{std::cos(yaw_), 0, -std::sin(yaw_)};
    control.movement =
        forward * (float(input.Down('W')) - float(input.Down('S'))) +
        right * (float(input.Down('D')) - float(input.Down('A')));
    control.jump = input.Pressed(VK_RETURN);
    attack = input.MousePressed() || input.Pressed('F');
    if (input.Pressed('B'))
      build = Build(selectedBuilding_, controller_.Position() + forward * 5.f,
                    selectedBuilding_ == BuildingKind::Wall ? yaw_ : 0);
  }
  controller_.Step(control, collisionWorld_, terrain_, deltaTime, BuildingLayer,
                   ActorLayer);
  worldStreamingStats_ = worldStreamer_.Update(controller_.Position());
  if (attack) {
    const uint64_t before = attacks_;
    Attack();
    if (attacks_ > before)
      engine.Audio().Play(swingSound_);
  }
  if (build)
    engine.Audio().Play(buildSound_);
  moving_ = Length(control.movement) > .1f;
  playerAnimation_.Play(attackAnimationClock_ > 0 ? attackClip_
                        : moving_                 ? walkClip_
                                                  : idleClip_,
                        .12f);
  playerAnimation_.Update(deltaTime);
  UpdateMonsters(deltaTime);
  const wchar_t *viewName = viewMode_ == ViewMode::FirstPerson ? L"FIRST PERSON"
                            : viewMode_ == ViewMode::ThirdPerson
                                ? L"THIRD PERSON"
                                : L"GOD VIEW";
  const auto physics = collisionWorld_.Stats();
  engine.SetTitle(std::format(
      L"Aevrynd Frontier World | {} | CC0 PBR {}/{} | HP {} LV {} XP {} | "
      L"BUILD {} MON {} KILL {} | SWING {}/{} | NAV S{} V{} P{} B{} | CH {} "
      L"Q{} L{}/U{} | PHYS Q{} | LOD F{} S{} | SH {}",
      viewName, materialLibraryStats_.materials,
      materialLibraryStats_.uniqueTextureSets, health_, level_, experience_,
      buildings_.size(),
      std::count_if(monsters_.begin(), monsters_.end(),
                    [](const Monster &value) { return value.alive; }),
      kills_, hits_, attacks_, navigationSearches_, navigationVisited_,
      navigationPathPoints_, navigationBlockedCells_,
      worldStreamingStats_.residentChunks, worldStreamingStats_.queuedChunks,
      worldStreamingStats_.totalLoaded, worldStreamingStats_.totalUnloaded,
      physics.queries, sceneStats_.farLodInstances,
      sceneStats_.lodTrianglesSaved, sceneStats_.shadowInstances));
}

void FrontierWorldGame::AddHumanoid(Vec3 position, float yaw, bool armed) {
  camera_ = ConstrainThirdPersonCamera(camera_, position + Vec3{0, 1.3f, 0},
                                       terrain_, .25f);
  const Matrix4 root =
      CreateTranslationMatrix(position) * CreateRotationMatrix({0, yaw, 0});
  playerAnimation_.EvaluateInto(humanoidSkeleton_, playerPose_, root);
  const AnimationPose3D &pose = playerPose_;
  scene_.Mesh(boxMesh_,
              {JointPosition(pose, 0),
               pose.localTransforms[0].rotation + Vec3{0, yaw, 0},
               {.7f, 1.05f, .38f}},
              {{.16f, .42f, .58f, 1}, {.68f, .02f, 0, .94f}});
  scene_.Mesh(sphereHighMesh_,
              {JointPosition(pose, 1), {0, yaw, 0}, {.52f, .55f, .52f}},
              {{.72f, .53f, .38f, 1}, SkinSurface});
  for (std::size_t joint : {std::size_t(2), std::size_t(3)})
    scene_.Mesh(boxMesh_,
                {JointPosition(pose, joint, {0, -.3f, 0}),
                 pose.localTransforms[joint].rotation + Vec3{0, yaw, 0},
                 {.2f, .65f, .22f}},
                {{.72f, .53f, .38f, 1}, SkinSurface});
  for (std::size_t joint : {std::size_t(4), std::size_t(5)})
    scene_.Mesh(boxMesh_,
                {JointPosition(pose, joint, {0, -.32f, 0}),
                 pose.localTransforms[joint].rotation + Vec3{0, yaw, 0},
                 {.22f, .72f, .25f}},
                {{.1f, .16f, .2f, 1}, {.74f, .04f, 0, .9f}});
  if (armed)
    scene_.Mesh(boxMesh_,
                {JointPosition(pose, 6, {0, 0, .55f}),
                 pose.localTransforms[3].rotation + Vec3{.18f, yaw, 0},
                 {.08f, .1f, 1.5f}},
                {{.72f, .78f, .8f, 1}, MetalSurface});
}

void FrontierWorldGame::AddMonster(const Monster &monster) {
  if (!monster.alive)
    return;
  const Vec3 pursuit = controller_.Position() - monster.position;
  const float monsterYaw = std::atan2(pursuit.x, pursuit.z);
  if (monster.kind == MonsterKind::Wolf) {
    const Matrix4 root = CreateTranslationMatrix(monster.position) *
                         CreateRotationMatrix({0, monsterYaw, 0});
    SampleAnimationClipInto(creatureSkeleton_, creatureWalkClip_,
                            automaticClock_ + monster.position.x * .17f,
                            creaturePose_, root);
    const AnimationPose3D &pose = creaturePose_;
    scene_.Mesh(
        boxMesh_,
        {JointPosition(pose, 0), {0, monsterYaw, 0}, {1.15f, .62f, .48f}},
        {{.25f, .27f, .3f, 1}, {.92f, 0, 0, .78f}});
    scene_.Mesh(sphereHighMesh_,
                {JointPosition(pose, 1),
                 pose.localTransforms[1].rotation + Vec3{0, monsterYaw, 0},
                 {.62f, .54f, .62f}},
                {{.31f, .32f, .34f, 1}, {.88f, 0, 0, .82f}});
    for (std::size_t joint = 2; joint < 6; ++joint)
      scene_.Mesh(
          boxMesh_,
          {JointPosition(pose, joint, {0, -.23f, 0}),
           pose.localTransforms[joint].rotation + Vec3{0, monsterYaw, 0},
           {.16f, .5f, .16f}},
          {{.16f, .16f, .18f, 1}, {.9f, 0, 0, .76f}});
  } else {
    const float stride =
        std::sin((automaticClock_ + monster.position.x * .11f) * 4.f) * .28f;
    scene_.Mesh(boxMesh_,
                {monster.position + Vec3{0, 1.15f + std::abs(stride) * .08f, 0},
                 {0, monsterYaw, 0},
                 {1.25f, 1.65f, .8f}},
                {{.34f, .38f, .4f, 1}, StoneSurface});
    scene_.Mesh(sphereHighMesh_,
                {monster.position + Vec3{0, 2.2f, 0},
                 {0, monsterYaw, 0},
                 {.82f, .72f, .78f}},
                {{.42f, .46f, .45f, 1}, StoneSurface});
    scene_.Mesh(boxMesh_,
                {monster.position + Vec3{-.85f, 1.2f, 0},
                 {stride, monsterYaw, 0},
                 {.4f, 1.35f, .42f}},
                {{.29f, .32f, .34f, 1}, StoneSurface});
    scene_.Mesh(boxMesh_,
                {monster.position + Vec3{.85f, 1.2f, 0},
                 {-stride, monsterYaw, 0},
                 {.4f, 1.35f, .42f}},
                {{.29f, .32f, .34f, 1}, StoneSurface});
  }
  scene_.Box(monster.position + Vec3{0, 2.8f, 0}, {1.8f, .12f, .12f},
             {.12f, .03f, .02f, 1});
  scene_.Box(
      monster.position +
          Vec3{-.9f + 1.8f * (monster.health / monster.maximumHealth) * .5f,
               2.8f, -.01f},
      {1.8f * (monster.health / monster.maximumHealth), .13f, .13f}, Coral);
}

void FrontierWorldGame::AddEnvironment() {
  scene_.Mesh(skyDomeMesh_, {controller_.Position(), {}, {360, 360, 360}},
              {{1, 1, 1, 1}, {1, 0, 1, 1}, false});
  scene_.Mesh(terrainMesh_, {},
              MaterialVariant(groundMaterial_, {1, 1, 1, 1}, .085f, false));
  scene_.Mesh(riverMesh_, {},
              WaterMaterial({.025f, .19f, .27f, .86f}, {.07f, .02f, .04f, 1}));
  const float chunkSize = worldStreamer_.Settings().chunkSize,
              half = chunkSize * .5f;
  for (const WorldChunk3D &chunk : worldStreamer_.ResidentChunks()) {
    uint32_t random = chunk.seed;
    for (int index = 0; index < 8; ++index) {
      const float x = chunk.center.x + RandomRange(random, -half + 2, half - 2),
                  z = chunk.center.z + RandomRange(random, -half + 2, half - 2),
                  base = terrain_.SampleHeight(x, z),
                  height = RandomRange(random, 4.f, 6.8f);
      const bool nearWater =
          std::abs(x - RiverCenter(z)) < RiverWidth(z) + 4.f ||
          Length(Vec2{x - 40.f, z - 72.f}) < 29.f;
      if (nearWater || Length(Vec2{x, z + 2.f}) < 24.f || base < -1.f)
        continue;
      const float treeYaw = RandomRange(random, 0, 6.28f),
                  treeScale = height / 1.42f;
      scene_.MeshLod(
          quiverTreeMesh_, treeLowMesh_, WorldLod(52),
          {{x, base, z}, {0, treeYaw, 0}, {treeScale, treeScale, treeScale}},
          quiverTreeMaterial_, FoliageMaterial({1, 1, 1, 1}));
    }
    for (int index = 0; index < 44; ++index) {
      const float x = chunk.center.x + RandomRange(random, -half + 1, half - 1),
                  z = chunk.center.z + RandomRange(random, -half + 1, half - 1),
                  base = terrain_.SampleHeight(x, z);
      if (base < -1.f || std::abs(x - RiverCenter(z)) < RiverWidth(z) + 1.2f)
        continue;
      const float scale = RandomRange(random, .68f, 1.24f),
                  yaw = RandomRange(random, 0, 6.28f);
      scene_.MeshLod(
          grassHighMesh_, grassLowMesh_, WorldLod(24),
          {{x, base + .015f, z}, {0, yaw, 0}, {scale, scale, scale}},
          FoliageMaterial({.07f + RandomRange(random, 0, .035f),
                           .24f + RandomRange(random, 0, .08f), .035f, 1},
                          false));
    }
    for (int index = 0; index < 3; ++index) {
      const float x = chunk.center.x + RandomRange(random, -half + 1, half - 1),
                  z = chunk.center.z + RandomRange(random, -half + 1, half - 1),
                  scale = RandomRange(random, .8f, 1.8f),
                  base = terrain_.SampleHeight(x, z);
      if (base < -1.5f)
        continue;
      const Transform3D rockTransform{
          {x, base + .02f, z},
          {RandomRange(random, -.15f, .15f), RandomRange(random, 0, 6.28f), 0},
          {scale, scale, scale}};
      scene_.MeshLod(
          rock09Mesh_, sphereLowMesh_, WorldLod(28), rockTransform,
          rock09Material_,
          MaterialVariant(rockMaterial_, {.78f, .76f, .72f, 1}, .7f));
    }
  }
  if (worldStreamer_.Contains(ChunkAt({40, 0, 72}, chunkSize)))
    scene_.Mesh(cylinderMesh_, {{40, -2.09f, 72}, {0, 0, 0}, {46, .12f, 34}},
                WaterMaterial({.03f, .22f, .31f, .84f}));
  auto addCabin = [&](Vec3 center, float yaw) {
    if (!worldStreamer_.Contains(ChunkAt(center, chunkSize)))
      return;
    const float base = terrain_.SampleHeight(center.x, center.z);
    center.y = base;
    scene_.Mesh(boxMesh_,
                {center + Vec3{0, .28f, 0}, {0, yaw, 0}, {7.6f, .56f, 6.4f}},
                MaterialVariant(rockMaterial_, {.96f, .95f, .92f, 1}, .35f));
    for (float side : {-1.f, 1.f})
      scene_.Mesh(boxMesh_,
                  {center + Vec3{side * 3.45f, 2.05f, 0},
                   {0, yaw, 0},
                   {.35f, 3.55f, 6.f}},
                  MaterialVariant(woodMaterial_, {.98f, .94f, .86f, 1}, .48f));
    for (float side : {-1.f, 1.f})
      scene_.Mesh(boxMesh_,
                  {center + Vec3{0, 2.05f, side * 2.8f},
                   {0, yaw, 0},
                   {7.2f, 3.55f, .35f}},
                  MaterialVariant(woodMaterial_, {.96f, .91f, .82f, 1}, .48f));
    for (float beam : {-2.8f, 0.f, 2.8f})
      scene_.Mesh(cylinderMesh_,
                  {center + Vec3{beam, 2.05f, 2.98f},
                   {0, 0, 1.57f},
                   {.34f, 3.6f, .34f}},
                  MaterialVariant(barkMaterial_, {.92f, .84f, .74f, 1}, 1.f));
    scene_.Mesh(
        boxMesh_,
        {center + Vec3{-1.8f, 1.6f, 3.02f}, {0, yaw, 0}, {1.1f, 2.45f, .18f}},
        MaterialVariant(woodMaterial_, {.88f, .82f, .74f, 1}, .6f));
    scene_.Mesh(
        boxMesh_,
        {center + Vec3{1.7f, 1.9f, 3.03f}, {0, yaw, 0}, {1.25f, 1.2f, .16f}},
        {{.22f, .58f, .72f, 1}, {.2f, .02f, .12f, 1}, false});
    scene_.Mesh(boxMesh_,
                {center + Vec3{0, 4.15f, 0}, {0, yaw, .5f}, {8.4f, .34f, 3.8f}},
                MaterialVariant(woodMaterial_, {.9f, .82f, .74f, 1}, .42f));
    scene_.Mesh(
        boxMesh_,
        {center + Vec3{0, 4.15f, 0}, {0, yaw, -.5f}, {8.4f, .34f, 3.8f}},
        MaterialVariant(woodMaterial_, {.94f, .86f, .76f, 1}, .42f));
    scene_.Mesh(
        boxMesh_,
        {center + Vec3{2.3f, 5.f, -1.2f}, {0, yaw, 0}, {.75f, 2.4f, .75f}},
        MaterialVariant(rockMaterial_, {.94f, .93f, .9f, 1}, .5f));
  };
  addCabin({-12, 0, 17}, .12f);
  addCabin({-38, 0, -18}, -.34f);
  addCabin({14, 0, 34}, .42f);
  for (const Building &building : buildings_) {
    if (!worldStreamer_.Contains(ChunkAt(building.position, chunkSize)))
      continue;
    if (building.kind == BuildingKind::Foundation)
      scene_.Mesh(boxMesh_,
                  {building.position + Vec3{0, .2f, 0},
                   {0, building.yaw, 0},
                   {5, .4f, 5}},
                  MaterialVariant(rockMaterial_, {.76f, .74f, .7f, 1}, .38f));
    else if (building.kind == BuildingKind::Wall)
      scene_.Mesh(boxMesh_,
                  {building.position + Vec3{0, 1.5f, 0},
                   {0, building.yaw, 0},
                   {5, 3, .35f}},
                  MaterialVariant(woodMaterial_, {.75f, .63f, .48f, 1}, .5f));
    else {
      scene_.Mesh(boxMesh_,
                  {building.position + Vec3{0, .15f, 0},
                   {0, 0, .43f},
                   {5.8f, .35f, 3.2f}},
                  MaterialVariant(woodMaterial_, {.58f, .45f, .34f, 1}, .42f));
      scene_.Mesh(boxMesh_,
                  {building.position + Vec3{0, .15f, 0},
                   {0, 0, -.43f},
                   {5.8f, .35f, 3.2f}},
                  MaterialVariant(woodMaterial_, {.62f, .48f, .36f, 1}, .42f));
    }
  }
  if (viewMode_ != ViewMode::GodView) {
    const Vec3 observer = controller_.Position();
    for (int cloud = 0; cloud < 5; ++cloud) {
      const float angle = float(cloud) * 1.256f + automaticClock_ * .003f;
      const Vec3 center =
          observer + Vec3{std::sin(angle) * 85.f, 42.f + float(cloud % 2) * 8.f,
                          std::cos(angle) * 85.f};
      for (int puff = 0; puff < 4; ++puff)
        scene_.MeshLod(
            sphereHighMesh_, sphereLowMesh_, WorldLod(90),
            {center + Vec3{float(puff - 1) * 5.f, float(puff % 2) * 1.8f,
                           float((puff * 3) % 5) - 2.f},
             {0, angle, 0},
             {12.f, 4.2f, 7.f}},
            {{.84f, .88f, .89f, .68f}, {.94f, 0, .2f, 1}, false});
    }
  }
}

uint64_t FrontierWorldGame::ShowcaseWorldStateHash() const {
  uint64_t hash = 1469598103934665603ull;
  const auto append = [&hash](uint64_t value) {
    for (unsigned byte = 0; byte < 8; ++byte) {
      hash ^= (value >> (byte * 8)) & 0xffu;
      hash *= 1099511628211ull;
    }
  };
  const auto appendFloat = [&append](float value) {
    append(std::bit_cast<uint32_t>(value));
  };
  const auto appendVec3 = [&appendFloat](Vec3 value) {
    appendFloat(value.x);
    appendFloat(value.y);
    appendFloat(value.z);
  };
  appendVec3(controller_.Position());
  appendVec3(camera_.position);
  appendVec3(camera_.target);
  appendFloat(yaw_);
  appendFloat(pitch_);
  appendFloat(automaticClock_);
  append(uint64_t(viewMode_));
  append(uint64_t(lightingProfile_));
  append(worldStreamingStats_.residentChunks);
  append(worldStreamingStats_.totalLoaded);
  append(worldStreamingStats_.totalUnloaded);
  append(buildings_.size());
  for (const Building &building : buildings_) {
    append(uint64_t(building.kind));
    appendVec3(building.position);
    appendFloat(building.yaw);
  }
  append(monsters_.size());
  for (const Monster &monster : monsters_) {
    append(uint64_t(monster.kind));
    appendVec3(monster.position);
    appendFloat(monster.health);
    append(monster.alive ? 1 : 0);
  }
  return hash;
}

void FrontierWorldGame::WriteShowcaseReceipt(Engine &engine) {
  const auto percentile95 = [](std::vector<uint64_t> values) {
    if (values.empty())
      return uint64_t{};
    std::sort(values.begin(), values.end());
    return values[(values.size() - 1) * 95 / 100];
  };
  const uint64_t gpuP95 = percentile95(showcaseGpuSamples_);
  const ShowcaseCpuPerformanceReceipt cpuPerformance=EvaluateShowcaseCpuPerformance(showcaseCpuWorkSamples_);
  const ShowcaseCpuPerformanceReceipt endToEndPerformance=EvaluateShowcaseCpuPerformance(showcaseCpuSamples_,ShowcaseCpuCatastrophicNanoseconds,ShowcaseCpuCatastrophicNanoseconds);
  std::array<ShowcaseCpuPerformanceReceipt,5> cpuStagePerformance{};for(std::size_t stage=0;stage<cpuStagePerformance.size();++stage)cpuStagePerformance[stage]=EvaluateShowcaseCpuPerformance(showcaseCpuStageSamples_[stage],ShowcaseCpuCatastrophicNanoseconds,ShowcaseCpuCatastrophicNanoseconds);
  constexpr std::array<uint64_t,7> ValidCpuContract={1'000'000,1'000'000,1'000'000,1'000'000,1'000'000,1'000'000,1'000'000};
  constexpr std::array<uint64_t,1> SingleWindowMutation={1'000'000};
  constexpr std::array<uint64_t,7> ZeroMutation={1'000'000,1'000'000,1'000'000,0,1'000'000,1'000'000,1'000'000};
  constexpr std::array<uint64_t,7> OverflowMutation={1'000'000,1'000'000,1'000'000,(std::numeric_limits<uint64_t>::max)(),1'000'000,1'000'000,1'000'000};
  const bool cpuHostileContractPassed=EvaluateShowcaseCpuPerformance(ValidCpuContract).passed&&!EvaluateShowcaseCpuPerformance(SingleWindowMutation).passed&&!EvaluateShowcaseCpuPerformance(ZeroMutation).passed&&!EvaluateShowcaseCpuPerformance(OverflowMutation).passed;
  const uint64_t cpuP95 = percentile95(showcaseCpuSamples_);
  const RendererStats before = engine.GetRendererStats();
  const EngineMemoryStats memoryBefore = engine.GetMemoryStats();
  const std::size_t trimReleased = engine.TrimUnusedMemory();
  engine.WaitRendererIdle3D();
  const RendererStats after = engine.GetRendererStats();
  const EngineMemoryStats memoryAfter = engine.GetMemoryStats();
  const bool featuresExecuted =
      before.lastDirectionalShadowCascades == 4 &&
      before.contactShadowEvaluationPasses > 0 &&
      before.softDirectionalShadowsEnabled &&
      before.ambientOcclusionEvaluationPasses > 0 &&
      before.screenSpaceReflectionPasses > 0 && before.bloomPasses > 0 &&
      before.volumetricCloudConfiguredFrames > 0 &&
      before.volumetricCloudShadowConfiguredFrames > 0 &&
      before.directionalLightShaftConfiguredFrames > 0 &&
      before.screenSpaceWaterRefractionPasses > 0;
  const bool memoryGate = showcaseMaximumWorkingSetBytes_ <= 1536ull * 1024 * 1024 &&
                          showcaseMaximumTrackedVramBytes_ <= 768ull * 1024 * 1024 &&
                          before.materialTextureBudgetRejections == 0 &&
                          before.pendingGpuReleaseBytes == 0 &&
                          after.pendingGpuReleaseBytes == 0 &&
                          showcaseSteadyAllocationChanges_ == 0 &&
                          showcaseSteadyAllocationEstablished_ &&
                          showcaseSteadyPendingViolations_ == 0 &&
                          showcaseSteadyAllocationFrames_ >= 120;
  const bool gpuGate = gpuP95 > 0 && gpuP95 <= 12000000ull;
  const bool cpuGate = showcaseThreadCycleCalibrationValid_&&cpuPerformance.passed&&endToEndPerformance.passed&&cpuHostileContractPassed;
  // Receipt只記錄renderer實際執行計數，不以requested設定冒充功能通過。
  std::ofstream receipt(captureDirectory_ / "frontier-showcase-receipt.json",
                        std::ios::binary | std::ios::trunc);
  receipt << "{\n"
          << "  \"source\": \"FrontierWorldHighShowcase\",\n"
          << "  \"entry\": \"--capture-demo\",\n"
          << "  \"requestedRenderQuality\": \"" << RenderQualityName3D(engine.RequestedRenderQuality()) << "\",\n"
          << "  \"actualRenderQuality\": \"" << RenderQualityName3D(engine.RenderQuality()) << "\",\n"
          << "  \"developmentUltraRendererApplied\": " << (engine.DevelopmentUltraReceipt().rendererApplied?1:0) << ",\n"
          << "  \"developmentUltraTransitionApplied\": " << (engine.DevelopmentUltraReceipt().transitionPolicyApplied?1:0) << ",\n"
          << "  \"runId\": \"" << showcaseRunId_ << "\",\n"
          << "  \"worldCaptures\": 12,\n"
          << "  \"diagnosticCaptures\": 2,\n"
          << "  \"profiles\": [\"day\", \"dusk\", \"night\"],\n"
          << "  \"slots\": [\n";
  constexpr std::array<const char *, 10> CounterNames = {
      "cascades", "softSamples", "contact", "ao", "ssr",
      "bloom",    "cloud",       "cloudShadow", "shafts", "water"};
  for (std::size_t index = 0; index < showcaseSlots_.size(); ++index) {
    const ShowcaseSlotReceipt &slot = showcaseSlots_[index];
    const char *profile = slot.profile == LightingProfile::Day
                              ? "day"
                          : slot.profile == LightingProfile::Dusk ? "dusk"
                                                                  : "night";
    const char *view = slot.view == ViewMode::FirstPerson
                           ? "first"
                       : slot.view == ViewMode::ThirdPerson ? "third"
                                                           : "god";
    receipt << "    {\"index\":" << index << ",\"name\":\""
            << std::filesystem::path(ShowcaseCaptureNames[index]).string()
            << "\",\"profile\":\"" << profile << "\",\"view\":\"" << view
            << "\",\"featureRequested\":" << (slot.featureRequested ? 1 : 0)
            << ",\"featureNormalized\":" << (slot.featureNormalized ? 1 : 0)
            << ",\"softShadowNormalized\":"
            << (slot.softShadowNormalized ? 1 : 0)
            << ",\"worldStateHash\":\"" << std::hex << slot.worldStateHash
            << std::dec << "\",\"counters\":{";
    for (std::size_t counter = 0; counter < CounterNames.size(); ++counter) {
      if (counter)
        receipt << ',';
      receipt << '\"' << CounterNames[counter] << "\":{\"baseline\":"
              << slot.counterBaseline[counter] << ",\"delta\":"
              << slot.counterDelta[counter] << '}';
    }
    receipt << "}}" << (index + 1 == showcaseSlots_.size() ? "\n" : ",\n");
  }
  receipt << "  ],\n"
          << "  \"directionalShadowCascades\": "
          << before.lastDirectionalShadowCascades << ",\n"
          << "  \"contactShadowPasses\": "
          << before.contactShadowEvaluationPasses << ",\n"
          << "  \"softShadowsEnabled\": "
          << (before.softDirectionalShadowsEnabled ? 1 : 0) << ",\n"
          << "  \"ambientOcclusionPasses\": "
          << before.ambientOcclusionEvaluationPasses << ",\n"
          << "  \"screenSpaceReflectionPasses\": "
          << before.screenSpaceReflectionPasses << ",\n"
          << "  \"bloomPasses\": " << before.bloomPasses << ",\n"
          << "  \"volumetricCloudFrames\": "
          << before.volumetricCloudConfiguredFrames << ",\n"
          << "  \"cloudShadowFrames\": "
          << before.volumetricCloudShadowConfiguredFrames << ",\n"
          << "  \"lightShaftFrames\": "
          << before.directionalLightShaftConfiguredFrames << ",\n"
          << "  \"waterRefractionPasses\": "
          << before.screenSpaceWaterRefractionPasses << ",\n"
          << "  \"residentTextureBytes\": "
          << before.residentTextureAllocationBytes << ",\n"
          << "  \"residentMeshBytes\": " << before.residentMeshBytes
          << ",\n"
          << "  \"maximumTrackedVramBytes\": "
          << showcaseMaximumTrackedVramBytes_ << ",\n"
          << "  \"maximumWorkingSetBytes\": "
          << showcaseMaximumWorkingSetBytes_ << ",\n"
          << "  \"workingSetBeforeTrimBytes\": "
          << memoryBefore.process.workingSetBytes << ",\n"
          << "  \"workingSetAfterTrimBytes\": "
          << memoryAfter.process.workingSetBytes << ",\n"
          << "  \"trimReleasedBytes\": " << trimReleased << ",\n"
          << "  \"pendingGpuReleaseBytes\": "
          << after.pendingGpuReleaseBytes << ",\n"
          << "  \"minimumGpuFrameNanoseconds\": "
          << (showcaseMinimumGpuFrameNanoseconds_ == ~uint64_t{}
                  ? 0
                  : showcaseMinimumGpuFrameNanoseconds_)
          << ",\n"
          << "  \"maximumGpuFrameNanoseconds\": "
          << showcaseMaximumGpuFrameNanoseconds_ << ",\n"
          << "  \"gpuP95Nanoseconds\": " << gpuP95 << ",\n"
          << "  \"drawCalls\": " << before.lastDrawCalls << ",\n"
          << "  \"meshInstances\": " << before.lastSubmittedMeshInstances << ",\n"
          << "  \"worldVertices\": " << before.lastSubmittedWorldVertices << ",\n"
          << "  \"hzbPrepassRecordingCpuNanoseconds\": " << before.lastHzbPrepassRecordingCpuNanoseconds << ",\n"
          << "  \"hzbBuildRecordingCpuNanoseconds\": " << before.lastHzbBuildRecordingCpuNanoseconds << ",\n"
          << "  \"hzbCullRecordingCpuNanoseconds\": " << before.lastHzbCullRecordingCpuNanoseconds << ",\n"
          << "  \"cpuP95Nanoseconds\": " << cpuP95 << ",\n"
          << "  \"cpuWorkClock\": \"QueryThreadCycleTime\",\n"
          << "  \"cpuThreadCycleCalibrationValid\": " << (showcaseThreadCycleCalibrationValid_?1:0) << ",\n"
          << "  \"cpuThreadCyclesPerSecond\": " << showcaseThreadCyclesPerSecond_ << ",\n"
          << "  \"cpuThreadCycleCalibration\": [" << showcaseThreadCycleCalibration_[0] << ',' << showcaseThreadCycleCalibration_[1] << ',' << showcaseThreadCycleCalibration_[2] << ',' << showcaseThreadCycleCalibration_[3] << ',' << showcaseThreadCycleCalibration_[4] << ',' << showcaseThreadCycleCalibration_[5] << ',' << showcaseThreadCycleCalibration_[6] << "],\n"
          << "  \"cpuSampleCount\": " << cpuPerformance.sampleCount << ",\n"
          << "  \"cpuWorkRawP95Nanoseconds\": " << cpuPerformance.rawP95Nanoseconds << ",\n"
          << "  \"cpuWindowCount\": " << ShowcaseCpuWindowCount << ",\n"
          << "  \"cpuWindowSampleCounts\": [" << cpuPerformance.windowSampleCounts[0] << ',' << cpuPerformance.windowSampleCounts[1] << ',' << cpuPerformance.windowSampleCounts[2] << ',' << cpuPerformance.windowSampleCounts[3] << ',' << cpuPerformance.windowSampleCounts[4] << ',' << cpuPerformance.windowSampleCounts[5] << ',' << cpuPerformance.windowSampleCounts[6] << "],\n"
          << "  \"cpuWindowP95Nanoseconds\": [" << cpuPerformance.windowP95Nanoseconds[0] << ',' << cpuPerformance.windowP95Nanoseconds[1] << ',' << cpuPerformance.windowP95Nanoseconds[2] << ',' << cpuPerformance.windowP95Nanoseconds[3] << ',' << cpuPerformance.windowP95Nanoseconds[4] << ',' << cpuPerformance.windowP95Nanoseconds[5] << ',' << cpuPerformance.windowP95Nanoseconds[6] << "],\n"
          << "  \"cpuMedianWindowP95Nanoseconds\": " << cpuPerformance.medianWindowP95Nanoseconds << ",\n"
          << "  \"cpuMaximumNanoseconds\": " << cpuPerformance.maximumNanoseconds << ",\n"
          << "  \"endToEndSampleCount\": " << endToEndPerformance.sampleCount << ",\n"
          << "  \"endToEndWindowSampleCounts\": [" << endToEndPerformance.windowSampleCounts[0] << ',' << endToEndPerformance.windowSampleCounts[1] << ',' << endToEndPerformance.windowSampleCounts[2] << ',' << endToEndPerformance.windowSampleCounts[3] << ',' << endToEndPerformance.windowSampleCounts[4] << ',' << endToEndPerformance.windowSampleCounts[5] << ',' << endToEndPerformance.windowSampleCounts[6] << "],\n"
          << "  \"endToEndWindowP95Nanoseconds\": [" << endToEndPerformance.windowP95Nanoseconds[0] << ',' << endToEndPerformance.windowP95Nanoseconds[1] << ',' << endToEndPerformance.windowP95Nanoseconds[2] << ',' << endToEndPerformance.windowP95Nanoseconds[3] << ',' << endToEndPerformance.windowP95Nanoseconds[4] << ',' << endToEndPerformance.windowP95Nanoseconds[5] << ',' << endToEndPerformance.windowP95Nanoseconds[6] << "],\n"
          << "  \"endToEndRawP95Nanoseconds\": " << endToEndPerformance.rawP95Nanoseconds << ",\n"
          << "  \"endToEndMedianWindowP95Nanoseconds\": " << endToEndPerformance.medianWindowP95Nanoseconds << ",\n"
          << "  \"endToEndMaximumNanoseconds\": " << endToEndPerformance.maximumNanoseconds << ",\n"
          << "  \"endToEndCoverageComplete\": " << (endToEndPerformance.coverageComplete?1:0) << ",\n"
          << "  \"cpuStages\": {\"preparation\":{\"samples\":" << cpuStagePerformance[0].sampleCount << ",\"rawP95\":" << cpuStagePerformance[0].rawP95Nanoseconds << ",\"medianWindowP95\":" << cpuStagePerformance[0].medianWindowP95Nanoseconds << ",\"max\":" << cpuStagePerformance[0].maximumNanoseconds << "},\"commandRecording\":{\"samples\":" << cpuStagePerformance[1].sampleCount << ",\"rawP95\":" << cpuStagePerformance[1].rawP95Nanoseconds << ",\"medianWindowP95\":" << cpuStagePerformance[1].medianWindowP95Nanoseconds << ",\"max\":" << cpuStagePerformance[1].maximumNanoseconds << "},\"queueSubmit\":{\"samples\":" << cpuStagePerformance[2].sampleCount << ",\"rawP95\":" << cpuStagePerformance[2].rawP95Nanoseconds << ",\"medianWindowP95\":" << cpuStagePerformance[2].medianWindowP95Nanoseconds << ",\"max\":" << cpuStagePerformance[2].maximumNanoseconds << "},\"fenceWait\":{\"samples\":" << cpuStagePerformance[3].sampleCount << ",\"rawP95\":" << cpuStagePerformance[3].rawP95Nanoseconds << ",\"medianWindowP95\":" << cpuStagePerformance[3].medianWindowP95Nanoseconds << ",\"max\":" << cpuStagePerformance[3].maximumNanoseconds << "},\"present\":{\"samples\":" << cpuStagePerformance[4].sampleCount << ",\"rawP95\":" << cpuStagePerformance[4].rawP95Nanoseconds << ",\"medianWindowP95\":" << cpuStagePerformance[4].medianWindowP95Nanoseconds << ",\"max\":" << cpuStagePerformance[4].maximumNanoseconds << "}},\n"
          << "  \"cpuBudgetNanoseconds\": " << ShowcaseCpuBudgetNanoseconds << ",\n"
          << "  \"cpuCatastrophicBudgetNanoseconds\": " << ShowcaseCpuCatastrophicNanoseconds << ",\n"
          << "  \"cpuCoverageComplete\": " << (cpuPerformance.coverageComplete?1:0) << ",\n"
          << "  \"cpuHostileContractPassed\": " << (cpuHostileContractPassed?1:0) << ",\n"
          << "  \"steadyAllocationTotal\": " << showcasePreviousSteadyAllocationTotal_ << ",\n"
          << "  \"steadyAllocationChanges\": " << showcaseSteadyAllocationChanges_ << ",\n"
          << "  \"steadyAllocationFrames\": " << showcaseSteadyAllocationFrames_ << ",\n"
          << "  \"steadyAllocationEstablished\": " << (showcaseSteadyAllocationEstablished_?1:0) << ",\n"
          << "  \"warmupResourceChanges\": " << showcaseWarmupResourceChanges_ << ",\n"
          << "  \"phaseSteadyCoverage\": [" << showcasePhaseSteadyCoverage_[0] << ',' << showcasePhaseSteadyCoverage_[1] << ',' << showcasePhaseSteadyCoverage_[2] << ',' << showcasePhaseSteadyCoverage_[3] << ',' << showcasePhaseSteadyCoverage_[4] << ',' << showcasePhaseSteadyCoverage_[5] << ',' << showcasePhaseSteadyCoverage_[6] << ',' << showcasePhaseSteadyCoverage_[7] << ',' << showcasePhaseSteadyCoverage_[8] << ',' << showcasePhaseSteadyCoverage_[9] << ',' << showcasePhaseSteadyCoverage_[10] << ',' << showcasePhaseSteadyCoverage_[11] << "],\n"
          << "  \"phaseSteadyChanges\": [" << showcasePhaseSteadyChanges_[0] << ',' << showcasePhaseSteadyChanges_[1] << ',' << showcasePhaseSteadyChanges_[2] << ',' << showcasePhaseSteadyChanges_[3] << ',' << showcasePhaseSteadyChanges_[4] << ',' << showcasePhaseSteadyChanges_[5] << ',' << showcasePhaseSteadyChanges_[6] << ',' << showcasePhaseSteadyChanges_[7] << ',' << showcasePhaseSteadyChanges_[8] << ',' << showcasePhaseSteadyChanges_[9] << ',' << showcasePhaseSteadyChanges_[10] << ',' << showcasePhaseSteadyChanges_[11] << "],\n"
          << "  \"steadyResourceCounterChanges\": [" << showcaseSteadyResourceCounterChanges_[0] << ',' << showcaseSteadyResourceCounterChanges_[1] << ',' << showcaseSteadyResourceCounterChanges_[2] << ',' << showcaseSteadyResourceCounterChanges_[3] << ',' << showcaseSteadyResourceCounterChanges_[4] << ',' << showcaseSteadyResourceCounterChanges_[5] << ',' << showcaseSteadyResourceCounterChanges_[6] << ',' << showcaseSteadyResourceCounterChanges_[7] << ',' << showcaseSteadyResourceCounterChanges_[8] << ',' << showcaseSteadyResourceCounterChanges_[9] << ',' << showcaseSteadyResourceCounterChanges_[10] << ',' << showcaseSteadyResourceCounterChanges_[11] << ',' << showcaseSteadyResourceCounterChanges_[12] << ',' << showcaseSteadyResourceCounterChanges_[13] << ',' << showcaseSteadyResourceCounterChanges_[14] << ',' << showcaseSteadyResourceCounterChanges_[15] << "],\n"
          << "  \"steadyPendingViolations\": " << showcaseSteadyPendingViolations_ << ",\n"
          << "  \"startupResourceCounterEnd\": " << showcaseStartupResourceCounterEnd_ << ",\n"
          << "  \"transitionResourceCounterStart\": " << showcaseTransitionResourceCounterStart_ << ",\n"
          << "  \"transitionResourceCounterEnd\": " << showcasePreviousSteadyAllocationTotal_ << ",\n"
          << "  \"featuresExecuted\": " << (featuresExecuted ? 1 : 0)
          << ",\n"
          << "  \"memoryGatePassed\": " << (memoryGate ? 1 : 0) << ",\n"
          << "  \"gpuGatePassed\": " << (gpuGate ? 1 : 0) << ",\n"
          << "  \"cpuGatePassed\": " << (cpuGate ? 1 : 0) << "\n"
          << "}\n";
}

void FrontierWorldGame::OnRender(Engine &engine, float) {
  if (!captureDirectory_.empty() && !capturePending_ &&
      captureIndex_ < ShowcaseCaptureNames.size())
    ++capturePresentationFrames_;
  scene_.Clear();
  AddEnvironment();
  const Vec3 feet = controller_.Position(),
             focus = feet + Vec3{0, riverView_ ? 4.5f : 1.3f, 0};
  if (viewMode_ == ViewMode::FirstPerson)
    camera_ = MakeFirstPersonCamera(feet + Vec3{0, 1.68f, 0}, yaw_, pitch_);
  else if (viewMode_ == ViewMode::ThirdPerson) {
    const Camera3D desired = MakeThirdPersonCamera(
        focus, yaw_, riverView_ ? .35f : .25f, riverView_ ? 12.5f : 8.6f);
    camera_ =
        ConstrainThirdPersonCamera(desired, focus, collisionWorld_, .2f,
                                   WorldLayer | BuildingLayer, ActorLayer);
    AddHumanoid(feet, yaw_, true);
  } else {
    camera_ = MakeOverheadCamera(focus, 78, true);
    AddHumanoid(feet, yaw_, true);
  }
  for (const Monster &monster : monsters_)
    AddMonster(monster);
  DrawList &draw = engine.Draw();
  draw.SetPerformanceTelemetryEnabled3D(!captureDirectory_.empty());
  const bool fixedProfile = !captureDirectory_.empty();
  const float profileTime = lightingProfile_ == LightingProfile::Day
                                ? 43200.f
                            : lightingProfile_ == LightingProfile::Dusk
                                ? 64800.f
                                : 79200.f;
  const float environmentTime = fixedProfile ? profileTime : float(engine.Time());
  const float sunCycle = fixedProfile
                             ? (lightingProfile_ == LightingProfile::Day
                                    ? 1.f
                                : lightingProfile_ == LightingProfile::Dusk
                                    ? .34f
                                    : .04f)
                             : .5f + .5f * std::sin(environmentTime * .012f);
  const float daylight = fixedProfile
                             ? (.12f + .88f * sunCycle)
                             : (.82f + .18f * sunCycle);
  const float localLightIntensity = 1.75f - 1.25f * daylight;
  const float sunAzimuth = fixedProfile
                               ? (lightingProfile_ == LightingProfile::Day
                                      ? .35f
                                  : lightingProfile_ == LightingProfile::Dusk
                                      ? 1.34f
                                      : 2.1f)
                               : environmentTime * .004f;
  draw.SetExposureCompensation(lightingProfile_ == LightingProfile::Night
                                   ? .45f
                               : lightingProfile_ == LightingProfile::Dusk
                                   ? -.05f
                                   : -.3f);
  draw.SetEnvironmentTime(environmentTime);
  const Color sky{.025f + .255f * daylight, .045f + .435f * daylight,
                  .09f + .65f * daylight, 1};
  engine.SetClearColor(sky);
  draw.SetDirectionalLight(
      {{std::cos(sunAzimuth) * .42f, -.82f - .16f * sunCycle,
        std::sin(sunAzimuth) * .34f},
       .47f,
       {1.f, .88f + .08f * sunCycle, .72f + .18f * sunCycle, 1},
       .35f + 1.22f * daylight});
  draw.SetAtmosphere({{.12f + .26f * daylight, .18f + .32f * daylight,
                       .3f + .33f * daylight, .68f},
                      camera_.position,
                      54,
                      230,
                      true,
                      .0035f,
                      .09f,
                      -2.f,
                      .76f});
  draw.AddPointLight({feet + Vec3{0, 1.4f, 0}, 7, {.18f, .62f, 1, 1}, .58f});
  draw.AddPointLight(
      {{-28, 2.2f, 21}, 17, {1, .48f, .18f, 1}, localLightIntensity});
  draw.AddPointLight(
      {{-46, 2.2f, -15}, 17, {1, .48f, .18f, 1}, localLightIntensity});
  draw.AddPointLight(
      {{18, 2.2f, 41}, 17, {1, .48f, .18f, 1}, localLightIntensity});
  sceneStats_ =
      scene_.Submit(draw, camera_, {1280, 720},
                    {true, true, {true, .022f, .2f, 82.f, 384}, {190.f, .28f}});
  scene_.Clear();
  const char *viewName = viewMode_ == ViewMode::FirstPerson   ? "FIRST"
                         : viewMode_ == ViewMode::ThirdPerson ? "THIRD"
                                                              : "GOD";
  draw.Quad({640, 38}, {1280, 76}, {.015f, .025f, .03f, .9f});
  draw.Text({24, 16}, "AEVRYND FRONTIER WORLD", 3.2f, Cyan);
  draw.Text({470, 18},
            std::format("CC0 PBR {}/{}  HP {}  LEVEL {}  XP {}",
                        materialLibraryStats_.materials,
                        materialLibraryStats_.uniqueTextureSets, health_,
                        level_, experience_),
            2.15f, materialLibraryStats_.materials == 6 ? Cyan : Coral);
  draw.Text({1070, 18}, viewName, 2.3f, Gold);
  draw.Line({630, 360}, {650, 360}, 2, Cyan);
  draw.Line({640, 350}, {640, 370}, 2, Cyan);
  draw.Quad({640, 686}, {1280, 68}, {.015f, .025f, .03f, .9f});
  draw.Text({24, 668},
            std::format(
                "CC0 PBR WORLD | HOUSE {} MON {} KILL {} CHUNK {} Q{} L{}/U{} "
                "LOD F{} SAVE {} CULL D{} P{} SH {}",
                buildings_.size() + 3,
                std::count_if(monsters_.begin(), monsters_.end(),
                              [](const Monster &value) { return value.alive; }),
                kills_, worldStreamingStats_.residentChunks,
                worldStreamingStats_.queuedChunks,
                worldStreamingStats_.totalLoaded,
                worldStreamingStats_.totalUnloaded, sceneStats_.farLodInstances,
                sceneStats_.lodTrianglesSaved,
                sceneStats_.distanceCulledMeshInstances,
                sceneStats_.screenSizeCulledMeshInstances,
                sceneStats_.shadowInstances),
            1.35f, Ink);
  draw.Text({1030, 668}, automatic_ ? "AUTO F1 F2 F3" : "F1 F2 F3", 1.45f,
            automatic_ ? Cyan : Gold);
}
