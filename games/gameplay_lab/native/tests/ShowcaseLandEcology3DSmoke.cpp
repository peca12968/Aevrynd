#include "ShowcaseLandEcology3D.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <iostream>

using namespace GameplayLab;

namespace {
bool Exact(const ShowcaseLandPlacement3D &left,
           const ShowcaseLandPlacement3D &right) {
  return left.kind == right.kind && left.band == right.band &&
      left.variant == right.variant && left.stableKey == right.stableKey &&
      std::bit_cast<std::uint32_t>(left.position.x) == std::bit_cast<std::uint32_t>(right.position.x) &&
      std::bit_cast<std::uint32_t>(left.position.z) == std::bit_cast<std::uint32_t>(right.position.z) &&
      std::bit_cast<std::uint32_t>(left.scale.x) == std::bit_cast<std::uint32_t>(right.scale.x) &&
      std::bit_cast<std::uint32_t>(left.scale.y) == std::bit_cast<std::uint32_t>(right.scale.y) &&
      std::bit_cast<std::uint32_t>(left.scale.z) == std::bit_cast<std::uint32_t>(right.scale.z) &&
      std::bit_cast<std::uint32_t>(left.yaw) == std::bit_cast<std::uint32_t>(right.yaw);
}

bool ExactReceipt(const ShowcaseLandEcologyReceipt3D &left,
                  const ShowcaseLandEcologyReceipt3D &right) {
  return left.seed == right.seed && left.fingerprint == right.fingerprint &&
      left.trees == right.trees && left.grass == right.grass &&
      left.rocks == right.rocks && left.variants == right.variants &&
      left.placements == right.placements &&
      left.integratedDescriptors == right.integratedDescriptors &&
      left.descriptorCapacity == right.descriptorCapacity &&
      left.rejectedCandidates == right.rejectedCandidates &&
      left.attemptedCandidates == right.attemptedCandidates &&
      left.failureKind == right.failureKind &&
      left.failureBand == right.failureBand &&
      left.failureItem == right.failureItem &&
      std::bit_cast<std::uint32_t>(left.minimumTreeSpacing) ==
          std::bit_cast<std::uint32_t>(right.minimumTreeSpacing) &&
      std::bit_cast<std::uint32_t>(left.minimumGrassSpacing) ==
          std::bit_cast<std::uint32_t>(right.minimumGrassSpacing) &&
      std::bit_cast<std::uint32_t>(left.minimumRockSpacing) ==
          std::bit_cast<std::uint32_t>(right.minimumRockSpacing) &&
      left.terrainBoundsExact == right.terrainBoundsExact &&
      left.bandsExact == right.bandsExact &&
      left.riverExcluded == right.riverExcluded &&
      left.spawnExcluded == right.spawnExcluded &&
      left.cottageExcluded == right.cottageExcluded &&
      left.stableKeysExact == right.stableKeysExact &&
      left.transformsExact == right.transformsExact &&
      left.footprintSpacingExact == right.footprintSpacingExact &&
      left.deterministicBudget == right.deterministicBudget;
}
}

int main() {
  const ShowcaseLandEcologySettings3D settings{};
  std::vector<ShowcaseLandPlacement3D> first;
  ShowcaseLandEcologyReceipt3D receipt{};
  // 中文：暖機只填滿程式碼與配置器快取，不列入三毫秒正式門檻。
  const bool warmupBuilt=BuildShowcaseLandEcology3D(settings, first, &receipt);
  if (!warmupBuilt || !receipt.Complete()) {
    std::cerr<<"land warmup gate built="<<warmupBuilt
             <<" count="<<first.size()<<" complete="<<receipt.Complete()
             <<" footprint="<<receipt.footprintSpacingExact
             <<" failure="<<receipt.failureKind<<'/'<<receipt.failureBand<<'/'<<receipt.failureItem
             <<" attempts="<<receipt.attemptedCandidates
             <<" rejected="<<receipt.rejectedCandidates
             <<" fingerprint="<<receipt.fingerprint<<'\n';
    return 1;
  }

  constexpr std::size_t TimedBuildCount=5;
  std::array<long long,TimedBuildCount> buildMicros{};
  for(std::size_t run=0;run<TimedBuildCount;++run){
    std::vector<ShowcaseLandPlacement3D> repeated;
    ShowcaseLandEcologyReceipt3D repeatedReceipt{};
    const auto buildStart=std::chrono::steady_clock::now();
    const bool repeatedBuilt=BuildShowcaseLandEcology3D(settings,repeated,&repeatedReceipt);
    buildMicros[run]=std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-buildStart).count();
    if(!repeatedBuilt||!ExactReceipt(receipt,repeatedReceipt)||
       first.size()!=repeated.size()||
       !std::equal(first.begin(),first.end(),repeated.begin(),Exact)){
      std::cerr<<"land deterministic gate run="<<run
               <<" built="<<repeatedBuilt
               <<" firstCount="<<first.size()<<" repeatedCount="<<repeated.size()
               <<" receiptExact="<<ExactReceipt(receipt,repeatedReceipt)
               <<" complete="<<repeatedReceipt.Complete()
               <<" failure="<<repeatedReceipt.failureKind<<'/'
               <<repeatedReceipt.failureBand<<'/'<<repeatedReceipt.failureItem
               <<" attempts="<<repeatedReceipt.attemptedCandidates
               <<" rejected="<<repeatedReceipt.rejectedCandidates
               <<" fingerprint="<<receipt.fingerprint<<'/'
               <<repeatedReceipt.fingerprint<<'\n';
      return 1;
    }
  }
  const auto sortedBuildMicros=[](auto values){std::ranges::sort(values);return values;}(buildMicros);
  const auto medianBuildMicros=sortedBuildMicros[TimedBuildCount/2];

  if (receipt.placements != 456 || receipt.integratedDescriptors != 464 ||
      receipt.descriptorCapacity != 480 ||
      receipt.trees != std::array<std::uint32_t, 3>{48, 64, 32} ||
      receipt.grass != std::array<std::uint32_t, 3>{108, 120, 54} ||
      receipt.rocks != std::array<std::uint32_t, 3>{12, 12, 6} ||
      !receipt.terrainBoundsExact || !receipt.bandsExact || !receipt.riverExcluded ||
      !receipt.spawnExcluded || !receipt.cottageExcluded ||
      !receipt.stableKeysExact || !receipt.transformsExact) return 2;
  if (!receipt.footprintSpacingExact) return 12;
  if(!receipt.attemptedCandidates||receipt.attemptedCandidates>32000u||medianBuildMicros>3000){std::cerr<<"land performance attempts="<<receipt.attemptedCandidates<<" medianElapsedUs="<<medianBuildMicros<<" samples=";for(const auto value:buildMicros)std::cerr<<value<<',';std::cerr<<'\n';return 15;}

  for (const auto variant : {ShowcaseLandVariant3D::IslandTree,
                             ShowcaseLandVariant3D::DeadTreeTrunk,
                             ShowcaseLandVariant3D::Boulder,
                             ShowcaseLandVariant3D::CoastalCliff,
                             ShowcaseLandVariant3D::GrassBlade,
                             ShowcaseLandVariant3D::GrassFan,
                             ShowcaseLandVariant3D::GrassTuft})
    if (std::ranges::none_of(first, [variant](const auto &item) {
          return item.variant == variant && !ShowcaseLandAssetId3D(variant).empty();
        })) return 3;

  // 中文：每個負向變更只破壞一項正式契約，不能靠縮小樣本或放寬預算通過。
  auto mutated = first;
  mutated[0].position = settings.spawnCenter;
  if (ValidateShowcaseLandEcology3D(settings, mutated)) return 4;
  mutated = first;
  mutated[1].position = mutated[0].position;
  if (ValidateShowcaseLandEcology3D(settings, mutated)) return 5;
  mutated = first;
  const auto tree=std::ranges::find_if(mutated,[](const auto& item){return item.kind==ShowcaseLandKind3D::Tree;});
  const auto rock=std::ranges::find_if(mutated,[](const auto& item){return item.kind==ShowcaseLandKind3D::Rock;});
  if(tree==mutated.end()||rock==mutated.end())return 13;
  rock->position=tree->position;
  if(ValidateShowcaseLandEcology3D(settings,mutated))return 14;
  mutated = first;
  mutated[1].stableKey = mutated[0].stableKey;
  if (ValidateShowcaseLandEcology3D(settings, mutated)) return 6;
  mutated = first;
  for (auto &item : mutated)
    if (item.kind == ShowcaseLandKind3D::Tree)
      item.variant = ShowcaseLandVariant3D::IslandTree;
  if (ValidateShowcaseLandEcology3D(settings, mutated)) return 7;
  mutated = first;
  for (auto &item : mutated)
    if (item.kind == ShowcaseLandKind3D::Grass)
      item.band = ShowcaseLandBand3D::Near;
  if (ValidateShowcaseLandEcology3D(settings, mutated)) return 8;
  mutated = first;
  mutated[2].yaw += .125f;
  ShowcaseLandEcologyReceipt3D fingerprintMutation{};
  if (!ValidateShowcaseLandEcology3D(settings, mutated, &fingerprintMutation) ||
      fingerprintMutation.fingerprint == receipt.fingerprint) return 9;
  auto overBudget = settings;
  overBudget.maximumIntegratedDescriptors = 463;
  if (BuildShowcaseLandEcology3D(overBudget, mutated)) return 10;
  auto invalidSeed = settings;
  invalidSeed.seed = 0;
  if (BuildShowcaseLandEcology3D(invalidSeed, mutated)) return 11;

  std::cout << "SHOWCASE_LAND_ECOLOGY=PASS fingerprint=" << receipt.fingerprint
            << " placements=" << receipt.placements
            << " integrated=" << receipt.integratedDescriptors
            << " spacing=" << receipt.minimumTreeSpacing << '/'
            << receipt.minimumGrassSpacing << '/'
            << receipt.minimumRockSpacing
            << " medianUs=" << medianBuildMicros << '\n';
  return 0;
}
