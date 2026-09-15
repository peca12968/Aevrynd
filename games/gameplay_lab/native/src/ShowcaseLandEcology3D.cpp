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
constexpr std::uint32_t HeroGroundCoverOrdinals = 84;
constexpr std::uint32_t MinimumHeroGroundCover = 28;
constexpr float UnderstoryTreeRadius = 12.f;
constexpr std::uint32_t MinimumUnderstoryGroundCover = 80;

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
  if (kind == ShowcaseLandKind3D::Tree) return scale.y * .11f;
  return span * (kind == ShowcaseLandKind3D::Grass ? .34f : .5f);
}

// 玩家到屋前的連續步行帶保留根部淨空；不是用降低相機假裝可通行。
bool InWalkingLane(const ShowcaseLandEcologySettings3D &settings,
                   Aevrynd::Vec3 point, float footprint) noexcept {
  const auto a=settings.spawnCenter;
  const Aevrynd::Vec3 b{settings.cottageCenter.x,0,settings.cottageCenter.z-5.f};
  const float dx=b.x-a.x,dz=b.z-a.z;
  const float lengthSquared=dx*dx+dz*dz;
  const float t=lengthSquared>0.f?(std::clamp)(((point.x-a.x)*dx+(point.z-a.z)*dz)/lengthSquared,0.f,1.f):0.f;
  return DistanceSquared(point,{a.x+t*dx,0,a.z+t*dz})<(1.4f+footprint)*(1.4f+footprint);
}

bool InExclusion(const ShowcaseLandEcologySettings3D &settings,
                 ShowcaseLandKind3D kind, Aevrynd::Vec3 point,
                 float footprint) noexcept {
  const float limit = settings.terrainHalfSpan - settings.terrainMargin;
  if (InWalkingLane(settings,point,footprint)) return true;
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

constexpr bool IsLivingTreeVariant(ShowcaseLandVariant3D variant) noexcept {
  return variant == ShowcaseLandVariant3D::IslandTree ||
         variant == ShowcaseLandVariant3D::TreeSmall02;
}

ShowcaseLandVariant3D Variant(ShowcaseLandKind3D kind,
                             std::uint32_t ordinal) noexcept {
  if (kind == ShowcaseLandKind3D::Tree) {
    // 八成以上仍為活樹；每十二株保留一株枯立木，以既有三段 LOD 的第二套
    // 樹形打散單一冠層剪影，同時避免把森林讀成枯敗林。
    if (ordinal % 12u == 11u)
      return ShowcaseLandVariant3D::DeadTreeTrunk;
    // 三個距離帶共 60 株使用第二種活闊葉樹；與枯木槽位互斥，因此總數固定。
    return ordinal % 4u == 1u ? ShowcaseLandVariant3D::TreeSmall02
                              : ShowcaseLandVariant3D::IslandTree;
  }
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
    // 幼樹、次冠層、主冠層三個高度帶；同一網格的縮放不宣稱新增樹種。
    const float height = variant == ShowcaseLandVariant3D::DeadTreeTrunk
                             ? 5.f + a * 3.f
                             : a < .2f ? 4.5f + a * 7.5f
                             : a < .65f ? 6.f + (a-.2f) * 11.111111f
                                        : 11.f + (a-.65f) * 13.f;
    // 闊葉樹的成熟冠幅通常不應只有樹高的一半；維持同一網格、三個 LOD 與
    // 固定 240 株預算，只讓近中景冠層形成可讀的交疊，不增加 draw 數。
    // 新增的稀疏枯立木不應以削薄既有活冠層換取輪廓多樣性；僅補回一個
    // 兩個百分點冠幅，維持 default 與 alternate seed 的既有固定 ROI canopy gate。
    const float width = height * (.70f + b * .18f) *
                        (IsLivingTreeVariant(variant) ? 1.02f : 1.f);
    return {width, height, width * (.90f + a * .18f)};
  }
  if (kind == ShowcaseLandKind3D::Rock) {
    const float size = (1.1f + a * 3.1f) *
                       (variant == ShowcaseLandVariant3D::CoastalCliff
                            ? 2.f
                            : 1.f);
    return {size * (.8f + b * .4f), size * (.55f + b * .25f), size};
  }
  // 草仍為既有程序三角網格，限制到踝／小腿尺度，避免膝高硬片遮住人物。
  // 一個 placement 代表交叉葉簇而非單葉；加寬水平覆蓋、保留既有矮株高度，
  // 讓固定 186 株預算在近景真的讀成林下層，而不是零星細線。
  const float width = .45f + a * .75f;
  const float height = variant == ShowcaseLandVariant3D::GrassBlade
                           ? .16f + b * .28f
                           : variant == ShowcaseLandVariant3D::GrassFan
                                 ? .34f + b * .10f
                                 : .31f + b * .09f;
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
      trees == std::array<std::uint32_t, 3>{124, 88, 28} &&
      grass == std::array<std::uint32_t, 3>{116, 52, 18} &&
      rocks == std::array<std::uint32_t, 3>{12, 12, 6} &&
      variants == std::array<std::uint32_t, 8>{160, 60, 20, 20, 10, 62, 62, 62} &&
      zones[0] && zones[1] && zones[2] && zones[3] &&
      minimumTreeSpacing >= RequiredSpacing[0] &&
      minimumGrassSpacing >= RequiredSpacing[1] &&
      minimumRockSpacing >= RequiredSpacing[2] && terrainBoundsExact &&
      bandsExact && riverExcluded && spawnExcluded && cottageExcluded &&
      stableKeysExact && transformsExact && footprintSpacingExact && zonesExact &&
      riverbankLayered && densityExact && heroGroundCoverExact &&
      understoryGroundCoverExact &&
      heroGroundCover >= MinimumHeroGroundCover &&
      heroGroundCoverQuadrantMask == 0x0fu && riverbankRocks == 30 &&
      riverbankGrass == 62 &&
      minimumPropEdgeSpacing >= MinimumPropEdgeSpacing && deterministicBudget &&
      walkingLaneClear && vegetationScaleBounded;
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
  std::array<std::vector<Aevrynd::Vec3>, 3> livingTreeAnchors;
  std::array<std::array<std::vector<Aevrynd::Vec3>, 3>, 3>
      grassCommunityAnchors;
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
    // 依葉叢序號保存中心，每四株形成一群；河岸另投影至正式河流淨空帶。
    // 非河岸群落保留完整道路、屋門、角色及樹根間距查詢。
    std::array<Aevrynd::Vec3,3> grassCenters{};
    Aevrynd::Vec3 treeCenter{};
    for (std::uint32_t band = 0; band < 3; ++band)
      for (std::uint32_t item = 0; item < counts[band]; ++item, ++ordinal) {
        const auto variant = Variant(kind, ordinal);
        const auto zone = Zone(kind, variant);
        const auto scale = Scale(kind, variant, random);
        const float footprint = PlacementFootprint(kind, scale);
        Aevrynd::Vec3 best{};
        float bestDistance = -1.f;
        bool found{};
        const bool heroWetland = kind == ShowcaseLandKind3D::Grass &&
                                 band == 0 &&
                                 zone == ShowcaseLandZone3D::Riverbank &&
                                 item < 18u;
        const bool heroGroundCover = kind == ShowcaseLandKind3D::Grass &&
                                     band == 0 &&
                                     (heroWetland ||
                                      (zone != ShowcaseLandZone3D::Riverbank &&
                                       item < HeroGroundCoverOrdinals));
        const bool pathShoulderCommunity =
            kind == ShowcaseLandKind3D::Grass && band == 0 &&
            zone != ShowcaseLandZone3D::Riverbank && item < 24u;
        const std::uint32_t boundedTrials =
            heroGroundCover
                ? settings.candidateTrials
                : kind == ShowcaseLandKind3D::Grass
                ? settings.candidateTrials / 4u
                : kind == ShowcaseLandKind3D::Rock
                      ? settings.candidateTrials / 2u
                      // 四十八候選覆蓋完整環帶，保留三毫秒 CPU 預算。
                      : settings.candidateTrials * 3u / 8u;
        const float seedPhase = float((settings.seed ^
            (std::uint64_t(ordinal + 1) * 0x9e3779b97f4a7c15ull)) & 0xffffu) /
            65536.f * 2.f * Pi;
        float radialDirectionX = std::cos(seedPhase);
        float radialDirectionZ = std::sin(seedPhase);
        // 不等角群落留下較寬空地，避免六等分環形人工排列。
        constexpr std::array<float,6> communityAngles{.18f,.86f,2.22f,3.05f,4.38f,5.72f};
        // 中景樹每四株共用一棵母樹；角槽必須由母群序號推進。若直接用
        // 單株 ordinal，母樹的 0,4,8... stride 與六槽有公因數，只會留下
        // 三輻。seedPhase 仍只提供原本的小幅抖動，不改群落槽的六向覆蓋。
        const std::uint32_t communityOrdinal =
            kind == ShowcaseLandKind3D::Tree && band == 1u ? ordinal / 4u
                                                           : ordinal;
        // 以和六槽互質的 5 做固定置換，解除母群序號與既有半徑候選
        // (ordinal * 37) 的相鄰相關；六槽計數不變，但不讓相鄰母群與
        // 同一組陰影／投影邊界規律鎖定。
        const std::uint32_t communitySlot =
            kind == ShowcaseLandKind3D::Tree && band == 1u
                ? (communityOrdinal * 5u + band * 2u) %
                      std::uint32_t(communityAngles.size())
                : communityOrdinal % std::uint32_t(communityAngles.size());
        const float communityCenter =
            communityAngles[communitySlot] +
            std::fmod(seedPhase, .17f) +
            // low=41 的近景母樹保留原群落槽，只沿弧移開 third-day 左側
            // temporal tile 邊界；小角度避免改寫後續群落拓撲與英雄區草帶。
            (kind == ShowcaseLandKind3D::Tree && band == 0u && item == 8u
                 ? .01f
                 : 0.f);
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
              zone != ShowcaseLandZone3D::Riverbank &&
              (kind != ShowcaseLandKind3D::Tree || trial < boundedTrials / 2u)) {
            // 六個可辨識群落保留視線廊道；每個群落內仍用低差異序列找無重疊位置。
            // 樹的後半候選沿徑向序列跨群落找淨空，不增加候選預算。
            const float offset = (communityFraction - .5f) *
                                 (kind == ShowcaseLandKind3D::Tree ? .65f : .9f);
            directionX = communityX - communityZ * offset;
            directionZ = communityZ + communityX * offset;
            const float inverseLength = 1.f /
                std::sqrt(directionX * directionX + directionZ * directionZ);
            directionX *= inverseLength;
            directionZ *= inverseLength;
          }
          communityFraction += .618033989f;
          if (communityFraction >= 1.f) communityFraction -= 1.f;
          // 不同樹使用固定互質偏移，避免半徑／群落方位始終彼此鎖定。
          const std::uint32_t treeRadiusIndex = (trial * 8u / 3u + ordinal * 37u) % 128u;
          float radius{};
          if (heroGroundCover) {
            // 英雄區保留全部近景非河岸群落及前六株河岸地被；其餘河岸維持完整環帶。
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
          // 前兩組非河岸母叢沿屋前步道兩肩交錯配置。中心離中線至少三公尺，
          // 後續仍通過帶足跡的 1.4m 半寬 gate，因此保留完整 2.8m 步行帶。
          if (pathShoulderCommunity && item % 12u < 3u) {
            const Aevrynd::Vec3 laneEnd{settings.cottageCenter.x, 0,
                                        settings.cottageCenter.z - 5.f};
            const float laneX = laneEnd.x - settings.spawnCenter.x;
            const float laneZ = laneEnd.z - settings.spawnCenter.z;
            const float inverseLaneLength =
                1.f / std::sqrt(laneX * laneX + laneZ * laneZ);
            const float tangentX = laneX * inverseLaneLength;
            const float tangentZ = laneZ * inverseLaneLength;
            const float normalX = -tangentZ;
            const float normalZ = tangentX;
            const std::uint32_t communityIndex = item / 12u;
            const std::uint32_t grassType = item % 3u;
            const float laneT = .18f + float(communityIndex) * .25f +
                                (grassType == 2u ? .055f : 0.f);
            const float side = ((communityIndex + grassType) & 1u) ? -1.f : 1.f;
            const float shoulderOffset =
                3.05f + .35f * float((communityIndex + grassType) % 3u);
            const float alongJitter = (communityFraction - .5f) * 1.2f;
            const float outwardJitter =
                (float(trial) / float((std::max)(1u, boundedTrials - 1u)) -
                 .5f) * .65f;
            candidate = {
                settings.spawnCenter.x + laneX * laneT +
                    tangentX * alongJitter + normalX * side *
                    (shoulderOffset + outwardJitter),
                0,
                settings.spawnCenter.z + laneZ * laneT +
                    tangentZ * alongJitter + normalZ * side *
                    (shoulderOffset + outwardJitter)};
          }
          // 每個非河岸地被母叢直接取同距離帶活樹作錨點；子叢沿用母叢，
          // 形成可見的林下島狀覆蓋。Near 僅選英雄 ROI 內側樹，維持角色安全圈。
          const bool understoryCenter =
              kind == ShowcaseLandKind3D::Grass &&
              zone != ShowcaseLandZone3D::Riverbank && item % 12u < 3u &&
              !pathShoulderCommunity && !livingTreeAnchors[band].empty();
          if (understoryCenter) {
            const auto &anchors = livingTreeAnchors[band];
            std::size_t anchorIndex =
                (std::size_t(ordinal) * 11u + std::size_t(trial) * 7u) %
                anchors.size();
            const float rootOffset = 1.25f + 4.75f *
                (float(trial) + .5f) / float(boundedTrials);
            candidate = {anchors[anchorIndex].x + directionX * rootOffset, 0,
                         anchors[anchorIndex].z + directionZ * rootOffset};
          }
          // 中遠景四株一群，先在母樹周圍找安全點；後半候選保留原環帶退路。
          // 根部間距、河岸與碰撞排除完全沿用，不以縮小淨空換取密度。
          // 近、中、遠景都採四株群落；舊路徑只群聚中遠景，使正式 God 鏡頭
          // 主要看見的 Near 樹被 maximin 均勻攤成稀疏果園。
          const bool treeChild = kind == ShowcaseLandKind3D::Tree && item % 4u != 0;
          if (treeChild && trial < boundedTrials / 2u) {
            const float childRadius = 5.f + 2.8f * float(trial) /
                                               float(boundedTrials / 2u);
            candidate = {treeCenter.x + radialDirectionX * childRadius, 0,
                         treeCenter.z + radialDirectionZ * childRadius};
            const float radiusSquared = DistanceSquared(candidate, settings.spawnCenter);
            if (radiusSquared < BandMinimum[band] * BandMinimum[band] ||
                radiusSquared > BandMaximum[band] * BandMaximum[band]) {
              ++rejectedCandidates;
              continue;
            }
          }
          const bool grassChild=kind==ShowcaseLandKind3D::Grass && item%12u>=3u;
          if(grassChild){
            const auto center=grassCenters[item%3u];
            // 每個母叢的三個子叢沿同一條緩彎林下帶排列。舊版讓每個子叢
            // 各自使用金角徑向，四個 descriptor 因而讀成近似同心草島。
            // 曲線只由母叢位置、band 與草型決定；候選序列只提供小幅轉角，
            // 因此仍可避開道路、河岸、樹根與其他足跡，且不移動具 4m gate
            // 的母叢中心或增加 descriptor。
            constexpr std::array<float,3> along{{-2.25f,1.65f,3.45f}};
            constexpr std::array<float,3> bow{{.52f,.28f,1.08f}};
            if(pathShoulderCommunity&&trial<boundedTrials*3u/4u){
              const Aevrynd::Vec3 laneEnd{settings.cottageCenter.x, 0,
                                          settings.cottageCenter.z - 5.f};
              const float laneX=laneEnd.x-settings.spawnCenter.x;
              const float laneZ=laneEnd.z-settings.spawnCenter.z;
              const float inverseLaneLength=1.f/std::sqrt(laneX*laneX+laneZ*laneZ);
              const float tangentX=laneX*inverseLaneLength;
              const float tangentZ=laneZ*inverseLaneLength;
              const float normalX=-tangentZ,normalZ=tangentX;
              const float cross=laneX*(center.z-settings.spawnCenter.z)-
                                laneZ*(center.x-settings.spawnCenter.x);
              const float side=cross>=0.f?1.f:-1.f;
              constexpr std::array<float,3> pathAlong{{-1.45f,.95f,2.2f}};
              constexpr std::array<float,3> pathBow{{-.18f,.32f,.68f}};
              const std::uint32_t childIndex=(item%12u)/3u-1u;
              const float jitter=(communityFraction-.5f)*.35f;
              candidate={center.x+tangentX*(pathAlong[childIndex]+jitter)+
                                   normalX*side*(pathBow[childIndex]+std::abs(jitter)*.2f),0,
                         center.z+tangentZ*(pathAlong[childIndex]+jitter)+
                                   normalZ*side*(pathBow[childIndex]+std::abs(jitter)*.2f)};
            }else if(trial<boundedTrials*3u/4u){
              const std::uint32_t childIndex=(item%12u)/3u-1u;
              const std::uint32_t communityIndex=item/12u;
              const float shape=std::fmod(float(communityIndex+1u)*.618033989f+
                  float(item%3u)*.173205081f+float(band)*.291796068f,1.f);
              const float stretch=.90f+shape*.20f;
              const float handedness=((communityIndex+item%3u+band)&1u)?-1.f:1.f;
              const float curvePhase=center.x*.031f+center.z*.017f+
                  float(band)*.43f+float(item%3u)*1.37f+(shape-.5f)*.40f;
              const float trialTurn=(communityFraction-.5f)*2.4f;
              const float reach=.72f+1.13f*float(trial)/float((std::max)(1u,boundedTrials-1u));
              const float tangentX=std::cos(curvePhase+trialTurn);
              const float tangentZ=std::sin(curvePhase+trialTurn);
              const float normalX=-tangentZ,normalZ=tangentX;
              const float alongOffset=along[childIndex]*stretch;
              const float bowOffset=(bow[childIndex]+(shape-.5f)*.25f)*handedness;
              candidate={center.x+(tangentX*alongOffset+normalX*bowOffset)*reach,0,
                         center.z+(tangentZ*alongOffset+normalZ*bowOffset)*reach};
            }else{
              const float childRadius=1.05f+6.f*float(trial)/float(boundedTrials);
              candidate={center.x+radialDirectionX*childRadius,0,
                         center.z+radialDirectionZ*childRadius};
            }
          }
          if (kind == ShowcaseLandKind3D::Rock ||
              zone == ShowcaseLandZone3D::Riverbank) {
            // 河岸項目直接由同一條正式河流函數取樣兩岸，避免先在整個環帶抽樣再
            // 丟棄絕大多數候選；clearance 仍由後續上下限與足跡 gate 驗證。
            // 正式河岸橫斷面由水向陸依序為：保留的濕土水線、低苔蘚／
            // 矮蕨帶、岩石帶。只重排既有 62 草與 30 岩石，不增加配置。
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
          if(kind==ShowcaseLandKind3D::Grass){
            const float r2=DistanceSquared(candidate,settings.spawnCenter);
            const float lo=BandMinimumFor(kind,band);
            const float hi=heroGroundCover?HeroGroundCoverRadius:BandMaximum[band];
            if(r2<lo*lo||r2>hi*hi){++rejectedCandidates;continue;}
            if(zone!=ShowcaseLandZone3D::Riverbank&&item%12u<3u){
              bool separated=true;
              for(const auto center:grassCommunityAnchors[band][ordinal%3u])
                if(DistanceSquared(candidate,center)<16.f){separated=false;break;}
              if(!separated){++rejectedCandidates;continue;}
            }
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
          for(int z=(std::max)(0,cellZ-2);z<=(std::min)(gridSide-1,cellZ+2) &&
              (!settings.exactCandidatePruning || nearest > bestDistance);++z)
          for(int x=(std::max)(0,cellX-2);x<=(std::min)(gridSide-1,cellX+2) &&
              (!settings.exactCandidatePruning || nearest > bestDistance);++x)
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
            // 最小 clearance 只會繼續下降；已不優於現有最佳者不可能贏得 maximin。
            if (settings.exactCandidatePruning && nearest <= bestDistance) break;
          }
          if (nearest > bestDistance) { best = candidate; bestDistance = nearest; found = true; }
          // 子叢與中遠景樹採固定序列第一個安全點；不以 maximin 再把群落攤平。
          if((grassChild || kind==ShowcaseLandKind3D::Tree) && nearest>=0.f)break;
        }
        if (!found || bestDistance < 0.f){failureKind=std::uint32_t(kind);failureBand=band;failureItem=item;return false;}
        placements.push_back({kind, ShowcaseLandBand3D(band), variant, best,
                              scale, UnitRandom(random) * 2.f * Pi,
                              NextRandom(random) | 1ull, zone});
        if(kind==ShowcaseLandKind3D::Grass&&item%12u<3u)grassCenters[item%3u]=best;
        if(kind==ShowcaseLandKind3D::Grass&&zone!=ShowcaseLandZone3D::Riverbank&&
           item%12u<3u)
          grassCommunityAnchors[band][ordinal%3u].push_back(best);
        if(kind==ShowcaseLandKind3D::Tree&&item%4u==0)treeCenter=best;
        if(kind==ShowcaseLandKind3D::Tree&&IsLivingTreeVariant(variant))
          livingTreeAnchors[band].push_back(best);
        const auto [x,z]=cell(best);grid[std::size_t(z*gridSide+x)].push_back(std::uint32_t(placements.size()-1));
      }
    return true;
  };
  // 河岸岩石的可用帶最窄，先保留固定三十顆的位置，避免較密活樹耗盡河岸候選。
  if (!addKind(ShowcaseLandKind3D::Rock, settings.rocks) ||
      !addKind(ShowcaseLandKind3D::Tree, settings.trees) ||
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
  result.walkingLaneClear=result.vegetationScaleBounded=true;
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
    result.walkingLaneClear &= !InWalkingLane(settings,placement.position,footprint);
    if(placement.kind==ShowcaseLandKind3D::Grass)
      result.vegetationScaleBounded &= placement.scale.y<=.44f && placement.scale.x<=1.5f;
    if(placement.kind==ShowcaseLandKind3D::Tree)
      result.vegetationScaleBounded &= placement.scale.y>=4.5f && placement.scale.y<=15.56f;
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
      result.meanRiverbankRockClearance += bankClearance;
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
      result.meanRiverbankGrassClearance += bankClearance;
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
  for (const auto &groundCover : placements) {
    if (groundCover.kind != ShowcaseLandKind3D::Grass ||
        groundCover.zone == ShowcaseLandZone3D::Riverbank)
      continue;
    for (const auto &tree : placements) {
      if (tree.kind != ShowcaseLandKind3D::Tree ||
          !IsLivingTreeVariant(tree.variant) ||
          tree.band != groundCover.band ||
          DistanceSquared(tree.position, groundCover.position) >
              UnderstoryTreeRadius * UnderstoryTreeRadius)
        continue;
      ++result.understoryGroundCover;
      result.understoryGroundCoverBandMask |=
          1u << std::uint32_t(groundCover.band);
      break;
    }
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
  if (result.riverbankRocks)
    result.meanRiverbankRockClearance /= float(result.riverbankRocks);
  if (result.riverbankGrass)
    result.meanRiverbankGrassClearance /= float(result.riverbankGrass);
  // 生態層次由全部實際足跡外緣的平均 clearance 證明；不是只確認兩種
  // 物件都出現，也不以單一離群石頭／苔蘚決定整條河岸。
  result.riverbankLayered &= result.riverbankGrass == 62 &&
      result.riverbankRocks == 30 &&
      result.meanRiverbankGrassClearance + 1.f <=
          result.meanRiverbankRockClearance;
  result.heroGroundCoverExact = result.heroGroundCover >= MinimumHeroGroundCover &&
                                result.heroGroundCoverQuadrantMask == 0x0fu;
  result.understoryGroundCoverExact =
      result.understoryGroundCover >= MinimumUnderstoryGroundCover &&
      result.understoryGroundCoverBandMask == 0x07u;
  result.densityExact = result.occupiedHabitatCells >= 12 &&
      result.densestHabitatCell >= settings.minimumDensestHabitatCell &&
      result.heroGroundCoverExact && result.understoryGroundCoverExact;
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
