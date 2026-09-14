#include "GameplayShowcaseEnvironment3D.h"
#define AEVRYND_CHARACTER_BUILD_FINGERPRINT "environment-test"
#include "GameplayLabGame.h"
#undef AEVRYND_CHARACTER_BUILD_FINGERPRINT

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <utility>

using namespace Aevrynd;
using namespace GameplayLab;

static_assert(!noexcept(ResolveShowcaseTerrainBiomeUvTriangleConflicts3D(
    std::declval<Mesh3D &>())));
static_assert(!noexcept(ApplyShowcaseTerrainBiomeUv3D(
    std::declval<Mesh3D &>(), std::size_t{},
    std::span<const ShowcaseSceneDescriptor3D>{})));

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

float Rec709(const Color &value) {
  return .2126f * value.r + .7152f * value.g + .0722f * value.b;
}

float Chroma(const Color &value) {
  return (std::max)({value.r, value.g, value.b}) -
         (std::min)({value.r, value.g, value.b});
}

bool TreeDescriptorDistributionContract(
    const std::vector<ShowcaseSceneDescriptor3D> &descriptors) {
  std::array<unsigned, 3> islandByBand{}, smallByBand{}, deadByBand{};
  unsigned resolvedTrees{};
  for (const auto &descriptor : descriptors) {
    if (descriptor.kind != ShowcaseSceneKind3D::Tree || descriptor.lodGroup >= 3)
      continue;
    auto *counts = descriptor.meshAsset == "poly-haven/island_tree_01/cooked-lod"
                       ? &islandByBand
                   : descriptor.meshAsset == "poly-haven/tree_small_02/cooked-lod"
                       ? &smallByBand
                   : descriptor.meshAsset == "poly-haven/dead_tree_trunk_02/cooked-lod"
                       ? &deadByBand
                       : nullptr;
    if (!counts)
      return false;
    ++resolvedTrees;
    ++(*counts)[descriptor.lodGroup];
  }
  return resolvedTrees == 240 &&
         islandByBand == std::array<unsigned, 3>{83, 59, 18} &&
         smallByBand == std::array<unsigned, 3>{31, 22, 7} &&
         deadByBand == std::array<unsigned, 3>{10, 7, 3};
}

bool TreeFormVariationContract(
    const std::vector<ShowcaseLandPlacement3D> &placements,
    const std::vector<ShowcaseSceneDescriptor3D> &descriptors) {
  if (descriptors.size() != placements.size() + 8u) return false;
  std::array<std::array<float, 2>, 3> heightRange{{
      {{(std::numeric_limits<float>::max)(), 0.f}},
      {{(std::numeric_limits<float>::max)(), 0.f}},
      {{(std::numeric_limits<float>::max)(), 0.f}}}};
  std::array<bool, 3> sawVariant{};
  std::array<bool, 3> sawBand{};
  std::array<bool, 32> silhouetteBuckets{};
  unsigned trees{};
  for (std::size_t index = 0; index < placements.size(); ++index) {
    const auto &placement = placements[index];
    if (placement.kind != ShowcaseLandKind3D::Tree) continue;
    const auto form = ShowcaseTreeFormVariation(placement);
    const auto repeated = ShowcaseTreeFormVariation(placement);
    const auto &descriptor = descriptors[index + 2u];
    if (form.scaleMultiplier.x != repeated.scaleMultiplier.x ||
        form.scaleMultiplier.y != repeated.scaleMultiplier.y ||
        form.scaleMultiplier.z != repeated.scaleMultiplier.z ||
        form.yawOffset != repeated.yawOffset ||
        form.scaleMultiplier.x < .58f || form.scaleMultiplier.x > 1.f ||
        form.scaleMultiplier.z < .51f || form.scaleMultiplier.z > 1.f ||
        form.scaleMultiplier.y < .70f || form.scaleMultiplier.y > 1.1f ||
        std::abs(form.yawOffset) > .211f ||
        descriptor.position.x != placement.position.x ||
        descriptor.position.y != placement.position.y ||
        descriptor.position.z != placement.position.z ||
        descriptor.scale.x != placement.scale.x * form.scaleMultiplier.x ||
        descriptor.scale.y != placement.scale.y * form.scaleMultiplier.y ||
        descriptor.scale.z != placement.scale.z * form.scaleMultiplier.z ||
        descriptor.yaw != placement.yaw + form.yawOffset)
      return false;
    const auto variant = placement.variant == ShowcaseLandVariant3D::IslandTree
                             ? 0u
                         : placement.variant == ShowcaseLandVariant3D::TreeSmall02
                             ? 1u
                         : placement.variant == ShowcaseLandVariant3D::DeadTreeTrunk
                             ? 2u
                             : 3u;
    if (variant >= sawVariant.size()) return false;
    const auto band = std::uint32_t(placement.band);
    if (band >= sawBand.size()) return false;
    sawVariant[variant] = true;
    sawBand[band] = true;
    heightRange[band][0] = (std::min)(heightRange[band][0], form.scaleMultiplier.y);
    heightRange[band][1] = (std::max)(heightRange[band][1], form.scaleMultiplier.y);
    const auto bucket = (std::min)(31u, std::uint32_t(
        form.scaleMultiplier.x * 19.f + form.scaleMultiplier.y * 11.f));
    silhouetteBuckets[bucket] = true;
    ++trees;
  }
  const auto distinct = std::ranges::count(silhouetteBuckets, true);
  return trees == 240 && std::ranges::all_of(sawVariant, [](bool v) { return v; }) &&
         std::ranges::all_of(sawBand, [](bool v) { return v; }) && distinct >= 8 &&
         heightRange[0][1] - heightRange[0][0] >= .20f &&
         heightRange[1][1] - heightRange[1][0] >= .18f &&
         heightRange[2][1] < heightRange[0][1];
}

bool UnderstoryFormVariationContract(
    const std::vector<ShowcaseLandPlacement3D> &placements,
    const std::vector<ShowcaseSceneDescriptor3D> &descriptors) {
  if (descriptors.size() != placements.size() + 8u) return false;
  std::array<unsigned, 3> counts{};
  std::array<float, 3> horizontalCoverageSums{};
  std::array<bool, 3> bands{};
  for (std::size_t index = 0; index < placements.size(); ++index) {
    const auto &placement = placements[index];
    if (placement.kind != ShowcaseLandKind3D::Grass) continue;
    const auto form = ShowcaseUnderstoryFormVariation(placement);
    const auto repeated = ShowcaseUnderstoryFormVariation(placement);
    const auto &descriptor = descriptors[index + 2u];
    if (form.scaleMultiplier.x != repeated.scaleMultiplier.x ||
        form.scaleMultiplier.y != repeated.scaleMultiplier.y ||
        form.scaleMultiplier.z != repeated.scaleMultiplier.z ||
        form.yawOffset != repeated.yawOffset ||
        form.scaleMultiplier.x <= 0.f || form.scaleMultiplier.x > 1.34f ||
        form.scaleMultiplier.z <= 0.f || form.scaleMultiplier.z > 1.34f ||
        form.scaleMultiplier.y < .70f || form.scaleMultiplier.y > 1.62f ||
        descriptor.position.x != placement.position.x ||
        descriptor.position.y != placement.position.y ||
        descriptor.position.z != placement.position.z ||
        descriptor.scale.x != placement.scale.x * form.scaleMultiplier.x ||
        descriptor.scale.y != placement.scale.y * form.scaleMultiplier.y ||
        descriptor.scale.z != placement.scale.z * form.scaleMultiplier.z ||
        descriptor.yaw != placement.yaw + form.yawOffset)
      return false;
    const auto band = std::uint32_t(placement.band);
    if (band >= bands.size()) return false;
    bands[band] = true;
    std::size_t zone{};
    if (placement.zone == ShowcaseLandZone3D::Riverbank) {
      zone = 0;
      const float z = placement.position.z;
      const float tangentYaw = std::atan2(
          1.f, .175f * std::cos(z * .035f) + .022f * std::cos(z * .011f));
      if (std::abs(std::remainder(descriptor.yaw - tangentYaw,
                                  6.2831853071795864769f)) > .121f)
        return false;
    } else if (placement.zone == ShowcaseLandZone3D::Meadow) {
      zone = 1;
    } else if (placement.zone == ShowcaseLandZone3D::Upland) {
      zone = 2;
    } else {
      return false;
    }
    ++counts[zone];
    horizontalCoverageSums[zone] +=
        form.scaleMultiplier.x * form.scaleMultiplier.z;
  }
  if (counts != std::array<unsigned, 3>{62, 62, 62} ||
      !std::ranges::all_of(bands, [](bool value) { return value; }))
    return false;
  const float riverCoverage = horizontalCoverageSums[0] / float(counts[0]);
  const float meadowCoverage = horizontalCoverageSums[1] / float(counts[1]);
  const float uplandCoverage = horizontalCoverageSums[2] / float(counts[2]);
  return riverCoverage >= .86f && meadowCoverage >= 1.f &&
         uplandCoverage > meadowCoverage;
}

template <std::size_t Size>
std::uint64_t InventoryHash(const std::array<std::string_view, Size> &values) {
  std::uint64_t hash = 14695981039346656037ull;
  for (const auto value : values)
    for (const char character : value) {
      hash ^= static_cast<unsigned char>(character);
      hash *= 1099511628211ull;
    }
  return hash;
}

bool ValidateGrassPalette(const std::array<Color, 3> &candidate,
                          const std::array<Color, 3> &baseline) {
  for (std::size_t index = 0; index < candidate.size(); ++index) {
    const float chromaRatio = Chroma(candidate[index]) / Chroma(baseline[index]);
    if (chromaRatio < .85f || chromaRatio > .9f ||
        Rec709(candidate[index]) - Rec709(baseline[index]) < .018f ||
        Rec709(candidate[index]) - Rec709(baseline[index]) > .032f ||
        candidate[index].g <= candidate[index].r ||
        candidate[index].g <= candidate[index].b || Chroma(candidate[index]) < .2f)
      return false;
  }
  if (!(Rec709(candidate[0]) < Rec709(candidate[1]) &&
        Rec709(candidate[1]) < Rec709(candidate[2])))
    return false;
  for (std::size_t left = 0; left < candidate.size(); ++left)
    for (std::size_t right = left + 1; right < candidate.size(); ++right) {
      const float dr = candidate[left].r - candidate[right].r;
      const float dg = candidate[left].g - candidate[right].g;
      const float db = candidate[left].b - candidate[right].b;
      if (dr * dr + dg * dg + db * db < .001f) return false;
    }
  return true;
}

bool ReadableForestWater(const WaterSurfaceSettings3D &value) {
  return value.minimumRoughness >= .035f && value.minimumRoughness <= .06f &&
         value.maximumRoughness >= .12f && value.maximumRoughness <= .16f &&
         value.broadWaveScale >= .12f && value.broadWaveScale <= .16f &&
         value.broadWaveStrength >= .18f && value.broadWaveStrength <= .22f &&
         value.detailWaveScale >= .85f && value.detailWaveScale <= 1.f &&
         value.detailWaveStrength >= .10f && value.detailWaveStrength <= .13f &&
         value.broadWaveStrength > value.detailWaveStrength * 1.5f &&
         value.shallowAlpha >= .62f && value.shallowAlpha <= .72f &&
         value.deepAlpha >= .86f && value.deepAlpha <= .92f &&
         value.deepAlpha - value.shallowAlpha >= .18f &&
         value.maximumNightRadiance >= .018f;
}

bool DuskVegetationReadable(const ShowcaseLightingState3D &value) {
  const float sunLuminance = .2126f * value.sun.color.r +
                             .7152f * value.sun.color.g +
                             .0722f * value.sun.color.b;
  return value.profile == ShowcaseLightingProfile3D::Dusk &&
         value.sun.ambient >= .18f && value.sun.ambient < .24f &&
         value.exposureEv >= .07f && value.exposureEv <= .09f &&
         value.sun.ambient * sunLuminance >= .13f;
}

bool NightForestReadable(const ShowcaseLightingState3D &state,
                         const EnvironmentLightingBlendSettings3D &environment,
                         const PhaseRadianceProfile3D &radiance) {
  const float skyLuminance = .2126f * state.sky.r + .7152f * state.sky.g +
                             .0722f * state.sky.b;
  return state.profile == ShowcaseLightingProfile3D::Night &&
         state.exposureEv <= -.10f && state.sun.ambient >= .108f &&
         state.sun.ambient <= .11f && state.sun.color.b > state.sun.color.g &&
         state.sun.color.g > state.sun.color.r && skyLuminance >= .08f &&
         environment.diffuseIntensity >= .325f &&
         environment.diffuseIntensity <= .34f &&
         environment.iblDiffuseWeight >= .14f &&
         environment.iblDiffuseWeight <= .20f &&
         environment.objectFillIntensity >= .57f &&
         environment.objectFillIntensity <= .60f &&
         radiance.skyIntensity >= .22f && radiance.skyIntensity <= .28f &&
         radiance.analyticDiffuseIntensity >= .22f &&
         radiance.analyticDiffuseIntensity <= .28f &&
         radiance.groundBounce.z > radiance.groundBounce.x;
}

bool NightCabinHighlightsBalanced(const std::vector<PointLight3D> &dusk,
                                  const std::vector<PointLight3D> &night) {
  if (dusk.size() != 3 || night.size() != dusk.size())
    return false;
  float duskIntensity = 0.f, nightIntensity = 0.f;
  for (std::size_t index = 0; index < dusk.size(); ++index) {
    if (night[index].position.x != dusk[index].position.x ||
        night[index].position.y != dusk[index].position.y ||
        night[index].position.z != dusk[index].position.z ||
        night[index].radius != dusk[index].radius ||
        night[index].color.r != dusk[index].color.r ||
        night[index].color.g != dusk[index].color.g ||
        night[index].color.b != dusk[index].color.b)
      return false;
    duskIntensity += dusk[index].intensity;
    nightIntensity += night[index].intensity;
  }
  return nightIntensity >= duskIntensity * 2.1f &&
         nightIntensity <= duskIntensity * 2.4f;
}

bool NightToneCurveBalanced(const TimeOfDayExposureSettings3D &tone,
                            const ShowcaseLightingState3D &night) {
  const float totalNightEv = night.exposureEv + tone.nightExposureEv;
  return tone.enabled && tone.dayExposureEv == .2f &&
         tone.duskExposureEv == .6f && tone.nightExposureEv >= 1.50f &&
         tone.nightExposureEv <= 1.52f && totalNightEv >= 1.35f &&
         totalNightEv <= 1.37f && tone.shadowLift >= .038f &&
         tone.shadowLift <= .042f && tone.toeStrength >= .30f &&
         tone.toeStrength <= .36f && tone.shoulderStrength >= 1.08f &&
         tone.shoulderStrength <= 1.18f;
}
} // namespace

int main() {
  constexpr auto terrainTint = ShowcaseForestTerrainTint3D();
  if (terrainTint.g < terrainTint.r + .1f ||
      terrainTint.g < terrainTint.b || terrainTint.r > .72f ||
      Rec709(terrainTint) < .62f || Rec709(terrainTint) > .69f ||
      terrainTint.a != 1.f)
    return 63;
  const std::array<ShowcaseSceneDescriptor3D, 1> ecologyTrees{{
      {ShowcaseSceneKind3D::Tree, {40, 0, 40}, {12, 10, 10}}}};
  const auto canopyCenter =
      SampleShowcaseTerrainEcologyTint3D({40, 0, 40}, ecologyTrees);
  const auto canopyFar =
      SampleShowcaseTerrainEcologyTint3D({100, 0, 100}, ecologyTrees);
  const auto cottagePath =
      SampleShowcaseTerrainEcologyTint3D({-14, 0, 14}, ecologyTrees);
  const auto cottagePathRepeated =
      SampleShowcaseTerrainEcologyTint3D({-14, 0, 14}, ecologyTrees);
  const auto cottagePathFourMetreGrid =
      SampleShowcaseTerrainEcologyTint3D({-16, 0, 12}, ecologyTrees);
  const auto wetBank =
      SampleShowcaseTerrainEcologyTint3D({4, 0, 0}, {});
  const std::array<ShowcaseSceneDescriptor3D, 1> overlappingTree{{
      {ShowcaseSceneKind3D::Tree, {4, 0, 0}, {12, 10, 10}}}};
  const auto overlappingBank =
      SampleShowcaseTerrainEcologyTint3D({4, 0, 0}, overlappingTree);
  const auto dryGround =
      SampleShowcaseTerrainEcologyTint3D({100, 0, 100}, {});
  const auto invalidEcology = SampleShowcaseTerrainEcologyTint3D(
      {(std::numeric_limits<float>::quiet_NaN)(), 0, 0}, ecologyTrees);
  const auto mutuallyExclusiveAndBounded = [](const auto &sample) {
    const std::array weights{sample.cottagePath, sample.leafLitter,
                             sample.riverWetSoil};
    unsigned active{};
    for (const float weight : weights) {
      if (!std::isfinite(weight) || weight < 0.f || weight > 1.f)
        return false;
      active += weight > 0.f;
    }
    return active <= 1;
  };
  if (canopyCenter.canopy < .99f || canopyCenter.leafLitter < .99f ||
      canopyCenter.biome != ShowcaseTerrainBiome3D::CanopyLeafLitter ||
      canopyFar.canopy != 0.f ||
      Rec709(canopyCenter.tint) >= Rec709(canopyFar.tint) ||
      cottagePath.cottagePath < .99f ||
      cottagePath.biome != ShowcaseTerrainBiome3D::CottagePath ||
      cottagePathFourMetreGrid.cottagePath < .20f ||
      cottagePathFourMetreGrid.biome != ShowcaseTerrainBiome3D::CottagePath ||
      cottagePath.tint.r != cottagePathRepeated.tint.r ||
      cottagePath.tint.g != cottagePathRepeated.tint.g ||
      cottagePath.tint.b != cottagePathRepeated.tint.b ||
      cottagePath.cottagePath != cottagePathRepeated.cottagePath ||
      wetBank.riverMoisture < .6f || wetBank.riverWetSoil < .6f ||
      wetBank.biome != ShowcaseTerrainBiome3D::RiverWetSoil ||
      overlappingBank.biome != ShowcaseTerrainBiome3D::RiverWetSoil ||
      overlappingBank.riverWetSoil < .6f || overlappingBank.leafLitter != 0.f ||
      dryGround.riverMoisture != 0.f ||
      dryGround.biome != ShowcaseTerrainBiome3D::Neutral ||
      Rec709(wetBank.tint) >= Rec709(dryGround.tint) ||
      Chroma(cottagePath.tint) <= Chroma(canopyCenter.tint) ||
      canopyCenter.tint.g <= canopyCenter.tint.r ||
      wetBank.tint.b <= wetBank.tint.r ||
      !mutuallyExclusiveAndBounded(canopyCenter) ||
      !mutuallyExclusiveAndBounded(cottagePath) ||
      !mutuallyExclusiveAndBounded(wetBank) ||
      !mutuallyExclusiveAndBounded(overlappingBank) ||
      !mutuallyExclusiveAndBounded(dryGround) ||
      invalidEcology.tint.r != 1.f || invalidEcology.tint.g != 1.f ||
      invalidEcology.tint.b != 1.f || invalidEcology.tint.a != 1.f ||
      invalidEcology.canopy != 0.f || invalidEcology.riverMoisture != 0.f ||
      invalidEcology.cottagePath != 0.f || invalidEcology.leafLitter != 0.f ||
      invalidEcology.riverWetSoil != 0.f ||
      invalidEcology.biome != ShowcaseTerrainBiome3D::Neutral) {
    std::cerr << "terrain ecology divergence canopy=" << canopyCenter.canopy
              << '/' << canopyCenter.leafLitter << '/'
              << unsigned(canopyCenter.biome) << " path="
              << cottagePath.cottagePath << '/' << unsigned(cottagePath.biome)
              << " wet=" << wetBank.riverMoisture << '/'
              << wetBank.riverWetSoil << '/' << unsigned(wetBank.biome)
              << " overlap=" << overlappingBank.cottagePath << '/'
              << overlappingBank.leafLitter << '/'
              << overlappingBank.riverWetSoil << '/'
              << unsigned(overlappingBank.biome) << " dry="
              << dryGround.cottagePath << '/' << dryGround.leafLitter << '/'
              << dryGround.riverWetSoil << '/' << unsigned(dryGround.biome)
              << '\n';
    return 77;
  }
  const auto nearWeight = [](float left, float right) {
    return std::abs(left - right) <= 1e-6f;
  };
  const auto encodedPath = EncodeShowcaseTerrainBiomeUv3D(cottagePath);
  const auto encodedLeaf = EncodeShowcaseTerrainBiomeUv3D(canopyCenter);
  const auto encodedWet = EncodeShowcaseTerrainBiomeUv3D(wetBank);
  const auto decodedPath = DecodeShowcaseTerrainBiomeUv3D(encodedPath);
  const auto decodedLeaf = DecodeShowcaseTerrainBiomeUv3D(encodedLeaf);
  const auto decodedWet = DecodeShowcaseTerrainBiomeUv3D(encodedWet);
  const auto decodedNeutral = DecodeShowcaseTerrainBiomeUv3D({0, 0});
  const auto hostileWeights =
      NormalizeShowcaseTerrainBiomeWeights3D({2.f, 0.f, .5f});
  const auto nonFiniteEncoding = EncodeShowcaseTerrainBiomeUv3D(
      ShowcaseTerrainBiomeWeights3D{
          .5f, (std::numeric_limits<float>::quiet_NaN)(), 0.f});
  const auto conflictingEncoding =
      EncodeShowcaseTerrainBiomeUv3D(
          ShowcaseTerrainBiomeWeights3D{.5f, 0.f, .2f});
  const auto hostileUv = DecodeShowcaseTerrainBiomeUv3D({2.f, 2.f});
  const auto invalidUv = DecodeShowcaseTerrainBiomeUv3D(
      {(std::numeric_limits<float>::quiet_NaN)(), 1.f});
  if (!(encodedPath.x > 0.f && encodedPath.y == 0.f) ||
      !(encodedWet.x < 0.f && encodedWet.y == 0.f) ||
      !(encodedLeaf.x == 0.f && encodedLeaf.y > 0.f) ||
      !nearWeight(decodedPath.cottagePath, cottagePath.cottagePath) ||
      !nearWeight(decodedLeaf.leafLitter, canopyCenter.leafLitter) ||
      !nearWeight(decodedWet.riverWetSoil, wetBank.riverWetSoil) ||
      decodedPath.leafLitter != 0.f || decodedPath.riverWetSoil != 0.f ||
      decodedLeaf.cottagePath != 0.f || decodedLeaf.riverWetSoil != 0.f ||
      decodedWet.cottagePath != 0.f || decodedWet.leafLitter != 0.f ||
      decodedNeutral != ShowcaseTerrainBiomeWeights3D{} ||
      !nearWeight(hostileWeights.cottagePath, 2.f / 3.f) ||
      hostileWeights.leafLitter != 0.f ||
      !nearWeight(hostileWeights.riverWetSoil, 1.f / 3.f) ||
      nonFiniteEncoding.x != 0.f || nonFiniteEncoding.y != 0.f ||
      conflictingEncoding.x != 0.f || conflictingEncoding.y != 0.f ||
      !nearWeight(hostileUv.cottagePath + hostileUv.leafLitter +
                      hostileUv.riverWetSoil,
                  1.f) ||
      invalidUv != ShowcaseTerrainBiomeWeights3D{}) {
    std::cerr << "terrain biome UV encoding divergence\n";
    return 78;
  }
  const ShowcaseSceneDescriptor3D fallenLog{
      ShowcaseSceneKind3D::Tree, {40, 0, 40}, {8, 6, 7}, .4f, 0, true,
      "poly-haven/dead_tree_trunk_02/cooked-lod",
      "poly-haven/dead_tree_trunk_02/cooked-pbr"};
  const std::array<ShowcaseSceneDescriptor3D, 1> fallenLogs{fallenLog};
  const auto logPoint = [&](float localX, float localZ) {
    const float cosine = std::cos(fallenLog.yaw);
    const float sine = std::sin(fallenLog.yaw);
    return Vec3{fallenLog.position.x + cosine * localX - sine * localZ, 0,
                fallenLog.position.z + sine * localX + cosine * localZ};
  };
  const auto disturbanceCenter = SampleShowcaseTerrainDisturbance3D(
      fallenLog.position, fallenLogs);
  const auto disturbanceRim = SampleShowcaseTerrainDisturbance3D(
      logPoint(4.4f * .72f, 0), fallenLogs);
  const auto disturbanceEdge = SampleShowcaseTerrainDisturbance3D(
      logPoint(4.4f, 0), fallenLogs);
  const auto disturbanceRepeated = SampleShowcaseTerrainDisturbance3D(
      fallenLog.position, fallenLogs);
  auto farLog = fallenLog;
  farLog.lodGroup = 1;
  const std::array<ShowcaseSceneDescriptor3D, 1> farLogs{farLog};
  auto protectedLog = fallenLog;
  protectedLog.position = {-16, 0, 8};
  const std::array<ShowcaseSceneDescriptor3D, 1> protectedLogs{protectedLog};
  const auto protectedDisturbance = SampleShowcaseTerrainDisturbance3D(
      protectedLog.position, protectedLogs);
  const auto farBandDisturbance = SampleShowcaseTerrainDisturbance3D(
      farLog.position, farLogs);
  const auto invalidDisturbance = SampleShowcaseTerrainDisturbance3D(
      {(std::numeric_limits<float>::quiet_NaN)(), 0, 0}, fallenLogs);
  if (disturbanceCenter.weight < .99f ||
      disturbanceCenter.heightDelta < -.0551f ||
      disturbanceCenter.heightDelta > -.0549f ||
      disturbanceCenter.tint.r < .699f || disturbanceCenter.tint.r > .701f ||
      disturbanceCenter.tint.g < .619f || disturbanceCenter.tint.g > .621f ||
      disturbanceCenter.tint.b < .479f || disturbanceCenter.tint.b > .481f ||
      disturbanceRim.heightDelta <= 0.f || disturbanceRim.heightDelta > .025f ||
      disturbanceEdge.weight != 0.f || disturbanceEdge.heightDelta != 0.f ||
      disturbanceCenter.weight != disturbanceRepeated.weight ||
      disturbanceCenter.heightDelta != disturbanceRepeated.heightDelta ||
      protectedDisturbance.weight != 0.f || farBandDisturbance.weight != 0.f ||
      invalidDisturbance.weight != 0.f || invalidDisturbance.heightDelta != 0.f)
    return 84;
  auto wrongAsset = fallenLog;
  wrongAsset.meshAsset = "poly-haven/island_tree_01/cooked-lod";
  auto invalidScale = fallenLog;
  invalidScale.scale.x = (std::numeric_limits<float>::infinity)();
  const std::array<ShowcaseSceneDescriptor3D, 2> rejectedLogs{
      wrongAsset, invalidScale};
  const auto rejectedDisturbance = SampleShowcaseTerrainDisturbance3D(
      fallenLog.position, rejectedLogs);
  if (rejectedDisturbance.weight != 0.f ||
      rejectedDisturbance.tint.r != 1.f || rejectedDisturbance.tint.g != 1.f ||
      rejectedDisturbance.tint.b != 1.f || rejectedDisturbance.tint.a != 1.f)
    return 85;
  static_assert(ShowcaseForestTerrainTextureScale3D() >= .45f &&
                ShowcaseForestTerrainTextureScale3D() <= .55f);
  constexpr auto bankGrass =
      ShowcaseForestGrassTint3D("poly-haven/stony_dirt_path/cooked-pbr");
  constexpr auto groveGrass =
      ShowcaseForestGrassTint3D("poly-haven/forest_ground_04/cooked-pbr");
  constexpr auto meadowGrass =
      ShowcaseForestGrassTint3D("ambientcg/Ground108/cooked-pbr");
  const std::array<Color, 3> grassPalette{bankGrass, groveGrass, meadowGrass};
  const std::array<Color, 3> originalGrassPalette{
      Color{.29f, .43f, .18f, 1.f}, Color{.26f, .46f, .17f, 1.f},
      Color{.36f, .58f, .22f, 1.f}};
  if (!ValidateGrassPalette(grassPalette, originalGrassPalette)) return 68;
  auto grayGrassPalette = grassPalette;
  for (auto &value : grayGrassPalette)
    value.r = value.g = value.b = Rec709(value);
  auto brightGrassPalette = grassPalette;
  for (auto &value : brightGrassPalette) {
    value.r += .03f;
    value.g += .03f;
    value.b += .03f;
  }
  auto mergedGrassPalette = grassPalette;
  mergedGrassPalette[1] = mergedGrassPalette[0];
  if (ValidateGrassPalette(originalGrassPalette, originalGrassPalette) ||
      ValidateGrassPalette(grayGrassPalette, originalGrassPalette) ||
      ValidateGrassPalette(brightGrassPalette, originalGrassPalette) ||
      ValidateGrassPalette(mergedGrassPalette, originalGrassPalette))
    return 69;
  std::cout << "grassTintYChroma=";
  for (std::size_t index = 0; index < grassPalette.size(); ++index) {
    if (index) std::cout << ',';
    std::cout << Rec709(grassPalette[index]) << '/'
              << Chroma(grassPalette[index]) << '/'
              << Chroma(grassPalette[index]) / Chroma(originalGrassPalette[index]);
  }
  std::cout << " hostilePalette=REJECTED\n";
  static_assert(ShowcaseForestGrassMaterialAsset3D(
                    "poly-haven/stony_dirt_path/cooked-pbr") ==
                    "stony_dirt_path" &&
                ShowcaseForestGrassMaterialAsset3D(
                    "poly-haven/forest_ground_04/cooked-pbr") ==
                    "forest_ground_04" &&
                ShowcaseForestGrassMaterialAsset3D(
                    "ambientcg/Ground108/cooked-pbr") == "Ground108" &&
                ShowcaseForestGrassMaterialAsset3D("invalid").empty());
  Material3D texturedGrass{};
  texturedGrass.textures = MaterialTextureSetHandle3D{17};
  texturedGrass.useUv = true;
  texturedGrass.castsShadow = true;
  const auto runtimeGrass = ShowcaseForestGrassMaterial3D(
      texturedGrass, "poly-haven/forest_ground_04/cooked-pbr", false);
  if (runtimeGrass.textures || runtimeGrass.useUv || runtimeGrass.castsShadow ||
      runtimeGrass.materialClass != MaterialClass3D::Foliage ||
      !runtimeGrass.doubleSided || runtimeGrass.surface.metallic != 0.f ||
      runtimeGrass.surface.emissive != 0.f ||
      runtimeGrass.surface.roughness < .88f)
    return 67;
  Camera3D clippedGod{{0,10,-15},{0,1,0},{0,1,0},{}};
  clippedGod.projection.mode=ProjectionMode::Orthographic;
  clippedGod.projection.orthographicHeight=36.f;
  Camera3D coveredGod{{0,32,-30},{0,0,0},{0,1,0},{}};
  coveredGod.projection.mode=ProjectionMode::Orthographic;
  coveredGod.projection.orthographicHeight=28.f;
  if(GameplayGodTerrainFrameCovered3D(clippedGod)||
     !GameplayGodTerrainFrameCovered3D(coveredGod))return 65;
  constexpr auto treeLod = ShowcaseTreeLodPolicies3D();
  constexpr auto opaqueLod = ShowcaseOpaqueLodPolicies3D();
  if (treeLod[0].switchDistance != 8.f ||
      treeLod[1].switchDistance != 20.f ||
      treeLod[0].transitionWidth != 2.f ||
      treeLod[1].transitionWidth != 4.f ||
      treeLod[1].switchDistance >= opaqueLod[1].switchDistance)
    return 62;
  Mesh3D surfaceTerrain{};
  surfaceTerrain.vertices = {
      {-4.f, 1.f, 6.f, 0x01020304u, 0x00112233u, -.34f, .51f},
      {2.f, -3.f, -8.f, 0x11223344u, 0x80445566u, .17f, -.68f},
      {7.f, 2.f, 10.f, 0x55667788u, 0xff778899u, .595f, .85f}};
  surfaceTerrain.indices = {0, 2, 1, 1, 2, 0};
  const auto beforeTerrainVertices = surfaceTerrain.vertices;
  const auto beforeTerrainIndices = surfaceTerrain.indices;
  const auto invariantHash = [](const Mesh3D &mesh) {
    std::uint64_t hash = 14695981039346656037ull;
    const auto mix = [&](std::uint64_t value) {
      hash ^= value;
      hash *= 1099511628211ull;
    };
    mix(mesh.vertices.size());
    mix(mesh.indices.size());
    for (const auto &vertex : mesh.vertices) {
      mix(std::bit_cast<std::uint32_t>(vertex.x));
      mix(std::bit_cast<std::uint32_t>(vertex.y));
      mix(std::bit_cast<std::uint32_t>(vertex.z));
      mix(vertex.normal);
      mix(vertex.color & 0x00ffffffu);
      mix(std::bit_cast<std::uint32_t>(vertex.u));
      mix(std::bit_cast<std::uint32_t>(vertex.v));
    }
    for (const auto index : mesh.indices) mix(index);
    return hash;
  };
  const auto beforeTerrainHash = invariantHash(surfaceTerrain);
  // 外環幾何 gate：原格點完全保留、索引共用、全部新面在遊戲區之外。
  Mesh3D apron;
  for(unsigned z=0;z<129;++z)for(unsigned x=0;x<129;++x){
    const float px=-256.f+float(x)*4.f,pz=-256.f+float(z)*4.f;
    apron.vertices.push_back({px,.01f*px+.02f*pz,pz,PackNormalSnorm8(Normalize(Vec3{-.01f,1,-.02f})),0xff778899u,px*.085f,pz*.085f});
  }
  const auto originalApron=apron;
  auto repeatedApron=apron;
  if(!AppendShowcaseTerrainApron3D(apron)||!AppendShowcaseTerrainApron3D(repeatedApron)||
      apron.vertices.size()!=129u*129u+2560u||apron.indices.size()!=15360u||
      invariantHash(apron)!=invariantHash(repeatedApron))return 40;
  for(unsigned i=0;i<129u*129u;++i){const auto& a=apron.vertices[i];const auto& b=originalApron.vertices[i];
    if(a.x!=b.x||a.y!=b.y||a.z!=b.z||a.normal!=b.normal||a.color!=b.color||a.u!=b.u||a.v!=b.v)return 41;
  }
  for(std::size_t i=0;i<apron.indices.size();i+=3){
    const auto& a=apron.vertices[apron.indices[i]];const auto& b=apron.vertices[apron.indices[i+1]];const auto& c=apron.vertices[apron.indices[i+2]];
    const float normalY=(b.z-a.z)*(c.x-a.x)-(b.x-a.x)*(c.z-a.z);
    if(!(normalY>0)||!(std::abs((a.x+b.x+c.x)/3.f)>=256.f||std::abs((a.z+b.z+c.z)/3.f)>=256.f))return 42;
  }
  if(AppendShowcaseTerrainApron3D(apron))return 43;
  for(std::size_t i=129u*129u;i<apron.vertices.size();++i){const auto& v=apron.vertices[i];
    if(!std::isfinite(v.y)||v.normal!=originalApron.vertices[0].normal||v.color!=0xff778899u||
        v.u!=v.x*.085f||v.v!=v.z*.085f||std::abs(v.x)>1024.f||std::abs(v.z)>1024.f)return 44;
  }
  auto encodedApron = apron;
  ApplyShowcaseTerrainBiomeUv3D(encodedApron, 129u * 129u, ecologyTrees);
  if (!ValidateShowcaseTerrainBiomeUv3D(encodedApron, 129u * 129u))
    return 79;
  bool foundPath{}, foundLeaf{}, foundWet{}, foundNeutral{};
  for (std::size_t i = 0; i < encodedApron.vertices.size(); ++i) {
    const auto &before = apron.vertices[i];
    const auto &after = encodedApron.vertices[i];
    if (after.x != before.x || after.y != before.y || after.z != before.z ||
        after.normal != before.normal || after.color != before.color)
      return 79;
    const auto decoded = DecodeShowcaseTerrainBiomeUv3D({after.u, after.v});
    if (i >= 129u * 129u) {
      if (after.u != 0.f || after.v != 0.f ||
          decoded != ShowcaseTerrainBiomeWeights3D{})
        return 80;
      continue;
    }
    foundPath |= decoded.cottagePath > .2f;
    foundLeaf |= decoded.leafLitter > .99f;
    foundWet |= decoded.riverWetSoil > .6f;
    foundNeutral |= decoded == ShowcaseTerrainBiomeWeights3D{};
  }
  if (!foundPath || !foundLeaf || !foundWet || !foundNeutral)
    return 81;
  Mesh3D crossSignTriangle{};
  crossSignTriangle.vertices = {
      {0, 0, 0, 0, 0, .8f, 0}, {1, 0, 0, 0, 0, -.7f, 0},
      {0, 0, 1, 0, 0, 0, 0}};
  crossSignTriangle.indices = {0, 1, 2};
  ShowcaseTerrainBiomeUvValidation3D crossSignFailure{};
  if (ValidateShowcaseTerrainBiomeUv3D(crossSignTriangle,
                                        crossSignTriangle.vertices.size(),
                                        &crossSignFailure) ||
      crossSignFailure.failure !=
          ShowcaseTerrainBiomeUvFailure3D::CrossSignedTriangle ||
      crossSignFailure.elementIndex != 0 ||
      crossSignFailure.minimumU != -.7f ||
      crossSignFailure.maximumU != .8f)
    return 82;
  auto resolvedCrossSignTriangle = crossSignTriangle;
  ResolveShowcaseTerrainBiomeUvTriangleConflicts3D(resolvedCrossSignTriangle);
  if (!ValidateShowcaseTerrainBiomeUv3D(
          resolvedCrossSignTriangle,
          resolvedCrossSignTriangle.vertices.size()) ||
      resolvedCrossSignTriangle.vertices[0].u != .8f ||
      resolvedCrossSignTriangle.vertices[1].u != 0.f)
    return 83;
  Mesh3D sharedConflictForward{};
  sharedConflictForward.vertices = {
      {0, 0, 0, 0, 0, .8f, .1f},
      {1, 0, 0, 0, 0, -.7f, .2f},
      {2, 0, 0, 0, 0, .6f, .3f},
      {1, 0, 1, 0, 0, 0.f, .4f},
      {1, 0, 2, 0, 0, -.5f, .5f},
      {2, 0, 2, 0, 0, -.2f, .6f}};
  sharedConflictForward.indices = {0, 1, 3, 1, 2, 3, 1, 4, 5};
  auto sharedConflictReverse = sharedConflictForward;
  sharedConflictReverse.indices = {1, 4, 5, 1, 2, 3, 0, 1, 3};
  ResolveShowcaseTerrainBiomeUvTriangleConflicts3D(sharedConflictForward);
  ResolveShowcaseTerrainBiomeUvTriangleConflicts3D(sharedConflictReverse);
  const auto exactVertices = [](const Mesh3D &left, const Mesh3D &right) {
    if (left.vertices.size() != right.vertices.size())
      return false;
    for (std::size_t index = 0; index < left.vertices.size(); ++index) {
      const auto &a = left.vertices[index];
      const auto &b = right.vertices[index];
      if (a.x != b.x || a.y != b.y || a.z != b.z ||
          a.normal != b.normal || a.color != b.color || a.u != b.u ||
          a.v != b.v)
        return false;
    }
    return true;
  };
  if (!ValidateShowcaseTerrainBiomeUv3D(
          sharedConflictForward, sharedConflictForward.vertices.size()) ||
      !ValidateShowcaseTerrainBiomeUv3D(
          sharedConflictReverse, sharedConflictReverse.vertices.size()) ||
      !exactVertices(sharedConflictForward, sharedConflictReverse) ||
      sharedConflictForward.vertices[0].u != .8f ||
      sharedConflictForward.vertices[1].u != 0.f ||
      sharedConflictForward.vertices[2].u != 0.f ||
      sharedConflictForward.vertices[4].u != -.5f ||
      sharedConflictForward.vertices[5].u != -.2f)
    return 84;
  auto tieConflict = crossSignTriangle;
  tieConflict.vertices[0].u = .5f;
  tieConflict.vertices[1].u = -.5f;
  ResolveShowcaseTerrainBiomeUvTriangleConflicts3D(tieConflict);
  if (!ValidateShowcaseTerrainBiomeUv3D(tieConflict,
                                         tieConflict.vertices.size()) ||
      tieConflict.vertices[0].u != .5f || tieConflict.vertices[1].u != 0.f)
    return 85;
  // 正式河岸細分後會多出內部頂點；只要原世界邊界完整，外環仍必須建立。
  auto refinedApron=originalApron;
  refinedApron.vertices.push_back({.5f,.25f,.5f,PackNormalSnorm8({0,1,0}),0xff334455u,.1f,.2f});
  if(!AppendShowcaseTerrainApron3D(refinedApron)||
      refinedApron.vertices.size()!=129u*129u+1u+2560u||
      refinedApron.indices.size()!=15360u)return 64;
  const RenderObjectIdentity3D terrainIdentity{{0x4a17u, 1u}, 1u,
                                                {0x4a17u, 0x100u}, true, false};
  const auto beforeTerrainIdentity = terrainIdentity;
  if (PrepareShowcaseSurfaceTerrainMesh3D(surfaceTerrain) ||
      surfaceTerrain.vertices.size() != beforeTerrainVertices.size() ||
      surfaceTerrain.indices != beforeTerrainIndices ||
      invariantHash(surfaceTerrain) != beforeTerrainHash ||
      terrainIdentity != beforeTerrainIdentity)
    return 33;
  for (std::size_t index = 0; index < surfaceTerrain.vertices.size(); ++index) {
    const auto &before = beforeTerrainVertices[index];
    const auto &after = surfaceTerrain.vertices[index];
    if (after.x != before.x || after.y != before.y || after.z != before.z ||
        after.normal != before.normal || after.color != before.color ||
        after.u != before.u ||
        after.v != before.v)
      return 34;
  }

  GameplayShowcaseEnvironment3D first, second, differentSeed;
  const bool invalidSeedAccepted = first.Initialize(0);
  const bool firstBuilt = first.Initialize(0x4a17u);
  const bool secondBuilt = second.Initialize(0x4a17u);
  const bool differentBuilt = differentSeed.Initialize(0x4a18u);
  if (invalidSeedAccepted || !firstBuilt || !secondBuilt || !differentBuilt) {
    const auto &failedLand = firstBuilt ? differentSeed.LandReceipt() : first.LandReceipt();
    std::cerr << "environment initialization invalid=" << invalidSeedAccepted
              << " first=" << firstBuilt << " repeated=" << secondBuilt
              << " different=" << differentBuilt << " land=" << failedLand.placements
              << " failure=" << failedLand.failureKind << '/' << failedLand.failureBand << '/' << failedLand.failureItem
              << " attempts=" << failedLand.attemptedCandidates << '\n';
    return 1;
  }
  if (!TreeDescriptorDistributionContract(first.Descriptors()))
    return 78;
  std::vector<ShowcaseLandPlacement3D> treePlacements;
  if (!BuildShowcaseLandEcology3D({}, treePlacements) ||
      !TreeFormVariationContract(treePlacements, first.Descriptors()) ||
      !UnderstoryFormVariationContract(treePlacements, first.Descriptors()))
    return 84;
  auto collapsedTreeDescriptors = first.Descriptors();
  for (auto &descriptor : collapsedTreeDescriptors)
    if (descriptor.meshAsset == "poly-haven/tree_small_02/cooked-lod") {
      descriptor.meshAsset = "poly-haven/island_tree_01/cooked-lod";
      descriptor.materialAsset = "poly-haven/island_tree_01/cooked-pbr";
    }
  if (TreeDescriptorDistributionContract(collapsedTreeDescriptors))
    return 79;
  auto unresolvedTreeDescriptors = first.Descriptors();
  const auto treeToBreak = std::ranges::find_if(
      unresolvedTreeDescriptors, [](const auto &descriptor) {
        return descriptor.meshAsset == "poly-haven/tree_small_02/cooked-lod";
      });
  if (treeToBreak == unresolvedTreeDescriptors.end())
    return 82;
  treeToBreak->meshAsset = "poly-haven/tree_small_03/cooked-lod";
  if (TreeDescriptorDistributionContract(unresolvedTreeDescriptors))
    return 83;
  const auto receipt = first.Receipt();
  const auto repeated = second.Receipt();
  const auto targets = GameplayShowcaseEnvironment3D::Targets();
  constexpr std::array<std::string_view, 15> materialIdentities{
      "forest_ground_04:slot-0", "Ground108:slot-0",
      "RoofingTiles014B:slot-0", "Bricks100:slot-0",
      "island_tree_01:trunk", "island_tree_01:leaves",
      "island_tree_01:branches", "tree_small_02:trunk",
      "tree_small_02:leaves", "tree_small_02:branches",
      "dead_tree_trunk_02:slot-0", "boulder_01:slot-0",
      "coastal_cliff_01:slot-0", "stony_dirt_path:slot-0",
      "runtime/water-optics"};
  constexpr std::array<std::string_view, 11> licensedAssets{
      "Bricks100|ambientcg|CC0-1.0|commercial",
      "Ground108|ambientcg|CC0-1.0|commercial",
      "RoofingTiles014B|ambientcg|CC0-1.0|commercial",
      "boulder_01|poly-haven|CC0-1.0|commercial",
      "coastal_cliff_01|poly-haven|CC0-1.0|commercial",
      "dead_tree_trunk_02|poly-haven|CC0-1.0|commercial",
      "forest_ground_04|poly-haven|CC0-1.0|commercial",
      "island_tree_01|poly-haven|CC0-1.0|commercial",
      "stony_dirt_path|poly-haven|CC0-1.0|commercial",
      "tree_small_02|poly-haven|CC0-1.0|commercial",
      "xanderklinge|poly-haven|CC0-1.0|commercial"};
  if (targets.materialIdentityHash != InventoryHash(materialIdentities) ||
      targets.licenseInventoryHash != InventoryHash(licensedAssets))
    return 80;
  auto missingTreeIdentity = materialIdentities;
  missingTreeIdentity[7] = "island_tree_01:trunk";
  auto missingTreeLicense = licensedAssets;
  missingTreeLicense[9] = "island_tree_01|poly-haven|CC0-1.0|commercial";
  if (targets.materialIdentityHash == InventoryHash(missingTreeIdentity) ||
      targets.licenseInventoryHash == InventoryHash(missingTreeLicense))
    return 81;
  // 精確負載不能只與可同時退回舊值的 Targets 比較，否則舊荒地統計會假通過。
  const auto healthyForestLoad = [](const ShowcaseEnvironmentReceipt3D &value) {
    return value.trees == 240 && value.grass == 186 && value.rocks == 30 &&
        value.vegetation == 426 && value.nearVegetation == 240 &&
        value.midVegetation == 140 && value.farVegetation == 46 &&
        value.riverbankProps == 92;
  };
  if (!healthyForestLoad(receipt)) return 52;
  auto legacyLoad = receipt;
  legacyLoad.trees = 144;
  legacyLoad.grass = 282;
  legacyLoad.nearVegetation = 156;
  legacyLoad.midVegetation = 184;
  legacyLoad.farVegetation = 86;
  if (healthyForestLoad(legacyLoad)) return 53;
  auto rocksAsVegetation = receipt;
  rocksAsVegetation.nearVegetation = 180;
  rocksAsVegetation.midVegetation = 196;
  rocksAsVegetation.farVegetation = 80;
  if (healthyForestLoad(rocksAsVegetation)) return 54;
  const auto land = first.LandReceipt();
  if (!land.Complete() || land.placements != 456 ||
      land.trees[0] + land.grass[0] + land.rocks[0] != 252 ||
      land.trees[1] + land.grass[1] + land.rocks[1] != 152 ||
      land.trees[2] + land.grass[2] + land.rocks[2] != 52) return 55;
  // 所有陸地 placement 必須由同一份最終高度場取得基部高度；非陸地
  // descriptor 保持原位置，重複套用亦必須位元穩定。
  GameplayShowcaseEnvironment3D grounded;
  if (!grounded.Initialize()) return 87;
  Aevrynd::HeightField3D groundingTerrain({{-256.f,0.f,-256.f},129,129,4.f});
  groundingTerrain.Fill(3.25f);
  std::vector<float> nonLandY;
  for (const auto &descriptor : grounded.Descriptors())
    if (descriptor.kind != ShowcaseSceneKind3D::Tree &&
        descriptor.kind != ShowcaseSceneKind3D::Grass &&
        descriptor.kind != ShowcaseSceneKind3D::Rock)
      nonLandY.push_back(descriptor.position.y);
  ShowcaseTerrainGroundingReceipt3D grounding{};
  if (!grounded.GroundLandDescriptors(groundingTerrain, &grounding) ||
      !grounding.Complete() || grounding.eligibleDescriptors != 456 ||
      grounding.groundedDescriptors != 456 || grounding.maximumHeightError > .0001f)
    return 88;
  std::size_t nonLandIndex{};
  for (const auto &descriptor : grounded.Descriptors()) {
    const bool landDescriptor = descriptor.kind == ShowcaseSceneKind3D::Tree ||
        descriptor.kind == ShowcaseSceneKind3D::Grass ||
        descriptor.kind == ShowcaseSceneKind3D::Rock;
    if (landDescriptor) {
      if (std::abs(descriptor.position.y - 3.25f) > .0001f) return 89;
    } else if (descriptor.position.y != nonLandY[nonLandIndex++]) return 90;
  }
  const auto groundedWorldHash = grounded.Receipt().worldHash;
  const auto groundedReceipt = grounded.Receipt();
  if (!groundedReceipt.terrainGroundingApplied ||
      groundedReceipt.terrainGroundingEligible != 456 ||
      groundedReceipt.terrainGroundedDescriptors != 456 ||
      groundedReceipt.terrainGroundingMaximumError > .0001f)
    return 92;
  ShowcaseTerrainGroundingReceipt3D repeatedGrounding{};
  if (!grounded.GroundLandDescriptors(groundingTerrain, &repeatedGrounding) ||
      !repeatedGrounding.Complete() ||
      grounded.Receipt().worldHash != groundedWorldHash)
    return 91;
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
      !receipt.propFootprintsNonOverlapping) {
    std::cerr << "environment receipt gate world=" << receipt.worldHash << '/'
              << repeated.worldHash << " vegetation=" << receipt.vegetation
              << " bands=" << receipt.nearVegetation << '/'
              << receipt.midVegetation << '/' << receipt.farVegetation
              << " targetBands=" << targets.nearVegetation << '/'
              << targets.midVegetation << '/' << targets.farVegetation
              << " descriptors=" << receipt.integratedDescriptors << '/'
              << receipt.descriptorCapacity << " habitat="
              << receipt.densestHabitatCell << '/'
              << targets.minimumDensestHabitatCell << " riverbank="
              << receipt.riverbankProps << '/' << targets.minimumRiverbankProps
              << " terrainZones=" << receipt.terrainZonesExact
              << " riverLayer=" << receipt.riverbankLayered
              << " footprints=" << receipt.propFootprintsNonOverlapping << '\n';
    return 2;
  }
  std::array<std::size_t, 3> terrainLayers{};
  std::size_t grassDescriptors = 0;
  std::size_t shadowCastingGrassDescriptors = 0;
  bool grassBiomeMaterialsExact = true;
  for (const auto &descriptor : first.Descriptors()) {
    if (descriptor.kind == ShowcaseSceneKind3D::Grass) {
      ++grassDescriptors;
      shadowCastingGrassDescriptors += descriptor.castsShadow ? 1u : 0u;
      if (descriptor.meshAsset == "procedural/grass-blade")
        grassBiomeMaterialsExact &=
            descriptor.materialAsset.find("stony_dirt_path") !=
            std::string::npos;
      else if (descriptor.meshAsset == "procedural/grass-fan")
        grassBiomeMaterialsExact &=
            descriptor.materialAsset.find("Ground108") != std::string::npos;
      else if (descriptor.meshAsset == "procedural/grass-tuft")
        grassBiomeMaterialsExact &=
            descriptor.materialAsset.find("forest_ground_04") !=
            std::string::npos;
      else
        grassBiomeMaterialsExact = false;
    }
    if (descriptor.materialAsset.find("forest_ground_04") != std::string::npos)
      ++terrainLayers[0];
    else if (descriptor.materialAsset.find("Ground108") != std::string::npos)
      ++terrainLayers[1];
    else if (descriptor.materialAsset.find("stony_dirt_path") != std::string::npos)
      ++terrainLayers[2];
  }
  if (terrainLayers != std::array<std::size_t, 3>{63, 62, 62} ||
      grassDescriptors != 186 || shadowCastingGrassDescriptors != 0 ||
      !grassBiomeMaterialsExact)
    return 32;

  constexpr Vec3 heroCenter{-16.f, 0.f, 8.f};
  constexpr float heroRadius = 22.f;
  std::uint32_t heroGroundCover = 0;
  std::uint32_t heroGroundCoverQuadrantMask = 0;
  for (const auto &descriptor : first.Descriptors()) {
    if (descriptor.kind != ShowcaseSceneKind3D::Grass) continue;
    const Vec3 delta = descriptor.position - heroCenter;
    if (delta.x * delta.x + delta.z * delta.z > heroRadius * heroRadius)
      continue;
    ++heroGroundCover;
    const std::uint32_t quadrant =
        (delta.z >= 0.f ? 2u : 0u) + (delta.x >= 0.f ? 1u : 0u);
    heroGroundCoverQuadrantMask |= 1u << quadrant;
  }
  // 英雄聚落周圍必須有四象限皆可見的非碰撞地被，避免固定鏡頭再次退化成空地。
  if (heroGroundCover < 28 || heroGroundCoverQuadrantMask != 0x0fu)
    return 35;

  DrawList draw{DrawListCapacityProfile::Minimal2D};
  first.ApplyHighProfile(draw);
  first.ApplyLighting(draw, ShowcaseLightingProfile3D::Day, {1, 2, 3});
  const auto dayAtmosphere = draw.Atmosphere();
  const auto dayEnvironmentBlend = draw.EnvironmentLightingBlend();
  const auto dayToneCurve = draw.TimeOfDayExposure();
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
      !draw.TerrainDistanceMaterial().mesoPartitionEnabled ||
      !draw.WaterOptics().screenSpaceRefraction ||
      !draw.WaterSurfaceReceipt().enabledRequested ||
      draw.WaterSurfaceReceipt().fingerprint != receipt.waterSurfaceFingerprint)
    return 3;

  // 低仰角英雄鏡頭不得再次使用會形成水平煙牆的低空、高覆蓋雲層。
  const auto &heroClouds = draw.VolumetricClouds();
  if (heroClouds.baseAltitude < 1800.f || heroClouds.coverage > .45f ||
      heroClouds.density > .65f || heroClouds.erosion < .50f ||
      heroClouds.shadowStrength > .40f || heroClouds.maximumDistance < 6000.f)
    return 38;

  // 岸泡深度依實際河床最淺百分位設定；不得再次把整條河當作泡沫。
  if(draw.WaterSurface().foamStartDepth > .06f ||
     draw.WaterSurface().foamEndDepth-draw.WaterSurface().foamStartDepth > .14f ||
     draw.WaterSurface().foamStrength <= 0.f ||
     draw.WaterSurface().foamStrength > .30f)
    return 37;
  // 林地河水不可再由高飽和青色固定源項主宰；仍維持自然水體的紅光
  // 吸收排序，但限制通道差距與深水散射的藍綠偏移。
  const auto &forestWater = draw.WaterSurface();
  if (!(forestWater.absorption.x > forestWater.absorption.y &&
        forestWater.absorption.y > forestWater.absorption.z) ||
      forestWater.absorption.x > forestWater.absorption.z * 2.5f ||
      forestWater.deepColor.y > forestWater.deepColor.x + .022f ||
      forestWater.deepColor.z > forestWater.deepColor.x + .025f)
    return 66;
  const auto oldFlatWater = NormalizeWaterSurfaceSettings3D(
      {true, {.42f,.25f,.19f}, {.012f,.028f,.030f}, .025f, .09f, .28f,
       .035f, .38f, .055f, .18f, .12f, .11f, .03f, .14f, .26f, .78f,
       .95f, .012f});
  if (!ReadableForestWater(forestWater) || ReadableForestWater(oldFlatWater))
    return 70;
  const auto day = first.EvaluateLighting(ShowcaseLightingProfile3D::Day);
  const auto dusk = first.EvaluateLighting(ShowcaseLightingProfile3D::Dusk);
  const auto night = first.EvaluateLighting(ShowcaseLightingProfile3D::Night);
  auto deadBlackDusk = dusk;
  deadBlackDusk.sun.ambient = .16f;
  if (!DuskVegetationReadable(dusk) || DuskVegetationReadable(deadBlackDusk))
    return 71;
  first.ApplyLighting(draw, ShowcaseLightingProfile3D::Dusk, {1, 2, 3});
  const auto duskAtmosphere = draw.Atmosphere();
  const auto duskEnvironmentBlend = draw.EnvironmentLightingBlend();
  const auto duskToneCurve = draw.TimeOfDayExposure();
  const auto duskSubmittedSun = draw.DirectionalLight();
  const auto duskRadianceFingerprint = draw.PhaseRadianceProfile().phaseFingerprint;
  const auto duskRadiance = draw.PhaseRadianceProfile();
  if (draw.VolumetricAtmospherePhase() != VolumetricAtmospherePhase3D::Dusk)
    return 21;
  first.ApplyLighting(draw, ShowcaseLightingProfile3D::Night, {1, 2, 3});
  const auto nightAtmosphere = draw.Atmosphere();
  const auto nightEnvironmentBlend = draw.EnvironmentLightingBlend();
  const auto nightToneCurve = draw.TimeOfDayExposure();
  const auto nightSubmittedSun = draw.DirectionalLight();
  const auto nightRadianceFingerprint = draw.PhaseRadianceProfile().phaseFingerprint;
  const auto nightRadiance = draw.PhaseRadianceProfile();
  if (draw.VolumetricAtmospherePhase() != VolumetricAtmospherePhase3D::Night)
    return 22;
  const TimeOfDayExposureSettings3D defaultToneCurve{};
  auto expectedDuskToneCurve = defaultToneCurve;
  expectedDuskToneCurve.duskExposureEv = .66f;
  auto unboundedNightToneCurve = nightToneCurve;
  unboundedNightToneCurve.nightExposureEv = 2.f;
  auto previousBrightNightToneCurve = nightToneCurve;
  previousBrightNightToneCurve.nightExposureEv = 1.65f;
  auto crushedNightToneCurve = nightToneCurve;
  crushedNightToneCurve.shadowLift = .08f;
  if (dayToneCurve != defaultToneCurve ||
      duskToneCurve != expectedDuskToneCurve ||
      !NightToneCurveBalanced(nightToneCurve, night) ||
      NightToneCurveBalanced(defaultToneCurve, night) ||
      NightToneCurveBalanced(unboundedNightToneCurve, night) ||
      NightToneCurveBalanced(previousBrightNightToneCurve, night) ||
      NightToneCurveBalanced(crushedNightToneCurve, night))
    return 76;
  DrawList duskLocalLights{DrawListCapacityProfile::Minimal2D};
  DrawList nightLocalLights{DrawListCapacityProfile::Minimal2D};
  first.ApplyLighting(duskLocalLights, ShowcaseLightingProfile3D::Dusk,
                      {1, 2, 3});
  first.ApplyLighting(nightLocalLights, ShowcaseLightingProfile3D::Night,
                      {1, 2, 3});
  if (!NightCabinHighlightsBalanced(duskLocalLights.PointLights(),
                                    nightLocalLights.PointLights()))
    return 73;
  auto previousWeakNightLights = nightLocalLights.PointLights();
  for (auto &light : previousWeakNightLights)
    light.intensity *= .28f / 1.25f;
  if (NightCabinHighlightsBalanced(duskLocalLights.PointLights(),
                                   previousWeakNightLights))
    return 74;
  auto excessiveNightLights = nightLocalLights.PointLights();
  for (auto &light : excessiveNightLights)
    light.intensity *= 1.6f / 1.25f;
  if (NightCabinHighlightsBalanced(duskLocalLights.PointLights(),
                                   excessiveNightLights))
    return 75;
  const EnvironmentLightingBlendSettings3D deadBlackNightEnvironment{
      {.72f,.78f,1.f},.26f,{1.f,1.f,1.f},.23f,.88f,.12f,.82f,.18f,
      {.72f,.78f,1.f},.52f,0x4e494748545f4942ull};
  const PhaseRadianceProfile3D deadBlackNightRadiance{
      {.008f,.014f,.032f},.19f,{.035f,.045f,.07f},.29f,
      {.006f,.008f,.012f},.00935f,{.72f,.78f,1.f},.16f,
      {.55f,.66f,1.f},1.2f,0x4e49474854534b31ull};
  auto fakeExposureNight = night;
  fakeExposureNight.exposureEv = .05f;
  auto previousMoonNight = night;
  previousMoonNight.sun.ambient = .105f;
  auto previousDarkNightEnvironment = nightEnvironmentBlend;
  previousDarkNightEnvironment.diffuseIntensity = .32f;
  auto previousDimObjectFill = nightEnvironmentBlend;
  previousDimObjectFill.objectFillIntensity = .56f;
  if (!NightForestReadable(night, nightEnvironmentBlend, nightRadiance) ||
      NightForestReadable(night, deadBlackNightEnvironment,
                          deadBlackNightRadiance) ||
      NightForestReadable(previousMoonNight, nightEnvironmentBlend,
                          nightRadiance) ||
      NightForestReadable(night, previousDarkNightEnvironment,
                          nightRadiance) ||
      NightForestReadable(night, previousDimObjectFill, nightRadiance) ||
      NightForestReadable(fakeExposureNight, nightEnvironmentBlend,
                          nightRadiance))
    return 72;
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
      dayRadiance.horizonExponent < 2.4f ||
      duskRadiance.horizonExponent < 2.f ||
      nightRadiance.horizonExponent < 1.6f ||
      !(dayRadiance.horizonExponent > duskRadiance.horizonExponent &&
        duskRadiance.horizonExponent > nightRadiance.horizonExponent) ||
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
  if (!(dayLuma > duskLuma * 1.05f && duskLuma > nightLuma * 1.05f &&
        nightLuma >= .075f))
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
            << " trees=" << receipt.trees << " grass=" << receipt.grass
            << " vegetationBands=" << receipt.nearVegetation << '/' << receipt.midVegetation << '/' << receipt.farVegetation
            << " landPlacements=" << land.placements
            << " descriptors=" << released.descriptorsReleased
            << " releasedBytes=" << released.retainedBytesBefore
            << " dayLuma=" << dayLuma << " duskLuma=" << duskLuma
            << " nightLuma=" << nightLuma
            << " duskAmbient=" << dusk.sun.ambient
            << " waterRoughness=" << forestWater.minimumRoughness << '/'
            << forestWater.maximumRoughness
            << " waterWaves=" << forestWater.broadWaveScale << '/'
            << forestWater.broadWaveStrength << '/'
            << forestWater.detailWaveScale << '/'
            << forestWater.detailWaveStrength
            << " waterAlpha=" << forestWater.shallowAlpha << '/'
            << forestWater.deepAlpha
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
