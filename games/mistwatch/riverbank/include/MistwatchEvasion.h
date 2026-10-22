#pragma once
#include "MistwatchSession.h"
#include <algorithm>
namespace Mistwatch {
struct EvasionMotion {Aevrynd::Vec3 movement{};bool dashing{};};
struct Evasion {
    static constexpr float Duration=.28f, Cooldown=1.2f, SpeedMultiplier=2.f;
    Aevrynd::Vec3 direction{};
    float remaining{},cooldown{};
    EvasionMotion Tick(float dt,bool requested,bool grounded,bool crouched,Aevrynd::Vec3 movement) {
        if(!std::isfinite(dt) || dt<=0 || dt>.1f || !Session::Finite(movement))return {};
        cooldown=(std::max)(0.f,cooldown-dt);
        const float length=std::hypot(movement.x,movement.z);
        if(requested && grounded && !crouched && cooldown<=0 && remaining<=0 && length>.001f) {
            direction={movement.x/length,0,movement.z/length};
            remaining=Duration;cooldown=Cooldown;
        }
        if(remaining<=0)return {movement,false};
        // 最後一幀按剩餘時間縮短方向向量；仍由角色控制器處理碰撞，不傳送或免傷。
        const float fraction=(std::min)(remaining,dt)/dt;
        remaining=(std::max)(0.f,remaining-dt);
        return {direction*fraction,true};
    }
};
}
