#pragma once
#include "Camera.h"
#include "CollisionWorld3D.h"
#include "HeightField3D.h"
#include "CharacterDynamicContact3D.h"
#include <cstdint>
#include <span>
#include <vector>

namespace Aevrynd {
struct CharacterControl3D {
    Vec3 movement{};
    bool jump{};
    // 非有限或 <=0 回退 1；其餘夾在 [.25,4]，且仍走同一碰撞、地形與 grounded Step。
    float speedMultiplier{1.f};
};
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
    // 0 保留舊行為；(0,16] 為高度場最大可上行坡度（rise/run）。
    // 限制足點接觸後切入地形的上坡；保留空中淨空前段及橋面移動，不提供滑坡物理。
    float maximumTerrainUphillGrade{};
    // 0 保留舊支撐；(0,16] 以腳中心所在真實三角面限制站立。
    // 陡面接觸時 Step 沿重力切向下滑，只接受等高線控制；不是全腳掌支撐或摩擦模型。
    // direct 沒有時間參數，只分類支撐，不自行積分重力。
    float maximumTerrainStandingGrade{};
    // 0 保留既有路徑；(0,.25] 允許自然離開真支撐後短暫補跳，單位為秒。
    // 啟用時統一走連續碰撞路徑；不保留傳送、讀檔或直接位移之前的寬限。
    float jumpGraceSeconds{};
};
struct CharacterControllerStats3D {uint64_t steps{},collisions{},jumps{},landings{},terrainContacts{},directTicks{},directBlocked{},directOverlaps{};};
// 可持久化的物理狀態；不包含預覽權杖、統計或直接位移時序。
struct CharacterMotionState3D {
    Vec3 feet{},velocity{};
    float height{1.8f},yaw{};
    bool grounded{};
};

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
    friend class AnimationGraphRootMotionController3D;
    // owner 綁定建立 preview 的 controller instance；不得序列化、仿造或跨 instance 使用。
    std::uintptr_t owner{};
    // generation/serial/tick/baseStateHash/candidateHash 共同防止 stale、replay 與 committed state 修改。
    uint64_t generation{},serial{},tick{},baseStateHash{},candidateHash{};
    // valid 仍須搭配 CanCommitDirectDisplacementPreview 驗證，不代表可直接提交。
    bool valid{};
};

class RootMotionController3D;
class AnimationGraphRootMotionController3D;
class ConvexAabbSweep3D;

// 本候選只提交合法初始分離下的側面碰撞；未解事件不能被當成 Clear 或支撐。
enum class CharacterDynamicStepStatus3D:uint8_t {
    Applied,InvalidInput,UnresolvedNumerics,UnresolvedVerticalEntry,
    UnresolvedSimultaneousEntry,UnresolvedStartedOverlap
};

// 頂面只提供碰撞法向阻擋；不授權接地、跳躍、跨階或搭乘。
enum class CharacterDynamicVerticalResponse3D:uint8_t { Reject,BlockOnly };
struct CharacterDynamicContactState3D {
    bool blockedSide{},blockedAbove{},blockedBelow{};
};

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
    // 斜向凸形體只借用本次呼叫期間的唯讀資料；失敗時整個控制器不變。
    // 空凸形集合保留原方盒路徑；非空時使用連續掃掠與完整身體向下支撐。
    bool Step(CharacterControl3D,std::span<const BoxCollider3D>,std::span<const ConvexAabbSweep3D* const>,float deltaTime);
    bool Step(CharacterControl3D,std::span<const BoxCollider3D>,std::span<const ConvexAabbSweep3D* const>,const HeightField3D&,float deltaTime);
    // 動態圓柱僅借用本次呼叫的靜止快照；不參與地面、跨階、IK 或視線。
    // 空集合走原 Step；非空時只有 Applied 會提交，其他狀態整個 controller 不變。
    CharacterDynamicStepStatus3D Step(CharacterControl3D,std::span<const BoxCollider3D>,
        std::span<const ConvexAabbSweep3D* const>,std::span<const DynamicCylinder3D>,float deltaTime);
    CharacterDynamicStepStatus3D Step(CharacterControl3D,std::span<const BoxCollider3D>,
        std::span<const ConvexAabbSweep3D* const>,const HeightField3D&,std::span<const DynamicCylinder3D>,float deltaTime);
    // 舊動態 overload 保留 Reject；只有本顯式政策會解上下接觸。
    // 成功才寫 appliedContact；其旗標只是本次實際接觸，不是持久支撐。
    CharacterDynamicStepStatus3D Step(CharacterControl3D,std::span<const BoxCollider3D>,
        std::span<const ConvexAabbSweep3D* const>,std::span<const DynamicCylinder3D>,float deltaTime,
        CharacterDynamicVerticalResponse3D,CharacterDynamicContactState3D* appliedContact=nullptr);
    CharacterDynamicStepStatus3D Step(CharacterControl3D,std::span<const BoxCollider3D>,
        std::span<const ConvexAabbSweep3D* const>,const HeightField3D&,std::span<const DynamicCylinder3D>,float deltaTime,
        CharacterDynamicVerticalResponse3D,CharacterDynamicContactState3D* appliedContact=nullptr);
    Vec3 Position()const{return position_;}
    Vec3 Velocity()const{return velocity_;}
    CharacterMotionState3D CaptureMotionState()const noexcept{return {position_,velocity_,settings_.height,yaw_,grounded_};}
    // 原地驗證並恢復，不積分、不吸地、不新增跳躍或落地事件。
    // 成功撤銷舊 preview；失敗保留全部狀態及有效權杖。統計與 direct tick 均保留。
    bool TryRestoreMotionState(const CharacterMotionState3D&,std::span<const BoxCollider3D>,
        std::span<const ConvexAabbSweep3D* const>,const HeightField3D&)noexcept;
    Vec3 Center()const{return position_+Vec3{0,settings_.height*.5f,0};}
    Vec3 Extents()const{return{settings_.radius,settings_.height*.5f,settings_.radius};}
    bool Grounded()const{return grounded_;}
    // 僅為輸入層提示；Step 仍自行驗證世界、下降狀態與尚未消耗的支撐歷史。
    bool CanJumpFromRecentGround()const noexcept{
        return settings_.jumpGraceSeconds>0&&std::isfinite(settings_.jumpSpeed)&&settings_.jumpSpeed>0&&!grounded_&&jumpGraceRemaining_>0&&velocity_.y<=0;
    }
    // 接地時啟動有限向上速度（例如跑步短騰空）；位置不變，後續仍由 Step 解碰撞及落地。
    // 上限沿用 jumpSpeed；失敗不變更狀態。Stats.jumps 計入所有成功離地脈衝。
    bool TryLaunchFromGround(float upwardSpeed)noexcept{
        if(settings_.maximumTerrainStandingGrade!=0)return false;
        return LaunchFromVerifiedGround(upwardSpeed);
    }
    // 啟用坡面站立政策時必須重新核對實際支撐；舊無場景 overload 拒絕啟動。
    bool TryLaunchFromGround(float upwardSpeed,std::span<const BoxCollider3D>,
        std::span<const ConvexAabbSweep3D* const>,const HeightField3D&)noexcept;
    // 動態體只驗證目前淨空；真起跳仍只能由既有靜態／地形接地授權。
    bool TryLaunchFromGround(float upwardSpeed,std::span<const BoxCollider3D>,
        std::span<const ConvexAabbSweep3D* const>,const HeightField3D&,std::span<const DynamicCylinder3D>)noexcept;
private:
    bool LaunchFromVerifiedGround(float upwardSpeed)noexcept{
        if(!grounded_||!std::isfinite(upwardSpeed)||upwardSpeed<=0||
           !std::isfinite(settings_.jumpSpeed)||settings_.jumpSpeed<=0||upwardSpeed>settings_.jumpSpeed||
           !std::isfinite(settings_.gravity)||settings_.gravity<=0||
           !std::isfinite(position_.x)||!std::isfinite(position_.y)||!std::isfinite(position_.z)||
           !std::isfinite(velocity_.x)||!std::isfinite(velocity_.y)||!std::isfinite(velocity_.z))return false;
        InvalidateDirectPreview(true);velocity_.y=upwardSpeed;grounded_=false;++stats_.jumps;return true;
    }
public:
    // actor yaw 的唯一 controller 權威值，範圍為 (-pi,pi]。
    float Yaw()const{return yaw_;}
    // 原子傳送並重設 direct timeline；下一合法 tick 是 previousTick+1。
    [[nodiscard]] bool CanResetDirectDisplacement(Vec3 feetPosition,float yawRadians=0)const noexcept;
    [[nodiscard]] bool CanTeleportDirectDisplacement(Vec3 feetPosition,float yawRadians,uint64_t previousTick)const noexcept;
    bool Teleport(Vec3 feetPosition,float yawRadians,uint64_t previousTick)noexcept;
    bool SetHeight(float height);
    // 增高前檢查完整候選體積，拒絕時保留高度與控制器狀態。
    bool TrySetHeight(float height,std::span<const BoxCollider3D> colliders);
    bool TrySetHeight(float height,std::span<const BoxCollider3D>,std::span<const ConvexAabbSweep3D* const>);
    // 增高碰到動態體時保留原高度；不靠縮身修復起始穿透。
    bool TrySetHeight(float height,std::span<const BoxCollider3D>,std::span<const ConvexAabbSweep3D* const>,
        std::span<const DynamicCylinder3D>);
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
    friend class AnimationGraphRootMotionController3D;
    void ResetDirectDisplacementUnchecked(Vec3 feetPosition,float yawRadians)noexcept;
    void TeleportDirectDisplacementUnchecked(Vec3 feetPosition,float yawRadians,uint64_t previousTick)noexcept;
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
    double jumpGraceRemaining_{};
    bool PreviewDirectDisplacementTickInternal(CharacterDirectDisplacementTick3D,std::span<const BoxCollider3D>,const HeightField3D*,bool,CharacterDirectDisplacementPreviewToken3D&,CharacterDirectDisplacementResult3D*)noexcept;
    bool StepConvexInternal(CharacterControl3D,std::span<const BoxCollider3D>,std::span<const ConvexAabbSweep3D* const>,const HeightField3D*,float,
        std::span<const DynamicCylinder3D> dynamic={},CharacterDynamicStepStatus3D* dynamicStatus=nullptr,
        CharacterDynamicVerticalResponse3D verticalResponse=CharacterDynamicVerticalResponse3D::Reject,
        CharacterDynamicContactState3D* appliedContact=nullptr);
};

// 先降低低頂棚下的仰角，再交給既有牆面／地形避障；保留方位、期望距離及投影設定。
Camera3D ConstrainThirdPersonCameraCeiling(Camera3D camera,std::span<const BoxCollider3D> colliders,float padding=.2f,float footprintMargin=.45f);
Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,std::span<const BoxCollider3D> colliders,float padding=.2f);
Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,const CollisionWorld3D& world,float padding=.2f,uint32_t layerMask=0xffffffffu,uint32_t cameraLayer=1);
Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,const HeightField3D& terrain,float padding=.2f,unsigned samples=32);
struct ThirdPersonFramingResult3D {
    Camera3D camera{};
    float yawAdjustment{};
    unsigned candidatesTested{};
    bool subjectVisible{};
};
// 最多 37 個固定候選（原方位加上 18 組左右方位）；每個重新套用屋頂、碰撞與地形，不能以距離下限穿過障礙。
// subjectSamples 是呼叫端提供的頭、腳及身體外緣；失敗保留原避障鏡頭並明確回報。
ThirdPersonFramingResult3D ResolveThirdPersonFraming3D(Vec3 target,float yaw,float pitch,float distance,
    std::span<const Vec3> subjectSamples,std::span<const BoxCollider3D> solids,
    std::span<const BoxCollider3D> ceilings,const HeightField3D& terrain,
    std::span<const BoxCollider3D> cameraOnly={},Vec2 viewport={1280,720},float minimumDistance=1.5f,
    float maximumYawAdjustment=1.5707963f,unsigned yawSteps=18);
}
