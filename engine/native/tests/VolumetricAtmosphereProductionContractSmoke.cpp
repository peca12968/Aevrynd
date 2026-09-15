#include <Aevrynd/Camera.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/Renderer.h>
#include <Windows.h>
#include <algorithm>
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
float DeterministicInverseSqrtContract(float value) {
  const float input = (std::max)(value, 1e-20f);
  float estimate = std::bit_cast<float>(
      0x5f375a86u - (std::bit_cast<std::uint32_t>(input) >> 1u));
  const float halfInput = .5f * input;
  for (unsigned iteration = 0; iteration < 4; ++iteration)
    estimate = estimate * (1.5f - halfInput * estimate * estimate);
  return estimate;
}

float DeterministicReciprocalContract(float value) {
  if (!std::isfinite(value) || std::abs(value) < 1e-20f)
    return 0.f;

  const auto valueBits = std::bit_cast<std::uint32_t>(value);
  const auto magnitudeBits = valueBits & 0x7fffffffu;
  const auto exponent = (magnitudeBits >> 23u) & 0xffu;
  const auto normalizedBits =
      0x3f800000u | (magnitudeBits & 0x007fffffu);
  const float normalizedMagnitude =
      std::bit_cast<float>(normalizedBits);
  float estimate = std::bit_cast<float>(
      0x7ef311c3u - normalizedBits);
  for (unsigned iteration = 0; iteration < 4; ++iteration)
    estimate = estimate * (2.f - normalizedMagnitude * estimate);

  const float exponentScale = exponent < 254u
                                  ? std::bit_cast<float>((254u - exponent) << 23u)
                                  : std::bit_cast<float>(0x00800000u) * .5f;
  estimate *= exponentScale;
  return (valueBits & 0x80000000u) != 0u ? -estimate : estimate;
}

bool CompositeDeterministicArithmeticContract(double &maximumInverseSqrtError,
                                              double &maximumReciprocalError) {
  const auto shaderPath = std::filesystem::path{__FILE__}.parent_path()
                              .parent_path() / "shaders" /
                          "VolumetricAtmosphereComposite.hlsl";
  std::ifstream input(shaderPath, std::ios::binary);
  if (!input)
    return false;
  const std::string shader{std::istreambuf_iterator<char>{input}, {}};
  const bool sourceContract =
      shader.find("normalize(") == std::string::npos &&
      shader.find("rcp(") == std::string::npos &&
      shader.find("AevryndDeterministicNormalize(rayVector)") !=
          std::string::npos &&
      shader.find("precise float2 CompositeViewportUv(uint2 pixel)") !=
          std::string::npos &&
      shader.find("precise float2 CompositeAtmosphereTexelUv(") !=
          std::string::npos &&
      shader.find("CompositeDeterministicReciprocal(ray.y)") !=
          std::string::npos &&
      shader.find("CompositeDeterministicReciprocal(accumulatedWeight)") !=
          std::string::npos &&
      shader.find("precise float SceneAtmosphereWeight(") !=
          std::string::npos &&
      shader.find("precise float4 SampleDepthAwareAtmosphere(") !=
          std::string::npos &&
      shader.find("precise float4 CompositeSceneColor(") !=
          std::string::npos &&
      shader.find("precise float4 VolumetricAtmosphereCompositePixelMain(") !=
          std::string::npos &&
      shader.find("precise float4 VolumetricAtmosphereCompositeBloomSemanticPixelMain(") !=
          std::string::npos &&
      shader.find("precise const float4 output=CompositeSceneColor(pixel,sampleIndex,false)") !=
          std::string::npos &&
      shader.find("precise const float4 output=CompositeSceneColor(pixel,sampleIndex,true)") !=
          std::string::npos &&
      shader.find("accumulated.r=accumulated.r+weightedSample.r") !=
          std::string::npos &&
      shader.find("rgb.r=atmosphereRgb.r+scaledScene.r") !=
          std::string::npos &&
      shader.find("scene.rgb*inverseAlpha") == std::string::npos &&
      shader.find("atmosphereSample*bilateralWeight") == std::string::npos;
  if (!sourceContract)
    return false;

  maximumInverseSqrtError = 0.0;
  for (unsigned exponent = 61; exponent <= 190; ++exponent)
    for (unsigned mantissaSample = 0; mantissaSample <= 512;
         ++mantissaSample) {
      const auto mantissa = std::uint32_t(
          (std::uint64_t{mantissaSample} * 0x7fffffu) / 512u);
      const float value =
          std::bit_cast<float>((exponent << 23u) | mantissa);
      const float reference = 1.f / std::sqrt(value);
      const float deterministic = DeterministicInverseSqrtContract(value);
      const double relativeError = std::abs(double(deterministic - reference)) /
                                   double(reference);
      maximumInverseSqrtError =
          (std::max)(maximumInverseSqrtError, relativeError);
    }

  maximumReciprocalError = 0.0;
  for (unsigned sample = 0; sample <= 65536; ++sample) {
    const float magnitude =
        1e-6f + (1.f - 1e-6f) * float(sample) / 65536.f;
    for (const float value : {-magnitude, magnitude}) {
      const float reference = 1.f / value;
      const float deterministic = DeterministicReciprocalContract(value);
      const double relativeError = std::abs(double(deterministic - reference)) /
                                   std::abs(double(reference));
      maximumReciprocalError =
          (std::max)(maximumReciprocalError, relativeError);
    }
  }
  for (unsigned exponent = 61; exponent <= 254; ++exponent) {
    for (unsigned mantissaSample = 0; mantissaSample <= 512;
         ++mantissaSample) {
      const auto mantissa = std::uint32_t(
          (std::uint64_t{mantissaSample} * 0x7fffffu) / 512u);
      const float magnitude =
          std::bit_cast<float>((exponent << 23u) | mantissa);
      for (const float value : {-magnitude, magnitude}) {
        const float reference = 1.f / value;
        const float deterministic = DeterministicReciprocalContract(value);
        if (deterministic == 0.f)
          return false;
        const double relativeError =
            std::abs(double(deterministic - reference)) /
            std::abs(double(reference));
        maximumReciprocalError =
            (std::max)(maximumReciprocalError, relativeError);
      }
    }
  }
  const float maximumFinite = (std::numeric_limits<float>::max)();
  return std::isfinite(maximumInverseSqrtError) &&
         maximumInverseSqrtError <= 5e-6 &&
         std::isfinite(maximumReciprocalError) &&
         maximumReciprocalError <= 5e-6 &&
         std::isfinite(DeterministicReciprocalContract(maximumFinite)) &&
         std::isfinite(DeterministicReciprocalContract(-maximumFinite)) &&
         DeterministicReciprocalContract(maximumFinite) >= 0.f &&
         DeterministicReciprocalContract(-maximumFinite) <= 0.f &&
         DeterministicReciprocalContract(0.f) == 0.f &&
         DeterministicReciprocalContract(
             std::numeric_limits<float>::infinity()) == 0.f &&
         DeterministicReciprocalContract(
             std::numeric_limits<float>::quiet_NaN()) == 0.f;
}

bool MultisampleStageDiagnosticSourceContract() {
  const auto shaderPath = std::filesystem::path{__FILE__}.parent_path()
                              .parent_path() / "shaders" /
                          "DeterministicMsaaResolve.hlsl";
  std::ifstream input(shaderPath, std::ios::binary);
  if (!input)
    return false;
  const std::string shader{std::istreambuf_iterator<char>{input}, {}};
  const auto validates = [](const std::string &source) {
    return source.find("float4 DeterministicMsaaResolvePixelMain(") !=
             std::string::npos &&
         source.find("multisampleHdr.Load(pixel, 0)") != std::string::npos &&
         source.find("multisampleHdr.Load(pixel, 1)") != std::string::npos &&
         source.find("multisampleHdr.Load(pixel, 2)") != std::string::npos &&
         source.find("multisampleHdr.Load(pixel, 3)") != std::string::npos &&
         source.find("sum * 0.25") != std::string::npos &&
         source.find("uint diagnosticSample;") != std::string::npos &&
         source.find("float4 DeterministicMsaaSampleDiagnosticPixelMain(") !=
             std::string::npos &&
         source.find("return multisampleHdr.Load(pixel, diagnosticSample);") !=
             std::string::npos &&
         source.find("f32tof16") == std::string::npos &&
         source.find("f16tof32") == std::string::npos &&
         source.find("round(") == std::string::npos;
  };
  const auto hostileRejected = [&](std::string before, std::string after) {
    auto mutation = shader;
    const auto position = mutation.find(before);
    if (position == std::string::npos)
      return false;
    mutation.replace(position, before.size(), after);
    return !validates(mutation);
  };
  return validates(shader) &&
         hostileRejected("sum * 0.25", "sum * 0.5") &&
         hostileRejected("multisampleHdr.Load(pixel, diagnosticSample)",
                         "multisampleHdr.Load(pixel, 0)") &&
         hostileRejected("return multisampleHdr.Load(pixel, diagnosticSample);",
                         "return f32tof16(multisampleHdr.Load(pixel, diagnosticSample));");
}

bool ContactShadowInvalidHistorySourceContract() {
  const auto shaderPath = std::filesystem::path{__FILE__}.parent_path()
                              .parent_path() / "shaders" /
                          "ContactShadowTemporal.hlsl";
  std::ifstream input(shaderPath, std::ios::binary);
  if (!input)
    return false;
  const std::string shader{std::istreambuf_iterator<char>{input}, {}};
  const auto validates = [](const std::string &source) {
    const auto current = source.find("current=currentContact.Load");
    const auto valid = source.find("const bool validFlags=");
    const auto reject = source.find("if(!validFlags)");
    const auto rejectedWrite = source.find(
        "temporalContact[id.xy]=float2(currentVisibility,currentDepth);");
    const auto rejectedReturn = source.find("return;", rejectedWrite);
    const auto previous = source.find("previousContact.Load");
    return current != std::string::npos && valid != std::string::npos &&
           reject != std::string::npos && rejectedWrite != std::string::npos &&
           rejectedReturn != std::string::npos && previous != std::string::npos &&
           current < valid && valid < reject && reject < rejectedWrite &&
           rejectedWrite < rejectedReturn && rejectedReturn < previous;
  };
  auto hostile = shader;
  const auto guard = hostile.find("if(!validFlags)");
  if (guard == std::string::npos)
    return false;
  hostile.replace(guard, sizeof("if(!validFlags)") - 1, "if(false)");
  return validates(shader) && !validates(hostile);
}

bool ContactShadowTemporalBypassDiagnosticSourceContract() {
  const auto rendererPath = std::filesystem::path{__FILE__}.parent_path()
                                .parent_path() / "src" / "D3D12Renderer.cpp";
  std::ifstream input(rendererPath, std::ios::binary);
  if (!input)
    return false;
  const std::string source{std::istreambuf_iterator<char>{input}, {}};
  const auto environment = source.find(
      "AEVRYND_CONTACT_SHADOW_TEMPORAL_DIAGNOSTIC_OFF");
  const auto strictValue = source.find("contactTemporalDiagnostic[0]!=L'1'",
                                       environment);
  const auto assignment = source.find(
      "d.contactShadowTemporalDiagnosticBypass=contactTemporalDiagnosticLength==1",
      strictValue);
  const auto use = source.find(
      "if(d.contactShadowTemporalDiagnosticBypass)",
      assignment);
  const auto currentSignal = source.find(
      "CreateShaderResourceView(d.ambientOcclusionContactCurrent.Get()",
      use);
  return environment != std::string::npos && strictValue != std::string::npos &&
         assignment != std::string::npos && use != std::string::npos &&
         currentSignal != std::string::npos && environment < strictValue &&
         strictValue < assignment && assignment < use && use < currentSignal;
}
} // namespace

int main() {
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  std::cerr << "Production contract must compile without internal hooks\n";
  return 1;
#else
  double maximumInverseSqrtError{}, maximumReciprocalError{};
  if (!CompositeDeterministicArithmeticContract(maximumInverseSqrtError,
                                                maximumReciprocalError)) {
    std::cerr << "Composite deterministic arithmetic contract failed\n";
    return 5;
  }
  if (!MultisampleStageDiagnosticSourceContract()) {
    std::cerr << "MSAA sample diagnostic source contract failed\n";
    return 6;
  }
  if (!ContactShadowInvalidHistorySourceContract()) {
    std::cerr << "Contact-shadow invalid-history source contract failed\n";
    return 7;
  }
  if (!ContactShadowTemporalBypassDiagnosticSourceContract()) {
    std::cerr << "Contact-shadow temporal-bypass diagnostic source contract failed\n";
    return 8;
  }
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  const wchar_t *className = L"AevryndVolumetricAtmosphereProductionContract";
  WNDCLASSW windowClass{};
  windowClass.lpfnWndProc = DefWindowProcW;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = className;
  RegisterClassW(&windowClass);
  const HWND window = CreateWindowW(className, L"production contract",
                                    WS_OVERLAPPED, 0, 0, 320, 180, nullptr,
                                    nullptr, instance, nullptr);
  if (!window)
    return 2;

  int result{};
  {
    D3D12Renderer renderer;
    renderer.Initialize(window, 320, 180, 320, 180, false);
    const auto initialized = renderer.GetStats();
    if (initialized.volumetricAtmosphereDiagnosticsAvailable ||
        initialized.volumetricAtmosphereMutationAvailable)
      result = 3;

    // 舊環境變數在 production build 不可啟用 mutation。
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_LEGACY_DEPTH_TEST", L"1");
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_NEAREST_TEST", L"1");
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_LEGACY_PLANAR_NOISE_TEST", L"1");
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_NO_PATH_CAP_TEST", L"1");
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_ADDITIVE_SHAFT_TEST", L"1");
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_WHOLE_SEGMENT_TEST", L"1");
    DrawList draw{DrawListCapacityProfile::Minimal2D};
    Camera3D camera{{0, 90, -220}, {0, 130, 420}, {0, 1, 0}, {}};
    camera.projection.farPlane = 2400.f;
    draw.SetViewProjection(CreateViewProjectionMatrix(camera, 16.f / 9.f));
    draw.SetAtmosphere({{.025f, .04f, .075f, 1}, camera.position, 210.f,
                        360.f, true, .008f, 0, .02f, .85f});
    draw.SetVolumetricClouds(
        {true, 210, 360, .05f, 4.f, 0.f, 24, 14, false, 0.f});
    draw.SetDirectionalLightShafts({true, 2.f, .72f, 520, 4});
    const Mesh3D box = Mesh3D::CreateUnitBox();
    const MeshHandle3D mesh = renderer.CreateMesh(box.vertices, box.indices);
    if (!mesh)
      result = 4;
    draw.MeshInstance(mesh, CreateTransformMatrix({0, -10, 500}, {},
                                                   {900, 20, 1200}),
                      {.2f, .31f, .14f, 1}, {.86f, 0, 0, 1},
                      MeshRenderLayer3D::Opaque);
    for (unsigned frame = 0; frame < 8; ++frame)
      renderer.Render(draw, {.015f, .025f, .05f, 1});
    renderer.WaitIdle();
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_LEGACY_DEPTH_TEST", nullptr);
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_NEAREST_TEST", nullptr);
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_LEGACY_PLANAR_NOISE_TEST", nullptr);
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_NO_PATH_CAP_TEST", nullptr);
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_ADDITIVE_SHAFT_TEST", nullptr);
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_WHOLE_SEGMENT_TEST", nullptr);

    const auto stats = renderer.GetStats();
    if (!stats.volumetricAtmosphereDispatched ||
        stats.volumetricAtmosphereDiagnosticsAvailable ||
        stats.volumetricAtmosphereDiagnosticsEnabled ||
        stats.volumetricAtmosphereMutationAvailable ||
        stats.volumetricAtmosphereTemporalAcceptedPixels ||
        stats.volumetricAtmosphereTemporalRejectedPixels ||
        stats.volumetricAtmosphereTemporalInvalidPixels ||
        stats.volumetricAtmosphereTemporalDiagnosticFrames ||
        stats.volumetricAtmosphereResourceCreates != 5 ||
        !stats.volumetricAtmosphereAppliedFingerprint ||
        stats.volumetricAtmosphereAppliedCloudSamples != 14 ||
        stats.volumetricAtmosphereAppliedShaftSamples != 4 ||
        stats.volumetricAtmosphereBlueNoiseBytes != 4096 ||
        stats.debugErrorCount || stats.debugWarningCount)
      result = 4;
    std::cout << "production diagnosticsAvailable="
              << stats.volumetricAtmosphereDiagnosticsAvailable
              << " mutationAvailable="
              << stats.volumetricAtmosphereMutationAvailable
              << " counters="
              << stats.volumetricAtmosphereTemporalAcceptedPixels << '/'
              << stats.volumetricAtmosphereTemporalRejectedPixels << '/'
              << stats.volumetricAtmosphereTemporalInvalidPixels
              << " diagnosticFrames="
              << stats.volumetricAtmosphereTemporalDiagnosticFrames
              << " resources=" << stats.volumetricAtmosphereResourceCreates
              << " inverseSqrtError=" << maximumInverseSqrtError
              << " reciprocalError=" << maximumReciprocalError
              << '\n';
    renderer.DestroyMesh(mesh);
    renderer.WaitIdle();
  }
  DestroyWindow(window);
  return result;
#endif
}
