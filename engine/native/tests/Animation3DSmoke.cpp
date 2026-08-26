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
    std::cout<<"joints="<<skeleton.joints.size()<<" loopX="<<looped.localTransforms[0].translation.x<<" childY="<<childPosition.y<<" blend="<<player.BlendAmount()<<" wrapX="<<wrapped.localTransforms[0].translation.x<<'\n';return 0;
}
