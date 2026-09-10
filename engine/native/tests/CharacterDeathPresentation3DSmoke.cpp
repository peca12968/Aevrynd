#include <Aevrynd/CharacterPresentation3D.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
using namespace Aevrynd;
int main(int argc,char** argv){
    if(argc!=3)return 2;
    for(int assetIndex=1;assetIndex<argc;++assetIndex){
        const auto loaded=LoadSkinnedModelAssetFileDetailed(argv[assetIndex]);
        if(!loaded)return 3;
        const auto& skeleton=loaded.asset.model.skeleton;
        HumanoidSkeletonMap3D rig;FallbackHumanoidAnimationSet3D clips;
        if(!ResolveCharacterRig3D(loaded.asset,rig)||!CreateFallbackHumanoidAnimationSet3D(loaded.asset,rig,clips))return 4;
        const auto& death=clips.clips[std::size_t(FallbackHumanoidClip3D::Death)];
        if(death.looping||death.duration!=1.f||!death.Valid(skeleton))return 5;
        std::set<std::size_t> joints;
        for(const auto& track:death.tracks)if(!joints.insert(track.joint).second)return 6;
        for(const auto semantic:{HumanoidJoint3D::Hips,HumanoidJoint3D::Spine,HumanoidJoint3D::Head,
            HumanoidJoint3D::LeftUpperArm,HumanoidJoint3D::LeftLowerArm,HumanoidJoint3D::RightUpperArm,HumanoidJoint3D::RightLowerArm,
            HumanoidJoint3D::LeftUpperLeg,HumanoidJoint3D::LeftLowerLeg,HumanoidJoint3D::RightUpperLeg,HumanoidJoint3D::RightLowerLeg,
            HumanoidJoint3D::LeftFoot,HumanoidJoint3D::RightFoot}){
            const auto index=rig.Joint(semantic);
            const auto track=std::ranges::find(death.tracks,index,&AnimationTrack3D::joint);
            if(track==death.tracks.end()||track->keyframes.size()<4){std::cerr<<"DEATH_ARTICULATION missing staged semantic="<<unsigned(semantic)<<" tracks="<<death.tracks.size()<<'\n';return 7;}
        }
        SkinningWorkspace3D workspace;Mesh3D mesh;AnimationPose3D pose;
        for(int frame=0;frame<=60;++frame){
            SampleAnimationClipInto(skeleton,death,float(frame)/60.f,pose);
            if(!SkinModelInto(loaded.asset.model,pose,workspace,mesh))return 8;
            for(const auto& vertex:mesh.vertices)if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z))return 9;
            for(std::size_t j=0;j<skeleton.joints.size();++j){
                if(Length(pose.localTransforms[j].scale-skeleton.joints[j].bindTransform.scale)>1e-6f)return 10;
                if(j!=rig.Joint(HumanoidJoint3D::Hips)&&Length(pose.localTransforms[j].translation-skeleton.joints[j].bindTransform.translation)>1e-6f)return 11;
            }
        }
        std::cout<<"DEATH_STAGED_PASS asset="<<argv[assetIndex]<<" tracks="<<death.tracks.size()<<" samples=61 GPU_NOT_RUN contact_NOT_VALIDATED\n";
    }
    return 0;
}
