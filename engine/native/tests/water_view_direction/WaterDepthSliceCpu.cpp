#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

namespace {
#include "WaterDepthValidity.inc"

float Saturate(float value) { return std::clamp(value, 0.f, 1.f); }

float Smoothstep(float low, float high, float value) {
  const float weight = Saturate((value - low) / (high - low));
  return weight * weight * (3.f - 2.f * weight);
}

float ExpectedDepthSlice(float shoreDepth, float depthValid) {
  const float boundedDepth = std::clamp(shoreDepth, 0.f, 8.f);
  const float depthSlice = Smoothstep(.06f, 1.35f, boundedDepth);
  return 1.f + (depthSlice - 1.f) * Saturate(depthValid);
}

float ExpectedRoughness(float minimumRoughness, float maximumRoughness,
                        float resolvedSlope, float unresolvedDetail,
                        float foamCoverage, float depthSlice) {
  const float weight = Saturate(.12f + resolvedSlope * 2.25f +
                                unresolvedDetail * .30f + foamCoverage * .70f +
                                (1.f - Saturate(depthSlice)) * .32f);
  return minimumRoughness + (maximumRoughness - minimumRoughness) * weight;
}

bool Contains(const std::string& source, const char* text) {
  return source.find(text) != std::string::npos;
}
}

int main() {
  const std::filesystem::path shader = std::filesystem::path{AEVRYND_SOURCE_DIR} /
      "engine" / "native" / "shaders" / "World.hlsl";
  std::ifstream stream(shader, std::ios::binary);
  const std::string source((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());

  const auto sliceBegin = source.find("float WaterDepthSlice3D(");
  const auto roughnessBegin = source.find("float WaterReflectionRoughness3D(");
  const auto bandLimitBegin = source.find("float WaterWavePixelBandLimit3D(");
  if (sliceBegin == std::string::npos || roughnessBegin == std::string::npos ||
      bandLimitBegin == std::string::npos || sliceBegin >= roughnessBegin ||
      roughnessBegin >= bandLimitBegin) {
    std::cerr << "missing production water depth-slice helper\n";
    return 1;
  }

  const std::string helperSource = source.substr(sliceBegin, bandLimitBegin - sliceBegin);
  if (!Contains(helperSource, "smoothstep(0.06, 1.35, boundedDepth)") ||
      !Contains(helperSource, "lerp(1.0, depthSlice, saturate(depthValid))") ||
      !Contains(helperSource, "(1.0 - saturate(depthSlice)) * 0.32") ||
      Contains(helperSource, ".Sample") || Contains(helperSource, "CloudNoise") ||
      Contains(helperSource, "cameraFogStart") || Contains(helperSource, "exposure")) {
    std::cerr << "depth slice is not bounded, physical, or texture-free\n";
    return 2;
  }

  const auto compositeBegin = source.find("[branch] if (waterCompositeMask > 0.5)");
  const auto aerialBegin = source.find("const float surfaceWeight", compositeBegin);
  if (compositeBegin == std::string::npos || aerialBegin == std::string::npos) return 3;
  const std::string composite = source.substr(compositeBegin, aerialBegin - compositeBegin);
  if (!Contains(composite, "WaterDepthSlice3D(shoreDepth, waterDepthValid)") ||
      !Contains(composite, "waterReflectionNormal") ||
      !Contains(composite, "dot(waterReflectionNormal, viewDirection)") ||
      !Contains(composite, "reflect(-viewDirection, waterReflectionNormal)") ||
      !Contains(composite, "EvaluateWaterScreenSpaceReflection(input.worldPosition,waterReflectionNormal") ||
      !Contains(composite, "EvaluateEnvironmentSpecular(input.worldPosition,reflect(-viewDirection, waterReflectionNormal)") ||
      !Contains(source, "const float3 atmosphereBase = EvaluatePhaseCoherentSkyRadiance(blurredDirection);") ||
      Contains(composite, "float3(0.23, 0.48, 1.0)")) {
    std::cerr << "water reflection does not share one depth-sliced normal and phase sky\n";
    return 4;
  }

  float previous = -1.f;
  for (int index = 0; index <= 80; ++index) {
    const float value = ExpectedDepthSlice(index * .025f, 1.f);
    if (!std::isfinite(value) || value < previous || value < 0.f || value > 1.f) return 5;
    previous = value;
  }
  const float invalid = ExpectedDepthSlice(0.f, 0.f);
  const float shore = ExpectedDepthSlice(0.f, 1.f);
  const float transition = ExpectedDepthSlice(.705f, 1.f);
  const float deep = ExpectedDepthSlice(1.35f, 1.f);
  const float shallowRoughness = ExpectedRoughness(.055f, .20f, 0.f, 0.f, 0.f, shore);
  const float deepRoughness = ExpectedRoughness(.055f, .20f, 0.f, 0.f, 0.f, deep);
  if (invalid != 1.f || shore != 0.f || std::abs(transition - .5f) > 1e-5f ||
      deep != 1.f || shallowRoughness <= deepRoughness ||
      shallowRoughness > .20001f || deepRoughness < .05499f) return 6;

  struct ValidityCase {
    float verticalDepth;
    float measuredThickness;
    float expected;
  };
  const ValidityCase validityCases[] = {
      {0.f, 0.f, 1.f},
      {0.75f, 1.25f, 1.f},
      {-0.02f, 0.5f, 1.f},
      {-0.0201f, 0.5f, 0.f},
      {-1.f, 1.f, 0.f},
      {100000.f, 1.f, 0.f},
      {1.f, 100000.f, 0.f},
      {std::numeric_limits<float>::infinity(), 1.f, 0.f},
      {-std::numeric_limits<float>::infinity(), 1.f, 0.f},
      {std::numeric_limits<float>::quiet_NaN(), 1.f, 0.f},
      {1.f, std::numeric_limits<float>::quiet_NaN(), 0.f},
  };
  for (const ValidityCase& test : validityCases) {
    const float actual = WaterDepthSampleValidity3D(test.verticalDepth,
                                                     test.measuredThickness);
    if (actual != test.expected) {
      std::cerr << "production depth validity mismatch vertical="
                << test.verticalDepth << " thickness=" << test.measuredThickness
                << " actual=" << actual << " expected=" << test.expected << '\n';
      return 7;
    }
  }

  std::cout << "WaterDepthSlice PASS invalid/shore/mid/deep=" << invalid << '/'
            << shore << '/' << transition << '/' << deep << " roughness="
            << shallowRoughness << '/' << deepRoughness << '\n';
}
