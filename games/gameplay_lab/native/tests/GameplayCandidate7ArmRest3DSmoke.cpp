#include "GameplayCandidate7Package3D.h"
#include "GameplayAnimation3D.h"
#include <iostream>
using namespace Aevrynd;using namespace GameplayLab;
int main(int argc,char** argv){
    if(argc!=2)return 2;auto loaded=LoadSkinnedModelAssetFileDetailed(argv[1]);if(!loaded)return 3;
    auto& model=loaded.asset.model;GameplayAnimation3D animation;std::string error;if(!animation.Initialize(loaded.asset,&error))return 4;
    const auto map=animation.Rig();float maxBone=0,maxPhaseError=0,maxIdle=0;unsigned samples=0;
    auto direction=[&](const AnimationPose3D& p,unsigned side){return Normalize(TransformPoint(p.globalMatrices[map.Joint(side?HumanoidJoint3D::RightLowerArm:HumanoidJoint3D::LeftLowerArm)],{})-TransformPoint(p.globalMatrices[map.Joint(side?HumanoidJoint3D::RightUpperArm:HumanoidJoint3D::LeftUpperArm)],{}));};
    for(auto state:{AnimationState3D::Idle,AnimationState3D::Walk,AnimationState3D::Run}){
        AnimationPose3D firstBefore,firstAfter;
        for(unsigned step=0;step<=40;++step){
            animation.SetState(state,true);animation.SynchronizeNormalizedPhase(float(step)/40);AnimationPose3D before;if(!animation.EvaluateInto(before))return 5;
            auto after=before;ApplyCandidate7ArmRest3D(model,map,after);if(step==0){firstBefore=before;firstAfter=after;}
            for(unsigned side=0;side<2;++side){
                auto a=direction(before,side),b=direction(after,side);
                if(state==AnimationState3D::Idle){float old=std::acos(Clamp(-a.y,-1.f,1.f))*180/3.14159265f,now=std::acos(Clamp(-b.y,-1.f,1.f))*180/3.14159265f;maxIdle=(std::max)(maxIdle,now);if(step==0)std::cout<<"side="<<side<<" old="<<old<<" corrected="<<now<<'\n';if(old<35||now>15)return 10;}
                // 父骨也在動畫，故以局部四元數相位內積檢驗固定左乘，不能混入全域軀幹擺動。
                const auto joint=map.Joint(side?HumanoidJoint3D::RightUpperArm:HumanoidJoint3D::LeftUpperArm);
                auto phaseDot=[&](const AnimationPose3D& p,const AnimationPose3D& first){auto quat=[](const JointTransform3D& t){return t.usesQuaternion?t.quaternion:QuaternionFromEuler3D(t.rotation);};const auto q=quat(p.localTransforms[joint]),r=quat(first.localTransforms[joint]);return std::abs(q.x*r.x+q.y*r.y+q.z*r.z+q.w*r.w);};
                maxPhaseError=(std::max)(maxPhaseError,std::abs(phaseDot(before,firstBefore)-phaseDot(after,firstAfter)));
            }
            for(std::size_t j=1;j<53;++j){const auto parent=model.skeleton.joints[j].parent;if(parent<0)continue;auto length=[&](const AnimationPose3D& p){return Length(TransformPoint(p.globalMatrices[j],{})-TransformPoint(p.globalMatrices[std::size_t(parent)],{}));};maxBone=(std::max)(maxBone,std::abs(length(before)-length(after)));}
            ++samples;
        }
    }
    AnimationPose3D bind;for(const auto& j:model.skeleton.joints)bind.localTransforms.push_back(j.bindTransform);bind.globalMatrices.resize(53);BuildAnimationPoseGlobalMatrices3D(model.skeleton,bind);
    auto correctedBind=bind;ApplyCandidate7ArmRest3D(model,map,correctedBind);
    const auto left=direction(correctedBind,0),right=direction(correctedBind,1);
    const float mirrorError=Length(Vec3{-left.x,left.y,left.z}-right);
    std::cout<<"bindMirrorDirectionError="<<mirrorError<<'\n';if(mirrorError>1e-3f)return 13;
    unsigned rejected=0;for(unsigned k=0;k<4;++k){auto m=model;auto p=bind;auto rig=map;const auto upper=rig.Joint(HumanoidJoint3D::LeftUpperArm),lower=rig.Joint(HumanoidJoint3D::LeftLowerArm);
        if(k==0)p.localTransforms[upper].translation.x=std::numeric_limits<float>::quiet_NaN();
        if(k==1)m.skeleton.joints[lower].parent=0;
        if(k==2)m.skeleton.joints[lower].bindTransform.translation={};
        if(k==3)for(auto pair:std::array<std::pair<HumanoidJoint3D,HumanoidJoint3D>,3>{{{HumanoidJoint3D::LeftUpperArm,HumanoidJoint3D::RightUpperArm},{HumanoidJoint3D::LeftLowerArm,HumanoidJoint3D::RightLowerArm},{HumanoidJoint3D::LeftHand,HumanoidJoint3D::RightHand}}})std::swap(rig.joints[std::size_t(pair.first)],rig.joints[std::size_t(pair.second)]);
        try{ApplyCandidate7ArmRest3D(m,rig,p);}catch(const std::exception&){++rejected;}
    }
    std::cout<<"samples="<<samples<<" maxIdleDegrees="<<maxIdle<<" maxBoneErrorMeters="<<maxBone<<" phaseDotError="<<maxPhaseError<<" hostile="<<rejected<<"/4\n";
    return maxBone<=1e-5f&&maxPhaseError<=1e-3f&&rejected==4?0:11;
}
