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
      v.x+=dx;v.z+=dz;v.y+=slope/(1.f+distance/32.f);
      v.u=v.x*.085f;v.v=v.z*.085f;
      outer[i]=unsigned(mesh.vertices.size());mesh.vertices.push_back(v);
    }
    for(unsigned i=0;i<perimeter;++i){const unsigned j=(i+1)%perimeter;
      mesh.indices.insert(mesh.indices.end(),{inner[i],inner[j],outer[j],inner[i],outer[j],outer[i]});
    }
    inner=outer;
  }
  return true;
}

bool GameplayShowcaseEnvironment3D::Initialize(std::uint64_t seed) {
  if (!seed)
    return false;

  std::vector<ShowcaseSceneDescriptor3D> candidate;
  candidate.reserve(464);
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
    const std::string id(ShowcaseLandAssetId3D(placement.variant));
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
      descriptor.kind = ShowcaseSceneKind3D::Grass;
      descriptor.meshAsset = id;
      descriptor.materialAsset =
          placement.zone == ShowcaseLandZone3D::Riverbank
              ? "poly-haven/stony_dirt_path/cooked-pbr"
              : placement.zone == ShowcaseLandZone3D::Grove
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
                         index == 5 ? "ambientcg/RoofingTiles014B/cooked-pbr"
                                    : "ambientcg/Bricks100/cooked-pbr"});

  descriptors_.swap(candidate);
  // 林地河水仍需吸收紅光，但紅、藍吸收差距不能大到讓水面成為自發光
  // 青色帶。降低深水藍綠散射並收斂通道差距，讓水色主要來自天空、
  // 河床與岸邊樹木的反射，同時保留淺灘可讀性。
  // 實際水面內 0.25m 採樣的深度 p1=.0476m、p5=.2710m；泡沫只保留最淺岸帶。
  // 寬波提升到約 7.1m 波長，配合八個非共線解析分量，讓第三人稱與 God 鏡位
  // 都看得到可解釋的法線與反射變化，不再是平面或規則百葉條紋。細波仍由
  // 像素世界足跡 band-limit，不以螢幕亮度雜訊製造假細節。
  waterSurface_ = Aevrynd::NormalizeWaterSurfaceSettings3D({true, {.42f,.25f,.19f}, {.018f,.035f,.038f}, .035f, .045f, .14f, .14f, .20f, .12f, .92f, .12f, .28f, .03f, .14f, .26f, .68f, .90f, .020f});
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
  draw.SetContactShadows({true, 24, 2.2f, .032f, .92f});
  draw.SetAmbientOcclusion({true, 1.25f, .025f, 1.32f});
  draw.SetScreenSpaceReflections({true, .82f, 42.f, .7f, .32f, 16});
  draw.SetBloom({true, 1.18f, .38f, .085f, 2.2f});
  // 英雄鏡頭仰角低，舊的低空高覆蓋雲層會把投影壓成一條貫穿畫面的
  // 灰色水平煙牆。保留真正的體積雲與雲影，但提高雲底、降低覆蓋與密度，
  // 並加強侵蝕，讓天空由分散雲團建立深度而不是由均勻長帶切割。
  Aevrynd::VolumetricCloudSettings3D clouds{};
  clouds.baseAltitude = 2100.f;
  clouds.thickness = 1700.f;
  clouds.coverage = .38f;
  clouds.density = .56f;
  clouds.erosion = .58f;
  clouds.windSpeed = 9.f;
  clouds.sampleCount = 16;
  clouds.shadowsEnabled = true;
  clouds.shadowStrength = .32f;
  clouds.maximumDistance = 9000.f;
  draw.SetVolumetricClouds(clouds);
  draw.SetDirectionalLightShafts({true, .68f, .72f, 360.f, 6});
  Aevrynd::TerrainDistanceMaterialSettings3D terrainMaterial{
      true, 32.f, 176.f, .0042f, .22f, 1.28f, .18f};
  terrainMaterial.mesoPartitionEnabled = true;
  draw.SetTerrainDistanceMaterial(terrainMaterial);
  draw.SetWaterOptics({true, .028f, .52f, .055f});
  draw.SetWaterSurface(waterSurface_);
}

ShowcaseLightingState3D GameplayShowcaseEnvironment3D::EvaluateLighting(
    ShowcaseLightingProfile3D profile) const {
  ShowcaseLightingState3D state{};
  state.profile = profile;
  if (profile == ShowcaseLightingProfile3D::Day) {
    state = {profile, {{.34f, -.91f, .23f}, .24f, {1, .96f, .88f, 1}, 1.42f},
             {.24f, .42f, .67f, 1}, {.32f, .43f, .55f, .62f}, -.12f, 46800.f};
  } else if (profile == ShowcaseLightingProfile3D::Dusk) {
    state = {profile, {{-.74f, -.42f, .18f}, .19f, {1, .66f, .38f, 1}, .94f},
             {.19f, .14f, .23f, 1}, {.3f, .19f, .25f, .68f}, .08f, 68400.f};
  } else {
    // 月光保留冷色辨識，但夜間亮度由局部光源與反射建立層次，不能用
    // 全畫面藍色增益把水面與地表抬得比黃昏更亮。
    state = {profile, {{-.25f, -.62f, -.74f}, .11f, {.42f, .55f, 1, 1}, .42f},
             {.05f, .085f, .155f, 1}, {.1f, .16f, .28f, .7f}, -.15f, 79200.f};
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
    const float night = profile == ShowcaseLightingProfile3D::Night ? 1.25f : .55f;
    draw.AddPointLight({{-12.f, 2.15f, 19.65f}, 11.f,
                        {1.f, .57f, .25f, 1.f}, 4.2f * night});
    draw.AddPointLight({{-14.55f, 2.25f, 19.75f}, 7.5f,
                        {1.f, .72f, .38f, 1.f}, 2.35f * night});
    draw.AddPointLight({{-9.45f, 2.25f, 19.75f}, 7.5f,
                        {1.f, .72f, .38f, 1.f}, 2.35f * night});
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
  else radiance={{.012f,.022f,.048f},.24f,{.045f,.06f,.095f},1.8f,{.010f,.014f,.020f},.00935f,{.72f,.78f,1.f},.24f,{.55f,.66f,1.f},1.2f,0x4e49474854534b32ull};
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
    environmentBlend.objectFillIntensity = .92f;
    environmentBlend.phaseFingerprint = 0x4441595f49424c31ull;
  } else if (profile == ShowcaseLightingProfile3D::Dusk) {
    environmentBlend.diffuseRgbScale = {1.f,.88f,.72f};
    environmentBlend.diffuseIntensity = .72f;
    environmentBlend.specularIntensity = .78f;
    environmentBlend.analyticDiffuseWeight = .68f;
    environmentBlend.iblDiffuseWeight = .32f;
    environmentBlend.analyticSpecularWeight = .62f;
    environmentBlend.iblSpecularWeight = .38f;
    environmentBlend.objectFillRadiance = {.44f,.25f,.16f};
    environmentBlend.objectFillIntensity = .72f;
    environmentBlend.phaseFingerprint = 0x4455534b5f49424cull;
  } else {
    environmentBlend.diffuseRgbScale = {.72f,.78f,1.f};
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
  if (profile == ShowcaseLightingProfile3D::Night) {
    // 夜間不沿用預設 +2 EV：壓住月光與小屋高光，同時只在 tone curve
    // 暗部提供有限 lift，讓林地輪廓可讀而不抬高整個畫面。
  timeOfDayExposure.nightExposureEv = 1.50f;
    timeOfDayExposure.shadowLift = .03f;
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
