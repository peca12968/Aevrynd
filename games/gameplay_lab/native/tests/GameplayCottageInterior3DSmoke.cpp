#include "GameplayCottageEntry3D.h"
#include "GameplayCottageInterior3D.h"
#include "GameplayShowcaseEnvironment3D.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

using namespace GameplayLab;

int main() {
  std::size_t floors{}, sideWalls{}, backWalls{}, ceilings{}, thresholds{}, lamps{};
  float deepestZ = 0.f;
  for (const auto &part : GameplayCottageInteriorParts3D) {
    const auto finite = [](Aevrynd::Vec3 value) {
      return std::isfinite(value.x) && std::isfinite(value.y) &&
             std::isfinite(value.z);
    };
    if (!finite(part.position) || !finite(part.scale) || part.scale.x <= 0.f ||
        part.scale.y <= 0.f || part.scale.z <= 0.f)
      return 1;
    deepestZ = (std::max)(deepestZ, part.position.z + part.scale.z * .5f);
    switch (part.role) {
    case GameplayCottageInteriorPartRole3D::Floor: ++floors; break;
    case GameplayCottageInteriorPartRole3D::SideWall: ++sideWalls; break;
    case GameplayCottageInteriorPartRole3D::BackWall: ++backWalls; break;
    case GameplayCottageInteriorPartRole3D::Ceiling: ++ceilings; break;
    case GameplayCottageInteriorPartRole3D::Threshold: ++thresholds; break;
    case GameplayCottageInteriorPartRole3D::Lamp: ++lamps; break;
    }
  }
  if (GameplayCottageInteriorParts3D.size() != 7u || floors != 1u ||
      sideWalls != 2u || backWalls != 1u || ceilings != 1u ||
      thresholds != 1u || lamps != 1u || deepestZ < 27.4f)
    return 2;

  const auto threshold = std::find_if(
      GameplayCottageInteriorParts3D.begin(), GameplayCottageInteriorParts3D.end(),
      [](const auto &part) {
        return part.role == GameplayCottageInteriorPartRole3D::Threshold;
      });
  if (threshold == GameplayCottageInteriorParts3D.end() ||
      threshold->scale.x > GameplayCottageDoorClearWidth3D ||
      threshold->scale.y > .08f || threshold->position.z <= 20.4f)
    return 3;
  const auto floor = std::find_if(
      GameplayCottageInteriorParts3D.begin(), GameplayCottageInteriorParts3D.end(),
      [](const auto &part) {
        return part.role == GameplayCottageInteriorPartRole3D::Floor;
      });
  if (floor == GameplayCottageInteriorParts3D.end() ||
      floor->position.y + floor->scale.y * .5f > .031f)
    return 3;

  const auto day = GameplayCottageInteriorDayLight3D();
  if (!ValidGameplayCottageInteriorLight3D(day) || day.position.z < 22.f ||
      day.radius > 4.5f || day.intensity > .75f)
    return 4;

  auto invalid = day;
  invalid.radius = std::numeric_limits<float>::quiet_NaN();
  if (ValidGameplayCottageInteriorLight3D(invalid))
    return 5;

  GameplayShowcaseEnvironment3D environment;
  Aevrynd::DrawList dayDraw{Aevrynd::DrawListCapacityProfile::Minimal2D};
  Aevrynd::DrawList duskDraw{Aevrynd::DrawListCapacityProfile::Minimal2D};
  Aevrynd::DrawList nightDraw{Aevrynd::DrawListCapacityProfile::Minimal2D};
  environment.ApplyLighting(dayDraw, ShowcaseLightingProfile3D::Day, {});
  environment.ApplyLighting(duskDraw, ShowcaseLightingProfile3D::Dusk, {});
  environment.ApplyLighting(nightDraw, ShowcaseLightingProfile3D::Night, {});
  const auto &dayLights = dayDraw.PointLights();
  if (dayLights.size() != 1u ||
      dayLights[0].position.x != day.position.x ||
      dayLights[0].position.y != day.position.y ||
      dayLights[0].position.z != day.position.z ||
      dayLights[0].radius != day.radius ||
      dayLights[0].intensity != day.intensity ||
      duskDraw.PointLights().size() !=
          3u + GameplayForestGuideLightPositions3D.size() ||
      nightDraw.PointLights().size() != duskDraw.PointLights().size())
    return 6;

  std::cout << "GAMEPLAY_COTTAGE_INTERIOR=PASS parts="
            << GameplayCottageInteriorParts3D.size() << " depth="
            << deepestZ - 20.4f << " dayLight=" << day.intensity
            << " radius=" << day.radius << '\n';
  return 0;
}
