#pragma once

#include <Aevrynd/Animation3D.h>
#include <algorithm>
#include <cmath>

namespace Mistwatch::ExplorerCounterMotion {
inline constexpr float Duration=.32f;

class Clock {
public:
    void Clear()noexcept{elapsed_=Duration;}
    void Commit(float dt,unsigned boarHealthBefore,unsigned boarHealthAfter,bool playerAlive)noexcept{
        if(!playerAlive){Clear();return;}
        if(!std::isfinite(dt)||dt<=0)return;
        if(boarHealthAfter<boarHealthBefore){elapsed_=0;return;}
        elapsed_=(std::min)(Duration,elapsed_+dt);
    }
    float Weight()const noexcept{
        constexpr float reach=.07f;
        if(elapsed_>=Duration)return 0;
        const float t=elapsed_<reach?elapsed_/reach:(Duration-elapsed_)/(Duration-reach);
        const float eased=t*t*(3.f-2.f*t);
        return elapsed_<reach?.64f+.36f*eased:eased;
    }
private:
    float elapsed_{Duration};
};

// 使用已求解的姿勢建立繪圖暫存；骨盆、髖、膝、踝都留在原來的分支。
inline bool Apply(const Aevrynd::Skeleton3D& skeleton,const Aevrynd::AnimationPose3D& original,
    float weight,Aevrynd::AnimationPose3D& drawing){
    if(!std::isfinite(weight)||weight<=0||weight>1||&drawing==&original||
       skeleton.joints.size()!=15||original.localTransforms.size()!=15||original.globalMatrices.size()!=15||
       skeleton.joints[1].parent!=0||skeleton.joints[3].parent!=1||skeleton.joints[4].parent!=3||
       skeleton.joints[6].parent!=1||skeleton.joints[7].parent!=6||
       skeleton.joints[9].parent!=0||skeleton.joints[12].parent!=0)return false;
    for(unsigned joint:{1u,3u,4u,6u,7u})
        if(original.localTransforms[joint].usesQuaternion)return false;
    drawing=original;
    auto& pose=drawing.localTransforms;
    pose[1].rotation.y+=.23f*weight;
    pose[3].rotation.x-=.29f*weight;
    pose[4].rotation.x-=.38f*weight;
    pose[6].rotation.x-=.84f*weight;
    pose[6].rotation.z+=.11f*weight;
    pose[7].rotation.x-=.69f*weight;
    Aevrynd::BuildAnimationPoseGlobalMatrices3D(skeleton,drawing);
    // 下肢保留原本 IK 的實際矩陣，不以重算誤差改變接地。
    drawing.globalMatrices[0]=original.globalMatrices[0];
    for(unsigned joint=9;joint<15;++joint)drawing.globalMatrices[joint]=original.globalMatrices[joint];
    return true;
}
}
