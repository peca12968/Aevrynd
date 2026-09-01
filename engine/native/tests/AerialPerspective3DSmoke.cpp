#include <Aevrynd/Renderer.h>

#include <cmath>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

using namespace Aevrynd;

namespace {
bool Near(float left, float right, float epsilon = 1e-5f) {
  return std::abs(left - right) <= epsilon;
}
}

int main() {
  AerialPerspectiveSettings3D hostile{};
  hostile.extinction = std::numeric_limits<float>::infinity();
  hostile.scattering = 4.f;
  hostile.heightFalloff = -1.f;
  hostile.nearDistance = 80.f;
  hostile.nearFadeDistance = 0.f;
  hostile.maximumDistance = 20.f;
  hostile.anisotropy = 9.f;
  hostile.skyRadiance = {-1.f, std::numeric_limits<float>::quiet_NaN(), 200.f, 0.f};
  hostile.sunRadianceScale = 8.f;
  const auto normalized = NormalizeAerialPerspectiveSettings3D(hostile);
  if (normalized.extinction != .0024f || normalized.scattering != .0024f ||
      normalized.heightFalloff != 0.f || normalized.nearFadeDistance != .001f ||
      normalized.maximumDistance <= normalized.nearDistance ||
      normalized.anisotropy != .85f || normalized.skyRadiance.r != 0.f ||
      normalized.skyRadiance.g != .17f || normalized.skyRadiance.b != 100.f ||
      normalized.sunRadianceScale != 1.f)
    return 1;

  AerialPerspectiveSettings3D settings{true, .003f, .0021f, .02f, 0.f,
                                       30.f, 20.f, 400.f, .2f,
                                       {.1f, .14f, .19f, 1.f}, .04f};
  const Vec3 camera{0.f, 3.f, 0.f};
  const auto excluded = EvaluateAerialPerspective3D(settings, camera, {0.f, 3.f, 30.f});
  const auto onset = EvaluateAerialPerspective3D(settings, camera, {0.f, 3.f, 30.01f});
  const auto nearValue = EvaluateAerialPerspective3D(settings, camera, {0.f, 3.f, 60.f});
  const auto middle = EvaluateAerialPerspective3D(settings, camera, {0.f, 3.f, 150.f});
  const auto farValue = EvaluateAerialPerspective3D(settings, camera, {0.f, 3.f, 360.f});
  if (!excluded.FiniteAndConservative() || excluded.transmittance != 1.f ||
      onset.transmittance < .999999f || !nearValue.FiniteAndConservative() ||
      !middle.FiniteAndConservative() || !farValue.FiniteAndConservative() ||
      !(nearValue.transmittance > middle.transmittance && middle.transmittance > farValue.transmittance) ||
      !(nearValue.scatteredFraction < middle.scatteredFraction &&
        middle.scatteredFraction < farValue.scatteredFraction))
    return 2;

  const auto elevated = EvaluateAerialPerspective3D(settings, camera, {0.f, 120.f, 150.f});
  if (!elevated.FiniteAndConservative() || elevated.transmittance <= middle.transmittance)
    return 3;

  // 世界原點平移不得改變積分；同一世界距離也不依透視或正交投影而改變。
  auto shifted = settings;
  shifted.baseHeight -= 1000.f;
  const auto originShifted = EvaluateAerialPerspective3D(
      shifted, camera - Vec3{500.f, 1000.f, -700.f},
      Vec3{0.f, 3.f, 150.f} - Vec3{500.f, 1000.f, -700.f});
  if (!Near(originShifted.transmittance, middle.transmittance) ||
      !Near(originShifted.scatteredFraction, middle.scatteredFraction))
    return 4;

  const auto maximumA = EvaluateAerialPerspective3D(settings, camera, {0.f, 3.f, 400.f});
  const auto maximumB = EvaluateAerialPerspective3D(settings, camera, {0.f, 3.f, 4000.f});
  if (!Near(maximumA.transmittance, maximumB.transmittance) ||
      !Near(maximumA.scatteredFraction, maximumB.scatteredFraction))
    return 5;

  const std::array<float, 5> verticalDirections{-.75f, -.25f, 0.f, .25f, .75f};
  const std::array<float, 4> hostileDistances{settings.nearDistance,
                                              settings.maximumDistance,
                                              settings.maximumDistance * 2.f,
                                              4000.f};
  for (const float directionY : verticalDirections) {
    const float directionZ = std::sqrt(1.f - directionY * directionY);
    const Vec3 direction{0.f, directionY, directionZ};
    const auto maximumReference = EvaluateAerialPerspective3D(
        settings, camera, camera + direction * settings.maximumDistance);
    if (!maximumReference.FiniteAndConservative()) return 8;
    for (const float distance : hostileDistances) {
      const auto value = EvaluateAerialPerspective3D(settings, camera,
                                                     camera + direction * distance);
      if (!value.FiniteAndConservative() ||
          (distance == settings.nearDistance && value.transmittance < .99999f) ||
          (distance >= settings.maximumDistance &&
           (!Near(value.transmittance, maximumReference.transmittance, 2e-5f) ||
            !Near(value.scatteredFraction, maximumReference.scatteredFraction, 2e-5f))))
        return 9;
    }
    const auto beforeCap = EvaluateAerialPerspective3D(
        settings, camera, camera + direction * (settings.maximumDistance - .01f));
    const auto afterCap = EvaluateAerialPerspective3D(
        settings, camera, camera + direction * (settings.maximumDistance + .01f));
    if (!beforeCap.FiniteAndConservative() || !afterCap.FiniteAndConservative() ||
        std::abs(beforeCap.transmittance - afterCap.transmittance) > 2e-4f ||
        std::abs(beforeCap.scatteredFraction - afterCap.scatteredFraction) > 2e-4f)
      return 10;
  }

  // 舊式 horizon 線性 lerp 在近場就產生不連續染色，此突變必須明確失敗。
  const float legacyFogAmount = .72f * (32.f / 400.f) * .62f;
  const auto physicalAtThirtyTwo = EvaluateAerialPerspective3D(settings, camera, {0.f, 3.f, 32.f});
  if (!(legacyFogAmount > (1.f - physicalAtThirtyTwo.transmittance) * 10.f))
    return 6;

  const std::filesystem::path shader = std::filesystem::path{AEVRYND_SOURCE_DIR} /
      "engine" / "native" / "shaders" / "World.hlsl";
  std::ifstream stream(shader, std::ios::binary);
  const std::string source((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
  if (source.empty() || source.find("lit * aerialTransmittance + aerialInscattering") == std::string::npos ||
      source.find("lerp(lit, fogColor.rgb") != std::string::npos ||
      source.find("legacyFog") != std::string::npos)
    return 7;

  std::cout << "AerialPerspective3D PASS nearT=" << nearValue.transmittance
            << " middleT=" << middle.transmittance << " farT=" << farValue.transmittance
            << " farScatter=" << farValue.scatteredFraction
            << " legacyMutation=" << legacyFogAmount << '\n';
  return 0;
}
