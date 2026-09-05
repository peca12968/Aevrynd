#pragma once
#include <Aevrynd/AnimationGraph3D.h>
#include <Aevrynd/CharacterPresentation3D.h>
#include <cstdint>

namespace GameplayLab {
float GameplayPresentationDelta3D(float simulationDelta) noexcept;
enum class AnimationState3D : std::uint8_t {Idle,Walk,Run,Crouch,Jump,Fall,Attack1,Attack2,Attack3,Hit,Death};

// 所有距離皆以實測角色身高正規化；收據數值取自正式姿勢的實際求值。
// 姿勢無法取得或格式錯誤時，透過 finite=false 封閉失敗，避免誤報為零誤差動作。
struct GameplayErgonomicMotionMetrics3D {
    float maximumFootSlip{};
    float centerOfMassSupportError{};
    float maximumJointAngleRadians{};
    float maximumJointAngularVelocity{};
    float weaponGripError{};
    float penetrationDepth{};
    float phaseContinuityError{};
    bool finite{};
};

GameplayErgonomicMotionMetrics3D MeasureGameplayErgonomicMotion3D(
    AnimationState3D state,
    const Aevrynd::Skeleton3D& skeleton,
    const Aevrynd::HumanoidSkeletonMap3D& rig,
    const Aevrynd::AnimationPose3D& pose,
    const Aevrynd::AnimationPose3D* previousPose = nullptr,
    float deltaTime = 0.f,
    float normalizedPhase = 0.f,
    float previousNormalizedPhase = 0.f) noexcept;

class GameplayAnimation3D {
public:
    bool Initialize(const Aevrynd::SkinnedModelAssetData& asset,std::string* error=nullptr);
    void SetState(AnimationState3D state,bool restart=false);
    void Tick(float deltaTime);
    void SynchronizeNormalizedPhase(float phase);
    [[nodiscard]] AnimationState3D State()const{return state_;}
    bool EvaluateInto(Aevrynd::AnimationPose3D& output,Aevrynd::Matrix4 root=Aevrynd::Matrix4::Identity());
    [[nodiscard]] float NormalizedPhase()const;
    [[nodiscard]] float Duration()const;
    [[nodiscard]] const Aevrynd::HumanoidSkeletonMap3D& Rig()const{return rig_;}
    [[nodiscard]] Aevrynd::ReusableMemoryStats MemoryStats()const{return graph_.MemoryStats();}
    std::size_t TrimUnusedMemory(){return graph_.TrimUnusedMemory();}
private:
    const Aevrynd::AnimationClip3D& Clip(AnimationState3D state)const;
    Aevrynd::Skeleton3D skeleton_{};Aevrynd::SkeletonCompatibilityId3D compatibility_{};Aevrynd::HumanoidSkeletonMap3D rig_{};Aevrynd::FallbackHumanoidAnimationSet3D clips_{};Aevrynd::AnimationGraph3D graph_{};AnimationState3D state_{AnimationState3D::Idle};float time_{};bool initialized_{};
};
}
