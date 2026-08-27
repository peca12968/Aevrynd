#include <Aevrynd/Animation3D.h>
#include <cmath>
#include <iostream>

using namespace Aevrynd;
namespace {
bool Near(float left,float right,float tolerance=.001f){return std::abs(left-right)<=tolerance;}
}

int main(){
    Skeleton3D skeleton{{{-1,{{0,1,0},{},{1,1,1}}},{0,{{0,2,0},{},{1,1,1}}}}};
    if(!skeleton.Valid()||Skeleton3D{{{0,{}}}}.Valid()){std::cerr<<"Skeleton hierarchy validation failed\n";return 1;}
    AnimationClip3D clip{1,true,{{0,{{0,{{0,1,0},{0,3.f,0},{1,1,1}}},{1,{{2,1,0},{0,-3.f,0},{1,1,1}}}}}}};
    if(!clip.Valid(skeleton)){std::cerr<<"Animation clip validation failed\n";return 2;}
    const AnimationPose3D looped=SampleAnimationClip(skeleton,clip,1.25f);
    if(!Near(looped.localTransforms[0].translation.x,.5f)||std::abs(std::abs(looped.localTransforms[0].rotation.y)-3.070796f)>.002f){std::cerr<<"Loop or shortest-angle sampling failed\n";return 3;}
    const Vec3 childPosition=TransformPoint(looped.globalMatrices[1],{});
    if(!Near(childPosition.y,3.f)){std::cerr<<"Hierarchical pose construction failed\n";return 4;}
    AnimationClip3D raised{1,false,{{1,{{0,{{0,2,0},{},{1,1,1}}},{1,{{0,3,0},{},{1,1,1}}}}}}};
    AnimationPlayer3D player;player.Play(clip,0);player.Update(.5f);player.Play(raised,1.f);player.Update(.5f);const AnimationPose3D blended=player.Evaluate(skeleton);if(!Near(player.BlendAmount(),.5f)||!Near(blended.localTransforms[1].translation.y,2.25f)){std::cerr<<"Animation transition blending failed\n";return 5;}
    AnimationTrack3D invalidTrack{4,{{0,{}}}};if(AnimationClip3D{1,true,{invalidTrack}}.Valid(skeleton)){std::cerr<<"Invalid joint acceptance failed\n";return 6;}
    AnimationClip3D seamless{1,true,{{0,{{.25f,{{0,1,0},{},{1,1,1}}},{.75f,{{2,1,0},{},{1,1,1}}}}}}};const AnimationPose3D wrapped=SampleAnimationClip(skeleton,seamless,.875f);if(!Near(wrapped.localTransforms[0].translation.x,1.5f)){std::cerr<<"Loop boundary interpolation failed\n";return 7;}
    const float radians179=179.f*3.14159265358979323846f/180.f;JointTransform3D quaternionFrom{{},{},{1,1,1},QuaternionFromEuler3D({0,radians179,0}),true},quaternionTo{{},{},{1,1,1},QuaternionFromEuler3D({0,-radians179,0}),true};const JointTransform3D quaternionMid=InterpolateJointTransform(quaternionFrom,quaternionTo,.5f);const Quaternion3D normalized=quaternionMid.quaternion;if(!quaternionMid.usesQuaternion||!Near(normalized.x*normalized.x+normalized.y*normalized.y+normalized.z*normalized.z+normalized.w*normalized.w,1.f,.0001f)||std::abs(normalized.w)>.001f){std::cerr<<"Quaternion shortest arc or normalization failed\n";return 8;}
    const Quaternion3D sameRotation=SlerpQuaternion3D({0,0,0,1},{0,0,0,-1},.5f);if(!Near(std::abs(sameRotation.w),1.f,.0001f)){std::cerr<<"Quaternion sign-equivalent interpolation failed\n";return 9;}
    JointTransform3D zeroQuaternion{};zeroQuaternion.usesQuaternion=true;zeroQuaternion.quaternion={0,0,0,0};AnimationClip3D invalidQuaternion{1,false,{{0,{{0,zeroQuaternion}}}}};if(invalidQuaternion.Valid(skeleton)||Skeleton3D{{{-1,zeroQuaternion}}}.Valid()){std::cerr<<"Zero-length quaternion validation failed\n";return 10;}
    const Quaternion3D safeZero=NormalizeQuaternion3D({0,0,0,0});if(!Near(safeZero.w,1.f)||safeZero.x!=0||safeZero.y!=0||safeZero.z!=0){std::cerr<<"Zero-length quaternion safe normalization failed\n";return 11;}
    Skeleton3D quaternionBind{{{-1,{{1,2,3},{},{1,1,1},QuaternionFromEuler3D({0,3.14159265358979323846f*.5f,0}),true}}}};const AnimationPose3D bindPose=SampleAnimationClip(quaternionBind,{},0);const Vec3 bindPoint=TransformPoint(bindPose.globalMatrices[0],{1,0,0});if(!Near(bindPoint.x,1.f,.002f)||!Near(bindPoint.y,2.f,.002f)||!Near(bindPoint.z,2.f,.002f)){std::cerr<<"Quaternion bind pose matrix failed\n";return 12;}
    AnimationClip3D quaternionClip{1,false,{{0,{{0,quaternionFrom},{1,quaternionTo}}}}};const AnimationPose3D quaternionPose=SampleAnimationClip(quaternionBind,quaternionClip,.5f);if(!quaternionClip.Valid(quaternionBind)||!quaternionPose.localTransforms[0].usesQuaternion){std::cerr<<"Quaternion keyframe sampling failed\n";return 13;}
    std::cout<<"joints="<<skeleton.joints.size()<<" loopX="<<looped.localTransforms[0].translation.x<<" childY="<<childPosition.y<<" blend="<<player.BlendAmount()<<" wrapX="<<wrapped.localTransforms[0].translation.x<<" quaternionW="<<normalized.w<<'\n';return 0;
}
