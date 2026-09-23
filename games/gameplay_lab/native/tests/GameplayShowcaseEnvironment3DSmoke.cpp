#include "GameplayShowcaseEnvironment3D.h"
#include "GameplayForestGroundCoverMesh3D.h"
#include "GameplayGrassCommunityMesh3D.h"
#include "ShowcaseTerrainMaterials3D.h"
#define AEVRYND_CHARACTER_BUILD_FINGERPRINT "environment-test"
#include "GameplayLabGame.h"
#undef AEVRYND_CHARACTER_BUILD_FINGERPRINT

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <utility>
#include <vector>

// 將 GameplayLabGame 使用的同一份程序地被 builder 編入這個 focused smoke。
// 此 target 原本只擁有 environment/ecology translation units；直接納入正式
// builder，讓 footprint 證據隨實際提交網格演進，不依賴手寫假幾何。
#include "../src/GameplayForestGroundCoverMesh3D.cpp"

using namespace Aevrynd;
using namespace GameplayLab;

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
  const auto effectiveDiffuse = [](const LightingRouteEvidence3D &route) {
    return route.typed.sun.ambient * route.environment.diffuseIntensity *
           route.radiance.analyticDiffuseIntensity;
  };
  // 單一 ambient 參數不能代表相位能量；真正進入不透明表面的解析漫射
  // 同時受環境強度與相位 radiance 約束。驗證合成後的能量仍維持日＞昏＞夜。
  if (!(effectiveDiffuse(observed[0]) > effectiveDiffuse(observed[1]) &&
        effectiveDiffuse(observed[1]) > effectiveDiffuse(observed[2]) &&
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
            expected[index].radiance.analyticDiffuseIntensity ||
        observed[index].radiance.lunarDetailStrength !=
            expected[index].radiance.lunarDetailStrength ||
        observed[index].radiance.stellarDetailStrength !=
            expected[index].radiance.stellarDetailStrength)
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
  return resolvedTrees == 296 &&
         islandByBand == std::array<unsigned, 3>{62, 46, 40} &&
         smallByBand == std::array<unsigned, 3>{52, 38, 34} &&
         deadByBand == std::array<unsigned, 3>{10, 8, 6};
}

// 以正式提交的中景 descriptor 檢查每四株一群的母樹方位；不能只檢查
// placement 或總樹數，否則 ordinal 以四遞增、六槽只命中三槽的 alias 仍會通過。
bool TreeMotherAngularDistributionContract(
    const std::vector<ShowcaseSceneDescriptor3D> &descriptors) {
  constexpr float twoPi = 6.2831853071795864769f;
  constexpr std::array<float, 6> directions{
      .18f, .86f, 2.22f, 3.05f, 4.38f, 5.72f};
  constexpr Vec3 spawnCenter{-16.f, 0.f, 8.f};
  std::array<unsigned, 6> mothersByDirection{};
  unsigned treeOrdinal{}, midMothers{};
  for (const auto &descriptor : descriptors) {
    if (descriptor.kind != ShowcaseSceneKind3D::Tree) continue;
    if (descriptor.lodGroup == 1u && treeOrdinal % 4u == 0u) {
      const float angle = std::atan2(descriptor.position.z - spawnCenter.z,
                                     descriptor.position.x - spawnCenter.x);
      float nearestDistance = twoPi;
      std::size_t nearestDirection{};
      for (std::size_t slot = 0; slot < directions.size(); ++slot) {
        const float distance =
            std::abs(std::remainder(angle - directions[slot], twoPi));
        if (distance < nearestDistance) {
          nearestDistance = distance;
          nearestDirection = slot;
        }
      }
      ++mothersByDirection[nearestDirection];
      ++midMothers;
    }
    ++treeOrdinal;
  }
  std::cout << "TREE_MOTHER_DIRECTIONS mid=" << midMothers << " slots=";
  for (const auto count : mothersByDirection) std::cout << count << ',';
  std::cout << '\n';
  return midMothers == 23u &&
         std::ranges::none_of(mothersByDirection,
                              [](unsigned count) { return count == 0u; });
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
        form.scaleMultiplier.x < .58f || form.scaleMultiplier.x > 1.24f ||
        form.scaleMultiplier.z < .51f || form.scaleMultiplier.z > 1.34f ||
        form.scaleMultiplier.y < .70f || form.scaleMultiplier.y > 1.16f ||
        std::abs(form.yawOffset) > .211f ||
        descriptor.position.x != placement.position.x ||
        descriptor.position.y != placement.position.y ||
        descriptor.position.z != placement.position.z ||
        descriptor.scale.x != placement.scale.x * form.scaleMultiplier.x ||
        descriptor.scale.y != placement.scale.y * form.scaleMultiplier.y ||
        descriptor.scale.z != placement.scale.z * form.scaleMultiplier.z ||
        descriptor.yaw != placement.yaw + form.yawOffset) {
      std::cerr << "TREE_FORM_MISMATCH index=" << index
                << " stableKey=" << placement.stableKey << '\n';
      return false;
    }
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
  return trees == 296 && std::ranges::all_of(sawVariant, [](bool v) { return v; }) &&
         std::ranges::all_of(sawBand, [](bool v) { return v; }) && distinct >= 8 &&
         heightRange[0][1] - heightRange[0][0] >= .20f &&
         heightRange[1][1] - heightRange[1][0] >= .18f &&
         heightRange[2][1] < heightRange[0][1];
}

bool UnderstoryFormVariationContract(
    const std::vector<ShowcaseLandPlacement3D> &placements,
    const std::vector<ShowcaseSceneDescriptor3D> &descriptors) {
  if (descriptors.size() != placements.size() + 8u) {
    std::cerr << "UNDERSTORY_DESCRIPTOR_COUNT " << descriptors.size() << '/'
              << placements.size() << '\n';
    return false;
  }
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
        descriptor.yaw != placement.yaw + form.yawOffset) {
      std::cerr << "UNDERSTORY_FORM_MISMATCH index=" << index
                << " position=" << placement.position.x << ','
                << placement.position.z << " descriptor="
                << descriptor.position.x << ',' << descriptor.position.y << ','
                << descriptor.position.z << " placementY="
                << placement.position.y
                << " scale=" << descriptor.scale.x << ','
                << descriptor.scale.y << ',' << descriptor.scale.z
                << " expected="
                << placement.scale.x * form.scaleMultiplier.x << ','
                << placement.scale.y * form.scaleMultiplier.y << ','
                << placement.scale.z * form.scaleMultiplier.z
                << " form=" << form.scaleMultiplier.x << ','
                << form.scaleMultiplier.y << ',' << form.scaleMultiplier.z
                << " yaw=" << descriptor.yaw << "/"
                << placement.yaw + form.yawOffset << '\n';
      return false;
    }
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
                                  6.2831853071795864769f)) > .121f) {
        std::cerr << "UNDERSTORY_RIVER_YAW_MISMATCH index=" << index
                  << " z=" << z << '\n';
        return false;
      }
    } else if (placement.zone == ShowcaseLandZone3D::Meadow) {
      zone = 1;
    } else if (placement.zone == ShowcaseLandZone3D::Upland) {
      zone = 2;
    } else {
      std::cerr << "UNDERSTORY_ZONE_MISMATCH index=" << index << '\n';
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

struct UnderstoryHighDetailBounds3D {
  MeshBounds3D riverbank{};
  MeshBounds3D fern{};
  MeshBounds3D community{};
};

UnderstoryHighDetailBounds3D BuildSubmittedUnderstoryHighDetailBounds3D() {
  return {
      CreateGameplayForestRiverbankGroundCoverMesh3D(
          GameplayForestGroundCoverDetail3D::Near, 0x31b7u).Bounds(),
      CreateGameplayForestGroundCoverMesh3D(
          GameplayForestGroundCoverSpecies3D::Fern,
          GameplayForestGroundCoverDetail3D::Near, 0x4a17u).Bounds(),
      CreateGameplayForestGroundCoverCommunityMesh3D(
          GameplayForestGroundCoverDetail3D::Near, 0x7c31u).Bounds()};
}

const MeshBounds3D *SubmittedUnderstoryMeshBounds3D(
    const ShowcaseSceneDescriptor3D &descriptor,
    const UnderstoryHighDetailBounds3D &bounds) {
  if (descriptor.meshAsset == "procedural/grass-blade") return &bounds.riverbank;
  if (descriptor.meshAsset == "procedural/grass-fan") return &bounds.fern;
  if (descriptor.meshAsset == "procedural/grass-tuft") return &bounds.community;
  return nullptr;
}

struct SubmittedUnderstoryFootprint3D {
  std::array<Vec2, 4> corners{};
  Vec3 worldCenter{};
  float sphereRadius{};
  float horizontalArea{};
};

SubmittedUnderstoryFootprint3D TransformSubmittedUnderstoryFootprint3D(
    const ShowcaseSceneDescriptor3D &descriptor,
    const MeshBounds3D &meshBounds) {
  // GameplayLabGame 在提交前套用 GameplayForestGroundCoverWorldScale3D：保留
  // descriptor 水平尺度，公尺建模的 Y 則固定為一；此處連同 local center
  // 一起變換真實 AABB。
  const Vec3 scale = GameplayForestGroundCoverWorldScale3D(descriptor.scale);
  const float sine = std::sin(descriptor.yaw);
  const float cosine = std::cos(descriptor.yaw);
  const auto rotate = [sine, cosine](float x, float z) {
    return Vec2{x * cosine - z * sine, x * sine + z * cosine};
  };
  const Vec2 rotatedCenter =
      rotate(meshBounds.center.x * scale.x, meshBounds.center.z * scale.z);
  SubmittedUnderstoryFootprint3D result{};
  result.worldCenter = {descriptor.position.x + rotatedCenter.x,
                        descriptor.position.y + meshBounds.center.y * scale.y,
                        descriptor.position.z + rotatedCenter.y};
  const float extentX = std::abs(meshBounds.extents.x * scale.x);
  const float extentY = std::abs(meshBounds.extents.y * scale.y);
  const float extentZ = std::abs(meshBounds.extents.z * scale.z);
  constexpr std::array<Vec2, 4> signs{{{-1.f, -1.f}, {1.f, -1.f},
                                       {1.f, 1.f}, {-1.f, 1.f}}};
  for (std::size_t corner = 0; corner < signs.size(); ++corner) {
    const Vec2 offset = rotate(signs[corner].x * extentX,
                               signs[corner].y * extentZ);
    result.corners[corner] = {result.worldCenter.x + offset.x,
                              result.worldCenter.z + offset.y};
  }
  result.sphereRadius =
      std::sqrt(extentX * extentX + extentY * extentY + extentZ * extentZ);
  result.horizontalArea = 4.f * extentX * extentZ;
  return result;
}

float PointSegmentDistance2D(Vec2 point, Vec2 start, Vec2 end) {
  const Vec2 delta{end.x - start.x, end.y - start.y};
  const float lengthSquared = delta.x * delta.x + delta.y * delta.y;
  const float t = lengthSquared > 0.f
                      ? std::clamp(((point.x - start.x) * delta.x +
                                    (point.y - start.y) * delta.y) /
                                       lengthSquared,
                                   0.f, 1.f)
                      : 0.f;
  const float dx = point.x - (start.x + delta.x * t);
  const float dz = point.y - (start.y + delta.y * t);
  return std::sqrt(dx * dx + dz * dz);
}

float SegmentSegmentDistance2D(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
  const auto cross = [](Vec2 left, Vec2 right) {
    return left.x * right.y - left.y * right.x;
  };
  const Vec2 ab{b.x - a.x, b.y - a.y};
  const Vec2 cd{d.x - c.x, d.y - c.y};
  const float denominator = cross(ab, cd);
  if (std::abs(denominator) > 1e-7f) {
    const Vec2 ac{c.x - a.x, c.y - a.y};
    const float t = cross(ac, cd) / denominator;
    const float u = cross(ac, ab) / denominator;
    if (t >= 0.f && t <= 1.f && u >= 0.f && u <= 1.f) return 0.f;
  }
  return (std::min)({PointSegmentDistance2D(a, c, d),
                     PointSegmentDistance2D(b, c, d),
                     PointSegmentDistance2D(c, a, b),
                     PointSegmentDistance2D(d, a, b)});
}

bool PointInsideFootprint2D(
    Vec2 point, const std::array<Vec2, 4> &corners) {
  bool positive = false, negative = false;
  for (std::size_t edge = 0; edge < corners.size(); ++edge) {
    const auto a = corners[edge];
    const auto b = corners[(edge + 1u) % corners.size()];
    const float side = (b.x - a.x) * (point.y - a.y) -
                       (b.y - a.y) * (point.x - a.x);
    positive |= side > 1e-6f;
    negative |= side < -1e-6f;
  }
  return !(positive && negative);
}

float SubmittedFootprintWalkingClearance3D(
    const SubmittedUnderstoryFootprint3D &footprint) {
  float clearance = (std::numeric_limits<float>::max)();
  const auto &controls = ShowcaseHeroTrailControls3D();
  for (const auto control : controls)
    if (PointInsideFootprint2D({control.x, control.z}, footprint.corners))
      return 0.f;
  for (std::size_t segment = 0; segment + 1 < controls.size(); ++segment) {
    const Vec2 laneStart{controls[segment].x, controls[segment].z};
    const Vec2 laneEnd{controls[segment + 1].x, controls[segment + 1].z};
    for (std::size_t edge = 0; edge < footprint.corners.size(); ++edge)
      clearance = (std::min)(clearance, SegmentSegmentDistance2D(
          laneStart, laneEnd, footprint.corners[edge],
          footprint.corners[(edge + 1u) % footprint.corners.size()]));
  }
  return clearance;
}

float LegacyPlacementWalkingClearance3D(
    const std::vector<ShowcaseLandPlacement3D> &placements) {
  float clearance = (std::numeric_limits<float>::max)();
  for (const auto &placement : placements) {
    if (placement.kind != ShowcaseLandKind3D::Grass) continue;
    const float oldRadius =
        .34f * (std::max)(placement.scale.x, placement.scale.z);
    clearance = (std::min)(
        clearance,
        SampleShowcaseHeroTrail3D(placement.position).distance - oldRadius);
  }
  return clearance;
}

float SubmittedUnderstoryWalkingClearance3D(
    const std::vector<ShowcaseLandPlacement3D> &placements,
    const std::vector<ShowcaseSceneDescriptor3D> &descriptors,
    const UnderstoryHighDetailBounds3D &bounds) {
  if (descriptors.size() != placements.size() + 8u) return -1.f;
  float clearance = (std::numeric_limits<float>::max)();
  for (std::size_t index = 0; index < placements.size(); ++index) {
    if (placements[index].kind != ShowcaseLandKind3D::Grass) continue;
    const auto &descriptor = descriptors[index + 2u];
    const auto *meshBounds = SubmittedUnderstoryMeshBounds3D(descriptor, bounds);
    if (!meshBounds) return -1.f;
    clearance = (std::min)(clearance, SubmittedFootprintWalkingClearance3D(
        TransformSubmittedUnderstoryFootprint3D(descriptor, *meshBounds)));
  }
  return clearance;
}

// 固定 third / God 視角只量正式 186 個地被的提交 footprint proxy；不以增加
// descriptor、移動根部或放寬步道淨空換取密度。proxy 是實際高細節 mesh AABB
// 投影面積的總和，沒有量 rasterized pixels、重疊或遮擋；clearance 則以旋轉後
// 的正式 AABB 外緣保證不蓋入角色至屋前的 2.8m 步行帶。
bool FixedViewUnderstoryFootprintProxyContract(
    const std::vector<ShowcaseLandPlacement3D> &placements,
    const std::vector<ShowcaseSceneDescriptor3D> &descriptors,
    std::span<const Camera3D> cameras,
    const UnderstoryHighDetailBounds3D &meshBoundsByVariant) {
  if (descriptors.size() != placements.size() + 8u || cameras.size() != 2u)
    return false;
  const auto distanceToWalkingLane = [](Vec3 point) {
    return SampleShowcaseHeroTrail3D(point).distance;
  };
  std::array<float, 2> projectedFootprintProxy{};
  std::array<unsigned, 2> frustumCandidateGrass{};
  std::array<float, 3> clusterAreaSum{}, clusterAreaSquaredSum{};
  std::array<unsigned, 3> clusterSamples{};
  unsigned grassCount{}, stableKeysSeen{};
  float pathShoulderArea{};
  unsigned pathShoulderCount{};
  float minimumWalkingClearance = (std::numeric_limits<float>::max)();
  std::size_t minimumWalkingClearanceIndex{};
  float minimumWalkingLaneRootDistance{};
  bool rootsUnchanged = true;
  for (std::size_t index = 0; index < placements.size(); ++index) {
    const auto &placement = placements[index];
    if (placement.kind != ShowcaseLandKind3D::Grass) continue;
    const auto &descriptor = descriptors[index + 2u];
    const auto *meshBounds =
        SubmittedUnderstoryMeshBounds3D(descriptor, meshBoundsByVariant);
    if (!meshBounds) return false;
    const auto footprint =
        TransformSubmittedUnderstoryFootprint3D(descriptor, *meshBounds);
    ++grassCount;
    stableKeysSeen += placement.stableKey ? 1u : 0u;
    rootsUnchanged &= descriptor.position.x == placement.position.x &&
                      descriptor.position.y == placement.position.y &&
                      descriptor.position.z == placement.position.z;
    const float areaMultiplier =
        descriptor.scale.x * descriptor.scale.z /
        (placement.scale.x * placement.scale.z);
    const std::size_t zone =
        placement.zone == ShowcaseLandZone3D::Riverbank ? 0u
        : placement.zone == ShowcaseLandZone3D::Meadow ? 1u : 2u;
    clusterAreaSum[zone] += areaMultiplier;
    clusterAreaSquaredSum[zone] += areaMultiplier * areaMultiplier;
    ++clusterSamples[zone];
    const float laneDistance = distanceToWalkingLane(descriptor.position);
    const float submittedClearance =
        SubmittedFootprintWalkingClearance3D(footprint);
    if (submittedClearance < minimumWalkingClearance) {
      minimumWalkingClearance = submittedClearance;
      minimumWalkingClearanceIndex = index;
      minimumWalkingLaneRootDistance = laneDistance;
    }
    if (placement.band == ShowcaseLandBand3D::Near &&
        placement.zone != ShowcaseLandZone3D::Riverbank &&
        laneDistance >= 1.4f && laneDistance <= 5.25f) {
      pathShoulderArea += footprint.horizontalArea;
      ++pathShoulderCount;
    }
    for (std::size_t view = 0; view < cameras.size(); ++view) {
      if (!SphereIntersectsFrustum(footprint.worldCenter,
                                   footprint.sphereRadius, cameras[view],
                                   16.f / 9.f))
        continue;
      ++frustumCandidateGrass[view];
      if (cameras[view].projection.mode == ProjectionMode::Orthographic) {
        const float height = cameras[view].projection.orthographicHeight;
        projectedFootprintProxy[view] += footprint.horizontalArea /
            (height * height * (16.f / 9.f));
      } else {
        const Vec3 forward =
            Normalize(cameras[view].target - cameras[view].position);
        const float depth = (std::max)(
            .05f, Dot(footprint.worldCenter - cameras[view].position, forward));
        const float halfHeight = depth * std::tan(
            cameras[view].projection.verticalFieldOfView * .5f);
        projectedFootprintProxy[view] += footprint.horizontalArea /
            (4.f * halfHeight * halfHeight * (16.f / 9.f));
      }
    }
  }
  std::array<float, 3> clusterCoefficient{};
  for (std::size_t zone = 0; zone < clusterCoefficient.size(); ++zone) {
    const float mean = clusterSamples[zone]
                           ? clusterAreaSum[zone] / float(clusterSamples[zone])
                           : 0.f;
    const float variance = clusterSamples[zone]
        ? (std::max)(0.f, clusterAreaSquaredSum[zone] /
                              float(clusterSamples[zone]) - mean * mean)
        : 0.f;
    clusterCoefficient[zone] = mean > 0.f ? std::sqrt(variance) / mean : 0.f;
  }
  std::cout << "UNDERSTORY_FIXED_VIEW frustumCandidatesThirdGod="
            << frustumCandidateGrass[0] << '/' << frustumCandidateGrass[1]
            << " projectedFootprintProxy=" << projectedFootprintProxy[0]
            << '/' << projectedFootprintProxy[1]
            << " clusterCv=" << clusterCoefficient[0] << '/'
            << clusterCoefficient[1] << '/' << clusterCoefficient[2]
            << " pathShoulder=" << pathShoulderCount << '/'
            << pathShoulderArea << " walkingClearance="
            << minimumWalkingClearance << " clearanceIndex="
            << minimumWalkingClearanceIndex << " rootDistance="
            << minimumWalkingLaneRootDistance << " scale="
            << descriptors[minimumWalkingClearanceIndex + 2u].scale.x << '/'
            << descriptors[minimumWalkingClearanceIndex + 2u].scale.z
            << " yaw=" << descriptors[minimumWalkingClearanceIndex + 2u].yaw
            << " mesh="
            << descriptors[minimumWalkingClearanceIndex + 2u].meshAsset << '\n';
  return grassCount == 186u && stableKeysSeen == grassCount &&
         rootsUnchanged && frustumCandidateGrass[0] >= 72u &&
         frustumCandidateGrass[1] >= 48u &&
         projectedFootprintProxy[0] >= .75f &&
         projectedFootprintProxy[1] >= .14f && clusterCoefficient[0] >= .08f &&
         clusterCoefficient[1] >= .10f && clusterCoefficient[2] >= .07f &&
         pathShoulderCount >= 20u && pathShoulderArea >= 50.f &&
         minimumWalkingClearance >= 1.4f;
}

// 冠層與林下層都以正式 descriptor 對照配置尺度，避免 placement 的覆蓋率
// 通過後，提交前又縮成果園。面積是水平橢圓 proxy，不宣稱是畫面像素覆蓋。
bool ForestMidgroundLayerContract(
    const std::vector<ShowcaseLandPlacement3D> &placements,
    const std::vector<ShowcaseSceneDescriptor3D> &descriptors) {
  if (descriptors.size() != placements.size() + 8u) return false;
  std::array<float, 2> sourceCanopy{}, submittedCanopy{};
  std::array<float, 2> sourceUnderstory{}, submittedUnderstory{};
  std::array<unsigned, 2> livingTrees{}, shrubs{};
  for (std::size_t index = 0; index < placements.size(); ++index) {
    const auto &placement = placements[index];
    const auto band = std::uint32_t(placement.band);
    if (band >= 2u) continue;
    const auto &descriptor = descriptors[index + 2u];
    const float sourceArea = placement.scale.x * placement.scale.z;
    const float submittedArea = descriptor.scale.x * descriptor.scale.z;
    if (placement.kind == ShowcaseLandKind3D::Tree &&
        placement.variant != ShowcaseLandVariant3D::DeadTreeTrunk) {
      sourceCanopy[band] += sourceArea;
      submittedCanopy[band] += submittedArea;
      ++livingTrees[band];
    } else if (placement.kind == ShowcaseLandKind3D::Grass &&
               placement.zone == ShowcaseLandZone3D::Upland) {
      sourceUnderstory[band] += sourceArea;
      submittedUnderstory[band] += submittedArea;
      ++shrubs[band];
    }
  }
  bool valid = livingTrees == std::array<unsigned, 2>{114, 84} &&
               shrubs == std::array<unsigned, 2>{38, 18};
  for (unsigned band = 0; band < 2u; ++band) {
    const float canopyRatio = sourceCanopy[band] > 0.f
        ? submittedCanopy[band] / sourceCanopy[band] : 0.f;
    const float understoryRatio = sourceUnderstory[band] > 0.f
        ? submittedUnderstory[band] / sourceUnderstory[band] : 0.f;
    std::cout << "FOREST_MIDGROUND band=" << band
              << " living=" << livingTrees[band] << " shrubs=" << shrubs[band]
              << " canopyAreaRetention=" << canopyRatio
              << " understoryAreaMultiplier=" << understoryRatio << '\n';
    // 真實葉片淨空會只縮減侵入步道的少數 Near 群落；仍要求兩個 seed 的
    // Upland 提交面積至少比 placement proxy 高 52%，但不以越界葉片湊門檻。
    valid &= canopyRatio >= 1.10f && canopyRatio <= 1.55f &&
             understoryRatio >= 1.52f && understoryRatio <= 1.80f;
  }
  unsigned eligible{}, covered{};
  for (int z = -160; z < 160; ++z)
    for (int x = -160; x < 160; ++x) {
      const float dx = (float(x) + .5f) * .5f;
      const float dz = (float(z) + .5f) * .5f;
      if (dx * dx + dz * dz > 80.f * 80.f) continue;
      ++eligible;
      for (const auto &descriptor : descriptors) {
        if (descriptor.kind != ShowcaseSceneKind3D::Tree ||
            descriptor.meshAsset.find("dead_tree") != std::string::npos)
          continue;
        const float u = (-16.f + dx - descriptor.position.x) /
                        (descriptor.scale.x * .5f);
        const float v = (8.f + dz - descriptor.position.z) /
                        (descriptor.scale.z * .5f);
        if (u * u + v * v <= 1.f) { ++covered; break; }
      }
    }
  const float coverage = eligible ? float(covered) / float(eligible) : 0.f;
  std::cout << "FOREST_MIDGROUND ROI80UnrotatedEllipseFraction=" << coverage
            << '\n';
  valid &= coverage >= .24f;
  return valid;
}

// 固定 first/third/god 視角各自把可見活樹依相機深度排序為近／中／遠三段；
// 每段都必須同時包含成熟樹、幼樹、低層與高層，避免只用整體跨度掩蓋其中
// 一段仍是等高樹牆。水平 transform 逐值對照正式 form，確保輪廓調整沒有
// 侵入河岸、道路或屋舍淨空。
bool FixedViewForestDepthContract(
    const std::vector<ShowcaseLandPlacement3D> &placements,
    const std::vector<ShowcaseSceneDescriptor3D> &descriptors,
    std::span<const Camera3D> cameras) {
  if (descriptors.size() != placements.size() + 8u || cameras.size() != 3u)
    return false;
  constexpr std::array<float, 3> projectedHeightAggregateLimit{
      14.70f, 13.55f, 13.50f};
  bool valid = true;
  for (std::size_t index = 0; index < placements.size(); ++index) {
    const auto &placement = placements[index];
    if (placement.kind != ShowcaseLandKind3D::Tree)
      continue;
    const auto &descriptor = descriptors[index + 2u];
    const auto form = ShowcaseTreeFormVariation(placement);
    valid &= descriptor.position.x == placement.position.x &&
             descriptor.position.z == placement.position.z &&
             descriptor.scale.x == placement.scale.x * form.scaleMultiplier.x &&
             descriptor.scale.z == placement.scale.z * form.scaleMultiplier.z;
  }
  for (std::size_t view = 0; view < cameras.size(); ++view) {
    const auto forward = Normalize(cameras[view].target - cameras[view].position);
    struct VisibleTree {
      float depth{}, normalizedHeight{}, projectedHeight{};
      bool sapling{};
    };
    std::vector<VisibleTree> visible;
    for (std::size_t index = 0; index < placements.size(); ++index) {
      const auto &placement = placements[index];
      if (placement.kind != ShowcaseLandKind3D::Tree ||
          placement.variant == ShowcaseLandVariant3D::DeadTreeTrunk)
        continue;
      const auto &descriptor = descriptors[index + 2u];
      const float radius = Length(descriptor.scale) * .5f;
      if (!SphereIntersectsFrustum(descriptor.position, radius, cameras[view],
                                   16.f / 9.f))
        continue;
      const float normalizedHeight = descriptor.scale.y / placement.scale.y;
      const float depth = Dot(descriptor.position - cameras[view].position,
                              forward);
      // 完整 360° 遠景森林牆會包含相機後方樹木。舊 proxy 把負深度
      // clamp 到 5cm 後仍加總，將不可見背後樹誤報成巨量 overdraw；正式
      // 前向近裁面之外才屬於此固定鏡頭的可見深度樣本。
      if (depth <= cameras[view].projection.nearPlane)
        continue;
      const float projectedHeight = cameras[view].projection.mode ==
                                            ProjectionMode::Orthographic
                                        ? descriptor.scale.y /
                                              cameras[view].projection
                                                  .orthographicHeight
                                        : descriptor.scale.y * .5f /
                                              ((std::max)(depth, .05f) *
                                               std::tan(cameras[view].projection
                                                            .verticalFieldOfView *
                                                        .5f));
      visible.push_back({depth, normalizedHeight, projectedHeight,
                         placement.variant ==
                             ShowcaseLandVariant3D::TreeSmall02});
    }
    std::ranges::sort(visible, {}, &VisibleTree::depth);
    const float depthSpan = visible.empty()
                                ? 0.f
                                : visible.back().depth - visible.front().depth;
    float projectedHeightAggregate{};
    for (const auto &tree : visible)
      projectedHeightAggregate += tree.projectedHeight;
    valid &= visible.size() >= 24u && depthSpan >= 30.f &&
             std::isfinite(projectedHeightAggregate) &&
             projectedHeightAggregate <= projectedHeightAggregateLimit[view];
    for (unsigned slice = 0; slice < 3u; ++slice) {
      const std::size_t begin = visible.size() * slice / 3u;
      const std::size_t end = visible.size() * (slice + 1u) / 3u;
      unsigned saplings{}, mature{}, lowerTier{}, upperTier{};
      for (std::size_t index = begin; index < end; ++index) {
        saplings += visible[index].sapling ? 1u : 0u;
        mature += visible[index].sapling ? 0u : 1u;
        lowerTier += visible[index].normalizedHeight <= .90f ? 1u : 0u;
        upperTier += visible[index].normalizedHeight >= .98f ? 1u : 0u;
      }
      std::cout << "FOREST_FIXED_VIEW_DEPTH view=" << view
                << " slice=" << slice << " count=" << end - begin
                << " mature=" << mature << " sapling=" << saplings
                << " lower=" << lowerTier << " upper=" << upperTier
                << " depthSpan=" << depthSpan
                << " projectedHeightAggregate=" << projectedHeightAggregate
                << '/' << projectedHeightAggregateLimit[view]
                << '\n';
      const auto minimumTier =
          (std::max)(2u, unsigned((end - begin + 4u) / 5u));
      valid &= end - begin >= 8u && mature >= 2u && saplings >= 1u &&
               lowerTier >= minimumTier && upperTier >= minimumTier;
    }
  }
  return valid;
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
  const float redTransmission = WaterTransmission3D(value.absorption.x, 2.f);
  const float greenTransmission = WaterTransmission3D(value.absorption.y, 2.f);
  const float blueTransmission = WaterTransmission3D(value.absorption.z, 2.f);
  return value.reflectance >= .019f && value.reflectance <= .023f &&
         value.minimumRoughness >= .05f && value.minimumRoughness <= .06f &&
         value.maximumRoughness >= .18f && value.maximumRoughness <= .22f &&
         value.broadWaveScale >= .12f && value.broadWaveScale <= .16f &&
         value.broadWaveStrength >= .14f && value.broadWaveStrength <= .18f &&
         value.broadWaveSpeed >= .15f && value.broadWaveSpeed <= .22f &&
         value.detailWaveScale >= .85f && value.detailWaveScale <= 1.f &&
         value.detailWaveStrength >= .045f && value.detailWaveStrength <= .065f &&
         value.detailWaveSpeed >= .38f && value.detailWaveSpeed <= .48f &&
         value.broadWaveStrength > value.detailWaveStrength * 2.5f &&
         value.foamStartDepth >= .02f && value.foamStartDepth <= .035f &&
         value.foamEndDepth >= .15f && value.foamEndDepth <= .17f &&
         value.foamStrength >= .24f && value.foamStrength <= .30f &&
         value.shallowAlpha >= .42f && value.shallowAlpha <= .50f &&
         value.deepAlpha >= .84f && value.deepAlpha <= .90f &&
         value.deepAlpha - value.shallowAlpha >= .36f &&
         value.maximumNightRadiance >= .025f &&
         value.maximumNightRadiance <= .04f &&
         redTransmission < greenTransmission &&
         greenTransmission < blueTransmission &&
         blueTransmission - redTransmission < .30f;
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

bool DuskShadowColorSeparated(
    const EnvironmentLightingBlendSettings3D &environment) {
  const float luminance = .2126f * environment.objectFillRadiance.x +
                          .7152f * environment.objectFillRadiance.y +
                          .0722f * environment.objectFillRadiance.z;
  return environment.objectFillIntensity >= .71f &&
         environment.objectFillIntensity <= .73f &&
         luminance >= .275f && luminance <= .29f &&
         environment.objectFillRadiance.z >
             environment.objectFillRadiance.x * 1.6f &&
         environment.objectFillRadiance.y >
             environment.objectFillRadiance.x * 1.25f;
}

bool NightForestReadable(const ShowcaseLightingState3D &state,
                         const EnvironmentLightingBlendSettings3D &environment,
                         const PhaseRadianceProfile3D &radiance) {
  const float skyLuminance = .2126f * state.sky.r + .7152f * state.sky.g +
                             .0722f * state.sky.b;
  return state.profile == ShowcaseLightingProfile3D::Night &&
         state.exposureEv <= -.10f && state.sun.ambient >= .418f &&
         state.sun.ambient <= .422f &&
         state.sun.color.b > state.sun.color.g &&
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
  if (dusk[0].position.x != GameplayCottagePorchLanternCenter3D.x ||
      dusk[0].position.y != GameplayCottagePorchLanternCenter3D.y ||
      dusk[0].position.z != GameplayCottagePorchLanternCenter3D.z)
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
         tone.duskExposureEv == .6f && tone.nightExposureEv >= 1.29f &&
         tone.nightExposureEv <= 1.31f && totalNightEv >= 1.14f &&
         totalNightEv <= 1.16f && tone.shadowLift >= .038f &&
         tone.shadowLift <= .042f && tone.toeStrength >= .30f &&
         tone.toeStrength <= .36f && tone.shoulderStrength >= 1.08f &&
         tone.shoulderStrength <= 1.18f;
}
} // namespace

int main() {
  const float minimumTreeBurial =
      ShowcaseTreeBurialOffset3D("island_tree_01", 1.f);
  const float scaledTreeBurial =
      ShowcaseTreeBurialOffset3D("island_tree_01", 10.f);
  const float maximumTreeBurial =
      ShowcaseTreeBurialOffset3D("tree_small_02", 30.f);
  const float firTreeBurial =
      ShowcaseTreeBurialOffset3D("fir_tree_01_c", 10.f);
  const float invalidScaleBurial = ShowcaseTreeBurialOffset3D(
      "tree_small_02", std::numeric_limits<float>::quiet_NaN());
  const float deadTreeBurial =
      ShowcaseTreeBurialOffset3D("dead_tree_trunk_02", 30.f);
  if (!std::isfinite(minimumTreeBurial) ||
      !std::isfinite(scaledTreeBurial) ||
      !std::isfinite(maximumTreeBurial) ||
      !std::isfinite(invalidScaleBurial) || minimumTreeBurial != .10f ||
      maximumTreeBurial != .20f || invalidScaleBurial != 0.f ||
      minimumTreeBurial > scaledTreeBurial ||
      scaledTreeBurial > maximumTreeBurial ||
      std::abs(scaledTreeBurial - .12f) > 1e-6f ||
      std::abs(firTreeBurial - scaledTreeBurial) > 1e-6f ||
      deadTreeBurial != 0.f ||
      ShowcaseTreeBurialOffset3D("unknown_tree", 30.f) != 0.f)
    return 100;

  const auto materialFixture=[](char seed){
    CookedRenderMaterial3D material{};
    material.baseColorBytes=material.surfaceBytes=material.normalBytes=128u;
    material.baseColorSha256=std::string(64,seed);
    material.surfaceSha256=std::string(64,char(seed+1));
    material.normalSha256=std::string(64,char(seed+2));
    material.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
    return material;
  };
  auto forestMaterial=materialFixture('1');
  auto groundMaterial=materialFixture('4');
  auto pathMaterial=materialFixture('7');
  std::array<const CookedRenderMaterial3D*,3> materialLayers{
      &forestMaterial,&groundMaterial,&pathMaterial};
  ShowcaseTerrainMaterialReceipt3D materialReceipt{};
  if(!ValidateShowcaseTerrainMaterialSources3D(materialLayers,&materialReceipt)||
      !materialReceipt.semanticIdentities[0]||
      !materialReceipt.semanticIdentities[1]||
      !materialReceipt.semanticIdentities[2])
    return 90;
  auto duplicateLayers=materialLayers;
  duplicateLayers[2]=duplicateLayers[0];
  if(ValidateShowcaseTerrainMaterialSources3D(duplicateLayers,&materialReceipt)||
      materialReceipt.failure!=ShowcaseTerrainMaterialFailure3D::DuplicateSemanticIdentity)
    return 91;
  groundMaterial.surfaceEncoding=MaterialSurfaceMapEncoding3D::RoughnessR;
  if(ValidateShowcaseTerrainMaterialSources3D(materialLayers,&materialReceipt)||
      materialReceipt.failure!=ShowcaseTerrainMaterialFailure3D::InvalidPackedOrm)
    return 92;
  groundMaterial.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
  constexpr auto terrainTint = ShowcaseForestTerrainTint3D();
  constexpr Color priorTerrainTint{.46f, .52f, .39f, 1.f};
  if (terrainTint.g < terrainTint.r + .055f ||
      terrainTint.g < terrainTint.b || terrainTint.r > .72f ||
      Rec709(terrainTint) < .50f || Rec709(terrainTint) > .52f ||
      std::abs(terrainTint.r - priorTerrainTint.r) > .02f ||
      std::abs(terrainTint.g - priorTerrainTint.g) > .02f ||
      std::abs(terrainTint.b - priorTerrainTint.b) > .02f ||
      terrainTint.a != 1.f)
    return 63;
  const std::array<ShowcaseSceneDescriptor3D, 1> ecologyTrees{{
      {ShowcaseSceneKind3D::Tree, {40, 0, 40}, {12, 10, 10}}}};
  const auto canopyCenter =
      SampleShowcaseTerrainEcologyTint3D({40, 0, 40}, ecologyTrees);
  const auto canopyFar =
      SampleShowcaseTerrainEcologyTint3D({100, 0, 100}, ecologyTrees);
  const auto cottagePath =
      SampleShowcaseTerrainEcologyTint3D({-16, 0, 8}, ecologyTrees);
  const auto cottagePathRepeated =
      SampleShowcaseTerrainEcologyTint3D({-16, 0, 8}, ecologyTrees);
  const auto cottagePathShoulderProbe =
      SampleShowcaseTerrainEcologyTint3D({-15.5f, 0, 12}, ecologyTrees);
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
  const auto pathDominantHandoff =
      ResolveShowcaseTerrainPathWetHandoff3D(.8f, .3f);
  const auto wetDominantHandoff =
      ResolveShowcaseTerrainPathWetHandoff3D(.2f, .7f);
  const auto balancedHandoff =
      ResolveShowcaseTerrainPathWetHandoff3D(.75f, .75f);
  const auto nearPathHandoff =
      ResolveShowcaseTerrainPathWetHandoff3D(.51f, .5f);
  const auto nearWetHandoff =
      ResolveShowcaseTerrainPathWetHandoff3D(.5f, .51f);
  const auto invalidHandoff = ResolveShowcaseTerrainPathWetHandoff3D(
      (std::numeric_limits<float>::quiet_NaN)(), .7f);
  const auto validAuthoredBlend = [](const auto &sample) {
    const std::array weights{sample.cottagePath, sample.leafLitter,
                             sample.riverWetSoil};
    float total{};
    for (const float weight : weights) {
      if (!std::isfinite(weight) || weight < 0.f || weight > 1.f)
        return false;
      total += weight;
    }
    return total <= 1.f + 1e-6f &&
           !(sample.cottagePath > 0.f && sample.riverWetSoil > 0.f);
  };
  // The authored hero trail must remain connected down its centre while
  // reading as worn ground rather than a uniform road.  Its full-strength
  // tread is deliberately wider than one terrain sample so it cannot vanish
  // between adjacent one-metre vertices.
  float minimumPathCentre = 1.f;
  float minimumPathShoulder = 1.f;
  float maximumPathShoulder{};
  float maximumPathAsymmetry{};
  float maximumPathOuter{};
  float pathShoulderSum{};
  unsigned pathShoulderSamples{};
  for (const float z : {9.f, 12.f, 15.f, 18.f}) {
    const Vec3 start = z <= 14.f ? Vec3{-16.f, 0.f, 8.f}
                                     : Vec3{-14.f, 0.f, 14.f};
    const Vec3 end = z <= 14.f ? Vec3{-14.f, 0.f, 14.f}
                                   : Vec3{-12.f, 0.f, 19.8f};
    const float t = (z - start.z) / (end.z - start.z);
    const Vec3 centre{start.x + (end.x - start.x) * t, 0.f, z};
    const float tangentLength =
        std::hypot(end.x - start.x, end.z - start.z);
    const Vec3 normal{-(end.z - start.z) / tangentLength, 0.f,
                      (end.x - start.x) / tangentLength};
    const auto sampleAt = [&](float offset) {
      return SampleShowcaseTerrainEcologyTint3D(
                 {centre.x + normal.x * offset, 0.f,
                  centre.z + normal.z * offset},
                 ecologyTrees)
          .cottagePath;
    };
    minimumPathCentre = (std::min)(minimumPathCentre, sampleAt(0.f));
    const float leftShoulder = sampleAt(-1.15f);
    const float rightShoulder = sampleAt(1.15f);
    minimumPathShoulder =
        (std::min)(minimumPathShoulder,
                   (std::min)(leftShoulder, rightShoulder));
    maximumPathShoulder =
        (std::max)(maximumPathShoulder,
                   (std::max)(leftShoulder, rightShoulder));
    maximumPathAsymmetry =
        (std::max)(maximumPathAsymmetry,
                   std::abs(leftShoulder - rightShoulder));
    maximumPathOuter =
        (std::max)(maximumPathOuter,
                   (std::max)(sampleAt(-2.f), sampleAt(2.f)));
    pathShoulderSum += leftShoulder + rightShoulder;
    pathShoulderSamples += 2u;
  }
  const float meanPathShoulder = pathShoulderSum / pathShoulderSamples;
  if (minimumPathCentre < .62f || maximumPathOuter > .08f ||
      maximumPathShoulder - minimumPathShoulder < .18f ||
      minimumPathShoulder < .18f || maximumPathShoulder > .75f ||
      maximumPathAsymmetry < .12f || meanPathShoulder < .30f ||
      meanPathShoulder > .60f) {
    std::cerr << "cottage path profile divergence centre="
              << minimumPathCentre << " shoulder=" << minimumPathShoulder
              << '/' << maximumPathShoulder << '/' << meanPathShoulder
              << " asymmetry=" << maximumPathAsymmetry
              << " outer=" << maximumPathOuter << '\n';
    return 101;
  }
  // R87 keeps the maintained tread clear while replacing the broad neutral
  // forecourt beside it with a deterministic, irregular leaf-litter band.
  // Empty descriptors isolate the authored trail shoulder from tree canopies;
  // all probes are world-space and therefore must remain camera independent.
  float minimumAuthoredShoulder = 1.f;
  float maximumLeafShoulder{};
  float maximumLeafEdgeVariation{};
  for (const float z : {9.f, 12.f, 15.f, 18.f}) {
    const Vec3 start = z <= 14.f ? Vec3{-16.f, 0.f, 8.f}
                                     : Vec3{-14.f, 0.f, 14.f};
    const Vec3 end = z <= 14.f ? Vec3{-14.f, 0.f, 14.f}
                                   : Vec3{-12.f, 0.f, 19.8f};
    const float t = (z - start.z) / (end.z - start.z);
    const Vec3 centre{start.x + (end.x - start.x) * t, 0.f, z};
    const float tangentLength =
        std::hypot(end.x - start.x, end.z - start.z);
    const Vec3 normal{-(end.z - start.z) / tangentLength, 0.f,
                      (end.x - start.x) / tangentLength};
    const auto sampleAt = [&](float offset) {
      return SampleShowcaseTerrainEcologyTint3D(
          {centre.x + normal.x * offset, 0.f,
           centre.z + normal.z * offset},
          {});
    };
    const auto core = sampleAt(0.f);
    const auto leftShoulder = sampleAt(-3.f);
    const auto rightShoulder = sampleAt(3.f);
    const auto edgeA = sampleAt(-4.7f);
    const auto edgeB = sampleAt(4.7f);
    const auto exterior = sampleAt(6.5f);
    minimumAuthoredShoulder =
        (std::min)(minimumAuthoredShoulder,
                   (std::min)(leftShoulder.leafLitter +
                                  leftShoulder.riverWetSoil,
                              rightShoulder.leafLitter +
                                  rightShoulder.riverWetSoil));
    maximumLeafShoulder =
        (std::max)(maximumLeafShoulder,
                   (std::max)(leftShoulder.leafLitter,
                              rightShoulder.leafLitter));
    maximumLeafEdgeVariation =
        (std::max)(maximumLeafEdgeVariation,
                   std::abs(edgeA.leafLitter - edgeB.leafLitter));
    if (core.leafLitter != 0.f || core.cottagePath < .62f ||
        leftShoulder.cottagePath != 0.f ||
        rightShoulder.cottagePath != 0.f ||
        exterior.leafLitter != 0.f || exterior.cottagePath != 0.f ||
        !validAuthoredBlend(core) || !validAuthoredBlend(leftShoulder) ||
        !validAuthoredBlend(rightShoulder) ||
        !validAuthoredBlend(exterior)) {
      std::cerr << "hero trail leaf shoulder topology divergence z=" << z
                << " core=" << core.cottagePath << '/' << core.leafLitter
                << " shoulder=" << leftShoulder.leafLitter << '/'
                << rightShoulder.leafLitter << " exterior="
                << exterior.cottagePath << '/' << exterior.leafLitter
                << '\n';
      return 104;
    }
  }
  if (minimumAuthoredShoulder < .45f || maximumLeafShoulder < .55f ||
      maximumLeafShoulder > .82f ||
      maximumLeafEdgeVariation < .03f) {
    std::cerr << "hero trail leaf shoulder profile divergence shoulder="
              << minimumAuthoredShoulder << '/' << maximumLeafShoulder
              << " edgeVariation=" << maximumLeafEdgeVariation << '\n';
    return 105;
  }
  for (const Vec3 control : {Vec3{-28.f, 0.f, -8.f},
                             Vec3{-25.5f, 0.f, 1.f},
                             Vec3{-16.f, 0.f, 8.f},
                             Vec3{-14.f, 0.f, 14.f},
                             Vec3{-12.f, 0.f, 19.8f}}) {
    const auto trail = SampleShowcaseHeroTrail3D(control);
    const auto terrain =
        SampleShowcaseTerrainEcologyTint3D(control, ecologyTrees);
    if (!trail.valid || trail.distance > 1e-5f ||
        terrain.cottagePath < .55f ||
        !ShowcaseHeroTrailWalkingLane3D(control, 0.f)) {
      std::cerr << "hero trail control divergence at " << control.x << ','
                << control.z << " distance=" << trail.distance
                << " weight=" << terrain.cottagePath << '\n';
      return 102;
    }
  }
  const auto invalidTrail = SampleShowcaseHeroTrail3D(
      {(std::numeric_limits<float>::quiet_NaN)(), 0.f, 0.f});
  if (invalidTrail.valid ||
      ShowcaseHeroTrailWalkingLane3D({100.f, 0.f, 100.f}, 0.f) ||
      ShowcaseHeroTrailWalkingLane3D({-16.f, 0.f, 8.f}, -1.f))
    return 103;
  if (canopyCenter.canopy < .99f || canopyCenter.leafLitter < .99f ||
      canopyCenter.biome != ShowcaseTerrainBiome3D::CanopyLeafLitter ||
      canopyFar.canopy != 0.f ||
      Rec709(canopyCenter.tint) >= Rec709(canopyFar.tint) ||
      cottagePath.cottagePath < .74f ||
      cottagePath.biome != ShowcaseTerrainBiome3D::CottagePath ||
      cottagePathShoulderProbe.cottagePath < .20f ||
      cottagePathShoulderProbe.biome != ShowcaseTerrainBiome3D::CottagePath ||
      cottagePath.tint.r != cottagePathRepeated.tint.r ||
      cottagePath.tint.g != cottagePathRepeated.tint.g ||
      cottagePath.tint.b != cottagePathRepeated.tint.b ||
      cottagePath.cottagePath != cottagePathRepeated.cottagePath ||
      wetBank.riverMoisture < .6f || wetBank.riverWetSoil < .6f ||
      wetBank.biome != ShowcaseTerrainBiome3D::RiverWetSoil ||
      overlappingBank.biome != ShowcaseTerrainBiome3D::RiverWetSoil ||
      overlappingBank.riverWetSoil < .6f || overlappingBank.leafLitter <= 0.f ||
      dryGround.riverMoisture != 0.f ||
      dryGround.biome != ShowcaseTerrainBiome3D::Neutral ||
      Rec709(wetBank.tint) >= Rec709(dryGround.tint) ||
      Chroma(cottagePath.tint) <= Chroma(canopyCenter.tint) ||
      canopyCenter.tint.g <= canopyCenter.tint.r ||
      wetBank.tint.b <= wetBank.tint.r ||
      !validAuthoredBlend(canopyCenter) ||
      !validAuthoredBlend(cottagePath) ||
      !validAuthoredBlend(wetBank) ||
      !validAuthoredBlend(overlappingBank) ||
      !validAuthoredBlend(dryGround) ||
      invalidEcology.tint.r != 1.f || invalidEcology.tint.g != 1.f ||
      invalidEcology.tint.b != 1.f || invalidEcology.tint.a != 1.f ||
      invalidEcology.canopy != 0.f || invalidEcology.riverMoisture != 0.f ||
      invalidEcology.cottagePath != 0.f || invalidEcology.leafLitter != 0.f ||
      invalidEcology.riverWetSoil != 0.f ||
      invalidEcology.biome != ShowcaseTerrainBiome3D::Neutral ||
      std::abs(pathDominantHandoff.cottagePath - .5f) > 1e-6f ||
      pathDominantHandoff.riverWetSoil != 0.f ||
      wetDominantHandoff.cottagePath != 0.f ||
      std::abs(wetDominantHandoff.riverWetSoil - .5f) > 1e-6f ||
      balancedHandoff.cottagePath != 0.f ||
      balancedHandoff.riverWetSoil != 0.f ||
      nearPathHandoff.cottagePath <= 0.f ||
      nearPathHandoff.cottagePath > .011f ||
      nearPathHandoff.riverWetSoil != 0.f ||
      nearWetHandoff.cottagePath != 0.f ||
      nearWetHandoff.riverWetSoil <= 0.f ||
      nearWetHandoff.riverWetSoil > .011f ||
      invalidHandoff.cottagePath != 0.f ||
      invalidHandoff.riverWetSoil != 0.f) {
    std::cerr << "terrain ecology divergence canopy=" << canopyCenter.canopy
              << '/' << canopyCenter.leafLitter << '/'
              << unsigned(canopyCenter.biome) << " path="
              << cottagePath.cottagePath << '/' << unsigned(cottagePath.biome)
              << " shoulderProbe=" << cottagePathShoulderProbe.cottagePath
              << '/' << unsigned(cottagePathShoulderProbe.biome)
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
  const auto wetCore=SampleShowcaseTerrainEcologyTint3D({-4,0,0},{});
  const auto wetShoulder=SampleShowcaseTerrainEcologyTint3D({7,0,0},{});
  const auto wetFar=SampleShowcaseTerrainEcologyTint3D({20,0,0},{});
  for (const float z : {-96.f, -48.f, 0.f, 48.f, 96.f}) {
    const float center = -4.f + std::sin(z * .035f) * 5.f +
                         std::sin(z * .011f) * 2.f;
    const float width = 12.f + std::sin(z * .057f + .7f) * 2.6f;
    const Vec3 shoulderPoint{center + width * .5f + 3.f, 0.f, z};
    const std::array<ShowcaseSceneDescriptor3D, 1> shoulderTree{{
        {ShowcaseSceneKind3D::Tree, shoulderPoint, {12, 10, 10}}}};
    const auto openShoulder =
        SampleShowcaseTerrainEcologyTint3D(shoulderPoint, {});
    const auto coveredShoulder =
        SampleShowcaseTerrainEcologyTint3D(shoulderPoint, shoulderTree);
    if (openShoulder.riverWetSoil <= 0.f ||
        openShoulder.riverWetSoil >= .95f ||
        coveredShoulder.riverWetSoil != openShoulder.riverWetSoil ||
        coveredShoulder.leafLitter <= 0.f ||
        coveredShoulder.cottagePath != 0.f ||
        coveredShoulder.riverWetSoil + coveredShoulder.leafLitter >
            1.f + 1e-6f) {
      std::cerr << "covered river shoulder divergence z=" << z
                << " open=" << openShoulder.riverWetSoil
                << " covered=" << coveredShoulder.riverWetSoil << '/'
                << coveredShoulder.leafLitter << '\n';
      return 94;
    }
  }
  float maximumWetStep{};
  float previousWet=wetCore.riverWetSoil;
  for(float x=-3.75f;x<=20.f;x+=.25f){
    const float wet=SampleShowcaseTerrainEcologyTint3D({x,0,0},{}).riverWetSoil;
    maximumWetStep=(std::max)(maximumWetStep,std::abs(wet-previousWet));
    previousWet=wet;
  }
  if(wetCore.riverWetSoil<.99f||wetShoulder.riverWetSoil<=.05f||
      wetShoulder.riverWetSoil>=.95f||wetFar.riverWetSoil!=0.f||
      maximumWetStep>.20f){
    std::cerr<<"river wet-band continuity divergence core="
             <<wetCore.riverWetSoil<<" shoulder="<<wetShoulder.riverWetSoil
             <<" far="<<wetFar.riverWetSoil
             <<" maxStep="<<maximumWetStep<<'\n';
    return 93;
  }
  // 正式 hero terrain 在此區為 1m 網格。掃描相鄰頂點而非只測單點，
  // 鎖住 path/wet 的連續 signed-U 交接；舊 winner-takes-all 最大跳幅
  // 會超過 1.5，並觸發共享頂點成片清零。
  float maximumAuthoredSignedStep{};
  Vec3 maximumAuthoredSignedStepAt{};
  std::size_t crossSignedEdges{};
  std::array<float, 81> previousRow{};
  for (int zIndex = 0; zIndex <= 296; ++zIndex) {
    const float z = -148.f + float(zIndex);
    float previousInRow{};
    for (int xIndex = 0; xIndex <= 80; ++xIndex) {
      const float x = -40.f + float(xIndex);
      const auto encoded = EncodeShowcaseTerrainBiomeUv3D(
          SampleShowcaseTerrainEcologyTint3D({x, 0, z}, ecologyTrees));
      const auto inspectEdge = [&](float neighbour) {
        const float step = std::abs(encoded.x - neighbour);
        if (step > maximumAuthoredSignedStep) {
          maximumAuthoredSignedStep = step;
          maximumAuthoredSignedStepAt = {x, 0.f, z};
        }
        crossSignedEdges +=
            (encoded.x < 0.f && neighbour > 0.f) ||
            (encoded.x > 0.f && neighbour < 0.f);
      };
      if (xIndex)
        inspectEdge(previousInRow);
      if (zIndex)
        inspectEdge(previousRow[std::size_t(xIndex)]);
      previousInRow = encoded.x;
      previousRow[std::size_t(xIndex)] = encoded.x;
    }
  }
  if (maximumAuthoredSignedStep > .90f || !crossSignedEdges) {
    std::cerr << "terrain authored handoff divergence maxStep="
              << maximumAuthoredSignedStep
              << " at=" << maximumAuthoredSignedStepAt.x << ','
              << maximumAuthoredSignedStepAt.z
              << " crossSignedEdges=" << crossSignedEdges << '\n';
    return 95;
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
  float middleRidgeMinimum=(std::numeric_limits<float>::max)();
  float farRidgeMinimum=(std::numeric_limits<float>::max)();
  float farRidgeMaximum=-(std::numeric_limits<float>::max)();
  std::array<float,512> farRidgeRelief{};
  const std::size_t apronStart=129u*129u;
  for(std::size_t i=apronStart;i<apron.vertices.size();++i){const auto& v=apron.vertices[i];
    const auto decodedNormal=UnpackNormalSnorm8(v.normal);
    if(!std::isfinite(v.y)||!std::isfinite(decodedNormal.x)||!std::isfinite(decodedNormal.y)||
        !std::isfinite(decodedNormal.z)||decodedNormal.y<=.25f||
        ((v.color>>24)&0xffu)!=0xffu||
        v.u!=v.x*.085f||v.v!=v.z*.085f||std::abs(v.x)>1024.f||std::abs(v.z)>1024.f)return 44;
    const std::size_t ring=(i-apronStart)/512u;
    const float distance=std::array{8.f,24.f,64.f,256.f,768.f}[ring];
    const float expansion=1.f+distance/256.f;
    const float sourceX=v.x/expansion,sourceZ=v.z/expansion;
    const float sourceY=.01f*sourceX+.02f*sourceZ;
    const float dx=sourceX/256.f*distance,dz=sourceZ/256.f*distance;
    const auto sourceNormal=UnpackNormalSnorm8(originalApron.vertices[0].normal);
    const float slope=-(sourceNormal.x*dx+sourceNormal.z*dz)/sourceNormal.y;
    const float relief=v.y-sourceY-slope/(1.f+distance/32.f);
    if(ring==2u)middleRidgeMinimum=(std::min)(middleRidgeMinimum,relief);
    if(ring==4u){
      const std::size_t ringIndex=(i-apronStart)%512u;
      farRidgeRelief[ringIndex]=relief;
      farRidgeMinimum=(std::min)(farRidgeMinimum,relief);
      farRidgeMaximum=(std::max)(farRidgeMaximum,relief);
    }
  }
  unsigned farRidgeTurningPoints{};
  float farRidgeSecondDifference{};
  float farRidgeMaximumSecondDifference{};
  for(std::size_t i=0;i<farRidgeRelief.size();++i){
    const float previous=farRidgeRelief[(i+farRidgeRelief.size()-1)%farRidgeRelief.size()];
    const float current=farRidgeRelief[i];
    const float next=farRidgeRelief[(i+1)%farRidgeRelief.size()];
    const float incoming=current-previous,outgoing=next-current;
    farRidgeTurningPoints+=incoming*outgoing<0.f;
    const float second=std::abs(next-2.f*current+previous);
    farRidgeSecondDifference+=second;
    farRidgeMaximumSecondDifference=(std::max)(farRidgeMaximumSecondDifference,second);
  }
  farRidgeSecondDifference/=float(farRidgeRelief.size());
  const auto& nearApronColor=apron.vertices[apronStart].color;
  const auto& farApronColor=apron.vertices[apronStart+4u*512u].color;
  if(nearApronColor!=0xff778899u||
      !((farApronColor&0xffu)<(nearApronColor&0xffu))||
      !(((farApronColor>>8)&0xffu)<((nearApronColor>>8)&0xffu))||
      !(((farApronColor>>16)&0xffu)<((nearApronColor>>16)&0xffu))||
      !(middleRidgeMinimum>.25f)||!(farRidgeMinimum>35.f)||
      !(farRidgeMaximum-farRidgeMinimum>100.f)||
      farRidgeTurningPoints<48u||farRidgeSecondDifference<.9f||
      farRidgeMaximumSecondDifference>20.f)return 97;
  auto encodedApron = apron;
  ApplyShowcaseTerrainBiomeUv3D(encodedApron, 129u * 129u, ecologyTrees);
  if (!ValidateShowcaseTerrainBiomeUv3D(encodedApron, 129u * 129u))
    return 79;
  bool foundPath{}, foundLeaf{}, foundWet{}, foundNeutral{};
  std::array<std::size_t,3> materialCoverage{};
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
    const auto expected = EncodeShowcaseTerrainBiomeUv3D(
        SampleShowcaseTerrainEcologyTint3D(
            {before.x, before.y, before.z}, ecologyTrees));
    if (after.u != expected.x || after.v != expected.y)
      return 96;
    foundPath |= decoded.cottagePath > .2f;
    foundLeaf |= decoded.leafLitter > .99f;
    foundWet |= decoded.riverWetSoil > .6f;
    foundNeutral |= decoded == ShowcaseTerrainBiomeWeights3D{};
    // 量測實際網格覆蓋而非 descriptor 數量：中性／落葉頂點使用森林層，
    // 濕岸使用 Ground108，已編寫的小徑使用碎石層。
    materialCoverage[0]+=decoded.leafLitter>0.f||decoded==ShowcaseTerrainBiomeWeights3D{};
    materialCoverage[1]+=decoded.riverWetSoil>0.f;
    materialCoverage[2]+=decoded.cottagePath>0.f;
  }
  // 直接檢查正式 129x129 encoded mesh，而非只檢查連續函式：多個河道
  // z 切片的兩側都必須有 partial wet 頂點，且相鄰 4m 頂點不得由飽和濕岸
  // 單步掉成零。這會重現舊 dominant-biome 分支被 canopy 截斷的缺口。
  for (const float sliceZ : {-96.f, -48.f, 0.f, 48.f, 96.f}) {
    std::vector<std::pair<float, float>> row;
    for (std::size_t i = 0; i < 129u * 129u; ++i) {
      const auto &vertex = encodedApron.vertices[i];
      if (std::abs(vertex.z - sliceZ) > .001f) continue;
      row.emplace_back(vertex.x,
          DecodeShowcaseTerrainBiomeUv3D({vertex.u, vertex.v}).riverWetSoil);
    }
    std::ranges::sort(row);
    unsigned partialWet{};
    bool saturatedToZero{};
    for (std::size_t i = 0; i < row.size(); ++i) {
      partialWet += row[i].second > 0.f && row[i].second < .95f;
      if (i && ((row[i - 1].second >= .95f && row[i].second == 0.f) ||
                (row[i].second >= .95f && row[i - 1].second == 0.f)))
        saturatedToZero = true;
    }
    if (row.size() != 129u || partialWet < 2u || saturatedToZero) {
      std::cerr << "encoded river wet-band divergence z=" << sliceZ
                << " row=" << row.size() << " partial=" << partialWet
                << " saturatedToZero=" << saturatedToZero << '\n';
      return 94;
    }
  }
  if (!foundPath || !foundLeaf || !foundWet || !foundNeutral ||
      materialCoverage[0]<1000u || !materialCoverage[1] ||
      !materialCoverage[2])
    return 81;
  Mesh3D crossSignTriangle{};
  crossSignTriangle.vertices = {
      {0, 0, 0, 0, 0, .8f, 0}, {1, 0, 0, 0, 0, -.7f, 0},
      {0, 0, 1, 0, 0, 0, 0}};
  crossSignTriangle.indices = {0, 1, 2};
  if (!ValidateShowcaseTerrainBiomeUv3D(
          crossSignTriangle, crossSignTriangle.vertices.size()))
    return 82;
  for (unsigned step = 0; step <= 100; ++step) {
    const float t = float(step) / 100.f;
    const Vec2 interpolated{.8f + (-.7f - .8f) * t, 0.f};
    const auto decoded = DecodeShowcaseTerrainBiomeUv3D(interpolated);
    const float total = decoded.cottagePath + decoded.leafLitter +
                        decoded.riverWetSoil;
    if (!std::isfinite(total) || total > 1.f + 1e-6f ||
        (decoded.cottagePath > 0.f && decoded.riverWetSoil > 0.f))
      return 83;
  }
  const float zeroCrossingT = .8f / 1.5f;
  const auto zeroCrossing = DecodeShowcaseTerrainBiomeUv3D(
      {.8f + (-.7f - .8f) * zeroCrossingT, 0.f});
  if (std::abs(zeroCrossing.cottagePath) > 1e-6f ||
      std::abs(zeroCrossing.riverWetSoil) > 1e-6f)
    return 84;
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
  if (!TreeDescriptorDistributionContract(first.Descriptors()) ||
      !TreeMotherAngularDistributionContract(first.Descriptors()))
    return 78;
  auto threeRayTreeMothers = first.Descriptors();
  constexpr std::array<float, 6> legacyCommunityDirections{
      .18f, .86f, 2.22f, 3.05f, 4.38f, 5.72f};
  constexpr Vec3 landSpawnCenter{-16.f, 0.f, 8.f};
  unsigned legacyTreeOrdinal{};
  for (auto &descriptor : threeRayTreeMothers) {
    if (descriptor.kind != ShowcaseSceneKind3D::Tree) continue;
    if (descriptor.lodGroup == 1u && legacyTreeOrdinal % 4u == 0u) {
      const float x = descriptor.position.x - landSpawnCenter.x;
      const float z = descriptor.position.z - landSpawnCenter.z;
      const float radius = std::sqrt(x * x + z * z);
      // 舊錯誤以單株 ordinal 選六槽；母樹 ordinal 每次加四，只命中三槽。
      const float angle =
          legacyCommunityDirections[legacyTreeOrdinal %
                                    legacyCommunityDirections.size()];
      descriptor.position.x = landSpawnCenter.x + std::cos(angle) * radius;
      descriptor.position.z = landSpawnCenter.z + std::sin(angle) * radius;
    }
    ++legacyTreeOrdinal;
  }
  if (TreeMotherAngularDistributionContract(threeRayTreeMothers)) return 99;
  std::vector<ShowcaseLandPlacement3D> treePlacements;
  const bool rebuiltLand = BuildShowcaseLandEcology3D({}, treePlacements);
  const bool treeForms = rebuiltLand &&
      TreeFormVariationContract(treePlacements, first.Descriptors());
  const bool understoryForms = rebuiltLand &&
      UnderstoryFormVariationContract(treePlacements, first.Descriptors());
  if (!rebuiltLand || !treeForms || !understoryForms) {
    std::cerr << "environment ecology form divergence rebuilt=" << rebuiltLand
              << " treeForms=" << treeForms
              << " understoryForms=" << understoryForms << '\n';
    return 84;
  }
  if (!ForestMidgroundLayerContract(treePlacements, first.Descriptors()))
    return 93;
  auto narrowedCanopy = first.Descriptors();
  for (auto &descriptor : narrowedCanopy)
    if (descriptor.kind == ShowcaseSceneKind3D::Tree) {
      descriptor.scale.x *= .82f;
      descriptor.scale.z *= .82f;
    }
  if (ForestMidgroundLayerContract(treePlacements, narrowedCanopy)) return 94;
  auto collapsedUnderstory = first.Descriptors();
  for (std::size_t index = 0; index < treePlacements.size(); ++index)
    if (treePlacements[index].kind == ShowcaseLandKind3D::Grass)
      collapsedUnderstory[index + 2u].scale = treePlacements[index].scale;
  if (ForestMidgroundLayerContract(treePlacements, collapsedUnderstory)) return 95;
  ShowcaseLandEcologySettings3D alternateLandSettings{};
  alternateLandSettings.seed = 0x4a18u;
  std::vector<ShowcaseLandPlacement3D> alternateLand;
  if (!BuildShowcaseLandEcology3D(alternateLandSettings, alternateLand) ||
      !ForestMidgroundLayerContract(alternateLand, differentSeed.Descriptors()) ||
      !TreeMotherAngularDistributionContract(differentSeed.Descriptors()))
    return 96;
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
  constexpr std::array<std::string_view, 19> materialIdentities{
      "forest_ground_04:slot-0", "Ground108:slot-0",
      "RoofingTiles014B:slot-0", "Bricks100:slot-0",
      "island_tree_01:trunk", "island_tree_01:leaves",
      "island_tree_01:branches", "tree_small_02:trunk",
      "tree_small_02:leaves", "tree_small_02:branches",
      "fir_tree_01_c:bark", "fir_tree_01_c:twig",
      "fir_tree_01_c:dead-branches", "fir_tree_01_c:trunk-c",
      "dead_tree_trunk_02:slot-0", "boulder_01:slot-0",
      "coastal_cliff_01:slot-0", "stony_dirt_path:slot-0",
      "runtime/water-optics"};
  constexpr std::array<std::string_view, 12> licensedAssets{
      "Bricks100|ambientcg|CC0-1.0|commercial",
      "Ground108|ambientcg|CC0-1.0|commercial",
      "RoofingTiles014B|ambientcg|CC0-1.0|commercial",
      "boulder_01|poly-haven|CC0-1.0|commercial",
      "coastal_cliff_01|poly-haven|CC0-1.0|commercial",
      "dead_tree_trunk_02|poly-haven|CC0-1.0|commercial",
      "forest_ground_04|poly-haven|CC0-1.0|commercial",
      "fir_tree_01_c|poly-haven|CC0-1.0|commercial",
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
  missingTreeLicense[10] = "island_tree_01|poly-haven|CC0-1.0|commercial";
  if (targets.materialIdentityHash == InventoryHash(missingTreeIdentity) ||
      targets.licenseInventoryHash == InventoryHash(missingTreeLicense))
    return 81;
  constexpr std::array expectedCottageMaterials{
      std::string_view{"ambientcg/Bricks100/cooked-pbr"},
      std::string_view{"ambientcg/WoodSiding006/material-pbr"},
      std::string_view{"ambientcg/WoodSiding006/material-pbr"},
      std::string_view{"ambientcg/WoodSiding006/material-pbr"},
      std::string_view{"ambientcg/WoodSiding006/material-pbr"},
      std::string_view{"ambientcg/RoofingTiles014B/cooked-pbr"}};
  constexpr std::array expectedCottageMeshes{
      std::string_view{"procedural/cottage-foundation"}, std::string_view{"procedural/cottage-front"},
      std::string_view{"procedural/cottage-back"}, std::string_view{"procedural/cottage-left"},
      std::string_view{"procedural/cottage-right"}, std::string_view{"procedural/cottage-pitched-roof"}};
  if(first.Descriptors().size()!=520)return 100;
  for(std::size_t cottageIndex=0;cottageIndex<expectedCottageMaterials.size();++cottageIndex){
    const auto& descriptor=first.Descriptors()[514u+cottageIndex];
    if(descriptor.kind!=ShowcaseSceneKind3D::Building||descriptor.meshAsset!=expectedCottageMeshes[cottageIndex]||
       descriptor.materialAsset!=expectedCottageMaterials[cottageIndex]||
       GameplayCottageDescriptorMaterialAsset3D(GameplayCottageBaseMaterialRole3D(descriptor.meshAsset))!=descriptor.materialAsset)return 101;
  }
  // 精確負載不能只與可同時退回舊值的 Targets 比較，否則舊荒地統計會假通過。
  const auto healthyForestLoad = [](const ShowcaseEnvironmentReceipt3D &value) {
    return value.trees == 296 && value.grass == 186 && value.rocks == 30 &&
        value.vegetation == 482 && value.nearVegetation == 240 &&
        value.midVegetation == 144 && value.farVegetation == 98 &&
        value.riverbankProps == 88;
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
  if (!land.Complete() || land.placements != 512 ||
      land.trees[0] + land.grass[0] + land.rocks[0] != 252 ||
      land.trees[1] + land.grass[1] + land.rocks[1] != 156 ||
      land.trees[2] + land.grass[2] + land.rocks[2] != 104) return 55;
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
      !grounding.Complete() || grounding.eligibleDescriptors != 512 ||
      grounding.groundedDescriptors != 512 || grounding.maximumHeightError > .0001f)
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
      groundedReceipt.terrainGroundingEligible != 512 ||
      groundedReceipt.terrainGroundedDescriptors != 512 ||
      groundedReceipt.terrainGroundingMaximumError > .0001f)
    return 92;
  ShowcaseTerrainGroundingReceipt3D repeatedGrounding{};
  if (!grounded.GroundLandDescriptors(groundingTerrain, &repeatedGrounding) ||
      !repeatedGrounding.Complete() ||
      grounded.Receipt().worldHash != groundedWorldHash)
    return 91;
  if (receipt.worldHash == 0 || receipt.worldHash != repeated.worldHash ||
      receipt.terrain != 1 || receipt.water != 1 ||
      receipt.vegetation != 482 || receipt.rocks != 30 ||
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
      receipt.integratedDescriptors != 520 || receipt.descriptorCapacity != 528 ||
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
  std::size_t verticalUnderstoryDescriptors = 0;
  bool grassBiomeMaterialsExact = true;
  for (const auto &descriptor : first.Descriptors()) {
    if (descriptor.kind == ShowcaseSceneKind3D::Grass) {
      ++grassDescriptors;
      shadowCastingGrassDescriptors += descriptor.castsShadow ? 1u : 0u;
      if (descriptor.meshAsset == "procedural/grass-blade")
        grassBiomeMaterialsExact &=
            descriptor.materialAsset.find("stony_dirt_path") !=
            std::string::npos;
      else if (descriptor.meshAsset == "procedural/grass-fan") {
        grassBiomeMaterialsExact &=
            descriptor.materialAsset.find("Ground108") != std::string::npos;
        verticalUnderstoryDescriptors += descriptor.lodGroup != 0 ? 1u : 0u;
      } else if (descriptor.meshAsset == "procedural/grass-tuft") {
        grassBiomeMaterialsExact &=
            descriptor.materialAsset.find("forest_ground_04") !=
            std::string::npos;
        verticalUnderstoryDescriptors += descriptor.lodGroup != 0 ? 1u : 0u;
      } else
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
      grassDescriptors != 186 || verticalUnderstoryDescriptors != 47 ||
      shadowCastingGrassDescriptors != 0 ||
      !grassBiomeMaterialsExact) {
    std::cerr << "GRASS_VARIANT_ROUTING=FAIL descriptors=" << grassDescriptors
              << " vertical=" << verticalUnderstoryDescriptors
              << " shadows=" << shadowCastingGrassDescriptors << '\n';
    return 32;
  }

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
  const auto terrainDistanceMaterial = draw.TerrainDistanceMaterial();
  if (std::abs(terrainDistanceMaterial.macroVariationScale - .020f) > 1e-6f ||
      std::abs(terrainDistanceMaterial.macroVariationStrength - .17f) >
          1e-6f ||
      std::abs(terrainDistanceMaterial.normalDetailStrength - 1.80f) >
          1e-6f)
    return 111;
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
  if(draw.WaterSurface().foamStartDepth > .035f ||
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
      forestWater.deepColor.z > forestWater.deepColor.x + .025f ||
      draw.WaterOptics().refractionStrength < .018f ||
      draw.WaterOptics().refractionStrength > .024f ||
      draw.WaterOptics().depthScale < .42f ||
      draw.WaterOptics().depthScale > .50f ||
      draw.WaterOptics().maximumUvOffset < .035f ||
      draw.WaterOptics().maximumUvOffset > .045f)
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
  if (!DuskShadowColorSeparated(duskEnvironmentBlend)) return 97;
  auto previousWarmDuskFill = duskEnvironmentBlend;
  previousWarmDuskFill.objectFillRadiance = {.44f,.25f,.16f};
  if (DuskShadowColorSeparated(previousWarmDuskFill)) return 98;
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
  expectedDuskToneCurve.duskExposureEv = 1.f;
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
      dayRadiance.lunarDetailStrength != 0.f ||
      duskRadiance.lunarDetailStrength != 0.f ||
      nightRadiance.lunarDetailStrength != 1.f ||
      dayRadiance.stellarDetailStrength != 0.f ||
      duskRadiance.stellarDetailStrength != 0.f ||
      nightRadiance.stellarDetailStrength != 1.f ||
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
               1.f) > 1e-6f ||
      duskEnvironmentBlend.iblSpecularWeight < .65f ||
      duskEnvironmentBlend.analyticSpecularWeight > .35f)
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
  const std::array<Camera3D, 3> forestDepthCameras{
      productionCameras[0], productionCameras[1], productionCameras[2]};
  const std::array<Camera3D, 2> understoryDensityCameras{
      productionCameras[1], productionCameras[2]};
  const auto submittedUnderstoryBounds =
      BuildSubmittedUnderstoryHighDetailBounds3D();
  std::cout << std::setprecision(9)
            << "UNDERSTORY_HIGH_BOUNDS riverbank="
            << submittedUnderstoryBounds.riverbank.center.x << '/'
            << submittedUnderstoryBounds.riverbank.center.z << ':'
            << submittedUnderstoryBounds.riverbank.extents.x << '/'
            << submittedUnderstoryBounds.riverbank.extents.z << " fern="
            << submittedUnderstoryBounds.fern.center.x << '/'
            << submittedUnderstoryBounds.fern.center.z << ':'
            << submittedUnderstoryBounds.fern.extents.x << '/'
            << submittedUnderstoryBounds.fern.extents.z << " community="
            << submittedUnderstoryBounds.community.center.x << '/'
            << submittedUnderstoryBounds.community.center.z << ':'
            << submittedUnderstoryBounds.community.extents.x << '/'
            << submittedUnderstoryBounds.community.extents.z << '\n';
  const auto productionBoundsMatch = [](ShowcaseLandVariant3D variant,
                                        const MeshBounds3D &actual) {
    const auto expected = ShowcaseUnderstoryHighDetailFootprintBounds3D(variant);
    return std::abs(expected.center.x - actual.center.x) <= 1e-5f &&
           std::abs(expected.center.y - actual.center.z) <= 1e-5f &&
           std::abs(expected.extents.x - actual.extents.x) <= 1e-5f &&
           std::abs(expected.extents.y - actual.extents.z) <= 1e-5f;
  };
  if (!productionBoundsMatch(ShowcaseLandVariant3D::GrassBlade,
                             submittedUnderstoryBounds.riverbank) ||
      !productionBoundsMatch(ShowcaseLandVariant3D::GrassFan,
                             submittedUnderstoryBounds.fern) ||
      !productionBoundsMatch(ShowcaseLandVariant3D::GrassTuft,
                             submittedUnderstoryBounds.community))
    return 110;
  if (!FixedViewUnderstoryFootprintProxyContract(
          treePlacements, first.Descriptors(), understoryDensityCameras,
          submittedUnderstoryBounds))
    return 105;
  auto sparseUnderstory = first.Descriptors();
  for (auto &descriptor : sparseUnderstory)
    if (descriptor.kind == ShowcaseSceneKind3D::Grass) {
      descriptor.scale.x *= .82f;
      descriptor.scale.z *= .82f;
    }
  if (FixedViewUnderstoryFootprintProxyContract(
          treePlacements, sparseUnderstory, understoryDensityCameras,
          submittedUnderstoryBounds))
    return 106;
  auto repeatedUnderstory = first.Descriptors();
  for (std::size_t index = 0; index < treePlacements.size(); ++index) {
    const auto &placement = treePlacements[index];
    if (placement.kind != ShowcaseLandKind3D::Grass) continue;
    const float uniformScale =
        placement.zone == ShowcaseLandZone3D::Riverbank ? 1.15f
        : placement.zone == ShowcaseLandZone3D::Meadow ? 1.22f : 1.28f;
    repeatedUnderstory[index + 2u].scale.x = placement.scale.x * uniformScale;
    repeatedUnderstory[index + 2u].scale.z = placement.scale.z * uniformScale;
  }
  if (FixedViewUnderstoryFootprintProxyContract(
          treePlacements, repeatedUnderstory, understoryDensityCameras,
          submittedUnderstoryBounds))
    return 107;
  auto blockedPlacements = treePlacements;
  auto blockedUnderstory = first.Descriptors();
  const auto blocked = std::ranges::find_if(
      blockedPlacements, [](const auto &placement) {
        return placement.kind == ShowcaseLandKind3D::Grass &&
               placement.variant == ShowcaseLandVariant3D::GrassTuft;
      });
  if (blocked == blockedPlacements.end()) return 108;
  const auto blockedIndex = std::size_t(blocked - blockedPlacements.begin());
  const auto laneFrame = SampleShowcaseHeroTrailFrame3D(.75f);
  const Vec2 laneMiddle{laneFrame.position.x, laneFrame.position.z};
  const Vec2 laneNormal{-laneFrame.tangent.z, laneFrame.tangent.x};
  const float oldRadius =
      .34f * (std::max)(blocked->scale.x, blocked->scale.z);
  const float oldPassingRootDistance = 1.4f + oldRadius + .01f;
  const auto placeOldMiss = [&](float side) {
    blocked->position.x = laneMiddle.x + laneNormal.x * oldPassingRootDistance * side;
    blocked->position.z = laneMiddle.y + laneNormal.y * oldPassingRootDistance * side;
    blockedUnderstory[blockedIndex + 2u].position = blocked->position;
  };
  placeOldMiss(1.f);
  if (SubmittedUnderstoryWalkingClearance3D(
          blockedPlacements, blockedUnderstory, submittedUnderstoryBounds) >=
      1.4f)
    placeOldMiss(-1.f);
  // 敵對控制：已淘汰的 .34 * placement.scale 圓會回報通過，但套用正式
  // descriptor scale/yaw 的 community AABB 已進入步行帶；禁止舊 proxy
  // 日後悄悄回流。
  if (LegacyPlacementWalkingClearance3D(blockedPlacements) < 1.4f ||
      SubmittedUnderstoryWalkingClearance3D(
          blockedPlacements, blockedUnderstory, submittedUnderstoryBounds) >=
          1.4f ||
      FixedViewUnderstoryFootprintProxyContract(
          blockedPlacements, blockedUnderstory, understoryDensityCameras,
          submittedUnderstoryBounds))
    return 109;
  if (!FixedViewForestDepthContract(treePlacements, first.Descriptors(),
                                    forestDepthCameras))
    return 102;
  auto flattenedCanopy = first.Descriptors();
  for (std::size_t index = 0; index < treePlacements.size(); ++index) {
    const auto &placement = treePlacements[index];
    if (placement.kind == ShowcaseLandKind3D::Tree &&
        placement.variant != ShowcaseLandVariant3D::DeadTreeTrunk)
      flattenedCanopy[index + 2u].scale.y = placement.scale.y * .94f;
  }
  if (FixedViewForestDepthContract(treePlacements, flattenedCanopy,
                                   forestDepthCameras))
    return 103;
  auto oversizedMatureCanopy = first.Descriptors();
  for (std::size_t index = 0; index < treePlacements.size(); ++index)
    if (treePlacements[index].kind == ShowcaseLandKind3D::Tree &&
        treePlacements[index].variant == ShowcaseLandVariant3D::IslandTree)
      oversizedMatureCanopy[index + 2u].scale.y *= 1.05f;
  if (FixedViewForestDepthContract(treePlacements, oversizedMatureCanopy,
                                   forestDepthCameras))
    return 104;
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
