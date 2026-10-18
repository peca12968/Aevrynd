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
inline float RiverOuterWidth(float x) {
    // 內側保留原岸邊林帶；外溪谷平順擴寬，不以縮減樹根離岸門檻換取植被。
    return 4.f+2.f*RiverOuterBlend(x)+4.f*RiverSmooth((std::abs(x)-20.f)/12.f);
}
inline float RiverDistance(float x,float z) {return std::abs(z-RiverCenterZ(x));}
inline float RiverErosionMask(float x,float z) {
    // 兩個外岸形成局部凹灣，平滑延長乾坡，不挖出需要陡峭補償的上緣。
    const bool north=z>RiverCenterZ(x);
    const float center=north?37.f:-37.f,extent=13.f;
    return RiverSmooth(1.f-std::abs(x-center)/extent);
}
inline float RiverTerrainHeight(float x,float z,float hills) {
    const float distance=RiverDistance(x,z),width=RiverOuterWidth(x);
    if(distance<width)return -8.f*(1.f-RiverSmooth(distance/width));
    // 中央橋樑與任務走廊保持原地形；外側改成較寬且兩岸不同的侵蝕坡。
    const float outer=RiverSmooth((std::abs(x)-12.f)/12.f);
    const float bank=z<RiverCenterZ(x)?13.f+2.f*std::sin(x*.07f):13.f+2.f*std::cos(x*.08f);
    const float transition=4.f+(bank-4.f)*outer;
    const float erosion=RiverErosionMask(x,z);
    return hills*RiverSmooth((distance-width)/(transition*(1.f+.5f*erosion)));
}
}
