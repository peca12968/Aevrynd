#include "GameplayCandidate7Package3D.h"
#include "GameplayFirstPersonPose3D.h"
#include <iostream>
using namespace Aevrynd;
using namespace GameplayLab;
int main(int argc,char** argv){
    if(argc!=2)return 2;
    auto source=LoadSkinnedModelAssetFileDetailed(argv[1]);if(!source)return 3;
    GameplayAnimation3D animation;std::string error;
    if(!animation.Initialize(source.asset,&error))return 4;
    CharacterEquipmentAsset3D weapon;
    if(!CreateCharacterEquipmentAsset3D(source.asset,animation.Rig(),CharacterEquipmentShape3D::Sword,weapon,&error))return 5;
    const auto& rig=animation.Rig();AnimationPose3D pose;
    if(!animation.EvaluateInto(pose))return 6;
    for(auto side:{HumanoidJoint3D::LeftLowerArm,HumanoidJoint3D::LeftHand,HumanoidJoint3D::RightLowerArm,HumanoidJoint3D::RightHand}){
        const auto j=rig.Joint(side);std::cout<<"BONE="<<source.asset.jointNames[j]<<" length="<<Length(source.asset.model.skeleton.joints[j].bindTransform.translation)<<'\n';
    }
    unsigned profile{};
    for(const Vec3 grip:std::array<Vec3,2>{{{.31f,-.055f,.47f},Candidate7ReadyGrip3D}}){
        unsigned ready{};float maxError{};int firstFailure=-1;
        for(auto state:{AnimationState3D::Idle,AnimationState3D::Walk,AnimationState3D::Run,AnimationState3D::Attack1,AnimationState3D::Attack2,AnimationState3D::Attack3})for(float phase:{0.f,.1f,.25f,.5f,.75f,.9f,1.f})for(float blend:{0.f,1.f}){
            animation.SetState(state,true);animation.SynchronizeNormalizedPhase(phase);if(!animation.EvaluateInto(pose))return 7;
            GameplayFirstPersonPoseInput3D input;input.character=&source.asset.model;input.rig=&rig;input.sourcePose=&pose;input.sword=&weapon.swordGeometry;
            input.camera.position={0,1.7f,-3};input.camera.target={0,1.7f,0};input.camera.up={0,1,0};input.state=state;input.phase=phase;input.enterBlend=blend;input.readyGrip=grip;
            GameplayFirstPersonPoseWorkspace3D workspace;GameplayFirstPersonPoseOutput3D output;
            auto status=EvaluateGameplayFirstPersonPose3D(input,workspace,output);
            ready+=status==GameplayFirstPersonPoseStatus3D::Ready?1u:0u;
            if(status!=GameplayFirstPersonPoseStatus3D::Ready&&firstFailure<0)firstFailure=int(status);
            maxError=(std::max)(maxError,output.maximumBoneLengthError);
        }
        std::cout<<"GRIP="<<grip.x<<','<<grip.y<<','<<grip.z<<" ready="<<ready<<" firstStatus="<<firstFailure<<" maxBoneLengthError="<<maxError<<'\n';
        if((profile==0&&(ready!=0||firstFailure!=int(GameplayFirstPersonPoseStatus3D::Unreachable)))||(profile==1&&(ready!=84||maxError>1e-5f)))return 10;
        ++profile;
    }
    return 0;
}
