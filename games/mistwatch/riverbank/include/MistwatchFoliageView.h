#pragma once
#include <Aevrynd/Camera.h>
#include <Aevrynd/Geometry3D.h>
#include <Aevrynd/Mesh3D.h>
#include <array>
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
class OpacityQuery {
public:
    OpacityQuery(const Aevrynd::Camera3D& camera,Aevrynd::Vec3 feet,float height){
        using namespace Aevrynd;
        if(!Finite(camera.position)||!Finite(feet)||!std::isfinite(height)||height<=0)
            throw std::invalid_argument("FOLIAGE_VIEW_NONFINITE_OR_INVALID");
        for(float fraction:{.30f,.60f,.92f}){
            const auto subject=feet+Vec3{0,height*fraction,0};
            const auto delta=subject-camera.position;
            const float length=Length(delta);
            if(!std::isfinite(length)||length<=SubjectClearance)continue;
            const auto direction=delta*(1.f/length);
            samples_[count_++]={{camera.position,direction},length-SubjectClearance};
            if(fraction==.60f){
                subjectCenter_=subject;
                subjectForward_=direction;
                extentWeights_={std::abs(direction.x),std::abs(direction.y),std::abs(direction.z)};
                hasSubjectDepth_=true;
            }
        }
    }
    float Opacity(const Aevrynd::MeshBounds3D& world)const{
        using namespace Aevrynd;
        if(!Finite(world.center)||!Finite(world.extents)||world.extents.x<0||world.extents.y<0||world.extents.z<0)
            throw std::invalid_argument("FOLIAGE_VIEW_NONFINITE_OR_INVALID");
        // 三條視線共用角色中央的深度，斜向採樣也不會淡出角色後方的葉片。
        float depthBlend=1.f;
        if(hasSubjectDepth_){
            const float frontDepth=Dot(subjectCenter_-world.center,subjectForward_)+Dot(world.extents,extentWeights_);
            if(frontDepth<=0)return 1.f;
            const float t=std::clamp(frontDepth/SubjectClearance,0.f,1.f);
            depthBlend=t*t*(3.f-2.f*t);
        }
        constexpr float margin=.55f;
        float clearance=margin;
        for(unsigned index=0;index<count_;++index){
            const auto& sample=samples_[index];
            const auto hit=[&](float radius){
                return RayIntersectsAabb(sample.ray,world.center,world.extents+Vec3{radius,radius,radius},sample.limit);
            };
            if(!hit(margin))continue;
            float low{},high=margin;
            if(hit(0))high=0;
            else for(unsigned step=0;step<9;++step){
                const float mid=(low+high)*.5f;
                if(hit(mid))high=mid;else low=mid;
            }
            clearance=(std::min)(clearance,high);
        }
        const float t=std::clamp(clearance/margin,0.f,1.f);
        const float foregroundOpacity=.12f+.88f*t*t*(3.f-2.f*t);
        return std::lerp(1.f,foregroundOpacity,depthBlend);
    }
private:
    static constexpr float SubjectClearance=.25f;
    struct Sample {Aevrynd::Ray3D ray{};float limit{};};
    static bool Finite(Aevrynd::Vec3 value){
        return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);
    }
    std::array<Sample,3> samples_{};
    unsigned count_{};
    Aevrynd::Vec3 subjectCenter_{},subjectForward_{},extentWeights_{};
    bool hasSubjectDepth_{};
};
inline float Opacity(const Aevrynd::Camera3D& camera,Aevrynd::Vec3 feet,float height,
    const Aevrynd::MeshBounds3D& world){
    return OpacityQuery(camera,feet,height).Opacity(world);
}
}
