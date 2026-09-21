#include "GameplayForestGroundCoverMesh3D.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

using namespace Aevrynd;
using namespace GameplayLab;

namespace {

bool Exact(const Mesh3D& left, const Mesh3D& right) {
  if (left.indices != right.indices || left.vertices.size() != right.vertices.size())
    return false;
  for (std::size_t index = 0; index < left.vertices.size(); ++index) {
    if (std::bit_cast<std::array<std::uint32_t, 7>>(left.vertices[index]) !=
        std::bit_cast<std::array<std::uint32_t, 7>>(right.vertices[index]))
      return false;
  }
  return true;
}

bool VertexContract(const Mesh3D& mesh,
                    GameplayForestGroundCoverSpecies3D species) {
  bool hasAnchor = false;
  bool hasWindTip = species == GameplayForestGroundCoverSpecies3D::LeafMoss;
  for (const auto& vertex : mesh.vertices) {
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) ||
        !std::isfinite(vertex.z) || !std::isfinite(vertex.u) ||
        !std::isfinite(vertex.v) || vertex.y < 0.f ||
        vertex.u < 0.f || vertex.u > 1.f ||
        vertex.v < 0.f || vertex.v > 1.f || (vertex.color >> 24) != 255u)
      return false;
    const auto normal = UnpackNormalSnorm8(vertex.normal);
    const float normalLength = Length(Vec3{normal.x, normal.y, normal.z});
    if (!std::isfinite(normalLength) || normalLength < .985f || normalLength > 1.015f)
      return false;
    hasAnchor |= normal.w > .98f;
    hasWindTip |= normal.w < .02f;
  }
  return hasAnchor && hasWindTip;
}

bool AtlasUvContract(const Mesh3D& mesh,
                     GameplayForestGroundCoverAtlas3D atlas) {
  if (mesh.vertices.empty()) return false;
  for (const auto& vertex : mesh.vertices) {
    bool knownRegion = false;
    for (std::uint32_t region = 0; region < 5; ++region) {
      const auto uv = GameplayForestGroundCoverAtlasUv3D(atlas, region);
      const float middleV = uv.minimumV +
                            (uv.maximumV - uv.minimumV) * .48f;
      knownRegion |= (vertex.u == uv.minimumU || vertex.u == uv.maximumU) &&
                     (vertex.v == uv.minimumV || vertex.v == uv.maximumV ||
                      vertex.v == middleV);
    }
    const auto stemUv = GameplayForestGroundCoverStemUv3D(atlas);
    knownRegion |= (vertex.u == stemUv.minimumU || vertex.u == stemUv.maximumU) &&
                   (vertex.v == stemUv.minimumV || vertex.v == stemUv.maximumV);
    if (!knownRegion || vertex.u == 0.f || vertex.u == 1.f ||
        vertex.v == 0.f || vertex.v == 1.f)
      return false;
  }
  return true;
}

bool LowShrubTierContract(const Mesh3D& mesh) {
  if (mesh.vertices.size() < 12 || mesh.vertices.size() % 4 != 0)
    return false;
  std::array<unsigned, 3> counts{};
  std::array<float, 3> radiusSums{};
  for (std::size_t base = 0; base < mesh.vertices.size(); base += 4) {
    const auto& rootA = mesh.vertices[base];
    const auto& rootB = mesh.vertices[base + 1];
    const auto& topA = mesh.vertices[base + 2];
    const auto& topB = mesh.vertices[base + 3];
    if (std::abs(rootA.y - rootB.y) > 1e-6f ||
        std::abs(topA.y - topB.y) > 1e-6f || topA.y <= rootA.y)
      return false;
    const float rootY = (rootA.y + rootB.y) * .5f;
    const float topY = (topA.y + topB.y) * .5f;
    const unsigned tier = rootY < .15f ? 0u : rootY < .38f ? 1u : 2u;
    const float rootX = (rootA.x + rootB.x) * .5f;
    const float rootZ = (rootA.z + rootB.z) * .5f;
    const float topX = (topA.x + topB.x) * .5f;
    const float topZ = (topA.z + topB.z) * .5f;
    const float width = Length(Vec3{topA.x - topB.x, topA.y - topB.y,
                                    topA.z - topB.z});
    const float length = Length(Vec3{topX - rootX, topY - rootY,
                                     topZ - rootZ});
    constexpr std::array<float, 3> MinTipY{{.24f, .43f, .62f}};
    constexpr std::array<float, 3> MaxTipY{{.36f, .55f, .73f}};
    if (topY < MinTipY[tier] || topY > MaxTipY[tier] ||
        width < .09f || width > .18f ||
        length < .18f || length > .29f)
      return false;
    ++counts[tier];
    radiusSums[tier] += std::sqrt(rootX * rootX + rootZ * rootZ);
  }
  if (!counts[0] || !counts[1] || !counts[2]) return false;
  const float outerRadius = radiusSums[0] / float(counts[0]);
  const float crownRadius = radiusSums[2] / float(counts[2]);
  return outerRadius >= crownRadius + .36f;
}

bool FernClusterContract(const Mesh3D& mesh,
                         GameplayForestGroundCoverDetail3D detail) {
  const auto fail = [detail](const char* reason) {
    std::cerr << "FERN_CLUSTER_CONTRACT=FAIL detail="
              << static_cast<unsigned>(detail) << " reason=" << reason << '\n';
    return false;
  };
  if (mesh.vertices.empty() || mesh.vertices.size() % 4 != 0)
    return fail("vertex-layout");
  constexpr std::array<unsigned, 3> LeafCards{{28u, 16u, 12u}};
  constexpr std::array<unsigned, 3> FrondCounts{{3u, 2u, 2u}};
  constexpr std::array<std::array<unsigned, 3>, 3> PairPlan{{
      {{5u, 5u, 4u}}, {{4u, 4u, 0u}}, {{3u, 3u, 0u}}}};
  constexpr std::array<std::array<unsigned, 3>, 3> SegmentPlan{{
      {{3u, 3u, 2u}}, {{2u, 2u, 0u}}, {{1u, 1u, 0u}}}};
  const auto detailIndex = static_cast<std::size_t>(detail);
  const std::size_t leafVertexCount = std::size_t(LeafCards[detailIndex]) * 4u;
  const unsigned frondCount = FrondCounts[detailIndex];
  if (mesh.vertices.size() <= leafVertexCount) return fail("leaf-layout");
  const auto center = [&mesh](std::size_t base, bool tip) {
    const std::size_t offset = tip ? 2u : 0u;
    return Vec3{
        (mesh.vertices[base + offset].x +
         mesh.vertices[base + offset + 1u].x) * .5f,
        (mesh.vertices[base + offset].y +
         mesh.vertices[base + offset + 1u].y) * .5f,
        (mesh.vertices[base + offset].z +
         mesh.vertices[base + offset + 1u].z) * .5f};
  };
  for (std::size_t base = 0; base < mesh.vertices.size(); base += 4) {
    const auto& rootA = mesh.vertices[base];
    const auto& rootB = mesh.vertices[base + 1];
    const auto& tipA = mesh.vertices[base + 2];
    const auto& tipB = mesh.vertices[base + 3];
    if (rootA.y < 0.f || rootB.y < 0.f || tipA.y < 0.f || tipB.y < 0.f ||
        rootA.y > .66f || rootB.y > .66f || tipA.y > .66f || tipB.y > .66f)
      return fail("finite-height");
    const float rootWidth = Length(Vec3{rootA.x - rootB.x, 0,
                                        rootA.z - rootB.z});
    const float tipWidth = Length(Vec3{tipA.x - tipB.x, 0,
                                       tipA.z - tipB.z});
    const float rootWidth3D = Length(Vec3{rootA.x - rootB.x,
                                          rootA.y - rootB.y,
                                          rootA.z - rootB.z});
    const float tipWidth3D = Length(Vec3{tipA.x - tipB.x,
                                         tipA.y - tipB.y,
                                         tipA.z - tipB.z});
    const float rootX = (rootA.x + rootB.x) * .5f;
    const float rootZ = (rootA.z + rootB.z) * .5f;
    const float tipX = (tipA.x + tipB.x) * .5f;
    const float tipZ = (tipA.z + tipB.z) * .5f;
    const float cardLength = Length(Vec3{tipX - rootX, tipA.y - rootA.y,
                                         tipZ - rootZ});
    const bool stem = base >= leafVertexCount;
    if (stem) {
      if (rootWidth3D < .0025f ||
          rootWidth3D > .0095f || tipWidth3D < .0025f ||
          tipWidth3D > .0095f ||
          cardLength < .05f || cardLength > .70f) {
        std::cerr << "FERN_STEM_SHAPE base=" << base
                  << " rootWidth=" << rootWidth3D
                  << " tipWidth=" << tipWidth3D
                  << " length=" << cardLength << '\n';
        return fail("stem-shape");
      }
    } else {
      if (base >= leafVertexCount ||
          std::abs(rootA.y - rootB.y) > 1e-6f ||
          std::abs(tipA.y - tipB.y) > 1e-6f ||
          rootWidth < .007f || rootWidth > .028f ||
          tipWidth < .032f || tipWidth > .122f ||
          cardLength < .08f || cardLength > .25f || tipWidth <= rootWidth ||
          std::abs(tipA.y - rootA.y) > .08f) {
        std::cerr << "FERN_LEAF_SHAPE base=" << base
                  << " rootWidth=" << rootWidth
                  << " tipWidth=" << tipWidth
                  << " length=" << cardLength
                  << " deltaY=" << std::abs(tipA.y - rootA.y) << '\n';
        return fail("leaf-shape");
      }
    }
  }
  // 每兩張卡片必須由同一個莖節向左右展開；這個幾何條件防止日後
  // 回退成沿莖交錯、低鏡頭只剩零碎浮片的配置。
  for (std::size_t base = 0; base + 7 < leafVertexCount; base += 8) {
    const Vec3 rootLeft = center(base, false);
    const Vec3 rootRight = center(base + 4u, false);
    const Vec3 tipLeft = center(base, true);
    const Vec3 tipRight = center(base + 4u, true);
    const float rootWeightLeft = UnpackNormalSnorm8(mesh.vertices[base].normal).w;
    const float rootWeightRight = UnpackNormalSnorm8(mesh.vertices[base + 4].normal).w;
    const float tipWeightLeft = UnpackNormalSnorm8(mesh.vertices[base + 2].normal).w;
    const float tipWeightRight = UnpackNormalSnorm8(mesh.vertices[base + 6].normal).w;
    if (Length(rootLeft - rootRight) > 1e-5f ||
        Length(tipLeft - tipRight) < .10f ||
        std::abs(rootWeightLeft - rootWeightRight) > .02f ||
        std::abs(tipWeightLeft - tipWeightRight) > .02f ||
        tipWeightLeft > rootWeightLeft + .02f)
      return fail("leaf-pair");
  }
  // 每支 frond 都從完全相同的地面根點開始，莖段在該支內連續並精確
  // 接到最後一對葉節；不同 frond 的終端方向必須真正分開。
  std::array<Vec3, 3> directions{};
  std::size_t leafPairOffset{};
  std::size_t stemBase = leafVertexCount;
  Vec3 commonRoot{};
  for (unsigned frond = 0; frond < frondCount; ++frond) {
    Vec3 previousEnd{};
    float previousEndWeight = 1.f;
    for (unsigned segment = 0; segment < SegmentPlan[detailIndex][frond];
         ++segment, stemBase += 8u) {
      const Vec3 startA = center(stemBase, false);
      const Vec3 endA = center(stemBase, true);
      const Vec3 startB = center(stemBase + 4u, false);
      const Vec3 endB = center(stemBase + 4u, true);
      const float startWeightA =
          UnpackNormalSnorm8(mesh.vertices[stemBase].normal).w;
      const float endWeightA =
          UnpackNormalSnorm8(mesh.vertices[stemBase + 2u].normal).w;
      const float startWeightB =
          UnpackNormalSnorm8(mesh.vertices[stemBase + 4u].normal).w;
      const float endWeightB =
          UnpackNormalSnorm8(mesh.vertices[stemBase + 6u].normal).w;
      if (Length(startA - startB) > 1e-5f ||
          Length(endA - endB) > 1e-5f ||
          (segment == 0u && startA.y > 1e-6f) ||
          (segment == 0u && frond > 0u && Length(startA - commonRoot) > 1e-5f) ||
          (segment > 0u && Length(startA - previousEnd) > 1e-5f) ||
          std::abs(startWeightA - startWeightB) > .02f ||
          std::abs(endWeightA - endWeightB) > .02f ||
          endWeightA > startWeightA + .02f ||
          (segment > 0u &&
           std::abs(startWeightA - previousEndWeight) > .02f))
        return fail("stem-chain");
      if (segment == 0u) commonRoot = startA;
      previousEnd = endA;
      previousEndWeight = endWeightA;
    }
    const std::size_t lastLeafPair =
        leafPairOffset + PairPlan[detailIndex][frond] - 1u;
    const Vec3 lastLeafRoot = center(lastLeafPair * 8u, false);
    if (Length(previousEnd - lastLeafRoot) > 1e-5f ||
        previousEndWeight > .02f)
      return fail("stem-terminal");
    directions[frond] = Aevrynd::Normalize(
        Vec3{previousEnd.x - commonRoot.x, 0,
             previousEnd.z - commonRoot.z});
    leafPairOffset += PairPlan[detailIndex][frond];
  }
  if (stemBase != mesh.vertices.size() || leafPairOffset * 8u != leafVertexCount)
    return fail("topology-count");
  for (unsigned left = 0; left < frondCount; ++left)
    for (unsigned right = left + 1u; right < frondCount; ++right)
      if (Aevrynd::Dot(directions[left], directions[right]) > .26f)
        return fail("direction-collapse");
  const auto bounds = mesh.Bounds();
  if (bounds.center.y - bounds.extents.y < -1e-6f ||
      bounds.center.y + bounds.extents.y < .48f ||
      bounds.center.y + bounds.extents.y > .63f ||
      bounds.extents.x > .75f || bounds.extents.z > .75f ||
      bounds.extents.x < .18f || bounds.extents.z < .18f) {
    std::cerr << "FERN_BOUNDS minY=" << bounds.center.y - bounds.extents.y
              << " maxY=" << bounds.center.y + bounds.extents.y
              << " extentX=" << bounds.extents.x
              << " extentZ=" << bounds.extents.z << '\n';
    return fail("bounds");
  }
  return true;
}

bool LeafMossCardScaleContract(const Mesh3D& mesh) {
  if (mesh.vertices.empty() || mesh.vertices.size() % 4 != 0) return false;
  for (std::size_t base = 0; base < mesh.vertices.size(); base += 4) {
    const auto& a = mesh.vertices[base];
    const auto& b = mesh.vertices[base + 1];
    const auto& d = mesh.vertices[base + 3];
    const float width = Length(Vec3{a.x - b.x, 0, a.z - b.z});
    const float length = Length(Vec3{a.x - d.x, 0, a.z - d.z});
    if (width < .12f || width > .21f || length < .15f || length > .26f)
      return false;
  }
  return true;
}

bool SameTint(Color left, Color right) {
  return left.r == right.r && left.g == right.g && left.b == right.b &&
         left.a == right.a;
}

} // namespace

int main() {
  constexpr Vec3 ecologyScale{.42f, .31f, .78f};
  constexpr auto metreAuthoredScale =
      GameplayForestGroundCoverWorldScale3D(ecologyScale);
  static_assert(metreAuthoredScale.x == ecologyScale.x &&
                metreAuthoredScale.y == 1.f &&
                metreAuthoredScale.z == ecologyScale.z);
  constexpr auto contract = GameplayForestGroundCoverAlphaCardContract();
  static_assert(contract.alphaMode == MaterialAlphaMode3D::Mask);
  static_assert(contract.alphaCutoff == .45f);
  static_assert(contract.requiresTexture && contract.requiresUv &&
                contract.doubleSided && contract.atlasColumns == 1);
  Material3D source{};
  source.textures = {7u, 11u};
  const auto prepared = PrepareGameplayForestGroundCoverMaterial3D(source);
  if (prepared.textures != source.textures ||
      prepared.materialClass != MaterialClass3D::Foliage ||
      prepared.alphaMode != MaterialAlphaMode3D::Mask ||
      prepared.alphaCutoff != .45f || !prepared.useUv ||
      !prepared.doubleSided || prepared.castsShadow)
    return 6;

  constexpr std::array groundCoverVariants{
      GameplayForestGroundCoverVariant3D::Fern,
      GameplayForestGroundCoverVariant3D::Riverbank,
      GameplayForestGroundCoverVariant3D::Community};
  std::array<Color, groundCoverVariants.size()> multipliers{};
  source.tint = {.8f, .7f, .6f, .4f};
  for (std::size_t index = 0; index < groundCoverVariants.size(); ++index) {
    const auto multiplier =
        GameplayForestGroundCoverTintMultiplier3D(groundCoverVariants[index]);
    multipliers[index] = multiplier;
    const auto tinted = PrepareGameplayForestGroundCoverMaterial3D(
        source, groundCoverVariants[index], true);
    if (!std::isfinite(multiplier.r) || !std::isfinite(multiplier.g) ||
        !std::isfinite(multiplier.b) || !std::isfinite(multiplier.a) ||
        multiplier.r < 0.f || multiplier.r > 1.f ||
        multiplier.g < 0.f || multiplier.g > 1.f ||
        multiplier.b < 0.f || multiplier.b > 1.f || multiplier.a != 1.f ||
        tinted.tint.r != source.tint.r * multiplier.r ||
        tinted.tint.g != source.tint.g * multiplier.g ||
        tinted.tint.b != source.tint.b * multiplier.b ||
        tinted.tint.a != source.tint.a || tinted.textures != source.textures ||
        tinted.materialClass != MaterialClass3D::Foliage ||
        tinted.alphaMode != MaterialAlphaMode3D::Mask ||
        tinted.alphaCutoff != .45f || !tinted.useUv || !tinted.doubleSided ||
        !tinted.castsShadow || tinted.surface.roughness != .86f ||
        tinted.surface.metallic != 0.f || tinted.surface.emissive != 0.f ||
        tinted.normalScale != .84f || tinted.textureScale != 1.f) {
      std::cerr << "FOREST_GROUND_COVER_TINT_CONTRACT=FAIL\n";
      return 11;
    }
  }
  if (SameTint(multipliers[0], multipliers[1]) ||
      SameTint(multipliers[0], multipliers[2]) ||
      SameTint(multipliers[1], multipliers[2])) {
    std::cerr << "FOREST_GROUND_COVER_TINT_VARIANTS=FAIL\n";
    return 12;
  }

  constexpr std::array species{
      GameplayForestGroundCoverSpecies3D::Fern,
      GameplayForestGroundCoverSpecies3D::LowShrub,
      GameplayForestGroundCoverSpecies3D::LeafMoss};
  constexpr std::array details{
      GameplayForestGroundCoverDetail3D::Near,
      GameplayForestGroundCoverDetail3D::Mid,
      GameplayForestGroundCoverDetail3D::Far};
  constexpr std::array<std::size_t, 3> expectedLowShrubTriangles{{20, 12, 6}};
  constexpr std::array<std::size_t, 3> expectedFernTriangles{{88, 48, 32}};

  for (const auto item : species) {
    std::size_t previousTriangles = (std::numeric_limits<std::size_t>::max)();
    for (std::size_t detailIndex = 0; detailIndex < details.size(); ++detailIndex) {
      const auto detail = details[detailIndex];
      const auto mesh = CreateGameplayForestGroundCoverMesh3D(item, detail, 0x51a7c3u);
      const auto repeated = CreateGameplayForestGroundCoverMesh3D(item, detail, 0x51a7c3u);
      const std::size_t triangles = mesh.indices.size() / 3;
      if (!mesh.Valid() || !Exact(mesh, repeated) ||
          triangles > GameplayForestGroundCoverTriangleBudget(detail) ||
          triangles >= previousTriangles || !VertexContract(mesh, item) ||
          (item == GameplayForestGroundCoverSpecies3D::Fern &&
           triangles != expectedFernTriangles[detailIndex]) ||
          !AtlasUvContract(mesh,
                           GameplayForestGroundCoverAtlas3D::IslandTree01) ||
          (item == GameplayForestGroundCoverSpecies3D::Fern &&
           !FernClusterContract(mesh, detail)) ||
          (item == GameplayForestGroundCoverSpecies3D::LeafMoss &&
           !LeafMossCardScaleContract(mesh)) ||
          (item == GameplayForestGroundCoverSpecies3D::LowShrub &&
           (triangles != expectedLowShrubTriangles[detailIndex] ||
            !LowShrubTierContract(mesh)))) {
        std::cerr << "FOREST_GROUND_COVER_SPECIES_CONTRACT=FAIL species="
                  << static_cast<unsigned>(item) << " detail=" << detailIndex
                  << " triangles=" << triangles
                  << " valid=" << mesh.Valid()
                  << " vertex=" << VertexContract(mesh, item)
                  << " atlas=" << AtlasUvContract(
                         mesh, GameplayForestGroundCoverAtlas3D::IslandTree01)
                  << " fern=" << (item != GameplayForestGroundCoverSpecies3D::Fern ||
                                      FernClusterContract(mesh, detail))
                  << " leaf=" << (item != GameplayForestGroundCoverSpecies3D::LeafMoss ||
                                      LeafMossCardScaleContract(mesh)) << '\n';
        return 1;
      }
      for (std::size_t triangle = 0; triangle < mesh.indices.size(); triangle += 3) {
        const auto& a = mesh.vertices[mesh.indices[triangle]];
        const auto& b = mesh.vertices[mesh.indices[triangle + 1]];
        const auto& c = mesh.vertices[mesh.indices[triangle + 2]];
        if (Length(Cross(Vec3{b.x - a.x, b.y - a.y, b.z - a.z},
                         Vec3{c.x - a.x, c.y - a.y, c.z - a.z})) <= 1e-6f) {
          std::cerr << "DEGENERATE_ALPHA_CARD=FAIL\n";
          return 2;
        }
      }
      previousTriangles = triangles;
    }
    const auto baseline = CreateGameplayForestGroundCoverMesh3D(
        item, GameplayForestGroundCoverDetail3D::Near, 11u);
    const auto alternate = CreateGameplayForestGroundCoverMesh3D(
        item, GameplayForestGroundCoverDetail3D::Near, 12u);
    if (Exact(baseline, alternate) ||
        baseline.indices.size() != alternate.indices.size() ||
        baseline.vertices.size() != alternate.vertices.size()) {
      std::cerr << "VARIATION_SEED_CONTRACT=FAIL\n";
      return 3;
    }
    const auto lod = CreateGameplayForestGroundCoverLodSet3D(item, 0x51a7c3u);
    if (!Exact(lod.nearMesh, CreateGameplayForestGroundCoverMesh3D(
                                 item, GameplayForestGroundCoverDetail3D::Near,
                                 0x51a7c3u)) ||
        lod.nearMesh.indices.size() <= lod.midMesh.indices.size() ||
        lod.midMesh.indices.size() <= lod.farMesh.indices.size()) {
      std::cerr << "LOD_SET_CONTRACT=FAIL\n";
      return 4;
    }
  }

  const auto fernNear = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::Fern,
      GameplayForestGroundCoverDetail3D::Near, 0x51a7c3u);
  const auto fernMid = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::Fern,
      GameplayForestGroundCoverDetail3D::Mid, 0x51a7c3u);
  const auto fernFar = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::Fern,
      GameplayForestGroundCoverDetail3D::Far, 0x51a7c3u);
  const auto leafRoot = [](const Mesh3D& mesh, std::size_t pair) {
    const std::size_t base = pair * 8u;
    return Vec3{(mesh.vertices[base].x + mesh.vertices[base + 1u].x) * .5f,
                (mesh.vertices[base].y + mesh.vertices[base + 1u].y) * .5f,
                (mesh.vertices[base].z + mesh.vertices[base + 1u].z) * .5f};
  };
  const auto horizontalDirection = [](Vec3 value) {
    value.y = 0.f;
    return Normalize(value);
  };
  constexpr std::array<std::size_t, 2> nearTerminals{{4u, 9u}};
  constexpr std::array<std::size_t, 2> midTerminals{{3u, 7u}};
  constexpr std::array<std::size_t, 2> farTerminals{{2u, 5u}};
  for (std::size_t frond = 0; frond < 2u; ++frond) {
    const auto nearDirection = horizontalDirection(
        leafRoot(fernNear, nearTerminals[frond]));
    const auto midDirection = horizontalDirection(
        leafRoot(fernMid, midTerminals[frond]));
    const auto farDirection = horizontalDirection(
        leafRoot(fernFar, farTerminals[frond]));
    if (Dot(nearDirection, midDirection) < .999f ||
        Dot(nearDirection, farDirection) < .999f) {
      std::cerr << "FERN_LOD_DIRECTION_STABILITY=FAIL\n";
      return 13;
    }
  }

  auto detachedRoot = fernNear;
  for (const std::size_t vertex : {136u, 137u, 140u, 141u})
    detachedRoot.vertices[vertex].x += .04f;
  if (FernClusterContract(detachedRoot,
                          GameplayForestGroundCoverDetail3D::Near)) {
    std::cerr << "FERN_COMMON_ROOT_HOSTILE=FAIL\n";
    return 14;
  }

  auto severedStem = fernNear;
  for (const std::size_t vertex : {120u, 121u, 124u, 125u})
    severedStem.vertices[vertex].z += .03f;
  if (FernClusterContract(severedStem,
                          GameplayForestGroundCoverDetail3D::Near)) {
    std::cerr << "FERN_SEVERED_STEM_HOSTILE=FAIL\n";
    return 15;
  }

  auto oversizedCrown = fernNear;
  for (auto& vertex : oversizedCrown.vertices) {
    vertex.x *= 2.f;
    vertex.z *= 2.f;
  }
  if (FernClusterContract(oversizedCrown,
                          GameplayForestGroundCoverDetail3D::Near)) {
    std::cerr << "FERN_OVERSIZED_CROWN_HOSTILE=FAIL\n";
    return 16;
  }

  auto reversedWind = fernNear;
  for (const std::size_t vertex : {120u, 121u, 124u, 125u}) {
    const auto normal = UnpackNormalSnorm8(reversedWind.vertices[vertex].normal);
    reversedWind.vertices[vertex].normal =
        PackNormalSnorm8({normal.x, normal.y, normal.z}, 1.f);
  }
  if (FernClusterContract(reversedWind,
                          GameplayForestGroundCoverDetail3D::Near)) {
    std::cerr << "FERN_WIND_MONOTONIC_HOSTILE=FAIL\n";
    return 17;
  }

  auto collapsedFronds = fernNear;
  const auto firstDirection = horizontalDirection(leafRoot(fernNear, 4u));
  const auto secondDirection = horizontalDirection(leafRoot(fernNear, 9u));
  const float rotation = std::atan2(firstDirection.z, firstDirection.x) -
                         std::atan2(secondDirection.z, secondDirection.x);
  const float rotationSine = std::sin(rotation);
  const float rotationCosine = std::cos(rotation);
  const auto rotateVertex = [&](std::size_t vertexIndex) {
    auto& vertex = collapsedFronds.vertices[vertexIndex];
    const float x = vertex.x;
    const float z = vertex.z;
    vertex.x = x * rotationCosine - z * rotationSine;
    vertex.z = x * rotationSine + z * rotationCosine;
  };
  for (std::size_t vertex = 40u; vertex < 80u; ++vertex)
    rotateVertex(vertex);
  for (std::size_t vertex = 136u; vertex < 160u; ++vertex)
    rotateVertex(vertex);
  if (FernClusterContract(collapsedFronds,
                          GameplayForestGroundCoverDetail3D::Near)) {
    std::cerr << "FERN_COLLAPSED_DIRECTIONS_HOSTILE=FAIL\n";
    return 18;
  }

  std::size_t previousCommunityTriangles =
      (std::numeric_limits<std::size_t>::max)();
  constexpr std::array<std::size_t, 3> expectedCommunityTriangles{{128, 74, 44}};
  for (std::size_t detailIndex = 0; detailIndex < details.size(); ++detailIndex) {
    const auto detail = details[detailIndex];
    const auto community =
        CreateGameplayForestGroundCoverCommunityMesh3D(detail, 0xf04e57u);
    const auto repeated =
        CreateGameplayForestGroundCoverCommunityMesh3D(detail, 0xf04e57u);
    const auto bounds = community.Bounds();
    const std::size_t triangles = community.indices.size() / 3;
    if (!community.Valid() || !Exact(community, repeated) ||
        !AtlasUvContract(community,
                         GameplayForestGroundCoverAtlas3D::TreeSmall02) ||
        triangles > GameplayForestGroundCoverCommunityTriangleBudget(detail) ||
        triangles != expectedCommunityTriangles[detailIndex] ||
        triangles >= previousCommunityTriangles ||
        bounds.extents.x < .78f || bounds.extents.z < .72f ||
        bounds.extents.x > 1.15f || bounds.extents.z > 1.15f ||
        bounds.center.y - bounds.extents.y < -1e-6f ||
        bounds.center.y + bounds.extents.y > .82f) {
      std::cerr << "FOREST_GROUND_COVER_COMMUNITY_CONTRACT=FAIL detail="
                << detailIndex << " triangles=" << triangles
                << " extents=" << bounds.extents.x << ',' << bounds.extents.y
                << ',' << bounds.extents.z << " centerY=" << bounds.center.y
                << '\n';
      return 5;
    }
    previousCommunityTriangles = triangles;
  }

  std::size_t previousRiverbankTriangles =
      (std::numeric_limits<std::size_t>::max)();
  constexpr std::array<std::size_t, 3> expectedRiverbankTriangles{{108, 62, 38}};
  for (std::size_t detailIndex = 0; detailIndex < details.size(); ++detailIndex) {
    const auto detail = details[detailIndex];
    const auto riverbank =
        CreateGameplayForestRiverbankGroundCoverMesh3D(detail, 0x31b7u);
    const auto repeated =
        CreateGameplayForestRiverbankGroundCoverMesh3D(detail, 0x31b7u);
    const auto bounds = riverbank.Bounds();
    const std::size_t triangles = riverbank.indices.size() / 3;
    if (!riverbank.Valid() || !Exact(riverbank, repeated) ||
        !AtlasUvContract(riverbank,
                         GameplayForestGroundCoverAtlas3D::IslandTree01) ||
        triangles != expectedRiverbankTriangles[detailIndex] ||
        triangles > GameplayForestRiverbankGroundCoverTriangleBudget(detail) ||
        triangles >= previousRiverbankTriangles ||
        bounds.extents.x < .55f || bounds.extents.z < .55f ||
        bounds.center.y - bounds.extents.y < -1e-6f ||
        bounds.center.y + bounds.extents.y > .61f) {
      std::cerr << "FOREST_RIVERBANK_GROUND_COVER_CONTRACT=FAIL detail="
                << detailIndex << " triangles=" << triangles
                << " extents=" << bounds.extents.x << ',' << bounds.extents.y
                << ',' << bounds.extents.z << " centerY=" << bounds.center.y
                << " atlas=" << AtlasUvContract(
                       riverbank, GameplayForestGroundCoverAtlas3D::IslandTree01)
                << '\n';
      return 9;
    }
    previousRiverbankTriangles = triangles;
  }

  const auto leafMoss = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::LeafMoss,
      GameplayForestGroundCoverDetail3D::Near, 0x51a7c3u);
  const auto leafMossBounds = leafMoss.Bounds();
  if (leafMossBounds.extents.x < .55f || leafMossBounds.extents.z < .55f ||
      leafMossBounds.center.y + leafMossBounds.extents.y > .07f) {
    std::cerr << "LEAF_MOSS_HORIZONTAL_COVERAGE=FAIL\n";
    return 10;
  }

  auto flattenedShrub = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::LowShrub,
      GameplayForestGroundCoverDetail3D::Near, 0x51a7c3u);
  for (std::size_t base = 0; base < flattenedShrub.vertices.size(); base += 4) {
    flattenedShrub.vertices[base + 2].y = .61f;
    flattenedShrub.vertices[base + 3].y = .61f;
  }
  if (LowShrubTierContract(flattenedShrub)) {
    std::cerr << "LOW_SHRUB_FLATTENED_HOSTILE=FAIL\n";
    return 8;
  }

  auto oversizedShrub = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::LowShrub,
      GameplayForestGroundCoverDetail3D::Near, 0x51a7c3u);
  oversizedShrub.vertices[2].x += .22f;
  if (LowShrubTierContract(oversizedShrub)) {
    std::cerr << "LOW_SHRUB_OVERSIZED_LEAF_HOSTILE=FAIL\n";
    return 8;
  }

  auto groundedShrub = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::LowShrub,
      GameplayForestGroundCoverDetail3D::Near, 0x51a7c3u);
  for (std::size_t base = 0; base < groundedShrub.vertices.size(); base += 4) {
    groundedShrub.vertices[base].y = 0.f;
    groundedShrub.vertices[base + 1].y = 0.f;
  }
  if (LowShrubTierContract(groundedShrub)) {
    std::cerr << "LOW_SHRUB_GROUNDED_WALL_HOSTILE=FAIL\n";
    return 8;
  }

  const auto fern = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::Fern,
      GameplayForestGroundCoverDetail3D::Near, 0x4a17u);
  const auto shrub = CreateGameplayForestGroundCoverCommunityMesh3D(
      GameplayForestGroundCoverDetail3D::Near, 0x7c31u);
  const auto fernBounds = fern.Bounds();
  const auto shrubBounds = shrub.Bounds();
  const float fernWorldHeight = fernBounds.extents.y * 2.f * metreAuthoredScale.y;
  const float shrubWorldHeight = shrubBounds.extents.y * 2.f * metreAuthoredScale.y;
  if (fernWorldHeight < .50f || fernWorldHeight > 1.f ||
      shrubWorldHeight < .50f || shrubWorldHeight > 1.f ||
      fernWorldHeight <= fernBounds.extents.y * 2.f * ecologyScale.y * 3.f ||
      shrubWorldHeight <= shrubBounds.extents.y * 2.f * ecologyScale.y * 3.f ||
      fernBounds.center.y - fernBounds.extents.y < -1e-6f ||
      shrubBounds.center.y - shrubBounds.extents.y < -1e-6f) {
    std::cerr << "FOREST_GROUND_COVER_WORLD_SCALE=FAIL\n";
    return 7;
  }

  std::cout << "GAMEPLAY_FOREST_GROUND_COVER_MESH=PASS\n";
  return 0;
}
