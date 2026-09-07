#include "ShowcaseLandEcology3D.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace GameplayLab {
namespace {

constexpr float Pi = 3.14159265358979323846f;
constexpr std::array<float, 3> BandMinimum{10.f, 48.f, 112.f};
constexpr std::array<float, 3> BandMaximum{48.f, 112.f, 230.f};
constexpr std::array<float, 3> RequiredSpacing{4.5f, .9f, 2.4f};
constexpr float MinimumPropEdgeSpacing = .12f;
constexpr std::uint32_t HabitatGridSide = 16;
constexpr float HeroGroundCoverRadius = 22.f;
constexpr std::uint32_t HeroGroundCoverOrdinals = 48;
constexpr std::uint32_t MinimumHeroGroundCover = 28;

float BandMinimumFor(ShowcaseLandKind3D kind, std::uint32_t band) noexcept {
  // 草叢不參與實體碰撞；近景地被可進入玩家安全圈外緣，樹與岩石仍維持原本的實體淨空。
  return kind == ShowcaseLandKind3D::Grass && band == 0 ? 4.f
                                                        : BandMinimum[band];
}

float SpawnExclusionRadius(const ShowcaseLandEcologySettings3D &settings,
                           ShowcaseLandKind3D kind) noexcept {
  return kind == ShowcaseLandKind3D::Grass ? 3.f
                                            : settings.spawnExclusionRadius;
}

Aevrynd::Vec3 CottageExclusionHalfExtent(
    const ShowcaseLandEcologySettings3D &settings,
    ShowcaseLandKind3D kind) noexcept {
  // 地被只避開房屋實際基座與一公尺維修帶；實體物件仍使用完整遊戲安全框。
  return kind == ShowcaseLandKind3D::Grass
             ? Aevrynd::Vec3{5.2f, 0.f, 4.8f}
             : settings.cottageExclusionHalfExtent;
}

std::uint64_t NextRandom(std::uint64_t &state) noexcept {
  state ^= state >> 12;
  state ^= state << 25;
  state ^= state >> 27;
  return state * 2685821657736338717ull;
}

float UnitRandom(std::uint64_t &state) noexcept {
  return float((NextRandom(state) >> 40) & 0xffffffu) / float(0xffffffu);
}

float RiverCenterX(float z) noexcept {
  return -4.f + std::sin(z * .035f) * 5.f + std::sin(z * .011f) * 2.f;
}

float RiverWidth(float z) noexcept {
  return 12.f + std::sin(z * .057f + .7f) * 2.6f;
}

float RiverClearance(ShowcaseLandKind3D kind) noexcept {
  return kind == ShowcaseLandKind3D::Tree ? 5.f
       : kind == ShowcaseLandKind3D::Grass ? 2.f : .75f;
}

float RiverbankClearance(Aevrynd::Vec3 point, float footprint) noexcept {
  return std::abs(point.x - RiverCenterX(point.z)) -
         RiverWidth(point.z) * .5f - footprint;
}

float DistanceSquared(Aevrynd::Vec3 left, Aevrynd::Vec3 right) noexcept {
  const float x = left.x - right.x, z = left.z - right.z;
  return x * x + z * z;
}

float PlacementFootprint(ShowcaseLandKind3D kind,
                         Aevrynd::Vec3 scale) noexcept {
  const float span = (std::max)(scale.x, scale.z);
  // 樹以樹幹落地圈、草以根叢、岩石以完整底座作不可重疊足跡；樹冠仍可自然交疊。
  return span * (kind == ShowcaseLandKind3D::Tree
                     ? .18f
                     : kind == ShowcaseLandKind3D::Grass ? .34f : .5f);
}

bool InExclusion(const ShowcaseLandEcologySettings3D &settings,
                 ShowcaseLandKind3D kind, Aevrynd::Vec3 point,
                 float footprint) noexcept {
  const float limit = settings.terrainHalfSpan - settings.terrainMargin;
  if (std::abs(point.x) + footprint > limit ||
      std::abs(point.z) + footprint > limit) return true;
  const float spawnExclusion = SpawnExclusionRadius(settings, kind);
  if (DistanceSquared(point, settings.spawnCenter) <
      (spawnExclusion + footprint) * (spawnExclusion + footprint)) return true;
  const auto cottageExclusion = CottageExclusionHalfExtent(settings, kind);
  if (std::abs(point.x - settings.cottageCenter.x) <
          cottageExclusion.x + footprint &&
      std::abs(point.z - settings.cottageCenter.z) <
          cottageExclusion.z + footprint) return true;
  return std::abs(point.x - RiverCenterX(point.z)) <
         RiverWidth(point.z) * .5f + RiverClearance(kind) + footprint;
}

ShowcaseLandVariant3D Variant(ShowcaseLandKind3D kind,
                             std::uint32_t ordinal) noexcept {
  if (kind == ShowcaseLandKind3D::Tree)
    return ordinal % 11u < 2u ? ShowcaseLandVariant3D::DeadTreeTrunk
                              : ShowcaseLandVariant3D::IslandTree;
  if (kind == ShowcaseLandKind3D::Rock)
    return ordinal % 3u ? ShowcaseLandVariant3D::Boulder
                        : ShowcaseLandVariant3D::CoastalCliff;
  return std::array{ShowcaseLandVariant3D::GrassBlade,
                    ShowcaseLandVariant3D::GrassFan,
                    ShowcaseLandVariant3D::GrassTuft}[ordinal % 3u];
}

ShowcaseLandZone3D Zone(ShowcaseLandKind3D kind,
                        ShowcaseLandVariant3D variant) noexcept {
  if (kind == ShowcaseLandKind3D::Tree) return ShowcaseLandZone3D::Grove;
  if (kind == ShowcaseLandKind3D::Rock) return ShowcaseLandZone3D::Riverbank;
  if (variant == ShowcaseLandVariant3D::GrassBlade)
    return ShowcaseLandZone3D::Riverbank;
  return variant == ShowcaseLandVariant3D::GrassFan
             ? ShowcaseLandZone3D::Meadow
             : ShowcaseLandZone3D::Upland;
}

Aevrynd::Vec3 Scale(ShowcaseLandKind3D kind,
                    ShowcaseLandVariant3D variant,
                    std::uint64_t &random) noexcept {
  const float a = UnitRandom(random), b = UnitRandom(random);
  if (kind == ShowcaseLandKind3D::Tree) {
    const float height = variant == ShowcaseLandVariant3D::DeadTreeTrunk
                             ? 6.5f + a * 4.5f : 8.5f + a * 6.f;
    const float width = height * (.46f + b * .12f);
    return {width, height, width * (.88f + a * .2f)};
  }
  if (kind == ShowcaseLandKind3D::Rock) {
    const float size = (1.1f + a * 3.1f) *
                       (variant == ShowcaseLandVariant3D::CoastalCliff
                            ? 2.f
                            : 1.f);
    return {size * (.8f + b * .4f), size * (.55f + b * .25f), size};
  }
  const float width = .85f + a * 1.8f;
  const float height = variant == ShowcaseLandVariant3D::GrassBlade
                           ? .85f + b * .9f
                           : .58f + b * .78f;
  if (variant == ShowcaseLandVariant3D::GrassBlade) return {width * .55f, height, width};
  if (variant == ShowcaseLandVariant3D::GrassFan) return {width * 1.25f, height * .82f, width * .7f};
  return {width, height * 1.08f, width};
}

std::uint32_t Count(const std::array<std::uint32_t, 3> &values) noexcept {
  return values[0] + values[1] + values[2];
}

void Mix(std::uint64_t &hash, const void *data, std::size_t size) noexcept {
  const auto *bytes = static_cast<const unsigned char *>(data);
  for (std::size_t index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ull;
  }
}

void MixFloat(std::uint64_t &hash, float value) noexcept {
  const auto bits = std::bit_cast<std::uint32_t>(value);
  Mix(hash, &bits, sizeof(bits));
}

bool ValidSettings(const ShowcaseLandEcologySettings3D &settings) noexcept {
  const std::uint64_t land = std::uint64_t(Count(settings.trees)) +
      Count(settings.grass) + Count(settings.rocks);
  return settings.seed && settings.terrainHalfSpan >= 240.f &&
      settings.terrainMargin >= 4.f && settings.spawnExclusionRadius >= 8.f &&
      settings.cottageExclusionHalfExtent.x >= 10.f &&
      settings.cottageExclusionHalfExtent.z >= 8.f &&
      settings.candidateTrials >= 32 && settings.candidateTrials <= 128 &&
      settings.riverbankRockMaximumClearance >= 6.f &&
      settings.riverbankGrassMaximumClearance >= 5.f &&
      settings.habitatCellSize >= 24.f && settings.habitatCellSize <= 48.f &&
      settings.minimumDensestHabitatCell >= 4 &&
      land <= settings.maximumIntegratedDescriptors &&
      land + settings.fixedNonLandDescriptors <=
          settings.maximumIntegratedDescriptors;
}

} // namespace

bool ShowcaseLandEcologyReceipt3D::Complete() const noexcept {
  return fingerprint && placements == 456 && integratedDescriptors == placements + 8 &&
         descriptorCapacity >= integratedDescriptors &&
      trees == std::array<std::uint32_t, 3>{48, 64, 32} &&
      grass == std::array<std::uint32_t, 3>{108, 120, 54} &&
      rocks == std::array<std::uint32_t, 3>{12, 12, 6} &&
      variants[0] && variants[1] && variants[2] && variants[3] &&
      variants[4] && variants[5] && variants[6] &&
      zones[0] && zones[1] && zones[2] && zones[3] &&
      minimumTreeSpacing >= RequiredSpacing[0] &&
      minimumGrassSpacing >= RequiredSpacing[1] &&
      minimumRockSpacing >= RequiredSpacing[2] && terrainBoundsExact &&
      bandsExact && riverExcluded && spawnExcluded && cottageExcluded &&
      stableKeysExact && transformsExact && footprintSpacingExact && zonesExact &&
      riverbankLayered && densityExact && heroGroundCoverExact &&
      heroGroundCover >= MinimumHeroGroundCover &&
      heroGroundCoverQuadrantMask == 0x0fu && riverbankRocks == 30 &&
      riverbankGrass == 94 &&
      minimumPropEdgeSpacing >= MinimumPropEdgeSpacing && deterministicBudget;
}

bool BuildShowcaseLandEcology3D(
    const ShowcaseLandEcologySettings3D &settings,
  std::vector<ShowcaseLandPlacement3D> &placements,
    ShowcaseLandEcologyReceipt3D *receipt) {
  placements.clear();
  if (!ValidSettings(settings)) {
    if (receipt) *receipt = {};
    return false;
  }
  const std::uint32_t expected = Count(settings.trees) + Count(settings.grass) +
                                 Count(settings.rocks);
  placements.reserve(expected);
  std::uint64_t random = settings.seed;
  std::uint32_t rejectedCandidates{},attemptedCandidates{};
  std::uint32_t failureKind{3},failureBand{3},failureItem{};
  constexpr float cellSize=16.f;
  constexpr int gridSide=32;
  std::array<std::vector<std::uint32_t>,gridSide*gridSide> grid;
  const auto cell=[=](Aevrynd::Vec3 point){const int x=(std::clamp)(int((point.x+256.f)/cellSize),0,gridSide-1),z=(std::clamp)(int((point.z+256.f)/cellSize),0,gridSide-1);return std::pair{x,z};};
  std::array<std::array<float,128>,3> treeRadii{};
  std::array<std::array<float,128>,3> rockRadii{};
  std::array<std::array<float,32>,3> smallRadii{};
  for(std::uint32_t band=0;band<3;++band){
    for(std::uint32_t trial=0;trial<128;++trial){const float u=(float(trial)+.5f)/128.f;treeRadii[band][trial]=rockRadii[band][trial]=std::sqrt(BandMinimum[band]*BandMinimum[band]+u*(BandMaximum[band]*BandMaximum[band]-BandMinimum[band]*BandMinimum[band]));}
    for(std::uint32_t trial=0;trial<32;++trial){const float u=(float(trial)+.5f)/32.f;const float minimum=BandMinimumFor(ShowcaseLandKind3D::Grass,band);smallRadii[band][trial]=std::sqrt(minimum*minimum+u*(BandMaximum[band]*BandMaximum[band]-minimum*minimum));}
  }
  const auto addKind = [&](ShowcaseLandKind3D kind,
                           const std::array<std::uint32_t, 3> &counts) {
    std::uint32_t ordinal{};
    for (std::uint32_t band = 0; band < 3; ++band)
      for (std::uint32_t item = 0; item < counts[band]; ++item, ++ordinal) {
        const auto variant = Variant(kind, ordinal);
        const auto zone = Zone(kind, variant);
        const auto scale = Scale(kind, variant, random);
        const float footprint = PlacementFootprint(kind, scale);
        Aevrynd::Vec3 best{};
        float bestDistance = -1.f;
        bool found{};
        const bool heroGroundCover = kind == ShowcaseLandKind3D::Grass &&
                                     band == 0 &&
                                     item < HeroGroundCoverOrdinals;
        const std::uint32_t boundedTrials =
            heroGroundCover
                ? settings.candidateTrials
                : kind == ShowcaseLandKind3D::Grass
                ? settings.candidateTrials / 4u
                : kind == ShowcaseLandKind3D::Rock
                      ? settings.candidateTrials / 2u
                      : settings.candidateTrials * 3u / 4u;
        const float seedPhase = float((settings.seed ^
            (std::uint64_t(ordinal + 1) * 0x9e3779b97f4a7c15ull)) & 0xffffu) /
            65536.f * 2.f * Pi;
        float radialDirectionX = std::cos(seedPhase);
        float radialDirectionZ = std::sin(seedPhase);
        constexpr std::uint32_t communities = 6;
        const float communityCenter = (float(ordinal % communities) + .5f) *
                                      (2.f * Pi / float(communities)) +
                                      std::fmod(seedPhase, .17f);
        const float communityX = std::cos(communityCenter);
        const float communityZ = std::sin(communityCenter);
        float communityFraction = std::fmod(float(ordinal) * .381966011f, 1.f);
        float bankFraction = std::fmod(float(ordinal) * .569840291f, 1.f);
        for (std::uint32_t trial = 0; trial < boundedTrials; ++trial) {
          ++attemptedCandidates;
          float directionX = radialDirectionX;
          float directionZ = radialDirectionZ;
          const float nextRadialX = radialDirectionX * -.737368878f -
                                    radialDirectionZ * .675490294f;
          radialDirectionZ = radialDirectionX * .675490294f +
                             radialDirectionZ * -.737368878f;
          radialDirectionX = nextRadialX;
          if (!heroGroundCover && kind != ShowcaseLandKind3D::Rock &&
              zone != ShowcaseLandZone3D::Riverbank) {
            // 六個可辨識群落保留視線廊道；每個群落內仍用低差異序列找無重疊位置。
            const float offset = (communityFraction - .5f) *
                                 (kind == ShowcaseLandKind3D::Tree ? 1.5f : 1.8f);
            directionX = communityX - communityZ * offset;
            directionZ = communityZ + communityX * offset;
            const float inverseLength = 1.f /
                std::sqrt(directionX * directionX + directionZ * directionZ);
            directionX *= inverseLength;
            directionZ *= inverseLength;
          }
          communityFraction += .618033989f;
          if (communityFraction >= 1.f) communityFraction -= 1.f;
          const std::uint32_t treeRadiusIndex = trial * 4u / 3u;
          float radius{};
          if (heroGroundCover) {
            // 固定前四十八株近景地被在英雄區內找位置；其餘配置仍覆蓋完整近景環帶。
            const float u = (float(trial) + .5f) / float(boundedTrials);
            const float minimum = BandMinimumFor(kind, band);
            radius = std::sqrt(minimum * minimum +
                u * (HeroGroundCoverRadius * HeroGroundCoverRadius -
                     minimum * minimum));
          } else if (kind == ShowcaseLandKind3D::Tree) {
            radius = treeRadii[band][treeRadiusIndex];
          } else if (kind == ShowcaseLandKind3D::Rock) {
            radius = rockRadii[band][trial * 2u];
          } else {
            radius = smallRadii[band][trial];
          }
          Aevrynd::Vec3 candidate{settings.spawnCenter.x + directionX * radius,
                                  0,
                                  settings.spawnCenter.z + directionZ * radius};
          if (kind == ShowcaseLandKind3D::Rock ||
              zone == ShowcaseLandZone3D::Riverbank) {
            // 河岸項目直接由同一條正式河流函數取樣兩岸，避免先在整個環帶抽樣再
            // 丟棄絕大多數候選；clearance 仍由後續上下限與足跡 gate 驗證。
            const float minimum = RiverClearance(kind);
            const float maximum = kind == ShowcaseLandKind3D::Rock
                                      ? settings.riverbankRockMaximumClearance
                                      : settings.riverbankGrassMaximumClearance;
            const float clearance = minimum + .05f +
                bankFraction * (maximum - minimum - .1f);
            bankFraction += .754877666f;
            if (bankFraction >= 1.f) bankFraction -= 1.f;
            const float side = ((trial + ordinal) & 1u) ? 1.f : -1.f;
            candidate.x = RiverCenterX(candidate.z) + side *
                (RiverWidth(candidate.z) * .5f + footprint + clearance);
            const float adjustedRadius = std::sqrt(
                DistanceSquared(candidate, settings.spawnCenter));
            if (adjustedRadius < BandMinimum[band] ||
                adjustedRadius > BandMaximum[band]) {
              ++rejectedCandidates;
              continue;
            }
          }
          if (InExclusion(settings, kind, candidate, footprint)) {
            ++rejectedCandidates;
            continue;
          }
          const float bankClearance = RiverbankClearance(candidate, footprint);
          if ((kind == ShowcaseLandKind3D::Rock &&
               bankClearance > settings.riverbankRockMaximumClearance) ||
              (zone == ShowcaseLandZone3D::Riverbank &&
               kind == ShowcaseLandKind3D::Grass &&
               bankClearance > settings.riverbankGrassMaximumClearance)) {
            ++rejectedCandidates;
            continue;
          }
          float nearest = (std::numeric_limits<float>::max)();
          const auto [cellX,cellZ]=cell(candidate);
          for(int z=(std::max)(0,cellZ-2);z<=(std::min)(gridSide-1,cellZ+2);++z)
          for(int x=(std::max)(0,cellX-2);x<=(std::min)(gridSide-1,cellX+2);++x)
          for (const auto placedIndex : grid[std::size_t(z*gridSide+x)]) {
            const auto &placed=placements[placedIndex];
            const float placedFootprint=PlacementFootprint(placed.kind,placed.scale);
            const float required=(std::max)(
                kind == placed.kind
                    ? (std::max)(RequiredSpacing[std::uint32_t(kind)],
                                 RequiredSpacing[std::uint32_t(placed.kind)])
                    : 0.f,
                footprint + placedFootprint + MinimumPropEdgeSpacing);
            const float clearance=DistanceSquared(candidate,placed.position)/(required*required)-1.f;
            nearest=(std::min)(nearest,clearance);
          }
          if (nearest > bestDistance) { best = candidate; bestDistance = nearest; found = true; }
        }
        if (!found || bestDistance < 0.f){failureKind=std::uint32_t(kind);failureBand=band;failureItem=item;return false;}
        placements.push_back({kind, ShowcaseLandBand3D(band), variant, best,
                              scale, UnitRandom(random) * 2.f * Pi,
                              NextRandom(random) | 1ull, zone});
        const auto [x,z]=cell(best);grid[std::size_t(z*gridSide+x)].push_back(std::uint32_t(placements.size()-1));
      }
    return true;
  };
  if (!addKind(ShowcaseLandKind3D::Tree, settings.trees) ||
      !addKind(ShowcaseLandKind3D::Rock, settings.rocks) ||
      !addKind(ShowcaseLandKind3D::Grass, settings.grass) ||
      placements.size() != expected) {
    const auto partialCount=std::uint32_t(placements.size());
    placements.clear();
    if (receipt){*receipt={};receipt->placements=partialCount;receipt->attemptedCandidates=attemptedCandidates;receipt->rejectedCandidates=rejectedCandidates;receipt->failureKind=failureKind;receipt->failureBand=failureBand;receipt->failureItem=failureItem;}
    return false;
  }
  ShowcaseLandEcologyReceipt3D result{};
  if (!ValidateShowcaseLandEcology3D(settings, placements, &result)) return false;
  result.rejectedCandidates = rejectedCandidates;
  result.attemptedCandidates = attemptedCandidates;
  if (receipt) *receipt = result;
  return true;
}

bool ValidateShowcaseLandEcology3D(
    const ShowcaseLandEcologySettings3D &settings,
    std::span<const ShowcaseLandPlacement3D> placements,
    ShowcaseLandEcologyReceipt3D *receipt) {
  ShowcaseLandEcologyReceipt3D result{};
  result.seed = settings.seed;
  result.placements = std::uint32_t(placements.size());
  result.integratedDescriptors = result.placements + settings.fixedNonLandDescriptors;
  result.descriptorCapacity = settings.maximumIntegratedDescriptors;
  result.terrainBoundsExact = result.bandsExact = result.riverExcluded =
      result.spawnExcluded = result.cottageExcluded = result.stableKeysExact =
      result.transformsExact = result.footprintSpacingExact = result.zonesExact =
      result.riverbankLayered = result.densityExact = true;
  std::uint64_t hash = 14695981039346656037ull;
  Mix(hash, &settings.seed, sizeof(settings.seed));
  Mix(hash, settings.trees.data(), sizeof(settings.trees));
  Mix(hash, settings.grass.data(), sizeof(settings.grass));
  Mix(hash, settings.rocks.data(), sizeof(settings.rocks));
  MixFloat(hash, settings.terrainHalfSpan); MixFloat(hash, settings.terrainMargin);
  MixFloat(hash, settings.spawnCenter.x); MixFloat(hash, settings.spawnCenter.z);
  MixFloat(hash, settings.spawnExclusionRadius);
  MixFloat(hash, settings.cottageCenter.x); MixFloat(hash, settings.cottageCenter.z);
  MixFloat(hash, settings.cottageExclusionHalfExtent.x);
  MixFloat(hash, settings.cottageExclusionHalfExtent.z);
  Mix(hash, &settings.fixedNonLandDescriptors, sizeof(settings.fixedNonLandDescriptors));
  Mix(hash, &settings.maximumIntegratedDescriptors, sizeof(settings.maximumIntegratedDescriptors));
  Mix(hash, &settings.candidateTrials, sizeof(settings.candidateTrials));
  MixFloat(hash, settings.riverbankRockMaximumClearance);
  MixFloat(hash, settings.riverbankGrassMaximumClearance);
  MixFloat(hash, settings.habitatCellSize);
  Mix(hash, &settings.minimumDensestHabitatCell,
      sizeof(settings.minimumDensestHabitatCell));
  std::array<std::uint64_t,456> stableKeys{};
  std::array<std::uint32_t, HabitatGridSide * HabitatGridSide> habitatCells{};
  std::size_t stableKeyCount{};
  for (const auto &placement : placements) {
    const auto band = std::uint32_t(placement.band);
    const auto kind = std::uint32_t(placement.kind);
    const auto variant = std::uint32_t(placement.variant);
    const auto zone = std::uint32_t(placement.zone);
    if (band >= 3 || kind >= 3 || variant >= result.variants.size() || zone >= 4 ||
        !placement.stableKey) return false;
    if(stableKeyCount<stableKeys.size())stableKeys[stableKeyCount++]=placement.stableKey;
    else result.stableKeysExact=false;
    result.transformsExact &= std::isfinite(placement.position.x) &&
        std::isfinite(placement.position.y) && std::isfinite(placement.position.z) &&
        std::isfinite(placement.scale.x) && std::isfinite(placement.scale.y) &&
        std::isfinite(placement.scale.z) && std::isfinite(placement.yaw) &&
        placement.scale.x > 0.f && placement.scale.y > 0.f && placement.scale.z > 0.f;
    auto *counts = kind == 0 ? &result.trees : kind == 1 ? &result.grass : &result.rocks;
    ++(*counts)[band]; ++result.variants[variant]; ++result.zones[zone];
    result.zonesExact &= placement.zone == Zone(placement.kind, placement.variant);
    const float footprint = PlacementFootprint(placement.kind, placement.scale);
    const float limit = settings.terrainHalfSpan - settings.terrainMargin;
    result.terrainBoundsExact &= std::abs(placement.position.x) + footprint <= limit &&
                                 std::abs(placement.position.z) + footprint <= limit;
    const float bandRadius = std::sqrt(DistanceSquared(placement.position,
                                                        settings.spawnCenter));
    result.bandsExact &= bandRadius >= BandMinimumFor(placement.kind, band) &&
                         bandRadius <= BandMaximum[band];
    result.riverExcluded &= std::abs(placement.position.x - RiverCenterX(placement.position.z)) >=
        RiverWidth(placement.position.z) * .5f + RiverClearance(placement.kind) + footprint;
    const float bankClearance = RiverbankClearance(placement.position, footprint);
    if (placement.kind == ShowcaseLandKind3D::Rock) {
      ++result.riverbankRocks;
      if (result.riverbankRocks == 1)
        result.minimumRiverbankRockClearance = bankClearance;
      else
        result.minimumRiverbankRockClearance =
            (std::min)(result.minimumRiverbankRockClearance, bankClearance);
      result.maximumRiverbankRockClearance =
          (std::max)(result.maximumRiverbankRockClearance, bankClearance);
      result.riverbankLayered &= bankClearance >= RiverClearance(placement.kind) &&
          bankClearance <= settings.riverbankRockMaximumClearance;
    } else if (placement.zone == ShowcaseLandZone3D::Riverbank) {
      ++result.riverbankGrass;
      result.maximumRiverbankGrassClearance =
          (std::max)(result.maximumRiverbankGrassClearance, bankClearance);
      result.riverbankLayered &= bankClearance >= RiverClearance(placement.kind) &&
          bankClearance <= settings.riverbankGrassMaximumClearance;
    }
    const int habitatX = (std::clamp)(
        int((placement.position.x + settings.terrainHalfSpan) /
            settings.habitatCellSize), 0, int(HabitatGridSide) - 1);
    const int habitatZ = (std::clamp)(
        int((placement.position.z + settings.terrainHalfSpan) /
            settings.habitatCellSize), 0, int(HabitatGridSide) - 1);
    ++habitatCells[std::size_t(habitatZ * int(HabitatGridSide) + habitatX)];
    const float spawnExclusion = SpawnExclusionRadius(settings, placement.kind);
    result.spawnExcluded &= DistanceSquared(placement.position, settings.spawnCenter) >=
        (spawnExclusion + footprint) * (spawnExclusion + footprint);
    const auto cottageExclusion = CottageExclusionHalfExtent(settings, placement.kind);
    result.cottageExcluded &= std::abs(placement.position.x - settings.cottageCenter.x) >=
        cottageExclusion.x + footprint ||
        std::abs(placement.position.z - settings.cottageCenter.z) >=
        cottageExclusion.z + footprint;
    if (placement.kind == ShowcaseLandKind3D::Grass &&
        bandRadius <= HeroGroundCoverRadius) {
      ++result.heroGroundCover;
      const bool right = placement.position.x >= settings.spawnCenter.x;
      const bool lower = placement.position.z >= settings.spawnCenter.z;
      result.heroGroundCoverQuadrantMask |=
          1u << ((lower ? 2u : 0u) + (right ? 1u : 0u));
    }
    Mix(hash, &kind, sizeof(kind)); Mix(hash, &band, sizeof(band));
    Mix(hash, &variant, sizeof(variant));
    Mix(hash, &zone, sizeof(zone));
    MixFloat(hash, placement.position.x); MixFloat(hash, placement.position.z);
    MixFloat(hash, placement.scale.x); MixFloat(hash, placement.scale.y);
    MixFloat(hash, placement.scale.z); MixFloat(hash, placement.yaw);
    Mix(hash, &placement.stableKey, sizeof(placement.stableKey));
  }
  std::sort(stableKeys.begin(),stableKeys.begin()+stableKeyCount);
  for(std::size_t index=1;index<stableKeyCount;++index)result.stableKeysExact&=stableKeys[index-1]!=stableKeys[index];
  constexpr float validationCellSize = 16.f;
  constexpr int validationGridSide = 32;
  std::array<std::vector<std::uint32_t>,
             validationGridSide * validationGridSide> validationGrid;
  std::array<float, 3> minimumKindDistanceSquared{
      (std::numeric_limits<float>::max)(),
      (std::numeric_limits<float>::max)(),
      (std::numeric_limits<float>::max)()};
  result.minimumPropEdgeSpacing = (std::numeric_limits<float>::max)();
  for (std::uint32_t index = 0; index < placements.size(); ++index) {
    const auto &item = placements[index];
    const int cellX = (std::clamp)(int((item.position.x + 256.f) /
                                      validationCellSize),
                                   0, validationGridSide - 1);
    const int cellZ = (std::clamp)(int((item.position.z + 256.f) /
                                      validationCellSize),
                                   0, validationGridSide - 1);
    const float footprint = PlacementFootprint(item.kind, item.scale);
    for (int z = (std::max)(0, cellZ - 1);
         z <= (std::min)(validationGridSide - 1, cellZ + 1); ++z)
      for (int x = (std::max)(0, cellX - 1);
           x <= (std::min)(validationGridSide - 1, cellX + 1); ++x)
        for (const auto otherIndex :
             validationGrid[std::size_t(z * validationGridSide + x)]) {
          const auto &other = placements[otherIndex];
          const float otherFootprint =
              PlacementFootprint(other.kind, other.scale);
          const float required = (std::max)(
              item.kind == other.kind
                  ? RequiredSpacing[std::uint32_t(item.kind)]
                  : 0.f,
              footprint + otherFootprint + MinimumPropEdgeSpacing);
          const float distanceSquared =
              DistanceSquared(item.position, other.position);
          if (item.kind == other.kind) {
            auto &minimum = minimumKindDistanceSquared[
                std::uint32_t(item.kind)];
            minimum = (std::min)(minimum, distanceSquared);
          }
          result.footprintSpacingExact &=
              distanceSquared + 1e-5f >= required * required;
          result.minimumPropEdgeSpacing = (std::min)(
              result.minimumPropEdgeSpacing,
              std::sqrt(distanceSquared) - footprint - otherFootprint);
        }
    validationGrid[std::size_t(cellZ * validationGridSide + cellX)]
        .push_back(index);
  }
  if (!std::isfinite(result.minimumPropEdgeSpacing))
    result.minimumPropEdgeSpacing = 0.f;
  for (const auto count : habitatCells) {
    result.occupiedHabitatCells += count ? 1u : 0u;
    result.densestHabitatCell = (std::max)(result.densestHabitatCell, count);
  }
  result.heroGroundCoverExact = result.heroGroundCover >= MinimumHeroGroundCover &&
                                result.heroGroundCoverQuadrantMask == 0x0fu;
  result.densityExact = result.occupiedHabitatCells >= 12 &&
      result.densestHabitatCell >= settings.minimumDensestHabitatCell &&
      result.heroGroundCoverExact;
  result.minimumTreeSpacing = std::sqrt(minimumKindDistanceSquared[0]);
  result.minimumGrassSpacing = std::sqrt(minimumKindDistanceSquared[1]);
  result.minimumRockSpacing = std::sqrt(minimumKindDistanceSquared[2]);
  result.deterministicBudget = ValidSettings(settings) &&
      result.integratedDescriptors <= result.descriptorCapacity;
  result.fingerprint = hash ? hash : 1;
  if (receipt) *receipt = result;
  return result.Complete() && result.footprintSpacingExact;
}

} // namespace GameplayLab
