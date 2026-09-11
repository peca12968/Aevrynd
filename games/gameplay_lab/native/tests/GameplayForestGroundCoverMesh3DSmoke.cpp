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
  const float minimumU = 0.f;
  const float maximumU = 1.f;
  bool hasMinimumU = false;
  bool hasMaximumU = false;
  bool hasAnchor = false;
  bool hasWindTip = species == GameplayForestGroundCoverSpecies3D::LeafMoss;
  for (const auto& vertex : mesh.vertices) {
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) ||
        !std::isfinite(vertex.z) || !std::isfinite(vertex.u) ||
        !std::isfinite(vertex.v) || vertex.y < 0.f ||
        vertex.u < minimumU - 1e-6f || vertex.u > maximumU + 1e-6f ||
        vertex.v < 0.f || vertex.v > 1.f || (vertex.color >> 24) != 255u)
      return false;
    hasMinimumU |= std::abs(vertex.u - minimumU) < 1e-6f;
    hasMaximumU |= std::abs(vertex.u - maximumU) < 1e-6f;
    const auto normal = UnpackNormalSnorm8(vertex.normal);
    const float normalLength = Length(Vec3{normal.x, normal.y, normal.z});
    if (!std::isfinite(normalLength) || normalLength < .985f || normalLength > 1.015f)
      return false;
    hasAnchor |= normal.w > .98f;
    hasWindTip |= normal.w < .02f;
  }
  return hasMinimumU && hasMaximumU && hasAnchor && hasWindTip;
}

bool CommunityUvContract(const Mesh3D& mesh) {
  bool minimumU{}, maximumU{};
  for (const auto& vertex : mesh.vertices) {
    if (vertex.u < 0.f || vertex.u > 1.f || vertex.v < 0.f || vertex.v > 1.f)
      return false;
    minimumU |= vertex.u == 0.f;
    maximumU |= vertex.u == 1.f;
  }
  return minimumU && maximumU;
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

  constexpr std::array species{
      GameplayForestGroundCoverSpecies3D::Fern,
      GameplayForestGroundCoverSpecies3D::LowShrub,
      GameplayForestGroundCoverSpecies3D::LeafMoss};
  constexpr std::array details{
      GameplayForestGroundCoverDetail3D::Near,
      GameplayForestGroundCoverDetail3D::Mid,
      GameplayForestGroundCoverDetail3D::Far};

  for (const auto item : species) {
    std::size_t previousTriangles = (std::numeric_limits<std::size_t>::max)();
    for (const auto detail : details) {
      const auto mesh = CreateGameplayForestGroundCoverMesh3D(item, detail, 0x51a7c3u);
      const auto repeated = CreateGameplayForestGroundCoverMesh3D(item, detail, 0x51a7c3u);
      const std::size_t triangles = mesh.indices.size() / 3;
      if (!mesh.Valid() || !Exact(mesh, repeated) ||
          triangles > GameplayForestGroundCoverTriangleBudget(detail) ||
          triangles >= previousTriangles || !VertexContract(mesh, item)) {
        std::cerr << "FOREST_GROUND_COVER_SPECIES_CONTRACT=FAIL\n";
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
  for (const auto detail : details) {
    const auto community =
        CreateGameplayForestGroundCoverCommunityMesh3D(detail, 0xf04e57u);
    const auto repeated =
        CreateGameplayForestGroundCoverCommunityMesh3D(detail, 0xf04e57u);
    const std::size_t triangles = community.indices.size() / 3;
    if (!community.Valid() || !Exact(community, repeated) ||
        !CommunityUvContract(community) ||
        triangles > GameplayForestGroundCoverCommunityTriangleBudget(detail) ||
        triangles >= previousCommunityTriangles ||
        community.Bounds().extents.x < .3f || community.Bounds().extents.z < .3f) {
      std::cerr << "FOREST_GROUND_COVER_COMMUNITY_CONTRACT=FAIL\n";
      return 5;
    }
    previousCommunityTriangles = triangles;
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
