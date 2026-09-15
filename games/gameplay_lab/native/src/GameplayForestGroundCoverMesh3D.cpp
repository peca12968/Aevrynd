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
                bool windAffected) {
  const Vec3 edgeA = points[1] - points[0];
  const Vec3 edgeB = points[3] - points[0];
  Vec3 normal = Aevrynd::Normalize(Aevrynd::Cross(edgeA, edgeB));
  if (!std::isfinite(normal.x) || !std::isfinite(normal.y) ||
      !std::isfinite(normal.z) || Aevrynd::Length(normal) < .99f) {
    normal = {0, 1, 0};
  }
  constexpr float u0 = 0.f;
  constexpr float u1 = 1.f;
  const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
  const std::uint32_t packedColor = Aevrynd::PackColorRgba8(color);
  const std::array<float, 4> u{{u0, u1, u1, u0}};
  const std::array<float, 4> v{{1.f, 1.f, 0.f, 0.f}};
  for (std::size_t index = 0; index < points.size(); ++index) {
    const float anchorWeight = windAffected ? (index < 2 ? 1.f : 0.f) : 1.f;
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
      {{10, 6, 3}},
      {{10, 6, 3}},
      {{12, 7, 4}},
  }};
  return Counts[static_cast<std::size_t>(species)]
               [static_cast<std::size_t>(detail)];
}

Mesh3D CreateFern(GameplayForestGroundCoverDetail3D detail,
                  std::uint32_t seed) {
  Mesh3D mesh;
  const unsigned count = CardCount(GameplayForestGroundCoverSpecies3D::Fern, detail);
  mesh.vertices.reserve(std::size_t(count) * 4);
  mesh.indices.reserve(std::size_t(count) * 6);
  for (unsigned item = 0; item < count; ++item) {
    const float phase = 2.f * Pi * (float(item) + UnitNoise(seed, item, 11u) * .42f) /
                        float(count);
    const float radial = .05f + UnitNoise(seed, item, 19u) * .07f;
    const float length = .58f + UnitNoise(seed, item, 23u) * .34f;
    const float width = .09f + UnitNoise(seed, item, 29u) * .07f;
    const Vec3 direction{std::cos(phase), 0, std::sin(phase)};
    const Vec3 side{-direction.z, 0, direction.x};
    const Vec3 root = direction * radial;
    const Vec3 tip = root + direction * length +
                     Vec3{0, .56f + UnitNoise(seed, item, 31u) * .36f, 0};
    const Vec3 shoulder = root + (tip - root) * .76f;
    AppendCard(mesh,
               {root - side * width * .22f, root + side * width * .22f,
                shoulder + side * width, tip - side * width * .08f},
               SpeciesColor(GameplayForestGroundCoverSpecies3D::Fern,
                            UnitNoise(seed, item, 37u)),
               true);
  }
  return mesh;
}

Mesh3D CreateLowShrub(GameplayForestGroundCoverDetail3D detail,
                      std::uint32_t seed) {
  Mesh3D mesh;
  const unsigned count = CardCount(GameplayForestGroundCoverSpecies3D::LowShrub, detail);
  mesh.vertices.reserve(std::size_t(count) * 4);
  mesh.indices.reserve(std::size_t(count) * 6);
  for (unsigned item = 0; item < count; ++item) {
    // One placement must read as a low forest shrub rather than a wall of
    // equally tall cards.  Every LOD retains one deterministic card from each
    // of the outer skirt, middle foliage and crown tiers; only the authored
    // vertex positions change, so triangle count, draw count and texture work
    // remain identical to the original community mesh.
    const unsigned tier = item % 3u;
    const float phase = Pi * (float(item) / float(count) +
                              UnitNoise(seed, item, 41u) * .13f);
    constexpr std::array<float, 3> RadiusBase{{.26f, .14f, .05f}};
    constexpr std::array<float, 3> RadiusVariation{{.10f, .08f, .07f}};
    constexpr std::array<float, 3> WidthBase{{.55f, .44f, .30f}};
    constexpr std::array<float, 3> WidthVariation{{.15f, .12f, .10f}};
    constexpr std::array<float, 3> HeightBase{{.38f, .54f, .72f}};
    constexpr std::array<float, 3> HeightVariation{{.10f, .11f, .12f}};
    const float radius = RadiusBase[tier] +
                         UnitNoise(seed, item, 43u) * RadiusVariation[tier];
    const float width = WidthBase[tier] +
                        UnitNoise(seed, item, 47u) * WidthVariation[tier];
    const float height = HeightBase[tier] +
                         UnitNoise(seed, item, 53u) * HeightVariation[tier];
    const Vec3 facing{std::sin(phase), 0, std::cos(phase)};
    const Vec3 side{facing.z, 0, -facing.x};
    const float radialPhase = 2.f * Pi * UnitNoise(seed, item, 59u);
    const Vec3 radial{std::cos(radialPhase), 0,
                      std::sin(radialPhase)};
    const Vec3 root = radial * (radius * .42f);
    const Vec3 top = radial * radius + Vec3{0, height, 0};
    const Vec3 rootHalfSide = side * (width * .16f);
    const Vec3 topHalfSide = side * (width * .5f);
    AppendCard(mesh,
               {root - rootHalfSide, root + rootHalfSide,
                top + topHalfSide, top - topHalfSide},
               SpeciesColor(GameplayForestGroundCoverSpecies3D::LowShrub,
                            UnitNoise(seed, item, 61u)),
               true);
  }
  return mesh;
}

Mesh3D CreateLeafMoss(GameplayForestGroundCoverDetail3D detail,
                      std::uint32_t seed) {
  Mesh3D mesh;
  const unsigned count = CardCount(GameplayForestGroundCoverSpecies3D::LeafMoss, detail);
  mesh.vertices.reserve(std::size_t(count) * 4);
  mesh.indices.reserve(std::size_t(count) * 6);
  for (unsigned item = 0; item < count; ++item) {
    const float phase = 2.f * Pi * UnitNoise(seed, item, 67u);
    // 低伏層負責填滿 placement 之間的裸地，不用增加 descriptor 或
    // alpha-card 數量。面積擴張只在 XZ，Y 仍貼地，避免冒充灌木高度。
    const float radius = .12f + UnitNoise(seed, item, 71u) * .48f;
    const float halfWidth = .21f + UnitNoise(seed, item, 73u) * .16f;
    const float halfLength = .27f + UnitNoise(seed, item, 79u) * .20f;
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
               false);
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

} // namespace

Mesh3D CreateGameplayForestGroundCoverMesh3D(
    GameplayForestGroundCoverSpecies3D species,
    GameplayForestGroundCoverDetail3D detail,
    std::uint32_t variationSeed) {
  switch (species) {
    case GameplayForestGroundCoverSpecies3D::Fern:
      return CreateFern(detail, variationSeed);
    case GameplayForestGroundCoverSpecies3D::LowShrub:
      return CreateLowShrub(detail, variationSeed);
    case GameplayForestGroundCoverSpecies3D::LeafMoss:
      return CreateLeafMoss(detail, variationSeed);
  }
  return {};
}

Mesh3D CreateGameplayForestGroundCoverCommunityMesh3D(
    GameplayForestGroundCoverDetail3D detail,
    std::uint32_t variationSeed) {
  Mesh3D result;
  const auto fern = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::Fern, detail,
      Mix(variationSeed ^ 0x19a4f3cdu));
  const auto shrub = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::LowShrub, detail,
      Mix(variationSeed ^ 0x753c28b1u));
  const auto leafMoss = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::LeafMoss, detail,
      Mix(variationSeed ^ 0xc4d82a67u));
  AppendTransformed(result, fern, {-.24f, 0, -.09f}, -.31f, .92f);
  AppendTransformed(result, shrub, {.22f, 0, .11f}, .47f, .88f);
  AppendTransformed(result, leafMoss, {0, 0, .03f}, .14f, 1.22f);
  return result;
}

Mesh3D CreateGameplayForestRiverbankGroundCoverMesh3D(
    GameplayForestGroundCoverDetail3D detail,
    std::uint32_t variationSeed) {
  Mesh3D result;
  const auto fern = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::Fern, detail,
      Mix(variationSeed ^ 0x82c6e4b9u));
  const auto leafMoss = CreateGameplayForestGroundCoverMesh3D(
      GameplayForestGroundCoverSpecies3D::LeafMoss, detail,
      Mix(variationSeed ^ 0x3d1a7f25u));
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
