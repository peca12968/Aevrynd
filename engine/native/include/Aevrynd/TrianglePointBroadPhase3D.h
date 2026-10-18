#pragma once
#include "Math.h"
#include <algorithm>
#include <cmath>

namespace Aevrynd {
// 只供float來源頂點與float查詢點；嚴格分離才排除，邊／角接觸保留。
// 這不是點在三角內的證明；無效輸入保留給呼叫者的精確驗證拒絕。
[[nodiscard]] inline bool TrianglePointMayOverlapXZ3D(Vec3 point,Vec3 a,Vec3 b,Vec3 c)noexcept{
    for(const auto v:{point,a,b,c})if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))return true;
    return point.x>=(std::min)({a.x,b.x,c.x})&&point.x<=(std::max)({a.x,b.x,c.x})&&
        point.z>=(std::min)({a.z,b.z,c.z})&&point.z<=(std::max)({a.z,b.z,c.z});
}
}
