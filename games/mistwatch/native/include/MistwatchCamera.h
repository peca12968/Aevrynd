#pragma once
#include <Aevrynd/Math.h>
#include <Aevrynd/CharacterController3D.h>
#include <algorithm>
#include <cmath>
namespace Mistwatch {
struct OrbitCamera {
    float yaw{}, pitch{.3f}, distance{6.f};
    void Update(Aevrynd::Vec2 drag,float wheel,float keyboard,float dt) {
        if(!std::isfinite(drag.x)||!std::isfinite(drag.y)||!std::isfinite(wheel)||
           !std::isfinite(keyboard)||!std::isfinite(dt)||dt<0) return;
        yaw=std::remainder(yaw-drag.x*.005f+keyboard*(std::min)(dt,.1f)*1.5f,6.2831853f);
        pitch=std::clamp(pitch+drag.y*.004f,-.15f,1.1f);
        distance=std::clamp(distance-wheel*.7f,2.5f,10.f);
    }
};
inline Aevrynd::Camera3D BuildPlayerCamera(Aevrynd::Vec3 feet,float height,const OrbitCamera& orbit,
    std::span<const Aevrynd::BoxCollider3D> solids,const Aevrynd::HeightField3D& terrain) {
    const auto target=feet+Aevrynd::Vec3{0,height<1.5f?.85f:1.2f,0};
    auto camera=Aevrynd::MakeThirdPersonCamera(target,orbit.yaw,orbit.pitch,orbit.distance);
    camera=Aevrynd::ConstrainThirdPersonCameraCeiling(camera,solids);
    camera=Aevrynd::ConstrainThirdPersonCamera(camera,target,solids);
    return Aevrynd::ConstrainThirdPersonCamera(camera,target,terrain);
}
}
