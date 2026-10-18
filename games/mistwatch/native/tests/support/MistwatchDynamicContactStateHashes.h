#pragma once
#include <MistwatchPlayerLocomotion.h>
#include <bit>
using namespace Aevrynd;
using namespace Mistwatch;
std::uint64_t PoseHash(const Explorer& e){
    std::uint64_t h=1469598103934665603ull;
    const auto add=[&](std::uint64_t n){h^=n;h*=1099511628211ull;};
    const auto scalar=[&](float f){add(std::bit_cast<std::uint32_t>(f));};
    const auto vec=[&](Vec3 v){scalar(v.x);scalar(v.y);scalar(v.z);};
    scalar(e.phase);scalar(e.yaw);scalar(e.requestedYaw);vec(e.lastFeet);add(e.feetInitialized);scalar(e.locomotionDistance);
    scalar(e.locomotionSeconds);vec(e.locomotionVelocity);add(e.gaitGrounded);add(e.nextFoot);
    for(const auto& t:e.pose.localTransforms){vec(t.translation);vec(t.rotation);vec(t.scale);
        scalar(t.quaternion.x);scalar(t.quaternion.y);scalar(t.quaternion.z);scalar(t.quaternion.w);add(t.usesQuaternion);}
    for(const auto& m:e.pose.globalMatrices)for(const auto& row:m.values)for(float f:row)scalar(f);
    for(unsigned i=0;i<2;++i){vec(e.footTargets[i]);add(e.planted[i]);scalar(e.footFacing[i]);scalar(e.swingFacingStart[i]);scalar(e.swingFacingEnd[i]);
        const auto& s=e.footSwings[i];add(s.Ready());add(s.Active());add(s.Replanned());
        for(double d:{s.Elapsed(),s.Duration(),s.LiftHeight(),s.SpeedBound()})add(std::bit_cast<std::uint64_t>(d));
        // 目前及未來採樣一起核對，避免只比較腳點而漏掉剩餘曲線的改變。
        for(unsigned j=0;j<5;++j){auto copy=s;FootSwingSample3D sample;
            const bool ok=copy.Advance((std::max)(0.,s.Duration()-s.Elapsed())*j/4.,sample);add(ok);
            if(ok){vec(sample.positionWorld);vec(sample.velocityWorld);add(sample.planted);}}
    }
    return h;
}
std::uint64_t EvasionHash(const Evasion& e){
    std::uint64_t h=0;for(float f:{e.direction.x,e.direction.y,e.direction.z,e.remaining,e.cooldown})h=(h^std::bit_cast<std::uint32_t>(f))*1099511628211ull;return h;
}
