#pragma once
#include <Aevrynd/Renderer.h>

namespace Aevrynd {
// Phase base sky 的全球平均 radiance（SH0／各向同性閉包）。
// 不是表面 irradiance/pi；不含太陽圓盤、高頻散射或多重散射補償。
[[nodiscard]] inline Vec3 EvaluatePhaseSkyAmbientRadiance3D(const PhaseRadianceProfile3D& source) noexcept {
    PhaseRadianceProfile3D phase{};
    if(!NormalizePhaseRadianceProfile3D(source,phase))return {};
    const float weight=.5f/(phase.horizonExponent+1.f);
    const float horizonWeight=1.f-2.f*weight;
    return (phase.skyZenith*weight+phase.skyHorizon*horizonWeight+phase.groundBounce*weight)*phase.skyIntensity;
}
}
