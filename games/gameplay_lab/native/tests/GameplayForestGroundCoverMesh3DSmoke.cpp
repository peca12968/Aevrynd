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
  std::array<float, 3> radiusSums{}, widthSums{};
  for (std::size_t base = 0; base < mesh.vertices.size(); base += 4) {
    const auto& rootA = mesh.vertices[base];
    const auto& rootB = mesh.vertices[base + 1];
    const auto& topA = mesh.vertices[base + 2];
    const auto& topB = mesh.vertices[base + 3];
    if (std::abs(rootA.y) > 1e-6f || std::abs(rootB.y) > 1e-6f ||
        std::abs(topA.y - topB.y) > 1e-6f)
      return false;
    const float topY = (topA.y + topB.y) * .5f;
    const unsigned tier = topY < .52f ? 0u : topY < .70f ? 1u : 2u;
    const float centerX = (topA.x + topB.x) * .5f;
    const float centerZ = (topA.z + topB.z) * .5f;
    const float width = Length(Vec3{topA.x - topB.x, topA.y - topB.y,
                                    topA.z - topB.z});
    if (topY < .37f || topY > .85f || width <= 0.f)
      return false;
    ++counts[tier];
    radiusSums[tier] += std::sqrt(centerX * centerX + centerZ * centerZ);
    widthSums[tier] += width;
  }
  if (!counts[0] || !counts[1] || !counts[2]) return false;
  const float outerRadius = radiusSums[0] / float(counts[0]);
  const float crownRadius = radiusSums[2] / float(counts[2]);
  const float outerWidth = widthSums[0] / float(counts[0]);
  const float crownWidth = widthSums[2] / float(counts[2]);
  return outerRadius >= crownRadius + .08f &&
         outerWidth >= crownWidth + .12f;
}

bool FernCardShapeContract(const Mesh3D& mesh) {
  if (mesh.vertices.empty() || mesh.vertices.size() % 4 != 0) return false;
  std::size_t leafVertexCount = 0;
  for (std::size_t base = 0; base < mesh.vertices.size(); base += 4) {
    const auto& rootA = mesh.vertices[base];
    const auto& rootB = mesh.vertices[base + 1];
    const auto& tipA = mesh.vertices[base + 2];
    const auto& tipB = mesh.vertices[base + 3];
    if (rootA.y < 0.f || rootB.y < 0.f || tipA.y < 0.f || tipB.y < 0.f ||
        rootA.y > .66f || rootB.y > .66f || tipA.y > .66f || tipB.y > .66f)
      return false;
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
    const bool stem = tipWidth3D < .05f;
    if (stem) {
      if (rootWidth3D < .0035f || rootWidth3D > .0095f ||
          tipWidth3D < .0035f || tipWidth3D > .0095f ||
          cardLength < .05f || cardLength > .70f)
        return false;
    } else {
      if (leafVertexCount != base ||
          std::abs(rootA.y - rootB.y) > 1e-6f ||
          std::abs(tipA.y - tipB.y) > 1e-6f ||
          rootWidth < .012f || rootWidth > .024f ||
          tipWidth < .050f || tipWidth > .112f ||
          cardLength < .12f || cardLength > .225f || tipWidth <= rootWidth ||
          std::abs(tipA.y - rootA.y) > .08f)
        return false;
      leafVertexCount += 4;
    }
  }
  // 每兩張卡片必須由同一個莖節向左右展開；這個幾何條件防止日後
  // 回退成沿莖交錯、低鏡頭只剩零碎浮片的配置。
  if (!leafVertexCount || leafVertexCount == mesh.vertices.size() ||
      leafVertexCount % 8 != 0)
    return false;
  for (std::size_t base = 0; base + 7 < leafVertexCount; base += 8) {
    const Vec3 rootLeft{
        (mesh.vertices[base].x + mesh.vertices[base + 1].x) * .5f,
        (mesh.vertices[base].y + mesh.vertices[base + 1].y) * .5f,
        (mesh.vertices[base].z + mesh.vertices[base + 1].z) * .5f};
    const Vec3 rootRight{
        (mesh.vertices[base + 4].x + mesh.vertices[base + 5].x) * .5f,
        (mesh.vertices[base + 4].y + mesh.vertices[base + 5].y) * .5f,
        (mesh.vertices[base + 4].z + mesh.vertices[base + 5].z) * .5f};
    const Vec3 tipLeft{
        (mesh.vertices[base + 2].x + mesh.vertices[base + 3].x) * .5f,
        (mesh.vertices[base + 2].y + mesh.vertices[base + 3].y) * .5f,
        (mesh.vertices[base + 2].z + mesh.vertices[base + 3].z) * .5f};
    const Vec3 tipRight{
        (mesh.vertices[base + 6].x + mesh.vertices[base + 7].x) * .5f,
        (mesh.vertices[base + 6].y + mesh.vertices[base + 7].y) * .5f,
        (mesh.vertices[base + 6].z + mesh.vertices[base + 7].z) * .5f};
    const float rootWeightLeft = UnpackNormalSnorm8(mesh.vertices[base].normal).w;
    const float rootWeightRight = UnpackNormalSnorm8(mesh.vertices[base + 4].normal).w;
    const float tipWeightLeft = UnpackNormalSnorm8(mesh.vertices[base + 2].normal).w;
    const float tipWeightRight = UnpackNormalSnorm8(mesh.vertices[base + 6].normal).w;
    if (Length(rootLeft - rootRight) > 1e-5f ||
        Length(tipLeft - tipRight) < .20f ||
        std::abs(rootWeightLeft - rootWeightRight) > .02f ||
        std::abs(tipWeightLeft - tipWeightRight) > .02f ||
        tipWeightLeft > rootWeightLeft + .02f)
      return false;
  }
  // 每段莖有兩張交叉卡片且必須首尾相接，從地面連到最後一組葉節；
  // 任何斷點都會在玩家鏡頭中重新形成懸空葉片。
  Vec3 previousEnd{};
  bool havePrevious = false;
  float previousEndWeight = 1.f;
  if ((mesh.vertices.size() - leafVertexCount) % 8 != 0) return false;
  for (std::size_t base = leafVertexCount; base < mesh.vertices.size(); base += 8) {
    const Vec3 startA{
        (mesh.vertices[base].x + mesh.vertices[base + 1].x) * .5f,
        (mesh.vertices[base].y + mesh.vertices[base + 1].y) * .5f,
        (mesh.vertices[base].z + mesh.vertices[base + 1].z) * .5f};
    const Vec3 endA{
        (mesh.vertices[base + 2].x + mesh.vertices[base + 3].x) * .5f,
        (mesh.vertices[base + 2].y + mesh.vertices[base + 3].y) * .5f,
        (mesh.vertices[base + 2].z + mesh.vertices[base + 3].z) * .5f};
    const Vec3 startB{
        (mesh.vertices[base + 4].x + mesh.vertices[base + 5].x) * .5f,
        (mesh.vertices[base + 4].y + mesh.vertices[base + 5].y) * .5f,
        (mesh.vertices[base + 4].z + mesh.vertices[base + 5].z) * .5f};
    const Vec3 endB{
        (mesh.vertices[base + 6].x + mesh.vertices[base + 7].x) * .5f,
        (mesh.vertices[base + 6].y + mesh.vertices[base + 7].y) * .5f,
        (mesh.vertices[base + 6].z + mesh.vertices[base + 7].z) * .5f};
    const float startWeightA = UnpackNormalSnorm8(mesh.vertices[base].normal).w;
    const float endWeightA = UnpackNormalSnorm8(mesh.vertices[base + 2].normal).w;
    const float startWeightB = UnpackNormalSnorm8(mesh.vertices[base + 4].normal).w;
    const float endWeightB = UnpackNormalSnorm8(mesh.vertices[base + 6].normal).w;
    if (Length(startA - startB) > 1e-5f || Length(endA - endB) > 1e-5f ||
        (!havePrevious && startA.y > 1e-6f) ||
        (havePrevious && Length(startA - previousEnd) > 1e-5f) ||
        std::abs(startWeightA - startWeightB) > .02f ||
        std::abs(endWeightA - endWeightB) > .02f ||
        endWeightA > startWeightA + .02f ||
        (havePrevious && std::abs(startWeightA - previousEndWeight) > .02f))
      return false;
    previousEnd = endA;
    previousEndWeight = endWeightA;
    havePrevious = true;
  }
  const std::size_t lastLeafBase = leafVertexCount - 8;
  const Vec3 lastLeafRoot{
      (mesh.vertices[lastLeafBase].x + mesh.vertices[lastLeafBase + 1].x) * .5f,
      (mesh.vertices[lastLeafBase].y + mesh.vertices[lastLeafBase + 1].y) * .5f,
      (mesh.vertices[lastLeafBase].z + mesh.vertices[lastLeafBase + 1].z) * .5f};
  if (!havePrevious || Length(previousEnd - lastLeafRoot) > 1e-5f ||
      previousEndWeight > .02f)
    return false;
  const auto bounds = mesh.Bounds();
  return bounds.center.y - bounds.extents.y >= -1e-6f &&
         bounds.center.y + bounds.extents.y >= .48f &&
         bounds.center.y + bounds.extents.y <= .63f;
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
          !AtlasUvContract(mesh,
                           GameplayForestGroundCoverAtlas3D::IslandTree01) ||
          (item == GameplayForestGroundCoverSpecies3D::Fern &&
           !FernCardShapeContract(mesh)) ||
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
                                      FernCardShapeContract(mesh))
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

  std::size_t previousCommunityTriangles =
      (std::numeric_limits<std::size_t>::max)();
  constexpr std::array<std::size_t, 3> expectedCommunityTriangles{{96, 62, 36}};
  for (std::size_t detailIndex = 0; detailIndex < details.size(); ++detailIndex) {
    const auto detail = details[detailIndex];
    const auto community =
        CreateGameplayForestGroundCoverCommunityMesh3D(detail, 0xf04e57u);
    const auto repeated =
        CreateGameplayForestGroundCoverCommunityMesh3D(detail, 0xf04e57u);
    const std::size_t triangles = community.indices.size() / 3;
    if (!community.Valid() || !Exact(community, repeated) ||
        !AtlasUvContract(community,
                         GameplayForestGroundCoverAtlas3D::TreeSmall02) ||
        triangles > GameplayForestGroundCoverCommunityTriangleBudget(detail) ||
        triangles != expectedCommunityTriangles[detailIndex] ||
        triangles >= previousCommunityTriangles ||
        community.Bounds().extents.x < .3f || community.Bounds().extents.z < .3f) {
      std::cerr << "FOREST_GROUND_COVER_COMMUNITY_CONTRACT=FAIL\n";
      return 5;
    }
    previousCommunityTriangles = triangles;
  }

  std::size_t previousRiverbankTriangles =
      (std::numeric_limits<std::size_t>::max)();
  constexpr std::array<std::size_t, 3> expectedRiverbankTriangles{{76, 50, 30}};
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
