#pragma once

#include <Aevrynd/Math.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace GameplayLab {

enum class GameplayCottageInteriorPartRole3D : std::uint8_t {
  Floor,
  SideWall,
  BackWall,
  Ceiling,
  Threshold,
  Lamp
};

struct GameplayCottageInteriorPart3D {
  Aevrynd::Vec3 position{};
  Aevrynd::Vec3 scale{1.f, 1.f, 1.f};
  GameplayCottageInteriorPartRole3D role{};
};

struct GameplayCottageInteriorLight3D {
  Aevrynd::Vec3 position{};
  float radius{};
  Aevrynd::Color color{};
  float intensity{};
};

inline constexpr std::array<GameplayCottageInteriorPart3D, 7>
    GameplayCottageInteriorParts3D{{
        {{-12.f, 0.f, 24.f}, {7.40f, .06f, 6.75f},
         GameplayCottageInteriorPartRole3D::Floor},
        {{-15.76f, 1.63f, 24.f}, {.10f, 3.20f, 6.70f},
         GameplayCottageInteriorPartRole3D::SideWall},
        {{-8.24f, 1.63f, 24.f}, {.10f, 3.20f, 6.70f},
         GameplayCottageInteriorPartRole3D::SideWall},
        {{-12.f, 1.63f, 27.38f}, {7.42f, 3.20f, .10f},
         GameplayCottageInteriorPartRole3D::BackWall},
        {{-12.f, 3.23f, 24.f}, {7.40f, .08f, 6.75f},
         GameplayCottageInteriorPartRole3D::Ceiling},
        {{-12.f, .04f, 20.62f}, {1.42f, .04f, .26f},
         GameplayCottageInteriorPartRole3D::Threshold},
        {{-12.f, 2.82f, 23.10f}, {.24f, .20f, .24f},
         GameplayCottageInteriorPartRole3D::Lamp},
    }};

inline constexpr GameplayCottageInteriorLight3D
GameplayCottageInteriorDayLight3D() noexcept {
  return {{-12.f, 2.66f, 23.10f}, 4.35f,
          {1.f, .67f, .34f, 1.f}, .72f};
}

inline bool ValidGameplayCottageInteriorLight3D(
    GameplayCottageInteriorLight3D value) noexcept {
  const auto finite = [](Aevrynd::Vec3 point) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::isfinite(point.z);
  };
  return finite(value.position) && std::isfinite(value.radius) &&
         value.radius >= 2.5f && value.radius <= 4.5f &&
         std::isfinite(value.color.r) && std::isfinite(value.color.g) &&
         std::isfinite(value.color.b) && std::isfinite(value.color.a) &&
         value.color.r >= value.color.g && value.color.g >= value.color.b &&
         value.color.a == 1.f && std::isfinite(value.intensity) &&
         value.intensity >= .5f && value.intensity <= 1.2f;
}

} // namespace GameplayLab
