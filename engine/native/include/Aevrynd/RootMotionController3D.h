#pragma once
#include "RootMotion3D.h"
#include "CharacterController3D.h"

namespace Aevrynd {
// 跨 RootMotion 與 CharacterController 的單 tick committed receipt。
struct RootMotionControllerResult3D {
    // 兩模組各自的 production receipt；只在跨模組 commit 完成後對外發布。
    RootMotionResult3D rootMotion{};
    CharacterDirectDisplacementResult3D controller{};
    // translationModel 位於段起始 model space；其餘向量皆為 world space。
    Vec3 translationModel{},requestedWorld{},consumedWorld{},remainderWorld{},groundCorrectionWorld{};
    // startingYaw 是 model-to-world 轉換擁有者；requested/consumed/remainder 均為本 tick yaw。
    float startingYawRadians{},requestedYawRadians{},consumedYawRadians{},remainderYawRadians{};
    // tick 是共同固定模擬 tick；deterministicHash 涵蓋此 committed receipt。
    std::uint64_t tick{},deterministicHash{};
};

// transaction 僅限建立它的 coordinator/owner/pose；不可序列化或跨 frame 儲存。
class RootMotionControllerTransaction3D {
public:
    RootMotionControllerTransaction3D()=default;
    RootMotionControllerTransaction3D(const RootMotionControllerTransaction3D&)=default;
    RootMotionControllerTransaction3D& operator=(const RootMotionControllerTransaction3D&)=default;
private:
    friend class RootMotionController3D;
    // owner/module/pose 綁定唯一 coordinator、兩個 production module 與原始 pose 物件。
    std::uintptr_t owner{};
    RootMotion3D* rootMotion{};
    CharacterController3D* controller{};
    const AnimationPose3D* pose{};
    // 子權杖不可由 caller 修改或分開提交。
    RootMotionPreviewToken3D rootToken{};
    CharacterDirectDisplacementPreviewToken3D controllerToken{};
    // preview 是未提交 receipt；generation/serial 防止 stale、replay 與 double commit。
    RootMotionControllerResult3D preview{};
    std::uint64_t generation{},serial{};
    // sealedHash 綁定完整 receipt、owner、generation 與兩個子權杖；任何 byte tamper 皆須 fail-closed。
    std::uint64_t sealedHash{};
    // valid 仍須通過 coordinator CanCommit；transaction 不可序列化或跨 frame 保存。
    bool valid{};
};

// 僅供自動測試的 one-shot failure injection；production 預設永遠為 None。
enum class RootMotionControllerFailureCheckpoint3D:std::uint8_t {None,AfterRootPreview,AfterControllerPreview,BeforeCommitValidation};
struct RootMotionController3DTestAccess;

// 此 coordinator 是 single-thread、non-reentrant，且每個 instance 同時只允許一筆 outstanding transaction。
class RootMotionController3D {
public:
    // 四種 Preview overload 皆只建立 pending transaction；false 時 pose、output 與兩模組 committed state 原子不變。
    bool PreviewTick(RootMotion3D&,CharacterController3D&,const Skeleton3D&,RootMotionTick3D,const AnimationPose3D&,std::span<const BoxCollider3D>,RootMotionControllerTransaction3D&,RootMotionControllerResult3D* preview=nullptr)noexcept;
    bool PreviewTick(RootMotion3D&,CharacterController3D&,const Skeleton3D&,RootMotionTick3D,const AnimationPose3D&,const CollisionWorld3D&,CollisionQueryFilter3D,RootMotionControllerTransaction3D&,RootMotionControllerResult3D* preview=nullptr)noexcept;
    bool PreviewTick(RootMotion3D&,CharacterController3D&,const Skeleton3D&,RootMotionTick3D,const AnimationPose3D&,std::span<const BoxCollider3D>,const HeightField3D&,RootMotionControllerTransaction3D&,RootMotionControllerResult3D* preview=nullptr)noexcept;
    bool PreviewTick(RootMotion3D&,CharacterController3D&,const Skeleton3D&,RootMotionTick3D,const AnimationPose3D&,const CollisionWorld3D&,const HeightField3D&,CollisionQueryFilter3D,RootMotionControllerTransaction3D&,RootMotionControllerResult3D* preview=nullptr)noexcept;
    // CanCommit 無副作用；Commit 先完成雙重驗證，再以無失敗 unchecked commit 同時發布兩模組結果。
    bool CanCommit(const RootMotionControllerTransaction3D&,const AnimationPose3D&)const noexcept;
    bool Commit(RootMotionControllerTransaction3D&,AnimationPose3D&,RootMotionControllerResult3D* output=nullptr)noexcept;
    bool Cancel(RootMotionControllerTransaction3D&)noexcept;
    // Reset 令下一共同 tick 從 1 建立 anchor；Teleport 令下一共同 tick 為 previousTick+1 且零位移。
    bool Reset(RootMotion3D&,CharacterController3D&,Vec3 feetPosition={},float yawRadians=0)noexcept;
    bool Teleport(RootMotion3D&,CharacterController3D&,Vec3 feetPosition,float yawRadians,std::uint64_t previousTick)noexcept;
    // 原子替換 clip 並沿用 shared previous tick；下一 tick 只建立零位移 anchor。
    bool SwitchClip(RootMotion3D&,CharacterController3D&,const Skeleton3D&,const HumanoidSkeletonMap3D&,SkeletonCompatibilityId3D,const AnimationClip3D&,RootMotionSettings3D={});
private:
    friend struct RootMotionController3DTestAccess;
    bool Begin(RootMotion3D&,CharacterController3D&,const Skeleton3D&,RootMotionTick3D,const AnimationPose3D&,RootMotionControllerTransaction3D&,RootMotionResult3D&,float&)noexcept;
    bool Finish(RootMotionControllerTransaction3D&,const RootMotionResult3D&,const CharacterDirectDisplacementResult3D&,float,RootMotionControllerResult3D*)noexcept;
    bool ConsumeFailure(RootMotionControllerFailureCheckpoint3D)noexcept;
    // 固定 private seam 保持所有 TU 類別定義一致；僅 friend 測試 adapter 可呼叫。
    void SetFailureCheckpointForTesting(RootMotionControllerFailureCheckpoint3D checkpoint)noexcept{failureCheckpoint_=checkpoint;}
    [[nodiscard]] std::uint64_t Seal(const RootMotionControllerTransaction3D&)const noexcept;
    bool pending_{};
    std::uint64_t generation_{1},serial_{};
    RootMotionControllerFailureCheckpoint3D failureCheckpoint_{};
    RootMotion3D* pendingRoot_{};
    CharacterController3D* pendingController_{};
    const AnimationPose3D* pendingPose_{};
    RootMotionPreviewToken3D pendingRootToken_{};
    CharacterDirectDisplacementPreviewToken3D pendingControllerToken_{};
    RootMotionControllerResult3D pendingResult_{};
    std::uint64_t pendingSeal_{};
};
}
