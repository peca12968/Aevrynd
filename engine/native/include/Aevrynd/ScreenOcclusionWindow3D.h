#pragma once
#include "Math.h"
#include <array>
#include <cmath>
namespace Aevrynd {
struct ScreenOcclusionWindow3D {
    bool enabled{};
    Vec2 center{},radius{};
    float maximumDepth{},innerFraction{.65f};
};
inline bool ValidScreenOcclusionWindow3D(const ScreenOcclusionWindow3D& s)noexcept{
    return std::isfinite(s.center.x)&&std::isfinite(s.center.y)&&std::isfinite(s.radius.x)&&std::isfinite(s.radius.y)&&
        std::isfinite(s.maximumDepth)&&std::isfinite(s.innerFraction)&&s.radius.x>0&&s.radius.y>0&&s.radius.x<=2&&s.radius.y<=2&&
        s.maximumDepth>=0&&s.maximumDepth<=1&&s.innerFraction>=0&&s.innerFraction<1;
}
inline ScreenOcclusionWindow3D NormalizeScreenOcclusionWindow3D(ScreenOcclusionWindow3D s)noexcept{
    return s.enabled&&ValidScreenOcclusionWindow3D(s)?s:ScreenOcclusionWindow3D{};
}
// 主體包絡的八角點經真正投影；跨過近裁面/非有限資料時拒絕局部化，而非產生無界遮罩。
inline ScreenOcclusionWindow3D ProjectScreenOcclusionWindow3D(const Matrix4& vp,Vec3 center,Vec3 extents,float padding=1.15f)noexcept{
    if(!std::isfinite(padding)||padding<1||padding>2||extents.x<=0||extents.y<=0||extents.z<=0)return {};
    Vec2 low{10000,10000},high{-10000,-10000};float depth{};
    for(unsigned i=0;i<8;++i){
        const Vec3 p=center+Vec3{(i&1)?extents.x:-extents.x,(i&2)?extents.y:-extents.y,(i&4)?extents.z:-extents.z};
        const auto q=vp*Vec4{p.x,p.y,p.z,1};
        if(!std::isfinite(q.x)||!std::isfinite(q.y)||!std::isfinite(q.z)||!std::isfinite(q.w)||q.w<=.001f||q.z<0||q.z>q.w)return {};
        const Vec2 uv{q.x/q.w*.5f+.5f,.5f-q.y/q.w*.5f};
        low.x=(std::min)(low.x,uv.x);low.y=(std::min)(low.y,uv.y);high.x=(std::max)(high.x,uv.x);high.y=(std::max)(high.y,uv.y);
        depth=(std::max)(depth,q.z/q.w);
    }
    return NormalizeScreenOcclusionWindow3D({true,(low+high)*.5f,(high-low)*(.5f*padding),depth,.65f});
}
inline std::array<float,8> PackScreenOcclusionWindow3D(ScreenOcclusionWindow3D s)noexcept{
    s=NormalizeScreenOcclusionWindow3D(s);if(!s.enabled)return {};
    return {s.center.x,s.center.y,s.radius.x,s.radius.y,1,s.maximumDepth,s.innerFraction,0};
}
}
