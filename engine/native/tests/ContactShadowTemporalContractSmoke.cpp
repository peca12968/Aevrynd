#include <Aevrynd/Renderer.h>

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Aevrynd;

namespace {
bool Near(float value, float expected) {
  return std::isfinite(value) && std::abs(value - expected) < 1e-6f;
}
bool Rejected(const ContactShadowTemporalResult3D &result,
              ContactShadowTemporalReset3D reason, float expected) {
  return !result.historyAccepted && result.rejection == reason &&
         Near(result.visibility, expected);
}
} // namespace

int main() {
  ContactShadowTemporalConstants3D constants{};
  constants.historyValid = 1;
  constants.historyWeight = .75f;
  constants.depthThreshold = .02f;
  constants.motionThresholdPixels = 4.f;
  const ContactShadowTemporalSample3D sample{.2f, .8f, .5f, .505f, 1.f, true};
  const auto stable = EvaluateContactShadowTemporal3D(sample, constants);
  if (!stable.historyAccepted || stable.rejection != ContactShadowTemporalReset3D::None ||
      !Near(stable.visibility, .65f))
    return 1;
  for(const auto depths:std::array{std::array{1.f,1.01f},std::array{10.f,10.01f},std::array{100.f,100.01f}})
    if(!EvaluateContactShadowTemporal3D({.4f,.5f,depths[0],depths[1],0.f,true},constants).historyAccepted)
      return 8;
  if(EvaluateContactShadowTemporal3D({.4f,.5f,10.f,10.2f,0.f,true},constants).historyAccepted)
    return 9;

  auto cut = constants; cut.cameraCut = 1;
  auto resize = constants; resize.resized = 1;
  auto quality = constants; quality.qualityChanged = 1;
  auto invalid = constants; invalid.historyValid = 0;
  struct ResetCase { ContactShadowTemporalConstants3D constants; bool enabled; ContactShadowTemporalReset3D reason; };
  const std::array resetCases{
      ResetCase{constants, false, ContactShadowTemporalReset3D::Disabled},
      ResetCase{cut, true, ContactShadowTemporalReset3D::CameraCut},
      ResetCase{resize, true, ContactShadowTemporalReset3D::Resize},
      ResetCase{quality, true, ContactShadowTemporalReset3D::Quality},
      ResetCase{invalid, true, ContactShadowTemporalReset3D::HistoryInvalid}};
  for (const auto &test : resetCases)
    if (!Rejected(EvaluateContactShadowTemporal3D(sample, test.constants, test.enabled),
                  test.reason, .2f))
      return 2;
  auto allResets = invalid;
  allResets.cameraCut = allResets.resized = allResets.qualityChanged = 1;
  if (EvaluateContactShadowTemporal3D(sample, allResets).rejection !=
      ContactShadowTemporalReset3D::CameraCut)
    return 3;

  const auto reprojection = EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.5f,0.f,false}, constants);
  const auto depthBoundary = EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.52f,0.f,true}, constants);
  const auto depthRejected = EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.521f,0.f,true}, constants);
  const auto motionBoundary = EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.5f,-4.f,true}, constants);
  const auto motionRejected = EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.5f,-4.01f,true}, constants);
  if (!Rejected(reprojection, ContactShadowTemporalReset3D::ReprojectionInvalid, .2f) ||
      !depthBoundary.historyAccepted ||
      !Rejected(depthRejected, ContactShadowTemporalReset3D::DepthRejected, .2f) ||
      !motionBoundary.historyAccepted ||
      !Rejected(motionRejected, ContactShadowTemporalReset3D::MotionRejected, .2f))
    return 4;

  auto clamped = constants; clamped.historyWeight = 2.f;
  const auto highWeight = EvaluateContactShadowTemporal3D({-.5f,2.f,.5f,.5f,0.f,true}, clamped);
  clamped.historyWeight = -1.f;
  const auto lowWeight = EvaluateContactShadowTemporal3D({.25f,.8f,.5f,.5f,0.f,true}, clamped);
  if (!highWeight.historyAccepted || !Near(highWeight.visibility, .98f) ||
      !lowWeight.historyAccepted || !Near(lowWeight.visibility, .25f))
    return 5;

  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  for (const float value : {nan, inf, -inf}) {
    if (!Rejected(EvaluateContactShadowTemporal3D({value,.8f,.5f,.5f,0.f,true}, constants), ContactShadowTemporalReset3D::ReprojectionInvalid, 1.f) ||
        !Rejected(EvaluateContactShadowTemporal3D({.4f,value,.5f,.5f,0.f,true}, constants), ContactShadowTemporalReset3D::ReprojectionInvalid, .4f) ||
        !Rejected(EvaluateContactShadowTemporal3D({.4f,.8f,value,.5f,0.f,true}, constants), ContactShadowTemporalReset3D::DepthRejected, .4f) ||
        !Rejected(EvaluateContactShadowTemporal3D({.4f,.8f,.5f,.5f,value,true}, constants), ContactShadowTemporalReset3D::MotionRejected, .4f))
      return 6;
  }
  auto fallback = constants;
  fallback.historyWeight = fallback.depthThreshold = fallback.motionThresholdPixels = nan;
  const auto fallbackResult = EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.505f,1.f,true}, fallback);
  if (!fallbackResult.historyAccepted || !Near(fallbackResult.visibility, .71f))
    return 7;
  return 0;
}
