#include "GameplayShowcaseEnvironment3D.h"

#include <algorithm>
#include <bit>
#include <cmath>
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

} // namespace

bool GameplayShowcaseEnvironment3D::Initialize(std::uint64_t seed) {
  if (!seed)
    return false;

  std::vector<ShowcaseSceneDescriptor3D> candidate;
  candidate.reserve(160);
  candidate.push_back({ShowcaseSceneKind3D::Terrain,
                       {0, -1, 0},
                       {512, 48, 512},
                       0,
                       0,
                       true,
                       "procedural/terrain-heightfield",
                       "ambientcg/ground037/1k"});

  // 河段是透明水材質描述；實際網格與GPU資源由整合端建立及持有。
  for (int segment = -8; segment <= 8; ++segment) {
    const float z = float(segment) * 28.f;
    const float x = std::sin(z * .018f) * 34.f;
    candidate.push_back({ShowcaseSceneKind3D::Water,
                         {x, -.35f, z},
                         {14.f, .08f, 30.f},
                         std::sin(z * .011f) * .18f,
                         1,
                         false,
                         "procedural/river-segment",
                         "runtime/water-optics"});
  }

  std::uint64_t random = seed;
  for (std::uint32_t index = 0; index < 72; ++index) {
    const float x = -210.f + UnitRandom(random) * 420.f;
    const float z = -210.f + UnitRandom(random) * 420.f;
    const float height = 5.5f + UnitRandom(random) * 8.f;
    const bool detailed = index < 24;
    candidate.push_back({ShowcaseSceneKind3D::Tree,
                         {x, 0, z},
                         {height * .3f, height, height * .3f},
                         UnitRandom(random) * 6.2831853f,
                         detailed ? 0u : 1u,
                         true,
                         detailed ? "poly-haven/quiver_tree_02/quiver_tree_02.aemesh"
                                  : "procedural/tree-low",
                         detailed ? "poly-haven/quiver_tree_02/1k"
                                  : "poly-haven/bark_brown_02/1k"});
  }
  for (std::uint32_t index = 0; index < 48; ++index) {
    const float x = -120.f + UnitRandom(random) * 240.f;
    const float z = -120.f + UnitRandom(random) * 240.f;
    candidate.push_back({ShowcaseSceneKind3D::Grass,
                         {x, 0, z},
                         {.8f + UnitRandom(random), .7f, .8f},
                         UnitRandom(random) * 6.2831853f,
                         index < 16 ? 0u : 1u,
                         true,
                         "procedural/grass-clump",
                         "ambientcg/ground037/1k"});
  }
  for (std::uint32_t index = 0; index < 12; ++index) {
    const float size = 1.2f + UnitRandom(random) * 3.2f;
    candidate.push_back({ShowcaseSceneKind3D::Rock,
                         {-90.f + UnitRandom(random) * 180.f, 0,
                          -90.f + UnitRandom(random) * 180.f},
                         {size, size * .75f, size},
                         UnitRandom(random) * 6.2831853f,
                         index < 5 ? 0u : 1u,
                         true,
                         index < 5 ? "poly-haven/rock_09/rock_09.aemesh"
                                   : "procedural/rock-low",
                         index < 5 ? "poly-haven/rock_09/1k"
                                   : "poly-haven/aerial_ground_rock/1k"});
  }
  constexpr std::array<Aevrynd::Vec3, 6> buildingPositions = {
      Aevrynd::Vec3{6, 0, 6}, Aevrynd::Vec3{6, .4f, 3.5f},
      Aevrynd::Vec3{6, .4f, 8.5f}, Aevrynd::Vec3{3.5f, .4f, 6},
      Aevrynd::Vec3{8.5f, .4f, 6}, Aevrynd::Vec3{6, 3.4f, 6}};
  for (std::size_t index = 0; index < buildingPositions.size(); ++index)
    candidate.push_back({ShowcaseSceneKind3D::Building,
                         buildingPositions[index],
                         index == 0 ? Aevrynd::Vec3{7, .4f, 7}
                                    : index == 5 ? Aevrynd::Vec3{7, 1.4f, 7}
                                                 : Aevrynd::Vec3{.35f, 3, 6},
                         index == 3 || index == 4 ? 1.5707963f : 0.f,
                         0,
                         true,
                         index == 0   ? "procedural/foundation"
                         : index == 5 ? "procedural/roof"
                                      : "procedural/wall",
                         "ambientcg/woodsiding006/1k"});

  descriptors_.swap(candidate);
  seed_ = seed;
  initialized_ = true;
  return true;
}

void GameplayShowcaseEnvironment3D::ApplyUltraProfile(
    Aevrynd::DrawList &draw) const {
  draw.SetDirectionalShadowMapEnabled(true);
  draw.SetDirectionalShadowCascades({4, 2048, .1f, 220.f, .7f, .1f});
  draw.SetSoftDirectionalShadows({true, 24.f, 128.f, 36.f});
  draw.SetContactShadows({true, 16, 1.8f, .045f, .85f});
  draw.SetAmbientOcclusion({true, 1.5f, .03f, 1.15f});
  draw.SetScreenSpaceReflections({true, .82f, 42.f, .7f, .32f, 16});
  draw.SetBloom({true, 1.05f, .55f, .13f, 3.f});
  draw.SetVolumetricClouds(
      {true, 1100.f, 1700.f, .48f, 1.08f, .28f, 14.f, 16, true, .7f});
  draw.SetDirectionalLightShafts({true, .68f, .72f, 360.f, 6});
  draw.SetTerrainDistanceMaterial(
      {true, 44.f, 196.f, .0065f, .16f, 1.f, .14f});
  draw.SetWaterOptics({true, .022f, .42f, .04f});
}

ShowcaseLightingState3D GameplayShowcaseEnvironment3D::EvaluateLighting(
    ShowcaseLightingProfile3D profile) const {
  ShowcaseLightingState3D state{};
  state.profile = profile;
  if (profile == ShowcaseLightingProfile3D::Day) {
    state = {profile, {{.34f, -.91f, .23f}, .47f, {1, .96f, .88f, 1}, 1.57f},
             {.28f, .48f, .74f, 1}, {.38f, .5f, .63f, .68f}, -.3f, 46800.f};
  } else if (profile == ShowcaseLightingProfile3D::Dusk) {
    state = {profile, {{-.74f, -.42f, .18f}, .47f, {1, .66f, .38f, 1}, .82f},
             {.22f, .16f, .25f, 1}, {.34f, .22f, .28f, .72f}, -.05f, 68400.f};
  } else {
    state = {profile, {{-.25f, -.62f, -.74f}, .47f, {.42f, .55f, 1, 1}, .35f},
             {.025f, .045f, .09f, 1}, {.12f, .18f, .3f, .76f}, .45f, 79200.f};
  }
  return state;
}

void GameplayShowcaseEnvironment3D::ApplyLighting(
    Aevrynd::DrawList &draw, ShowcaseLightingProfile3D profile,
    Aevrynd::Vec3 cameraPosition) const {
  const ShowcaseLightingState3D state = EvaluateLighting(profile);
  draw.SetDirectionalLight(state.sun);
  draw.SetAtmosphere({state.fog, cameraPosition, 54, 230, true, .0035f, .09f,
                      -2.f, .76f});
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
  receipt.seed = seed_;
  receipt.worldHash = WorldHash();
  receipt.retainedDescriptorBytes = RetainedDescriptorBytes();
  for (const ShowcaseSceneDescriptor3D &descriptor : descriptors_) {
    switch (descriptor.kind) {
    case ShowcaseSceneKind3D::Terrain: ++receipt.terrain; break;
    case ShowcaseSceneKind3D::Water: ++receipt.water; break;
    case ShowcaseSceneKind3D::Tree:
    case ShowcaseSceneKind3D::Grass: ++receipt.vegetation; break;
    case ShowcaseSceneKind3D::Rock: ++receipt.rocks; break;
    case ShowcaseSceneKind3D::Building: ++receipt.buildings; break;
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
