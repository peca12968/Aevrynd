#include "GameplayShowcaseEnvironment3D.h"

#include <iostream>

using namespace Aevrynd;
using namespace GameplayLab;

int main() {
  GameplayShowcaseEnvironment3D first, second;
  if (first.Initialize(0) || !first.Initialize(0x4a17u) ||
      !second.Initialize(0x4a17u))
    return 1;

  const auto receipt = first.Receipt();
  const auto repeated = second.Receipt();
  if (receipt.worldHash == 0 || receipt.worldHash != repeated.worldHash ||
      receipt.terrain != 1 || receipt.water != 17 ||
      receipt.vegetation != 120 || receipt.rocks != 12 ||
      receipt.buildings != 6 || receipt.retainedDescriptorBytes == 0)
    return 2;

  DrawList draw{DrawListCapacityProfile::Minimal2D};
  first.ApplyUltraProfile(draw);
  first.ApplyLighting(draw, ShowcaseLightingProfile3D::Day, {1, 2, 3});
  if (!draw.DirectionalShadowMapEnabled() ||
      draw.DirectionalShadowCascades().cascadeCount != 4 ||
      !draw.SoftDirectionalShadows().enabled ||
      !draw.ContactShadows().enabled || !draw.AmbientOcclusion().enabled ||
      !draw.ScreenSpaceReflections().enabled || !draw.Bloom().enabled ||
      !draw.VolumetricClouds().enabled ||
      !draw.VolumetricClouds().shadowsEnabled ||
      !draw.DirectionalLightShafts().enabled ||
      !draw.TerrainDistanceMaterial().enabled ||
      !draw.WaterOptics().screenSpaceRefraction)
    return 3;

  const auto day = first.EvaluateLighting(ShowcaseLightingProfile3D::Day);
  const auto dusk = first.EvaluateLighting(ShowcaseLightingProfile3D::Dusk);
  const auto night = first.EvaluateLighting(ShowcaseLightingProfile3D::Night);
  if (!(day.sun.intensity > dusk.sun.intensity &&
        dusk.sun.intensity > night.sun.intensity &&
        day.environmentTime < dusk.environmentTime &&
        dusk.environmentTime < night.environmentTime))
    return 4;

  RendererStats before{}, after{};
  for (std::size_t index = 0; index < 4; ++index)
    after.lastDirectionalShadowCascadeRendered[index] = 1;
  after.contactShadowEvaluationPasses = 1;
  after.ambientOcclusionEvaluationPasses = 1;
  after.screenSpaceReflectionPasses = 1;
  after.bloomPasses = 1;
  after.volumetricCloudConfiguredFrames = 1;
  after.volumetricCloudShadowConfiguredFrames = 1;
  after.directionalLightShaftConfiguredFrames = 1;
  after.screenSpaceWaterRefractionPasses = 1;
  after.depthAwareWaterPasses = 1;
  const auto execution = first.ExecutionReceipt(before, after);
  if (!execution.executed || execution.deltas[0] != 4)
    return 5;
  if (first.ExecutionReceipt(after, before).executed ||
      first.ExecutionReceipt(after, after).executed)
    return 6;

  const auto released = first.Release();
  if (released.worldHashBefore != receipt.worldHash ||
      released.descriptorsReleased != 156 ||
      released.retainedBytesBefore == 0 || released.retainedBytesAfter != 0 ||
      first.Initialized() || !first.Descriptors().empty())
    return 7;

  std::cout << "worldHash=" << receipt.worldHash
            << " descriptors=" << released.descriptorsReleased
            << " releasedBytes=" << released.retainedBytesBefore << '\n';
}
