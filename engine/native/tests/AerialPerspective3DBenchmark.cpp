#include "Aevrynd/Renderer.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace Aevrynd;

namespace {
uint64_t Median(std::vector<uint64_t> values) {
  if (values.empty()) return 0;
  std::ranges::sort(values);
  return values[values.size() / 2];
}
}

int main() {
  HWND window = CreateWindowExW(0, L"STATIC", L"AevryndAerialPerspectiveBenchmark",
                                WS_OVERLAPPED, 0, 0, 1280, 720, nullptr, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
  if (!window) return 1;
  int result{};
  {
    D3D12Renderer renderer;
    renderer.Initialize(window, 1280, 720, 1280, 720, false);
    renderer.SetQualitySettings({false, false, false, false});
    DrawList draw{DrawListCapacityProfile::Minimal2D};
    draw.WorldTriangle({-1.f, -1.f, .5f}, {0.f, 1.f, .5f}, {1.f, -1.f, .5f},
                       {.92f, .88f, .78f, 1.f});
    draw.SetDirectionalLight({{.2f, -1.f, .1f}, 1.f, {1.f, 1.f, 1.f, 1.f}, 0.f});
    Atmosphere3D atmosphere{};
    atmosphere.enabled = true;
    atmosphere.heightFogDensity = 0.f;
    atmosphere.aerialPerspective = {true, .01f, 0.f, 0.f, 0.f, 16.f, 20.f,
                                    80.f, .15f, {0.f, 0.f, 0.f, 1.f}, 0.f};
    const Vec3 rayDirection = Normalize(Vec3{.35f, .25f, 1.f});
    const Vec3 worldCenter{0.f, 0.f, .5f};
    auto sample = [&](bool enabled, float cameraDistance,
                      const std::filesystem::path& capture) {
      atmosphere.aerialPerspective.enabled = enabled;
      atmosphere.cameraPosition = worldCenter - rayDirection * cameraDistance;
      draw.SetAtmosphere(atmosphere);
      std::vector<uint64_t> timings;
      for (int frame = 0; frame < 64; ++frame) {
        renderer.Render(draw, {.02f, .03f, .04f, 1.f});
        if (frame >= 24 && renderer.GetStats().lastGpuFrameNanoseconds)
          timings.push_back(renderer.GetStats().lastGpuFrameNanoseconds);
      }
      renderer.CaptureNextFrame(capture);
      renderer.Render(draw, {.02f, .03f, .04f, 1.f});
      renderer.WaitIdle();
      return std::pair{Median(std::move(timings)), renderer.GetStats()};
    };
    const auto root = std::filesystem::temp_directory_path();
    const auto offPath = root / "aevrynd-aerial-perspective-off.png";
    const auto onPath = root / "aevrynd-aerial-perspective-on.png";
    const auto farPath = root / "aevrynd-aerial-perspective-far-cap.png";
    const auto [offGpu, off] = sample(false, 80.f, offPath);
    const auto [onGpu, on] = sample(true, 80.f, onPath);
    const auto [farGpu, farStats] = sample(true, 160.f, farPath);
    const int64_t delta = int64_t(onGpu) - int64_t(offGpu);
    std::cout << "AerialPerspective3DBenchmark gpuOffUs=" << offGpu / 1000
              << " gpuOnUs=" << onGpu / 1000 << " gpuDeltaUs=" << delta / 1000
              << " luminanceOff=" << off.lastCapturedAverageLuminance
              << " luminanceOn=" << on.lastCapturedAverageLuminance
              << " luminanceFar=" << farStats.lastCapturedAverageLuminance
              << " capDelta=" << std::abs(farStats.lastCapturedAverageLuminance -
                                            on.lastCapturedAverageLuminance)
              << " residentDeltaBytes="
              << int64_t(on.hdrSceneColorBytes) - int64_t(off.hdrSceneColorBytes) << '\n';
    // 增量上限採中位數且允許負值；不得新增任何常駐 render target。
    if (!offGpu || !onGpu || delta > 250000 ||
        !farGpu || off.successfulFrameCaptures < 1 || on.successfulFrameCaptures < 2 ||
        farStats.successfulFrameCaptures < 3 ||
        !(on.lastCapturedAverageLuminance < off.lastCapturedAverageLuminance * .9f) ||
        std::abs(farStats.lastCapturedAverageLuminance - on.lastCapturedAverageLuminance) > .00025f ||
        on.hdrSceneColorBytes != off.hdrSceneColorBytes)
      result = 2;
    std::error_code error;
    std::filesystem::remove(offPath, error);
    std::filesystem::remove(onPath, error);
    std::filesystem::remove(farPath, error);
  }
  DestroyWindow(window);
  return result;
}
