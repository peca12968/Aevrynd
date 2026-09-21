#include "GameplayForestGroundCoverMesh3D.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace GameplayLab {
namespace {

using Aevrynd::Color;
using Aevrynd::Mesh3D;
using Aevrynd::Vec3;
using Aevrynd::Vertex3D;

constexpr float Pi = 3.14159265358979323846f;

std::uint32_t Mix(std::uint32_t value) noexcept {
  value ^= value >> 16;
  value *= 0x7feb352du;
  value ^= value >> 15;
  value *= 0x846ca68bu;
  return value ^ (value >> 16);
}

float UnitNoise(std::uint32_t seed, std::uint32_t item, std::uint32_t salt) noexcept {
  const auto value = Mix(seed ^ (item * 0x9e3779b9u) ^ salt);
  return float(value & 0x00ffffffu) / float(0x01000000u);
}

Color SpeciesColor(GameplayForestGroundCoverSpecies3D species,
                   float variation) noexcept {
  switch (species) {
    case GameplayForestGroundCoverSpecies3D::Fern:
      return {.94f + variation * .06f, .95f + variation * .05f,
              .93f + variation * .07f, 1.f};
    case GameplayForestGroundCoverSpecies3D::LowShrub:
      return {.93f + variation * .07f, .95f + variation * .05f,
              .92f + variation * .08f, 1.f};
    case GameplayForestGroundCoverSpecies3D::LeafMoss:
      return {.92f + variation * .08f, .90f + variation * .10f,
              .84f + variation * .12f, 1.f};
  }
  return {1.f, 1.f, 1.f, 1.f};
}

void AppendCard(Mesh3D& mesh,
                 const std::array<Vec3, 4>& points,
                 Color color,
                 bool windAffected,
                 GameplayForestGroundCoverUvRect3D uv,
                 float rootAnchorWeight = -1.f,
                 float tipAnchorWeight = -1.f) {
  const Vec3 edgeA = points[1] - points[0];
  const Vec3 edgeB = points[3] - points[0];
  Vec3 normal = Aevrynd::Normalize(Aevrynd::Cross(edgeA, edgeB));
  if (!std::isfinite(normal.x) || !std::isfinite(normal.y) ||
      !std::isfinite(normal.z) || Aevrynd::Length(normal) < .99f) {
    normal = {0, 1, 0};
  }
  const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
  const std::uint32_t packedColor = Aevrynd::PackColorRgba8(color);
  const std::array<float, 4> u{{uv.minimumU, uv.maximumU,
                                uv.maximumU, uv.minimumU}};
  const std::array<float, 4> v{{uv.maximumV, uv.maximumV,
                                uv.minimumV, uv.minimumV}};
  for (std::size_t index = 0; index < points.size(); ++index) {
    const bool explicitWeights = rootAnchorWeight >= 0.f && tipAnchorWeight >= 0.f;
    const float anchorWeight = explicitWeights
        ? (index < 2 ? rootAnchorWeight : tipAnchorWeight)
        : windAffected ? (index < 2 ? 1.f : 0.f) : 1.f;
    mesh.vertices.push_back(Vertex3D{
        points[index].x, points[index].y, points[index].z,
        Aevrynd::PackNormalSnorm8(normal, anchorWeight), packedColor,
        u[index], v[index]});
  }
  mesh.indices.insert(mesh.indices.end(),
                      {base, base + 1, base + 2, base, base + 2, base + 3});
}

unsigned CardCount(GameplayForestGroundCoverSpecies3D species,
                   GameplayForestGroundCoverDetail3D detail) noexcept {
  constexpr std::array<std::array<unsigned, 3>, 3> Counts{{
      {{28, 16, 12}},
      {{10, 6, 3}},
      {{10, 7, 3}},
  }};
  return Counts[static_cast<std::size_t>(species)]
               [static_cast<std::size_t>(detail)];
}

Mesh3D CreateFern(GameplayForestGroundCoverDetail3D detail,
                   std::uint32_t seed,
                   GameplayForestGroundCoverAtlas3D atlas) {
  Mesh3D mesh;
  const unsigned leafCount = CardCount(GameplayForestGroundCoverSpecies3D::Fern,
                                       detail);
  const unsigned stemSegments = detail == GameplayForestGroundCoverDetail3D::Near
                                    ? 8u
                                    : detail == GameplayForestGroundCoverDetail3D::Mid
                                          ? 4u : 2u;
  const unsigned cardCount = leafCount + stemSegments * 2u;
  mesh.vertices.reserve(std::size_t(cardCount) * 4);
  mesh.indices.reserve(std::size_t(cardCount) * 6);
  struct FernFrond {
    unsigned pairs{};
    unsigned segments{};
    float scale{};
    Vec3 direction{};
    Vec3 side{};
    float length{};
    float height{};
  };
  std::array<FernFrond, 3> fronds{};
  const unsigned frondCount = detail == GameplayForestGroundCoverDetail3D::Near
                                  ? 3u : 2u;
  constexpr std::array<std::array<unsigned, 3>, 3> PairPlan{{
      {{5u, 5u, 4u}}, {{4u, 4u, 0u}}, {{3u, 3u, 0u}}}};
  constexpr std::array<std::array<unsigned, 3>, 3> SegmentPlan{{
      {{3u, 3u, 2u}}, {{2u, 2u, 0u}}, {{1u, 1u, 0u}}}};
  constexpr std::array<std::array<float, 3>, 3> ScalePlan{{
      {{1.f, .90f, .80f}}, {{1.f, .88f, 0.f}}, {{1.f, .84f, 0.f}}}};
  const auto detailIndex = static_cast<std::size_t>(detail);
  const float basePhase = UnitNoise(seed, 0u, 11u) * 2.f * Pi;
  unsigned plannedPairs{};
  unsigned plannedSegments{};
  for (unsigned frond = 0; frond < frondCount; ++frond) {
    auto& geometry = fronds[frond];
    geometry.pairs = PairPlan[detailIndex][frond];
    geometry.segments = SegmentPlan[detailIndex][frond];
    geometry.scale = ScalePlan[detailIndex][frond];
    // 固定 125 度槽位讓 Near 保留三向冠幅，Mid/Far 保留前兩向；方向
    // 不依 LOD 的 frond 數重算，避免切換時整株旋轉或翻面。
    const float phase = basePhase + 2.18166156f * float(frond) +
                        (UnitNoise(seed, frond, 17u) - .5f) * .22f;
    geometry.direction = {std::cos(phase), 0, std::sin(phase)};
    geometry.side = {-geometry.direction.z, 0, geometry.direction.x};
    geometry.length = (.43f + UnitNoise(seed, frond, 23u) * .12f) *
                      geometry.scale;
    geometry.height = (.525f + UnitNoise(seed, frond, 31u) * .06f) *
                      geometry.scale;
    plannedPairs += geometry.pairs;
    plannedSegments += geometry.segments;
  }
  if (plannedPairs * 2u != leafCount || plannedSegments != stemSegments)
    return {};

  const auto stemPointAt = [](const FernFrond& frond, float t) {
    // 單調上升的四分之一弧取代倒 U；所有 frond 從同一地面根部發散，
    // 但以不同高度與方向形成真正植株，而不是一條被放大的葉枝。
    return frond.direction * (frond.length * t) +
           Vec3{0, std::sin(.5f * Pi * t) * frond.height, 0};
  };
  unsigned leafItem{};
  for (unsigned frond = 0; frond < frondCount; ++frond) {
    const auto& geometry = fronds[frond];
    const float stemEndT = float(geometry.pairs) / float(geometry.pairs + 1u);
    for (unsigned pair = 0; pair < geometry.pairs; ++pair) {
      const float t = float(pair + 1u) / float(geometry.pairs + 1u);
      const Vec3 stemPoint = stemPointAt(geometry, t);
      for (unsigned sideIndex = 0; sideIndex < 2u; ++sideIndex, ++leafItem) {
        const float handedness = sideIndex ? 1.f : -1.f;
        const Vec3 horizontalDirection = Aevrynd::Normalize(
            geometry.direction * (.22f + t * .16f) +
            geometry.side * handedness);
        const float tangentY = .30f * geometry.scale *
                               std::cos(.5f * Pi * t);
        const Vec3 leafDirection = Aevrynd::Normalize(
            horizontalDirection + Vec3{0, tangentY, 0});
        // 每支 frond 都保有連續葉序；較短的側枝只縮短莖長，不再把葉片
        // 面積同步縮到只剩細線。這是可讀群落體積，不是放大單一卡片。
        const float leafScale = .84f + .16f * geometry.scale;
        const float leafLength =
            (.12f + (1.f - t) * .105f +
             UnitNoise(seed, leafItem, 29u) * .015f) * leafScale;
        const float halfWidth =
            (.028f + (1.f - t) * .032f) * leafScale;
        const Vec3 leafSide{-horizontalDirection.z, 0,
                            horizontalDirection.x};
        const Vec3 leafTip = stemPoint + leafDirection * leafLength;
        const float rootAnchor = (std::max)(0.f, 1.f - t / stemEndT);
        const float tipAnchor = (std::max)(0.f, rootAnchor - .12f);
        AppendCard(mesh,
                   {stemPoint - leafSide * halfWidth * .22f,
                    stemPoint + leafSide * halfWidth * .22f,
                    leafTip + leafSide * halfWidth,
                    leafTip - leafSide * halfWidth},
                   SpeciesColor(GameplayForestGroundCoverSpecies3D::Fern,
                                UnitNoise(seed, leafItem, 37u)),
                   true, GameplayForestGroundCoverAtlasUv3D(atlas, leafItem),
                   rootAnchor, tipAnchor);
      }
    }
  }
  for (unsigned frond = 0; frond < frondCount; ++frond) {
    const auto& geometry = fronds[frond];
    const float stemEndT = float(geometry.pairs) / float(geometry.pairs + 1u);
    for (unsigned segment = 0; segment < geometry.segments; ++segment) {
      const float t0 = stemEndT * float(segment) / float(geometry.segments);
      const float t1 = stemEndT * float(segment + 1u) / float(geometry.segments);
      const Vec3 start = stemPointAt(geometry, t0);
      const Vec3 end = stemPointAt(geometry, t1);
      const float startHalfWidth =
          (.0042f - .0023f * (t0 / stemEndT)) * geometry.scale;
      const float endHalfWidth =
          (.0042f - .0023f * (t1 / stemEndT)) * geometry.scale;
      const float startAnchor = 1.f - t0 / stemEndT;
      const float endAnchor = 1.f - t1 / stemEndT;
      for (const Vec3 widthDirection : {geometry.side, geometry.direction}) {
        AppendCard(mesh,
                   {start - widthDirection * startHalfWidth,
                    start + widthDirection * startHalfWidth,
                    end + widthDirection * endHalfWidth,
                    end - widthDirection * endHalfWidth},
                   {.90f, 1.f, .82f, 1.f}, false,
                   GameplayForestGroundCoverStemUv3D(atlas),
                   startAnchor, endAnchor);
      }
    }
  }
  return mesh;
}

Mesh3D CreateLowShrub(GameplayForestGroundCoverDetail3D detail,
                       std::uint32_t seed,
                       GameplayForestGroundCoverAtlas3D atlas) {
  Mesh3D mesh;
  const unsigned count = CardCount(GameplayForestGroundCoverSpecies3D::LowShrub, detail);
  mesh.vertices.reserve(std::size_t(count) * 4);
  mesh.indices.reserve(std::size_t(count) * 6);
  for (unsigned item = 0; item < count; ++item) {
    // The atlas region represents one leaf, not an entire shrub.  Keep the
    // original card/triangle budget, but distribute leaf-scale cards over a
    // three-tier branch volume so the placement reads as one continuous plant
    // instead of metre-wide quads rooted along the ground plane.
    const unsigned tier = item % 3u;
    const float phase = 2.f * Pi *
                        (float(item) / float(count) +
                         UnitNoise(seed, item, 41u) * .09f);
    constexpr std::array<float, 3> RadiusBase{{.46f, .18f, .05f}};
    constexpr std::array<float, 3> RadiusVariation{{.08f, .07f, .05f}};
    constexpr std::array<float, 3> RootHeightBase{{.035f, .255f, .455f}};
    constexpr std::array<float, 3> RootHeightVariation{{.035f, .045f, .045f}};
    constexpr std::array<float, 3> LeafLengthBase{{.245f, .225f, .195f}};
    constexpr std::array<float, 3> LeafLengthVariation{{.035f, .030f, .025f}};
    constexpr std::array<float, 3> LeafWidthBase{{.145f, .125f, .105f}};
    constexpr std::array<float, 3> LeafWidthVariation{{.030f, .025f, .020f}};
    constexpr std::array<float, 3> RadialLean{{.105f, .080f, .050f}};
    const float radius = RadiusBase[tier] +
                         UnitNoise(seed, item, 43u) * RadiusVariation[tier];
    const float rootHeight = RootHeightBase[tier] +
                             UnitNoise(seed, item, 47u) * RootHeightVariation[tier];
    const float leafLength = LeafLengthBase[tier] +
                             UnitNoise(seed, item, 53u) * LeafLengthVariation[tier];
    const float leafWidth = LeafWidthBase[tier] +
                            UnitNoise(seed, item, 59u) * LeafWidthVariation[tier];
    const Vec3 radial{std::cos(phase), 0, std::sin(phase)};
    const Vec3 side{-radial.z, 0, radial.x};
    const float lean = RadialLean[tier] *
                       (.82f + UnitNoise(seed, item, 67u) * .18f);
    const float rise = std::sqrt(std::max(.0001f,
        leafLength * leafLength - lean * lean));
    const Vec3 root = radial * radius + Vec3{0, rootHeight, 0};
    const Vec3 top = root + radial * lean + Vec3{0, rise, 0};
    const Vec3 rootHalfSide = side * (leafWidth * .34f);
    const Vec3 topHalfSide = side * (leafWidth * .5f);
    AppendCard(mesh,
               {root - rootHalfSide, root + rootHalfSide,
                top + topHalfSide, top - topHalfSide},
               SpeciesColor(GameplayForestGroundCoverSpecies3D::LowShrub,
                             UnitNoise(seed, item, 61u)),
               true, GameplayForestGroundCoverAtlasUv3D(atlas, item + 2u));
  }
  return mesh;
}

Mesh3D CreateLeafMoss(GameplayForestGroundCoverDetail3D detail,
                       std::uint32_t seed,
                       GameplayForestGroundCoverAtlas3D atlas) {
  Mesh3D mesh;
  const unsigned count = CardCount(GameplayForestGroundCoverSpecies3D::LeafMoss, detail);
  mesh.vertices.reserve(std::size_t(count) * 4);
  mesh.indices.reserve(std::size_t(count) * 6);
  for (unsigned item = 0; item < count; ++item) {
    const float phase = 2.f * Pi *
                        (float(item) + UnitNoise(seed, item, 67u) * .35f) /
                        float(count);
    // atlas 子區域已是一片葉，尺寸必須維持真實落葉尺度；中心分布仍負責
    // 覆蓋裸地，不再用接近一公尺的單片平面填滿 placement。
    const float radius = .17f + UnitNoise(seed, item, 71u) * .64f;
    const float halfWidth = .06f + UnitNoise(seed, item, 73u) * .04f;
    const float halfLength = .075f + UnitNoise(seed, item, 79u) * .05f;
    const Vec3 direction{std::cos(phase), 0, std::sin(phase)};
    const Vec3 side{-direction.z, 0, direction.x};
    const Vec3 center{direction.x * radius,
                      .008f + UnitNoise(seed, item, 83u) * .025f,
                      direction.z * radius};
    const Vec3 rise{0, .012f + UnitNoise(seed, item, 89u) * .018f, 0};
    AppendCard(mesh,
               {center - side * halfWidth - direction * halfLength,
                center + side * halfWidth - direction * halfLength,
                center + side * halfWidth + direction * halfLength + rise,
                center - side * halfWidth + direction * halfLength + rise},
               SpeciesColor(GameplayForestGroundCoverSpecies3D::LeafMoss,
                             UnitNoise(seed, item, 97u)),
               false, GameplayForestGroundCoverAtlasUv3D(atlas, item + 4u));
  }
  return mesh;
}

void AppendTransformed(Mesh3D& destination, const Mesh3D& source,
                       Vec3 offset, float yaw, float scale) {
  const auto base = static_cast<std::uint32_t>(destination.vertices.size());
  const float sine = std::sin(yaw);
  const float cosine = std::cos(yaw);
  destination.vertices.reserve(destination.vertices.size() + source.vertices.size());
  destination.indices.reserve(destination.indices.size() + source.indices.size());
  for (auto vertex : source.vertices) {
    const float x = vertex.x * scale;
    const float z = vertex.z * scale;
    vertex.x = x * cosine - z * sine + offset.x;
    vertex.y = vertex.y * scale + offset.y;
    vertex.z = x * sine + z * cosine + offset.z;
    const auto normal = Aevrynd::UnpackNormalSnorm8(vertex.normal);
    vertex.normal = Aevrynd::PackNormalSnorm8(
        {normal.x * cosine - normal.z * sine, normal.y,
         normal.x * sine + normal.z * cosine},
        normal.w);
    destination.vertices.push_back(vertex);
  }
  for (const auto index : source.indices) destination.indices.push_back(base + index);
}

void TransformCardRange(Mesh3D& mesh, unsigned firstCard, unsigned cardCount,
                        Vec3 offset, float yaw, float scale) {
  const float sine = std::sin(yaw);
  const float cosine = std::cos(yaw);
  const std::size_t firstVertex = std::size_t(firstCard) * 4u;
  const std::size_t endVertex = firstVertex + std::size_t(cardCount) * 4u;
  if (endVertex > mesh.vertices.size()) return;
  for (std::size_t index = firstVertex; index < endVertex; ++index) {
    auto& vertex = mesh.vertices[index];
    const float x = vertex.x * scale;
    const float z = vertex.z * scale;
    vertex.x = x * cosine - z * sine + offset.x;
    vertex.y *= scale;
    vertex.z = x * sine + z * cosine + offset.z;
    const auto normal = Aevrynd::UnpackNormalSnorm8(vertex.normal);
    vertex.normal = Aevrynd::PackNormalSnorm8(
        {normal.x * cosine - normal.z * sine, normal.y,
         normal.x * sine + normal.z * cosine},
        normal.w);
  }
}

void SplitLeafMossColonies(Mesh3D& mesh, Vec3 firstOffset, float firstYaw,
                           float firstScale, Vec3 secondOffset,
                           float secondYaw, float secondScale) {
  const unsigned cardCount = static_cast<unsigned>(mesh.vertices.size() / 4u);
  for (unsigned card = 0; card < cardCount; ++card) {
    const bool secondColony = (card & 1u) != 0u;
    TransformCardRange(mesh, card, 1u,
                       secondColony ? secondOffset : firstOffset,
                       secondColony ? secondYaw : firstYaw,
                       secondColony ? secondScale : firstScale);
  }
}

} // namespace

Mesh3D CreateGameplayForestGroundCoverMesh3D(
    GameplayForestGroundCoverSpecies3D species,
    GameplayForestGroundCoverDetail3D detail,
    std::uint32_t variationSeed) {
  switch (species) {
    case GameplayForestGroundCoverSpecies3D::Fern:
      return CreateFern(detail, variationSeed,
                        GameplayForestGroundCoverAtlas3D::IslandTree01);
    case GameplayForestGroundCoverSpecies3D::LowShrub:
      return CreateLowShrub(detail, variationSeed,
                            GameplayForestGroundCoverAtlas3D::IslandTree01);
    case GameplayForestGroundCoverSpecies3D::LeafMoss:
      return CreateLeafMoss(detail, variationSeed,
                            GameplayForestGroundCoverAtlas3D::IslandTree01);
  }
  return {};
}

Mesh3D CreateGameplayForestGroundCoverCommunityMesh3D(
    GameplayForestGroundCoverDetail3D detail,
    std::uint32_t variationSeed) {
  Mesh3D result;
  auto fern = CreateFern(detail, Mix(variationSeed ^ 0x19a4f3cdu),
                         GameplayForestGroundCoverAtlas3D::TreeSmall02);
  const auto shrub = CreateLowShrub(detail, Mix(variationSeed ^ 0x753c28b1u),
                                    GameplayForestGroundCoverAtlas3D::TreeSmall02);
  auto leafMoss = CreateLeafMoss(detail, Mix(variationSeed ^ 0xc4d82a67u),
                                 GameplayForestGroundCoverAtlas3D::TreeSmall02);
  // Near and Mid reuse their existing cards as two asymmetric fern and
  // leaf/moss colonies.  This fills several viewing quadrants without adding a
  // descriptor, draw call, placement, collision identity or triangle.  Far is
  // deliberately left on the previous single-root layout and cost.
  if (detail == GameplayForestGroundCoverDetail3D::Near) {
    // Third frond: eight leaf cards followed by four two-sided stem cards.
    TransformCardRange(fern, 20u, 8u, {.24f, 0, .76f}, .69f, .82f);
    TransformCardRange(fern, 40u, 4u, {.24f, 0, .76f}, .69f, .82f);
    SplitLeafMossColonies(leafMoss, {-.24f, 0, .27f}, -.29f, .61f,
                           {.34f, 0, -.29f}, .53f, .66f);
  } else if (detail == GameplayForestGroundCoverDetail3D::Mid) {
    // Second frond: eight leaf cards followed by four two-sided stem cards.
    TransformCardRange(fern, 8u, 8u, {.28f, 0, .82f}, .61f, .78f);
    TransformCardRange(fern, 20u, 4u, {.28f, 0, .82f}, .61f, .78f);
    SplitLeafMossColonies(leafMoss, {-.27f, 0, .30f}, -.24f, .59f,
                           {.39f, 0, -.34f}, .47f, .63f);
  }
  // One ecology descriptor remains a single bounded mesh.  The final offsets
  // keep all four visual centres inside the original 2.3 m diameter contract.
  AppendTransformed(result, fern, {-.52f, 0, -.20f}, -.31f, .92f);
  AppendTransformed(result, shrub, {.48f, 0, .22f}, .47f, .88f);
  AppendTransformed(result, leafMoss, {.02f, 0, .02f}, .14f, 1.22f);
  return result;
}

Mesh3D CreateGameplayForestRiverbankGroundCoverMesh3D(
    GameplayForestGroundCoverDetail3D detail,
    std::uint32_t variationSeed) {
  Mesh3D result;
  const auto fern = CreateFern(detail, Mix(variationSeed ^ 0x82c6e4b9u),
                               GameplayForestGroundCoverAtlas3D::IslandTree01);
  const auto leafMoss = CreateLeafMoss(detail, Mix(variationSeed ^ 0x3d1a7f25u),
                                       GameplayForestGroundCoverAtlas3D::IslandTree01);
  // 水線向外先讀到大片低苔蘚，再由兩個方向的矮蕨葉銜接既有岩石。
  // 蕨類保留真實原始比例但縮短至約 0.55m；苔蘚只擴張水平面。
  AppendTransformed(result, leafMoss, {0, 0, 0}, .08f, 1.32f);
  AppendTransformed(result, fern, {-.12f, 0, .07f}, -.38f, .60f);
  return result;
}

GameplayForestGroundCoverLodSet3D CreateGameplayForestGroundCoverLodSet3D(
    GameplayForestGroundCoverSpecies3D species,
    std::uint32_t variationSeed) {
  return {
      CreateGameplayForestGroundCoverMesh3D(
          species, GameplayForestGroundCoverDetail3D::Near, variationSeed),
      CreateGameplayForestGroundCoverMesh3D(
          species, GameplayForestGroundCoverDetail3D::Mid, variationSeed),
      CreateGameplayForestGroundCoverMesh3D(
          species, GameplayForestGroundCoverDetail3D::Far, variationSeed),
  };
}

} // namespace GameplayLab
