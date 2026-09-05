#include "GameplayAnimation3D.h"

#include <algorithm>
#include <cmath>

using namespace Aevrynd;
namespace GameplayLab {
float GameplayPresentationDelta3D(float simulationDelta) noexcept {
    return std::isfinite(simulationDelta)&&simulationDelta>0.f?Aevrynd::Clamp(simulationDelta,.001f,.1f):1.f/60.f;
}
namespace {
constexpr float Pi = 3.14159265358979323846f;
FallbackHumanoidClip3D ClipId(AnimationState3D state){return FallbackHumanoidClip3D(std::uint8_t(state));}

Quaternion3D MultiplyRotation(Quaternion3D left, Quaternion3D right) noexcept {
    return NormalizeQuaternion3D({
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z});
}

void OffsetRotation(JointTransform3D& transform, Vec3 offset) noexcept {
    if (transform.usesQuaternion)
        transform.quaternion = MultiplyRotation(transform.quaternion, QuaternionFromEuler3D(offset));
    else {
        transform.rotation.x += offset.x;
        transform.rotation.y += offset.y;
        transform.rotation.z += offset.z;
    }
}

float Pulse(float phase, float center, float radius) noexcept {
    return Clamp(1.f - std::abs(phase - center) / radius, 0.f, 1.f);
}

bool IsAttack(AnimationState3D state) noexcept {
    return state == AnimationState3D::Attack1 || state == AnimationState3D::Attack2 ||
           state == AnimationState3D::Attack3;
}

void SolveSupportHandGrip(const Skeleton3D& skeleton, const HumanoidSkeletonMap3D& rig,
                          AnimationPose3D& pose, Matrix4 root) {
    const std::size_t weaponHand=rig.Joint(HumanoidJoint3D::RightHand);
    const std::size_t upper=rig.Joint(HumanoidJoint3D::LeftUpperArm);
    const std::size_t lower=rig.Joint(HumanoidJoint3D::LeftLowerArm);
    const std::size_t supportHand=rig.Joint(HumanoidJoint3D::LeftHand);
    if(skeleton.joints[upper].parent<0)return;
    const Vec3 target=TransformPoint(pose.globalMatrices[weaponHand],{0,-.34f,0});
    const Vec3 shoulder=TransformPoint(pose.globalMatrices[upper],{});
    const Vec3 elbow=TransformPoint(pose.globalMatrices[lower],{});
    const Vec3 hand=TransformPoint(pose.globalMatrices[supportHand],{});
    const float upperLength=Length(elbow-shoulder),lowerLength=Length(hand-elbow);
    Vec3 reach=target-shoulder;float distance=Length(reach);
    if(upperLength<.0001f||lowerLength<.0001f||distance<.0001f)return;
    const Vec3 direction=reach*(1.f/distance);
    distance=Clamp(distance,std::abs(upperLength-lowerLength)+.0001f,upperLength+lowerLength-.0001f);
    const Vec3 solvedHand=shoulder+direction*distance;
    Vec3 bend=(elbow-shoulder)-direction*Dot(elbow-shoulder,direction);
    if(Length(bend)<.0001f){bend=Cross(direction,{0,1,0});if(Length(bend)<.0001f)bend=Cross(direction,{1,0,0});}
    bend=Normalize(bend);
    const float along=(upperLength*upperLength-lowerLength*lowerLength+distance*distance)/(2.f*distance);
    const float height=std::sqrt((std::max)(0.f,upperLength*upperLength-along*along));
    const Vec3 solvedElbow=shoulder+direction*along+bend*height;
    const auto inverseVector=[](const Matrix4& matrix,Vec3 value){
        const float a=matrix[0][0],b=matrix[0][1],c=matrix[0][2],d=matrix[1][0],e=matrix[1][1],f=matrix[1][2],g=matrix[2][0],h=matrix[2][1],i=matrix[2][2];
        const float determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
        if(std::abs(determinant)<.000001f)return Vec3{};
        const float inverse=1.f/determinant;
        return Vec3{((e*i-f*h)*value.x+(c*h-b*i)*value.y+(b*f-c*e)*value.z)*inverse,
                    ((f*g-d*i)*value.x+(a*i-c*g)*value.y+(c*d-a*f)*value.z)*inverse,
                    ((d*h-e*g)*value.x+(b*g-a*h)*value.y+(a*e-b*d)*value.z)*inverse};
    };
    const auto between=[](Vec3 from,Vec3 to){
        from=Normalize(from);to=Normalize(to);const float cosine=Clamp(Dot(from,to),-1.f,1.f);
        if(cosine<-.9999f){Vec3 axis=Normalize(Cross(from,{1,0,0}));if(Length(axis)<.0001f)axis=Normalize(Cross(from,{0,1,0}));return Quaternion3D{axis.x,axis.y,axis.z,0};}
        const Vec3 axis=Cross(from,to);return NormalizeQuaternion3D({axis.x,axis.y,axis.z,1.f+cosine});
    };
    const std::size_t upperParent=std::size_t(skeleton.joints[upper].parent);
    Vec3 upperSource=skeleton.joints[lower].bindTransform.translation;
    upperSource={upperSource.x*pose.localTransforms[upper].scale.x,upperSource.y*pose.localTransforms[upper].scale.y,upperSource.z*pose.localTransforms[upper].scale.z};
    const Vec3 desiredUpper=inverseVector(pose.globalMatrices[upperParent],solvedElbow-shoulder);
    pose.localTransforms[upper].quaternion=between(upperSource,desiredUpper);pose.localTransforms[upper].usesQuaternion=true;
    BuildAnimationPoseGlobalMatrices3D(skeleton,pose,root);
    const Vec3 solvedElbowActual=TransformPoint(pose.globalMatrices[lower],{});
    Vec3 lowerSource=skeleton.joints[supportHand].bindTransform.translation;
    lowerSource={lowerSource.x*pose.localTransforms[lower].scale.x,lowerSource.y*pose.localTransforms[lower].scale.y,lowerSource.z*pose.localTransforms[lower].scale.z};
    const Vec3 desiredLower=inverseVector(pose.globalMatrices[upper],solvedHand-solvedElbowActual);
    pose.localTransforms[lower].quaternion=between(lowerSource,desiredLower);pose.localTransforms[lower].usesQuaternion=true;
    BuildAnimationPoseGlobalMatrices3D(skeleton,pose,root);
}

void ApplyErgonomicPose(AnimationState3D state, float phase,
                        const Skeleton3D& skeleton, const HumanoidSkeletonMap3D& rig,
                        AnimationPose3D& pose, Matrix4 root) {
    if (!rig.Valid(skeleton, rig.compatibility) || pose.localTransforms.size() != skeleton.joints.size()) return;
    phase = Clamp(std::isfinite(phase) ? phase : 0.f, 0.f, 1.f);
    const auto joint = [&](HumanoidJoint3D semantic) -> JointTransform3D& {
        return pose.localTransforms[rig.Joint(semantic)];
    };
    const float cycle = phase * 2.f * Pi;
    const float wave = std::sin(cycle), crossWave = std::cos(cycle);

    if (state == AnimationState3D::Idle) {
        OffsetRotation(joint(HumanoidJoint3D::Hips), {0.f, .018f * wave, .012f * crossWave});
        OffsetRotation(joint(HumanoidJoint3D::Spine), {.018f * crossWave, -.026f * wave, -.008f * crossWave});
        OffsetRotation(joint(HumanoidJoint3D::LeftLowerArm), {.20f + .025f * wave, 0.f, -.035f});
        OffsetRotation(joint(HumanoidJoint3D::RightLowerArm), {.20f - .025f * wave, 0.f, .035f});
    } else if (state == AnimationState3D::Walk || state == AnimationState3D::Run) {
        const float run = state == AnimationState3D::Run ? 1.f : 0.f;
        OffsetRotation(joint(HumanoidJoint3D::Hips), {0.f, (.08f + .035f * run) * wave, (.022f + .012f * run) * crossWave});
        OffsetRotation(joint(HumanoidJoint3D::Spine), {.035f + .025f * run, (-.12f - .045f * run) * wave, (-.014f - .008f * run) * crossWave});
        OffsetRotation(joint(HumanoidJoint3D::LeftLowerArm), {.28f + .09f * crossWave, 0.f, -.045f});
        OffsetRotation(joint(HumanoidJoint3D::RightLowerArm), {.28f - .09f * crossWave, 0.f, .045f});
        // 膝與足掌由原製動畫片段和 FootPlacement3D 單獨負責，避免動畫後處理與貼地 IK 重複寫入。
    } else if (state == AnimationState3D::Crouch) {
        OffsetRotation(joint(HumanoidJoint3D::Hips), {.055f, .025f * wave, 0.f});
        OffsetRotation(joint(HumanoidJoint3D::Spine), {-.035f, -.04f * wave, 0.f});
        OffsetRotation(joint(HumanoidJoint3D::LeftLowerArm), {.34f, 0.f, -.04f});
        OffsetRotation(joint(HumanoidJoint3D::RightLowerArm), {.34f, 0.f, .04f});
    } else if (IsAttack(state)) {
        const float side = state == AnimationState3D::Attack2 ? -1.f : 1.f;
        const float anticipation = Pulse(phase, .18f, .18f);
        const float contact = Pulse(phase, .52f, .22f);
        const float followThrough = Pulse(phase, .78f, .22f);
        const float drive = -.42f * anticipation + contact + .55f * followThrough;
        OffsetRotation(joint(HumanoidJoint3D::Hips), {.025f * anticipation, side * .14f * drive, side * .025f * anticipation});
        OffsetRotation(joint(HumanoidJoint3D::Spine), {-.07f * contact, side * .24f * drive, -side * .045f * anticipation});
        // 三個攻擊階段皆維持雙肘彎曲，並讓雙手跟隨武器平面。
        OffsetRotation(joint(HumanoidJoint3D::LeftUpperArm), {-.34f - .28f * contact, -side * (.28f + .18f * drive), -.18f});
        OffsetRotation(joint(HumanoidJoint3D::RightUpperArm), {-.34f - .28f * contact, -side * (.28f + .18f * drive), .18f});
        OffsetRotation(joint(HumanoidJoint3D::LeftLowerArm), {.72f - .24f * contact, side * .10f * drive, -.08f});
        OffsetRotation(joint(HumanoidJoint3D::RightLowerArm), {.72f - .24f * contact, side * .10f * drive, .08f});
        OffsetRotation(joint(HumanoidJoint3D::LeftHand), {-.08f * anticipation, side * .08f * drive, -.04f});
        OffsetRotation(joint(HumanoidJoint3D::RightHand), {-.08f * anticipation, side * .08f * drive, .04f});
    }
    BuildAnimationPoseGlobalMatrices3D(skeleton, pose, root);
    if(IsAttack(state))SolveSupportHandGrip(skeleton,rig,pose,root);
}

Vec3 JointPosition(const AnimationPose3D& pose, const HumanoidSkeletonMap3D& rig,
                   HumanoidJoint3D semantic) noexcept {
    return TransformPoint(pose.globalMatrices[rig.Joint(semantic)], {});
}

float RotationDelta(const JointTransform3D& current, const JointTransform3D& previous) noexcept {
    if (current.usesQuaternion || previous.usesQuaternion) {
        const Quaternion3D a = current.usesQuaternion ? NormalizeQuaternion3D(current.quaternion) : QuaternionFromEuler3D(current.rotation);
        const Quaternion3D b = previous.usesQuaternion ? NormalizeQuaternion3D(previous.quaternion) : QuaternionFromEuler3D(previous.rotation);
        const float dot = std::abs(a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w);
        return 2.f * std::acos(Clamp(dot, 0.f, 1.f));
    }
    const auto wrapped = [](float value) { return std::remainder(value, 2.f * Pi); };
    const float x=wrapped(current.rotation.x-previous.rotation.x), y=wrapped(current.rotation.y-previous.rotation.y), z=wrapped(current.rotation.z-previous.rotation.z);
    return std::sqrt(x*x+y*y+z*z);
}

float HorizontalDistance(Vec3 left, Vec3 right) noexcept {
    const float x=left.x-right.x,z=left.z-right.z;return std::sqrt(x*x+z*z);
}
}

bool GameplayAnimation3D::Initialize(const SkinnedModelAssetData& asset,std::string* error){
    if(error)error->clear();HumanoidSkeletonMap3D rig;FallbackHumanoidAnimationSet3D clips;
    if(!ResolveCharacterRig3D(asset,rig)||!CreateFallbackHumanoidAnimationSet3D(asset,rig,clips,error))return false;
    const AnimationBlendSample1D3D samples[]={{0.f,&clips.clips[std::size_t(FallbackHumanoidClip3D::Idle)]},{1.5f,&clips.clips[std::size_t(FallbackHumanoidClip3D::Walk)]},{4.f,&clips.clips[std::size_t(FallbackHumanoidClip3D::Run)]}};
    const AnimationGraphState3D states[]={{"FallbackLocomotion",samples}};AnimationGraph3D graph;
    if(!graph.Initialize(asset.model.skeleton,asset.model.skeletonCompatibilityId,states)){if(error)*error="Fallback locomotion graph initialization failed";return false;}
    skeleton_=asset.model.skeleton;compatibility_=asset.model.skeletonCompatibilityId;rig_=rig;clips_=std::move(clips);graph_=std::move(graph);state_=AnimationState3D::Idle;time_=0;initialized_=true;return true;
}

const AnimationClip3D& GameplayAnimation3D::Clip(AnimationState3D state)const{return clips_.clips[std::size_t(ClipId(state))];}
void GameplayAnimation3D::SetState(AnimationState3D state,bool restart){if(state_!=state||restart){state_=state;time_=0;}}
void GameplayAnimation3D::Tick(float deltaTime){if(!initialized_)return;const float dt=Clamp(std::isfinite(deltaTime)?deltaTime:0.f,0.f,.1f);time_+=dt;if(state_==AnimationState3D::Idle||state_==AnimationState3D::Walk||state_==AnimationState3D::Run){const float speed=state_==AnimationState3D::Idle?0.f:state_==AnimationState3D::Walk?1.5f:4.f;graph_.SetParameter(speed);graph_.Update(dt);}}
void GameplayAnimation3D::SynchronizeNormalizedPhase(float phase){if(initialized_&&std::isfinite(phase))time_=Clamp(phase,0.f,1.f)*Duration();}
bool GameplayAnimation3D::EvaluateInto(AnimationPose3D& output,Matrix4 root){if(!initialized_)return false;bool evaluated=false;if(state_==AnimationState3D::Idle||state_==AnimationState3D::Walk||state_==AnimationState3D::Run)evaluated=graph_.EvaluateInto(skeleton_,compatibility_,output,root);else{SampleAnimationClipInto(skeleton_,Clip(state_),time_,output,root);evaluated=output.localTransforms.size()==skeleton_.joints.size()&&output.globalMatrices.size()==skeleton_.joints.size();}if(evaluated)ApplyErgonomicPose(state_,NormalizedPhase(),skeleton_,rig_,output,root);return evaluated;}
float GameplayAnimation3D::Duration()const{return initialized_?Clip(state_).duration:0.f;}
float GameplayAnimation3D::NormalizedPhase()const{const float duration=Duration();if(duration<=0)return 0;return Clip(state_).looping?std::fmod((std::max)(0.f,time_),duration)/duration:Clamp(time_/duration,0.f,1.f);}

GameplayErgonomicMotionMetrics3D MeasureGameplayErgonomicMotion3D(
    AnimationState3D state, const Skeleton3D& skeleton, const HumanoidSkeletonMap3D& rig,
    const AnimationPose3D& pose, const AnimationPose3D* previousPose, float deltaTime,
    float normalizedPhase, float previousNormalizedPhase) noexcept {
    GameplayErgonomicMotionMetrics3D metrics{};
    if (!rig.Valid(skeleton, rig.compatibility) || pose.localTransforms.size()!=skeleton.joints.size() ||
        pose.globalMatrices.size()!=skeleton.joints.size() || !std::isfinite(normalizedPhase)) return metrics;
    for (const JointTransform3D& transform : pose.localTransforms) {
        const float values[]={transform.translation.x,transform.translation.y,transform.translation.z,
            transform.rotation.x,transform.rotation.y,transform.rotation.z,transform.scale.x,transform.scale.y,
            transform.scale.z,transform.quaternion.x,transform.quaternion.y,transform.quaternion.z,transform.quaternion.w};
        if (std::any_of(std::begin(values),std::end(values),[](float value){return !std::isfinite(value);})) return metrics;
    }
    for (const Matrix4& matrix : pose.globalMatrices) for (const auto& row : matrix.values)
        for (float value : row) if (!std::isfinite(value)) return metrics;

    const Vec3 leftFoot=JointPosition(pose,rig,HumanoidJoint3D::LeftFoot),rightFoot=JointPosition(pose,rig,HumanoidJoint3D::RightFoot);
    const Vec3 hips=JointPosition(pose,rig,HumanoidJoint3D::Hips),head=JointPosition(pose,rig,HumanoidJoint3D::Head);
    const Vec3 feetMid=(leftFoot+rightFoot)*.5f;
    const float height=(std::max)(Length(head-feetMid),.001f);
    const Vec3 segment=rightFoot-leftFoot,toHips=hips-leftFoot;
    const float segmentLengthSquared=segment.x*segment.x+segment.z*segment.z;
    const float amount=segmentLengthSquared>.000001f?Clamp((toHips.x*segment.x+toHips.z*segment.z)/segmentLengthSquared,0.f,1.f):0.f;
    const Vec3 supportPoint=leftFoot+segment*amount;
    metrics.centerOfMassSupportError=(std::max)(0.f,HorizontalDistance(hips,supportPoint)-height*.10f)/height;

    for(std::size_t semantic=0;semantic<std::size_t(HumanoidJoint3D::Count);++semantic){
        const std::size_t index=rig.Joint(HumanoidJoint3D(semantic));
        metrics.maximumJointAngleRadians=(std::max)(metrics.maximumJointAngleRadians,
            RotationDelta(pose.localTransforms[index],skeleton.joints[index].bindTransform));
    }
    if(previousPose&&previousPose->localTransforms.size()==pose.localTransforms.size()&&previousPose->globalMatrices.size()==pose.globalMatrices.size()&&std::isfinite(deltaTime)&&deltaTime>0.f&&std::isfinite(previousNormalizedPhase)){
        const Vec3 previousLeft=JointPosition(*previousPose,rig,HumanoidJoint3D::LeftFoot),previousRight=JointPosition(*previousPose,rig,HumanoidJoint3D::RightFoot);
        metrics.maximumFootSlip=(std::max)(HorizontalDistance(leftFoot,previousLeft),HorizontalDistance(rightFoot,previousRight))/height;
        float largestTranslation{};
        for(std::size_t semantic=0;semantic<std::size_t(HumanoidJoint3D::Count);++semantic){const std::size_t index=rig.Joint(HumanoidJoint3D(semantic));const float angle=RotationDelta(pose.localTransforms[index],previousPose->localTransforms[index]);metrics.maximumJointAngularVelocity=(std::max)(metrics.maximumJointAngularVelocity,angle/deltaTime);largestTranslation=(std::max)(largestTranslation,Length(pose.localTransforms[index].translation-previousPose->localTransforms[index].translation)/height);}
        const bool wrapped=normalizedPhase<previousNormalizedPhase;
        metrics.phaseContinuityError=(std::max)(largestTranslation,metrics.maximumJointAngularVelocity*deltaTime/Pi);
        if(!wrapped&&std::abs(normalizedPhase-previousNormalizedPhase)>.25f)metrics.phaseContinuityError=(std::max)(metrics.phaseContinuityError,std::abs(normalizedPhase-previousNormalizedPhase));
    }
    const Vec3 leftHand=JointPosition(pose,rig,HumanoidJoint3D::LeftHand),rightHand=JointPosition(pose,rig,HumanoidJoint3D::RightHand),spine=JointPosition(pose,rig,HumanoidJoint3D::Spine);
    if(IsAttack(state)){
        const Vec3 weaponEnd=TransformPoint(pose.globalMatrices[rig.Joint(HumanoidJoint3D::RightHand)],{0,-.72f,0});
        const Vec3 shaft=weaponEnd-rightHand,toSupport=leftHand-rightHand;
        const float shaftLengthSquared=shaft.x*shaft.x+shaft.y*shaft.y+shaft.z*shaft.z;
        const float gripAmount=shaftLengthSquared>.000001f?Clamp((toSupport.x*shaft.x+toSupport.y*shaft.y+toSupport.z*shaft.z)/shaftLengthSquared,0.f,1.f):0.f;
        metrics.weaponGripError=Length(leftHand-(rightHand+shaft*gripAmount))/height;
    }
    const float torsoRadius=height*.105f;
    metrics.penetrationDepth=(std::max)({0.f,torsoRadius-Length(leftHand-spine),torsoRadius-Length(rightHand-spine)})/height;
    metrics.finite=true;
    return metrics;
}
}
