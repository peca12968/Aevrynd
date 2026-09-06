#include "GameplayFirstPersonPose3D.h"
#include <algorithm>
#include <cmath>
#include <utility>

using namespace Aevrynd;
namespace GameplayLab {
namespace {
constexpr float Pi=3.14159265358979323846f;
bool Finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool Finite(const Matrix4& m){for(const auto& row:m.values)for(float v:row)if(!std::isfinite(v))return false;return true;}
bool Inverse(const Matrix4& m,Matrix4& out){
    const float a=m[0][0],b=m[0][1],c=m[0][2],d=m[1][0],e=m[1][1],f=m[1][2],g=m[2][0],h=m[2][1],i=m[2][2];
    const float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(!std::isfinite(det)||std::abs(det)<1e-8f)return false;
    out=Matrix4::Identity();out[0][0]=(e*i-f*h)/det;out[0][1]=(c*h-b*i)/det;out[0][2]=(b*f-c*e)/det;
    out[1][0]=(f*g-d*i)/det;out[1][1]=(a*i-c*g)/det;out[1][2]=(c*d-a*f)/det;
    out[2][0]=(d*h-e*g)/det;out[2][1]=(b*g-a*h)/det;out[2][2]=(a*e-b*d)/det;
    const Vec3 t=TransformPoint(out,{m[0][3],m[1][3],m[2][3]});out[0][3]=-t.x;out[1][3]=-t.y;out[2][3]=-t.z;return Finite(out);
}
Quaternion3D Between(Vec3 from,Vec3 to){
    from=Normalize(from);to=Normalize(to);const float cosine=Clamp(Dot(from,to),-1.f,1.f);
    if(cosine<-.9999f){Vec3 axis=Cross(from,{1,0,0});if(Length(axis)<.001f)axis=Cross(from,{0,1,0});axis=Normalize(axis);return {axis.x,axis.y,axis.z,0};}
    const Vec3 cross=Cross(from,to);return NormalizeQuaternion3D({cross.x,cross.y,cross.z,1.f+cosine});
}
Quaternion3D RotationQuaternion(const Matrix4& m){
    Quaternion3D q;const float trace=m[0][0]+m[1][1]+m[2][2];
    if(trace>0){const float s=std::sqrt(trace+1.f)*2;q={ (m[2][1]-m[1][2])/s,(m[0][2]-m[2][0])/s,(m[1][0]-m[0][1])/s,s*.25f};}
    else if(m[0][0]>m[1][1]&&m[0][0]>m[2][2]){const float s=std::sqrt(1.f+m[0][0]-m[1][1]-m[2][2])*2;q={s*.25f,(m[0][1]+m[1][0])/s,(m[0][2]+m[2][0])/s,(m[2][1]-m[1][2])/s};}
    else if(m[1][1]>m[2][2]){const float s=std::sqrt(1.f+m[1][1]-m[0][0]-m[2][2])*2;q={(m[0][1]+m[1][0])/s,s*.25f,(m[1][2]+m[2][1])/s,(m[0][2]-m[2][0])/s};}
    else{const float s=std::sqrt(1.f+m[2][2]-m[0][0]-m[1][1])*2;q={(m[0][2]+m[2][0])/s,(m[1][2]+m[2][1])/s,s*.25f,(m[1][0]-m[0][1])/s};}
    return NormalizeQuaternion3D(q);
}
bool SolveArm(const Skeleton3D& skeleton,AnimationPose3D& pose,const Matrix4& root,
    std::size_t upper,std::size_t lower,std::size_t hand,Vec3 target,const Matrix4& handRotation,Vec3 pole,float& lengthError){
    const auto position=[&](std::size_t joint){return TransformPoint(pose.globalMatrices[joint],{});};
    const Vec3 shoulder=position(upper),elbow=position(lower),wrist=position(hand);
    const float a=Length(elbow-shoulder),b=Length(wrist-elbow),distance=Length(target-shoulder);
    if(a<.001f||b<.001f||distance<=std::abs(a-b)+.001f||distance>=a+b-.001f)return false;
    const Vec3 direction=Normalize(target-shoulder);Vec3 bend=pole-shoulder; bend=bend-direction*Dot(bend,direction);
    if(Length(bend)<.001f)return false;bend=Normalize(bend);
    const float along=(a*a-b*b+distance*distance)/(2*distance);
    const Vec3 elbowTarget=shoulder+direction*along+bend*std::sqrt((std::max)(0.f,a*a-along*along));
    const auto rotate=[&](std::size_t joint,std::size_t child,Vec3 destination){
        const int parent=skeleton.joints[joint].parent;if(parent<0)return false;
        Matrix4 inverse;if(!Inverse(pose.globalMatrices[std::size_t(parent)],inverse))return false;
        const Vec3 desired=TransformPoint(inverse,destination)-TransformPoint(inverse,position(joint));
        pose.localTransforms[joint].quaternion=Between(pose.localTransforms[child].translation,desired);pose.localTransforms[joint].usesQuaternion=true;
        BuildAnimationPoseGlobalMatrices3D(skeleton,pose,root);return true;
    };
    if(!rotate(upper,lower,elbowTarget)||!rotate(lower,hand,target))return false;
    Matrix4 inverse;if(!Inverse(pose.globalMatrices[lower],inverse))return false;
    pose.localTransforms[hand].quaternion=RotationQuaternion(inverse*handRotation);pose.localTransforms[hand].usesQuaternion=true;
    BuildAnimationPoseGlobalMatrices3D(skeleton,pose,root);
    lengthError=(std::max)({lengthError,std::abs(Length(position(lower)-position(upper))-a),std::abs(Length(position(hand)-position(lower))-b)});
    return Length(position(hand)-target)<.0001f&&lengthError<.0001f;
}
}
GameplayFirstPersonPoseStatus3D EvaluateGameplayFirstPersonPose3D(const GameplayFirstPersonPoseInput3D& input,
    GameplayFirstPersonPoseWorkspace3D& workspace,GameplayFirstPersonPoseOutput3D& output){
    using Status=GameplayFirstPersonPoseStatus3D;
    if(!input.character||!input.rig||!input.sourcePose||!input.sword||!input.sword->available||
       &workspace.scratch==&output||input.sourcePose==&workspace.scratch.pose||input.sourcePose==&output.pose||
       !std::isfinite(input.phase)||input.phase<0||input.phase>1||!std::isfinite(input.enterBlend)||input.enterBlend<0||input.enterBlend>1||
       !Finite(input.shoulderCenter)||!Finite(input.readyGrip)||!Finite(input.sword->grip)||
       !Finite(input.camera.position)||!Finite(input.camera.target)||!Finite(input.camera.up)||unsigned(input.state)>unsigned(AnimationState3D::Death))return Status::InvalidInput;
    const auto& skeleton=input.character->skeleton;const auto& rig=*input.rig;
    if(skeleton.joints.empty()||skeleton.joints.size()>128||!rig.Valid(skeleton,input.character->skeletonCompatibilityId)||input.sourcePose->localTransforms.size()!=skeleton.joints.size()||input.sourcePose->globalMatrices.size()!=skeleton.joints.size())return Status::InvalidRig;
    for(const auto& transform:input.sourcePose->localTransforms){
        if(!Finite(transform.translation)||!Finite(transform.scale)||Length(transform.scale-Vec3{1,1,1})>.0001f)return Status::InvalidInput;
        // 必須先驗證原始有效表示；Quaternion 正規化可能把無效資料退回 identity。
        if(transform.usesQuaternion){
            const auto& q=transform.quaternion;
            if(!std::isfinite(q.x)||!std::isfinite(q.y)||!std::isfinite(q.z)||!std::isfinite(q.w))return Status::InvalidInput;
            const double norm=double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w;
            if(!std::isfinite(norm)||norm<=1e-12)return Status::InvalidInput;
        }else if(!Finite(transform.rotation))return Status::InvalidInput;
        if(!Finite(transform.Matrix()))return Status::InvalidInput;
    }
    for(const auto& matrix:input.sourcePose->globalMatrices)if(!Finite(matrix))return Status::InvalidInput;
    const Vec3 forward=Normalize(input.camera.target-input.camera.position),right=Normalize(Cross(input.camera.up,forward));
    if(!Finite(forward)||!Finite(right)||Length(forward)<.001f||Length(right)<.001f)return Status::InvalidInput;const Vec3 up=Cross(forward,right);
    Matrix4 cameraWorld=Matrix4::Identity();for(std::size_t row=0;row<3;++row){const float r[]={right.x,right.y,right.z},u[]={up.x,up.y,up.z},f[]={forward.x,forward.y,forward.z};cameraWorld[row][0]=r[row];cameraWorld[row][1]=u[row];cameraWorld[row][2]=f[row];}
    cameraWorld[0][3]=input.camera.position.x;cameraWorld[1][3]=input.camera.position.y;cameraWorld[2][3]=input.camera.position.z;
    auto& candidate=workspace.scratch;candidate.valid=false;candidate.palmContactVerified=false;candidate.maximumBoneLengthError=0;candidate.pose.localTransforms=input.sourcePose->localTransforms;
    const std::array<std::size_t,2> upper{rig.Joint(HumanoidJoint3D::LeftUpperArm),rig.Joint(HumanoidJoint3D::RightUpperArm)},lower{rig.Joint(HumanoidJoint3D::LeftLowerArm),rig.Joint(HumanoidJoint3D::RightLowerArm)},hand{rig.Joint(HumanoidJoint3D::LeftHand),rig.Joint(HumanoidJoint3D::RightHand)};
    for(std::size_t side=0;side<2;++side){
        if(skeleton.joints[lower[side]].parent!=int(upper[side])||skeleton.joints[hand[side]].parent!=int(lower[side]))return Status::InvalidRig;
        // 肩帶祖先回到固定 bind，防止右手動畫反向帶動整組雙臂；腿部來源未改寫。
        for(int joint=skeleton.joints[upper[side]].parent;joint>=0;joint=skeleton.joints[std::size_t(joint)].parent)candidate.pose.localTransforms[std::size_t(joint)]=skeleton.joints[std::size_t(joint)].bindTransform;
    }
    BuildAnimationPoseGlobalMatrices3D(skeleton,candidate.pose);
    const Vec3 mid=(TransformPoint(candidate.pose.globalMatrices[upper[0]],{})+TransformPoint(candidate.pose.globalMatrices[upper[1]],{}))*.5f;
    const Matrix4 rootCamera=CreateTranslationMatrix(input.shoulderCenter)*CreateRotationMatrix({0,Pi,0})*CreateTranslationMatrix(mid*-1.f);
    candidate.modelToWorld=cameraWorld*rootCamera;
    BuildAnimationPoseGlobalMatrices3D(skeleton,candidate.pose,rootCamera);
    const float blend=input.enterBlend*input.enterBlend*(3-2*input.enterBlend),wave=std::sin(input.phase*2*Pi);
    const bool attack=input.state==AnimationState3D::Attack1||input.state==AnimationState3D::Attack2||input.state==AnimationState3D::Attack3;
    const float drive=attack?std::sin(input.phase*Pi):0.f;
    candidate.targetGripCamera=input.readyGrip+Vec3{drive*.001f,-.002f*(1-blend)+drive*.001f,drive*.001f};
    // 刀身方向與握點視線分離，保留可辨識的向上刀身，不以透視縮短換取包圍矩形通過。
    // 固定 Y * X * Z 姿態保留原始武器尺度與毫米級動作；真正構圖由獨立頂點投影驗證。
    const Matrix4 handRotation=CreateRotationMatrix({1.13f-drive*.005f,.6f,3.23f});
    const Vec3 gripOffset=TransformPoint(handRotation,input.sword->grip);
    const Vec3 targetWrist=candidate.targetGripCamera-gripOffset;
    const Vec3 leftTarget{-.20f,-.20f+.01f*wave,.27f};
    for(std::size_t side=0;side<2;++side)candidate.shouldersCamera[side]=TransformPoint(candidate.pose.globalMatrices[upper[side]],{});
    if(!SolveArm(skeleton,candidate.pose,rootCamera,upper[1],lower[1],hand[1],targetWrist,handRotation,{.42f,-.52f,.12f},candidate.maximumBoneLengthError)||
       !SolveArm(skeleton,candidate.pose,rootCamera,upper[0],lower[0],hand[0],leftTarget,CreateRotationMatrix({.5f,0,Pi}),{-.42f,-.52f,.10f},candidate.maximumBoneLengthError))return Status::Unreachable;
    for(std::size_t side=0;side<2;++side)candidate.wristsCamera[side]=TransformPoint(candidate.pose.globalMatrices[hand[side]],{});
    candidate.actualGripCamera=TransformPoint(candidate.pose.globalMatrices[hand[1]],input.sword->grip);
    candidate.gripError=Length(candidate.actualGripCamera-candidate.targetGripCamera);
    if(!std::isfinite(candidate.gripError)||candidate.gripError>.0001f)return Status::SolverFailure;
    // 回傳模型空間 palette；雙臂與武器共用 modelToWorld，避免重複乘相機根。
    BuildAnimationPoseGlobalMatrices3D(skeleton,candidate.pose);
    candidate.valid=true;std::swap(candidate,output);return Status::Ready;
}
}
