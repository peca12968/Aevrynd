#pragma once
#include <algorithm>
#include <cmath>
namespace Mistwatch {
inline constexpr float RiverHeight=-4.f;
inline float RiverSmooth(float value) {
    const float t=std::clamp(value,0.f,1.f);
    return t*t*(3.f-2.f*t);
}
inline float RiverOuterBlend(float x) {return RiverSmooth((std::abs(x)-4.f)/12.f);}
inline float RiverCenterZ(float x) {
    // 橋樑四公尺走廊保留直線；兩側逐漸過渡至彎曲溪谷。
    return 36.f+3.5f*std::sin(x*.095f)*RiverOuterBlend(x);
}
inline float RiverOuterWidth(float x) {return 4.f+2.f*RiverOuterBlend(x);}
inline float RiverDistance(float x,float z) {return std::abs(z-RiverCenterZ(x));}
inline float RiverTerrainHeight(float x,float z,float hills) {
    const float distance=RiverDistance(x,z),width=RiverOuterWidth(x);
    if(distance<width)return -8.f*(1.f-RiverSmooth(distance/width));
    // 溪谷邊緣不直接跳回丘陵高度，留下四公尺的平滑侵蝕過渡。
    return hills*RiverSmooth((distance-width)/4.f);
}
}
