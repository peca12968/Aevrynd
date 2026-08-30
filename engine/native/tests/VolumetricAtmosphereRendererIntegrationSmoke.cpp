#include <Aevrynd/Renderer.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace Aevrynd;

namespace {
std::uint64_t HashFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::uint64_t hash = 1469598103934665603ull;
  for (char value{}; input.get(value);)
    hash = (hash ^ std::uint8_t(value)) * 1099511628211ull;
  return hash;
}
} // namespace

int main() {
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  const wchar_t *className = L"AevryndVolumetricAtmosphereRendererGate";
  WNDCLASSW windowClass{};
  windowClass.lpfnWndProc = DefWindowProcW;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = className;
  RegisterClassW(&windowClass);
  const HWND window = CreateWindowW(className, L"Volumetric atmosphere gate",
                                    WS_OVERLAPPED, 0, 0, 640, 360, nullptr,
                                    nullptr, instance, nullptr);
  if (!window)
    return 1;

  const auto output = std::filesystem::temp_directory_path() /
                      "aevrynd-volumetric-atmosphere-renderer-gate";
  std::filesystem::create_directories(output);
  const std::array phases{VolumetricAtmospherePhase3D::Day,
                          VolumetricAtmospherePhase3D::Dusk,
                          VolumetricAtmospherePhase3D::Night};
  const std::array names{"day.png", "dusk.png", "night.png"};
  std::array<std::uint64_t, 3> hashes{};
  std::array<std::uint64_t, 3> gpuTimes{};
  std::size_t residentBytes{};

  int result{};
  {
    D3D12Renderer renderer;
    renderer.Initialize(window, 640, 360, 640, 360, false);
    renderer.SetQualitySettings({false, false, false, false});
    DrawList draw{DrawListCapacityProfile::Minimal2D};
    draw.SetViewProjection(Matrix4::Identity());
    draw.SetBackfaceCulling(false);
    draw.SetAtmosphere({{.025f, .04f, .075f, 1}, {0, 1.6f, -4}, 4, 120,
                        true, .008f, 0, .02f, .85f});
    draw.SetVolumetricClouds({true, 1200, 1800, .42f, 1.35f, .2f, 24, 16,
                              true, .72f});
    draw.SetDirectionalLightShafts({true, 1.1f, .72f, 320, 4});
    draw.SetDirectionalLight({{-.35f, -.82f, .44f}, .12f,
                              {1, .9f, .72f, 1}, 2.2f});
    draw.WorldTriangle({-.72f, -.82f, .55f}, {0, .45f, .55f},
                       {.72f, -.82f, .55f}, {.16f, .22f, .12f, 1});

    for (std::size_t index = 0; index < phases.size(); ++index) {
      draw.SetVolumetricAtmospherePhase(phases[index]);
      draw.SetEnvironmentTime(float(index) * 3.5f);
      for (int warmup = 0; warmup < 12; ++warmup)
        renderer.Render(draw, {.015f, .025f, .05f, 1});
      const auto capture = output / names[index];
      std::error_code error;
      std::filesystem::remove(capture, error);
      if (!renderer.CaptureNextFrame(capture)) {
        result = 2;
        break;
      }
      renderer.Render(draw, {.015f, .025f, .05f, 1});
      renderer.WaitIdle();
      const auto stats = renderer.GetStats();
      if (!std::filesystem::exists(capture) ||
          !std::filesystem::file_size(capture) ||
          !stats.volumetricAtmosphereConfigured ||
          !stats.volumetricAtmosphereDispatched ||
          stats.volumetricAtmospherePhase != phases[index] ||
          stats.volumetricAtmosphereWidth != 320 ||
          stats.volumetricAtmosphereHeight != 180 ||
          !stats.volumetricAtmosphereThreadGroups ||
          !stats.volumetricAtmosphereResidentBytes ||
          !stats.volumetricAtmosphereDispatches ||
          !stats.volumetricAtmosphereCompositePasses ||
          !stats.lastVolumetricAtmosphereGpuNanoseconds ||
          stats.debugErrorCount || stats.debugWarningCount ||
          (stats.gpuValidationRequested &&
           (!stats.gpuValidationAvailable || !stats.gpuValidationExecuted ||
            !stats.gpuValidationPassed))) {
        result = 3;
        break;
      }
      hashes[index] = HashFile(capture);
      gpuTimes[index] = stats.lastVolumetricAtmosphereGpuNanoseconds;
      residentBytes = stats.volumetricAtmosphereResidentBytes;
    }
    if (!result && (hashes[0] == hashes[1] || hashes[0] == hashes[2] ||
                    hashes[1] == hashes[2]))
      result = 4;

    if (!result) {
      renderer.Resize(800, 450);
      renderer.Render(draw, {.015f, .025f, .05f, 1});
      renderer.WaitIdle();
      const auto resized = renderer.GetStats();
      if (resized.volumetricAtmosphereWidth != 400 ||
          resized.volumetricAtmosphereHeight != 225 ||
          resized.volumetricAtmosphereResourceCreates < 4 ||
          resized.volumetricAtmosphereResourceReleases < 2)
        result = 5;
    }

    if (!result) {
      auto disabled = draw.VolumetricClouds();
      disabled.enabled = false;
      draw.SetVolumetricClouds(disabled);
      auto shafts = draw.DirectionalLightShafts();
      shafts.enabled = false;
      draw.SetDirectionalLightShafts(shafts);
      renderer.Render(draw, {.015f, .025f, .05f, 1});
      renderer.WaitIdle();
      const auto off = renderer.GetStats();
      if (off.volumetricAtmosphereConfigured ||
          off.volumetricAtmosphereDispatched ||
          off.volumetricAtmosphereThreadGroups ||
          off.lastVolumetricAtmosphereGpuNanoseconds)
        result = 6;
      renderer.TrimTransientMemory();
      const auto released = renderer.GetStats();
      if (released.volumetricAtmosphereResidentBytes ||
          released.volumetricAtmosphereResourceReleases < 4)
        result = 7;
    }
  }
  DestroyWindow(window);
  std::cout << "VolumetricAtmosphereRendererIntegration "
            << (result ? "FAIL" : "PASS") << " dayHash=" << hashes[0]
            << " duskHash=" << hashes[1] << " nightHash=" << hashes[2]
            << " dayGpuNs=" << gpuTimes[0] << " duskGpuNs=" << gpuTimes[1]
            << " nightGpuNs=" << gpuTimes[2]
            << " residentBytes=" << residentBytes
            << '\n';
  return result;
}
