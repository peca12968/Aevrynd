#pragma once
#include "MistwatchRiverProfile.h"
#include <algorithm>
#include <cmath>
namespace Mistwatch {
// 站點與地形共用高度來源；站點位於半公尺網格的精確採樣座標。
inline float HillElevation(float x,float z) {
    const float side=(std::max)(0.f,std::abs(x)-10.f);
    return side*.3f*(1.f+.3f*std::sin(z*.13f)+.2f*std::cos(x*.19f));
}
inline float GroundElevation(float x,float z) {
    return RiverTerrainHeight(x,z,HillElevation(x,z));
}
}
