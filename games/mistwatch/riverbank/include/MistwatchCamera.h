#pragma once
#include <Aevrynd/Math.h>
#include <Aevrynd/CharacterController3D.h>
#include "MistwatchWorld.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace Mistwatch {
struct OrbitCamera {
    float yaw{}, pitch{.3f}, distance{6.f};
    void Update(Aevrynd::Vec2 drag,float wheel,float keyboard,float dt,bool recenter=false,float heading=0,
        float sensitivity=1.f,bool invertVertical=false) {
        if(!std::isfinite(drag.x)||!std::isfinite(drag.y)||!std::isfinite(wheel)||
           !std::isfinite(keyboard)||!std::isfinite(dt)||dt<0||(recenter&&!std::isfinite(heading))||
           !std::isfinite(sensitivity)||sensitivity<.25f||sensitivity>2.f) return;
        // 只調整滑鼠位移；鍵盤、縮放、回正與碰撞解算仍使用原路徑。
        const double x=double(drag.x)*sensitivity,y=double(drag.y)*sensitivity*(invertVertical?-1.:1.);
        if(std::abs(x)>(std::numeric_limits<float>::max)()||std::abs(y)>(std::numeric_limits<float>::max)())return;
        drag={float(x),float(y)};
        if(recenter&&drag.x==0&&drag.y==0&&keyboard==0)yaw=std::remainder(heading,6.2831853f);
        else yaw=std::remainder(yaw-drag.x*.005f+keyboard*(std::min)(dt,.1f)*1.5f,6.2831853f);
        pitch=std::clamp(pitch+drag.y*.004f,-.15f,1.1f);
        distance=std::clamp(distance-wheel*.7f,2.5f,10.f);
    }
};
inline auto PlayerCameraSamples(Aevrynd::Vec3 feet,float height) {
    return std::array{feet+Aevrynd::Vec3{0,.2f,0},feet+Aevrynd::Vec3{0,height,0},
        feet+Aevrynd::Vec3{0,height*.5f,0},feet+Aevrynd::Vec3{.22f,height*.75f,0},
        feet+Aevrynd::Vec3{-.22f,height*.75f,0},feet+Aevrynd::Vec3{0,height*.75f,.22f},
        feet+Aevrynd::Vec3{0,height*.75f,-.22f}};
}
inline Aevrynd::ThirdPersonFramingResult3D BuildPlayerCameraFraming(Aevrynd::Vec3 feet,float height,const OrbitCamera& orbit,
    std::span<const Aevrynd::BoxCollider3D> solids,const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::BoxCollider3D> cameraOnly={}) {
    const auto target=feet+Aevrynd::Vec3{0,height<1.5f?.85f:1.2f,0};
    const auto samples=PlayerCameraSamples(feet,height);
    return Aevrynd::ResolveThirdPersonFraming3D(target,orbit.yaw,orbit.pitch,orbit.distance,
        samples,solids,CameraCeilingSolids(),terrain,cameraOnly);
}
inline Aevrynd::Camera3D BuildPlayerCamera(Aevrynd::Vec3 feet,float height,const OrbitCamera& orbit,
    std::span<const Aevrynd::BoxCollider3D> solids,const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::BoxCollider3D> cameraOnly={}) {
    return BuildPlayerCameraFraming(feet,height,orbit,solids,terrain,cameraOnly).camera;
}
// 切鏡起點先選安全構圖；後續追蹤必須收到玩家原始方向，不能再次套用左右候選排序。
inline Aevrynd::Camera3D BuildPlayerCameraMotionRequest(Aevrynd::Vec3 feet,float height,const OrbitCamera& orbit,
    bool initialized,std::span<const Aevrynd::BoxCollider3D> solids,const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::BoxCollider3D> cameraOnly={}) {
    if(!initialized)return BuildPlayerCamera(feet,height,orbit,solids,terrain,cameraOnly);
    return Aevrynd::MakeThirdPersonCamera(feet+Aevrynd::Vec3{0,height<1.5f?.85f:1.2f,0},orbit.yaw,orbit.pitch,orbit.distance);
}
inline float PlayerCameraMovementYaw(const Aevrynd::Camera3D& camera,float fallback) {
    const auto offset=camera.position-camera.target;
    return std::hypot(offset.x,offset.z)>.001f?std::atan2(offset.x,-offset.z):fallback;
}
}
