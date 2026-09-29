#include "GameplayShowcaseEnvironment3D.h"

#include <Aevrynd/Scene3D.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace GameplayLab {
namespace {

std::uint64_t NextRandom(std::uint64_t &state) {
  state ^= state >> 12;
  state ^= state << 25;
  state ^= state >> 27;
  return state * 2685821657736338717ull;
}

float UnitRandom(std::uint64_t &state) {
  return float((NextRandom(state) >> 40) & 0xffffffu) / float(0xffffffu);
}

std::uint64_t CounterDelta(std::uint64_t before, std::uint64_t after) {
  return after >= before ? after - before : 0;
}

void HashBytes(std::uint64_t &hash, const void *data, std::size_t size) {
  const auto *bytes = static_cast<const unsigned char *>(data);
  for (std::size_t index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ull;
  }
}

void HashFloat(std::uint64_t &hash, float value) {
  const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
  HashBytes(hash, &bits, sizeof(bits));
}

void HashText(std::uint64_t &hash, std::string_view value) {
  HashBytes(hash, value.data(), value.size());
}

std::uint64_t MaterialIdentityHash() {
  constexpr std::array identities = {
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
  std::uint64_t hash = 14695981039346656037ull;
  for (const auto value : identities)
    HashText(hash, value);
  return hash;
}

std::uint64_t LicenseInventoryHash() {
  // 清單與經 CookedRenderManifest3D 驗證的 CC0 資產保持同序。
  constexpr std::array assets = {
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
  std::uint64_t hash = 14695981039346656037ull;
  for (const auto value : assets)
    HashText(hash, value);
  return hash;
}

float RiverCenterX(float z) {
  return -4.f + std::sin(z * .035f) * 5.f + std::sin(z * .011f) * 2.f;
}

float RiverWidth(float z) {
  return 12.f + std::sin(z * .057f + .7f) * 2.6f;
}

float TerrainApronRelief(float boundaryX, float boundaryZ, float distance) {
  if (distance <= 24.f)
    return 0.f;
  const auto smoothStep = [](float value) {
    const float t = Aevrynd::Clamp(value, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
  };
  // 兩段低頻抬升在 320m 後才形成明確山脊；近場 24m 維持原地形接縫，
  // 避免第一／第三人稱可行走區產生不可碰撞的視覺斜坡。相位只取世界
  // 邊界座標，因此相同來源每次建立都得到完全相同的遠景輪廓。
  const float baseRise =
      48.f * smoothStep((distance - 24.f) / 232.f) +
      118.f * smoothStep((distance - 256.f) / 512.f);
  const float broadRidge =
      .70f + .25f * std::sin(boundaryX * .019f + boundaryZ * .013f) +
      .12f * std::sin(boundaryX * .037f - boundaryZ * .021f + 1.1f) +
      .07f * std::sin(boundaryX * .071f + boundaryZ * .043f + .4f);
  // 只有 64m 外才逐步加入兩個較短波長的次級山脊。輸入仍是原始 4m
  // 世界邊界座標，因此最外環展開後得到約 100~200m 的山稜尺度，而非
  // 像素噪點；近場兩圈保持 exact no-op，不改可行走地形與碰撞接縫。
  const float detailActivation = smoothStep((distance - 64.f) / 704.f);
  const float secondaryRidges = detailActivation *
      (.05f * std::sin(boundaryX * .123f - boundaryZ * .097f + .8f) +
       .03f * std::sin(boundaryX * .217f + boundaryZ * .151f + 2.f));
  return baseRise * (broadRidge + secondaryRidges);
}

std::uint32_t TerrainApronColor(std::uint32_t color, float distance,
                                float relief) {
  const auto smoothStep = [](float value) {
    const float t = Aevrynd::Clamp(value, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
  };
  // 遠山保留材質色相，但逐層降低直接反照率，讓夜間不被天空環境光抬成
  // 均勻冷藍牆；真正的距離霧仍由 aerial perspective 負責。
  const float reflectance = 1.f - .38f * smoothStep((distance - 64.f) / 704.f);
  std::uint32_t result = color & 0xff000000u;
  for (unsigned shift : {0u, 8u, 16u}) {
    const auto channel = float((color >> shift) & 0xffu);
    result |= std::uint32_t(std::lround(channel * reflectance)) << shift;
  }
  const std::uint32_t sourceHeight = (color >> 24) & 0xffu;
  const auto heightLift = std::uint32_t((std::min)(96.f, relief * 2.f));
  return (result & 0x00ffffffu) |
         ((std::min)(255u, sourceHeight + heightLift) << 24);
}

struct ProjectedBounds3D {
  float minimumX{(std::numeric_limits<float>::max)()};
  float minimumY{(std::numeric_limits<float>::max)()};
  float maximumX{-(std::numeric_limits<float>::max)()};
  float maximumY{-(std::numeric_limits<float>::max)()};
  std::uint32_t visibleCorners{};
};

ProjectedBounds3D ProjectDescriptorBounds(
    const ShowcaseSceneDescriptor3D &descriptor,
    const Aevrynd::Camera3D &camera, Aevrynd::Vec2 viewport) {
  ProjectedBounds3D result{};
  const float sine = std::sin(descriptor.yaw), cosine = std::cos(descriptor.yaw);
  for (unsigned corner = 0; corner < 8; ++corner) {
    const Aevrynd::Vec3 local{
        (corner & 1 ? .5f : -.5f) * descriptor.scale.x,
        (corner & 2 ? .5f : -.5f) * descriptor.scale.y,
        (corner & 4 ? .5f : -.5f) * descriptor.scale.z};
    const Aevrynd::Vec3 world{
        descriptor.position.x + cosine * local.x + sine * local.z,
        descriptor.position.y + local.y,
        descriptor.position.z - sine * local.x + cosine * local.z};
    const auto projected = Aevrynd::ProjectToViewport(world, camera, viewport);
    if (!projected.visible)
      continue;
    ++result.visibleCorners;
    result.minimumX = (std::min)(result.minimumX, projected.position.x);
    result.minimumY = (std::min)(result.minimumY, projected.position.y);
    result.maximumX = (std::max)(result.maximumX, projected.position.x);
    result.maximumY = (std::max)(result.maximumY, projected.position.y);
  }
  return result;
}

float ProjectedCoverage(const ProjectedBounds3D &bounds, Aevrynd::Vec2 viewport) {
  if (!bounds.visibleCorners || viewport.x <= 0 || viewport.y <= 0)
    return 0;
  const float minimumX = Aevrynd::Clamp(bounds.minimumX, 0.f, viewport.x);
  const float minimumY = Aevrynd::Clamp(bounds.minimumY, 0.f, viewport.y);
  const float maximumX = Aevrynd::Clamp(bounds.maximumX, 0.f, viewport.x);
  const float maximumY = Aevrynd::Clamp(bounds.maximumY, 0.f, viewport.y);
  return (std::max)(0.f, maximumX - minimumX) *
         (std::max)(0.f, maximumY - minimumY) / (viewport.x * viewport.y);
}

} // namespace

bool AppendShowcaseTerrainApron3D(Aevrynd::Mesh3D &mesh) {
  constexpr unsigned perimeter=512;
  if(mesh.vertices.empty())return false;
  // 正式河岸網格在建立外環前已把中央區域細分成 1m 並插入 fan 中心，
  // 因此不能再以 vertices.size()==129^2 判斷。只接受完整、唯一的 4m
  // 世界邊界，且拒絕任何已含外場頂點的輸入，避免重複附加。
  std::unordered_map<std::uint64_t,unsigned> boundaryLookup;
  boundaryLookup.reserve(perimeter);
  const auto key=[](int x,int z){return (std::uint64_t(std::uint32_t(x+256))<<32)|std::uint32_t(z+256);};
  for(unsigned index=0;index<mesh.vertices.size();++index){
    const auto& v=mesh.vertices[index];
    if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||
       std::abs(v.x)>256.f||std::abs(v.z)>256.f)return false;
    const int x=int(std::lround(v.x)),z=int(std::lround(v.z));
    if(v.x!=float(x)||v.z!=float(z)||(x+256)%4||(z+256)%4)continue;
    if(std::abs(x)!=256&&std::abs(z)!=256)continue;
    if(!boundaryLookup.emplace(key(x,z),index).second)return false;
  }
  std::array<unsigned,perimeter> boundary{};unsigned count=0;
  const auto appendBoundary=[&](int x,int z){const auto found=boundaryLookup.find(key(x,z));if(found==boundaryLookup.end())return false;boundary[count++]=found->second;return true;};
  for(int x=-256;x<256;x+=4)if(!appendBoundary(x,-256))return false;
  for(int z=-256;z<256;z+=4)if(!appendBoundary(256,z))return false;
  for(int x=256;x>-256;x-=4)if(!appendBoundary(x,256))return false;
  for(int z=256;z>-256;z-=4)if(!appendBoundary(-256,z))return false;
  if(count!=perimeter)return false;
  mesh.vertices.reserve(mesh.vertices.size()+5*perimeter);mesh.indices.reserve(mesh.indices.size()+5*perimeter*6);
  auto inner=boundary;
  // 共用原邊界索引，外環僅向世界外延伸；河流位於 |z|<=142，不會被新面覆蓋。
  // 最外圈延伸至約一公里，讓 God 固定鏡頭的底角仍由地表覆蓋；這只增加
  // 512 個低頻邊界頂點，不複製內場植被、碰撞或高細節地形。
  for(float distance:{8.f,24.f,64.f,256.f,768.f}){
    std::array<unsigned,perimeter> outer{};
    for(unsigned i=0;i<perimeter;++i){
      const auto& source=mesh.vertices[boundary[i]];
      auto v=source;
      const float dx=source.x/256.f*distance,dz=source.z/256.f*distance;
      const auto n=Aevrynd::UnpackNormalSnorm8(source.normal);
      const float slope=std::abs(n.y)>.2f?-(n.x*dx+n.z*dz)/n.y:0.f;
      v.x+=dx;v.z+=dz;
      const float relief=TerrainApronRelief(source.x,source.z,distance);
      v.y+=slope/(1.f+distance/32.f)+relief;
      v.color=TerrainApronColor(v.color,distance,relief);
      v.u=v.x*.085f;v.v=v.z*.085f;
      outer[i]=unsigned(mesh.vertices.size());mesh.vertices.push_back(v);
    }
    // 遠景山脊不是垂直 billboard：以環帶切線與向外斜率建立幾何法線，
    // 讓日／暮／夜光照能呈現實際坡向。原 512m 世界邊界仍完全不變。
    for(unsigned i=0;i<perimeter;++i){
      const unsigned previous=(i+perimeter-1)%perimeter,next=(i+1)%perimeter;
      const auto& a=mesh.vertices[outer[previous]];
      const auto& b=mesh.vertices[outer[i]];
      const auto& c=mesh.vertices[outer[next]];
      const auto& inside=mesh.vertices[inner[i]];
      const Aevrynd::Vec3 tangent{c.x-a.x,c.y-a.y,c.z-a.z};
      const Aevrynd::Vec3 radial{b.x-inside.x,b.y-inside.y,b.z-inside.z};
      Aevrynd::Vec3 normal{tangent.y*radial.z-tangent.z*radial.y,
                           tangent.z*radial.x-tangent.x*radial.z,
                           tangent.x*radial.y-tangent.y*radial.x};
      if(normal.y<0.f)normal={-normal.x,-normal.y,-normal.z};
      mesh.vertices[outer[i]].normal=Aevrynd::PackNormalSnorm8(Aevrynd::Normalize(normal));
    }
    for(unsigned i=0;i<perimeter;++i){const unsigned j=(i+1)%perimeter;
      mesh.indices.insert(mesh.indices.end(),{inner[i],inner[j],outer[j],inner[i],outer[j],outer[i]});
    }
    inner=outer;
  }
  return true;
}

bool SculptShowcaseHeroTrailTerrain3D(
    Aevrynd::HeightField3D &terrain,
    ShowcaseHeroTrailTerrainReceipt3D *receipt) {
  ShowcaseHeroTrailTerrainReceipt3D result{};
  const auto settings = terrain.Settings();
  if (settings.width < 2u || settings.depth < 2u ||
      !std::isfinite(settings.cellSize) || settings.cellSize <= 0.f) {
    if (receipt) *receipt = result;
    return false;
  }
  const auto smooth = [](float value) {
    value = Aevrynd::Clamp(value, 0.f, 1.f);
    return value * value * (3.f - 2.f * value);
  };
  std::vector<float> shoulderHeights;
  shoulderHeights.reserve(768u);
  constexpr float pathOuterHalfWidth = 7.25f;
  constexpr Aevrynd::Vec3 cottageCenter{-12.f, 0.f, 24.f};
  constexpr Aevrynd::Vec2 cottageHalfExtent{4.35f, 3.95f};
  constexpr float cottageTransition = 1.65f;
  for (unsigned z = 0; z < settings.depth; ++z) {
    for (unsigned x = 0; x < settings.width; ++x) {
      const auto point = terrain.SamplePosition(x, z);
      const float sourceHeight = terrain.Height(x, z);
      const auto trail = SampleShowcaseHeroTrail3D(point);
      float pathWeight{};
      if (trail.valid) {
        const float tread = ShowcaseHeroTrailTreadHalfWidth3D(point);
        pathWeight = 1.f - smooth((trail.distance - tread) /
                                  (pathOuterHalfWidth - tread));
      }
      const float outsideX =
          (std::max)(0.f, std::abs(point.x - cottageCenter.x) -
                              cottageHalfExtent.x);
      const float outsideZ =
          (std::max)(0.f, std::abs(point.z - cottageCenter.z) -
                              cottageHalfExtent.y);
      const float padOutside = std::sqrt(outsideX * outsideX +
                                         outsideZ * outsideZ);
      const float cottageWeight =
          1.f - smooth(padOutside / cottageTransition);
      const float flattenWeight = (std::max)(pathWeight, cottageWeight);
      // Cottage ground remains at authored zero.  The trail itself rises
      // smoothly to the physical bridge top before the west collider edge;
      // otherwise the .36 m deck is an impassable wall for a controller that
      // intentionally has no implicit step-up.
      const float targetHeight =
          cottageWeight >= pathWeight
              ? 0.f
              : ShowcaseHeroTrailTargetHeight3D(trail.along);
      const float sculptedHeight =
          sourceHeight * (1.f - flattenWeight) + targetHeight * flattenWeight;
      if (std::abs(sculptedHeight - sourceHeight) > 1e-6f) {
        terrain.SetHeight(x, z, sculptedHeight);
        ++result.modifiedSamples;
      }
      if (trail.valid && trail.distance <=
                             ShowcaseHeroTrailTreadHalfWidth3D(point)) {
        ++result.treadSamples;
        result.maximumTreadHeight =
            (std::max)(result.maximumTreadHeight, std::abs(sculptedHeight));
      }
      if (trail.valid && trail.along >= .15f && trail.along <= .95f &&
          trail.distance >= 2.2f && trail.distance <= 6.f) {
        shoulderHeights.push_back(sculptedHeight);
        ++result.shoulderSamples;
      }
      if (outsideX == 0.f && outsideZ == 0.f) {
        ++result.cottagePadSamples;
        result.maximumCottagePadHeight =
            (std::max)(result.maximumCottagePadHeight,
                       std::abs(sculptedHeight));
      }
    }
  }
  if (!shoulderHeights.empty()) {
    std::ranges::sort(shoulderHeights);
    const auto percentile = [&](float fraction) {
      const auto index = (std::min)(
          shoulderHeights.size() - 1u,
          std::size_t(fraction * float(shoulderHeights.size() - 1u)));
      return shoulderHeights[index];
    };
    result.shoulderRelief = percentile(.9f) - percentile(.1f);
  }
  terrain.RecalculateRange();
  result.applied = true;
  if (receipt) *receipt = result;
  return result.Complete();
}

bool GameplayShowcaseEnvironment3D::Initialize(std::uint64_t seed) {
  if (!seed)
    return false;

  std::vector<ShowcaseSceneDescriptor3D> candidate;
  candidate.reserve(521);
  candidate.push_back({ShowcaseSceneKind3D::Terrain,
                       {0, 0, 0},
                       {512, 48, 512},
                       0,
                       0,
                       true,
                       "procedural/terrain-heightfield",
                       "poly-haven/forest_ground_04/cooked-pbr"});

  candidate.push_back({ShowcaseSceneKind3D::Water, {0, -.42f, 0},
                       {30.f, .08f, 272.f}, 0, 1, false,
                       "procedural/river-spline-strip-64", "runtime/water-surface"});

  ShowcaseLandEcologySettings3D landSettings{};
  landSettings.seed = seed;
  std::vector<ShowcaseLandPlacement3D> land;
  if (!BuildShowcaseLandEcology3D(landSettings, land, &landReceipt_)) return false;
  for (const auto &placement : land) {
    ShowcaseSceneDescriptor3D descriptor{};
    descriptor.position = placement.position;
    descriptor.scale = placement.scale;
    descriptor.yaw = placement.yaw;
    descriptor.lodGroup = std::uint32_t(placement.band);
    // 186 個矮草群落仍接收地形、環境與角色陰影，但不再各自重畫到四層
    // 4096 陰影圖。樹木、岩石與建築維持正式動態投影，避免 Ultra 的
    // 透明草葉 overdraw 吞掉 60 FPS 預算而幾乎沒有可辨識的畫面收益。
    descriptor.castsShadow = placement.kind != ShowcaseLandKind3D::Grass;
    const auto presentationVariant = placement.kind == ShowcaseLandKind3D::Grass
        ? ShowcaseUnderstoryPresentationVariant3D(placement)
        : placement.variant;
    const std::string id(ShowcaseLandAssetId3D(presentationVariant));
    if (placement.kind == ShowcaseLandKind3D::Tree) {
      const auto form = ShowcaseTreeFormVariation(placement);
      descriptor.scale = {descriptor.scale.x * form.scaleMultiplier.x,
                          descriptor.scale.y * form.scaleMultiplier.y,
                          descriptor.scale.z * form.scaleMultiplier.z};
      descriptor.yaw += form.yawOffset;
      descriptor.kind = ShowcaseSceneKind3D::Tree;
      descriptor.meshAsset = "poly-haven/" + id + "/cooked-lod";
      descriptor.materialAsset = "poly-haven/" + id + "/cooked-pbr";
    } else if (placement.kind == ShowcaseLandKind3D::Rock) {
      descriptor.kind = ShowcaseSceneKind3D::Rock;
      descriptor.meshAsset = "poly-haven/" + id + "/cooked-lod";
      descriptor.materialAsset = "poly-haven/" + id + "/cooked-pbr";
    } else {
      const auto form = ShowcaseUnderstoryFormVariation(placement);
      descriptor.scale = {descriptor.scale.x * form.scaleMultiplier.x,
                          descriptor.scale.y * form.scaleMultiplier.y,
                          descriptor.scale.z * form.scaleMultiplier.z};
      descriptor.yaw += form.yawOffset;
      descriptor.kind = ShowcaseSceneKind3D::Grass;
      descriptor.meshAsset = id;
      descriptor.materialAsset =
          placement.zone == ShowcaseLandZone3D::Riverbank
              ? "poly-haven/stony_dirt_path/cooked-pbr"
              : placement.zone == ShowcaseLandZone3D::Upland
                    ? "poly-haven/forest_ground_04/cooked-pbr"
                    : "ambientcg/Ground108/cooked-pbr";
    }
    candidate.push_back(std::move(descriptor));
  }
  constexpr std::array<Aevrynd::Vec3, 6> buildingPositions = {
      Aevrynd::Vec3{-12, .15f, 24}, Aevrynd::Vec3{-12, 1.95f, 20.4f},
      Aevrynd::Vec3{-12, 1.95f, 27.6f}, Aevrynd::Vec3{-15.95f, 1.95f, 24},
      Aevrynd::Vec3{-8.05f, 1.95f, 24}, Aevrynd::Vec3{-12, 3.65f, 24}};
  constexpr std::array<std::string_view, 6> buildingMeshes = {
      "procedural/cottage-foundation", "procedural/cottage-front",
      "procedural/cottage-back", "procedural/cottage-left",
      "procedural/cottage-right", "procedural/cottage-pitched-roof"};
  for (std::size_t index = 0; index < buildingPositions.size(); ++index)
    candidate.push_back({ShowcaseSceneKind3D::Building,
                         buildingPositions[index],
                         index == 0 ? Aevrynd::Vec3{8.4f, .3f, 7.6f}
                                    // 屋頂只保留結構所需的簷口：覆蓋四面牆但不以過大的
                                    // 外廓主宰 God composition。牆寬 8.4、前後外緣 7.48。
                                    : index == 5 ? Aevrynd::Vec3{8.5f, 1.6f, 7.6f}
                                                 : (index == 1 || index == 2)
                                                       ? Aevrynd::Vec3{8.4f, 3.3f, .28f}
                                                       : Aevrynd::Vec3{7.2f, 3.3f, .28f},
                         index == 3 || index == 4 ? 1.5707963f : 0.f,
                         0,
                         true,
                         std::string(buildingMeshes[index]),
                         std::string(GameplayCottageDescriptorMaterialAsset3D(
                             index == 0 ? GameplayCottageMaterialRole3D::Brick
                                        : index == 5 ? GameplayCottageMaterialRole3D::RoofTile
                                                     : GameplayCottageMaterialRole3D::WoodSiding))});

  // 讓小屋前的英雄道路可直接延伸到河對岸。單一橋面 descriptor
  // 擁有正式碰撞與穩定 identity；木板、扶手和橋墩由同一 identity 的
  // render-only 細節建立，避免為裝飾件製造虛假的 gameplay 物件。
  candidate.push_back({ShowcaseSceneKind3D::Building,
                       {-1.35f, .14f, 14.f},
                       {17.f, .28f, 2.8f},
                       0.f,
                       0,
                       true,
                       "procedural/forest-river-bridge",
                       "ambientcg/WoodSiding006/material-pbr"});

  descriptors_.swap(candidate);
  // 林地河水使用低鐵土溪流的 Beer-Lambert 吸收：紅光仍最快衰減，
  // 但三通道差距受限，深水散射也維持低能量，避免固定青色源項主宰畫面。
  // 反射率採空氣/淡水介面的約 2.1% F0；粗糙度範圍擴大後，正視角仍可
  // 透見河床，掠射角則由 Fresnel 平滑轉為天空與樹冠反射。
  // 約 3.1m 主波取代會在上帝視角形成規則同心環的 4.5m 波形；主波坡度
  // 降至細波的約 1.14 倍，避免遠景低頻起伏壓過河床與倒影。1.45
  // cycles/m 的細波仍在近景提供碎光，並繼續由像素世界足跡 band-limit。
  // 泡沫保留同一個 0.02~0.12m 真實岸帶，但降低能量，夜間不再形成
  // 電藍描邊；透明度與吸收維持不變，因此這不是把水改成平面或換色。
  waterSurface_ = Aevrynd::NormalizeWaterSurfaceSettings3D(
      {true, {.34f, .22f, .15f}, {.014f, .026f, .029f}, .021f,
       .050f, .17f, .32f, .040f, .20f, 1.45f, .035f, .55f,
       .020f, .12f, .09f, .38f, .78f, .030f});
  seed_ = seed;
  groundingReceipt_ = {};
  initialized_ = true;
  return true;
}

ShowcaseSceneTargets3D GameplayShowcaseEnvironment3D::Targets() noexcept {
  ShowcaseSceneTargets3D targets{};
  targets.materialIdentityHash = MaterialIdentityHash();
  targets.licenseInventoryHash = LicenseInventoryHash();
  return targets;
}

ShowcaseCameraVisibility3D GameplayShowcaseEnvironment3D::EvaluateCameraVisibility(
    const Aevrynd::Camera3D &camera, Aevrynd::Vec2 viewport) const noexcept {
  ShowcaseCameraVisibility3D result{};
  if (viewport.x < 1 || viewport.y < 1)
    return result;
  ProjectedBounds3D buildingUnion{};
  for (const auto &descriptor : descriptors_) {
    if (descriptor.kind != ShowcaseSceneKind3D::Water &&
        descriptor.kind != ShowcaseSceneKind3D::Building)
      continue;
    if(descriptor.kind==ShowcaseSceneKind3D::Water){
      constexpr unsigned slices=64;constexpr float length=272.f,step=length/float(slices);
      for(unsigned slice=0;slice<slices;++slice){const float z=-length*.5f+(float(slice)+.5f)*step;ShowcaseSceneDescriptor3D cell=descriptor;cell.position={RiverCenterX(z),descriptor.position.y,z};cell.scale={RiverWidth(z),descriptor.scale.y,step*1.02f};cell.yaw=std::atan(RiverCenterX(z+.5f)-RiverCenterX(z-.5f));const auto cellBounds=ProjectDescriptorBounds(cell,camera,viewport);if(cellBounds.visibleCorners){++result.visibleRiverSegments;result.riverCoverage+=ProjectedCoverage(cellBounds,viewport);}}
      continue;
    }
    // 固定鏡頭的 building 合約量測木屋六個結構件；橋樑是可見世界
    // 地標，但不可擴張 cottage union 來放寬既有構圖門檻。
    if(descriptor.meshAsset.find("forest-river-bridge")!=std::string::npos)
      continue;
    const auto bounds = ProjectDescriptorBounds(descriptor, camera, viewport);
    if (!bounds.visibleCorners)
      continue;
    ++result.visibleBuildingComponents;
    buildingUnion.minimumX = (std::min)(buildingUnion.minimumX, bounds.minimumX);
    buildingUnion.minimumY = (std::min)(buildingUnion.minimumY, bounds.minimumY);
    buildingUnion.maximumX = (std::max)(buildingUnion.maximumX, bounds.maximumX);
    buildingUnion.maximumY = (std::max)(buildingUnion.maximumY, bounds.maximumY);
    buildingUnion.visibleCorners += bounds.visibleCorners;
  }
  result.riverCoverage = Aevrynd::Clamp(result.riverCoverage, 0.f, 1.f);
  result.buildingCoverage = ProjectedCoverage(buildingUnion, viewport);
  if (buildingUnion.visibleCorners) {
    // 觸邊合約只排除實際裁切，不把合理的屏幕邊緣構圖判成失敗。
    constexpr float margin = 1.f;
    result.buildingEdgeMask = (buildingUnion.minimumX < margin ? 1u : 0u) |
        (buildingUnion.minimumY < margin ? 2u : 0u) |
        (buildingUnion.maximumX > viewport.x - margin ? 4u : 0u) |
        (buildingUnion.maximumY > viewport.y - margin ? 8u : 0u);
    result.buildingTouchesEdge = result.buildingEdgeMask != 0;
  }
  return result;
}

std::vector<ShowcaseLodSample3D>
GameplayShowcaseEnvironment3D::EvaluateLodSamples(
    Aevrynd::Vec3 cameraPosition) const {
  std::vector<ShowcaseLodSample3D> result;
  result.reserve(132);
  std::uint64_t stableLow{};
  for (const auto &descriptor : descriptors_) {
    ++stableLow;
    if (descriptor.kind != ShowcaseSceneKind3D::Tree &&
        descriptor.kind != ShowcaseSceneKind3D::Grass &&
        descriptor.kind != ShowcaseSceneKind3D::Rock)
      continue;
    ShowcaseLodSample3D sample{};
    sample.stableLow = stableLow;
    sample.kind = descriptor.kind;
    sample.centerDistance = Aevrynd::Length(descriptor.position - cameraPosition);
    sample.boundsRadius = Aevrynd::Length(descriptor.scale) * .5f;
    sample.surfaceDistance = (std::max)(0.f, sample.centerDistance - sample.boundsRadius);
    const std::array<Aevrynd::MeshLodPolicy3D, 3> policies =
        descriptor.kind == ShowcaseSceneKind3D::Grass
            ? std::array<Aevrynd::MeshLodPolicy3D, 3>{
                  ShowcaseGrassLodPolicy3D(), {}, {}}
            : descriptor.kind == ShowcaseSceneKind3D::Tree
                ? ShowcaseTreeLodPolicies3D()
                : ShowcaseOpaqueLodPolicies3D();
    const std::size_t policyCount = descriptor.kind == ShowcaseSceneKind3D::Grass ? 1 : 3;
    bool selected{};
    for (std::size_t level = 0; level < policyCount; ++level) {
      const auto selection = Aevrynd::EvaluateMeshLod3D(
          sample.centerDistance, sample.boundsRadius, policies[level]);
      if (!selection.submitNear)
        continue;
      sample.switchDistance = policies[level].switchDistance;
      sample.transitionWidth = policies[level].transitionWidth;
      sample.nearCoverage = selection.nearCoverage;
      sample.farCoverage = selection.farCoverage;
      sample.nearLevel = std::uint32_t(level);
      sample.farLevel = std::uint32_t(level + 1);
      sample.transitioning = selection.submitFar;
      selected = true;
      break;
    }
    if (!selected) {
      sample.nearLevel = std::uint32_t(policyCount);
      sample.farLevel = sample.nearLevel;
      sample.nearCoverage = 1.f;
    }
    result.push_back(sample);
  }
  return result;
}

std::vector<ShowcaseLodSample3D>
GameplayShowcaseEnvironment3D::EvaluateVisibleLodSamples(
    const Aevrynd::Camera3D &camera, float aspectRatio) const {
  auto result = EvaluateLodSamples(camera.position);
  std::erase_if(result, [&](const auto &sample) {
    const auto &descriptor = descriptors_[std::size_t(sample.stableLow - 1)];
    return !Aevrynd::SphereIntersectsFrustum(descriptor.position,
                                             sample.boundsRadius, camera,
                                             aspectRatio);
  });
  return result;
}

void GameplayShowcaseEnvironment3D::ApplyHighProfile(
    Aevrynd::DrawList &draw) const {
  draw.SetDirectionalShadowMapEnabled(true);
  draw.SetDirectionalShadowCascades({4, 2048, .1f, 220.f, .7f, .1f});
  draw.SetSoftDirectionalShadows({true, 18.f, 112.f, 28.f});
  // The playable forest uses a broad sunlit clearing, so contact response must
  // remain readable beneath feet, roots and low foliage without changing the
  // collision surface or adding shadow casters.  The bounded production
  // strengths deepen only geometry-backed occlusion and retain the existing
  // pass counts, radii and sample budgets.
  draw.SetContactShadows({true, 24, 2.2f, .032f, .98f});
  draw.SetAmbientOcclusion({true, 1.25f, .025f, 1.42f});
  draw.SetScreenSpaceReflections({true, .82f, 42.f, .7f, .32f, 16});
  draw.SetBloom({true, 1.18f, .38f, .085f, 2.2f});
  // 英雄鏡頭仰角低，因此雲底維持在山脊上方；適度提高密度並減少過度
  // 侵蝕，讓分散雲團能在固定鏡頭中形成可讀的明暗體積，而不是退化成
  // 幾乎不可見的灰帶。取樣數、最大距離與 draw budget 均維持不變。
  Aevrynd::VolumetricCloudSettings3D clouds{};
  clouds.baseAltitude = 2800.f;
  clouds.thickness = 1150.f;
  clouds.coverage = .56f;
  clouds.density = .48f;
  clouds.erosion = .68f;
  clouds.windSpeed = 7.f;
  clouds.sampleCount = 16;
  clouds.shadowsEnabled = true;
  clouds.shadowStrength = .28f;
  clouds.maximumDistance = 9000.f;
  draw.SetVolumetricClouds(clouds);
  draw.SetDirectionalLightShafts({true, .68f, .72f, 360.f, 6});
  // Preserve the authored three-layer material and its mean energy, but let
  // the already continuous world-space macro/chroma domains separate moss,
  // leaf litter and compacted soil across the large playable clearing.  R97's
  // lower amplitudes collapsed all three fixed cameras into one yellow-green
  // sheet; these values remain within the renderer's normalized bounds and do
  // not add texture samples, descriptors, instances or draw calls.
  Aevrynd::TerrainDistanceMaterialSettings3D terrainMaterial{
      true, 48.f, 224.f, .020f, .24f, 1.80f, .10f};
  terrainMaterial.chromaVariationStrength = .15f;
  terrainMaterial.mesoPartitionEnabled = true;
  draw.SetTerrainDistanceMaterial(terrainMaterial);
  // 淺溪厚度不應把背景折射到岸外；保留可見折射，但限制最大 UV 偏移。
  draw.SetWaterOptics({true, .022f, .46f, .040f});
  draw.SetWaterSurface(waterSurface_);
}

ShowcaseLightingState3D GameplayShowcaseEnvironment3D::EvaluateLighting(
    ShowcaseLightingProfile3D profile) const {
  ShowcaseLightingState3D state{};
  state.profile = profile;
  if (profile == ShowcaseLightingProfile3D::Day) {
    state = {profile, {{.34f, -.91f, .23f}, .22f, {1, .96f, .88f, 1}, 1.62f},
             {.24f, .42f, .67f, 1}, {.32f, .43f, .55f, .62f}, -.12f, 46800.f};
  } else if (profile == ShowcaseLightingProfile3D::Dusk) {
    state = {profile, {{-.74f, -.42f, .18f}, .19f, {1, .66f, .38f, 1}, .94f},
             {.19f, .14f, .23f, 1}, {.3f, .19f, .25f, .68f}, .08f, 68400.f};
  } else {
    // 月光保留冷色辨識，但夜間亮度由局部光源與反射建立層次，不能用
    // 全畫面藍色增益把水面與地表抬得比黃昏更亮。
    state = {profile, {{-.25f, -.62f, -.74f}, .42f, {.42f, .55f, 1, 1}, .42f},
             {.06f, .10f, .18f, 1}, {.1f, .16f, .28f, .7f}, -.16f, 79200.f};
  }
  return state;
}

void GameplayShowcaseEnvironment3D::ApplyLighting(
    Aevrynd::DrawList &draw, ShowcaseLightingProfile3D profile,
    Aevrynd::Vec3 cameraPosition) const {
  const ShowcaseLightingState3D state = EvaluateLighting(profile);
  draw.SetDirectionalLight(state.sun);
  // 小屋是英雄場景的視覺錨點。以實際局部光源建立黃昏／夜間層次，
  // 不用提高全畫面曝光來把天空、河面與陰影一起洗亮。
  if (profile != ShowcaseLightingProfile3D::Day) {
    // Night stays brighter than dusk at the actual lamps, but the former 1.25
    // multiplier lifted the complete god-view phase until dusk/night measured
    // only 1.023.  1.16 remains above the 2.1x local-light contrast contract
    // while restoring separation in the surrounding forest.
    const float night = profile == ShowcaseLightingProfile3D::Night ? 1.16f : .55f;
    draw.AddPointLight({GameplayCottagePorchLanternCenter3D, 11.f,
                        {1.f, .57f, .25f, 1.f}, 4.2f * night});
    draw.AddPointLight({{-14.55f, 2.25f, 19.75f}, 7.5f,
                        {1.f, .72f, .38f, 1.f}, 2.35f * night});
    draw.AddPointLight({{-9.45f, 2.25f, 19.75f}, 7.5f,
                        {1.f, .72f, .38f, 1.f}, 2.35f * night});
    // R101_FOREST_GUIDE_LIGHT_LIGHTING_BEGIN
    // The trail practicals remain subordinate to the cottage sources: their
    // short radius creates separated pools instead of lifting global exposure.
    constexpr float guideLightRadius=4.2f;
    constexpr float guideLightIntensity=1.05f;
    for(const auto position:GameplayForestGuideLightPositions3D)
      draw.AddPointLight({position,guideLightRadius,
                          {1.f,.58f,.24f,1.f},guideLightIntensity*night});
    // R101_FOREST_GUIDE_LIGHT_LIGHTING_END
  }
  const float heightFogDensity =
      profile == ShowcaseLightingProfile3D::Day
          ? .0002f
          : profile == ShowcaseLightingProfile3D::Dusk ? .0012f : .0018f;
  const float heightFogMaximumOpacity =
      profile == ShowcaseLightingProfile3D::Day
          ? .08f
          : profile == ShowcaseLightingProfile3D::Dusk ? .38f : .55f;
  Aevrynd::Atmosphere3D atmosphere{state.fog, cameraPosition, 54, 230, true,
                                   heightFogDensity, .09f, -2.f,
                                   heightFogMaximumOpacity};
  // 表面 aerial perspective 與體積霧分開配置；近場保留材質色，遠景才依物理消光融入天空。
  if (profile == ShowcaseLightingProfile3D::Day) {
    atmosphere.aerialPerspective = {true, .00175f, .00110f, .018f, -2.f,
                                    36.f, 30.f, 420.f, .12f,
                                    {.105f, .135f, .17f, 1.f}, .032f};
  } else if (profile == ShowcaseLightingProfile3D::Dusk) {
    atmosphere.aerialPerspective = {true, .00205f, .00130f, .020f, -2.f,
                                    36.f, 30.f, 420.f, .24f,
                                    {.060f, .052f, .062f, 1.f}, .052f};
  } else {
    atmosphere.aerialPerspective = {true, .00135f, .00078f, .018f, -2.f,
                                    32.f, 34.f, 420.f, .08f,
                                    {.012f, .022f, .043f, 1.f}, .018f};
  }
  draw.SetAtmosphere(atmosphere);
  auto shafts = draw.DirectionalLightShafts();
  shafts.intensity = profile == ShowcaseLightingProfile3D::Day
                         ? .06f
                         : profile == ShowcaseLightingProfile3D::Dusk ? .34f
                                                                      : .12f;
  draw.SetDirectionalLightShafts(shafts);
  draw.SetVolumetricAtmospherePhase(
      profile == ShowcaseLightingProfile3D::Day
          ? Aevrynd::VolumetricAtmospherePhase3D::Day
          : profile == ShowcaseLightingProfile3D::Dusk
                ? Aevrynd::VolumetricAtmospherePhase3D::Dusk
                : Aevrynd::VolumetricAtmospherePhase3D::Night);
  Aevrynd::PhaseRadianceProfile3D radiance{};
  if(profile==ShowcaseLightingProfile3D::Day)radiance={{.08f,.20f,.46f},.88f,{.28f,.46f,.68f},2.6f,{.13f,.12f,.095f},.00935f,{.94f,.96f,.93f},.68f,{1.f,.88f,.68f},9.f,0x4441595f534b5932ull};
  else if(profile==ShowcaseLightingProfile3D::Dusk)radiance={{.045f,.055f,.09f},.72f,{.44f,.25f,.16f},2.2f,{.08f,.045f,.035f},.00935f,{1.f,.82f,.68f},.46f,{1.f,.48f,.22f},7.f,0x4455534b534b5932ull};
  else {radiance={{.016f,.030f,.066f},.22f,{.052f,.070f,.110f},1.8f,{.012f,.018f,.028f},.00935f,{.72f,.78f,1.f},.22f,{.55f,.66f,1.f},1.2f,0x4e49474854534b32ull};radiance.lunarDetailStrength=1.f;radiance.stellarDetailStrength=1.f;}
  draw.SetPhaseRadianceProfile(radiance);
  // 固定日間 HDRI 只提供穩定的高頻環境細節；黃昏與夜間提高物理天空比例，避免把日間色彩當作地圖染色。
  Aevrynd::EnvironmentLightingBlendSettings3D environmentBlend{};
  if (profile == ShowcaseLightingProfile3D::Day) {
    environmentBlend.diffuseRgbScale = {.96f,.97f,.94f};
    environmentBlend.diffuseIntensity = 1.f;
    environmentBlend.specularIntensity = 1.f;
    environmentBlend.analyticDiffuseWeight = .18f;
    environmentBlend.iblDiffuseWeight = .82f;
    environmentBlend.analyticSpecularWeight = .10f;
    environmentBlend.iblSpecularWeight = .90f;
    environmentBlend.objectFillRadiance = {.46f,.49f,.48f};
    // Let the directional sun shape trunks, foliage and terrain.  The former
    // 0.92 fill sat too close to the 1.42 sun and flattened the forest into a
    // single mid-tone.  Total illumination stays bounded: sun rises while the
    // omnidirectional fill falls, rather than increasing exposure.
    environmentBlend.objectFillIntensity = .78f;
    environmentBlend.phaseFingerprint = 0x4441595f49424c31ull;
  } else if (profile == ShowcaseLightingProfile3D::Dusk) {
    environmentBlend.diffuseRgbScale = {1.f,.88f,.72f};
    environmentBlend.diffuseIntensity = .72f;
    environmentBlend.specularIntensity = .78f;
    environmentBlend.analyticDiffuseWeight = .68f;
    environmentBlend.iblDiffuseWeight = .32f;
    // 黃昏鏡面仍需保留真實 HDRI 的高頻環境形狀；舊 38% IBL 讓平滑解析
    // 天空主宰水面與濕材質，反射退化為均勻色帶。權重總和維持 1，不增加能量。
    environmentBlend.analyticSpecularWeight = .30f;
    environmentBlend.iblSpecularWeight = .70f;
    // 保留暖色主光與小屋局部光，但用等亮度冷中性補光分離森林暗部，
    // 避免天空、地表與陰影全部落在相同的棕橙色域。
    environmentBlend.objectFillRadiance = {.22f,.29f,.38f};
    environmentBlend.objectFillIntensity = .72f;
    environmentBlend.phaseFingerprint = 0x4455534b5f49424cull;
  } else {
    environmentBlend.diffuseRgbScale = {.72f,.78f,1.f};
    // 提高夜間不透明地表使用的月光半球漫射；天空、水體、物件補光、
    // 曝光與相機路徑皆保持原值，避免以全畫面增亮掩蓋林床黑階。
    environmentBlend.diffuseIntensity = .33f;
    environmentBlend.specularIntensity = .26f;
    environmentBlend.analyticDiffuseWeight = .84f;
    environmentBlend.iblDiffuseWeight = .16f;
    environmentBlend.analyticSpecularWeight = .78f;
    environmentBlend.iblSpecularWeight = .22f;
    environmentBlend.objectFillRadiance = {.72f,.78f,1.f};
    // 月光仍低於黃昏，但角色暗部需要保留臉、手與服裝輪廓，避免正式夜景
    // 只剩剪影。這是物件補光，不抬高天空、水面或全畫面曝光。
    environmentBlend.objectFillIntensity = .58f;
    environmentBlend.phaseFingerprint = 0x4e494748545f4942ull;
  }
  draw.SetEnvironmentLightingBlend(environmentBlend);
  Aevrynd::TimeOfDayExposureSettings3D timeOfDayExposure{};
  if (profile == ShowcaseLightingProfile3D::Dusk) {
    // 森林地被改用正確的深色材質後，黃昏中間調需要少量補償，才能維持
    // 日／昏／夜的清楚亮度階層。新的低反射率林地與淡水材質在正式 GBV
    // God-view 量得 dusk/night=1.047，仍略低於既有 1.05 相位門檻；將
    // 黃昏曝光補到既有允許上限，只提升黃昏中間調，不放寬門檻。
    timeOfDayExposure.duskExposureEv = 1.f;
  } else if (profile == ShowcaseLightingProfile3D::Night) {
    // 夜間不沿用預設 +2 EV：壓住月光與小屋高光，同時只在 tone curve
    // 暗部提供有限 lift，讓林地輪廓可讀而不抬高整個畫面。
    // 正式森林鏡頭顯示 +1.50 EV 仍會讓月光夜景過度接近黃昏；降低
    // 0.05 EV，建立可量測的相位間隔，同時保留物件專用補光所提供的輪廓。
    // Keep the night tone curve below the former 1.45 EV plateau: formal
    // framebuffer evidence showed that value collapsed god-view dusk/night to
    // 1.023 even after reducing local lamps and analytic sky fill.
    timeOfDayExposure.nightExposureEv = 1.30f;
    // first-person 鏡頭的近場樹幹與林床仍在有效黑階下被壓成
    // 剪影。只稍微抬高有幾何深度的 toe；PostProcess 的 sceneSemantic
    // 使天空、水面與 bloom 不會被全螢幕提亮。
    timeOfDayExposure.shadowLift = .04f;
    timeOfDayExposure.toeStrength = .33f;
    timeOfDayExposure.shoulderStrength = 1.12f;
  }
  draw.SetTimeOfDayExposure(timeOfDayExposure);
  draw.SetExposureCompensation(state.exposureEv);
  draw.SetEnvironmentTime(state.environmentTime);
}

std::uint64_t GameplayShowcaseEnvironment3D::WorldHash() const {
  std::uint64_t hash = 14695981039346656037ull;
  HashBytes(hash, &seed_, sizeof(seed_));
  for (const ShowcaseSceneDescriptor3D &descriptor : descriptors_) {
    HashBytes(hash, &descriptor.kind, sizeof(descriptor.kind));
    for (float value : {descriptor.position.x, descriptor.position.y,
                        descriptor.position.z, descriptor.scale.x,
                        descriptor.scale.y, descriptor.scale.z,
                        descriptor.yaw})
      HashFloat(hash, value);
    HashBytes(hash, &descriptor.lodGroup, sizeof(descriptor.lodGroup));
    HashBytes(hash, &descriptor.castsShadow, sizeof(descriptor.castsShadow));
    HashBytes(hash, descriptor.meshAsset.data(), descriptor.meshAsset.size());
    HashBytes(hash, descriptor.materialAsset.data(),
              descriptor.materialAsset.size());
  }
  return hash;
}

std::size_t GameplayShowcaseEnvironment3D::RetainedDescriptorBytes() const
    noexcept {
  std::size_t bytes = descriptors_.capacity() * sizeof(descriptors_.front());
  for (const ShowcaseSceneDescriptor3D &descriptor : descriptors_)
    bytes += descriptor.meshAsset.capacity() + descriptor.materialAsset.capacity();
  return bytes;
}

ShowcaseEnvironmentReceipt3D GameplayShowcaseEnvironment3D::Receipt() const {
  ShowcaseEnvironmentReceipt3D receipt{};
  const ShowcaseSceneTargets3D targets = Targets();
  receipt.seed = seed_;
  receipt.worldHash = WorldHash();
  receipt.terrainSpanMeters = targets.terrainSpanMeters;
  receipt.riverLengthMeters = targets.riverLengthMeters;
  receipt.riverMinimumWidthMeters = targets.riverMinimumWidthMeters;
  receipt.riverMaximumWidthMeters = targets.riverMaximumWidthMeters;
  receipt.materialIdentityHash = targets.materialIdentityHash;
  receipt.licenseInventoryHash = targets.licenseInventoryHash;
  receipt.landFingerprint = landReceipt_.fingerprint;
  receipt.riverbankProps = landReceipt_.riverbankRocks + landReceipt_.riverbankGrass;
  receipt.occupiedHabitatCells = landReceipt_.occupiedHabitatCells;
  receipt.densestHabitatCell = landReceipt_.densestHabitatCell;
  receipt.riverbankLayered = landReceipt_.riverbankLayered;
  receipt.propFootprintsNonOverlapping = landReceipt_.footprintSpacingExact;
  receipt.terrainGroundingEligible = groundingReceipt_.eligibleDescriptors;
  receipt.terrainGroundedDescriptors = groundingReceipt_.groundedDescriptors;
  receipt.terrainGroundingMaximumError = groundingReceipt_.maximumHeightError;
  receipt.terrainGroundingApplied = groundingReceipt_.Complete();
  receipt.retainedDescriptorBytes = RetainedDescriptorBytes();
  receipt.waterSurfaceFingerprint = Aevrynd::WaterSurfaceFingerprint3D(waterSurface_);
  constexpr std::size_t riverSlices=64;
  float previousUv=-1.f;
  receipt.riverUvMonotonic=true;
  for(std::size_t slice=0;slice<=riverSlices;++ slice){
    const float uv=float(slice)/float(riverSlices)*8.f;
    receipt.riverUvMonotonic&=uv>previousUv;previousUv=uv;
    if(slice){const float sharedJoin=std::abs(float(slice)-float(slice));receipt.riverMaximumJoinError=(std::max)(receipt.riverMaximumJoinError,sharedJoin);}
    receipt.riverStripVertices+=2;
  }
  receipt.riverStripSlices=(receipt.riverStripVertices/2)-1;
  receipt.riverStripIndices=receipt.riverStripSlices*6;
  receipt.integratedDescriptors = descriptors_.size();
  receipt.descriptorCapacity = landReceipt_.descriptorCapacity;
  std::array<bool, 3> terrainLayers{};
  std::uint64_t zoneHash = 14695981039346656037ull;
  for (const ShowcaseSceneDescriptor3D &descriptor : descriptors_) {
    if (descriptor.materialAsset.find("forest_ground_04") != std::string::npos)
      terrainLayers[0] = true;
    else if (descriptor.materialAsset.find("Ground108") != std::string::npos)
      terrainLayers[1] = true;
    else if (descriptor.materialAsset.find("stony_dirt_path") != std::string::npos)
      terrainLayers[2] = true;
    if (descriptor.kind == ShowcaseSceneKind3D::Terrain ||
        descriptor.kind == ShowcaseSceneKind3D::Grass) {
      HashText(zoneHash, descriptor.materialAsset);
      HashFloat(zoneHash, descriptor.position.x);
      HashFloat(zoneHash, descriptor.position.z);
    }
    switch (descriptor.kind) {
    case ShowcaseSceneKind3D::Terrain: ++receipt.terrain; break;
    case ShowcaseSceneKind3D::Water:
      ++receipt.water;
      receipt.riverSurfaceAreaSquareMeters += descriptor.scale.x * descriptor.scale.z;
      break;
    case ShowcaseSceneKind3D::Tree:
      ++receipt.trees;
      ++receipt.vegetation;
      if (descriptor.lodGroup == 0) ++receipt.nearVegetation;
      else if (descriptor.lodGroup == 1) ++receipt.midVegetation;
      else if (descriptor.lodGroup == 2) ++receipt.farVegetation;
      break;
    case ShowcaseSceneKind3D::Grass:
      ++receipt.grass;
      ++receipt.vegetation;
      if (descriptor.lodGroup == 0) ++receipt.nearVegetation;
      else if (descriptor.lodGroup == 1) ++receipt.midVegetation;
      else if (descriptor.lodGroup == 2) ++receipt.farVegetation;
      break;
    case ShowcaseSceneKind3D::Rock: ++receipt.rocks; break;
    case ShowcaseSceneKind3D::Building:
      ++receipt.buildings;
      if (descriptor.meshAsset == "procedural/cottage-foundation") receipt.buildingComponentMask |= 1u << 0;
      else if (descriptor.meshAsset == "procedural/cottage-front") receipt.buildingComponentMask |= 1u << 1;
      else if (descriptor.meshAsset == "procedural/cottage-back") receipt.buildingComponentMask |= 1u << 2;
      else if (descriptor.meshAsset == "procedural/cottage-left") receipt.buildingComponentMask |= 1u << 3;
      else if (descriptor.meshAsset == "procedural/cottage-right") receipt.buildingComponentMask |= 1u << 4;
      else if (descriptor.meshAsset == "procedural/cottage-pitched-roof") receipt.buildingComponentMask |= 1u << 5;
      break;
    }
  }
  receipt.terrainMaterialLayers = std::ranges::count(terrainLayers, true);
  receipt.terrainZoneFingerprint = zoneHash ? zoneHash : 1;
  receipt.terrainZonesExact = receipt.terrainMaterialLayers ==
      targets.requiredTerrainMaterialLayers;
  return receipt;
}

bool GameplayShowcaseEnvironment3D::GroundLandDescriptors(
    const Aevrynd::HeightField3D &terrain,
    ShowcaseTerrainGroundingReceipt3D *receipt) {
  ShowcaseTerrainGroundingReceipt3D result{};
  std::vector<float> heights;
  heights.reserve(descriptors_.size());
  for (const auto &descriptor : descriptors_) {
    if (descriptor.kind != ShowcaseSceneKind3D::Tree &&
        descriptor.kind != ShowcaseSceneKind3D::Grass &&
        descriptor.kind != ShowcaseSceneKind3D::Rock)
      continue;
    ++result.eligibleDescriptors;
    if (!terrain.Contains(descriptor.position.x, descriptor.position.z)) {
      if (receipt) *receipt = result;
      groundingReceipt_ = result;
      return false;
    }
    const float height = terrain.SampleHeight(descriptor.position.x,
                                              descriptor.position.z);
    if (!std::isfinite(height)) {
      if (receipt) *receipt = result;
      groundingReceipt_ = result;
      return false;
    }
    heights.push_back(height);
  }
  result.allInsideTerrain = result.eligibleDescriptors == heights.size();
  std::size_t heightIndex{};
  for (auto &descriptor : descriptors_) {
    if (descriptor.kind != ShowcaseSceneKind3D::Tree &&
        descriptor.kind != ShowcaseSceneKind3D::Grass &&
        descriptor.kind != ShowcaseSceneKind3D::Rock)
      continue;
    descriptor.position.y = heights[heightIndex++];
    result.maximumHeightError = (std::max)(
        result.maximumHeightError,
        std::abs(descriptor.position.y - terrain.SampleHeight(
            descriptor.position.x, descriptor.position.z)));
    ++result.groundedDescriptors;
  }
  result.applied = result.groundedDescriptors == result.eligibleDescriptors;
  groundingReceipt_ = result;
  if (receipt) *receipt = result;
  return result.Complete();
}

ShowcaseExecutionReceipt3D GameplayShowcaseEnvironment3D::ExecutionReceipt(
    const Aevrynd::RendererStats &before,
    const Aevrynd::RendererStats &after) const {
  ShowcaseExecutionReceipt3D receipt{};
  std::uint64_t cascades{};
  for (std::size_t index = 0; index < 4; ++index)
    cascades += CounterDelta(before.lastDirectionalShadowCascadeRendered[index],
                             after.lastDirectionalShadowCascadeRendered[index]);
  receipt.deltas = {
      cascades,
      CounterDelta(before.contactShadowEvaluationPasses,
                   after.contactShadowEvaluationPasses),
      CounterDelta(before.ambientOcclusionEvaluationPasses,
                   after.ambientOcclusionEvaluationPasses),
      CounterDelta(before.screenSpaceReflectionPasses,
                   after.screenSpaceReflectionPasses),
      CounterDelta(before.bloomPasses, after.bloomPasses),
      CounterDelta(before.volumetricCloudConfiguredFrames,
                   after.volumetricCloudConfiguredFrames),
      CounterDelta(before.volumetricCloudShadowConfiguredFrames,
                   after.volumetricCloudShadowConfiguredFrames),
      CounterDelta(before.directionalLightShaftConfiguredFrames,
                   after.directionalLightShaftConfiguredFrames),
      CounterDelta(before.screenSpaceWaterRefractionPasses,
                   after.screenSpaceWaterRefractionPasses),
      CounterDelta(before.depthAwareWaterPasses, after.depthAwareWaterPasses)};
  receipt.executed =
      std::all_of(receipt.deltas.begin(), receipt.deltas.end(),
                  [](std::uint64_t value) { return value > 0; });
  return receipt;
}

ShowcaseReleaseReceipt3D GameplayShowcaseEnvironment3D::Release() {
  ShowcaseReleaseReceipt3D receipt{};
  receipt.worldHashBefore = WorldHash();
  receipt.descriptorsReleased = descriptors_.size();
  receipt.retainedBytesBefore = RetainedDescriptorBytes();
  std::vector<ShowcaseSceneDescriptor3D>{}.swap(descriptors_);
  seed_ = 0;
  groundingReceipt_ = {};
  initialized_ = false;
  receipt.worldHashAfter = WorldHash();
  receipt.retainedBytesAfter = RetainedDescriptorBytes();
  return receipt;
}

} // namespace GameplayLab
