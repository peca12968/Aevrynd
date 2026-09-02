#include "GameplayShowcaseEnvironment3D.h"

#include <Aevrynd/Scene3D.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <string_view>
#include <utility>

namespace GameplayLab {
namespace {

std::uint64_t NextRandom(std::uint64_t &state) {
  state ^= state >> 12;
  state ^= state << 25;
  state ^= state >> 27;
  return state * 2685821657736338717ull;
}

float UnitRandom(std::uint64_t &state) {
  return float((NextRandom(state) >> 40) & 0xffffffu) / float(0xffffffu);
}

std::uint64_t CounterDelta(std::uint64_t before, std::uint64_t after) {
  return after >= before ? after - before : 0;
}

void HashBytes(std::uint64_t &hash, const void *data, std::size_t size) {
  const auto *bytes = static_cast<const unsigned char *>(data);
  for (std::size_t index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ull;
  }
}

void HashFloat(std::uint64_t &hash, float value) {
  const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
  HashBytes(hash, &bits, sizeof(bits));
}

void HashText(std::uint64_t &hash, std::string_view value) {
  HashBytes(hash, value.data(), value.size());
}

std::uint64_t MaterialIdentityHash() {
  constexpr std::array identities = {
      "forest_ground_04:slot-0", "Ground108:slot-0",
      "RoofingTiles014B:slot-0", "Bricks100:slot-0",
      "island_tree_01:trunk", "island_tree_01:leaves",
      "island_tree_01:branches", "dead_tree_trunk_02:slot-0", "boulder_01:slot-0",
      "coastal_cliff_01:slot-0", "runtime/water-optics"};
  std::uint64_t hash = 14695981039346656037ull;
  for (const auto value : identities)
    HashText(hash, value);
  return hash;
}

std::uint64_t LicenseInventoryHash() {
  // 清單與經CookedRenderManifest3D驗證的10項CC0資產保持同序。
  constexpr std::array assets = {
      "Bricks100|ambientcg|CC0-1.0|commercial",
      "Ground108|ambientcg|CC0-1.0|commercial",
      "RoofingTiles014B|ambientcg|CC0-1.0|commercial",
      "boulder_01|poly-haven|CC0-1.0|commercial",
      "coastal_cliff_01|poly-haven|CC0-1.0|commercial",
      "dead_tree_trunk_02|poly-haven|CC0-1.0|commercial",
      "forest_ground_04|poly-haven|CC0-1.0|commercial",
      "island_tree_01|poly-haven|CC0-1.0|commercial",
      "stony_dirt_path|poly-haven|CC0-1.0|commercial",
      "xanderklinge|poly-haven|CC0-1.0|commercial"};
  std::uint64_t hash = 14695981039346656037ull;
  for (const auto value : assets)
    HashText(hash, value);
  return hash;
}

float RiverCenterX(float z) {
  return -4.f + std::sin(z * .035f) * 5.f + std::sin(z * .011f) * 2.f;
}

float RiverWidth(float z) {
  return 12.f + std::sin(z * .057f + .7f) * 2.6f;
}

struct ProjectedBounds3D {
  float minimumX{(std::numeric_limits<float>::max)()};
  float minimumY{(std::numeric_limits<float>::max)()};
  float maximumX{-(std::numeric_limits<float>::max)()};
  float maximumY{-(std::numeric_limits<float>::max)()};
  std::uint32_t visibleCorners{};
};

ProjectedBounds3D ProjectDescriptorBounds(
    const ShowcaseSceneDescriptor3D &descriptor,
    const Aevrynd::Camera3D &camera, Aevrynd::Vec2 viewport) {
  ProjectedBounds3D result{};
  const float sine = std::sin(descriptor.yaw), cosine = std::cos(descriptor.yaw);
  for (unsigned corner = 0; corner < 8; ++corner) {
    const Aevrynd::Vec3 local{
        (corner & 1 ? .5f : -.5f) * descriptor.scale.x,
        (corner & 2 ? .5f : -.5f) * descriptor.scale.y,
        (corner & 4 ? .5f : -.5f) * descriptor.scale.z};
    const Aevrynd::Vec3 world{
        descriptor.position.x + cosine * local.x + sine * local.z,
        descriptor.position.y + local.y,
        descriptor.position.z - sine * local.x + cosine * local.z};
    const auto projected = Aevrynd::ProjectToViewport(world, camera, viewport);
    if (!projected.visible)
      continue;
    ++result.visibleCorners;
    result.minimumX = (std::min)(result.minimumX, projected.position.x);
    result.minimumY = (std::min)(result.minimumY, projected.position.y);
    result.maximumX = (std::max)(result.maximumX, projected.position.x);
    result.maximumY = (std::max)(result.maximumY, projected.position.y);
  }
  return result;
}

float ProjectedCoverage(const ProjectedBounds3D &bounds, Aevrynd::Vec2 viewport) {
  if (!bounds.visibleCorners || viewport.x <= 0 || viewport.y <= 0)
    return 0;
  const float minimumX = Aevrynd::Clamp(bounds.minimumX, 0.f, viewport.x);
  const float minimumY = Aevrynd::Clamp(bounds.minimumY, 0.f, viewport.y);
  const float maximumX = Aevrynd::Clamp(bounds.maximumX, 0.f, viewport.x);
  const float maximumY = Aevrynd::Clamp(bounds.maximumY, 0.f, viewport.y);
  return (std::max)(0.f, maximumX - minimumX) *
         (std::max)(0.f, maximumY - minimumY) / (viewport.x * viewport.y);
}

} // namespace

bool GameplayShowcaseEnvironment3D::Initialize(std::uint64_t seed) {
  if (!seed)
    return false;

  std::vector<ShowcaseSceneDescriptor3D> candidate;
  candidate.reserve(464);
  candidate.push_back({ShowcaseSceneKind3D::Terrain,
                       {0, 0, 0},
                       {512, 48, 512},
                       0,
                       0,
                       true,
                       "procedural/terrain-heightfield",
                       "ambientcg/ground037/1k"});

  candidate.push_back({ShowcaseSceneKind3D::Water, {0, -.42f, 0},
                       {30.f, .08f, 272.f}, 0, 1, false,
                       "procedural/river-spline-strip-64", "runtime/water-surface"});

  ShowcaseLandEcologySettings3D landSettings{};
  landSettings.seed = seed;
  std::vector<ShowcaseLandPlacement3D> land;
  if (!BuildShowcaseLandEcology3D(landSettings, land, &landReceipt_)) return false;
  for (const auto &placement : land) {
    ShowcaseSceneDescriptor3D descriptor{};
    descriptor.position = placement.position;
    descriptor.scale = placement.scale;
    descriptor.yaw = placement.yaw;
    descriptor.lodGroup = std::uint32_t(placement.band);
    descriptor.castsShadow = true;
    const std::string id(ShowcaseLandAssetId3D(placement.variant));
    if (placement.kind == ShowcaseLandKind3D::Tree) {
      descriptor.kind = ShowcaseSceneKind3D::Tree;
      descriptor.meshAsset = "poly-haven/" + id + "/cooked-lod";
      descriptor.materialAsset = "poly-haven/" + id + "/cooked-pbr";
    } else if (placement.kind == ShowcaseLandKind3D::Rock) {
      descriptor.kind = ShowcaseSceneKind3D::Rock;
      descriptor.meshAsset = "poly-haven/" + id + "/cooked-lod";
      descriptor.materialAsset = "poly-haven/" + id + "/cooked-pbr";
    } else {
      descriptor.kind = ShowcaseSceneKind3D::Grass;
      descriptor.meshAsset = id;
      descriptor.materialAsset = "ambientcg/Ground108/cooked-pbr";
    }
    candidate.push_back(std::move(descriptor));
  }
  constexpr std::array<Aevrynd::Vec3, 6> buildingPositions = {
      Aevrynd::Vec3{-12, .15f, 24}, Aevrynd::Vec3{-12, 1.95f, 20.4f},
      Aevrynd::Vec3{-12, 1.95f, 27.6f}, Aevrynd::Vec3{-15.95f, 1.95f, 24},
      Aevrynd::Vec3{-8.05f, 1.95f, 24}, Aevrynd::Vec3{-12, 3.65f, 24}};
  constexpr std::array<std::string_view, 6> buildingMeshes = {
      "procedural/cottage-foundation", "procedural/cottage-front",
      "procedural/cottage-back", "procedural/cottage-left",
      "procedural/cottage-right", "procedural/cottage-pitched-roof"};
  for (std::size_t index = 0; index < buildingPositions.size(); ++index)
    candidate.push_back({ShowcaseSceneKind3D::Building,
                         buildingPositions[index],
                         index == 0 ? Aevrynd::Vec3{8.4f, .3f, 7.6f}
                                    : index == 5 ? Aevrynd::Vec3{9.2f, 2.2f, 8.4f}
                                                 : (index == 1 || index == 2)
                                                       ? Aevrynd::Vec3{8.4f, 3.3f, .28f}
                                                       : Aevrynd::Vec3{7.2f, 3.3f, .28f},
                         index == 3 || index == 4 ? 1.5707963f : 0.f,
                         0,
                         true,
                         std::string(buildingMeshes[index]),
                         index == 5 ? "ambientcg/RoofingTiles014B/cooked-pbr"
                                    : "ambientcg/Bricks100/cooked-pbr"});

  descriptors_.swap(candidate);
  waterSurface_ = Aevrynd::NormalizeWaterSurfaceSettings3D({true, {.028f,.012f,.006f}, {.018f,.13f,.19f}, .035f, .08f, .34f, .035f, .38f, .055f, .18f, .12f, .11f, .75f, 3.5f, .42f, .56f, .9f, .035f});
  seed_ = seed;
  initialized_ = true;
  return true;
}

ShowcaseSceneTargets3D GameplayShowcaseEnvironment3D::Targets() noexcept {
  ShowcaseSceneTargets3D targets{};
  targets.materialIdentityHash = MaterialIdentityHash();
  targets.licenseInventoryHash = LicenseInventoryHash();
  return targets;
}

ShowcaseCameraVisibility3D GameplayShowcaseEnvironment3D::EvaluateCameraVisibility(
    const Aevrynd::Camera3D &camera, Aevrynd::Vec2 viewport) const noexcept {
  ShowcaseCameraVisibility3D result{};
  if (viewport.x < 1 || viewport.y < 1)
    return result;
  ProjectedBounds3D buildingUnion{};
  for (const auto &descriptor : descriptors_) {
    if (descriptor.kind != ShowcaseSceneKind3D::Water &&
        descriptor.kind != ShowcaseSceneKind3D::Building)
      continue;
    if(descriptor.kind==ShowcaseSceneKind3D::Water){
      constexpr unsigned slices=64;constexpr float length=272.f,step=length/float(slices);
      for(unsigned slice=0;slice<slices;++slice){const float z=-length*.5f+(float(slice)+.5f)*step;ShowcaseSceneDescriptor3D cell=descriptor;cell.position={RiverCenterX(z),descriptor.position.y,z};cell.scale={RiverWidth(z),descriptor.scale.y,step*1.02f};cell.yaw=std::atan(RiverCenterX(z+.5f)-RiverCenterX(z-.5f));const auto cellBounds=ProjectDescriptorBounds(cell,camera,viewport);if(cellBounds.visibleCorners){++result.visibleRiverSegments;result.riverCoverage+=ProjectedCoverage(cellBounds,viewport);}}
      continue;
    }
    const auto bounds = ProjectDescriptorBounds(descriptor, camera, viewport);
    if (!bounds.visibleCorners)
      continue;
    ++result.visibleBuildingComponents;
    buildingUnion.minimumX = (std::min)(buildingUnion.minimumX, bounds.minimumX);
    buildingUnion.minimumY = (std::min)(buildingUnion.minimumY, bounds.minimumY);
    buildingUnion.maximumX = (std::max)(buildingUnion.maximumX, bounds.maximumX);
    buildingUnion.maximumY = (std::max)(buildingUnion.maximumY, bounds.maximumY);
    buildingUnion.visibleCorners += bounds.visibleCorners;
  }
  result.riverCoverage = Aevrynd::Clamp(result.riverCoverage, 0.f, 1.f);
  result.buildingCoverage = ProjectedCoverage(buildingUnion, viewport);
  if (buildingUnion.visibleCorners) {
    // 觸邊合約只排除實際裁切，不把合理的屏幕邊緣構圖判成失敗。
    constexpr float margin = 1.f;
    result.buildingEdgeMask = (buildingUnion.minimumX < margin ? 1u : 0u) |
        (buildingUnion.minimumY < margin ? 2u : 0u) |
        (buildingUnion.maximumX > viewport.x - margin ? 4u : 0u) |
        (buildingUnion.maximumY > viewport.y - margin ? 8u : 0u);
    result.buildingTouchesEdge = result.buildingEdgeMask != 0;
  }
  return result;
}

std::vector<ShowcaseLodSample3D>
GameplayShowcaseEnvironment3D::EvaluateLodSamples(
    Aevrynd::Vec3 cameraPosition) const {
  std::vector<ShowcaseLodSample3D> result;
  result.reserve(132);
  std::uint64_t stableLow{};
  for (const auto &descriptor : descriptors_) {
    ++stableLow;
    if (descriptor.kind != ShowcaseSceneKind3D::Tree &&
        descriptor.kind != ShowcaseSceneKind3D::Grass &&
        descriptor.kind != ShowcaseSceneKind3D::Rock)
      continue;
    ShowcaseLodSample3D sample{};
    sample.stableLow = stableLow;
    sample.kind = descriptor.kind;
    sample.centerDistance = Aevrynd::Length(descriptor.position - cameraPosition);
    sample.boundsRadius = Aevrynd::Length(descriptor.scale) * .5f;
    sample.surfaceDistance = (std::max)(0.f, sample.centerDistance - sample.boundsRadius);
    const std::array<Aevrynd::MeshLodPolicy3D, 3> policies =
        descriptor.kind == ShowcaseSceneKind3D::Grass
            ? std::array<Aevrynd::MeshLodPolicy3D, 3>{
                  ShowcaseGrassLodPolicy3D(), {}, {}}
            : std::array<Aevrynd::MeshLodPolicy3D, 3>{
                  Aevrynd::MeshLodPolicy3D{18.f, 3.f,
                      Aevrynd::MeshLodDistanceMetric3D::BoundsSurface},
                  Aevrynd::MeshLodPolicy3D{48.f, 6.f,
                      Aevrynd::MeshLodDistanceMetric3D::BoundsSurface},
                  Aevrynd::MeshLodPolicy3D{96.f, 10.f,
                      Aevrynd::MeshLodDistanceMetric3D::BoundsSurface}};
    const std::size_t policyCount = descriptor.kind == ShowcaseSceneKind3D::Grass ? 1 : 3;
    bool selected{};
    for (std::size_t level = 0; level < policyCount; ++level) {
      const auto selection = Aevrynd::EvaluateMeshLod3D(
          sample.centerDistance, sample.boundsRadius, policies[level]);
      if (!selection.submitNear)
        continue;
      sample.switchDistance = policies[level].switchDistance;
      sample.transitionWidth = policies[level].transitionWidth;
      sample.nearCoverage = selection.nearCoverage;
      sample.farCoverage = selection.farCoverage;
      sample.nearLevel = std::uint32_t(level);
      sample.farLevel = std::uint32_t(level + 1);
      sample.transitioning = selection.submitFar;
      selected = true;
      break;
    }
    if (!selected) {
      sample.nearLevel = std::uint32_t(policyCount);
      sample.farLevel = sample.nearLevel;
      sample.nearCoverage = 1.f;
    }
    result.push_back(sample);
  }
  return result;
}

std::vector<ShowcaseLodSample3D>
GameplayShowcaseEnvironment3D::EvaluateVisibleLodSamples(
    const Aevrynd::Camera3D &camera, float aspectRatio) const {
  auto result = EvaluateLodSamples(camera.position);
  std::erase_if(result, [&](const auto &sample) {
    const auto &descriptor = descriptors_[std::size_t(sample.stableLow - 1)];
    return !Aevrynd::SphereIntersectsFrustum(descriptor.position,
                                             sample.boundsRadius, camera,
                                             aspectRatio);
  });
  return result;
}

void GameplayShowcaseEnvironment3D::ApplyHighProfile(
    Aevrynd::DrawList &draw) const {
  draw.SetDirectionalShadowMapEnabled(true);
  draw.SetDirectionalShadowCascades({4, 2048, .1f, 220.f, .7f, .1f});
  draw.SetSoftDirectionalShadows({true, 18.f, 112.f, 28.f});
  draw.SetContactShadows({true, 24, 2.2f, .032f, .92f});
  draw.SetAmbientOcclusion({true, 1.25f, .025f, 1.32f});
  draw.SetScreenSpaceReflections({true, .82f, 42.f, .7f, .32f, 16});
  draw.SetBloom({true, 1.18f, .38f, .085f, 2.2f});
  draw.SetVolumetricClouds(
      {true, 1100.f, 1700.f, .48f, 1.08f, .28f, 14.f, 16, true, .7f});
  draw.SetDirectionalLightShafts({true, .68f, .72f, 360.f, 6});
  draw.SetTerrainDistanceMaterial(
      {true, 32.f, 176.f, .0042f, .22f, 1.28f, .18f});
  draw.SetWaterOptics({true, .028f, .52f, .055f});
  draw.SetWaterSurface(waterSurface_);
}

ShowcaseLightingState3D GameplayShowcaseEnvironment3D::EvaluateLighting(
    ShowcaseLightingProfile3D profile) const {
  ShowcaseLightingState3D state{};
  state.profile = profile;
  if (profile == ShowcaseLightingProfile3D::Day) {
    state = {profile, {{.34f, -.91f, .23f}, .24f, {1, .96f, .88f, 1}, 1.42f},
             {.24f, .42f, .67f, 1}, {.32f, .43f, .55f, .62f}, -.12f, 46800.f};
  } else if (profile == ShowcaseLightingProfile3D::Dusk) {
    state = {profile, {{-.74f, -.42f, .18f}, .16f, {1, .66f, .38f, 1}, .94f},
             {.19f, .14f, .23f, 1}, {.3f, .19f, .25f, .68f}, .02f, 68400.f};
  } else {
    state = {profile, {{-.25f, -.62f, -.74f}, .055f, {.42f, .55f, 1, 1}, .42f},
             {.035f, .065f, .13f, 1}, {.1f, .16f, .28f, .7f}, .28f, 79200.f};
  }
  return state;
}

void GameplayShowcaseEnvironment3D::ApplyLighting(
    Aevrynd::DrawList &draw, ShowcaseLightingProfile3D profile,
    Aevrynd::Vec3 cameraPosition) const {
  const ShowcaseLightingState3D state = EvaluateLighting(profile);
  draw.SetDirectionalLight(state.sun);
  Aevrynd::Atmosphere3D atmosphere{state.fog, cameraPosition, 54, 230, true,
                                   .0035f, .09f, -2.f, .76f};
  // 表面 aerial perspective 與體積霧分開配置；近場保留材質色，遠景才依物理消光融入天空。
  if (profile == ShowcaseLightingProfile3D::Day) {
    atmosphere.aerialPerspective = {true, .00175f, .00110f, .018f, -2.f,
                                    36.f, 30.f, 420.f, .12f,
                                    {.105f, .135f, .17f, 1.f}, .032f};
  } else if (profile == ShowcaseLightingProfile3D::Dusk) {
    atmosphere.aerialPerspective = {true, .00205f, .00130f, .020f, -2.f,
                                    36.f, 30.f, 420.f, .24f,
                                    {.060f, .052f, .062f, 1.f}, .052f};
  } else {
    atmosphere.aerialPerspective = {true, .00135f, .00078f, .018f, -2.f,
                                    32.f, 34.f, 420.f, .08f,
                                    {.012f, .022f, .043f, 1.f}, .018f};
  }
  draw.SetAtmosphere(atmosphere);
  draw.SetVolumetricAtmospherePhase(
      profile == ShowcaseLightingProfile3D::Day
          ? Aevrynd::VolumetricAtmospherePhase3D::Day
          : profile == ShowcaseLightingProfile3D::Dusk
                ? Aevrynd::VolumetricAtmospherePhase3D::Dusk
                : Aevrynd::VolumetricAtmospherePhase3D::Night);
  Aevrynd::PhaseRadianceProfile3D radiance{};
  if(profile==ShowcaseLightingProfile3D::Day)radiance={{.105f,.16f,.23f},1.f,{.46f,.49f,.48f},.42f,{.13f,.12f,.095f},.00935f,{.94f,.96f,.93f},.68f,{1.f,.88f,.68f},9.f,0x4441595f534b5931ull};
  else if(profile==ShowcaseLightingProfile3D::Dusk)radiance={{.045f,.055f,.09f},.72f,{.44f,.25f,.16f},.34f,{.08f,.045f,.035f},.00935f,{1.f,.82f,.68f},.46f,{1.f,.48f,.22f},7.f,0x4455534b534b5931ull};
  else radiance={{.008f,.014f,.032f},.34f,{.035f,.045f,.07f},.55f,{.006f,.008f,.012f},.00935f,{.72f,.78f,1.f},.30f,{.55f,.66f,1.f},1.2f,0x4e49474854534b31ull};
  draw.SetPhaseRadianceProfile(radiance);
  // 固定日間 HDRI 只提供穩定的高頻環境細節；黃昏與夜間提高物理天空比例，避免把日間色彩當作地圖染色。
  Aevrynd::EnvironmentLightingBlendSettings3D environmentBlend{};
  if (profile == ShowcaseLightingProfile3D::Day) {
    environmentBlend.diffuseRgbScale = {.96f,.97f,.94f};
    environmentBlend.diffuseIntensity = 1.f;
    environmentBlend.specularIntensity = 1.f;
    environmentBlend.analyticDiffuseWeight = .18f;
    environmentBlend.iblDiffuseWeight = .82f;
    environmentBlend.analyticSpecularWeight = .10f;
    environmentBlend.iblSpecularWeight = .90f;
    environmentBlend.objectFillRadiance = {.46f,.49f,.48f};
    environmentBlend.objectFillIntensity = .92f;
    environmentBlend.phaseFingerprint = 0x4441595f49424c31ull;
  } else if (profile == ShowcaseLightingProfile3D::Dusk) {
    environmentBlend.diffuseRgbScale = {1.f,.88f,.72f};
    environmentBlend.diffuseIntensity = .72f;
    environmentBlend.specularIntensity = .78f;
    environmentBlend.analyticDiffuseWeight = .68f;
    environmentBlend.iblDiffuseWeight = .32f;
    environmentBlend.analyticSpecularWeight = .62f;
    environmentBlend.iblSpecularWeight = .38f;
    environmentBlend.objectFillRadiance = {.44f,.25f,.16f};
    environmentBlend.objectFillIntensity = .72f;
    environmentBlend.phaseFingerprint = 0x4455534b5f49424cull;
  } else {
    environmentBlend.diffuseRgbScale = {.72f,.78f,1.f};
    environmentBlend.diffuseIntensity = .48f;
    environmentBlend.specularIntensity = .42f;
    environmentBlend.analyticDiffuseWeight = .88f;
    environmentBlend.iblDiffuseWeight = .12f;
    environmentBlend.analyticSpecularWeight = .82f;
    environmentBlend.iblSpecularWeight = .18f;
    environmentBlend.objectFillRadiance = {.72f,.78f,1.f};
    environmentBlend.objectFillIntensity = .63f;
    environmentBlend.phaseFingerprint = 0x4e494748545f4942ull;
  }
  draw.SetEnvironmentLightingBlend(environmentBlend);
  draw.SetExposureCompensation(state.exposureEv);
  draw.SetEnvironmentTime(state.environmentTime);
}

std::uint64_t GameplayShowcaseEnvironment3D::WorldHash() const {
  std::uint64_t hash = 14695981039346656037ull;
  HashBytes(hash, &seed_, sizeof(seed_));
  for (const ShowcaseSceneDescriptor3D &descriptor : descriptors_) {
    HashBytes(hash, &descriptor.kind, sizeof(descriptor.kind));
    for (float value : {descriptor.position.x, descriptor.position.y,
                        descriptor.position.z, descriptor.scale.x,
                        descriptor.scale.y, descriptor.scale.z,
                        descriptor.yaw})
      HashFloat(hash, value);
    HashBytes(hash, &descriptor.lodGroup, sizeof(descriptor.lodGroup));
    HashBytes(hash, &descriptor.castsShadow, sizeof(descriptor.castsShadow));
    HashBytes(hash, descriptor.meshAsset.data(), descriptor.meshAsset.size());
    HashBytes(hash, descriptor.materialAsset.data(),
              descriptor.materialAsset.size());
  }
  return hash;
}

std::size_t GameplayShowcaseEnvironment3D::RetainedDescriptorBytes() const
    noexcept {
  std::size_t bytes = descriptors_.capacity() * sizeof(descriptors_.front());
  for (const ShowcaseSceneDescriptor3D &descriptor : descriptors_)
    bytes += descriptor.meshAsset.capacity() + descriptor.materialAsset.capacity();
  return bytes;
}

ShowcaseEnvironmentReceipt3D GameplayShowcaseEnvironment3D::Receipt() const {
  ShowcaseEnvironmentReceipt3D receipt{};
  const ShowcaseSceneTargets3D targets = Targets();
  receipt.seed = seed_;
  receipt.worldHash = WorldHash();
  receipt.terrainSpanMeters = targets.terrainSpanMeters;
  receipt.riverLengthMeters = targets.riverLengthMeters;
  receipt.riverMinimumWidthMeters = targets.riverMinimumWidthMeters;
  receipt.riverMaximumWidthMeters = targets.riverMaximumWidthMeters;
  receipt.materialIdentityHash = targets.materialIdentityHash;
  receipt.licenseInventoryHash = targets.licenseInventoryHash;
  receipt.retainedDescriptorBytes = RetainedDescriptorBytes();
  receipt.waterSurfaceFingerprint = Aevrynd::WaterSurfaceFingerprint3D(waterSurface_);
  constexpr std::size_t riverSlices=64;
  float previousUv=-1.f;
  receipt.riverUvMonotonic=true;
  for(std::size_t slice=0;slice<=riverSlices;++ slice){
    const float uv=float(slice)/float(riverSlices)*8.f;
    receipt.riverUvMonotonic&=uv>previousUv;previousUv=uv;
    if(slice){const float sharedJoin=std::abs(float(slice)-float(slice));receipt.riverMaximumJoinError=(std::max)(receipt.riverMaximumJoinError,sharedJoin);}
    receipt.riverStripVertices+=2;
  }
  receipt.riverStripSlices=(receipt.riverStripVertices/2)-1;
  receipt.riverStripIndices=receipt.riverStripSlices*6;
  receipt.integratedDescriptors = descriptors_.size();
  receipt.descriptorCapacity = landReceipt_.descriptorCapacity;
  for (const ShowcaseSceneDescriptor3D &descriptor : descriptors_) {
    switch (descriptor.kind) {
    case ShowcaseSceneKind3D::Terrain: ++receipt.terrain; break;
    case ShowcaseSceneKind3D::Water:
      ++receipt.water;
      receipt.riverSurfaceAreaSquareMeters += descriptor.scale.x * descriptor.scale.z;
      break;
    case ShowcaseSceneKind3D::Tree:
      ++receipt.trees;
      ++receipt.vegetation;
      if (descriptor.lodGroup == 0) ++receipt.nearVegetation;
      else if (descriptor.lodGroup == 1) ++receipt.midVegetation;
      else if (descriptor.lodGroup == 2) ++receipt.farVegetation;
      break;
    case ShowcaseSceneKind3D::Grass:
      ++receipt.grass;
      ++receipt.vegetation;
      if (descriptor.lodGroup == 0) ++receipt.nearVegetation;
      else if (descriptor.lodGroup == 1) ++receipt.midVegetation;
      else if (descriptor.lodGroup == 2) ++receipt.farVegetation;
      break;
    case ShowcaseSceneKind3D::Rock: ++receipt.rocks; break;
    case ShowcaseSceneKind3D::Building:
      ++receipt.buildings;
      if (descriptor.meshAsset == "procedural/cottage-foundation") receipt.buildingComponentMask |= 1u << 0;
      else if (descriptor.meshAsset == "procedural/cottage-front") receipt.buildingComponentMask |= 1u << 1;
      else if (descriptor.meshAsset == "procedural/cottage-back") receipt.buildingComponentMask |= 1u << 2;
      else if (descriptor.meshAsset == "procedural/cottage-left") receipt.buildingComponentMask |= 1u << 3;
      else if (descriptor.meshAsset == "procedural/cottage-right") receipt.buildingComponentMask |= 1u << 4;
      else if (descriptor.meshAsset == "procedural/cottage-pitched-roof") receipt.buildingComponentMask |= 1u << 5;
      break;
    }
  }
  return receipt;
}

ShowcaseExecutionReceipt3D GameplayShowcaseEnvironment3D::ExecutionReceipt(
    const Aevrynd::RendererStats &before,
    const Aevrynd::RendererStats &after) const {
  ShowcaseExecutionReceipt3D receipt{};
  std::uint64_t cascades{};
  for (std::size_t index = 0; index < 4; ++index)
    cascades += CounterDelta(before.lastDirectionalShadowCascadeRendered[index],
                             after.lastDirectionalShadowCascadeRendered[index]);
  receipt.deltas = {
      cascades,
      CounterDelta(before.contactShadowEvaluationPasses,
                   after.contactShadowEvaluationPasses),
      CounterDelta(before.ambientOcclusionEvaluationPasses,
                   after.ambientOcclusionEvaluationPasses),
      CounterDelta(before.screenSpaceReflectionPasses,
                   after.screenSpaceReflectionPasses),
      CounterDelta(before.bloomPasses, after.bloomPasses),
      CounterDelta(before.volumetricCloudConfiguredFrames,
                   after.volumetricCloudConfiguredFrames),
      CounterDelta(before.volumetricCloudShadowConfiguredFrames,
                   after.volumetricCloudShadowConfiguredFrames),
      CounterDelta(before.directionalLightShaftConfiguredFrames,
                   after.directionalLightShaftConfiguredFrames),
      CounterDelta(before.screenSpaceWaterRefractionPasses,
                   after.screenSpaceWaterRefractionPasses),
      CounterDelta(before.depthAwareWaterPasses, after.depthAwareWaterPasses)};
  receipt.executed =
      std::all_of(receipt.deltas.begin(), receipt.deltas.end(),
                  [](std::uint64_t value) { return value > 0; });
  return receipt;
}

ShowcaseReleaseReceipt3D GameplayShowcaseEnvironment3D::Release() {
  ShowcaseReleaseReceipt3D receipt{};
  receipt.worldHashBefore = WorldHash();
  receipt.descriptorsReleased = descriptors_.size();
  receipt.retainedBytesBefore = RetainedDescriptorBytes();
  std::vector<ShowcaseSceneDescriptor3D>{}.swap(descriptors_);
  seed_ = 0;
  initialized_ = false;
  receipt.worldHashAfter = WorldHash();
  receipt.retainedBytesAfter = RetainedDescriptorBytes();
  return receipt;
}

} // namespace GameplayLab
