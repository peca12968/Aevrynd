#pragma once

#include <Aevrynd/Camera.h>
#include <Aevrynd/Renderer.h>
#include <Aevrynd/Scene3D.h>

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace GameplayLab {

enum class ShowcaseLightingProfile3D : std::uint8_t { Day, Dusk, Night };
enum class ShowcaseSceneKind3D : std::uint8_t {
  Terrain,
  Water,
  Tree,
  Grass,
  Rock,
  Building
};

// 選擇器與場景提交共用同一份正式草地LOD政策，避免距離與混合寬度漂移。
[[nodiscard]] inline constexpr Aevrynd::MeshLodPolicy3D
ShowcaseGrassLodPolicy3D() noexcept {
  return {22.f, 5.f, Aevrynd::MeshLodDistanceMetric3D::BoundsSurface};
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
    for (std::size_t index = 0; index < instances.size(); ++index) {
      if (used[index]) continue;
      const auto &instance = instances[index];
      if (instance.layer != Aevrynd::MeshRenderLayer3D::Opaque ||
          instance.identity.id.high != identityHigh ||
          instance.identity.id.low != wanted.identity.stableLow ||
          instance.identity.generation != wanted.identity.generation ||
          (instance.lodSeed & 7u) != wanted.lodSeed)
        continue;
      if (instance.lodSeed & 8u) {
        if (nearIndex == instances.size()) nearIndex = index;
      } else if (farIndex == instances.size()) {
        farIndex = index;
      }
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
  std::size_t retainedDescriptorBytes{};
};

struct ShowcaseSceneTargets3D {
  std::size_t terrainPatches{1}, waterSegments{17}, treeInstances{72},
      grassInstances{48}, rockInstances{12}, buildingComponents{6};
  std::size_t nearVegetation{40}, midVegetation{56}, farVegetation{24};
  float terrainSpanMeters{512.f}, riverLengthMeters{272.f},
      riverMinimumWidthMeters{9.f}, riverMaximumWidthMeters{15.f},
      minimumRiverSurfaceAreaSquareMeters{2800.f};
  std::uint32_t requiredBuildingComponentMask{0x3fu};
  std::uint64_t materialIdentityHash{}, licenseInventoryHash{};
};

struct ShowcaseExecutionReceipt3D {
  std::array<std::uint64_t, 10> deltas{};
  bool executed{};
};

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

private:
  [[nodiscard]] std::uint64_t WorldHash() const;
  [[nodiscard]] std::size_t RetainedDescriptorBytes() const noexcept;

  std::vector<ShowcaseSceneDescriptor3D> descriptors_;
  std::uint64_t seed_{};
  bool initialized_{};
};

} // namespace GameplayLab
