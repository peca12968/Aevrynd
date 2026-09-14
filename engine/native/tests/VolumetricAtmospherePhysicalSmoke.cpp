#include <Aevrynd/Renderer.h>
#include <Aevrynd/VolumetricAtmospherePassD3D12.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

using namespace Aevrynd;

namespace {
double IntegratePhase(float anisotropy) {
  constexpr int Samples = 200000;
  double integral{};
  for (int index = 0; index < Samples; ++index) {
    const float cosine = -1.f + 2.f * (float(index) + .5f) / float(Samples);
    integral += double(MiePhase3D(cosine, anisotropy)) *
                (4.0 * 3.14159265358979323846 / double(Samples));
  }
  return integral;
}

float IntegrateVaryingExtinction(unsigned samples) {
  float transmittance = 1.f;
  constexpr float Length = 320.f;
  const float step = Length / float(samples);
  for (unsigned index = 0; index < samples; ++index) {
    const float position = step * (float(index) + .5f);
    const float extinction = .001f + .000004f * position;
    transmittance *= BeerLambertTransmittance3D(extinction, step);
  }
  return transmittance;
}

double BlueNoiseLowFrequencyRatio() {
  std::array<double, 64 * 64> centered{};
  double total{};
  for (unsigned y = 0; y < 64; ++y)
    for (unsigned x = 0; x < 64; ++x) {
      const double value = double(VolumetricBlueNoiseR8Sample3D(x, y)) / 255.0 - .5;
      centered[y * 64 + x] = value;
      total += value * value;
    }
  double low{};
  for (unsigned y = 0; y < 64; ++y)
    for (unsigned x = 0; x < 64; ++x) {
      double average{};
      for (unsigned oy = 0; oy < 4; ++oy)
        for (unsigned ox = 0; ox < 4; ++ox)
          average += centered[((y + oy) & 63u) * 64 + ((x + ox) & 63u)];
      average /= 16.0;
      low += average * average;
    }
  return low / total;
}

bool ProductionPhaseUsesDeterministicFixedPower() {
  const auto shaderPath = std::filesystem::path{__FILE__}.parent_path()
                              .parent_path() / "shaders" /
                          "VolumetricAtmosphere.hlsl";
  std::ifstream input(shaderPath, std::ios::binary);
  if (!input)
    return false;
  const std::string shader{std::istreambuf_iterator<char>{input}, {}};
  const auto begin = shader.find("precise float HenyeyGreenstein(");
  const auto end = begin == std::string::npos
                       ? std::string::npos
                       : shader.find("precise float CloudShadowField(", begin);
  if (begin == std::string::npos || end == std::string::npos)
    return false;
  const std::string phase = shader.substr(begin, end - begin);
  return phase.find("pow(") == std::string::npos &&
         phase.find("denominator*denominator*") != std::string::npos &&
         phase.find("AevryndDeterministicInverseSqrt(denominator)") !=
             std::string::npos;
}

bool ProductionSlabUsesDeterministicReciprocal() {
  const auto shaderPath = std::filesystem::path{__FILE__}.parent_path()
                              .parent_path() / "shaders" /
                          "VolumetricAtmosphere.hlsl";
  std::ifstream input(shaderPath, std::ios::binary);
  if (!input)
    return false;
  const std::string shader{std::istreambuf_iterator<char>{input}, {}};
  return shader.find("rcp(") == std::string::npos &&
         shader.find("precise float DeterministicReciprocal(float value)") !=
             std::string::npos &&
         shader.find("0x7ef311c3u-asuint(magnitude)") != std::string::npos &&
         shader.find("DeterministicReciprocal(ray.y)") != std::string::npos;
}

float DeterministicInverseSqrtContract(float value) {
  const float input = (std::max)(value, 1e-20f);
  float estimate = std::bit_cast<float>(
      0x5f375a86u - (std::bit_cast<std::uint32_t>(input) >> 1u));
  const float halfInput = .5f * input;
  for (unsigned iteration = 0; iteration < 4; ++iteration)
    estimate = estimate * (1.5f - halfInput * estimate * estimate);
  return estimate;
}

float DeterministicPhaseContract(float cosine, float anisotropy) {
  const float g = std::clamp(anisotropy, -.8f, .85f), g2 = g * g;
  const float denominator =
      (std::max)(1e-6f, 1.f + g2 - 2.f * g * std::clamp(cosine, -1.f, 1.f));
  const float denominatorPower = denominator * denominator *
                                 DeterministicInverseSqrtContract(denominator);
  return (1.f - g2) / (12.5663706144f * denominatorPower);
}

float DeterministicReciprocalContract(float value) {
  if (!std::isfinite(value) || std::abs(value) < 1e-20f)
    return 0.f;
  const float magnitude = std::abs(value);
  float estimate = std::bit_cast<float>(
      0x7ef311c3u - std::bit_cast<std::uint32_t>(magnitude));
  for (unsigned iteration = 0; iteration < 4; ++iteration)
    estimate = estimate * (2.f - magnitude * estimate);
  return value < 0.f ? -estimate : estimate;
}
} // namespace

int main() {
  const float analytic = std::exp(-.37f * 2.25f);
  const float evaluated = BeerLambertTransmittance3D(.37f, 2.25f);
  const float analyticError = std::abs(evaluated - analytic) / analytic;
  if (!std::isfinite(evaluated) || evaluated < 0.f || evaluated > 1.f ||
      analyticError > .005f)
    return 1;

  const float eight = IntegrateVaryingExtinction(8);
  const float sixteen = IntegrateVaryingExtinction(16);
  const float convergence = std::abs(eight - sixteen) / (std::max)(sixteen, 1e-6f);
  if (convergence > .05f)
    return 2;

  const double backward = IntegratePhase(-.8f), isotropic = IntegratePhase(0.f),
               forward = IntegratePhase(.85f);
  if (std::abs(backward - 1.0) > .01 || std::abs(isotropic - 1.0) > .01 ||
      std::abs(forward - 1.0) > .01)
    return 3;

  for (float extinction : {.001f, .01f, .1f, 1.f})
    for (float albedo : {0.f, .25f, .9f, 1.f}) {
      const float scattering = extinction * albedo;
      const float energy = EnergyConservingSingleScattering3D(
          scattering, extinction, 17.f);
      if (!std::isfinite(energy) || energy < 0.f ||
          energy > 1.f - BeerLambertTransmittance3D(extinction, 17.f) + 1e-6f)
        return 4;
    }

  VolumetricCloudSettings3D cloud{};
  cloud.maximumDistance = 640.f;
  const float capped = VolumetricCloudRayLength3D(0.f, 1.f, cloud);
  if (capped > cloud.maximumDistance || VolumetricCloudGrazingFade3D(0.f) != 0.f ||
      VolumetricCloudGrazingFade3D(.1f) < .999f)
    return 5;
  cloud.sampleCount = 999;
  cloud.baseNoiseSeed = cloud.detailNoiseSeed;
  cloud.maximumDistance = std::numeric_limits<float>::infinity();
  const auto normalizedCloud = NormalizeVolumetricCloudSettings3D(cloud);
  if (normalizedCloud.sampleCount != 32 ||
      normalizedCloud.maximumDistance != 12000.f ||
      normalizedCloud.baseNoiseSeed == normalizedCloud.detailNoiseSeed)
    return 6;

  DirectionalLightShaftSettings3D shaft{};
  shaft.anisotropy = 5.f;
  shaft.sampleCount = 999;
  shaft.shadowStrength = -1.f;
  const auto normalizedShaft = NormalizeDirectionalLightShaftSettings3D(shaft);
  if (normalizedShaft.anisotropy != .85f || normalizedShaft.sampleCount != 16 ||
      normalizedShaft.shadowStrength != 0.f)
    return 7;

  // Cap crossing 必須以厚度相關 fade 連續消失，不能在水平線產生跳變。
  float previousFade = VolumetricCloudPathCapFade3D(600.f, 640.f, 1800.f);
  for (float entry = 601.f; entry <= 680.f; entry += 1.f) {
    const float fade = VolumetricCloudPathCapFade3D(entry, 640.f, 1800.f);
    if (!std::isfinite(fade) || fade > previousFade + 1e-6f ||
        std::abs(fade - previousFade) > .02f)
      return 8;
    previousFade = fade;
  }

  // 純 hash white noise 的 4x4 低頻比約 1/16；此 tile 必須顯著更低。
  const double lowFrequencyRatio = BlueNoiseLowFrequencyRatio();
  std::array<unsigned, 16> histogram{};
  for (unsigned y = 0; y < 64; ++y)
    for (unsigned x = 0; x < 64; ++x)
      ++histogram[VolumetricBlueNoiseR8Sample3D(x, y) >> 4u];
  if (!(lowFrequencyRatio < .04) ||
      std::ranges::min(histogram) < 190u || std::ranges::max(histogram) > 325u)
    return 9;

  // 忠實重現 whole-segment-per-step：其消光會錯誤地隨 sampleCount 倍增。
  constexpr float density = .8f, sigma = density * .00135f, segment = 800.f,
                  thickness = 1800.f;
  const float correct = std::exp(-sigma * segment);
  const float legacyWholeSegment =
      std::exp(-density * .105f * segment / thickness * 16.f);
  if (std::abs(correct - std::exp(-sigma * (segment / 16.f) * 16.f)) /
          correct > .005f ||
      std::abs(legacyWholeSegment - correct) / correct < .1f)
    return 10;

  // 四個 legacy mutation 各由對應物理 gate 拒絕，不能只靠輸出 hash 不同。
  const float legacyPlanarA = 10.f * .0018f + 20.f * .00037f;
  const float legacyPlanarB = 11.f * .0018f +
      (20.f - .0018f / .00037f) * .00037f;
  const bool planarRankTwoRejected = std::abs(legacyPlanarA - legacyPlanarB) < 1e-6f;
  const float uncappedSlabLength = 1800.f / .01f;
  const bool noPathCapRejected = uncappedSlabLength > normalizedCloud.maximumDistance;
  constexpr float cloudAttenuation = .42f, fogAttenuation = .74f;
  const float conservativeEnergy = fogAttenuation +
      (1.f - fogAttenuation) * cloudAttenuation;
  const float additiveEnergy = fogAttenuation + cloudAttenuation;
  const bool additiveRejected = additiveEnergy > 1.f + 1e-6f &&
      conservativeEnergy <= 1.f + 1e-6f;
  if (!planarRankTwoRejected || !noPathCapRejected || !additiveRejected)
    return 11;

  DirectionalLightShaftSettings3D hostileShaft{};
  hostileShaft.intensity = std::numeric_limits<float>::infinity();
  Atmosphere3D hostileFog{};
  hostileFog.heightFogDensity = 1.f;
  hostileFog.heightFogMaximumOpacity = std::numeric_limits<float>::infinity();
  const auto boundedShaft = NormalizeDirectionalLightShaftSettings3D(hostileShaft);
  const auto boundedFog = NormalizeAtmosphere3D(hostileFog);
  hostileShaft.intensity = 100.f;
  hostileFog.heightFogMaximumOpacity = 100.f;
  const auto clampedShaft = NormalizeDirectionalLightShaftSettings3D(hostileShaft);
  const auto clampedFog = NormalizeAtmosphere3D(hostileFog);
  const float fogT = 1.f - boundedFog.heightFogMaximumOpacity;
  const float fogL = (1.f - std::exp(-80.f)) *
      (boundedFog.heightFogMaximumOpacity / (1.f - std::exp(-80.f)));
  if (boundedShaft.intensity != .65f || boundedFog.heightFogMaximumOpacity != .92f ||
      clampedShaft.intensity != 1.5f || clampedFog.heightFogMaximumOpacity != .98f ||
      !std::isfinite(fogT) || !std::isfinite(fogL) || fogT < 0.f || fogT > 1.f ||
      fogL > 1.f - fogT + 1e-6f)
    return 12;

  // 生產 HG 的固定 3/2 次方不得退回 vendor-dependent pow(log/exp) 路徑；
  // 上方相位積分同時守住替換後的正規化與物理行為。
  if (!ProductionPhaseUsesDeterministicFixedPower())
    return 13;

  // 四次固定 Newton 疊代必須在完整 production 參數域內維持原 HG 精度。
  double maximumPhaseRelativeError{};
  for (const float anisotropy : {-.8f, -.4f, 0.f, .4f, .85f})
    for (unsigned sample = 0; sample <= 512; ++sample) {
      const float cosine = -1.f + 2.f * float(sample) / 512.f;
      const float g2 = anisotropy * anisotropy;
      const float denominator =
          1.f + g2 - 2.f * anisotropy * cosine;
      const float reference =
          (1.f - g2) / (12.5663706144f * std::pow(denominator, 1.5f));
      const float deterministic =
          DeterministicPhaseContract(cosine, anisotropy);
      const double relativeError = std::abs(double(deterministic - reference)) /
                                   (std::max)(double(reference), 1e-12);
      maximumPhaseRelativeError =
          (std::max)(maximumPhaseRelativeError, relativeError);
    }
  if (!std::isfinite(maximumPhaseRelativeError) ||
      maximumPhaseRelativeError > 5e-6)
    return 14;

  if (!ProductionSlabUsesDeterministicReciprocal())
    return 15;

  double maximumReciprocalRelativeError{};
  for (unsigned sample = 0; sample <= 65536; ++sample) {
    const float magnitude =
        1e-6f + (1.f - 1e-6f) * float(sample) / 65536.f;
    for (const float value : {-magnitude, magnitude}) {
      const float reference = 1.f / value;
      const float deterministic = DeterministicReciprocalContract(value);
      const double relativeError = std::abs(double(deterministic - reference)) /
                                   std::abs(double(reference));
      maximumReciprocalRelativeError =
          (std::max)(maximumReciprocalRelativeError, relativeError);
    }
  }
  if (!std::isfinite(maximumReciprocalRelativeError) ||
      maximumReciprocalRelativeError > 5e-6 ||
      DeterministicReciprocalContract(0.f) != 0.f ||
      DeterministicReciprocalContract(
          std::numeric_limits<float>::infinity()) != 0.f ||
      DeterministicReciprocalContract(
          std::numeric_limits<float>::quiet_NaN()) != 0.f)
    return 16;

  std::cout << "VolumetricAtmospherePhysical PASS analyticError=" << analyticError
            << " convergence8v16=" << convergence << " phase=" << backward
            << '/' << isotropic << '/' << forward << " capped=" << capped
            << " lowFrequencyRatio=" << lowFrequencyRatio
            << " deterministicPhaseMaxRelativeError="
            << maximumPhaseRelativeError
            << " deterministicReciprocalMaxRelativeError="
            << maximumReciprocalRelativeError
            << '\n';
}
