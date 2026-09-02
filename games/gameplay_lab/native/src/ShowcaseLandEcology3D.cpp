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

float DistanceSquared(Aevrynd::Vec3 left, Aevrynd::Vec3 right) noexcept {
  const float x = left.x - right.x, z = left.z - right.z;
  return x * x + z * z;
}

bool InExclusion(const ShowcaseLandEcologySettings3D &settings,
                 ShowcaseLandKind3D kind, Aevrynd::Vec3 point,
                 float footprint) noexcept {
  const float limit = settings.terrainHalfSpan - settings.terrainMargin;
  if (std::abs(point.x) + footprint > limit ||
      std::abs(point.z) + footprint > limit) return true;
  if (DistanceSquared(point, settings.spawnCenter) <
      (settings.spawnExclusionRadius + footprint) *
          (settings.spawnExclusionRadius + footprint)) return true;
  if (std::abs(point.x - settings.cottageCenter.x) <
          settings.cottageExclusionHalfExtent.x + footprint &&
      std::abs(point.z - settings.cottageCenter.z) <
          settings.cottageExclusionHalfExtent.z + footprint) return true;
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

Aevrynd::Vec3 Scale(ShowcaseLandKind3D kind,
                    ShowcaseLandVariant3D variant,
                    std::uint64_t &random) noexcept {
  const float a = UnitRandom(random), b = UnitRandom(random);
  if (kind == ShowcaseLandKind3D::Tree) {
    const float height = variant == ShowcaseLandVariant3D::DeadTreeTrunk
                             ? 5.2f + a * 4.f : 7.f + a * 5.5f;
    const float width = height * (.64f + b * .17f);
    return {width, height, width * (.88f + a * .2f)};
  }
  if (kind == ShowcaseLandKind3D::Rock) {
    const float size = 1.1f + a * 3.1f;
    return {size * (.8f + b * .4f), size * (.55f + b * .25f), size};
  }
  const float width = .75f + a * 1.65f, height = .5f + b * .72f;
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

float MinimumSpacing(std::span<const ShowcaseLandPlacement3D> placements,
                     ShowcaseLandKind3D kind) noexcept {
  float minimum = (std::numeric_limits<float>::max)();
  for (std::size_t left = 0; left < placements.size(); ++left) {
    if (placements[left].kind != kind) continue;
    for (std::size_t right = left + 1; right < placements.size(); ++right)
      if (placements[right].kind == kind)
        minimum = (std::min)(minimum, DistanceSquared(
            placements[left].position, placements[right].position));
  }
  return std::isfinite(minimum) ? std::sqrt(minimum) : 0.f;
}

bool ValidSettings(const ShowcaseLandEcologySettings3D &settings) noexcept {
  const std::uint64_t land = std::uint64_t(Count(settings.trees)) +
      Count(settings.grass) + Count(settings.rocks);
  return settings.seed && settings.terrainHalfSpan >= 240.f &&
      settings.terrainMargin >= 4.f && settings.spawnExclusionRadius >= 8.f &&
      settings.cottageExclusionHalfExtent.x >= 10.f &&
      settings.cottageExclusionHalfExtent.z >= 8.f &&
      settings.candidateTrials >= 32 && settings.candidateTrials <= 128 &&
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
      minimumTreeSpacing >= RequiredSpacing[0] &&
      minimumGrassSpacing >= RequiredSpacing[1] &&
      minimumRockSpacing >= RequiredSpacing[2] && terrainBoundsExact &&
      bandsExact && riverExcluded && spawnExcluded && cottageExcluded &&
      stableKeysExact && transformsExact && footprintSpacingExact && deterministicBudget;
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
  std::array<std::array<float,32>,3> smallRadii{};
  for(std::uint32_t band=0;band<3;++band){
    for(std::uint32_t trial=0;trial<128;++trial){const float u=(float(trial)+.5f)/128.f;treeRadii[band][trial]=std::sqrt(BandMinimum[band]*BandMinimum[band]+u*(BandMaximum[band]*BandMaximum[band]-BandMinimum[band]*BandMinimum[band]));}
    for(std::uint32_t trial=0;trial<32;++trial){const float u=(float(trial)+.5f)/32.f;smallRadii[band][trial]=std::sqrt(BandMinimum[band]*BandMinimum[band]+u*(BandMaximum[band]*BandMaximum[band]-BandMinimum[band]*BandMinimum[band]));}
  }
  const auto addKind = [&](ShowcaseLandKind3D kind,
                           const std::array<std::uint32_t, 3> &counts) {
    std::uint32_t ordinal{};
    for (std::uint32_t band = 0; band < 3; ++band)
      for (std::uint32_t item = 0; item < counts[band]; ++item, ++ordinal) {
        const auto variant = Variant(kind, ordinal);
        const auto scale = Scale(kind, variant, random);
        const float footprint = (std::max)(scale.x, scale.z) * .5f;
        Aevrynd::Vec3 best{};
        float bestDistance = -1.f;
        bool found{};
        const std::uint32_t boundedTrials=kind==ShowcaseLandKind3D::Tree
            ? settings.candidateTrials : settings.candidateTrials/4u;
        const float phase=float((settings.seed^(std::uint64_t(ordinal+1)*0x9e3779b97f4a7c15ull))&0xffffu)/65536.f*2.f*Pi;
        float directionX=std::cos(phase),directionZ=std::sin(phase);
        for (std::uint32_t trial = 0; trial < boundedTrials; ++trial) {
          ++attemptedCandidates;
          const float radius=kind==ShowcaseLandKind3D::Tree?treeRadii[band][trial]:smallRadii[band][trial];
          const Aevrynd::Vec3 candidate{settings.spawnCenter.x + directionX * radius,
                                        0,
                                        settings.spawnCenter.z + directionZ * radius};
          const float nextX=directionX*-.737368878f-directionZ*.675490294f;
          directionZ=directionX*.675490294f+directionZ*-.737368878f;directionX=nextX;
          if (InExclusion(settings, kind, candidate, footprint)) {
            ++rejectedCandidates;
            continue;
          }
          float nearest = (std::numeric_limits<float>::max)();
          const auto [cellX,cellZ]=cell(candidate);
          for(int z=(std::max)(0,cellZ-2);z<=(std::min)(gridSide-1,cellZ+2);++z)
          for(int x=(std::max)(0,cellX-2);x<=(std::min)(gridSide-1,cellX+2);++x)
          for (const auto placedIndex : grid[std::size_t(z*gridSide+x)]) {
            const auto &placed=placements[placedIndex];
            const bool sameKind = placed.kind == kind;
            const bool woodyPair = kind != ShowcaseLandKind3D::Grass &&
                                   placed.kind != ShowcaseLandKind3D::Grass;
            if (!sameKind && !woodyPair) continue;
            const float placedFootprint=(std::max)(placed.scale.x,placed.scale.z)*.5f;
            const float required=(std::max)(RequiredSpacing[std::uint32_t(kind)],
              (std::max)(RequiredSpacing[std::uint32_t(placed.kind)],footprint+placedFootprint));
            const float clearance=DistanceSquared(candidate,placed.position)/(required*required)-1.f;
            nearest=(std::min)(nearest,clearance);
          }
          if (nearest > bestDistance) { best = candidate; bestDistance = nearest; found = true; }
        }
        if (!found || bestDistance < 0.f){failureKind=std::uint32_t(kind);failureBand=band;failureItem=item;return false;}
        placements.push_back({kind, ShowcaseLandBand3D(band), variant, best,
                              scale, UnitRandom(random) * 2.f * Pi,
                              NextRandom(random) | 1ull});
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
      result.transformsExact = result.footprintSpacingExact = true;
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
  std::array<std::uint64_t,456> stableKeys{};
  std::size_t stableKeyCount{};
  for (const auto &placement : placements) {
    const auto band = std::uint32_t(placement.band);
    const auto kind = std::uint32_t(placement.kind);
    const auto variant = std::uint32_t(placement.variant);
    if (band >= 3 || kind >= 3 || variant >= result.variants.size() ||
        !placement.stableKey) return false;
    if(stableKeyCount<stableKeys.size())stableKeys[stableKeyCount++]=placement.stableKey;
    else result.stableKeysExact=false;
    result.transformsExact &= std::isfinite(placement.position.x) &&
        std::isfinite(placement.position.y) && std::isfinite(placement.position.z) &&
        std::isfinite(placement.scale.x) && std::isfinite(placement.scale.y) &&
        std::isfinite(placement.scale.z) && std::isfinite(placement.yaw) &&
        placement.scale.x > 0.f && placement.scale.y > 0.f && placement.scale.z > 0.f;
    auto *counts = kind == 0 ? &result.trees : kind == 1 ? &result.grass : &result.rocks;
    ++(*counts)[band]; ++result.variants[variant];
    const float footprint = (std::max)(placement.scale.x, placement.scale.z) * .5f;
    const float limit = settings.terrainHalfSpan - settings.terrainMargin;
    result.terrainBoundsExact &= std::abs(placement.position.x) + footprint <= limit &&
                                 std::abs(placement.position.z) + footprint <= limit;
    const float bandRadius = std::sqrt(DistanceSquared(placement.position,
                                                        settings.spawnCenter));
    result.bandsExact &= bandRadius >= BandMinimum[band] &&
                         bandRadius <= BandMaximum[band];
    result.riverExcluded &= std::abs(placement.position.x - RiverCenterX(placement.position.z)) >=
        RiverWidth(placement.position.z) * .5f + RiverClearance(placement.kind) + footprint;
    result.spawnExcluded &= DistanceSquared(placement.position, settings.spawnCenter) >=
        (settings.spawnExclusionRadius + footprint) *
            (settings.spawnExclusionRadius + footprint);
    result.cottageExcluded &= std::abs(placement.position.x - settings.cottageCenter.x) >=
        settings.cottageExclusionHalfExtent.x + footprint ||
        std::abs(placement.position.z - settings.cottageCenter.z) >=
        settings.cottageExclusionHalfExtent.z + footprint;
    Mix(hash, &kind, sizeof(kind)); Mix(hash, &band, sizeof(band));
    Mix(hash, &variant, sizeof(variant));
    MixFloat(hash, placement.position.x); MixFloat(hash, placement.position.z);
    MixFloat(hash, placement.scale.x); MixFloat(hash, placement.scale.y);
    MixFloat(hash, placement.scale.z); MixFloat(hash, placement.yaw);
    Mix(hash, &placement.stableKey, sizeof(placement.stableKey));
  }
  std::sort(stableKeys.begin(),stableKeys.begin()+stableKeyCount);
  for(std::size_t index=1;index<stableKeyCount;++index)result.stableKeysExact&=stableKeys[index-1]!=stableKeys[index];
  for(std::size_t left=0;left<placements.size();++left)for(std::size_t right=left+1;right<placements.size();++right){
    const auto& a=placements[left];const auto& b=placements[right];
    const bool same=a.kind==b.kind,woody=a.kind!=ShowcaseLandKind3D::Grass&&b.kind!=ShowcaseLandKind3D::Grass;
    if(!same&&!woody)continue;
    const float af=(std::max)(a.scale.x,a.scale.z)*.5f,bf=(std::max)(b.scale.x,b.scale.z)*.5f;
    const float required=(std::max)((std::max)(RequiredSpacing[std::uint32_t(a.kind)],RequiredSpacing[std::uint32_t(b.kind)]),af+bf);
    result.footprintSpacingExact&=DistanceSquared(a.position,b.position)+1e-5f>=required*required;
  }
  result.minimumTreeSpacing = MinimumSpacing(placements, ShowcaseLandKind3D::Tree);
  result.minimumGrassSpacing = MinimumSpacing(placements, ShowcaseLandKind3D::Grass);
  result.minimumRockSpacing = MinimumSpacing(placements, ShowcaseLandKind3D::Rock);
  result.deterministicBudget = ValidSettings(settings) &&
      result.integratedDescriptors <= result.descriptorCapacity;
  result.fingerprint = hash ? hash : 1;
  if (receipt) *receipt = result;
  return result.Complete() && result.footprintSpacingExact;
}

} // namespace GameplayLab
