#pragma once

#include <Aevrynd/Camera.h>

#include <cstdint>
#include <span>
#include <vector>

namespace GameplayLab {

struct GameplayOccluderBox3D {
  std::uint64_t stableLow{};
  Aevrynd::Vec3 center{};
  Aevrynd::Vec3 size{};
  float yaw{};
};

struct GameplayOccluderMask3D {
  std::uint64_t stableLow{};
  std::uint32_t width{}, height{};
  std::uint64_t coveredPixels{}, maskHash{};
  float coverage{};
  bool nearPlaneClipped{};
  std::vector<std::uint8_t> pixels{};
};

// 對每個stable ID獨立做齊次裁切與像素raster，回傳覆蓋最大的物件。
[[nodiscard]] bool AnalyzeGameplayOccluderMask3D(
    const Aevrynd::Camera3D &camera,
    std::span<const GameplayOccluderBox3D> occluders,
    std::uint32_t width, std::uint32_t height,
    GameplayOccluderMask3D &output) noexcept;

} // namespace GameplayLab
