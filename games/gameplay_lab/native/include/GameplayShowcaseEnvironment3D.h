#pragma once

#include <Aevrynd/Camera.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/Renderer.h>
#include "ShowcaseLandEcology3D.h"
#include <Aevrynd/Scene3D.h>

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace GameplayLab {

enum class ShowcaseLightingProfile3D : std::uint8_t { Day, Dusk, Night };
inline constexpr Aevrynd::Vec3 GameplayCottagePorchLanternCenter3D{
    -12.f, 2.45f, 19.70f};
enum class ShowcaseSceneKind3D : std::uint8_t {
  Terrain,
  Water,
  Tree,
  Grass,
  Rock,
  Building
};

struct ShowcaseTreeFormVariation3D {
  Aevrynd::Vec3 scaleMultiplier{1.f, 1.f, 1.f};
  float yawOffset{};
};

struct ShowcaseUnderstoryFormVariation3D {
  Aevrynd::Vec3 scaleMultiplier{1.f, 1.f, 1.f};
  float yawOffset{};
};

struct ShowcaseUnderstoryFootprintBounds3D {
  Aevrynd::Vec2 center{};
  Aevrynd::Vec2 extents{};
};

// The authored ecology variant describes habitat intent.  Near hero-trail
// shoulders need the already-loaded multi-species community silhouette rather
// than a single fern, while every placement outside that narrow band keeps its
// original mesh.  This is deterministic world-space routing, not camera LOD.
[[nodiscard]] inline ShowcaseLandVariant3D
ShowcaseUnderstoryPresentationVariant3D(
    const ShowcaseLandPlacement3D &placement) noexcept {
  if (placement.kind != ShowcaseLandKind3D::Grass ||
      placement.variant != ShowcaseLandVariant3D::GrassFan ||
      placement.band != ShowcaseLandBand3D::Near ||
      placement.zone == ShowcaseLandZone3D::Riverbank)
    return placement.variant;
  const float laneDistance =
      SampleShowcaseHeroTrail3D(placement.position).distance;
  return laneDistance >= 1.4f && laneDistance <= 5.25f
             ? ShowcaseLandVariant3D::GrassTuft
             : placement.variant;
}

// 這三組水平 AABB 來自正式 submission 使用的 Near 網格與固定 seed；smoke
// 會直接重建 production mesh 並逐值核對，避免此處在網格演進後靜默失真。
[[nodiscard]] inline constexpr ShowcaseUnderstoryFootprintBounds3D
ShowcaseUnderstoryHighDetailFootprintBounds3D(
    ShowcaseLandVariant3D variant) noexcept {
  switch (variant) {
  case ShowcaseLandVariant3D::GrassBlade:
    return {{.0554493f, .00563303f}, {.736945f, .703501f}};
  case ShowcaseLandVariant3D::GrassFan:
    return {{.0142904f, -.0146019f}, {.40636f, .442553f}};
  case ShowcaseLandVariant3D::GrassTuft:
    return {{.00975448f, .136403f}, {.975619f, .856231f}};
  default: return {};
  }
}

[[nodiscard]] inline float ShowcaseUnderstoryPointSegmentDistance3D(
    Aevrynd::Vec2 point, Aevrynd::Vec2 start, Aevrynd::Vec2 end) noexcept {
  const Aevrynd::Vec2 delta{end.x - start.x, end.y - start.y};
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

[[nodiscard]] inline float ShowcaseUnderstorySegmentDistance3D(
    Aevrynd::Vec2 a, Aevrynd::Vec2 b, Aevrynd::Vec2 c,
    Aevrynd::Vec2 d) noexcept {
  const auto cross = [](Aevrynd::Vec2 left, Aevrynd::Vec2 right) {
    return left.x * right.y - left.y * right.x;
  };
  const Aevrynd::Vec2 ab{b.x - a.x, b.y - a.y};
  const Aevrynd::Vec2 cd{d.x - c.x, d.y - c.y};
  const float denominator = cross(ab, cd);
  if (std::abs(denominator) > 1e-7f) {
    const Aevrynd::Vec2 ac{c.x - a.x, c.y - a.y};
    const float t = cross(ac, cd) / denominator;
    const float u = cross(ac, ab) / denominator;
    if (t >= 0.f && t <= 1.f && u >= 0.f && u <= 1.f) return 0.f;
  }
  return (std::min)({ShowcaseUnderstoryPointSegmentDistance3D(a, c, d),
                     ShowcaseUnderstoryPointSegmentDistance3D(b, c, d),
                     ShowcaseUnderstoryPointSegmentDistance3D(c, a, b),
                     ShowcaseUnderstoryPointSegmentDistance3D(d, a, b)});
}

[[nodiscard]] inline bool ShowcaseUnderstoryPointInsideFootprint3D(
    Aevrynd::Vec2 point,
    const std::array<Aevrynd::Vec2, 4> &corners) noexcept {
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

// 以正式 descriptor scale/yaw 變換高細節網格 AABB，再量到共享英雄步道
// 折線的最近距離；不再另存一條會和地表路徑漂移的直線。
[[nodiscard]] inline float ShowcaseUnderstoryWalkingClearance3D(
    const ShowcaseLandPlacement3D &placement,
    const ShowcaseUnderstoryFormVariation3D &form) noexcept {
  const auto bounds = ShowcaseUnderstoryHighDetailFootprintBounds3D(
      ShowcaseUnderstoryPresentationVariant3D(placement));
  if (bounds.extents.x <= 0.f || bounds.extents.y <= 0.f)
    return (std::numeric_limits<float>::max)();
  const float scaleX = placement.scale.x * form.scaleMultiplier.x;
  const float scaleZ = placement.scale.z * form.scaleMultiplier.z;
  const float yaw = placement.yaw + form.yawOffset;
  const float sine = std::sin(yaw), cosine = std::cos(yaw);
  const auto rotate = [sine, cosine](float x, float z) {
    return Aevrynd::Vec2{x * cosine - z * sine, x * sine + z * cosine};
  };
  const auto centerOffset = rotate(bounds.center.x * scaleX,
                                   bounds.center.y * scaleZ);
  const Aevrynd::Vec2 center{placement.position.x + centerOffset.x,
                             placement.position.z + centerOffset.y};
  constexpr std::array<Aevrynd::Vec2, 4> signs{{
      {-1.f, -1.f}, {1.f, -1.f}, {1.f, 1.f}, {-1.f, 1.f}}};
  std::array<Aevrynd::Vec2, 4> corners{};
  for (std::size_t index = 0; index < corners.size(); ++index) {
    const auto offset = rotate(signs[index].x * bounds.extents.x * scaleX,
                               signs[index].y * bounds.extents.y * scaleZ);
    corners[index] = {center.x + offset.x, center.y + offset.y};
  }
  float clearance = (std::numeric_limits<float>::max)();
  const auto &controls = ShowcaseHeroTrailControls3D();
  for (const auto control : controls)
    if (ShowcaseUnderstoryPointInsideFootprint3D(
            {control.x, control.z}, corners))
      return 0.f;
  for (std::size_t segment = 0; segment + 1 < controls.size(); ++segment) {
    const Aevrynd::Vec2 laneStart{controls[segment].x, controls[segment].z};
    const Aevrynd::Vec2 laneEnd{controls[segment + 1].x,
                                controls[segment + 1].z};
    for (std::size_t edge = 0; edge < corners.size(); ++edge)
      clearance = (std::min)(clearance, ShowcaseUnderstorySegmentDistance3D(
          laneStart, laneEnd, corners[edge],
          corners[(edge + 1u) % corners.size()]));
  }
  return clearance;
}

// 只在正式葉片 AABB 會進入步行帶時，沿用原寬深比例做最大可通過縮放；
// 二分搜尋保留盡可能多的既有密度變化，並留 0.5mm 浮點安全量。
[[nodiscard]] inline ShowcaseUnderstoryFormVariation3D
ClampShowcaseUnderstoryWalkingClearance3D(
    const ShowcaseLandPlacement3D &placement,
    ShowcaseUnderstoryFormVariation3D form) noexcept {
  constexpr float requiredClearance = 1.4005f;
  if (ShowcaseUnderstoryWalkingClearance3D(placement, form) >=
      requiredClearance)
    return form;
  float passing = 0.f, failing = 1.f;
  for (unsigned iteration = 0; iteration < 24u; ++iteration) {
    const float candidate = (passing + failing) * .5f;
    auto trial = form;
    trial.scaleMultiplier.x *= candidate;
    trial.scaleMultiplier.z *= candidate;
    if (ShowcaseUnderstoryWalkingClearance3D(placement, trial) >=
        requiredClearance)
      passing = candidate;
    else
      failing = candidate;
  }
  form.scaleMultiplier.x *= passing;
  form.scaleMultiplier.z *= passing;
  return form;
}

// 活樹模型根部帶有來源資產的亮色切面；只在 render transform 將它埋入
// 地表少許。枯木保留既有落地姿態，未知資產也採保守的零位移策略。
[[nodiscard]] inline float ShowcaseTreeBurialOffset3D(
    std::string_view assetId, float scaleY) noexcept {
  if (assetId.find("dead_tree_trunk_02") != std::string_view::npos)
    return 0.f;
  const bool liveTree =
      assetId.find("island_tree_01") != std::string_view::npos ||
      assetId.find("tree_small_02") != std::string_view::npos ||
      assetId.find("fir_tree_01_c") != std::string_view::npos;
  if (!liveTree) return 0.f;
  if (!std::isfinite(scaleY)) return 0.f;
  return std::clamp(scaleY * .012f, .10f, .20f);
}

// 同一棵來源樹仍維持原 material、descriptor、collision 與 HLOD identity；
// 只由 placement 的永久 stableKey、距離帶與既有樹種選出有限輪廓變化。
// 近中景成熟林冠可在不改樹幹碰撞與 placement spacing 的前提下彼此搭接；
// 這是森林冠層而不是果園淨空。林緣的 Far 樹略矮，讓主冠層、次冠層、
// 幼樹與枯立木形成可辨識的高度節奏而不增加 draw。
[[nodiscard]] inline ShowcaseTreeFormVariation3D
ShowcaseTreeFormVariation(const ShowcaseLandPlacement3D &placement) noexcept {
  if (placement.kind != ShowcaseLandKind3D::Tree || !placement.stableKey)
    return {};
  const float u = float((placement.stableKey >> 16u) & 0xffffu) / 65535.f;
  const float v = float((placement.stableKey >> 40u) & 0xffffu) / 65535.f;
  const float bandHeight = placement.band == ShowcaseLandBand3D::Far
                               ? .86f
                           : placement.band == ShowcaseLandBand3D::Mid
                               ? .94f
                               : 1.f;
  const bool dead = placement.variant == ShowcaseLandVariant3D::DeadTreeTrunk;
  const bool sapling = placement.variant == ShowcaseLandVariant3D::TreeSmall02;
  // 舊版活冠在兩軸重複收窄，正式 Near/Mid 面積只剩配置的約 72%，
  // 使已群聚的樹林再次讀成果園。水平僅做小幅輪廓差異；render form 不回寫
  // placement，因此正式樹幹 collision 代理與道路／河岸淨空逐值不變。
  const bool connectedCanopy = !dead &&
      placement.band != ShowcaseLandBand3D::Far;
  const float canopy = connectedCanopy
                           ? (sapling ? .98f + .14f * u : 1.08f + .16f * u)
                       : dead ? .58f + .16f * u
                       : sapling ? .72f + .18f * u
                                 : .82f + .18f * u;
  float heightProfile = dead ? .82f + .24f * v
                              : sapling ? .86f + .22f * v
                                        : .90f + .26f * v;
  if (!dead) {
    // 三個距離帶都保留清楚的物種高度角色：幼樹形成連續低層，成熟樹以
    // 四階冠層建立高輪廓，Far 再整體略矮。只改 render Y；水平 footprint、
    // placement/collision、道路與河岸淨空完全不變，且 stableKey 使結果永久
    // 固定，不引入相機依賴或 descriptor/draw 增量。
    if (sapling) {
      const float submittedHeight = placement.band == ShowcaseLandBand3D::Far
                                        ? .70f + .10f * v
                                    : placement.band == ShowcaseLandBand3D::Mid
                                        ? .71f + .13f * v
                                        : .74f + .12f * v;
      heightProfile = submittedHeight / bandHeight;
    } else {
      constexpr std::array<float, 4> crownTierOffset{-.02f, .01f, .025f,
                                                     .04f};
      const auto tier = std::size_t((placement.stableKey >> 6u) & 3u);
      // 直接約束提交後的高度，避免 Far 先用偏大的 profile 再依 band 縮放，
      // 仍留下昂貴高冠層。成熟樹維持比幼樹高約 0.18~0.32，但最大值壓在
      // 1.05 左右，降低 masked 葉片與四層 shadow 的投影面積。
      const float submittedHeight = placement.band == ShowcaseLandBand3D::Far
                                        ? .93f + .08f * v +
                                              crownTierOffset[tier]
                                    : placement.band == ShowcaseLandBand3D::Mid
                                        ? .94f + .09f * v +
                                              crownTierOffset[tier]
                                        : .94f + .09f * v +
                                              crownTierOffset[tier];
      heightProfile = submittedHeight / bandHeight;
    }
  }
  const float height = bandHeight * heightProfile;
  const float depth = canopy * (connectedCanopy ? .96f + .08f * v
                                                : .88f + .12f * v);
  return {{canopy, height, depth}, (u - .5f) * .42f};
}

// 固定 186 個地被 descriptor 不增量；只以既有 stableKey、生境與距離帶
// 形成三種可讀輪廓。河岸葉片較高且順著河道切線排列，草甸維持開闊扇形，
// 林下叢則形成樹根周圍的連續中低層。每個 stableKey 固定選出寬窄節奏，
// 讓既有十二株群落在 third / God 視角仍讀成不規則植被島；屋前步道肩帶只
// 補足葉面，不移動根部。草是明確 non-solid，placement 仍是碰撞／步行淨空
// 權威；葉片僅允許受限外伸（<=1.34），不改 HLOD identity、descriptor/draw
// budget 或任何實體碰撞代理。
[[nodiscard]] inline ShowcaseUnderstoryFormVariation3D
ShowcaseUnderstoryFormVariation(
    const ShowcaseLandPlacement3D &placement) noexcept {
  if (placement.kind != ShowcaseLandKind3D::Grass || !placement.stableKey)
    return {};
  const float u = float((placement.stableKey >> 8u) & 0xffffu) / 65535.f;
  const float v = float((placement.stableKey >> 32u) & 0xffffu) / 65535.f;
  const float bandHeight = placement.band == ShowcaseLandBand3D::Far
                               ? .92f
                           : placement.band == ShowcaseLandBand3D::Mid
                               ? .97f
                               : 1.f;
  constexpr std::array<float, 4> riverWidths{.94f, 1.04f, 1.15f, 1.25f};
  constexpr std::array<float, 4> riverDepths{1.08f, 1.17f, 1.26f, 1.34f};
  constexpr std::array<float, 4> meadowWidths{1.03f, 1.14f, 1.25f, 1.34f};
  constexpr std::array<float, 4> meadowDepths{1.06f, 1.16f, 1.27f, 1.34f};
  constexpr std::array<float, 4> uplandWidths{1.18f, 1.27f, 1.33f, 1.34f};
  constexpr std::array<float, 4> uplandDepths{1.18f, 1.27f, 1.32f, 1.34f};
  const auto xClass = std::size_t((placement.stableKey >> 12u) & 3u);
  const auto zClass = std::size_t((placement.stableKey >> 46u) & 3u);
  if (placement.zone == ShowcaseLandZone3D::Riverbank) {
    const float z = placement.position.z;
    const float riverDerivative =
        .175f * std::cos(z * .035f) + .022f * std::cos(z * .011f);
    const float tangentYaw = std::atan2(1.f, riverDerivative);
    const float targetYaw = tangentYaw + (u - .5f) * .24f;
    return ClampShowcaseUnderstoryWalkingClearance3D(
        placement,
        {{riverWidths[xClass], bandHeight * (1.28f + .34f * v),
          riverDepths[zClass]},
         std::remainder(targetYaw - placement.yaw,
                        6.2831853071795864769f)});
  }
  float pathShoulderBoost{};
  if (placement.band == ShowcaseLandBand3D::Near) {
    const float laneDistance =
        SampleShowcaseHeroTrail3D(placement.position).distance;
    // 既有肩帶中心約落在道路外 3~5m；只提高該葉簇的 render footprint。
    if (laneDistance >= 1.4f && laneDistance <= 5.25f)
      pathShoulderBoost = 1.32f;
  }
  if (placement.zone == ShowcaseLandZone3D::Meadow)
    return ClampShowcaseUnderstoryWalkingClearance3D(
        placement,
        {{(std::max)(meadowWidths[xClass], pathShoulderBoost), 1.f,
          (std::max)(meadowDepths[zClass], pathShoulderBoost)},
         (u - .5f) * .64f});
  // Upland 是樹根周圍的正式林下生境；中景必須維持完整群落冠幅，不能
  // 再隨 band 收窄成零星草點。蕨／灌木網格以公尺建模，正式 submission
  // 維持 Y=1；只補回水平覆蓋，仍守既有 1.34 上限且不擴張 Far 的 overdraw。
  return ClampShowcaseUnderstoryWalkingClearance3D(
      placement,
      {{(std::max)(uplandWidths[xClass], pathShoulderBoost), 1.f,
        (std::max)(uplandDepths[zClass], pathShoulderBoost)},
       (u - .5f) * .76f});
}

// 日光下的 moss/soil 貼圖會經 IBL 與直射光再次增亮；保持綠色通道優勢並
// 壓低總反射率，避免林地在正式曝光下變成黃白色草原。
[[nodiscard]] inline constexpr Aevrynd::Color
ShowcaseForestTerrainTint3D() noexcept {
  // 以低反射率苔蘚綠取代偏黃的亮草坪色：在不改曝光、燈光或材質
  // 取樣的前提下，保留綠色優勢，同時讓土壤、落葉與濕岸分區可讀。
  return {.47f, .535f, .405f, 1.f};
}

// 三層地表使用同一套世界座標貼圖；較低頻率保留近景 PBR 細節，同時
// 延長重複週期，避免 God 鏡頭把短週期紋理壓成規則斜向刷痕。
[[nodiscard]] inline constexpr float
ShowcaseForestTerrainTextureScale3D() noexcept {
  return .5f;
}

// placement 已依河岸、林地與草地選出不同生態材質，但舊提交路徑把三者
// 全部覆寫成同一個暗綠色，遠景因而只剩黑色短線。保留共用 PBR 紋理，
// 以受限色調恢復可辨識的林下分區。
[[nodiscard]] inline constexpr Aevrynd::Color
ShowcaseForestGrassTint3D(std::string_view materialAsset) noexcept {
  if (materialAsset.find("stony_dirt_path") != std::string_view::npos)
    return {.32606232f, .44926232f, .22926232f, 1.f};
  if (materialAsset.find("forest_ground_04") != std::string_view::npos)
    return {.30138504f, .47738504f, .22218504f, 1.f};
  return {.40266832f, .59626832f, .27946832f, 1.f};
}

[[nodiscard]] inline constexpr std::string_view
ShowcaseForestGrassMaterialAsset3D(std::string_view materialAsset) noexcept {
  if (materialAsset.find("stony_dirt_path") != std::string_view::npos)
    return "stony_dirt_path";
  if (materialAsset.find("forest_ground_04") != std::string_view::npos)
    return "forest_ground_04";
  if (materialAsset.find("Ground108") != std::string_view::npos)
    return "Ground108";
  return {};
}

[[nodiscard]] inline Aevrynd::Material3D ShowcaseForestGrassMaterial3D(
    Aevrynd::Material3D material, std::string_view materialAsset,
    bool castsShadow) noexcept {
  material.materialClass = Aevrynd::MaterialClass3D::Foliage;
  material.tint = ShowcaseForestGrassTint3D(materialAsset);
  material.surface.roughness = .9f;
  material.surface.metallic = 0.f;
  material.surface.emissive = 0.f;
  // 程序葉片已有逐頂點色、法線與風權重；地面用碎石／泥土貼圖不能再
  // 投影到細葉上，否則縮至遠景時會形成高反差黑針。
  material.textures = {};
  material.useUv = false;
  material.normalScale = .72f;
  material.doubleSided = true;
  material.castsShadow = castsShadow;
  return material;
}

// 選擇器與場景提交共用同一份正式草地LOD政策，避免距離與混合寬度漂移。
[[nodiscard]] inline constexpr Aevrynd::MeshLodPolicy3D
ShowcaseGrassLodPolicy3D() noexcept {
  return {22.f, 5.f, Aevrynd::MeshLodDistanceMetric3D::BoundsSurface};
}

// 正式闊葉樹的來源 LOD2 仍保留完整樹冠輪廓，但三角形已比 LOD0 少約
// 六倍。密林不能沿用岩石的 18/48m 切換，否則近景環帶會同時提交大量
// 百萬級 masked 葉片。只縮短幾何細節距離，不改 placement、樹冠尺度、
// 陰影語意或物件 identity。
[[nodiscard]] inline constexpr std::array<Aevrynd::MeshLodPolicy3D, 3>
ShowcaseTreeLodPolicies3D() noexcept {
  using Metric = Aevrynd::MeshLodDistanceMetric3D;
  return {Aevrynd::MeshLodPolicy3D{8.f, 2.f, Metric::BoundsSurface},
          Aevrynd::MeshLodPolicy3D{20.f, 4.f, Metric::BoundsSurface},
          Aevrynd::MeshLodPolicy3D{64.f, 8.f, Metric::BoundsSurface}};
}

[[nodiscard]] inline constexpr std::array<Aevrynd::MeshLodPolicy3D, 3>
ShowcaseOpaqueLodPolicies3D() noexcept {
  using Metric = Aevrynd::MeshLodDistanceMetric3D;
  return {Aevrynd::MeshLodPolicy3D{18.f, 3.f, Metric::BoundsSurface},
          Aevrynd::MeshLodPolicy3D{48.f, 6.f, Metric::BoundsSurface},
          Aevrynd::MeshLodPolicy3D{96.f, 10.f, Metric::BoundsSurface}};
}

// 正式 Terrain shader 以頂點 alpha 解碼正規化高度，進而混合林地、土壤與
// 岩層；Terrain 輸出本身固定不透明，因此不得把 alpha 改成 1。舊提交路徑
// 在此覆寫 alpha，會令整片地表都被判成最高高度的岩層。
// 僅正式 129x129 地表可接外環；不改原始頂點／碰撞高度場或 biome 權重。
bool AppendShowcaseTerrainApron3D(Aevrynd::Mesh3D &mesh);
[[nodiscard]] inline bool PrepareShowcaseSurfaceTerrainMesh3D(Aevrynd::Mesh3D &mesh) {
  return AppendShowcaseTerrainApron3D(mesh);
}

struct ShowcaseSceneDescriptor3D {
  ShowcaseSceneKind3D kind{};
  Aevrynd::Vec3 position{};
  Aevrynd::Vec3 scale{1, 1, 1};
  float yaw{};
  std::uint32_t lodGroup{};
  bool castsShadow{true};
  std::string meshAsset;
  std::string materialAsset;
};

enum class GameplayCottageMaterialRole3D : std::uint8_t { Brick, WoodSiding, RoofTile };
[[nodiscard]] constexpr GameplayCottageMaterialRole3D
GameplayCottageBaseMaterialRole3D(std::string_view meshAsset) noexcept {
  return meshAsset.find("foundation") != std::string_view::npos
             ? GameplayCottageMaterialRole3D::Brick
         : meshAsset.find("roof") != std::string_view::npos
             ? GameplayCottageMaterialRole3D::RoofTile
             : GameplayCottageMaterialRole3D::WoodSiding;
}
[[nodiscard]] constexpr std::string_view
GameplayCottageDescriptorMaterialAsset3D(GameplayCottageMaterialRole3D role) noexcept {
  return role == GameplayCottageMaterialRole3D::Brick
             ? "ambientcg/Bricks100/cooked-pbr"
         : role == GameplayCottageMaterialRole3D::RoofTile
             ? "ambientcg/RoofingTiles014B/cooked-pbr"
             : "ambientcg/WoodSiding006/material-pbr";
}

enum class ShowcaseTerrainBiome3D : std::uint8_t {
  Neutral,
  CottagePath,
  CanopyLeafLitter,
  RiverWetSoil
};

struct ShowcaseTerrainEcologyTint3D {
  Aevrynd::Color tint{1, 1, 1, 1};
  float canopy{}, riverMoisture{};
  // Path 與濕岸保持互斥；落葉可使用濕岸剩餘權重連續混合。
  // Keeping the weights explicit makes the rule testable while
  // canopy/riverMoisture remain backward-compatible aliases.
  float cottagePath{}, leafLitter{}, riverWetSoil{};
  ShowcaseTerrainBiome3D biome{ShowcaseTerrainBiome3D::Neutral};
};

struct ShowcaseTerrainBiomeWeights3D {
  float cottagePath{}, leafLitter{}, riverWetSoil{};
  friend constexpr bool
  operator==(const ShowcaseTerrainBiomeWeights3D &,
             const ShowcaseTerrainBiomeWeights3D &) = default;
};

// Terrain 的三層貼圖使用 world-space triplanar，正式地表原本的 UV 因而未被
// 取樣。以 signed U 區分互斥的步道／濕土、V 保存落葉權重；(0,0) 明確代表
// neutral。先夾限再正規化，惡意或插值輸入也不會令總權重超過一。
[[nodiscard]] inline ShowcaseTerrainBiomeWeights3D
NormalizeShowcaseTerrainBiomeWeights3D(
    ShowcaseTerrainBiomeWeights3D value) noexcept {
  const auto finiteWeight = [](float weight) {
    return std::isfinite(weight) ? Aevrynd::Clamp(weight, 0.f, 1.f) : 0.f;
  };
  value.cottagePath = finiteWeight(value.cottagePath);
  value.leafLitter = finiteWeight(value.leafLitter);
  value.riverWetSoil = finiteWeight(value.riverWetSoil);
  const float total =
      value.cottagePath + value.leafLitter + value.riverWetSoil;
  if (total > 1.f) {
    const float inverse = 1.f / total;
    value.cottagePath *= inverse;
    value.leafLitter *= inverse;
    value.riverWetSoil *= inverse;
  }
  return value;
}

// Path 與 wet 共用 signed U，若以 winner-takes-all 在兩個候選值的交界
// 直接換號，線性插值會由強正值跳到強負值，後續三角形修復還會把共享頂點
// 成片清零。改以候選值的有號殘差交接：兩側都連續收斂到 neutral，再由
// 另一側增強。這不是畫面空間平滑，也不改變任一候選的世界座標邊界。
[[nodiscard]] inline ShowcaseTerrainBiomeWeights3D
ResolveShowcaseTerrainPathWetHandoff3D(float cottagePathCandidate,
                                       float riverWetSoilCandidate) noexcept {
  if (!std::isfinite(cottagePathCandidate) ||
      !std::isfinite(riverWetSoilCandidate))
    return {};
  cottagePathCandidate = Aevrynd::Clamp(cottagePathCandidate, 0.f, 1.f);
  riverWetSoilCandidate = Aevrynd::Clamp(riverWetSoilCandidate, 0.f, 1.f);
  return {(std::max)(0.f, cottagePathCandidate - riverWetSoilCandidate), 0.f,
          (std::max)(0.f, riverWetSoilCandidate - cottagePathCandidate)};
}

[[nodiscard]] inline Aevrynd::Vec2 EncodeShowcaseTerrainBiomeUv3D(
    ShowcaseTerrainBiomeWeights3D value) noexcept {
  if (!std::isfinite(value.cottagePath) ||
      !std::isfinite(value.leafLitter) ||
      !std::isfinite(value.riverWetSoil))
    return {};
  value = NormalizeShowcaseTerrainBiomeWeights3D(value);
  // signed U 無法同時無損表達 path 與 wet。正式採樣本來互斥；任何違反
  // 契約的輸入整體 fail-neutral，禁止以相減掩蓋衝突資料。
  if (value.cottagePath > 0.f && value.riverWetSoil > 0.f)
    return {};
  return {value.cottagePath - value.riverWetSoil, value.leafLitter};
}

[[nodiscard]] inline Aevrynd::Vec2 EncodeShowcaseTerrainBiomeUv3D(
    const ShowcaseTerrainEcologyTint3D &value) noexcept {
  return EncodeShowcaseTerrainBiomeUv3D(
      ShowcaseTerrainBiomeWeights3D{value.cottagePath, value.leafLitter,
                                    value.riverWetSoil});
}

[[nodiscard]] inline ShowcaseTerrainBiomeWeights3D
DecodeShowcaseTerrainBiomeUv3D(Aevrynd::Vec2 encoded) noexcept {
  if (!std::isfinite(encoded.x) || !std::isfinite(encoded.y))
    return {};
  return NormalizeShowcaseTerrainBiomeWeights3D(
      {(std::max)(0.f, encoded.x), (std::max)(0.f, encoded.y),
       (std::max)(0.f, -encoded.x)});
}

struct ShowcaseTerrainDisturbance3D {
  Aevrynd::Color tint{1, 1, 1, 1};
  float heightDelta{}, weight{};
};

// 屋前步道由出生點至門廊的固定世界幾何定義；林下落葉與河岸濕土則由
// 場景中的實際樹冠、河道位置決定。不以相機或畫面空間噪聲製造假細節。
// Path 與 wet 依強度明確互斥；leaf 可填入 wet 剩餘容量，因此樹冠不會
// 把濕岸肩帶切成零值。全部權重維持於 [0,1] 且總和不超過一。呼叫端
// 會將權重寫入 Terrain 專用 UV，並保留高度場原有 alpha；不新增 descriptor
// 或 draw call。
[[nodiscard]] inline ShowcaseTerrainEcologyTint3D
SampleShowcaseTerrainEcologyTint3D(
    Aevrynd::Vec3 worldPosition,
    std::span<const ShowcaseSceneDescriptor3D> descriptors) noexcept {
  ShowcaseTerrainEcologyTint3D result{};
  if (!std::isfinite(worldPosition.x) || !std::isfinite(worldPosition.z))
    return result;
  const auto smooth = [](float value) {
    value = Aevrynd::Clamp(value, 0.f, 1.f);
    return value * value * (3.f - 2.f * value);
  };
  const auto authoredEdgeNoise = [](Aevrynd::Vec3 point) {
    // Stable world-space harmonics break up biome borders without adding a
    // texture lookup, descriptor, camera dependency, or temporal shimmer.
    return std::sin(point.x * .47f + point.z * .19f) * .10f +
           std::sin(point.x * .13f - point.z * .41f + 1.7f) * .06f;
  };
  const auto path = SampleShowcaseHeroTrail3D(worldPosition);
  // Low-frequency world-space width changes keep the trail connected while
  // avoiding a constant-width carpet.  A small signed-side bias and the
  // existing authored harmonics break the two shoulders differently without
  // changing height, geometry, descriptors, or camera-dependent state.
  const float pathTreadHalfWidth =
      ShowcaseHeroTrailTreadHalfWidth3D(worldPosition);
  // Give the authored shoulder enough world-space support to hand off to wet
  // soil without flipping the signed terrain weight between adjacent one-metre
  // vertices.  The mild power curve keeps the perceptual trail narrow and
  // guarantees the authored band is clear before two metres.
  const float pathVisibleHalfWidth =
      ShowcaseHeroTrailVisibleHalfWidth3D(worldPosition);
  const float pathShoulderWidth =
      (std::max)(.75f, pathVisibleHalfWidth - pathTreadHalfWidth);
  const float pathEdge = path.distance + authoredEdgeNoise(worldPosition) +
                         (path.signedSide >= 0.f ? .12f : -.06f);
  const float pathShoulderCoordinate =
      Aevrynd::Clamp((pathEdge - pathTreadHalfWidth) / pathShoulderWidth,
                     0.f, 1.f);
  // Reserve headroom for the signed path/wet handoff where the trail reaches
  // the river-side porch.  The old saturated core could jump from +1 to wet
  // soil in one terrain vertex even though both source fields were smooth.
  const float pathCandidate =
      .76f * std::pow(1.f - pathShoulderCoordinate, 1.2f);

  float canopyCandidate{};
  for (const auto &descriptor : descriptors) {
    if (descriptor.kind != ShowcaseSceneKind3D::Tree ||
        !std::isfinite(descriptor.position.x) ||
        !std::isfinite(descriptor.position.z) ||
        !std::isfinite(descriptor.scale.x) ||
        !std::isfinite(descriptor.scale.z))
      continue;
    const float dx = worldPosition.x - descriptor.position.x;
    const float dz = worldPosition.z - descriptor.position.z;
    const float crownRadius = (std::max)(
        2.5f, (std::max)(std::abs(descriptor.scale.x),
                         std::abs(descriptor.scale.z)) * .62f);
    const float radialDistance = std::sqrt(dx * dx + dz * dz);
    const float normalized =
        1.f - radialDistance / (crownRadius * 1.4f);
    const float canopyCore = smooth(normalized);
    // Mature trees affect more than the vertical crown projection: needles,
    // leaves and moss form a lower-density forest-floor skirt between trunks.
    // Anchor that skirt to the real descriptor and use bounded world-space
    // harmonics only to break the circular edge.  It therefore remains stable
    // across cameras and frames, reaches beyond the crown without becoming an
    // infinite biome, and reuses the existing forest PBR layer/draw.
    const float skirtRadiusMultiplier = Aevrynd::Clamp(
        2.60f + authoredEdgeNoise(worldPosition) * 2.40f, 2.20f, 2.90f);
    const float forestFloorSkirt =
        .55f * smooth(1.f - radialDistance /
                                (crownRadius * skirtRadiusMultiplier));
    canopyCandidate = (std::max)(
        canopyCandidate, (std::max)(canopyCore, forestFloorSkirt));
  }
  // The authored trail is a maintained route rather than an isolated strip of
  // road material.  Build a deterministic leaf-litter shoulder outside the
  // walkable tread so the forecourt reads as a forest clearing with a broken
  // edge, not a broad sheet of neutral dirt.  The shoulder is encoded into the
  // existing leaf layer: no descriptor, draw, collision, texture, or camera
  // dependent state is added.  The inner fade preserves the full walking lane;
  // the two swapped world axes give each side a different but stable outline.
  float trailShoulderCandidate{};
  if (path.valid) {
    const float longitudinal =
        smooth(path.along / .08f) * smooth((1.f - path.along) / .08f);
    const float inner = pathVisibleHalfWidth + .18f;
    const float shoulderNoise =
        authoredEdgeNoise({worldPosition.z, 0.f, worldPosition.x});
    const float outer = 5.35f + shoulderNoise * 3.25f;
    const float enter = smooth((path.distance - inner) / .72f);
    const float leave = 1.f - smooth((path.distance - (outer - 1.10f)) /
                                     1.10f);
    const float mottling = Aevrynd::Clamp(
        .88f + authoredEdgeNoise({worldPosition.x * 1.37f, 0.f,
                                  worldPosition.z * .83f}) * .75f,
        .70f, 1.f);
    trailShoulderCandidate =
        Aevrynd::Clamp(.82f * longitudinal * enter * leave * mottling,
                       0.f, .82f);
  }
  canopyCandidate = (std::max)(canopyCandidate, trailShoulderCandidate);
  const float riverCenter = -4.f + std::sin(worldPosition.z * .035f) * 5.f +
                            std::sin(worldPosition.z * .011f) * 2.f;
  const float riverWidth =
      12.f + std::sin(worldPosition.z * .057f + .7f) * 2.6f;
  const float bankClearance =
      std::abs(worldPosition.x - riverCenter) - riverWidth * .5f;
  const float irregularBankClearance =
      bankClearance + authoredEdgeNoise({worldPosition.z, 0, worldPosition.x}) *
                          1.8f;
  // 寬而連續的濕岸肩帶讓河水與乾土之間逐步過渡；它是世界座標資料，
  // 不依賴畫面空間模糊，也不會因相機或 phase 改變岸線位置。
  constexpr float wetBankShoulderMetres = 6.f;
  float wetSoilCandidate =
      1.f - smooth((std::max)(0.f, irregularBankClearance) /
                   wetBankShoulderMetres);

  // The maintained route reaches the west bridge landing and continues over
  // the deck to the east bank.  Compacted bridge-head soil must remain visible
  // instead of being cancelled by the river-bank candidate at full strength.
  // Keep this authored exception local: outside the 2.8 m landing apron the
  // original wet shoreline is untouched, and the river channel itself is not
  // moved or widened.
  constexpr Aevrynd::Vec3 bridgeWestLanding{-9.85f, 0.f, 14.f};
  const float bridgeLandingDistance =
      std::hypot(worldPosition.x - bridgeWestLanding.x,
                 worldPosition.z - bridgeWestLanding.z);
  const float bridgeLandingCompaction =
      1.f - smooth(bridgeLandingDistance / 2.8f);
  const float bridgeApproachCompaction =
      path.valid
          ? smooth((path.along - .40f) / .15f) *
                (1.f - smooth(path.distance / 4.5f)) *
                (.80f + .20f *
                            smooth((path.signedSide + 1.5f) / 3.f))
          : 0.f;
  wetSoilCandidate *=
      1.f - (std::max)(bridgeLandingCompaction, bridgeApproachCompaction);

  const auto pathWetHandoff = ResolveShowcaseTerrainPathWetHandoff3D(
      pathCandidate, wetSoilCandidate);
  Aevrynd::Color authoredTint{1, 1, 1, 1};
  // signed-U 無法同時表達 path/wet；以兩候選的殘差連續交接，避免在
  // 相鄰 terrain 頂點由完整 path 突然翻成完整 wet。
  if (pathWetHandoff.cottagePath > 0.f) {
    result.biome = ShowcaseTerrainBiome3D::CottagePath;
    result.cottagePath = pathWetHandoff.cottagePath;
    authoredTint = {.88f, .75f, .56f, 1.f};
  } else if (pathWetHandoff.riverWetSoil > 0.f) {
    result.biome = ShowcaseTerrainBiome3D::RiverWetSoil;
    result.riverWetSoil = result.riverMoisture =
        pathWetHandoff.riverWetSoil;
    result.leafLitter = result.canopy =
        (std::min)(canopyCandidate, 1.f - result.riverWetSoil);
    result.tint = {1.f + (.62f - 1.f) * result.riverWetSoil +
                             (.64f - 1.f) * result.leafLitter,
                   1.f + (.70f - 1.f) * result.riverWetSoil +
                             (.72f - 1.f) * result.leafLitter,
                   1.f + (.66f - 1.f) * result.riverWetSoil +
                             (.50f - 1.f) * result.leafLitter,
                   1.f};
    return result;
  } else if (canopyCandidate > 0.f) {
    result.biome = ShowcaseTerrainBiome3D::CanopyLeafLitter;
    result.leafLitter = result.canopy = canopyCandidate;
    authoredTint = {.64f, .72f, .50f, 1.f};
  }
  const float authoredWeight = result.cottagePath + result.leafLitter;
  result.tint = {1.f + (authoredTint.r - 1.f) * authoredWeight,
                 1.f + (authoredTint.g - 1.f) * authoredWeight,
                 1.f + (authoredTint.b - 1.f) * authoredWeight, 1.f};
  return result;
}

// 必須在外環建立後呼叫，確保正式網格的每個頂點都有明確 biome 資料。
// 外環不延伸遊戲區內的 authored biome；它只負責 God 鏡頭的遠景覆蓋。
inline void ApplyShowcaseTerrainBiomeUv3D(
    Aevrynd::Mesh3D &mesh, std::size_t authoredVertexCount,
    std::span<const ShowcaseSceneDescriptor3D> descriptors) {
  authoredVertexCount = (std::min)(authoredVertexCount, mesh.vertices.size());
  for (std::size_t index = 0; index < mesh.vertices.size(); ++index) {
    auto &vertex = mesh.vertices[index];
    const auto encoded = index >= authoredVertexCount
                             ? Aevrynd::Vec2{}
                             : EncodeShowcaseTerrainBiomeUv3D(
                                   SampleShowcaseTerrainEcologyTint3D(
                                       {vertex.x, vertex.y, vertex.z},
                                       descriptors));
    vertex.u = encoded.x;
    vertex.v = encoded.y;
  }
}

enum class ShowcaseTerrainBiomeUvFailure3D : std::uint8_t {
  None,
  InvalidMeshShape,
  InvalidVertexEncoding,
  InvalidWeightSum,
  NonNeutralApron,
  IndexOutOfRange
};

struct ShowcaseTerrainBiomeUvValidation3D {
  ShowcaseTerrainBiomeUvFailure3D failure{
      ShowcaseTerrainBiomeUvFailure3D::None};
  std::size_t elementIndex{};
  float minimumU{}, maximumU{};
};

[[nodiscard]] inline bool ValidateShowcaseTerrainBiomeUv3D(
    const Aevrynd::Mesh3D &mesh, std::size_t authoredVertexCount,
    ShowcaseTerrainBiomeUvValidation3D *validation = nullptr) noexcept {
  if (validation)
    *validation = {};
  const auto reject = [&](ShowcaseTerrainBiomeUvFailure3D failure,
                          std::size_t elementIndex, float minimumU = 0.f,
                          float maximumU = 0.f) {
    if (validation)
      *validation = {failure, elementIndex, minimumU, maximumU};
    return false;
  };
  if (authoredVertexCount > mesh.vertices.size() || mesh.indices.size() % 3)
    return reject(ShowcaseTerrainBiomeUvFailure3D::InvalidMeshShape, 0);
  for (std::size_t index = 0; index < mesh.vertices.size(); ++index) {
    const auto &vertex = mesh.vertices[index];
    if (!std::isfinite(vertex.u) || !std::isfinite(vertex.v) ||
        vertex.u < -1.f || vertex.u > 1.f || vertex.v < 0.f ||
        vertex.v > 1.f)
      return reject(ShowcaseTerrainBiomeUvFailure3D::InvalidVertexEncoding,
                    index, vertex.u, vertex.v);
    const auto decoded =
        DecodeShowcaseTerrainBiomeUv3D({vertex.u, vertex.v});
    const float total = decoded.cottagePath + decoded.leafLitter +
                        decoded.riverWetSoil;
    if (!std::isfinite(total) || total > 1.f + 1e-6f ||
        total < 0.f)
      return reject(ShowcaseTerrainBiomeUvFailure3D::InvalidWeightSum, index,
                    total, total);
    if (index >= authoredVertexCount && (vertex.u != 0.f || vertex.v != 0.f))
      return reject(ShowcaseTerrainBiomeUvFailure3D::NonNeutralApron, index,
                    vertex.u, vertex.v);
  }
  // signed U 可以在三角形內由 path 連續穿零轉為 wet；Decode 使用正負半軸，
  // 因此光柵插值的每一點仍然互斥。這裡只驗證索引，不得再破壞已採樣頂點。
  for (std::size_t offset = 0; offset < mesh.indices.size(); offset += 3) {
    for (unsigned corner = 0; corner < 3; ++corner) {
      const std::size_t index = mesh.indices[offset + corner];
      if (index >= mesh.vertices.size())
        return reject(ShowcaseTerrainBiomeUvFailure3D::IndexOutOfRange,
                      offset / 3, float(index), float(mesh.vertices.size()));
    }
  }
  return true;
}

// 倒木會壓低表土並在外緣推起一圈鬆土；腐殖與落葉的範圍取自實際倒木
// descriptor、尺度與朝向，而不是相機、畫面座標或時間雜訊。出生點、屋舍與
// 河床維持原碰撞／水面契約。只使用 near band，避免遠景被同一效果鋪滿。
[[nodiscard]] inline ShowcaseTerrainDisturbance3D
SampleShowcaseTerrainDisturbance3D(
    Aevrynd::Vec3 worldPosition,
    std::span<const ShowcaseSceneDescriptor3D> descriptors) noexcept {
  ShowcaseTerrainDisturbance3D result{};
  if (!std::isfinite(worldPosition.x) || !std::isfinite(worldPosition.z))
    return result;
  const auto distanceSquared = [](Aevrynd::Vec3 a, Aevrynd::Vec3 b) {
    const float x = a.x - b.x, z = a.z - b.z;
    return x * x + z * z;
  };
  if (distanceSquared(worldPosition, {-16.f, 0.f, 8.f}) <= 64.f ||
      distanceSquared(worldPosition, {-12.f, 0.f, 24.f}) <= 51.84f)
    return result;
  const float riverCenter = -4.f + std::sin(worldPosition.z * .035f) * 5.f +
                            std::sin(worldPosition.z * .011f) * 2.f;
  const float riverWidth =
      12.f + std::sin(worldPosition.z * .057f + .7f) * 2.6f;
  if (std::abs(worldPosition.x - riverCenter) <= riverWidth * .5f + 1.f)
    return result;
  const auto smooth = [](float value) {
    value = Aevrynd::Clamp(value, 0.f, 1.f);
    return value * value * (3.f - 2.f * value);
  };
  float closestRadius = 1.f;
  for (const auto &descriptor : descriptors) {
    if (descriptor.kind != ShowcaseSceneKind3D::Tree || descriptor.lodGroup != 0 ||
        descriptor.meshAsset.find("dead_tree_trunk_02") == std::string::npos ||
        !std::isfinite(descriptor.position.x) ||
        !std::isfinite(descriptor.position.z) ||
        !std::isfinite(descriptor.scale.x) ||
        !std::isfinite(descriptor.scale.z) || !std::isfinite(descriptor.yaw))
      continue;
    const float halfLength =
        Aevrynd::Clamp(std::abs(descriptor.scale.x) * .55f, 3.f, 5.f);
    const float halfWidth =
        Aevrynd::Clamp(std::abs(descriptor.scale.z) * .22f, 1.4f, 2.4f);
    const float dx = worldPosition.x - descriptor.position.x;
    const float dz = worldPosition.z - descriptor.position.z;
    const float cosine = std::cos(descriptor.yaw);
    const float sine = std::sin(descriptor.yaw);
    const float localX = cosine * dx + sine * dz;
    const float localZ = -sine * dx + cosine * dz;
    const auto ellipseRadiusSquared = [](float x, float z, float xRadius,
                                         float zRadius) {
      const float nx = x / xRadius, nz = z / zRadius;
      return nx * nx + nz * nz;
    };
    const float primary = ellipseRadiusSquared(
        localX, localZ, halfLength, halfWidth);
    const float secondary = ellipseRadiusSquared(
        localX - halfLength * .28f, localZ + halfWidth * .12f,
        halfLength * .68f, halfWidth * .82f);
    const float weight = (std::max)(smooth(1.f - primary),
                                    smooth(1.f - secondary) * .6f);
    if (weight <= result.weight) continue;
    result.weight = weight;
    closestRadius = (std::min)(primary, secondary);
  }
  if (result.weight <= 0.f) return result;
  const float radius = std::sqrt((std::max)(0.f, closestRadius));
  const float innerEdge = smooth((radius - .55f) / .17f);
  const float outerEdge = smooth((1.f - radius) / .17f);
  const float rim = innerEdge * outerEdge;
  result.heightDelta = Aevrynd::Clamp(-.055f * result.weight + .05f * rim,
                                       -.055f, .025f);
  result.tint = {1.f - result.weight * .30f,
                 1.f - result.weight * .38f,
                 1.f - result.weight * .52f, 1.f};
  return result;
}

struct ShowcaseLightingState3D {
  ShowcaseLightingProfile3D profile{};
  Aevrynd::DirectionalLight3D sun{};
  Aevrynd::Color sky{};
  Aevrynd::Color fog{};
  float exposureEv{};
  float environmentTime{};
};

struct ShowcaseCameraVisibility3D {
  std::uint32_t visibleRiverSegments{}, visibleBuildingComponents{};
  std::uint32_t buildingEdgeMask{};
  float riverCoverage{}, buildingCoverage{};
  bool buildingTouchesEdge{};
  [[nodiscard]] bool Meets(bool requireRiver, bool requireBuilding) const noexcept {
    return (!requireRiver || (visibleRiverSegments >= 2 && riverCoverage >= .001f)) &&
           (!requireBuilding || (visibleBuildingComponents == 6 &&
             buildingCoverage >= .002f && buildingCoverage <= .18f &&
             !buildingTouchesEdge));
  }
};

struct ShowcaseLodSample3D {
  std::uint64_t stableLow{};
  ShowcaseSceneKind3D kind{};
  float centerDistance{}, boundsRadius{}, surfaceDistance{}, switchDistance{},
      transitionWidth{}, nearCoverage{1.f}, farCoverage{};
  std::uint32_t nearLevel{}, farLevel{};
  bool transitioning{};
};

struct ShowcaseSubmittedLodIdentity3D {
  std::uint64_t stableLow{};
  std::uint32_t generation{1}, submesh{};
  ShowcaseSceneKind3D kind{};
  Aevrynd::Vec3 worldCenter{};
  float worldRadius{};
  std::array<float, 3> switchDistances{}, transitionWidths{};
  std::uint32_t levelCount{2}, lodSeedBase{};
};

struct ShowcaseLodRenderRow3D {
  ShowcaseSubmittedLodIdentity3D identity{};
  std::uint32_t nearLevel{}, farLevel{};
  float centerDistance{}, boundsRadius{}, surfaceDistance{}, switchDistance{},
      transitionWidth{}, nearCoverage{1.f}, farCoverage{};
  std::uint32_t lodSeed{};
  bool transitioning{};
};

struct ShowcaseLodContractReceipt3D {
  std::uint32_t expected{}, actual{}, missing{}, extra{}, wrongLevel{}, wrongBlend{};
  [[nodiscard]] constexpr bool Exact() const noexcept {
    return expected == actual && !missing && !extra && !wrongLevel && !wrongBlend;
  }
};

[[nodiscard]] inline std::vector<ShowcaseLodRenderRow3D>
BuildShowcaseLodRenderRows3D(
    const std::vector<ShowcaseLodSample3D> &samples,
    const std::vector<ShowcaseSubmittedLodIdentity3D> &submitted) {
  std::vector<ShowcaseLodRenderRow3D> rows;
  rows.reserve(submitted.size());
  for (const auto &identity : submitted) {
    const auto found = std::find_if(samples.begin(), samples.end(), [&](const auto &sample) {
      return sample.stableLow == identity.stableLow && sample.kind == identity.kind;
    });
    if (found == samples.end()) continue;
    rows.push_back({identity, found->nearLevel, found->farLevel,
                    found->centerDistance, found->boundsRadius,
                    found->surfaceDistance, found->switchDistance,
                    found->transitionWidth, found->nearCoverage,
                    found->farCoverage,
                    (identity.lodSeedBase + found->nearLevel * 3u) & 7u,
                    found->transitioning});
  }
  return rows;
}

// 直接使用成功提交的實際LOD聯集邊界，與Scene3D共用同一公開混合算法。
[[nodiscard]] inline std::vector<ShowcaseLodRenderRow3D>
BuildShowcaseLodRenderRows3D(
    const Aevrynd::Camera3D &camera,
    const std::vector<ShowcaseSubmittedLodIdentity3D> &submitted,
    Aevrynd::Vec2 viewport = {1280.f, 720.f},
    float maximumDistance = 150.f,
    float minimumProjectedRadiusPixels = .1f) {
  std::vector<ShowcaseLodRenderRow3D> rows;
  rows.reserve(submitted.size());
  const float aspect = viewport.x / (std::max)(1.f, viewport.y);
  const auto view = Aevrynd::CreateViewMatrix(camera);
  const float projectionScale = camera.projection.mode == Aevrynd::ProjectionMode::Orthographic
      ? viewport.y / (std::max)(.001f, camera.projection.orthographicHeight)
      : viewport.y * .5f / std::tan(Aevrynd::Clamp(
            camera.projection.verticalFieldOfView, .01f, 3.13f) * .5f);
  for (const auto &identity : submitted) {
    if (identity.levelCount < 2 || identity.levelCount > 4 ||
        identity.worldRadius <= 0.f)
      continue;
    const float centerDistance = Aevrynd::Length(identity.worldCenter - camera.position);
    if ((maximumDistance > 0.f && centerDistance - identity.worldRadius > maximumDistance) ||
        !Aevrynd::SphereIntersectsFrustum(identity.worldCenter, identity.worldRadius,
                                          camera, aspect))
      continue;
    const float depth = camera.projection.mode == Aevrynd::ProjectionMode::Orthographic
        ? 1.f : Aevrynd::TransformPoint(view, identity.worldCenter).z;
    const float projectedRadius = camera.projection.mode == Aevrynd::ProjectionMode::Orthographic
        ? identity.worldRadius * projectionScale
        : depth + identity.worldRadius <= .001f ? 0.f
        : depth - identity.worldRadius <= .001f
            ? (std::numeric_limits<float>::infinity)()
            : identity.worldRadius * projectionScale / depth;
    if (projectedRadius < minimumProjectedRadiusPixels) continue;
    ShowcaseLodRenderRow3D row{};
    row.identity = identity;
    row.centerDistance = centerDistance;
    row.boundsRadius = identity.worldRadius;
    row.surfaceDistance = (std::max)(0.f, row.centerDistance - row.boundsRadius);
    bool selected{};
    for (std::uint32_t level = 0; level + 1 < identity.levelCount; ++level) {
      const Aevrynd::MeshLodPolicy3D policy{
          identity.switchDistances[level], identity.transitionWidths[level],
          Aevrynd::MeshLodDistanceMetric3D::BoundsSurface};
      const auto selection = Aevrynd::EvaluateMeshLod3D(
          row.centerDistance, row.boundsRadius, policy);
      if (!selection.submitNear) continue;
      row.nearLevel = level;
      row.farLevel = level + 1;
      row.switchDistance = policy.switchDistance;
      row.transitionWidth = policy.transitionWidth;
      row.nearCoverage = selection.nearCoverage;
      row.farCoverage = selection.farCoverage;
      row.lodSeed = (identity.lodSeedBase + level * 3u) & 7u;
      row.transitioning = selection.submitFar;
      selected = true;
      break;
    }
    if (!selected) {
      row.nearLevel = row.farLevel = identity.levelCount - 1;
      row.nearCoverage = 1.f;
      row.lodSeed = (identity.lodSeedBase + row.nearLevel * 3u) & 7u;
    }
    rows.push_back(row);
  }
  return rows;
}

[[nodiscard]] inline std::vector<ShowcaseLodRenderRow3D>
BuildShowcaseActualLodRows3D(
    const std::vector<ShowcaseLodRenderRow3D> &expected,
    std::span<const Aevrynd::MeshInstance3D> instances,
    std::uint64_t identityHigh) {
  std::vector<ShowcaseLodRenderRow3D> actual;
  std::vector<bool> used(instances.size());
  actual.reserve(expected.size());
  for (const auto &wanted : expected) {
    std::size_t nearIndex = instances.size(), farIndex = instances.size();
    std::size_t mismatchedNear = instances.size(), mismatchedFar = instances.size();
    // 同一物件的多個子網格只有三位元 dither seed，seed 碰撞是合法情況。
    // 近、遠 draw 由相同的實際 coverage 共同產生，因此先配對互補 draw，
    // 不可把同 seed、coverage=1 的非過渡子網格誤認成遠端 draw。
    for (std::size_t candidateNear = 0; candidateNear < instances.size(); ++candidateNear) {
      if (used[candidateNear]) continue;
      const auto &nearInstance = instances[candidateNear];
      if (nearInstance.layer != Aevrynd::MeshRenderLayer3D::Opaque ||
          nearInstance.identity.id.high != identityHigh ||
          nearInstance.identity.id.low != wanted.identity.stableLow ||
          nearInstance.identity.generation != wanted.identity.generation ||
          (nearInstance.lodSeed & 15u) != (wanted.lodSeed | 8u) ||
          !std::isfinite(nearInstance.lodCoverage) || nearInstance.lodCoverage <= 0.f ||
          nearInstance.lodCoverage >= 1.f)
        continue;
      for (std::size_t candidateFar = 0; candidateFar < instances.size(); ++candidateFar) {
        if (used[candidateFar]) continue;
        const auto &farInstance = instances[candidateFar];
        if (farInstance.layer != Aevrynd::MeshRenderLayer3D::Opaque ||
            farInstance.identity != nearInstance.identity ||
            (farInstance.lodSeed & 15u) != wanted.lodSeed ||
            !std::isfinite(farInstance.lodCoverage) || farInstance.lodCoverage <= 0.f ||
            farInstance.lodCoverage >= 1.f)
          continue;
        if (mismatchedNear == instances.size()) {
          mismatchedNear = candidateNear;
          mismatchedFar = candidateFar;
        }
        if (std::abs(farInstance.lodCoverage - nearInstance.lodCoverage) > .00001f)
          continue;
        nearIndex = candidateNear;
        farIndex = candidateFar;
        break;
      }
      if (farIndex != instances.size()) break;
    }
    // 沒有精確互補 pair 時仍保留一筆實際錯配，讓 hostile gate 回報 wrongBlend；
    // coverage=1 的非過渡 draw 不可成為此 fallback。
    if (nearIndex == instances.size()) {
      nearIndex = mismatchedNear;
      farIndex = mismatchedFar;
    }
    if (nearIndex == instances.size() || farIndex == instances.size()) continue;
    used[nearIndex] = used[farIndex] = true;
    auto row = wanted;
    row.nearCoverage = 1.f - instances[nearIndex].lodCoverage;
    row.farCoverage = instances[farIndex].lodCoverage;
    actual.push_back(row);
  }
  return actual;
}

[[nodiscard]] inline ShowcaseLodContractReceipt3D CompareShowcaseLodRenderRows3D(
    const std::vector<ShowcaseLodRenderRow3D> &expected,
    const std::vector<ShowcaseLodRenderRow3D> &actual) noexcept {
  ShowcaseLodContractReceipt3D receipt{std::uint32_t(expected.size()),
                                       std::uint32_t(actual.size())};
  const auto sameIdentity = [](const auto &left, const auto &right) {
    return left.identity.stableLow == right.identity.stableLow &&
           left.identity.generation == right.identity.generation &&
           left.identity.submesh == right.identity.submesh &&
           left.identity.kind == right.identity.kind;
  };
  for (const auto &wanted : expected) {
    const auto found = std::find_if(actual.begin(), actual.end(),
                                    [&](const auto &row) { return sameIdentity(wanted, row); });
    if (found == actual.end()) { ++receipt.missing; continue; }
    if (wanted.nearLevel != found->nearLevel || wanted.farLevel != found->farLevel ||
        wanted.transitioning != found->transitioning) ++receipt.wrongLevel;
    if (wanted.lodSeed != found->lodSeed ||
        std::abs(wanted.centerDistance - found->centerDistance) > .00001f ||
        std::abs(wanted.boundsRadius - found->boundsRadius) > .00001f ||
        std::abs(wanted.surfaceDistance - found->surfaceDistance) > .00001f ||
        std::abs(wanted.switchDistance - found->switchDistance) > .00001f ||
        std::abs(wanted.transitionWidth - found->transitionWidth) > .00001f ||
        std::abs(wanted.nearCoverage - found->nearCoverage) > .00001f ||
        std::abs(wanted.farCoverage - found->farCoverage) > .00001f) ++receipt.wrongBlend;
  }
  for (const auto &found : actual)
    if (std::ranges::none_of(expected, [&](const auto &row) { return sameIdentity(row, found); }))
      ++receipt.extra;
  return receipt;
}

struct ShowcaseEnvironmentReceipt3D {
  std::uint64_t seed{};
  std::uint64_t worldHash{};
  std::size_t terrain{}, water{}, vegetation{}, rocks{}, buildings{};
  std::size_t trees{}, grass{}, nearVegetation{}, midVegetation{}, farVegetation{};
  float terrainSpanMeters{}, riverLengthMeters{}, riverMinimumWidthMeters{},
      riverMaximumWidthMeters{}, riverSurfaceAreaSquareMeters{};
  std::uint32_t buildingComponentMask{};
  std::uint64_t materialIdentityHash{}, licenseInventoryHash{};
  std::uint64_t landFingerprint{}, terrainZoneFingerprint{};
  std::size_t retainedDescriptorBytes{};
  std::uint64_t waterSurfaceFingerprint{};
  std::size_t riverStripSlices{}, riverStripVertices{}, riverStripIndices{}, integratedDescriptors{}, descriptorCapacity{};
  float riverMaximumJoinError{};
  std::size_t terrainMaterialLayers{}, riverbankProps{}, occupiedHabitatCells{},
      densestHabitatCell{};
  std::size_t terrainGroundingEligible{}, terrainGroundedDescriptors{};
  float terrainGroundingMaximumError{};
  bool riverUvMonotonic{}, terrainZonesExact{}, riverbankLayered{},
      propFootprintsNonOverlapping{}, terrainGroundingApplied{};
};

struct ShowcaseSceneTargets3D {
  std::size_t terrainPatches{1}, waterSegments{1}, treeInstances{296},
      grassInstances{186}, rockInstances{30}, buildingComponents{7};
  // 植被僅含樹與草；含岩石的陸地分帶另為 252／156／104。
  std::size_t nearVegetation{240}, midVegetation{144}, farVegetation{98};
  float terrainSpanMeters{512.f}, riverLengthMeters{272.f},
      riverMinimumWidthMeters{9.f}, riverMaximumWidthMeters{15.f},
      minimumRiverSurfaceAreaSquareMeters{2800.f};
  std::uint32_t requiredBuildingComponentMask{0x3fu};
  // 六十二株河岸草加二十六顆河岸岩石；另四顆移至玩家林緣且仍受實體淨空。
  std::size_t requiredTerrainMaterialLayers{3}, minimumRiverbankProps{88},
      minimumDensestHabitatCell{8};
  std::uint64_t materialIdentityHash{}, licenseInventoryHash{};
};

struct ShowcaseExecutionReceipt3D {
  std::array<std::uint64_t, 10> deltas{};
  bool executed{};
};

struct ShowcaseTerrainGroundingReceipt3D {
  std::size_t eligibleDescriptors{}, groundedDescriptors{};
  float maximumHeightError{};
  bool applied{}, allInsideTerrain{};
  [[nodiscard]] bool Complete() const noexcept {
    return applied && allInsideTerrain && eligibleDescriptors != 0 &&
           groundedDescriptors == eligibleDescriptors &&
           maximumHeightError <= .0001f;
  }
};

struct ShowcaseHeroTrailTerrainReceipt3D {
  std::size_t modifiedSamples{}, treadSamples{}, shoulderSamples{},
      cottagePadSamples{};
  float shoulderRelief{}, maximumTreadHeight{},
      maximumCottagePadHeight{};
  bool applied{};
  [[nodiscard]] bool Complete() const noexcept {
    return applied && modifiedSamples != 0 && treadSamples != 0 &&
           shoulderSamples != 0 && cottagePadSamples != 0 &&
           std::isfinite(shoulderRelief) &&
           std::isfinite(maximumTreadHeight) &&
           std::isfinite(maximumCottagePadHeight);
  }
};

// 將出生點至屋前的可走區限制在共享 HeroTrail spline，而不是以兩個
// 重疊大圓抹平整片前場。道路中心保持水平，肩帶平順接回原高度場；
// 木屋實際 footprint 另有小型 pad，避免建築懸空。
[[nodiscard]] bool SculptShowcaseHeroTrailTerrain3D(
    Aevrynd::HeightField3D &terrain,
    ShowcaseHeroTrailTerrainReceipt3D *receipt = nullptr);

struct ShowcaseReleaseReceipt3D {
  std::uint64_t worldHashBefore{};
  std::uint64_t worldHashAfter{};
  std::size_t descriptorsReleased{};
  std::size_t retainedBytesBefore{};
  std::size_t retainedBytesAfter{};
};

class GameplayShowcaseEnvironment3D {
public:
  bool Initialize(std::uint64_t seed = 0x4a17u);
  void ApplyHighProfile(Aevrynd::DrawList &draw) const;
  void ApplyLighting(Aevrynd::DrawList &draw,
                     ShowcaseLightingProfile3D profile,
                     Aevrynd::Vec3 cameraPosition) const;

  [[nodiscard]] ShowcaseLightingState3D
  EvaluateLighting(ShowcaseLightingProfile3D profile) const;
  [[nodiscard]] ShowcaseEnvironmentReceipt3D Receipt() const;
  [[nodiscard]] bool GroundLandDescriptors(
      const Aevrynd::HeightField3D &terrain,
      ShowcaseTerrainGroundingReceipt3D *receipt = nullptr);
  [[nodiscard]] static ShowcaseSceneTargets3D Targets() noexcept;
  [[nodiscard]] ShowcaseCameraVisibility3D
  EvaluateCameraVisibility(const Aevrynd::Camera3D &camera,
                           Aevrynd::Vec2 viewport = {1280, 720}) const noexcept;
  [[nodiscard]] std::vector<ShowcaseLodSample3D>
  EvaluateLodSamples(Aevrynd::Vec3 cameraPosition) const;
  [[nodiscard]] std::vector<ShowcaseLodSample3D>
  EvaluateVisibleLodSamples(const Aevrynd::Camera3D &camera,
                           float aspectRatio = 1280.f / 720.f) const;
  [[nodiscard]] ShowcaseExecutionReceipt3D
  ExecutionReceipt(const Aevrynd::RendererStats &before,
                   const Aevrynd::RendererStats &after) const;
  [[nodiscard]] ShowcaseReleaseReceipt3D Release();
  [[nodiscard]] const std::vector<ShowcaseSceneDescriptor3D> &
  Descriptors() const noexcept {
    return descriptors_;
  }
  [[nodiscard]] bool Initialized() const noexcept { return initialized_; }
  [[nodiscard]] const Aevrynd::WaterSurfaceSettings3D &WaterSurfaceSettings() const noexcept { return waterSurface_; }
  [[nodiscard]] const ShowcaseLandEcologyReceipt3D &LandReceipt() const noexcept { return landReceipt_; }

private:
  [[nodiscard]] std::uint64_t WorldHash() const;
  [[nodiscard]] std::size_t RetainedDescriptorBytes() const noexcept;

  std::vector<ShowcaseSceneDescriptor3D> descriptors_;
  std::uint64_t seed_{};
  Aevrynd::WaterSurfaceSettings3D waterSurface_{};
  ShowcaseLandEcologyReceipt3D landReceipt_{};
  ShowcaseTerrainGroundingReceipt3D groundingReceipt_{};
  bool initialized_{};
};

} // namespace GameplayLab
