#pragma once

#include <Aevrynd/Math.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace GameplayLab {

enum class ShowcaseLandKind3D : std::uint8_t { Tree, Grass, Rock };
enum class ShowcaseLandBand3D : std::uint8_t { Near, Mid, Far };
enum class ShowcaseLandZone3D : std::uint8_t {
  Grove,
  Meadow,
  Riverbank,
  Upland
};
enum class ShowcaseLandVariant3D : std::uint8_t {
  IslandTree,
  TreeSmall02,
  DeadTreeTrunk,
  Boulder,
  CoastalCliff,
  GrassBlade,
  GrassFan,
  GrassTuft
};

[[nodiscard]] constexpr std::string_view ShowcaseLandAssetId3D(
    ShowcaseLandVariant3D variant) noexcept {
  switch (variant) {
  case ShowcaseLandVariant3D::IslandTree: return "island_tree_01";
  case ShowcaseLandVariant3D::TreeSmall02: return "tree_small_02";
  case ShowcaseLandVariant3D::DeadTreeTrunk: return "dead_tree_trunk_02";
  case ShowcaseLandVariant3D::Boulder: return "boulder_01";
  case ShowcaseLandVariant3D::CoastalCliff: return "coastal_cliff_01";
  case ShowcaseLandVariant3D::GrassBlade: return "procedural/grass-blade";
  case ShowcaseLandVariant3D::GrassFan: return "procedural/grass-fan";
  case ShowcaseLandVariant3D::GrassTuft: return "procedural/grass-tuft";
  }
  return {};
}

struct ShowcaseLandPlacement3D {
  ShowcaseLandKind3D kind{};
  ShowcaseLandBand3D band{};
  ShowcaseLandVariant3D variant{};
  Aevrynd::Vec3 position{};
  Aevrynd::Vec3 scale{1, 1, 1};
  float yaw{};
  std::uint64_t stableKey{};
  ShowcaseLandZone3D zone{};
};

struct ShowcaseLandEcologySettings3D {
  std::uint64_t seed{0x4a17u};
  // 固定四百六十四項預算；近景非河岸草集中英雄區，每生境四株一群。
  // 中遠景樹以四株群落優先找安全點，不再用全域最大淨空將森林攤成果園。
  std::array<std::uint32_t, 3> trees{124, 92, 80};
  std::array<std::uint32_t, 3> grass{116, 52, 18};
  std::array<std::uint32_t, 3> rocks{12, 12, 6};
  float terrainHalfSpan{256.f};
  float terrainMargin{8.f};
  Aevrynd::Vec3 spawnCenter{-16, 0, 8};
  float spawnExclusionRadius{9.f};
  Aevrynd::Vec3 cottageCenter{-12, 0, 24};
  Aevrynd::Vec3 cottageExclusionHalfExtent{11, 0, 9};
  std::uint32_t fixedNonLandDescriptors{9};
  std::uint32_t maximumIntegratedDescriptors{528};
  std::uint32_t candidateTrials{128};
  float riverbankRockMaximumClearance{9.f};
  float riverbankGrassMaximumClearance{7.f};
  float habitatCellSize{32.f};
  std::uint32_t minimumDensestHabitatCell{8};
  // 關閉僅供逐位元差分驗證；剪枝不改候選或最終配置，故不混入配置指紋。
  bool exactCandidatePruning{true};
};

// The hero trail is gameplay geometry, not a camera-space decoration.  Keep
// one world-space definition for terrain authoring, vegetation clearance and
// traversal tests so the visible path is always the path the player can use.
struct ShowcaseHeroTrailSample3D {
  float distance{(std::numeric_limits<float>::max)()};
  float signedSide{};
  float along{};
  bool valid{};
};

struct ShowcaseHeroTrailFrame3D {
  Aevrynd::Vec3 position{};
  Aevrynd::Vec3 tangent{1.f, 0.f, 0.f};
  bool valid{};
};

[[nodiscard]] inline const std::array<Aevrynd::Vec3, 5> &
ShowcaseHeroTrailControls3D() noexcept {
  static const std::array<Aevrynd::Vec3, 5> controls{{
      {-28.f, 0.f, -8.f}, {-25.5f, 0.f, 1.f}, {-16.f, 0.f, 8.f},
      {-14.f, 0.f, 14.f}, {-12.f, 0.f, 19.8f}}};
  return controls;
}

[[nodiscard]] inline ShowcaseHeroTrailFrame3D
SampleShowcaseHeroTrailFrame3D(float along) noexcept {
  if (!std::isfinite(along))
    return {};
  const auto &controls = ShowcaseHeroTrailControls3D();
  std::array<float, 4> lengths{};
  float totalLength{};
  for (std::size_t index = 0; index + 1 < controls.size(); ++index) {
    lengths[index] = std::hypot(controls[index + 1].x - controls[index].x,
                                controls[index + 1].z - controls[index].z);
    totalLength += lengths[index];
  }
  float target = std::clamp(along, 0.f, 1.f) * totalLength;
  for (std::size_t index = 0; index < lengths.size(); ++index) {
    if (target <= lengths[index] || index + 1 == lengths.size()) {
      const float inverseLength = lengths[index] > 0.f ? 1.f / lengths[index]
                                                       : 0.f;
      const float t = lengths[index] > 0.f
                          ? std::clamp(target * inverseLength, 0.f, 1.f)
                          : 0.f;
      const auto start = controls[index];
      const auto end = controls[index + 1];
      return {{start.x + (end.x - start.x) * t, 0.f,
               start.z + (end.z - start.z) * t},
              {(end.x - start.x) * inverseLength, 0.f,
               (end.z - start.z) * inverseLength},
              lengths[index] > 0.f};
    }
    target -= lengths[index];
  }
  return {};
}

[[nodiscard]] inline ShowcaseHeroTrailSample3D
SampleShowcaseHeroTrail3D(Aevrynd::Vec3 point) noexcept {
  if (!std::isfinite(point.x) || !std::isfinite(point.z))
    return {};
  const auto &controls = ShowcaseHeroTrailControls3D();
  std::array<float, 4> lengths{};
  float totalLength{};
  for (std::size_t index = 0; index + 1 < controls.size(); ++index) {
    lengths[index] = std::hypot(controls[index + 1].x - controls[index].x,
                                controls[index + 1].z - controls[index].z);
    totalLength += lengths[index];
  }

  ShowcaseHeroTrailSample3D result{};
  float precedingLength{};
  for (std::size_t index = 0; index + 1 < controls.size(); ++index) {
    const auto start = controls[index];
    const auto end = controls[index + 1];
    const float vx = end.x - start.x, vz = end.z - start.z;
    const float lengthSquared = vx * vx + vz * vz;
    const float wx = point.x - start.x, wz = point.z - start.z;
    const float t = lengthSquared > 0.f
                        ? std::clamp((wx * vx + wz * vz) / lengthSquared,
                                     0.f, 1.f)
                        : 0.f;
    const float dx = point.x - (start.x + vx * t);
    const float dz = point.z - (start.z + vz * t);
    const float distance = std::hypot(dx, dz);
    if (distance < result.distance) {
      result.distance = distance;
      result.signedSide = lengths[index] > 0.f
                              ? (vx * dz - vz * dx) / lengths[index]
                              : 0.f;
      result.along = totalLength > 0.f
                         ? (precedingLength + lengths[index] * t) / totalLength
                         : 0.f;
      result.valid = true;
    }
    precedingLength += lengths[index];
  }
  return result;
}

[[nodiscard]] inline float ShowcaseHeroTrailTreadHalfWidth3D(
    Aevrynd::Vec3 point) noexcept {
  return .72f + std::sin(point.z * .31f + .8f) * .05f +
         std::sin(point.z * .13f - 1.1f) * .03f;
}

[[nodiscard]] inline float ShowcaseHeroTrailVisibleHalfWidth3D(
    Aevrynd::Vec3 point) noexcept {
  return 2.12f +
         std::sin(point.z * .23f - point.x * .07f + .35f) * .10f;
}

[[nodiscard]] inline bool ShowcaseHeroTrailWalkingLane3D(
    Aevrynd::Vec3 point, float footprint) noexcept {
  if (!std::isfinite(footprint) || footprint < 0.f)
    return false;
  const auto trail = SampleShowcaseHeroTrail3D(point);
  return trail.valid && trail.distance < .75f + footprint;
}

struct ShowcaseLandEcologyReceipt3D {
  std::uint64_t seed{}, fingerprint{};
  std::array<std::uint32_t, 3> trees{}, grass{}, rocks{};
  std::array<std::uint32_t, 8> variants{};
  std::array<std::uint32_t, 4> zones{};
  std::uint32_t placements{}, integratedDescriptors{}, descriptorCapacity{528}, rejectedCandidates{}, attemptedCandidates{};
  std::uint32_t riverbankRocks{}, riverbankGrass{}, occupiedHabitatCells{},
      densestHabitatCell{}, heroGroundCover{}, heroGroundCoverQuadrantMask{},
      heroPathShoulderCluster{}, heroCottageRearCluster{},
      heroRiverForegroundCluster{}, heroForestEdgeRocks{},
      understoryGroundCover{}, understoryGroundCoverBandMask{};
  std::uint32_t failureKind{3}, failureBand{3}, failureItem{};
  float minimumTreeSpacing{}, minimumGrassSpacing{}, minimumRockSpacing{},
      minimumPropEdgeSpacing{}, minimumRiverbankRockClearance{},
      maximumRiverbankRockClearance{}, maximumRiverbankGrassClearance{},
      meanRiverbankRockClearance{}, meanRiverbankGrassClearance{};
  bool terrainBoundsExact{}, bandsExact{}, riverExcluded{}, spawnExcluded{}, cottageExcluded{};
  bool stableKeysExact{}, transformsExact{}, footprintSpacingExact{},
      zonesExact{}, riverbankLayered{}, densityExact{}, heroGroundCoverExact{},
      heroCompositionClustersExact{},
      understoryGroundCoverExact{}, heroForestEdgeRocksExact{};
  bool deterministicBudget{};
  bool walkingLaneClear{}, vegetationScaleBounded{};
  [[nodiscard]] bool Complete() const noexcept;
};

[[nodiscard]] bool BuildShowcaseLandEcology3D(
    const ShowcaseLandEcologySettings3D &settings,
    std::vector<ShowcaseLandPlacement3D> &placements,
    ShowcaseLandEcologyReceipt3D *receipt = nullptr);

[[nodiscard]] bool ValidateShowcaseLandEcology3D(
    const ShowcaseLandEcologySettings3D &settings,
    std::span<const ShowcaseLandPlacement3D> placements,
    ShowcaseLandEcologyReceipt3D *receipt = nullptr);

} // namespace GameplayLab
