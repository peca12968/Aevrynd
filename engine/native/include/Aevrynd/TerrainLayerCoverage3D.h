#pragma once
#include "Math.h"
#include <cmath>

namespace Aevrynd {
// 明確 opt-in 的三層覆蓋契約，與展示用 signed-biome marker 分離。
// Terrain + textures + useUv + 此 marker 才使用 UV；UV=(土壤覆蓋,岩石覆蓋)。
// (0,0) 精確保留程序式材質，剩餘權重沿用原本草／土／岩比例。
inline constexpr float TerrainLayerCoverageNormalScaleMarker3D=-15.46875f;
inline bool TryEncodeTerrainLayerCoverage3D(float soil,float rock,Vec2& output) noexcept {
    if(!std::isfinite(soil)||!std::isfinite(rock)||soil<0.f||rock<0.f||soil>1.f||rock>1.f||soil+rock>1.f)return false;
    output={soil,rock};return true;
}
}
