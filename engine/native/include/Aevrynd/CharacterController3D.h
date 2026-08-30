#pragma once
#include "Camera.h"
#include "CollisionWorld3D.h"
#include "HeightField3D.h"
#include <cstdint>
#include <span>
#include <vector>

namespace Aevrynd {
struct CharacterControl3D {Vec3 movement{};bool jump{};};
struct CharacterControllerSettings3D {
    float radius{.45f};
    float height{1.8f};
    float moveSpeed{6.f};
    float jumpSpeed{7.f};
    float gravity{20.f};
    float maximumFallSpeed{35.f};
    float groundSnapDistance{.4f};
    // 單次直接位移的世界空間長度上限；避免不可信動畫輸入跨越過大範圍。
    float maximumDirectDisplacement{100.f};
    // swept-AABB 滑動解算次數，production 目前只接受 1..3。
    unsigned maximumSlideIterations{3};
};
struct CharacterControllerStats3D {uint64_t steps{},collisions{},jumps{},landings{},terrainContacts{},directTicks{},directBlocked{},directOverlaps{};};

// 固定模擬 tick 的直接位移輸入；translationWorld 不會再乘速度或 deltaTime。
struct CharacterDirectDisplacementTick3D {
    // tick 必須嚴格連續；Reset 後從 1 開始，Teleport 後接 previousTick+1。
    uint64_t tick{};
    // 世界空間 requested translation，由 locomotion owner 在呼叫前完成 model-to-world 轉換。
    Vec3 translationWorld{};
    // 本 tick requested actor yaw delta；直立碰撞形狀不會阻擋旋轉。
    float yawDeltaRadians{};
};
// Applied/NoMotion/Blocked/StartedOverlapping 都代表成功並提交 tick；false 才代表交易失敗。
enum class CharacterDirectDisplacementStatus3D:uint8_t {Applied,NoMotion,Blocked,StartedOverlapping};
// 直接位移交易結果；requestedWorld == consumedWorld + remainderWorld，地形修正獨立列出。
struct CharacterDirectDisplacementResult3D {
    CharacterDirectDisplacementStatus3D status{CharacterDirectDisplacementStatus3D::NoMotion};
    uint64_t tick{},deterministicHash{};
    Vec3 requestedWorld{},consumedWorld{},remainderWorld{},groundCorrectionWorld{};
    float requestedYawRadians{},consumedYawRadians{},remainderYawRadians{};
    bool grounded{};
    uint32_t collisionCount{};
};
// Controller direct preview 的本機權杖；不可序列化或跨 instance 使用。
class CharacterDirectDisplacementPreviewToken3D {
public:
    CharacterDirectDisplacementPreviewToken3D()=default;
private:
    friend class CharacterController3D;
    friend class RootMotionController3D;
    // owner 綁定建立 preview 的 controller instance；不得序列化、仿造或跨 instance 使用。
    std::uintptr_t owner{};
    // generation/serial/tick/baseStateHash/candidateHash 共同防止 stale、replay 與 committed state 修改。
    uint64_t generation{},serial{},tick{},baseStateHash{},candidateHash{};
    // valid 仍須搭配 CanCommitDirectDisplacementPreview 驗證，不代表可直接提交。
    bool valid{};
};

class RootMotionController3D;

class CharacterController3D {
public:
    explicit CharacterController3D(CharacterControllerSettings3D settings={}):settings_(settings){}
    void Reset(Vec3 feetPosition={});
    // 唯一允許為 CollisionWorld direct path 配置容量的階段；失敗保留舊容量與狀態。
    bool PrepareDirectDisplacement(std::size_t maximumWorldColliders);
    // span/world 與 terrain overload 皆為整體交易；false 時 controller 與 output 保持不變。
    bool ApplyDirectDisplacementTick(CharacterDirectDisplacementTick3D,std::span<const BoxCollider3D>,CharacterDirectDisplacementResult3D* output=nullptr)noexcept;
    bool ApplyDirectDisplacementTick(CharacterDirectDisplacementTick3D,const CollisionWorld3D&,CollisionQueryFilter3D filter={},CharacterDirectDisplacementResult3D* output=nullptr)noexcept;
    bool ApplyDirectDisplacementTick(CharacterDirectDisplacementTick3D,std::span<const BoxCollider3D>,const HeightField3D&,CharacterDirectDisplacementResult3D* output=nullptr)noexcept;
    bool ApplyDirectDisplacementTick(CharacterDirectDisplacementTick3D,const CollisionWorld3D&,const HeightField3D&,CollisionQueryFilter3D filter={},CharacterDirectDisplacementResult3D* output=nullptr)noexcept;
    // Preview overload 只改 pending workspace；committed state、StateHash 與 output 在失敗時不變。
    bool PreviewDirectDisplacementTick(CharacterDirectDisplacementTick3D,std::span<const BoxCollider3D>,CharacterDirectDisplacementPreviewToken3D&,CharacterDirectDisplacementResult3D* preview=nullptr)noexcept;
    bool PreviewDirectDisplacementTick(CharacterDirectDisplacementTick3D,const CollisionWorld3D&,CollisionQueryFilter3D,CharacterDirectDisplacementPreviewToken3D&,CharacterDirectDisplacementResult3D* preview=nullptr)noexcept;
    bool PreviewDirectDisplacementTick(CharacterDirectDisplacementTick3D,std::span<const BoxCollider3D>,const HeightField3D&,CharacterDirectDisplacementPreviewToken3D&,CharacterDirectDisplacementResult3D* preview=nullptr)noexcept;
    bool PreviewDirectDisplacementTick(CharacterDirectDisplacementTick3D,const CollisionWorld3D&,const HeightField3D&,CollisionQueryFilter3D,CharacterDirectDisplacementPreviewToken3D&,CharacterDirectDisplacementResult3D* preview=nullptr)noexcept;
    // CanCommit 僅驗證不提交；Cancel 只撤銷同 owner 的 outstanding preview，兩者皆不改 committed state。
    bool CanCommitDirectDisplacementPreview(const CharacterDirectDisplacementPreviewToken3D&)const noexcept;
    bool CancelDirectDisplacementPreview(CharacterDirectDisplacementPreviewToken3D&)noexcept;
    void Step(CharacterControl3D control,std::span<const BoxCollider3D> colliders,float deltaTime);
    void Step(CharacterControl3D control,const CollisionWorld3D& world,float deltaTime,uint32_t layerMask=0xffffffffu,uint32_t characterLayer=1);
    void Step(CharacterControl3D control,std::span<const BoxCollider3D> colliders,const HeightField3D& terrain,float deltaTime);
    void Step(CharacterControl3D control,const CollisionWorld3D& world,const HeightField3D& terrain,float deltaTime,uint32_t layerMask=0xffffffffu,uint32_t characterLayer=1);
    Vec3 Position()const{return position_;}
    Vec3 Velocity()const{return velocity_;}
    Vec3 Center()const{return position_+Vec3{0,settings_.height*.5f,0};}
    Vec3 Extents()const{return{settings_.radius,settings_.height*.5f,settings_.radius};}
    bool Grounded()const{return grounded_;}
    // actor yaw 的唯一 controller 權威值，範圍為 (-pi,pi]。
    float Yaw()const{return yaw_;}
    // 原子傳送並重設 direct timeline；下一合法 tick 是 previousTick+1。
    bool Teleport(Vec3 feetPosition,float yawRadians,uint64_t previousTick)noexcept;
    bool SetHeight(float height);
    const CharacterControllerSettings3D& Settings()const{return settings_;}
    CharacterControllerStats3D Stats()const{return stats_;}
    [[nodiscard]] ReusableMemoryStats MemoryStats()const noexcept;
    std::size_t TrimUnusedMemory();
    [[nodiscard]] uint64_t StateHash()const noexcept;
    [[nodiscard]] uint64_t DirectLastTick()const noexcept{return directLastTick_;}
    bool CanRebaseOrigin(Vec3 delta)const noexcept;
    void RebaseOrigin(Vec3 delta)noexcept;
private:
    friend class RootMotionController3D;
    void CommitDirectDisplacementPreviewUnchecked(CharacterDirectDisplacementPreviewToken3D&)noexcept;
    void InvalidateDirectPreview(bool committedMutation)noexcept;
    CharacterControllerSettings3D settings_{};
    CharacterControllerStats3D stats_{};
    Vec3 position_{},velocity_{};
    float yaw_{};
    uint64_t directLastTick_{},lastReleasedBytes_{},trimCount_{};
    bool grounded_{};
    std::vector<BoxCollider3D> collisionScratch_;
    // World 查詢先寫入 staging；只有整筆 direct transaction 成功才與 active scratch 交換。
    std::vector<BoxCollider3D> collisionCandidateScratch_;
    std::size_t maximumDirectWorldColliders_{};
    uint64_t directGeneration_{1},directPreviewSerial_{};
    bool directPreviewPending_{},directPreviewUsesWorldScratch_{};
    CharacterDirectDisplacementResult3D directPreviewResult_{};
    CharacterControllerStats3D directPreviewStats_{};
    Vec3 directPreviewPosition_{},directPreviewVelocity_{};
    float directPreviewYaw_{};
    bool directPreviewGrounded_{};
    uint64_t directPreviewBaseStateHash_{};
    bool PreviewDirectDisplacementTickInternal(CharacterDirectDisplacementTick3D,std::span<const BoxCollider3D>,const HeightField3D*,bool,CharacterDirectDisplacementPreviewToken3D&,CharacterDirectDisplacementResult3D*)noexcept;
};

Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,std::span<const BoxCollider3D> colliders,float padding=.2f);
Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,const CollisionWorld3D& world,float padding=.2f,uint32_t layerMask=0xffffffffu,uint32_t cameraLayer=1);
Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,const HeightField3D& terrain,float padding=.2f,unsigned samples=32);
}
