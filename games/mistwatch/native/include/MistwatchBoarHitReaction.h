#pragma once
#include "MistwatchBoar.h"
#include <algorithm>
#include <cmath>

namespace Mistwatch::BoarHitReaction {
inline constexpr float Cooldown=.65f, Duration=.28f, Attack=.04f, MaximumPitch=.12f;
inline constexpr unsigned JointCount=16, HeadJoint=1;
enum class Status {Inactive,Active,Invalid};
struct Plan {Status status{Status::Inactive};float elapsed{},pitch{};};
inline float Smooth(float t)noexcept{return t*t*(3.f-2.f*t);}
// 真命中第一幀已有短促反應，之後快速達峰、較慢返回；不持有另一條時間線。
inline float Pitch(float elapsed)noexcept{
    const float amount=elapsed<Attack?.65f+.35f*Smooth(elapsed/Attack):
        Smooth((Duration-elapsed)/(Duration-Attack));
    return -MaximumPitch*amount;
}
inline Plan Build(const Encounter& enemy,bool enabled=true)noexcept{
    if(!enemy.Valid())return {Status::Invalid};
    // 成功反擊是唯一把冷卻寫為 .65 的正式分支；末幀命中可立即進入追擊／下一輪蓄力。
    if(!enabled||!enemy.health||enemy.health>=3)return {};
    const float elapsed=Cooldown-enemy.playerCooldown;
    if(elapsed>=Duration)return {};
    if(!std::isfinite(elapsed)||elapsed<0)return {Status::Invalid};
    return {Status::Active,elapsed,Pitch(elapsed)};
}
namespace Detail {
inline bool FinitePose(const Aevrynd::AnimationPose3D& pose)noexcept{
    if(pose.localTransforms.size()!=JointCount||pose.globalMatrices.size()!=JointCount)return false;
    for(const auto& t:pose.localTransforms){
        for(float v:{t.translation.x,t.translation.y,t.translation.z,t.rotation.x,t.rotation.y,t.rotation.z,
            t.scale.x,t.scale.y,t.scale.z,t.quaternion.x,t.quaternion.y,t.quaternion.z,t.quaternion.w})
            if(!std::isfinite(v))return false;
        if(t.scale.x!=1.f||t.scale.y!=1.f||t.scale.z!=1.f)return false;
    }
    for(const auto& m:pose.globalMatrices)for(const auto& row:m.values)for(float v:row)if(!std::isfinite(v))return false;
    return true;
}
inline Aevrynd::SkeletonCompatibilityId3D CanonicalStructure(){
    // 只在首次初始化建立一次原作骨架；不能把 AESK1 的 LegacySynthetic 名稱 ID 當結構 ID。
    static const auto identity=[]{const Boar canonical;return Aevrynd::ComputeStructuralSkeletonCompatibilityId3D(
        canonical.model.skeleton,canonical.model.inverseBindMatrices);}();
    return identity;
}
}
struct Scratch {
    Aevrynd::AnimationPose3D pose;
    Aevrynd::SkeletonCompatibilityId3D structure{};
    bool Initialize(const Aevrynd::SkinnedModel3D& model){
        const auto actual=Aevrynd::ComputeStructuralSkeletonCompatibilityId3D(model.skeleton,model.inverseBindMatrices);
        if(!model.Valid()||model.skeleton.joints.size()!=JointCount||!actual||actual!=Detail::CanonicalStructure())return false;
        pose.localTransforms.resize(JointCount);pose.globalMatrices.resize(JointCount);structure=actual;return true;
    }
};
// 原 pose、模型與接地 owner 全唯讀；只有 active 分支寫可重用的渲染 scratch。
inline bool Apply(const Aevrynd::SkinnedModel3D& model,const Aevrynd::AnimationPose3D& original,
    const Plan& plan,Scratch& scratch){
    if(plan.status!=Status::Active||!std::isfinite(plan.elapsed)||plan.elapsed<0||plan.elapsed>=Duration||
       !std::isfinite(plan.pitch)||plan.pitch!=Pitch(plan.elapsed)||plan.pitch>=0||plan.pitch< -MaximumPitch||
       &original==&scratch.pose||!scratch.structure||scratch.structure!=Detail::CanonicalStructure()||
       Aevrynd::ComputeStructuralSkeletonCompatibilityId3D(model.skeleton,model.inverseBindMatrices)!=scratch.structure||
       scratch.pose.localTransforms.size()!=JointCount||scratch.pose.globalMatrices.size()!=JointCount||
       !Detail::FinitePose(original)||original.localTransforms[HeadJoint].usesQuaternion)return false;
    std::copy(original.localTransforms.begin(),original.localTransforms.end(),scratch.pose.localTransforms.begin());
    scratch.pose.localTransforms[HeadJoint].rotation.x+=plan.pitch;
    Aevrynd::BuildAnimationPoseGlobalMatrices3D(model.skeleton,scratch.pose);
    return Detail::FinitePose(scratch.pose);
}
}
