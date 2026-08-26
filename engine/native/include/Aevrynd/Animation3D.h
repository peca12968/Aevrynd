#pragma once
#include "Mesh3D.h"
#include <cstddef>
#include <vector>

namespace Aevrynd {
struct JointTransform3D {
    Vec3 translation{};
    Vec3 rotation{};
    Vec3 scale{1,1,1};
    Matrix4 Matrix()const{return CreateTransformMatrix(translation,rotation,scale);}
};

struct SkeletonJoint3D {
    int parent{-1};
    JointTransform3D bindTransform{};
};

struct Skeleton3D {
    std::vector<SkeletonJoint3D> joints;
    bool Valid()const;
};

struct AnimationKeyframe3D {
    float time{};
    JointTransform3D transform{};
};

struct AnimationTrack3D {
    std::size_t joint{};
    std::vector<AnimationKeyframe3D> keyframes;
};

struct AnimationClip3D {
    float duration{1.f};
    bool looping{true};
    std::vector<AnimationTrack3D> tracks;
    bool Valid(const Skeleton3D& skeleton)const;
};

struct AnimationPose3D {
    std::vector<JointTransform3D> localTransforms;
    std::vector<Matrix4> globalMatrices;
};

JointTransform3D InterpolateJointTransform(JointTransform3D from,JointTransform3D to,float amount);
AnimationPose3D SampleAnimationClip(const Skeleton3D& skeleton,const AnimationClip3D& clip,float time,Matrix4 root=Matrix4::Identity());
AnimationPose3D BlendAnimationPoses(const Skeleton3D& skeleton,const AnimationPose3D& from,const AnimationPose3D& to,float amount,Matrix4 root=Matrix4::Identity());

class AnimationPlayer3D {
public:
    void Play(const AnimationClip3D& clip,float blendDuration=.15f,bool restart=false);
    void Update(float deltaTime);
    AnimationPose3D Evaluate(const Skeleton3D& skeleton,Matrix4 root=Matrix4::Identity())const;
    bool Playing()const{return current_!=nullptr;}
    float Time()const{return currentTime_;}
    float BlendAmount()const;
private:
    const AnimationClip3D* current_{};
    const AnimationClip3D* previous_{};
    float currentTime_{},previousTime_{},blendDuration_{},blendRemaining_{};
};
}
