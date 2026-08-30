#pragma once
#include "SkeletonSemantics3D.h"
#include "Memory.h"
#include <cstdint>

namespace Aevrynd {
// RootMotion 單次 tick 的資源與抽取政策；所有欄位均於 Prepare 深拷貝保存。
struct RootMotionSettings3D {
    // 單次 tick 可跨越的完整循環上限；必須非零。
    std::uint32_t maximumCyclesPerTick{8};
    // false 時只抽 XZ；true 時同時抽取並 in-place 移除 Y 位移。
    bool extractVertical{};
};
// 固定模擬 tick 的輸入；time 是 clip 秒數且不可先取 modulo。
struct RootMotionTick3D {
    // tick 必須嚴格等於上次成功 tick + 1，Reset 後從 1 開始。
    std::uint64_t tick{};
    // 時間必須保持未包裝；負速播放可令 time 小於上一個成功 tick。
    double time{};
    // 建立輸入與 in-place 輸出 globals 的 pose root；不參與 extracted delta。
    Matrix4 poseRoot{Matrix4::Identity()};
};
// 以該段起始 root yaw 為基準的 model-space SE(2) 位移。
struct RootMotionDelta3D {
    // 模型空間位移；是否包含 Y 由 settings 決定。
    Vec3 translationModel{};
    // 正規化至 (-pi,pi] 的 yaw delta。
    float yawRadians{};
    // tick、穩定雜湊及跨越循環數；reverse 時 crossedCycles 為負。
    std::uint64_t tick{},deterministicHash{};
    std::int64_t crossedCycles{};
};
// Evaluate 的細分狀態：Applied/NoMotion 是成功；InvalidRig/Clip/Pose/Time 分別代表信任邊界失敗；
// DiscontinuousTick 代表 tick 不連續；Limit 代表循環、整數或數值上限。false 不會寫入 output。
enum class RootMotionStatus3D : std::uint8_t {Applied,NoMotion,InvalidRig,InvalidClip,InvalidPose,InvalidTime,DiscontinuousTick,Limit};
// 成功結果；Applied/NoMotion 才是可提交結果。
struct RootMotionResult3D {
    // 成功時描述有無實際移動；失敗不會提交此欄位。
    RootMotionStatus3D status{RootMotionStatus3D::InvalidPose};
    // 與本 tick 對應的抽取結果。
    RootMotionDelta3D delta{};
    // 僅 Applied 與 NoMotion 可視為成功。
    explicit constexpr operator bool()const noexcept{return status==RootMotionStatus3D::Applied||status==RootMotionStatus3D::NoMotion;}
};

class RootMotion3D {
public:
    static constexpr std::size_t MaximumJoints=256;
    // Prepare 是唯一可配置 scratch 的階段；Root 必須為真正 skeleton root。
    // clip 與 skeleton 會深拷貝；失敗保留舊 prepared state。
    bool Prepare(const Skeleton3D&,const HumanoidSkeletonMap3D&,SkeletonCompatibilityId3D,const AnimationClip3D&,RootMotionSettings3D settings={});
    // pose 必須是 clip 在 currentTime wrapped phase 的完整取樣；pose/state/output 共同原子提交。
    bool EvaluateTick(const Skeleton3D&,RootMotionTick3D,AnimationPose3D&,RootMotionResult3D* output=nullptr)noexcept;
    // Reset 保留 prepared clip/scratch 容量；下一個 tick=1 只建立零位移 anchor。
    void Reset()noexcept;
    // Teleport 保留 prepared 資源，將下一個合法 tick 鎖為 previousTick+1，且該 tick delta 為零。
    void Teleport(std::uint64_t previousTick)noexcept;
    // Trim 釋放所有 prepared 容量並使物件回到 unprepared；需重新 Prepare。
    std::size_t TrimUnusedMemory();
    // 回報 scratch、clip 與 skeleton 深拷貝的容量及最近釋放資訊。
    [[nodiscard]] ReusableMemoryStats MemoryStats()const noexcept;
    // 穩定狀態雜湊供 rollback/network 測試；包含 timeline 與 scratch 內容。
    [[nodiscard]] std::uint64_t StateHash()const noexcept;
    // 回傳 Prepare 成功後的不可變抽取設定。
    [[nodiscard]] const RootMotionSettings3D& Settings()const noexcept{return settings_;}
private:
    RootMotionSettings3D settings_{};
    AnimationPose3D scratch_{};
    Skeleton3D skeleton_{};
    SkeletonCompatibilityId3D compatibility_{};
    std::size_t jointCount_{},rootJoint_{};
    AnimationClip3D clip_{};
    bool anchored_{};
    double lastTime_{};
    std::uint64_t lastTick_{},lastReleasedBytes_{},trimCount_{};
};
}
