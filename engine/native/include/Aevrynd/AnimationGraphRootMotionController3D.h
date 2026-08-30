#pragma once
#include "AnimationGraph3D.h"
#include "CharacterController3D.h"

namespace Aevrynd {
// Graph root-motion、in-place pose 與角色控制器的單 tick committed receipt。
struct AnimationGraphRootMotionControllerResult3D {
    AnimationGraphRootMotionResult3D graph{};
    CharacterDirectDisplacementResult3D controller{};
    Vec3 translationModel{},requestedWorld{},consumedWorld{},remainderWorld{},groundCorrectionWorld{};
    float startingYawRadians{},requestedYawRadians{},consumedYawRadians{},remainderYawRadians{};
    std::uint64_t tick{},deterministicHash{};
};

// 僅限建立它的 coordinator、Graph、Controller 與 pose；不可序列化。
class AnimationGraphRootMotionControllerTransaction3D {
public:
    AnimationGraphRootMotionControllerTransaction3D()=default;
private:
    friend class AnimationGraphRootMotionController3D;
    friend struct AnimationGraphRootMotionController3DTestAccess;
    std::uintptr_t owner{};
    AnimationGraph3D* graph{};
    CharacterController3D* controller{};
    const AnimationPose3D* pose{};
    AnimationGraphRootMotionPreviewToken3D graphToken{};
    CharacterDirectDisplacementPreviewToken3D controllerToken{};
    AnimationGraphRootMotionControllerResult3D preview{};
    std::uint64_t generation{},serial{},sealedHash{};
    bool valid{};
};

enum class AnimationGraphRootMotionControllerFailureCheckpoint3D:std::uint8_t {
    None,AfterGraphPreview,AfterControllerPreview,BeforeCommitValidation,AfterDualCanCommit
};
struct AnimationGraphRootMotionController3DTestAccess;

// Single-thread、non-reentrant；每個 instance 同時只允許一筆 outstanding transaction。
class AnimationGraphRootMotionController3D {
public:
    // 四種 Preview 只建立 pending workspace；false 時兩模組、pose、transaction 與 output 均不提交。
    bool PreviewTick(AnimationGraph3D&,CharacterController3D&,const Skeleton3D&,SkeletonCompatibilityId3D,AnimationGraphRootMotionTick3D,const AnimationPose3D&,std::span<const BoxCollider3D>,AnimationGraphRootMotionControllerTransaction3D&,AnimationGraphRootMotionControllerResult3D* preview=nullptr)noexcept;
    bool PreviewTick(AnimationGraph3D&,CharacterController3D&,const Skeleton3D&,SkeletonCompatibilityId3D,AnimationGraphRootMotionTick3D,const AnimationPose3D&,const CollisionWorld3D&,CollisionQueryFilter3D,AnimationGraphRootMotionControllerTransaction3D&,AnimationGraphRootMotionControllerResult3D* preview=nullptr)noexcept;
    bool PreviewTick(AnimationGraph3D&,CharacterController3D&,const Skeleton3D&,SkeletonCompatibilityId3D,AnimationGraphRootMotionTick3D,const AnimationPose3D&,std::span<const BoxCollider3D>,const HeightField3D&,AnimationGraphRootMotionControllerTransaction3D&,AnimationGraphRootMotionControllerResult3D* preview=nullptr)noexcept;
    bool PreviewTick(AnimationGraph3D&,CharacterController3D&,const Skeleton3D&,SkeletonCompatibilityId3D,AnimationGraphRootMotionTick3D,const AnimationPose3D&,const CollisionWorld3D&,const HeightField3D&,CollisionQueryFilter3D,AnimationGraphRootMotionControllerTransaction3D&,AnimationGraphRootMotionControllerResult3D* preview=nullptr)noexcept;
    // Commit 先完成雙 CanCommit；其後只呼叫兩個 noexcept/zero-allocation unchecked commit。
    bool CanCommit(const AnimationGraphRootMotionControllerTransaction3D&,const AnimationPose3D&)const noexcept;
    bool Commit(AnimationGraphRootMotionControllerTransaction3D&,AnimationPose3D&,AnimationGraphRootMotionControllerResult3D* output=nullptr)noexcept;
    bool Cancel(AnimationGraphRootMotionControllerTransaction3D&)noexcept;
    // Reset/Teleport 先完整預驗，再同步重設兩個 timeline；下一 tick 只建立零位移 anchor。
    bool Reset(AnimationGraph3D&,CharacterController3D&,Vec3 feetPosition={},float yawRadians=0)noexcept;
    bool Teleport(AnimationGraph3D&,CharacterController3D&,Vec3 feetPosition,float yawRadians,std::uint64_t previousTick)noexcept;
private:
    friend struct AnimationGraphRootMotionController3DTestAccess;
    bool Begin(AnimationGraph3D&,CharacterController3D&,const Skeleton3D&,SkeletonCompatibilityId3D,AnimationGraphRootMotionTick3D,const AnimationPose3D&,AnimationGraphRootMotionControllerTransaction3D&,AnimationGraphRootMotionResult3D&,float&)noexcept;
    bool Finish(AnimationGraphRootMotionControllerTransaction3D&,const AnimationGraphRootMotionResult3D&,const CharacterDirectDisplacementResult3D&,float,AnimationGraphRootMotionControllerResult3D*)noexcept;
    bool ConsumeFailure(AnimationGraphRootMotionControllerFailureCheckpoint3D)noexcept;
    void SetFailureCheckpointForTesting(AnimationGraphRootMotionControllerFailureCheckpoint3D value)noexcept{failureCheckpoint_=value;}
    [[nodiscard]] std::uint64_t Seal(const AnimationGraphRootMotionControllerTransaction3D&)const noexcept;
    bool pending_{};
    std::uint64_t generation_{1},serial_{};
    AnimationGraphRootMotionControllerFailureCheckpoint3D failureCheckpoint_{};
    AnimationGraph3D* pendingGraph_{};
    CharacterController3D* pendingController_{};
    const AnimationPose3D* pendingPose_{};
    AnimationGraphRootMotionPreviewToken3D pendingGraphToken_{};
    CharacterDirectDisplacementPreviewToken3D pendingControllerToken_{};
    AnimationGraphRootMotionControllerResult3D pendingResult_{};
    std::uint64_t pendingSeal_{};
};
}
