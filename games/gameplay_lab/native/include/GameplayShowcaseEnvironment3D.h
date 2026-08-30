#pragma once

#include <Aevrynd/Renderer.h>

#include <array>
#include <cstddef>
#include <cstdint>
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

struct ShowcaseEnvironmentReceipt3D {
  std::uint64_t seed{};
  std::uint64_t worldHash{};
  std::size_t terrain{}, water{}, vegetation{}, rocks{}, buildings{};
  std::size_t retainedDescriptorBytes{};
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
  void ApplyUltraProfile(Aevrynd::DrawList &draw) const;
  void ApplyLighting(Aevrynd::DrawList &draw,
                     ShowcaseLightingProfile3D profile,
                     Aevrynd::Vec3 cameraPosition) const;

  [[nodiscard]] ShowcaseLightingState3D
  EvaluateLighting(ShowcaseLightingProfile3D profile) const;
  [[nodiscard]] ShowcaseEnvironmentReceipt3D Receipt() const;
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
