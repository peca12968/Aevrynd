#pragma once
#include <Aevrynd/Camera.h>
#include <Aevrynd/Geometry3D.h>
#include <Aevrynd/Mesh3D.h>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace Mistwatch::FoliageView {
inline constexpr std::uint64_t Namespace=0x4d57464952464144ull;
inline constexpr std::wstring_view Profile=L"foreground-needles-color-v1";
inline constexpr std::wstring_view LocalProfile=L"subject-window-needles-v2";
inline Aevrynd::RenderObjectIdentity3D LeafIdentity(bool forestWorld,std::uint64_t ordinal){
    if(!forestWorld)return {};
    if(!ordinal)throw std::invalid_argument("FOLIAGE_VIEW_ZERO_IDENTITY");
    return {{Namespace,ordinal},1};
}
inline bool Resolve(bool diagnostic,bool forestWorld,std::wstring_view raw){
    if(raw.empty())return forestWorld;
    if(!diagnostic)throw std::invalid_argument("FOLIAGE_VIEW_DIAGNOSTIC_OVERRIDE_ONLY");
    if(raw==L"original")return false;
    if((raw!=Profile&&raw!=LocalProfile)||!forestWorld)throw std::invalid_argument("FOLIAGE_VIEW_PROFILE_CONTEXT");
    return true;
}
inline bool ResolveWindow(bool diagnostic,bool forestWorld,std::wstring_view raw){
    const bool enabled=Resolve(diagnostic,forestWorld,raw);
    return enabled&&(raw.empty()||raw==LocalProfile);
}
// 用鏡頭到身體三個部位的有界視線管線；這是保守包絡淡出，不冒稱alpha貼圖可見性查詢。
// 空間過渡取代幀數計時，鏡頭離開即恢復；不改角色、相機、木體或葉片的幾何。
inline float Opacity(const Aevrynd::Camera3D& camera,Aevrynd::Vec3 feet,float height,
    const Aevrynd::MeshBounds3D& world){
    using namespace Aevrynd;
    const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    if(!finite(camera.position)||!finite(feet)||!finite(world.center)||!finite(world.extents)||
        !std::isfinite(height)||height<=0||world.extents.x<0||world.extents.y<0||world.extents.z<0)
        throw std::invalid_argument("FOLIAGE_VIEW_NONFINITE_OR_INVALID");
    constexpr float margin=.55f;float clearance=margin;
    for(float fraction:{.30f,.60f,.92f}){
        const auto delta=feet+Vec3{0,height*fraction,0}-camera.position;const float length=Length(delta);
        if(!std::isfinite(length)||length<=.25f)continue;
        const Ray3D ray{camera.position,delta*(1.f/length)};
        const float limit=length-.25f;
        const auto hit=[&](float radius){return RayIntersectsAabb(ray,world.center,world.extents+Vec3{radius,radius,radius},limit);};
        if(!hit(margin))continue;
        float low{},high=margin;
        if(hit(0))high=0;
        else for(unsigned step=0;step<9;++step){const float mid=(low+high)*.5f;if(hit(mid))high=mid;else low=mid;}
        clearance=(std::min)(clearance,high);
    }
    const float t=std::clamp(clearance/margin,0.f,1.f);return .12f+.88f*t*t*(3.f-2.f*t);
}
}
