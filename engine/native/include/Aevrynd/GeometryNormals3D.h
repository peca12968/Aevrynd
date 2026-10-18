#pragma once

#include <Aevrynd/Math.h>
#include <algorithm>
#include <cmath>

namespace Aevrynd {
// 求三角形方向，不把面積誤當向量長度門檻；面積與品質驗收仍由呼叫端處理。
// 無效或退化輸入不產生替代法線，亦不變更呼叫端原有結果。
[[nodiscard]] inline bool TryComputeTriangleNormal(Vec3 a, Vec3 b, Vec3 c, Vec3& output) noexcept {
    const auto finite = [](Vec3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    if (!finite(a) || !finite(b) || !finite(c)) return false;

    // 先轉成雙精度再相減，避免有限單精度頂點的差值與叉積溢位或下溢。
    const double ux = double(b.x) - double(a.x);
    const double uy = double(b.y) - double(a.y);
    const double uz = double(b.z) - double(a.z);
    const double vx = double(c.x) - double(a.x);
    const double vy = double(c.y) - double(a.y);
    const double vz = double(c.z) - double(a.z);
    const double nx = uy * vz - uz * vy;
    const double ny = uz * vx - ux * vz;
    const double nz = ux * vy - uy * vx;
    const double maximum = (std::max)({std::abs(nx), std::abs(ny), std::abs(nz)});
    if (!std::isfinite(maximum) || maximum <= 0.0) return false;

    const double sx = nx / maximum, sy = ny / maximum, sz = nz / maximum;
    const double length = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (!std::isfinite(length) || length <= 0.0) return false;
    const Vec3 normal{float(sx / length), float(sy / length), float(sz / length)};
    if (!finite(normal)) return false;
    output = normal;
    return true;
}
}
