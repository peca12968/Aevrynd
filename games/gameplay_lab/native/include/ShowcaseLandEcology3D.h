#pragma once

#include <Aevrynd/Math.h>

#include <array>
#include <cstddef>
#include <cstdint>
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
  // 固定四百五十六項預算；近景非河岸草集中英雄區，每生境四株一群。
  // 中遠景樹以四株群落優先找安全點，不再用全域最大淨空將森林攤成果園。
  std::array<std::uint32_t, 3> trees{124, 88, 28};
  std::array<std::uint32_t, 3> grass{116, 52, 18};
  std::array<std::uint32_t, 3> rocks{12, 12, 6};
  float terrainHalfSpan{256.f};
  float terrainMargin{8.f};
  Aevrynd::Vec3 spawnCenter{-16, 0, 8};
  float spawnExclusionRadius{9.f};
  Aevrynd::Vec3 cottageCenter{-12, 0, 24};
  Aevrynd::Vec3 cottageExclusionHalfExtent{11, 0, 9};
  std::uint32_t fixedNonLandDescriptors{8};
  std::uint32_t maximumIntegratedDescriptors{480};
  std::uint32_t candidateTrials{128};
  float riverbankRockMaximumClearance{9.f};
  float riverbankGrassMaximumClearance{7.f};
  float habitatCellSize{32.f};
  std::uint32_t minimumDensestHabitatCell{8};
  // 關閉僅供逐位元差分驗證；剪枝不改候選或最終配置，故不混入配置指紋。
  bool exactCandidatePruning{true};
};

struct ShowcaseLandEcologyReceipt3D {
  std::uint64_t seed{}, fingerprint{};
  std::array<std::uint32_t, 3> trees{}, grass{}, rocks{};
  std::array<std::uint32_t, 8> variants{};
  std::array<std::uint32_t, 4> zones{};
  std::uint32_t placements{}, integratedDescriptors{}, descriptorCapacity{480}, rejectedCandidates{}, attemptedCandidates{};
  std::uint32_t riverbankRocks{}, riverbankGrass{}, occupiedHabitatCells{},
      densestHabitatCell{}, heroGroundCover{}, heroGroundCoverQuadrantMask{},
      understoryGroundCover{}, understoryGroundCoverBandMask{};
  std::uint32_t failureKind{3}, failureBand{3}, failureItem{};
  float minimumTreeSpacing{}, minimumGrassSpacing{}, minimumRockSpacing{},
      minimumPropEdgeSpacing{}, minimumRiverbankRockClearance{},
      maximumRiverbankRockClearance{}, maximumRiverbankGrassClearance{};
  bool terrainBoundsExact{}, bandsExact{}, riverExcluded{}, spawnExcluded{}, cottageExcluded{};
  bool stableKeysExact{}, transformsExact{}, footprintSpacingExact{},
      zonesExact{}, riverbankLayered{}, densityExact{}, heroGroundCoverExact{},
      understoryGroundCoverExact{};
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
