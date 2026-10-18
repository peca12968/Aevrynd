#pragma once

#include <Aevrynd/Math.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace Aevrynd {
namespace SurfaceNormalTransportDetail3D {
struct Vector {
    double x{}, y{}, z{};
    Vector operator+(Vector b) const noexcept { return {x+b.x,y+b.y,z+b.z}; }
    Vector operator-(Vector b) const noexcept { return {x-b.x,y-b.y,z-b.z}; }
    Vector operator*(double s) const noexcept { return {x*s,y*s,z*s}; }
};
inline Vector ToDouble(Vec3 v) noexcept { return {v.x,v.y,v.z}; }
inline bool Finite(Vector v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
inline double Dot(Vector a, Vector b) noexcept { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vector Cross(Vector a, Vector b) noexcept {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
inline bool Unit(Vector input, Vector& output) noexcept {
    if (!Finite(input)) return false;
    const double maximum=(std::max)({std::abs(input.x),std::abs(input.y),std::abs(input.z)});
    if (maximum<=0.0) return false;
    const Vector scaled{input.x/maximum,input.y/maximum,input.z/maximum};
    const double length=std::sqrt(Dot(scaled,scaled));
    if (!std::isfinite(length) || length<=0.0) return false;
    output=scaled*(1.0/length);
    return Finite(output);
}
}

// 以三角面切向變形傳遞作者法線，成功結果為單位向量。
// 三角面無法決定垂直於表面的體積伸縮；此API明定「單位面法線延伸」：
// S=[原edge1,原edge2,原單位面法線]，T為變形後同基底，回傳normalize((T*S^-1)^-T*n)。
// 因此平面XY均勻放大s時，傾斜法線按(nx/s,ny/s,nz)轉換；不冒稱一般3D仿射逆轉置。
// 兩個三角形同時反向繞向不影響結果；UV、平滑分域與packed identity保真由呼叫端管理。
// 零法線、非有限或退化三角形失敗時不改output。沒有固定世界面積門檻。
[[nodiscard]] inline bool TryTransportSurfaceNormal3D(
    const std::array<Vec3,3>& source,
    const std::array<Vec3,3>& deformed,
    Vec3 authoredNormal,
    Vec3& output) noexcept {
    namespace D=SurfaceNormalTransportDetail3D;
    std::array<D::Vector,3> before,after;
    for (unsigned i=0;i<3;++i) {
        before[i]=D::ToDouble(source[i]);
        after[i]=D::ToDouble(deformed[i]);
        if (!D::Finite(before[i]) || !D::Finite(after[i])) return false;
    }
    D::Vector n;
    if (!D::Unit(D::ToDouble(authoredNormal),n)) return false;

    // 在double內相減／叉積，避免float有限座標差、微小面積或平方長度溢位／下溢。
    const auto a=before[1]-before[0], b=before[2]-before[0];
    const auto x=after[1]-after[0], y=after[2]-after[0];
    D::Vector c,z;
    if (!D::Unit(D::Cross(a,b),c) || !D::Unit(D::Cross(x,y),z)) return false;

    // T^-T*S^T*n的餘因子式。det(T)=|x cross y|嚴格為正；
    // 最後會正規化，省略同一正比例除數可避免不必要的極端縮放。
    const auto transformed=D::Cross(y,z)*D::Dot(a,n)
        +D::Cross(z,x)*D::Dot(b,n)+D::Cross(x,y)*D::Dot(c,n);
    D::Vector unit;
    if (!D::Unit(transformed,unit)) return false;
    const Vec3 candidate{float(unit.x),float(unit.y),float(unit.z)};
    if (!D::Finite(D::ToDouble(candidate))) return false;
    output=candidate;
    return true;
}
}
