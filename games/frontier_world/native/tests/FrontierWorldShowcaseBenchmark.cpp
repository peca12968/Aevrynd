#include <Aevrynd/Renderer.h>
#include <chrono>
#include <iostream>

using namespace Aevrynd;

int main() {
  DrawList draw;
  const auto begin = std::chrono::steady_clock::now();
  for (unsigned iteration = 0; iteration < 100000; ++iteration) {
    draw.SetDirectionalShadowCascades({4, 2048, .1f, 220.f, .7f, .1f});
    draw.SetSoftDirectionalShadows({true, 24.f, 128.f, 36.f});
    draw.SetContactShadows({true, 16, 1.8f, .045f, .85f});
    draw.SetAmbientOcclusion({true, 1.5f, .03f, 1.15f});
    draw.SetScreenSpaceReflections({true, .82f, 42.f, .7f, .32f, 16});
    draw.SetBloom({true, 1.05f, .55f, .13f, 3.f});
    draw.SetVolumetricClouds(
        {true, 1100.f, 1700.f, .48f, 1.08f, .28f, 14.f, 16, true, .7f});
    draw.SetDirectionalLightShafts({true, .68f, .72f, 360.f, 6});
    draw.SetTerrainDistanceMaterial({true, 44.f, 196.f, .0065f, .16f, 1.f,
                                     .14f});
    draw.SetWaterOptics({true, .022f, .42f, .04f});
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now() - begin)
                           .count();
  const bool valid =
      draw.DirectionalShadowCascades().cascadeCount == 4 &&
      draw.DirectionalShadowCascades().resolution == 2048 &&
      draw.SoftDirectionalShadows().enabled && draw.ContactShadows().enabled &&
      draw.AmbientOcclusion().enabled &&
      draw.ScreenSpaceReflections().sampleCount == 16 && draw.Bloom().enabled &&
      draw.VolumetricClouds().sampleCount == 16 &&
      draw.VolumetricClouds().shadowsEnabled &&
      draw.DirectionalLightShafts().sampleCount == 6 &&
      draw.TerrainDistanceMaterial().enabled &&
      draw.WaterOptics().screenSpaceRefraction;
  std::cout << "profile_iterations=100000 cpu_ns=" << elapsed
            << " cascades=" << draw.DirectionalShadowCascades().cascadeCount
            << " cloud_samples=" << draw.VolumetricClouds().sampleCount
            << " ssr_samples=" << draw.ScreenSpaceReflections().sampleCount
            << '\n';
  return valid ? 0 : 1;
}
