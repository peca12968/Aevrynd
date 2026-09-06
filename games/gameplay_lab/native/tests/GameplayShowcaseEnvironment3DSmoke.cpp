#include "GameplayShowcaseEnvironment3D.h"
#define AEVRYND_CHARACTER_BUILD_FINGERPRINT "environment-test"
#include "GameplayLabGame.h"
#undef AEVRYND_CHARACTER_BUILD_FINGERPRINT

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

using namespace Aevrynd;
using namespace GameplayLab;

namespace {
struct LightingRouteEvidence3D {
  ShowcaseLightingState3D typed{};
  DirectionalLight3D submittedSun{};
  VolumetricAtmospherePhase3D volumePhase{};
  EnvironmentLightingBlendSettings3D environment{};
  std::uint64_t radianceFingerprint{};
  PhaseRadianceProfile3D radiance{};
};

bool SameSun(const DirectionalLight3D &left,
             const DirectionalLight3D &right) {
  const Vec3 leftDirection = Normalize(left.direction);
  const Vec3 rightDirection = Normalize(right.direction);
  return std::abs(leftDirection.x - rightDirection.x) <= 1e-6f &&
         std::abs(leftDirection.y - rightDirection.y) <= 1e-6f &&
         std::abs(leftDirection.z - rightDirection.z) <= 1e-6f &&
         left.ambient == right.ambient && left.color.r == right.color.r &&
         left.color.g == right.color.g && left.color.b == right.color.b &&
         left.color.a == right.color.a && left.intensity == right.intensity;
}

bool ValidateLightingRoutes(
    const std::array<LightingRouteEvidence3D, 3> &expected,
    const std::array<LightingRouteEvidence3D, 3> &observed) {
  constexpr std::array<ShowcaseLightingProfile3D, 3> profiles{
      ShowcaseLightingProfile3D::Day, ShowcaseLightingProfile3D::Dusk,
      ShowcaseLightingProfile3D::Night};
  constexpr std::array<VolumetricAtmospherePhase3D, 3> volumePhases{
      VolumetricAtmospherePhase3D::Day, VolumetricAtmospherePhase3D::Dusk,
      VolumetricAtmospherePhase3D::Night};
  if (!(observed[0].typed.sun.ambient > observed[1].typed.sun.ambient &&
        observed[1].typed.sun.ambient > observed[2].typed.sun.ambient &&
        observed[0].environment.diffuseIntensity > observed[1].environment.diffuseIntensity &&
        observed[1].environment.diffuseIntensity > observed[2].environment.diffuseIntensity &&
        observed[0].radiance.analyticDiffuseIntensity > observed[1].radiance.analyticDiffuseIntensity &&
        observed[1].radiance.analyticDiffuseIntensity > observed[2].radiance.analyticDiffuseIntensity))
    return false;
  for (std::size_t index = 0; index < observed.size(); ++index) {
    if (observed[index].typed.profile != profiles[index] ||
        observed[index].volumePhase != volumePhases[index] ||
        !SameSun(observed[index].typed.sun, observed[index].submittedSun) ||
        !SameSun(expected[index].typed.sun, observed[index].typed.sun) ||
        observed[index].environment != expected[index].environment ||
        observed[index].radianceFingerprint !=
            expected[index].radianceFingerprint ||
        observed[index].radiance.analyticDiffuseIntensity !=
            expected[index].radiance.analyticDiffuseIntensity)
      return false;
  }
  return true;
}
} // namespace

int main() {
  GameplayShowcaseEnvironment3D first, second, differentSeed;
  if (first.Initialize(0) || !first.Initialize(0x4a17u) ||
      !second.Initialize(0x4a17u) || !differentSeed.Initialize(0x4a18u))
    return 1;

  const auto receipt = first.Receipt();
  const auto repeated = second.Receipt();
  const auto targets = GameplayShowcaseEnvironment3D::Targets();
  if (receipt.worldHash == 0 || receipt.worldHash != repeated.worldHash ||
      receipt.terrain != 1 || receipt.water != 1 ||
      receipt.vegetation != 426 || receipt.rocks != 30 ||
      receipt.buildings != 6 || receipt.trees != targets.treeInstances ||
      receipt.grass != targets.grassInstances ||
      receipt.nearVegetation != targets.nearVegetation ||
      receipt.midVegetation != targets.midVegetation ||
      receipt.farVegetation != targets.farVegetation ||
      receipt.riverSurfaceAreaSquareMeters <
          targets.minimumRiverSurfaceAreaSquareMeters ||
      receipt.buildingComponentMask != targets.requiredBuildingComponentMask ||
      receipt.materialIdentityHash == 0 ||
      receipt.materialIdentityHash != targets.materialIdentityHash ||
      receipt.licenseInventoryHash == 0 ||
      receipt.licenseInventoryHash != targets.licenseInventoryHash ||
      receipt.riverStripSlices != 64 || receipt.riverStripVertices != 130 ||
      receipt.riverStripIndices != 384 || !receipt.riverUvMonotonic ||
      receipt.riverMaximumJoinError > 0.00001f ||
      receipt.integratedDescriptors != 464 || receipt.descriptorCapacity != 480 ||
      receipt.retainedDescriptorBytes == 0 || !receipt.landFingerprint ||
      !receipt.terrainZoneFingerprint ||
      receipt.terrainZoneFingerprint != repeated.terrainZoneFingerprint ||
      receipt.terrainZoneFingerprint ==
          differentSeed.Receipt().terrainZoneFingerprint ||
      receipt.terrainMaterialLayers != targets.requiredTerrainMaterialLayers ||
      receipt.riverbankProps < targets.minimumRiverbankProps ||
      receipt.densestHabitatCell < targets.minimumDensestHabitatCell ||
      !receipt.terrainZonesExact || !receipt.riverbankLayered ||
      !receipt.propFootprintsNonOverlapping)
    return 2;
  std::array<std::size_t, 3> terrainLayers{};
  for (const auto &descriptor : first.Descriptors()) {
    if (descriptor.materialAsset.find("forest_ground_04") != std::string::npos)
      ++terrainLayers[0];
    else if (descriptor.materialAsset.find("Ground108") != std::string::npos)
      ++terrainLayers[1];
    else if (descriptor.materialAsset.find("stony_dirt_path") != std::string::npos)
      ++terrainLayers[2];
  }
  if (terrainLayers != std::array<std::size_t, 3>{1, 188, 94}) return 32;

  DrawList draw{DrawListCapacityProfile::Minimal2D};
  first.ApplyHighProfile(draw);
  first.ApplyLighting(draw, ShowcaseLightingProfile3D::Day, {1, 2, 3});
  const auto dayAtmosphere = draw.Atmosphere();
  const auto dayEnvironmentBlend = draw.EnvironmentLightingBlend();
  const auto daySubmittedSun = draw.DirectionalLight();
  const auto dayRadianceFingerprint = draw.PhaseRadianceProfile().phaseFingerprint;
  const auto dayRadiance = draw.PhaseRadianceProfile();
  if (draw.VolumetricAtmospherePhase() != VolumetricAtmospherePhase3D::Day)
    return 20;
  if (!draw.DirectionalShadowMapEnabled() ||
      draw.DirectionalShadowCascades().cascadeCount != 4 ||
      !draw.SoftDirectionalShadows().enabled ||
      !draw.ContactShadows().enabled || !draw.AmbientOcclusion().enabled ||
      !draw.ScreenSpaceReflections().enabled || !draw.Bloom().enabled ||
      !draw.VolumetricClouds().enabled ||
      !draw.VolumetricClouds().shadowsEnabled ||
      !draw.DirectionalLightShafts().enabled ||
      !draw.TerrainDistanceMaterial().enabled ||
      !draw.WaterOptics().screenSpaceRefraction ||
      !draw.WaterSurfaceReceipt().enabledRequested ||
      draw.WaterSurfaceReceipt().fingerprint != receipt.waterSurfaceFingerprint)
    return 3;

  const auto day = first.EvaluateLighting(ShowcaseLightingProfile3D::Day);
  const auto dusk = first.EvaluateLighting(ShowcaseLightingProfile3D::Dusk);
  const auto night = first.EvaluateLighting(ShowcaseLightingProfile3D::Night);
  first.ApplyLighting(draw, ShowcaseLightingProfile3D::Dusk, {1, 2, 3});
  const auto duskAtmosphere = draw.Atmosphere();
  const auto duskEnvironmentBlend = draw.EnvironmentLightingBlend();
  const auto duskSubmittedSun = draw.DirectionalLight();
  const auto duskRadianceFingerprint = draw.PhaseRadianceProfile().phaseFingerprint;
  const auto duskRadiance = draw.PhaseRadianceProfile();
  if (draw.VolumetricAtmospherePhase() != VolumetricAtmospherePhase3D::Dusk)
    return 21;
  first.ApplyLighting(draw, ShowcaseLightingProfile3D::Night, {1, 2, 3});
  const auto nightAtmosphere = draw.Atmosphere();
  const auto nightEnvironmentBlend = draw.EnvironmentLightingBlend();
  const auto nightSubmittedSun = draw.DirectionalLight();
  const auto nightRadianceFingerprint = draw.PhaseRadianceProfile().phaseFingerprint;
  const auto nightRadiance = draw.PhaseRadianceProfile();
  if (draw.VolumetricAtmospherePhase() != VolumetricAtmospherePhase3D::Night)
    return 22;
  if (!dayAtmosphere.aerialPerspective.enabled ||
      dayAtmosphere.aerialPerspective.nearDistance < 32.f ||
      duskAtmosphere.aerialPerspective.skyRadiance.r <=
          duskAtmosphere.aerialPerspective.skyRadiance.b * .8f ||
      nightAtmosphere.aerialPerspective.skyRadiance.b <=
          nightAtmosphere.aerialPerspective.skyRadiance.r * 2.f ||
      dayAtmosphere.aerialPerspective.scattering >
          dayAtmosphere.aerialPerspective.extinction ||
      duskAtmosphere.aerialPerspective.scattering >
          duskAtmosphere.aerialPerspective.extinction ||
      nightAtmosphere.aerialPerspective.scattering >
          nightAtmosphere.aerialPerspective.extinction)
    return 23;
  if (dayEnvironmentBlend.phaseFingerprint ==
          duskEnvironmentBlend.phaseFingerprint ||
      duskEnvironmentBlend.phaseFingerprint ==
          nightEnvironmentBlend.phaseFingerprint ||
      dayEnvironmentBlend.iblDiffuseWeight <=
          duskEnvironmentBlend.iblDiffuseWeight ||
      duskEnvironmentBlend.iblDiffuseWeight <=
          nightEnvironmentBlend.iblDiffuseWeight ||
      dayEnvironmentBlend.analyticDiffuseWeight >=
          duskEnvironmentBlend.analyticDiffuseWeight ||
      duskEnvironmentBlend.analyticDiffuseWeight >=
          nightEnvironmentBlend.analyticDiffuseWeight ||
      std::abs(dayEnvironmentBlend.analyticDiffuseWeight +
                   dayEnvironmentBlend.iblDiffuseWeight -
               1.f) > 1e-6f ||
      std::abs(duskEnvironmentBlend.analyticSpecularWeight +
                   duskEnvironmentBlend.iblSpecularWeight -
               1.f) > 1e-6f)
    return 24;
  const std::array<LightingRouteEvidence3D, 3> productionLighting{{
      {day, daySubmittedSun, VolumetricAtmospherePhase3D::Day,
       dayEnvironmentBlend, dayRadianceFingerprint, dayRadiance},
      {dusk, duskSubmittedSun, VolumetricAtmospherePhase3D::Dusk,
       duskEnvironmentBlend, duskRadianceFingerprint, duskRadiance},
      {night, nightSubmittedSun, VolumetricAtmospherePhase3D::Night,
       nightEnvironmentBlend, nightRadianceFingerprint, nightRadiance}}};
  if (!ValidateLightingRoutes(productionLighting, productionLighting)) return 25;
  auto legacyAmbient = productionLighting;
  for (auto &evidence : legacyAmbient) evidence.submittedSun.ambient = .47f;
  if (ValidateLightingRoutes(productionLighting, legacyAmbient)) return 26;
  auto legacyNightFill = productionLighting;
  legacyNightFill[2].environment.diffuseIntensity = .32f;
  legacyNightFill[2].radiance.analyticDiffuseIntensity = .16f;
  if (ValidateLightingRoutes(productionLighting, legacyNightFill)) return 31;
  auto swappedPhase = productionLighting;
  std::swap(swappedPhase[0], swappedPhase[1]);
  if (ValidateLightingRoutes(productionLighting, swappedPhase)) return 27;
  if (!(day.sun.intensity > dusk.sun.intensity &&
        dusk.sun.intensity > night.sun.intensity &&
        day.environmentTime < dusk.environmentTime &&
        dusk.environmentTime < night.environmentTime))
    return 4;

  const auto luminanceTarget = [](const ShowcaseLightingState3D &state) {
    const auto luminance = [](Color value) {
      return .2126f * value.r + .7152f * value.g + .0722f * value.b;
    };
    return (luminance(state.sky) +
            .1f * state.sun.intensity * luminance(state.sun.color)) *
           std::exp2(state.exposureEv);
  };
  const float dayLuma = luminanceTarget(day), duskLuma = luminanceTarget(dusk),
              nightLuma = luminanceTarget(night);
  if (!(dayLuma > duskLuma && duskLuma > nightLuma && nightLuma >= .075f))
    return 8;

  constexpr CameraProjection productionPerspective{
      ProjectionMode::Perspective, 1.0471976f, 20.f, .05f, 320.f};
  // 11景使用正式角色錨點與選擇器的視角族群，不改動角色或相機去追物件。
  std::array<Camera3D, 11> productionCameras{{
      {{-16, 1.68f, 8.73f}, {-16, 1.68f, 22}, {0, 1, 0}, productionPerspective},
      {{-15.76f, 1.86f, 5.64f}, {-16, 1.05f, 8.4f}, {0, 1, 0}, productionPerspective},
      {{-32, 23, -8}, {-16, 1, 8}, {0, 1, 0}, productionPerspective},
      {{-16, 1.68f, 8.73f}, {-16, 1.68f, 22}, {0, 1, 0}, productionPerspective},
      {{-15.72f, 2.27f, 5.86f}, {-16, 1.05f, 8.4f}, {0, 1, 0}, productionPerspective},
      {{-32, 23, -8}, {-16, 1, 8}, {0, 1, 0}, productionPerspective},
      {{-16, 1.68f, 8.73f}, {-16, 1.68f, 22}, {0, 1, 0}, productionPerspective},
      {{-14.74f, 1.50f, 5.23f}, {-16, 1.05f, 8.4f}, {0, 1, 0}, productionPerspective},
      {{-32, 23, -8}, {-16, 1, 8}, {0, 1, 0}, productionPerspective},
      {{-14.79f, 1.51f, 5.17f}, {-16, 1.05f, 8.4f}, {0, 1, 0}, productionPerspective},
      {{-16.25f, 1.22f, 5.20f}, {-16, 1.05f, 8.4f}, {0, 1, 0}, productionPerspective}}};
  for (std::size_t index = 0; index < productionCameras.size(); ++index) {
    const auto route = BuildGameplayCaptureProductionRoute3D(
        GameplayCaptureExpectations3D[index]);
    productionCameras[index].projection.orthographicHeight = 20.f;
    if (!ApplyGameplayCaptureProductionProjection3D(productionCameras[index], route) ||
        productionCameras[index].projection.mode != route.projection)
      return 28;
  }
  auto wrongGodRoute = BuildGameplayCaptureProductionRoute3D(
      GameplayCaptureExpectations3D[2]);
  wrongGodRoute.projection = ProjectionMode::Perspective;
  if (ValidateGameplayCaptureProductionRoute3D(wrongGodRoute)) return 29;
  auto wrongFirstRoute = BuildGameplayCaptureProductionRoute3D(
      GameplayCaptureExpectations3D[0]);
  wrongFirstRoute.projection = ProjectionMode::Orthographic;
  if (ValidateGameplayCaptureProductionRoute3D(wrongFirstRoute)) return 30;
  constexpr std::array<bool, 11> requireHeroCell{
      false, true, true, false, true, true, false, true, true, true, true};
  std::size_t transitionTargets{};
  float minimumRiverCoverage = 1.f, minimumBuildingCoverage = 1.f;
  std::uint32_t minimumRiverSegments = 17, minimumBuildingComponents = 6;
  for (std::size_t index = 0; index < productionCameras.size(); ++index) {
    const auto visibility = first.EvaluateCameraVisibility(productionCameras[index]);
    if (!std::isfinite(visibility.riverCoverage) ||
        !std::isfinite(visibility.buildingCoverage) ||
        (requireHeroCell[index] && !visibility.Meets(true, true))) {
      std::cerr << "visibility index=" << index
                << " river=" << visibility.visibleRiverSegments << '/'
                << visibility.riverCoverage << " building="
                << visibility.visibleBuildingComponents << '/'
                << visibility.buildingCoverage << " edge="
                << visibility.buildingTouchesEdge << " mask="
                << visibility.buildingEdgeMask << '\n';
      return 9;
    }
    if (requireHeroCell[index]) {
      minimumRiverCoverage = (std::min)(minimumRiverCoverage, visibility.riverCoverage);
      minimumBuildingCoverage = (std::min)(minimumBuildingCoverage, visibility.buildingCoverage);
      minimumRiverSegments = (std::min)(minimumRiverSegments, visibility.visibleRiverSegments);
      minimumBuildingComponents = (std::min)(minimumBuildingComponents,
                                               visibility.visibleBuildingComponents);
    }
    const auto samples = first.EvaluateLodSamples(productionCameras[index].position);
    if (samples.size() != receipt.trees + receipt.grass + receipt.rocks)
      return 10;
    for (const auto &sample : samples) {
      if (!sample.stableLow || !std::isfinite(sample.centerDistance) ||
          !std::isfinite(sample.surfaceDistance) ||
          !std::isfinite(sample.nearCoverage) ||
          !std::isfinite(sample.farCoverage) ||
          std::abs(sample.nearCoverage + sample.farCoverage - 1.f) > .0001f)
        return 11;
      transitionTargets += sample.transitioning ? 1u : 0u;
    }
  }
  if (!transitionTargets)
    return 12;

  const Vec3 lodCamera = productionCameras[2].position;
  const auto lodA = first.EvaluateLodSamples(lodCamera);
  const auto lodB = first.EvaluateLodSamples(lodCamera + Vec3{.001f, 0, 0});
  for (std::size_t index = 0; index < lodA.size(); ++index) {
    const auto &a = lodA[index], &b = lodB[index];
    if (a.stableLow != b.stableLow || a.kind != b.kind ||
        a.nearLevel != b.nearLevel || a.farLevel != b.farLevel ||
        a.switchDistance != b.switchDistance ||
        a.transitionWidth != b.transitionWidth ||
        std::abs(a.nearCoverage - b.nearCoverage) > .001f ||
        std::abs(a.farCoverage - b.farCoverage) > .001f)
      return 13;
  }

  std::vector<ShowcaseSubmittedLodIdentity3D> submitted;
  submitted.reserve(lodA.size());
  for (const auto &sample : lodA)
    submitted.push_back({sample.stableLow, 1, 0, sample.kind});
  const auto expectedRows = BuildShowcaseLodRenderRows3D(lodA, submitted);
  if (expectedRows.size() != submitted.size() ||
      !CompareShowcaseLodRenderRows3D(expectedRows, expectedRows).Exact())
    return 14;
  auto missingRows = expectedRows;
  missingRows.pop_back();
  const auto missingContract = CompareShowcaseLodRenderRows3D(expectedRows, missingRows);
  auto extraRows = expectedRows;
  auto extraRow = expectedRows.front();
  extraRow.identity.submesh = 99;
  extraRows.push_back(extraRow);
  const auto extraContract = CompareShowcaseLodRenderRows3D(expectedRows, extraRows);
  auto wrongLevelRows = expectedRows;
  ++wrongLevelRows.front().nearLevel;
  const auto wrongLevelContract = CompareShowcaseLodRenderRows3D(expectedRows, wrongLevelRows);
  auto wrongBlendRows = expectedRows;
  wrongBlendRows.front().nearCoverage -= .1f;
  wrongBlendRows.front().farCoverage += .1f;
  const auto wrongBlendContract = CompareShowcaseLodRenderRows3D(expectedRows, wrongBlendRows);
  if (missingContract.Exact() || !missingContract.missing || extraContract.Exact() ||
      !extraContract.extra || wrongLevelContract.Exact() || !wrongLevelContract.wrongLevel ||
      wrongBlendContract.Exact() || !wrongBlendContract.wrongBlend)
    return 15;

  const Camera3D exactCamera = productionCameras[2];
  const Vec3 exactForward = Normalize(exactCamera.target - exactCamera.position);
  ShowcaseSubmittedLodIdentity3D exactSubmission{};
  exactSubmission.stableLow = 9001;
  exactSubmission.generation = 7;
  exactSubmission.submesh = 2;
  exactSubmission.kind = ShowcaseSceneKind3D::Tree;
  exactSubmission.worldCenter = exactCamera.position + exactForward * 24.f;
  exactSubmission.worldRadius = 2.f;
  exactSubmission.switchDistances = {22.f, 48.f, 96.f};
  exactSubmission.transitionWidths = {5.f, 6.f, 10.f};
  exactSubmission.levelCount = 4;
  exactSubmission.lodSeedBase = 6;
  const auto exactRows = BuildShowcaseLodRenderRows3D(
      exactCamera, std::vector<ShowcaseSubmittedLodIdentity3D>{exactSubmission},
      {1280, 720}, 150.f, .1f);
  if (exactRows.size() != 1 || !exactRows.front().transitioning ||
      exactRows.front().nearLevel != 0 || exactRows.front().farLevel != 1 ||
      exactRows.front().lodSeed != 6 ||
      std::abs(exactRows.front().centerDistance - 24.f) > .0001f ||
      std::abs(exactRows.front().surfaceDistance - 22.f) > .0001f ||
      std::abs(exactRows.front().nearCoverage - .5f) > .0001f ||
      !CompareShowcaseLodRenderRows3D(exactRows, exactRows).Exact())
    return 16;
  auto wrongSeedRows = exactRows;
  wrongSeedRows.front().lodSeed ^= 1u;
  auto wrongBoundsRows = exactRows;
  wrongBoundsRows.front().boundsRadius += .25f;
  auto exactWrongBlendRows = exactRows;
  exactWrongBlendRows.front().nearCoverage += .01f;
  exactWrongBlendRows.front().farCoverage -= .01f;
  if (!CompareShowcaseLodRenderRows3D(exactRows, wrongSeedRows).wrongBlend ||
      !CompareShowcaseLodRenderRows3D(exactRows, wrongBoundsRows).wrongBlend ||
      !CompareShowcaseLodRenderRows3D(exactRows, exactWrongBlendRows).wrongBlend)
    return 17;
  std::array<MeshInstance3D, 2> submittedDraws{};
  for (auto &instance : submittedDraws) {
    instance.identity = {{0xabcdu, exactSubmission.stableLow}, exactSubmission.generation};
    instance.lodSeed = exactRows.front().lodSeed;
    instance.lodCoverage = exactRows.front().farCoverage;
  }
  submittedDraws[0].lodSeed |= 8u;
  const auto actualExactRows = BuildShowcaseActualLodRows3D(
      exactRows, submittedDraws, 0xabcdu);
  if (!CompareShowcaseLodRenderRows3D(exactRows, actualExactRows).Exact())
    return 18;
  auto missingSeedDraws = submittedDraws;
  missingSeedDraws[1].lodSeed ^= 1u;
  const auto actualMissingSeedRows = BuildShowcaseActualLodRows3D(
      exactRows, missingSeedDraws, 0xabcdu);
  auto wrongActualBlendDraws = submittedDraws;
  wrongActualBlendDraws[1].lodCoverage += .05f;
  const auto actualWrongBlendRows = BuildShowcaseActualLodRows3D(
      exactRows, wrongActualBlendDraws, 0xabcdu);
  if (!CompareShowcaseLodRenderRows3D(exactRows, actualMissingSeedRows).missing ||
      !CompareShowcaseLodRenderRows3D(exactRows, actualWrongBlendRows).wrongBlend)
    return 19;
  std::vector<MeshInstance3D> collidingDraws(submittedDraws.begin(), submittedDraws.end());
  auto collidingFar = submittedDraws[1];
  collidingFar.lodCoverage = 1.f;
  collidingDraws.insert(collidingDraws.begin() + 1, collidingFar);
  const auto actualCollidingRows = BuildShowcaseActualLodRows3D(
      exactRows, collidingDraws, 0xabcdu);
  if (!CompareShowcaseLodRenderRows3D(exactRows, actualCollidingRows).Exact())
    return 20;

  RendererStats before{}, after{};
  for (std::size_t index = 0; index < 4; ++index)
    after.lastDirectionalShadowCascadeRendered[index] = 1;
  after.contactShadowEvaluationPasses = 1;
  after.ambientOcclusionEvaluationPasses = 1;
  after.screenSpaceReflectionPasses = 1;
  after.bloomPasses = 1;
  after.volumetricCloudConfiguredFrames = 1;
  after.volumetricCloudShadowConfiguredFrames = 1;
  after.directionalLightShaftConfiguredFrames = 1;
  after.screenSpaceWaterRefractionPasses = 1;
  after.depthAwareWaterPasses = 1;
  const auto execution = first.ExecutionReceipt(before, after);
  if (!execution.executed || execution.deltas[0] != 4)
    return 5;
  if (first.ExecutionReceipt(after, before).executed ||
      first.ExecutionReceipt(after, after).executed)
    return 6;

  const auto released = first.Release();
  if (released.worldHashBefore != receipt.worldHash ||
      released.descriptorsReleased != receipt.integratedDescriptors ||
      released.retainedBytesBefore == 0 || released.retainedBytesAfter != 0 ||
      first.Initialized() || !first.Descriptors().empty())
    return 7;

  std::cout << "worldHash=" << receipt.worldHash
            << " descriptors=" << released.descriptorsReleased
            << " releasedBytes=" << released.retainedBytesBefore
            << " dayLuma=" << dayLuma << " duskLuma=" << duskLuma
            << " nightLuma=" << nightLuma
            << " lodTransitions=" << transitionTargets
            << " minRiver=" << minimumRiverSegments << '/'
            << minimumRiverCoverage << " minBuilding="
            << minimumBuildingComponents << '/' << minimumBuildingCoverage
            << " terrainLayers=" << receipt.terrainMaterialLayers
            << " riverbankProps=" << receipt.riverbankProps
            << " habitat=" << receipt.occupiedHabitatCells << '/'
            << receipt.densestHabitatCell
            << '\n';
}
