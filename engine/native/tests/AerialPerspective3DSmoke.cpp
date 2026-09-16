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

std::array<float, 3> EvaluatePhaseSkyBase(
    const std::array<float, 3>& zenith, const std::array<float, 3>& horizon,
    const std::array<float, 3>& ground, float intensity, float exponent,
    float directionY) {
  const float hemisphere = std::clamp(std::abs(directionY), 0.f, 1.f);
  const float shaped = std::pow(hemisphere, std::max(.05f, exponent));
  const auto& pole = directionY >= 0.f ? zenith : ground;
  std::array<float, 3> result{};
  for (std::size_t channel = 0; channel < result.size(); ++channel)
    result[channel] = std::max(0.f, (horizon[channel] +
        (pole[channel] - horizon[channel]) * shaped) * intensity);
  return result;
}

float Luminance(const std::array<float, 3>& value) {
  return value[0] * .2126f + value[1] * .7152f + value[2] * .0722f;
}

std::array<float, 3> EvaluateTerrainMaterialDepthContrastReference(
    float mesoSignal, std::array<float, 3> authoredWeights) {
  const float total = std::clamp(authoredWeights[0] + authoredWeights[1] +
                                 authoredWeights[2], 0.f, 1.f);
  const float normalization = std::max(.000001f, authoredWeights[0] +
                                                   authoredWeights[1] +
                                                   authoredWeights[2]);
  for (float& value : authoredWeights) value = std::clamp(value, 0.f, 1.f) / normalization;
  const float x = std::clamp((mesoSignal - .18f) / (.82f - .18f), 0.f, 1.f);
  const float shapedMeso = x * x * (3.f - 2.f * x);
  const float proceduralScale = .84f + (1.14f - .84f) * shapedMeso;
  constexpr std::array<std::array<float, 3>, 3> targets{{
      {{1.12f, 1.13f, 1.18f}}, {{1.02f, .88f, .72f}}, {{.70f, .76f, .82f}}}};
  std::array<float, 3> result{};
  for (std::size_t channel = 0; channel < result.size(); ++channel) {
    const float authored = authoredWeights[0] * targets[0][channel] +
                           authoredWeights[1] * targets[1][channel] +
                           authoredWeights[2] * targets[2][channel];
    result[channel] = proceduralScale + (authored - proceduralScale) * total;
  }
  return result;
}

std::array<float, 3> EvaluateTerrainMaterialDepthContrast(
    float mesoSignal, std::array<float, 3> authoredWeights) {
  for (float& value : authoredWeights) value = std::clamp(value, 0.f, 1.f);
  const float total = authoredWeights[0] + authoredWeights[1] + authoredWeights[2];
  const float x = std::clamp((mesoSignal - .18f) * 1.5625f, 0.f, 1.f);
  const float shapedMeso = x * x * (3.f - 2.f * x);
  const float proceduralScale = .84f + .30f * shapedMeso;
  if (total <= .000001f) return {proceduralScale, proceduralScale, proceduralScale};
  constexpr std::array<std::array<float, 3>, 3> targets{{
      {{1.12f, 1.13f, 1.18f}}, {{1.02f, .88f, .72f}}, {{.70f, .76f, .82f}}}};
  const float authoredAmount = std::clamp(total, 0.f, 1.f);
  std::array<float, 3> result{};
  for (std::size_t channel = 0; channel < result.size(); ++channel)
    result[channel] = std::clamp(
        proceduralScale * (1.f - authoredAmount) +
        authoredWeights[0] * targets[0][channel] +
        authoredWeights[1] * targets[1][channel] +
        authoredWeights[2] * targets[2][channel], .70f, 1.18f);
  return result;
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
  const auto phaseSkyBegin = source.find("float3 EvaluatePhaseCoherentSkyRadiance(");
  const auto analyticSkyBegin = source.find("float3 EvaluateAnalyticSkyRadiance(");
  if (source.empty() || source.find("lit * aerialTransmittance + aerialInscattering") == std::string::npos ||
      source.find("lerp(lit, fogColor.rgb") != std::string::npos ||
      source.find("legacyFog") != std::string::npos ||
      phaseSkyBegin == std::string::npos || analyticSkyBegin == std::string::npos ||
      phaseSkyBegin >= analyticSkyBegin)
    return 7;

  // 天空與遠景材質必須共用 phase radiance；若重新落回固定日間藍／橙常數，
  // source oracle 直接關閉候選，不讓曝光或 capture 平均值掩蓋錯誤。
  const std::string phaseSkySource = source.substr(phaseSkyBegin, analyticSkyBegin - phaseSkyBegin);
  if (phaseSkySource.find("phaseSkyZenith") == std::string::npos ||
      phaseSkySource.find("phaseSkyHorizon") == std::string::npos ||
      phaseSkySource.find("phaseSkyGround") == std::string::npos ||
      source.find("const float3 atmosphereBase = EvaluatePhaseCoherentSkyRadiance(blurredDirection);") == std::string::npos ||
      source.find("float3(0.23, 0.48, 1.0)") != std::string::npos ||
      source.find("float3(0.72, 0.49, 0.30)") != std::string::npos)
    return 11;

  const auto dayZenith = EvaluatePhaseSkyBase({.12f, .31f, .72f}, {.88f, .48f, .18f},
                                               {.08f, .065f, .05f}, 1.15f, 4.f, 1.f);
  const auto dayHorizon = EvaluatePhaseSkyBase({.12f, .31f, .72f}, {.88f, .48f, .18f},
                                                {.08f, .065f, .05f}, 1.15f, 4.f, 0.f);
  const auto duskZenith = EvaluatePhaseSkyBase({.045f, .075f, .18f}, {.42f, .16f, .07f},
                                                {.035f, .025f, .03f}, .72f, 4.f, 1.f);
  const auto nightZenith = EvaluatePhaseSkyBase({.008f, .014f, .035f}, {.025f, .035f, .075f},
                                                 {.006f, .009f, .018f}, .42f, 4.f, 1.f);
  if (!(Luminance(dayZenith) > Luminance(duskZenith) &&
        Luminance(duskZenith) > Luminance(nightZenith) &&
        std::abs(Luminance(dayHorizon) - Luminance(dayZenith)) > .08f &&
        Luminance(nightZenith) > 0.f))
    return 12;

  const auto terrainDepthBegin = source.find("float3 TerrainMaterialDepthContrast(");
  const auto terrainWarpBegin = source.find("float3 TerrainDecorrelatedWorldPosition(");
  if (terrainDepthBegin == std::string::npos || terrainWarpBegin == std::string::npos ||
      terrainDepthBegin >= terrainWarpBegin ||
      source.find("const float3 terrainMaterialDepthContrast = TerrainMesoPartitionEnabled()") == std::string::npos ||
      source.find("? TerrainMaterialDepthContrast(terrainMesoSignal,") == std::string::npos ||
      source.find("baseColor *= terrainMaterialDepthContrast;") == std::string::npos)
    return 13;
  const std::string terrainDepthSource = source.substr(
      terrainDepthBegin, terrainWarpBegin - terrainDepthBegin);
  if (terrainDepthSource.find("float3(1.12, 1.13, 1.18)") == std::string::npos ||
      terrainDepthSource.find("float3(1.02, 0.88, 0.72)") == std::string::npos ||
      terrainDepthSource.find("float3(0.70, 0.76, 0.82)") == std::string::npos ||
      terrainDepthSource.find("[branch] if (authoredWeightTotal <= 0.000001)") == std::string::npos ||
      terrainDepthSource.find("const float mesoCoordinate = saturate((mesoSignal - 0.18) * 1.5625);") == std::string::npos ||
      terrainDepthSource.find("authoredWeights /= authoredWeightTotal") != std::string::npos ||
      terrainDepthSource.find("proceduralScale.xxx * (1.0 - authoredAmount) + authoredContrast") == std::string::npos ||
      terrainDepthSource.find("cameraFogStart") != std::string::npos ||
      terrainDepthSource.find("fogEndEnabled") != std::string::npos)
    return 14;

  const auto mesoLow = EvaluateTerrainMaterialDepthContrast(0.f, {0.f, 0.f, 0.f});
  const auto mesoHigh = EvaluateTerrainMaterialDepthContrast(1.f, {0.f, 0.f, 0.f});
  const auto path = EvaluateTerrainMaterialDepthContrast(.5f, {1.f, 0.f, 0.f});
  const auto leaf = EvaluateTerrainMaterialDepthContrast(.5f, {0.f, 1.f, 0.f});
  const auto wet = EvaluateTerrainMaterialDepthContrast(.5f, {0.f, 0.f, 1.f});
  if (!(Luminance(mesoHigh) - Luminance(mesoLow) > .25f &&
        Luminance(path) > Luminance(leaf) + .18f &&
        Luminance(leaf) > Luminance(wet) + .10f &&
        wet[0] >= .70f && path[2] <= 1.18f))
    return 15;
  for (float meso : {0.f, .18f, .31f, .5f, .82f, 1.f})
    for (int pathStep = 0; pathStep <= 4; ++pathStep)
      for (int leafStep = 0; leafStep <= 4 - pathStep; ++leafStep)
        for (int wetStep = 0; wetStep <= 4 - pathStep - leafStep; ++wetStep) {
          const std::array<float, 3> weights{pathStep * .25f, leafStep * .25f,
                                             wetStep * .25f};
          const auto reference = EvaluateTerrainMaterialDepthContrastReference(meso, weights);
          const auto optimized = EvaluateTerrainMaterialDepthContrast(meso, weights);
          for (std::size_t channel = 0; channel < optimized.size(); ++channel)
            if (!Near(reference[channel], optimized[channel], 2e-6f)) return 16;
        }

  std::cout << "AerialPerspective3D PASS nearT=" << nearValue.transmittance
            << " middleT=" << middle.transmittance << " farT=" << farValue.transmittance
            << " farScatter=" << farValue.scatteredFraction
            << " legacyMutation=" << legacyFogAmount << '\n';
  return 0;
}
