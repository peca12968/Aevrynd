#pragma once
#include "GameplayAnimation3D.h"
#include <Aevrynd/Camera.h>

namespace GameplayLab {
enum class GameplayFirstPersonPoseStatus3D {InvalidInput,InvalidRig,Unreachable,SolverFailure,Ready};
struct GameplayFirstPersonPoseInput3D {
    const Aevrynd::SkinnedModel3D* character{};
    const Aevrynd::HumanoidSkeletonMap3D* rig{};
    const Aevrynd::AnimationPose3D* sourcePose{};
    const Aevrynd::CharacterSwordGeometry3D* sword{};
    Aevrynd::Camera3D camera{};
    AnimationState3D state{AnimationState3D::Idle};
    float phase{},enterBlend{1.f};
    // 相機座標公尺；正式遊玩與擷取共用，不依呈現幀率累積。
    Aevrynd::Vec3 shoulderCenter{.12f,-.26f,.14f};
    Aevrynd::Vec3 readyGrip{.31f,-.055f,.47f};
};
struct GameplayFirstPersonPoseOutput3D {
    Aevrynd::AnimationPose3D pose{};
    Aevrynd::Matrix4 modelToWorld{};
    std::array<Aevrynd::Vec3,2> shouldersCamera{},wristsCamera{};
    Aevrynd::Vec3 targetGripCamera{},actualGripCamera{};
    float maximumBoneLengthError{},gripError{};
    bool valid{},palmContactVerified{};
};
struct GameplayFirstPersonPoseWorkspace3D {GameplayFirstPersonPoseOutput3D scratch{};};
// 只在模擬更新求值；render 重用 output。失敗不更改 output，不修改 sourcePose。
GameplayFirstPersonPoseStatus3D EvaluateGameplayFirstPersonPose3D(
    const GameplayFirstPersonPoseInput3D& input,GameplayFirstPersonPoseWorkspace3D& workspace,
    GameplayFirstPersonPoseOutput3D& output);
}
