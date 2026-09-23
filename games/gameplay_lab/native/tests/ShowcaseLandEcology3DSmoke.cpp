#include "ShowcaseLandEcology3D.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <iostream>

using namespace GameplayLab;

namespace {
bool Exact(const ShowcaseLandPlacement3D &left,
           const ShowcaseLandPlacement3D &right) {
  return left.kind == right.kind && left.band == right.band &&
      left.variant == right.variant && left.stableKey == right.stableKey &&
      left.zone == right.zone &&
      std::bit_cast<std::uint32_t>(left.position.x) == std::bit_cast<std::uint32_t>(right.position.x) &&
      std::bit_cast<std::uint32_t>(left.position.y) == std::bit_cast<std::uint32_t>(right.position.y) &&
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
      left.zones == right.zones &&
      left.placements == right.placements &&
      left.integratedDescriptors == right.integratedDescriptors &&
      left.descriptorCapacity == right.descriptorCapacity &&
      left.rejectedCandidates == right.rejectedCandidates &&
      left.attemptedCandidates == right.attemptedCandidates &&
      left.riverbankRocks == right.riverbankRocks &&
      left.riverbankGrass == right.riverbankGrass &&
      left.occupiedHabitatCells == right.occupiedHabitatCells &&
      left.densestHabitatCell == right.densestHabitatCell &&
      left.heroGroundCover == right.heroGroundCover &&
      left.heroGroundCoverQuadrantMask == right.heroGroundCoverQuadrantMask &&
      left.heroPathShoulderCluster == right.heroPathShoulderCluster &&
      left.heroCottageRearCluster == right.heroCottageRearCluster &&
      left.heroRiverForegroundCluster == right.heroRiverForegroundCluster &&
      left.heroForestEdgeRocks == right.heroForestEdgeRocks &&
      left.understoryGroundCover == right.understoryGroundCover &&
      left.understoryGroundCoverBandMask ==
          right.understoryGroundCoverBandMask &&
      left.failureKind == right.failureKind &&
      left.failureBand == right.failureBand &&
      left.failureItem == right.failureItem &&
      std::bit_cast<std::uint32_t>(left.minimumTreeSpacing) ==
          std::bit_cast<std::uint32_t>(right.minimumTreeSpacing) &&
      std::bit_cast<std::uint32_t>(left.minimumGrassSpacing) ==
          std::bit_cast<std::uint32_t>(right.minimumGrassSpacing) &&
      std::bit_cast<std::uint32_t>(left.minimumRockSpacing) ==
          std::bit_cast<std::uint32_t>(right.minimumRockSpacing) &&
      std::bit_cast<std::uint32_t>(left.minimumPropEdgeSpacing) ==
          std::bit_cast<std::uint32_t>(right.minimumPropEdgeSpacing) &&
      std::bit_cast<std::uint32_t>(left.minimumRiverbankRockClearance) ==
          std::bit_cast<std::uint32_t>(right.minimumRiverbankRockClearance) &&
      std::bit_cast<std::uint32_t>(left.maximumRiverbankRockClearance) ==
          std::bit_cast<std::uint32_t>(right.maximumRiverbankRockClearance) &&
      std::bit_cast<std::uint32_t>(left.maximumRiverbankGrassClearance) ==
          std::bit_cast<std::uint32_t>(right.maximumRiverbankGrassClearance) &&
      std::bit_cast<std::uint32_t>(left.meanRiverbankRockClearance) ==
          std::bit_cast<std::uint32_t>(right.meanRiverbankRockClearance) &&
      std::bit_cast<std::uint32_t>(left.meanRiverbankGrassClearance) ==
          std::bit_cast<std::uint32_t>(right.meanRiverbankGrassClearance) &&
      left.terrainBoundsExact == right.terrainBoundsExact &&
      left.bandsExact == right.bandsExact &&
      left.riverExcluded == right.riverExcluded &&
      left.spawnExcluded == right.spawnExcluded &&
      left.cottageExcluded == right.cottageExcluded &&
      left.stableKeysExact == right.stableKeysExact &&
      left.transformsExact == right.transformsExact &&
      left.footprintSpacingExact == right.footprintSpacingExact &&
      left.zonesExact == right.zonesExact &&
      left.riverbankLayered == right.riverbankLayered &&
      left.densityExact == right.densityExact &&
      left.heroGroundCoverExact == right.heroGroundCoverExact &&
      left.heroCompositionClustersExact ==
          right.heroCompositionClustersExact &&
      left.understoryGroundCoverExact == right.understoryGroundCoverExact &&
      left.heroForestEdgeRocksExact == right.heroForestEdgeRocksExact &&
      left.deterministicBudget == right.deterministicBudget &&
      left.walkingLaneClear == right.walkingLaneClear &&
      left.vegetationScaleBounded == right.vegetationScaleBounded;
}

bool IsLivingTreeVariant(ShowcaseLandVariant3D variant) {
  return variant == ShowcaseLandVariant3D::IslandTree ||
         variant == ShowcaseLandVariant3D::TreeSmall02;
}

bool TreeVariantDistributionContract(
    const std::vector<ShowcaseLandPlacement3D> &placements) {
  std::array<unsigned, 3> islandByBand{}, smallByBand{}, deadByBand{};
  unsigned island{}, small{}, dead{};
  for (const auto &placement : placements) {
    if (placement.kind != ShowcaseLandKind3D::Tree)
      continue;
    const auto band = std::size_t(placement.band);
    if (placement.variant == ShowcaseLandVariant3D::IslandTree) {
      ++island;
      ++islandByBand[band];
    } else if (placement.variant == ShowcaseLandVariant3D::TreeSmall02) {
      ++small;
      ++smallByBand[band];
    } else if (placement.variant == ShowcaseLandVariant3D::DeadTreeTrunk) {
      ++dead;
      ++deadByBand[band];
    }
  }
  return island == 148 && small == 124 && dead == 24 &&
         islandByBand == std::array<unsigned, 3>{62, 46, 40} &&
         smallByBand == std::array<unsigned, 3>{52, 38, 34} &&
         deadByBand == std::array<unsigned, 3>{10, 8, 6};
}

struct HeroClearingDressingMetrics {
  unsigned pathShoulders{};
  unsigned pathSideMask{};
  unsigned pathLongitudinalMask{};
  unsigned rootIslandCover{};
  unsigned rootIslandAnchors{};
  unsigned wetlandCover{};
  unsigned wetlandClearanceMask{};
  unsigned wetlandLongitudinalMask{};
};

float TestRiverCenterX(float z) {
  return -4.f + std::sin(z * .035f) * 5.f + std::sin(z * .011f) * 2.f;
}

float TestRiverWidth(float z) {
  return 12.f + std::sin(z * .057f + .7f) * 2.6f;
}

bool IsPathShoulder(const ShowcaseLandEcologySettings3D &settings,
                    const ShowcaseLandPlacement3D &placement,
                    unsigned *side = nullptr, unsigned *longitudinal = nullptr) {
  (void)settings;
  if (placement.kind != ShowcaseLandKind3D::Grass ||
      placement.zone == ShowcaseLandZone3D::Riverbank)
    return false;
  const auto trail = SampleShowcaseHeroTrail3D(placement.position);
  if (!trail.valid || trail.along < .08f || trail.along > .88f)
    return false;
  if (trail.distance < 1.65f || trail.distance > 5.5f)
    return false;
  if (side)
    *side = trail.signedSide >= 0.f ? 1u : 0u;
  if (longitudinal)
    *longitudinal = (std::min)(3u, unsigned(trail.along * 4.f));
  return true;
}

HeroClearingDressingMetrics HeroClearingDressingContract(
    const ShowcaseLandEcologySettings3D &settings,
    const std::vector<ShowcaseLandPlacement3D> &placements) {
  HeroClearingDressingMetrics metrics{};
  std::vector<bool> rootAnchorUsed(placements.size());
  for (const auto &placement : placements) {
    if (placement.kind != ShowcaseLandKind3D::Grass ||
        placement.band != ShowcaseLandBand3D::Near)
      continue;
    const float heroX = placement.position.x - settings.spawnCenter.x;
    const float heroZ = placement.position.z - settings.spawnCenter.z;
    if (heroX * heroX + heroZ * heroZ > 22.f * 22.f)
      continue;
    if (placement.zone == ShowcaseLandZone3D::Riverbank) {
      ++metrics.wetlandCover;
      const float clearance =
          std::abs(placement.position.x - TestRiverCenterX(placement.position.z)) -
          TestRiverWidth(placement.position.z) * .5f;
      metrics.wetlandClearanceMask |=
          1u << (std::min)(2u, unsigned((std::max)(0.f, clearance - 2.f)));
      const float wetlandT = (std::clamp)(
          (placement.position.z - (settings.spawnCenter.z - 14.f)) / 32.f,
          0.f, .999f);
      metrics.wetlandLongitudinalMask |= 1u << unsigned(wetlandT * 4.f);
      continue;
    }
    unsigned side{}, longitudinal{};
    if (IsPathShoulder(settings, placement, &side, &longitudinal)) {
      ++metrics.pathShoulders;
      metrics.pathSideMask |= 1u << side;
      metrics.pathLongitudinalMask |= 1u << longitudinal;
      continue;
    }
    for (std::size_t index = 0; index < placements.size(); ++index) {
      const auto &tree = placements[index];
      if (tree.kind != ShowcaseLandKind3D::Tree ||
          tree.band != ShowcaseLandBand3D::Near ||
          !IsLivingTreeVariant(tree.variant))
        continue;
      const float x = tree.position.x - placement.position.x;
      const float z = tree.position.z - placement.position.z;
      if (x * x + z * z > 7.5f * 7.5f)
        continue;
      ++metrics.rootIslandCover;
      if (!rootAnchorUsed[index]) {
        rootAnchorUsed[index] = true;
        ++metrics.rootIslandAnchors;
      }
      break;
    }
  }
  return metrics;
}

bool HeroClearingDressingExact(const HeroClearingDressingMetrics &metrics) {
  return metrics.pathShoulders >= 12 && metrics.pathSideMask == 0x3u &&
      std::popcount(metrics.pathLongitudinalMask) >= 3 &&
      metrics.rootIslandCover >= 18 && metrics.rootIslandAnchors >= 3 &&
      metrics.wetlandCover >= 12 &&
      std::popcount(metrics.wetlandClearanceMask) >= 2 &&
      std::popcount(metrics.wetlandLongitudinalMask) >= 3;
}

// R89：森林入口不能只靠遠處河岸岩石宣稱有岩層。四顆既有 Near 岩石
// 必須留在步道左側林緣，形成前、中景可讀的天然門框；仍保留完整步行帶、
// 角色出生安全圈與小屋維修淨空。這裡量測正式 placement，不用相機貼花。
bool ForestEdgeRockClusterContract(
    const ShowcaseLandEcologySettings3D &settings,
    const std::vector<ShowcaseLandPlacement3D> &placements) {
  unsigned rocks{}, longitudinalMask{}, sizeBands{};
  float minimumTrailClearance = 1e9f;
  for (const auto &placement : placements) {
    if (placement.kind != ShowcaseLandKind3D::Rock ||
        placement.band != ShowcaseLandBand3D::Near ||
        placement.zone != ShowcaseLandZone3D::Upland ||
        placement.position.x > -23.f || placement.position.z < -8.f ||
        placement.position.z > 32.f)
      continue;
    ++rocks;
    const auto trail = SampleShowcaseHeroTrail3D(placement.position);
    if (!trail.valid) return false;
    minimumTrailClearance =
        (std::min)(minimumTrailClearance, trail.distance);
    const float normalized = (placement.position.z + 8.f) / 40.f;
    longitudinalMask |= 1u << (std::min)(3u, unsigned(normalized * 4.f));
    const float span = (std::max)(placement.scale.x, placement.scale.z);
    sizeBands |= span < 2.4f ? 1u : span < 4.2f ? 2u : 4u;
    const float spawnX = placement.position.x - settings.spawnCenter.x;
    const float spawnZ = placement.position.z - settings.spawnCenter.z;
    if (spawnX * spawnX + spawnZ * spawnZ < 10.f * 10.f)
      return false;
  }
  std::cout << "FOREST_EDGE_ROCKS count=" << rocks
            << " longitudinal=" << std::popcount(longitudinalMask)
            << " sizeBands=" << std::popcount(sizeBands)
            << " trailClearance=" << minimumTrailClearance << '\n';
  return rocks == 4u && std::popcount(longitudinalMask) >= 3 &&
         std::popcount(sizeBands) >= 2 && minimumTrailClearance >= 2.4f;
}

// R89：112–230m 的遠景帶必須形成完整森林輪廓，而不是以 32 棵樹讓
// third-person 鏡頭直接露出裸山。這是世界空間扇區與高度層契約；不以
// fog、相機裁切或重複提交同一棵樹製造假密度。
bool FarForestWallContract(
    const ShowcaseLandEcologySettings3D &settings,
    const std::vector<ShowcaseLandPlacement3D> &placements) {
  unsigned trees{}, living{}, sectorMask{}, heightMask{};
  std::array<unsigned, 12> sectorCounts{};
  constexpr float twoPi = 6.28318530717958647692f;
  for (const auto &placement : placements) {
    if (placement.kind != ShowcaseLandKind3D::Tree ||
        placement.band != ShowcaseLandBand3D::Far)
      continue;
    ++trees;
    if (placement.variant != ShowcaseLandVariant3D::DeadTreeTrunk)
      ++living;
    float angle = std::atan2(placement.position.z - settings.spawnCenter.z,
                             placement.position.x - settings.spawnCenter.x);
    if (angle < 0.f) angle += twoPi;
    const unsigned sector = (std::min)(11u, unsigned(angle / twoPi * 12.f));
    sectorMask |= 1u << sector;
    ++sectorCounts[sector];
    heightMask |= placement.scale.y < 8.f ? 1u
                : placement.scale.y < 12.f ? 2u : 4u;
  }
  const auto [minimum, maximum] =
      std::ranges::minmax_element(sectorCounts);
  std::cout << "FAR_FOREST_WALL trees=" << trees << " living=" << living
            << " sectors=" << std::popcount(sectorMask)
            << " sectorRange=" << *minimum << '-' << *maximum
            << " heightTiers=" << std::popcount(heightMask) << '\n';
  return trees == 80u && living >= 72u && sectorMask == 0x0fffu &&
         *minimum >= 4u && *maximum <= 10u &&
         std::popcount(heightMask) == 3;
}

}

int main() {
  const ShowcaseLandEcologySettings3D settings{};
  std::vector<ShowcaseLandPlacement3D> first;
  ShowcaseLandEcologyReceipt3D receipt{};
  // 中文：暖機只填滿程式碼與配置器快取，不列入三毫秒正式門檻。
  const bool warmupBuilt=BuildShowcaseLandEcology3D(settings, first, &receipt);
  if (!warmupBuilt && first.size() == 512) {
    const bool diagnosticValid =
        ValidateShowcaseLandEcology3D(settings, first, &receipt);
    (void)diagnosticValid;
    for (std::size_t index = 0; index < first.size(); ++index) {
      const auto &placement = first[index];
      const float footprint = placement.kind == ShowcaseLandKind3D::Tree
          ? placement.scale.y * .11f
          : (std::max)(placement.scale.x, placement.scale.z) *
                (placement.kind == ShowcaseLandKind3D::Grass ? .34f : .5f);
      if (ShowcaseHeroTrailWalkingLane3D(placement.position, footprint))
        std::cerr << "walking lane overlap index=" << index
                  << " kind=" << unsigned(placement.kind)
                  << " band=" << unsigned(placement.band)
                  << " zone=" << unsigned(placement.zone)
                  << " position=" << placement.position.x << ','
                  << placement.position.z << " footprint=" << footprint
                  << '\n';
    }
  }
  if (!warmupBuilt || !receipt.Complete()) {
    std::cerr<<"land warmup gate built="<<warmupBuilt
             <<" count="<<first.size()<<" complete="<<receipt.Complete()
             <<" footprint="<<receipt.footprintSpacingExact
             <<" failure="<<receipt.failureKind<<'/'<<receipt.failureBand<<'/'<<receipt.failureItem
             <<" attempts="<<receipt.attemptedCandidates
             <<" rejected="<<receipt.rejectedCandidates
             <<" hero="<<receipt.heroGroundCover<<"/0x"<<std::hex
             <<receipt.heroGroundCoverQuadrantMask<<std::dec
             <<" density="<<receipt.densityExact
             <<" bands="<<receipt.bandsExact
             <<" spawn="<<receipt.spawnExcluded
             <<" cottage="<<receipt.cottageExcluded
             <<" clusters="<<receipt.heroPathShoulderCluster<<'/'
             <<receipt.heroCottageRearCluster<<'/'
             <<receipt.heroRiverForegroundCluster
             <<" clusterExact="<<receipt.heroCompositionClustersExact
             <<" understory="<<receipt.understoryGroundCover<<"/0x"<<std::hex
             <<receipt.understoryGroundCoverBandMask<<std::dec
             <<" spacing="<<receipt.minimumTreeSpacing<<'/'
             <<receipt.minimumGrassSpacing<<'/'<<receipt.minimumRockSpacing<<'/'
             <<receipt.minimumPropEdgeSpacing
             <<" river="<<receipt.riverbankLayered
             <<'/'<<receipt.meanRiverbankGrassClearance<<'/'
             <<receipt.meanRiverbankRockClearance
             <<" deterministic="<<receipt.deterministicBudget
             <<" lane="<<receipt.walkingLaneClear
             <<" scale="<<receipt.vegetationScaleBounded
             <<" fingerprint="<<receipt.fingerprint<<'\n';
    return 1;
  }
  if (!TreeVariantDistributionContract(first))
    return 35;
  if (!ForestEdgeRockClusterContract(settings, first))
    return 44;
  if (!FarForestWallContract(settings, first))
    return 45;
  if (!receipt.heroCompositionClustersExact ||
      receipt.heroPathShoulderCluster < 3u ||
      receipt.heroCottageRearCluster < 3u ||
      receipt.heroRiverForegroundCluster < 6u)
    return 42;
  // 三群必須由既有中景配置本身成立；移除任一樹群的三個 transform 應使正式
  // validator 拒絕，而不是只靠總 placement 數或測試內的別名計數通過。
  auto brokenComposition = first;
  unsigned movedHeroTrees{};
  for (auto &placement : brokenComposition)
    if (placement.kind == ShowcaseLandKind3D::Tree &&
        placement.band == ShowcaseLandBand3D::Mid &&
        placement.position.z >= 55.f && placement.position.z <= 80.f &&
        placement.position.x >= -40.f && movedHeroTrees < 3u) {
      ++movedHeroTrees;
      placement.position = {100.f + float(movedHeroTrees) * 6.f, 0.f, 20.f};
    }
  ShowcaseLandEcologyReceipt3D brokenCompositionReceipt{};
  if (movedHeroTrees < 3u ||
      ValidateShowcaseLandEcology3D(settings, brokenComposition,
                                    &brokenCompositionReceipt) ||
      brokenCompositionReceipt.heroCompositionClustersExact)
    return 43;
  auto concentratedDeadwood = first;
  for (auto &placement : concentratedDeadwood)
    if (placement.kind == ShowcaseLandKind3D::Tree)
      placement.variant = ShowcaseLandVariant3D::IslandTree;
  unsigned hostileDeadwood{};
  for (auto &placement : concentratedDeadwood)
    if (placement.kind == ShowcaseLandKind3D::Tree &&
        placement.band == ShowcaseLandBand3D::Near && hostileDeadwood < 20) {
      placement.variant = ShowcaseLandVariant3D::DeadTreeTrunk;
      ++hostileDeadwood;
    }
  if (hostileDeadwood != 20 ||
      TreeVariantDistributionContract(concentratedDeadwood))
    return 36;
  auto collapsedLivingSpecies = first;
  for (auto &placement : collapsedLivingSpecies)
    if (placement.variant == ShowcaseLandVariant3D::TreeSmall02)
      placement.variant = ShowcaseLandVariant3D::IslandTree;
  if (TreeVariantDistributionContract(collapsedLivingSpecies))
    return 37;
  // 中景活樹至少六成具有八公尺內同帶鄰樹；量測樹幹群聚，不冒充葉片像素覆蓋。
  // 八公尺保留既有 4.5 公尺根部淨空，且不把總樹數當作森林密度證據。
  const auto treeCommunityGate=[](const auto& values,ShowcaseLandBand3D band,unsigned minimumLiving){
    unsigned living{}, connected{};
    for(const auto& p:values)if(p.kind==ShowcaseLandKind3D::Tree&&
        p.band==band&&IsLivingTreeVariant(p.variant)){
      ++living;
      for(const auto& q:values)if(q.stableKey!=p.stableKey&&
          q.kind==ShowcaseLandKind3D::Tree&&q.band==p.band&&
          IsLivingTreeVariant(q.variant)){
        const float x=p.position.x-q.position.x,z=p.position.z-q.position.z;
        if(x*x+z*z<=64.f){++connected;break;}
      }
    }
    std::cout<<"TREE_COMMUNITY band="<<unsigned(band)<<" living="<<living<<" neighborWithin8m="<<connected<<'\n';
    return living>=minimumLiving&&connected*5u>=living*3u;
  };
  if(!treeCommunityGate(first,ShowcaseLandBand3D::Near,112)||
     !treeCommunityGate(first,ShowcaseLandBand3D::Mid,78))return 32;
  auto orchard=first;
  unsigned orchardIndex{};
  for(auto& p:orchard)if(p.kind==ShowcaseLandKind3D::Tree){
    p.position={float(orchardIndex%16u)*16.f,0,float(orchardIndex/16u)*16.f};
    ++orchardIndex;
  }
  if(treeCommunityGate(orchard,ShowcaseLandBand3D::Near,112)||
     treeCommunityGate(orchard,ShowcaseLandBand3D::Mid,78))return 33;
  // 固定玩家周圍 80 公尺的俯視冠層代理。這不是葉片像素覆蓋，而是以正式
  // placement 的冠幅 scale 計算可重現的世界空間聯集，專門攔截「樹很多，
  // 但都分散到鏡頭外」的果園式配置。
  const auto canopyCoverage=[](const auto& values){
    constexpr float radius=80.f,step=.5f;
    unsigned eligible{},covered{};
    for(int z=-160;z<160;++z)for(int x=-160;x<160;++x){
      const float worldX=-16.f+(float(x)+.5f)*step;
      const float worldZ=8.f+(float(z)+.5f)*step;
      const float dx=worldX+16.f,dz=worldZ-8.f;
      if(dx*dx+dz*dz>radius*radius)continue;
      ++eligible;
      for(const auto& p:values)if(p.kind==ShowcaseLandKind3D::Tree&&
          IsLivingTreeVariant(p.variant)){
        const float rx=p.scale.x*.5f,rz=p.scale.z*.5f;
        const float u=(worldX-p.position.x)/rx,v=(worldZ-p.position.z)/rz;
        if(u*u+v*v<=1.f){++covered;break;}
      }
    }
    const float fraction=eligible?float(covered)/float(eligible):0.f;
    std::cout<<"CANOPY_ROI80 proxyFraction="<<fraction
             <<" coveredM2="<<float(covered)*step*step<<'\n';
    return fraction;
  };
  const float canopyFraction=canopyCoverage(first);
  if(canopyFraction<.27f)return 34;
  // 固定世界空間 ROI，以未旋轉 scale 橢圓作可重現覆蓋代理；不是像素或不透明葉面積。
  const auto coverageGate=[&](const auto& first){
  unsigned covered{}, closePairs{},heroCount{};
  for (int z=-88;z<88;++z) for(int x=-88;x<88;++x) {
    const float dx=(float(x)+.5f)*.25f,dz=(float(z)+.5f)*.25f;
    if(dx*dx+dz*dz>22.f*22.f)continue;
    for(const auto& p:first)if(p.kind==ShowcaseLandKind3D::Grass){
      const float u=(settings.spawnCenter.x+dx-p.position.x)/(p.scale.x*.5f);
      const float v=(settings.spawnCenter.z+dz-p.position.z)/(p.scale.z*.5f);
      if(u*u+v*v<=1.f){++covered;break;}
    }
  }
  for(std::size_t i=0;i<first.size();++i)if(first[i].kind==ShowcaseLandKind3D::Grass){
    const auto p=first[i].position;const float dx=p.x-settings.spawnCenter.x,dz=p.z-settings.spawnCenter.z;
    if(dx*dx+dz*dz>484.f)continue;
    ++heroCount;
    float nearest=1e9f;
    for(std::size_t j=0;j<first.size();++j)if(i!=j&&first[j].kind==ShowcaseLandKind3D::Grass){
      const float x=p.x-first[j].position.x,z=p.z-first[j].position.z;nearest=(std::min)(nearest,x*x+z*z);
    }
    closePairs+=nearest<=2.5f*2.5f?1u:0u;
  }
  std::cout<<"HERO_ROI unrotatedEllipseProxyM2="<<covered*.0625f<<" nearbyWithin2_5m="<<closePairs<<" count="<<heroCount<<'\n';
  return covered*.0625f>=24.f&&closePairs>=24&&heroCount>=60;
  };
  if(!coverageGate(first))return 27;
  auto noCover=first;
  std::erase_if(noCover,[](const auto& p){return p.kind==ShowcaseLandKind3D::Grass;});
  if(coverageGate(noCover))return 28;
  const auto dressing = HeroClearingDressingContract(settings, first);
  std::cout << "HERO_DRESSING path=" << dressing.pathShoulders
            << "/sides=" << dressing.pathSideMask
            << "/longitudinal=" << std::popcount(dressing.pathLongitudinalMask)
            << " roots=" << dressing.rootIslandCover
            << "/anchors=" << dressing.rootIslandAnchors
            << " wetland=" << dressing.wetlandCover
            << "/clearance=" << std::popcount(dressing.wetlandClearanceMask)
            << "/longitudinal="
            << std::popcount(dressing.wetlandLongitudinalMask) << '\n';
  if (!HeroClearingDressingExact(dressing))
    return 38;
  auto noPathShoulders = first;
  for (auto &placement : noPathShoulders)
    if (IsPathShoulder(settings, placement))
      placement.position = {200.f, 0, 200.f};
  if (HeroClearingDressingExact(
          HeroClearingDressingContract(settings, noPathShoulders)))
    return 39;
  auto noRootIslands = first;
  for (auto &placement : noRootIslands)
    if (placement.kind == ShowcaseLandKind3D::Tree &&
        placement.band == ShowcaseLandBand3D::Near &&
        IsLivingTreeVariant(placement.variant))
      placement.position = {200.f, 0, 200.f};
  if (HeroClearingDressingExact(
          HeroClearingDressingContract(settings, noRootIslands)))
    return 40;
  auto noWetland = first;
  for (auto &placement : noWetland)
    if (placement.kind == ShowcaseLandKind3D::Grass &&
        placement.zone == ShowcaseLandZone3D::Riverbank)
      placement.position = {200.f, 0, 200.f};
  if (HeroClearingDressingExact(
          HeroClearingDressingContract(settings, noWetland)))
    return 41;
  // 同生境群落中心必須分離；子叢必須受自己的中心約束，不能只以總叢數宣稱群落。
  const auto communityGate=[](const auto& values){
    std::array<Aevrynd::Vec3,3> centers{};
    std::array<std::vector<Aevrynd::Vec3>,3> previousCenters;
    std::array<std::vector<Aevrynd::Vec3>,3> ribbonChildren;
    unsigned item{},children{},ribbons{},ribbonLike{};float withinSum{},withinMax{},betweenMin=1e9f,ribbonSpanSum{},ribbonWidthMax{};
    for(const auto& p:values)if(p.kind==ShowcaseLandKind3D::Grass&&p.band==ShowcaseLandBand3D::Near){
      const auto type=item%3u;
      if(p.zone!=ShowcaseLandZone3D::Riverbank){
        if(item%12u<3u){
          for(const auto q:previousCenters[type]){
            const float x=p.position.x-q.x,z=p.position.z-q.z;
            betweenMin=(std::min)(betweenMin,std::sqrt(x*x+z*z));
          }
          previousCenters[type].push_back(p.position);centers[type]=p.position;ribbonChildren[type].clear();
        }else{
          const float x=p.position.x-centers[type].x,z=p.position.z-centers[type].z;
          const float distance=std::sqrt(x*x+z*z);
          withinSum+=distance;withinMax=(std::max)(withinMax,distance);++children;
          ribbonChildren[type].push_back(p.position);
          if(ribbonChildren[type].size()==3){
            std::array<Aevrynd::Vec3,4> points{{centers[type],ribbonChildren[type][0],ribbonChildren[type][1],ribbonChildren[type][2]}};
            float spanSquared{};std::size_t first{},last{};
            for(std::size_t a=0;a<points.size();++a)for(std::size_t b=a+1;b<points.size();++b){
              const float px=points[b].x-points[a].x,pz=points[b].z-points[a].z,d2=px*px+pz*pz;
              if(d2>spanSquared){spanSquared=d2;first=a;last=b;}
            }
            const float span=std::sqrt(spanSquared),axisX=(points[last].x-points[first].x)/span,axisZ=(points[last].z-points[first].z)/span;
            float width{};for(const auto point:points){const float px=point.x-points[first].x,pz=point.z-points[first].z;width=(std::max)(width,std::abs(px*axisZ-pz*axisX));}
            ++ribbons;ribbonSpanSum+=span;ribbonWidthMax=(std::max)(ribbonWidthMax,width);
            ribbonLike+=span>=3.25f&&width<=1.55f?1u:0u;
          }
        }
      }
      ++item;
    }
    const float mean=children?withinSum/float(children):1e9f;
    std::cout<<"GRASS_COMMUNITY meanWithin="<<mean<<" maxWithin="<<withinMax<<" minBetween="<<betweenMin
             <<" ribbons="<<ribbonLike<<'/'<<ribbons<<" meanSpan="<<(ribbons?ribbonSpanSum/float(ribbons):0.f)
             <<" maxWidth="<<ribbonWidthMax<<'\n';
    return children==57&&withinMax<=7.1f&&mean<=3.5f&&betweenMin>=4.f&&betweenMin>=mean*1.5f&&
        ribbons==18&&ribbonLike>=14;
  };
  if(!communityGate(first))return 29;
  auto scattered=first;
  unsigned scatterOrdinal{};
  for(auto& p:scattered)if(p.kind==ShowcaseLandKind3D::Grass&&p.band==ShowcaseLandBand3D::Near){
    if(scatterOrdinal%12u>=3u)p.position.x+=20.f;
    ++scatterOrdinal;
  }
  if(communityGate(scattered))return 30;
  auto concentric=first;
  std::array<Aevrynd::Vec3,3> concentricCenters{};unsigned concentricItem{};
  for(auto& p:concentric)if(p.kind==ShowcaseLandKind3D::Grass&&p.band==ShowcaseLandBand3D::Near){
    const auto type=concentricItem%3u;
    if(p.zone!=ShowcaseLandZone3D::Riverbank){
      if(concentricItem%12u<3u)concentricCenters[type]=p.position;
      else{const float angle=float((concentricItem%12u)/3u-1u)*2.094395102f;p.position={concentricCenters[type].x+std::cos(angle)*2.5f,0,concentricCenters[type].z+std::sin(angle)*2.5f};}
    }
    ++concentricItem;
  }
  // 半徑與平均群聚距離都合理的同心三角草島，仍必須被弧帶形狀 gate 拒絕。
  if(communityGate(concentric))return 31;
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
  // 第二個正式環境 seed 也必須完整且逐位元可重現，不能只挑可成功的單一分布。
  auto alternateSettings = settings;
  alternateSettings.seed = 0x4a18u;
  std::vector<ShowcaseLandPlacement3D> alternate, alternateRepeated;
  ShowcaseLandEcologyReceipt3D alternateReceipt{}, alternateRepeatedReceipt{};
  if (!BuildShowcaseLandEcology3D(alternateSettings, alternate, &alternateReceipt) ||
      !BuildShowcaseLandEcology3D(alternateSettings, alternateRepeated, &alternateRepeatedReceipt) ||
      !ExactReceipt(alternateReceipt, alternateRepeatedReceipt) ||
      alternate.size() != alternateRepeated.size() ||
      !std::equal(alternate.begin(), alternate.end(), alternateRepeated.begin(), Exact) ||
      alternateReceipt.fingerprint == receipt.fingerprint) {
    std::cerr << "land alternate seed gate count=" << alternateReceipt.placements
              << " failure=" << alternateReceipt.failureKind << '/' << alternateReceipt.failureBand << '/' << alternateReceipt.failureItem << '\n';
    return 22;
  }
  // 兩個 seed 均與完整鄰格掃描參考路徑比較配置欄位、指紋及全部 receipt，非只比總數。
  if(!coverageGate(alternate)||!communityGate(alternate)||
     !treeCommunityGate(alternate,ShowcaseLandBand3D::Near,112)||
     !treeCommunityGate(alternate,ShowcaseLandBand3D::Mid,78)||
     canopyCoverage(alternate)<.27f)return 31;
  for (const auto seed : {settings.seed, alternateSettings.seed}) {
    auto referenceSettings = settings;
    referenceSettings.seed = seed;
    referenceSettings.exactCandidatePruning = false;
    std::vector<ShowcaseLandPlacement3D> reference;
    ShowcaseLandEcologyReceipt3D referenceReceipt{};
    const auto &expected = seed == settings.seed ? first : alternate;
    const auto &expectedReceipt = seed == settings.seed ? receipt : alternateReceipt;
    if (!BuildShowcaseLandEcology3D(referenceSettings, reference, &referenceReceipt) ||
        !ExactReceipt(referenceReceipt, expectedReceipt) || reference.size() != expected.size() ||
        !std::equal(reference.begin(), reference.end(), expected.begin(), Exact)) {
      std::cerr << "land pruning reference mismatch seed=" << seed
                << " fingerprint=" << expectedReceipt.fingerprint << '/' << referenceReceipt.fingerprint << '\n';
      return 23;
    }
  }

  if (receipt.placements != 512 || receipt.integratedDescriptors != 521 ||
      receipt.descriptorCapacity != 528 ||
      receipt.trees != std::array<std::uint32_t, 3>{124, 92, 80} ||
      receipt.grass != std::array<std::uint32_t, 3>{116, 52, 18} ||
      receipt.rocks != std::array<std::uint32_t, 3>{12, 12, 6} ||
      receipt.variants != std::array<std::uint32_t, 8>{148, 124, 24, 20, 10, 62, 62, 62} ||
      !receipt.terrainBoundsExact || !receipt.bandsExact || !receipt.riverExcluded ||
      !receipt.spawnExcluded || !receipt.cottageExcluded ||
      !receipt.stableKeysExact || !receipt.transformsExact) return 2;
  if (!receipt.footprintSpacingExact || !receipt.zonesExact ||
      !receipt.riverbankLayered || !receipt.densityExact ||
      receipt.riverbankRocks != 26 || receipt.riverbankGrass != 62 ||
      receipt.heroForestEdgeRocks != 4 ||
      !receipt.heroForestEdgeRocksExact ||
      receipt.heroGroundCover < 28 ||
      receipt.heroGroundCoverQuadrantMask != 0x0fu ||
      !receipt.heroGroundCoverExact ||
      receipt.understoryGroundCover < 80 ||
      receipt.understoryGroundCoverBandMask != 0x07u ||
      !receipt.understoryGroundCoverExact ||
      receipt.minimumPropEdgeSpacing < .12f ||
      receipt.maximumRiverbankRockClearance >
          settings.riverbankRockMaximumClearance ||
      receipt.maximumRiverbankGrassClearance >
          settings.riverbankGrassMaximumClearance ||
      receipt.meanRiverbankGrassClearance + 1.f >
          receipt.meanRiverbankRockClearance ||
      receipt.densestHabitatCell < settings.minimumDensestHabitatCell)
    return 12;
  if(!receipt.attemptedCandidates||receipt.attemptedCandidates>32000u||medianBuildMicros>3000){std::cerr<<"land performance attempts="<<receipt.attemptedCandidates<<" medianElapsedUs="<<medianBuildMicros<<" samples=";for(const auto value:buildMicros)std::cerr<<value<<',';std::cerr<<'\n';return 15;}
  // 子叢找到安全點即停止；候選數可減少，但不能超過原本二萬四千次上限。
  if (receipt.attemptedCandidates > 24000u ||
      receipt.zones != std::array<std::uint32_t, 4>{296, 62, 88, 66}) return 19;

  std::array<unsigned,3> canopyLayers{};
  for(const auto& item:first)if(IsLivingTreeVariant(item.variant))
    ++canopyLayers[item.scale.y<6.f?0:item.scale.y<11.f?1:2];
  if(!receipt.walkingLaneClear||!receipt.vegetationScaleBounded||
      std::ranges::any_of(canopyLayers,[](unsigned count){return count<30;}))return 24;
  auto oversized=first;
  const auto oversizedGrass=std::ranges::find_if(oversized,[](const auto& item){return item.kind==ShowcaseLandKind3D::Grass;});
  oversizedGrass->scale.y=2.f;
  ShowcaseLandEcologyReceipt3D oversizedReceipt{};
  if(ValidateShowcaseLandEcology3D(settings,oversized,&oversizedReceipt)||oversizedReceipt.vegetationScaleBounded)return 25;
  auto blockedLane=first;
  const auto laneGrass=std::ranges::find_if(blockedLane,[](const auto& item){return item.kind==ShowcaseLandKind3D::Grass;});
  laneGrass->position = SampleShowcaseHeroTrailFrame3D(.35f).position;
  ShowcaseLandEcologyReceipt3D laneReceipt{};
  if(ValidateShowcaseLandEcology3D(settings,blockedLane,&laneReceipt)||laneReceipt.walkingLaneClear)return 26;

  for (const auto variant : {ShowcaseLandVariant3D::IslandTree,
                             ShowcaseLandVariant3D::TreeSmall02,
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
  const auto grass=std::ranges::find_if(mutated,[](const auto& item){return item.kind==ShowcaseLandKind3D::Grass;});
  const auto overlapRock=std::ranges::find_if(mutated,[](const auto& item){return item.kind==ShowcaseLandKind3D::Rock;});
  grass->position=overlapRock->position;
  if(ValidateShowcaseLandEcology3D(settings,mutated))return 16;
  mutated = first;
  mutated[1].stableKey = mutated[0].stableKey;
  if (ValidateShowcaseLandEcology3D(settings, mutated)) return 6;
  mutated = first;
  for (auto &item : mutated)
    if (item.kind == ShowcaseLandKind3D::Tree)
      item.variant = ShowcaseLandVariant3D::IslandTree;
  if (ValidateShowcaseLandEcology3D(settings, mutated)) return 7;
  // 林相亦須拒絕倒退為大量枯木，不能只確認「兩種樹都存在」。
  mutated = first;
  const auto livingTree = std::ranges::find_if(mutated, [](const auto &item) {
    return item.variant == ShowcaseLandVariant3D::IslandTree;
  });
  if (livingTree == mutated.end()) return 20;
  livingTree->variant = ShowcaseLandVariant3D::DeadTreeTrunk;
  if (ValidateShowcaseLandEcology3D(settings, mutated)) return 21;
  mutated = first;
  for (auto &item : mutated)
    if (item.kind == ShowcaseLandKind3D::Grass)
      item.band = ShowcaseLandBand3D::Near;
  if (ValidateShowcaseLandEcology3D(settings, mutated)) return 8;
  mutated = first;
  mutated.front().zone = ShowcaseLandZone3D::Meadow;
  if (ValidateShowcaseLandEcology3D(settings, mutated)) return 17;
  auto impossibleDensity = settings;
  impossibleDensity.minimumDensestHabitatCell = 457;
  if (ValidateShowcaseLandEcology3D(impossibleDensity, first)) return 18;
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
            << " attempts=" << receipt.attemptedCandidates
            << " integrated=" << receipt.integratedDescriptors
            << " trees=" << receipt.trees[0] << '/' << receipt.trees[1] << '/' << receipt.trees[2]
            << " liveTrees=" << receipt.variants[0] << '/' << receipt.variants[1]
            << " dead=" << receipt.variants[2]
            << " grass=" << receipt.grass[0] << '/' << receipt.grass[1] << '/' << receipt.grass[2]
            << " spacing=" << receipt.minimumTreeSpacing << '/'
            << receipt.minimumGrassSpacing << '/'
            << receipt.minimumRockSpacing
            << " propEdge=" << receipt.minimumPropEdgeSpacing
            << " bank=" << receipt.riverbankRocks << '/'
            << receipt.riverbankGrass << '@'
            << receipt.maximumRiverbankRockClearance << '/'
            << receipt.maximumRiverbankGrassClearance << " mean="
            << receipt.meanRiverbankRockClearance << '/'
            << receipt.meanRiverbankGrassClearance
            << " habitat=" << receipt.occupiedHabitatCells << '/'
            << receipt.densestHabitatCell
            << " hero=" << receipt.heroGroundCover << "/0x" << std::hex
            << receipt.heroGroundCoverQuadrantMask << std::dec
            << " understory=" << receipt.understoryGroundCover << "/0x"
            << std::hex << receipt.understoryGroundCoverBandMask << std::dec
            << " pruningReferenceSeeds=2 medianUs=" << medianBuildMicros << " samples=";
  for (const auto value : buildMicros) std::cout << value << ',';
  std::cout << '\n';
  return 0;
}
