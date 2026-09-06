#include "GameplayAnimation3D.h"
#include "GameplayCombat3D.h"

#include <Aevrynd/CharacterPresentation3D.h>
#include <Aevrynd/FootPlacement3D.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <set>

using namespace Aevrynd;
using namespace GameplayLab;

namespace {
std::uint64_t HashPose(const AnimationPose3D& pose) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const Matrix4& matrix : pose.globalMatrices) for (const auto& row : matrix.values)
        for (float value : row) {
            const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
            for (std::size_t index = 0; index < sizeof(value); ++index) {
                hash ^= bytes[index];
                hash *= 1099511628211ull;
            }
        }
    return hash;
}

AnimationState3D StateForAttack(Attack3D attack) {
    return attack == Attack3D::Attack1 ? AnimationState3D::Attack1
        : attack == Attack3D::Attack2 ? AnimationState3D::Attack2
        : AnimationState3D::Attack3;
}

Vec3 JointPoint(const AnimationPose3D& pose, const HumanoidSkeletonMap3D& rig,
                HumanoidJoint3D joint) {
    return TransformPoint(pose.globalMatrices[rig.Joint(joint)], {});
}
}

int main(int argumentCount,char** arguments) {
    const auto source = std::filesystem::path{AEVRYND_SOURCE_DIR};
    const auto assetPath=argumentCount>1?std::filesystem::path{arguments[1]}:
        source / "games/character_lab/assets/model-packs/realistic-female/realistic-female.aeskin";
    const auto loaded = LoadSkinnedModelAssetFileDetailed(assetPath);
    GameplayAnimation3D animation;
    std::string error;
    if (!loaded || !animation.Initialize(loaded.asset, &error)) {
        std::cerr << "Gameplay F2 animation initialization failed: " << error << '\n';
        return 1;
    }
    std::set<std::uint64_t> poseHashes;
    AnimationPose3D pose;
    const std::size_t productionJointCount = loaded.asset.model.skeleton.joints.size();
    for (int value = 0; value < 11; ++value) {
        animation.SetState(AnimationState3D(value), true);
        animation.Tick(animation.Duration() * .47f);
        if (!animation.EvaluateInto(pose) ||
            pose.localTransforms.size() != productionJointCount ||
            pose.globalMatrices.size() != productionJointCount) {
            std::cerr << "Gameplay animation pose contract failed state=" << value
                      << " skeletonJoints=" << productionJointCount
                      << " local=" << pose.localTransforms.size()
                      << " global=" << pose.globalMatrices.size() << '\n';
            return 2;
        }
        poseHashes.insert(HashPose(pose));
    }
    if (poseHashes.size() != 11) {
        std::cerr << "Gameplay animation states were not distinct\n";
        return 3;
    }
    AnimationPose3D missingSocketPose = pose;
    missingSocketPose.globalMatrices.clear();
    Matrix4 unchangedSocket = CreateScaleMatrix({3.f, 4.f, 5.f});
    const Matrix4 socketSentinel = unchangedSocket;
    if (CharacterSocketTransform3D(loaded.asset.model.skeleton, animation.Rig(),
                                   HumanoidJoint3D::RightHand,
                                   missingSocketPose, Matrix4::Identity(),
                                   unchangedSocket) ||
        std::memcmp(&unchangedSocket, &socketSentinel, sizeof(Matrix4)) != 0) {
        std::cerr << "Gameplay animation accepted missing socket pose\n";
        return 12;
    }
    if (std::abs(GameplayPresentationDelta3D(0.f)-1.f/60.f)>1e-6f ||
        std::abs(GameplayPresentationDelta3D(std::numeric_limits<float>::infinity())-1.f/60.f)>1e-6f ||
        std::abs(GameplayPresentationDelta3D(5000.f)-.1f)>1e-6f) return 7;
    FootPlacement3D noGroundPlacement;
    CollisionWorld3D noGroundWorld;
    if (!noGroundPlacement.Prepare(loaded.asset.model.skeleton,animation.Rig(),loaded.asset.model.skeletonCompatibilityId)) return 8;
    animation.SetState(AnimationState3D::Idle,true);
    animation.Tick(1.f/60.f);
    if (!animation.EvaluateInto(pose)) return 9;
    FootPlacementFrame3D noGroundFrame{};
    noGroundFrame.collisionWorld=&noGroundWorld;
    noGroundFrame.deltaTime=GameplayPresentationDelta3D(5000.f);
    noGroundFrame.cameraDistance=1.f;
    FootPlacementResult3D noGroundResult{};
    if (!noGroundPlacement.Evaluate(loaded.asset.model.skeleton,animation.Rig(),noGroundFrame,pose,&noGroundResult) || noGroundResult.applied) return 10;

    // 非 restart 的 locomotion 切換必須保留腿、軀幹與擺臂共用相位；明確 restart 才從接觸相位開始。
    animation.SetState(AnimationState3D::Idle,true);
    animation.Tick(animation.Duration()*.37f);
    const float idlePhase=animation.NormalizedPhase();
    if(!animation.EvaluateInto(pose))return 18;
    const std::uint64_t idlePoseHash=HashPose(pose);
    animation.SetState(AnimationState3D::Walk);
    if(std::abs(animation.NormalizedPhase()-idlePhase)>1e-6f||!animation.EvaluateInto(pose)||HashPose(pose)==idlePoseHash){
        std::cerr<<"Idle-to-walk gait phase split phase="<<animation.NormalizedPhase()<<" expected="<<idlePhase<<'\n';
        return 18;
    }
    if(argumentCount>1&&!animation.HasDetailedGripPose()){
        std::cerr<<"Detailed grip retrofit was not activated\n";
        return 21;
    }
    animation.Tick(animation.Duration()*.11f);
    if(!animation.EvaluateInto(pose))return 19;
    const float walkPhase=animation.NormalizedPhase();
    const std::uint64_t walkPoseHash=HashPose(pose);
    animation.SetState(AnimationState3D::Run);
    if(std::abs(animation.NormalizedPhase()-walkPhase)>1e-6f||!animation.EvaluateInto(pose)||HashPose(pose)==walkPoseHash){
        std::cerr<<"Walk-to-run gait phase split phase="<<animation.NormalizedPhase()<<" expected="<<walkPhase<<'\n';
        return 19;
    }
    animation.SetState(AnimationState3D::Idle,true);
    if(std::abs(animation.NormalizedPhase())>1e-6f)return 20;

    GameplayErgonomicMotionMetrics3D measuredMaximum{};
    const std::array<AnimationState3D,7> ergonomicStates{
        AnimationState3D::Idle,AnimationState3D::Walk,AnimationState3D::Run,
        AnimationState3D::Crouch,AnimationState3D::Attack1,
        AnimationState3D::Attack2,AnimationState3D::Attack3};
    for (AnimationState3D state : ergonomicStates) {
        animation.SetState(state,true);
        AnimationPose3D previous;
        float previousPhase{};
        for (int frame=0;frame<=72;++frame) {
            const float dt=animation.Duration()/72.f;
            animation.Tick(frame?dt:0.f);
            if(!animation.EvaluateInto(pose)) return 13;
            const float phase=animation.NormalizedPhase();
            const auto metrics=MeasureGameplayErgonomicMotion3D(
                state,loaded.asset.model.skeleton,animation.Rig(),pose,
                frame?&previous:nullptr,dt,phase,previousPhase);
            if(!metrics.finite) {
                std::cerr<<"Ergonomic receipt rejected production pose state="<<int(state)
                         <<" frame="<<frame<<'\n';
                return 14;
            }
            measuredMaximum.maximumFootSlip=(std::max)(measuredMaximum.maximumFootSlip,metrics.maximumFootSlip);
            measuredMaximum.centerOfMassSupportError=(std::max)(measuredMaximum.centerOfMassSupportError,metrics.centerOfMassSupportError);
            measuredMaximum.maximumJointAngleRadians=(std::max)(measuredMaximum.maximumJointAngleRadians,metrics.maximumJointAngleRadians);
            measuredMaximum.maximumJointAngularVelocity=(std::max)(measuredMaximum.maximumJointAngularVelocity,metrics.maximumJointAngularVelocity);
            measuredMaximum.weaponGripError=(std::max)(measuredMaximum.weaponGripError,metrics.weaponGripError);
            measuredMaximum.penetrationDepth=(std::max)(measuredMaximum.penetrationDepth,metrics.penetrationDepth);
            measuredMaximum.phaseContinuityError=(std::max)(measuredMaximum.phaseContinuityError,metrics.phaseContinuityError);
            previous=pose;previousPhase=phase;
        }
    }

    animation.SetState(AnimationState3D::Attack1,true);
    animation.Tick(animation.Duration()*.52f);
    if(!animation.EvaluateInto(pose)) return 15;
    const auto productionAttack=MeasureGameplayErgonomicMotion3D(
        AnimationState3D::Attack1,loaded.asset.model.skeleton,animation.Rig(),pose);
    const auto& skeleton=loaded.asset.model.skeleton;
    const auto& rig=animation.Rig();
    const float characterHeight=(std::max)(Length(JointPoint(pose,rig,HumanoidJoint3D::Head)-
        (JointPoint(pose,rig,HumanoidJoint3D::LeftFoot)+JointPoint(pose,rig,HumanoidJoint3D::RightFoot))*.5f),.001f);
    if(measuredMaximum.maximumFootSlip>.04f||measuredMaximum.centerOfMassSupportError>.04f||
       measuredMaximum.maximumJointAngleRadians>2.7f||measuredMaximum.maximumJointAngularVelocity>75.f||
       measuredMaximum.weaponGripError>.12f||measuredMaximum.penetrationDepth>.02f||
       measuredMaximum.phaseContinuityError>.22f){
        std::cerr<<"Ergonomic production gate failed"
                 <<" foot="<<measuredMaximum.maximumFootSlip
                 <<" support="<<measuredMaximum.centerOfMassSupportError
                 <<" angle="<<measuredMaximum.maximumJointAngleRadians
                 <<" angularVelocity="<<measuredMaximum.maximumJointAngularVelocity
                 <<" grip="<<measuredMaximum.weaponGripError
                 <<" penetration="<<measuredMaximum.penetrationDepth
                 <<" continuity="<<measuredMaximum.phaseContinuityError<<'\n';
        return 17;
    }
    AnimationPose3D hostile=pose;
    hostile.localTransforms[rig.Joint(HumanoidJoint3D::LeftFoot)].translation.x+=characterHeight;
    BuildAnimationPoseGlobalMatrices3D(skeleton,hostile);
    const auto footSlipHostile=MeasureGameplayErgonomicMotion3D(
        AnimationState3D::Attack1,skeleton,rig,hostile,&pose,.01f,.53f,.52f);
    hostile=pose;hostile.globalMatrices[rig.Joint(HumanoidJoint3D::Hips)]=
        CreateTranslationMatrix({characterHeight,0,0})*hostile.globalMatrices[rig.Joint(HumanoidJoint3D::Hips)];
    const auto supportHostile=MeasureGameplayErgonomicMotion3D(AnimationState3D::Attack1,skeleton,rig,hostile);
    hostile=pose;hostile.localTransforms[rig.Joint(HumanoidJoint3D::LeftLowerArm)].usesQuaternion=false;
    hostile.localTransforms[rig.Joint(HumanoidJoint3D::LeftLowerArm)].rotation={3.f,0,0};
    BuildAnimationPoseGlobalMatrices3D(skeleton,hostile);
    const auto angleHostile=MeasureGameplayErgonomicMotion3D(
        AnimationState3D::Attack1,skeleton,rig,hostile,&pose,.01f,.53f,.52f);
    hostile=pose;hostile.localTransforms[rig.Joint(HumanoidJoint3D::LeftHand)].translation.x+=characterHeight;
    BuildAnimationPoseGlobalMatrices3D(skeleton,hostile);
    const auto gripHostile=MeasureGameplayErgonomicMotion3D(AnimationState3D::Attack1,skeleton,rig,hostile);
    hostile=pose;hostile.globalMatrices[rig.Joint(HumanoidJoint3D::LeftHand)]=hostile.globalMatrices[rig.Joint(HumanoidJoint3D::Spine)];
    const auto penetrationHostile=MeasureGameplayErgonomicMotion3D(AnimationState3D::Attack1,skeleton,rig,hostile);
    hostile=pose;hostile.localTransforms[rig.Joint(HumanoidJoint3D::RightUpperArm)].translation.z+=characterHeight;
    BuildAnimationPoseGlobalMatrices3D(skeleton,hostile);
    const auto continuityHostile=MeasureGameplayErgonomicMotion3D(
        AnimationState3D::Attack1,skeleton,rig,hostile,&pose,.01f,.90f,.52f);
    hostile=pose;hostile.localTransforms[0].translation.x=(std::numeric_limits<float>::quiet_NaN)();
    const auto nonFiniteHostile=MeasureGameplayErgonomicMotion3D(AnimationState3D::Attack1,skeleton,rig,hostile);
    if(footSlipHostile.maximumFootSlip<.5f||supportHostile.centerOfMassSupportError<.4f||
       angleHostile.maximumJointAngleRadians<2.5f||angleHostile.maximumJointAngularVelocity<100.f||
       gripHostile.weaponGripError<.4f||penetrationHostile.penetrationDepth<.09f||
       continuityHostile.phaseContinuityError<.4f||nonFiniteHostile.finite||!productionAttack.finite){
        std::cerr<<"Ergonomic hostile gate failed"
                 <<" foot="<<footSlipHostile.maximumFootSlip
                 <<" support="<<supportHostile.centerOfMassSupportError
                 <<" angle="<<angleHostile.maximumJointAngleRadians
                 <<" angularVelocity="<<angleHostile.maximumJointAngularVelocity
                 <<" grip="<<gripHostile.weaponGripError
                 <<" penetration="<<penetrationHostile.penetrationDepth
                 <<" continuity="<<continuityHostile.phaseContinuityError
                 <<" finite="<<nonFiniteHostile.finite<<'\n';
        return 16;
    }

    CollisionWorld3D collision;
    PhysicsWorld3D physics;
    GameplayCombat3D combat(collision, physics);
    combat.SpawnMonster({0, 0, 1.8f}, 100);
    CombatInput3D input{{0, 0, 0}, {0, 0, 1}, true};
    std::array<std::uint64_t, 3> trajectories{};
    float maximumPhaseError{};
    for (int attackIndex = 0; attackIndex < 3; ++attackIndex) {
        combat.Tick(input, .01f);
        input.attackPressed = false;
        animation.SetState(StateForAttack(combat.CurrentAttack()), true);
        std::uint64_t trajectory = 14695981039346656037ull;
        for (int frame = 0; frame < 54; ++frame) {
            combat.Tick(input, .01f);
            animation.Tick(.01f);
            animation.SynchronizeNormalizedPhase(combat.NormalizedAttackPhase());
            if (!animation.EvaluateInto(pose)) return 4;
            Matrix4 socket{};
            if (!CharacterSocketTransform3D(loaded.asset.model.skeleton, animation.Rig(),
                                            HumanoidJoint3D::RightHand, pose,
                                            Matrix4::Identity(), socket)) return 5;
            const Vec3 tip = TransformPoint(socket, {0, -.72f, 0});
            for (float value : {tip.x, tip.y, tip.z}) {
                const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
                for (std::size_t index = 0; index < sizeof(value); ++index) {
                    trajectory ^= bytes[index];
                    trajectory *= 1099511628211ull;
                }
            }
            maximumPhaseError = (std::max)(maximumPhaseError,
                std::abs(animation.NormalizedPhase() - combat.NormalizedAttackPhase()));
        }
        trajectories[std::size_t(attackIndex)] = trajectory;
        input.attackPressed = true;
    }
    if (combat.Monster().alive || combat.Stats().hits != 3 || combat.Stats().kills != 1 ||
        collision.Stats().colliders || physics.Stats().bodies ||
        trajectories[0] == trajectories[1] || trajectories[1] == trajectories[2] ||
        trajectories[0] == trajectories[2] || maximumPhaseError > .025f) {
        std::cerr << "Combat/animation/weapon synchronization gate failed phaseError="
                  << maximumPhaseError << '\n';
        return 6;
    }
    CollisionWorld3D reachCollision;PhysicsWorld3D reachPhysics;GameplayCombat3D reachCombat(reachCollision,reachPhysics);
    reachCombat.SpawnMonster({GameplayMonsterStopDistance3D,0,0},100);
    const auto obstacle=reachCollision.CreateBox({{{0,1,.4f},{.2f,.8f,.2f}},2,1,99,false});
    reachCombat.Tick({{}, {1,0,0}, true},.1f);reachCombat.Tick({{}, {1,0,0}, false},.02f);
    if(!obstacle||reachCombat.Stats().hits!=1||std::abs(reachCombat.Monster().health-70.f)>.001f||Length(reachCombat.Monster().position-Vec3{GameplayMonsterStopDistance3D,0,0})>.001f||!reachCollision.Get(obstacle))return 11;
    std::cout << "combatHits=3 kills=1 animationStates=11 joints="
              << productionJointCount << " trajectories=3 phaseError="
              << maximumPhaseError
              << " ergonomicFootSlip="<<measuredMaximum.maximumFootSlip
              << " ergonomicSupport="<<measuredMaximum.centerOfMassSupportError
              << " ergonomicAngle="<<measuredMaximum.maximumJointAngleRadians
              << " ergonomicAngularVelocity="<<measuredMaximum.maximumJointAngularVelocity
              << " ergonomicGrip="<<measuredMaximum.weaponGripError
              << " ergonomicPenetration="<<measuredMaximum.penetrationDepth
              << " ergonomicContinuity="<<measuredMaximum.phaseContinuityError<<'\n';
}
