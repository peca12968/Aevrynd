#include <Aevrynd/Engine.h>

#include <cstdint>
#include <iostream>

using namespace Aevrynd;

int main() {
  EngineConfig stagedConfig{};
  stagedConfig.renderQuality = RenderQuality3D::Ultra;
  Engine stagedEngine(stagedConfig);
  const auto stagedReceipt = stagedEngine.DevelopmentUltraReceipt();
  const auto stagedStats = stagedEngine.GetRendererStats();
  if (stagedEngine.RequestedRenderQuality() != RenderQuality3D::Ultra ||
      stagedEngine.RenderQuality() != RenderQuality3D::High ||
      !stagedReceipt.requestActive || stagedReceipt.rendererApplied ||
      stagedReceipt.transitionPolicyApplied ||
      stagedReceipt.reason != DevelopmentUltraAdmissionReason3D::RuntimeApplicationNotIntegrated ||
      stagedStats.submittedFrames || stagedStats.directionalShadowMapBytes ||
      stagedStats.volumetricAtmosphereResidentBytes) {
    return 5;
  }
  EngineConfig config{};
  config.renderQuality = RenderQuality3D::High;
  Engine engine(config);
  if (engine.RenderQuality() != RenderQuality3D::High ||
      engine.RequestedRenderQuality() != RenderQuality3D::High ||
      engine.DevelopmentUltraReceipt().requestActive) {
    return 1;
  }
  engine.SetRenderQuality(RenderQuality3D::Ultra);
  const auto receipt = engine.DevelopmentUltraReceipt();
  if (engine.RequestedRenderQuality() != RenderQuality3D::Ultra ||
      engine.RenderQuality() != RenderQuality3D::High ||
      receipt.requested != DevelopmentUltraQualityMode3D::DevelopmentUltra ||
      receipt.actual != DevelopmentUltraQualityMode3D::High ||
      receipt.reason != DevelopmentUltraAdmissionReason3D::
                            RuntimeApplicationNotIntegrated ||
      !receipt.requestActive || receipt.rendererApplied || receipt.transitionPolicyApplied ||
      !receipt.noOp || receipt.epoch == 0) {
    return 2;
  }
  const auto renderer = engine.GetRendererStats();
  if (!renderer.requestedQuality.directionalShadows ||
      !renderer.requestedQuality.ambientOcclusion ||
      !renderer.requestedQuality.multisampling ||
      !renderer.requestedQuality.postProcessAntiAliasing) {
    return 3;
  }
  const auto unknown = static_cast<RenderQuality3D>(255);
  engine.SetRenderQuality(unknown);
  if (engine.RenderQuality() != RenderQuality3D::High ||
      engine.RequestedRenderQuality() != unknown ||
      RenderQualityName3D(unknown) != "UNKNOWN" ||
      engine.DevelopmentUltraReceipt().reason !=
          DevelopmentUltraAdmissionReason3D::InvalidProfile ||
      engine.DevelopmentUltraReceipt().requestActive ||
      engine.DevelopmentUltraReceipt().rendererApplied) {
    return 4;
  }
  std::cout << "requested=ULTRA actual=HIGH rendererApplied=0 reason="
            << static_cast<unsigned>(receipt.reason) << '\n';
  return 0;
}
