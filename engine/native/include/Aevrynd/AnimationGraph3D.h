#pragma once
#include "Animation3D.h"
#include "Memory.h"
#include "Renderer.h"
#include "RootMotion3D.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace Aevrynd {
struct AnimationBlendSample1D3D {float parameter{};const AnimationClip3D* clip{};};
struct AnimationGraphState3D {std::string_view name{};std::span<const AnimationBlendSample1D3D> samples{};};
struct AnimationGraphLayer3D {const AnimationClip3D* clip{};std::span<const float> jointWeights{};float weight{};};
enum class AnimationGraphError3D : std::uint8_t {None,InvalidSkeleton,UnknownCompatibility,Limit,InvalidState,InvalidSample,DuplicateSample,InvalidLayer,InvalidTime};
struct AnimationGraphStats3D {std::uint64_t evaluations{},transitions{},interruptions{},rejectedEvaluations{};};
struct AnimationGraphRootMotionSettings3D {
    // 單 tick 每個 leaf 可跨越的循環上限；必須大於零。
    std::uint32_t maximumCyclesPerTick{8};
    // false 時 Y 強制為正零；true 時抽取並移除 root Y。
    bool extractVertical{};
};
struct AnimationGraphRootMotionTick3D {
    // 固定模擬 tick 必須嚴格連續；Reset 後從 1 開始。
    std::uint64_t tick{};
    // 固定 tick 秒數與播放倍率；允許負倍率，兩者均須 finite。
    double fixedDeltaSeconds{},playbackRate{1};
    // 只用於輸出 globals，不可混入 root delta。
    Matrix4 poseRoot{Matrix4::Identity()};
};
struct AnimationGraphRootMotionResult3D {
    RootMotionDelta3D delta{};
    std::size_t state{};
    float transitionWeight{};
    std::uint32_t activeLeaves{};
    // deterministicHash 僅保證同 build/version/platform 重播一致。
    std::uint64_t deterministicHash{};
};
class AnimationGraphRootMotionPreviewToken3D {
public: AnimationGraphRootMotionPreviewToken3D()=default;
private:
    friend class AnimationGraph3D;
    std::uintptr_t owner{};const AnimationPose3D* pose{};
    std::uint64_t generation{},serial{},tick{},baseStateHash{},poseHash{},candidateHash{};
    bool valid{};
};

// 圖表刻意不派發 clip 事件；事件系統加入前不會在混合期間重複觸發。
class AnimationGraph3D {
public:
    static constexpr std::size_t MaximumStates=8,MaximumSamplesPerState=8,MaximumRootMotionLeaves=16;
    bool Initialize(const Skeleton3D&,SkeletonCompatibilityId3D,std::span<const AnimationGraphState3D>,AnimationGraphError3D* error=nullptr);
    bool SetState(std::size_t state,float duration=0.15f,bool synchronizeNormalizedTime=true);
    void SetParameter(float value)noexcept;
    bool SetNormalizedPhase(float value)noexcept;
    bool SetUpperBodyLayer(AnimationGraphLayer3D layer,AnimationGraphError3D* error=nullptr);
    void ClearUpperBodyLayer()noexcept;
    void Update(float deltaTime)noexcept;
    bool EvaluateInto(const Skeleton3D&,SkeletonCompatibilityId3D,AnimationPose3D&,Matrix4 root=Matrix4::Identity());
    void Reset()noexcept;
    [[nodiscard]] std::size_t State()const noexcept{return state_;}
    [[nodiscard]] float TransitionAmount()const noexcept;
    [[nodiscard]] float Parameter()const noexcept{return parameter_;}
    [[nodiscard]] float NormalizedPhase()const noexcept{return phase_;}
    [[nodiscard]] const AnimationGraphStats3D& Stats()const noexcept{return stats_;}
    [[nodiscard]] ReusableMemoryStats MemoryStats()const noexcept;
    std::size_t TrimUnusedMemory();
    // Root-motion Prepare 深拷貝契約沿用 Initialize 的 clips，並一次配置所有 warm scratch。
    bool PrepareRootMotion(const HumanoidSkeletonMap3D&,SkeletonCompatibilityId3D,AnimationGraphRootMotionSettings3D={});
    // Preview 只寫 candidate workspace；graph clocks/transition/pose/output 均不提交。
    bool PreviewRootMotionTick(const Skeleton3D&,SkeletonCompatibilityId3D,AnimationGraphRootMotionTick3D,const AnimationPose3D&,AnimationGraphRootMotionPreviewToken3D&,AnimationGraphRootMotionResult3D* preview=nullptr)noexcept;
    bool CanCommitRootMotionPreview(const AnimationGraphRootMotionPreviewToken3D&,const AnimationPose3D&)const noexcept;
    bool CommitRootMotionPreview(AnimationGraphRootMotionPreviewToken3D&,AnimationPose3D&,AnimationGraphRootMotionResult3D* output=nullptr)noexcept;
    bool CancelRootMotionPreview(AnimationGraphRootMotionPreviewToken3D&)noexcept;
    bool EvaluateRootMotionTick(const Skeleton3D&,SkeletonCompatibilityId3D,AnimationGraphRootMotionTick3D,AnimationPose3D&,AnimationGraphRootMotionResult3D* output=nullptr)noexcept;
    void ResetRootMotion()noexcept;
    bool TeleportRootMotion(std::uint64_t previousTick)noexcept;
private:
    struct OwnedSample {float parameter{};AnimationClip3D clip{};};
    struct StateStorage {std::string name{};std::array<OwnedSample,MaximumSamplesPerState> samples{};std::size_t count{};};
    bool SampleState(const Skeleton3D&,std::size_t,float,AnimationPose3D&,Matrix4);
    float StateDuration(std::size_t state)const noexcept;
    struct RootMotionLeaf {std::uint16_t state{},sample{};std::int64_t clockQ32{},rateQ32{std::int64_t{1}<<32};float weightStart{},weightEnd{};std::uint64_t activationSerial{};};
    bool BuildRootMotionDestination(std::size_t,std::int64_t,float,std::array<RootMotionLeaf,MaximumRootMotionLeaves>&,std::size_t&,std::uint64_t&)const noexcept;
    bool BeginRootMotionTransition(std::size_t,float,bool)noexcept;
    void InvalidateRootMotionPreview(bool committedMutation)noexcept;
    [[nodiscard]] std::uint64_t RootMotionStateHash()const noexcept;
    std::array<StateStorage,MaximumStates> states_{};std::size_t stateCount_{},state_{};
    SkeletonCompatibilityId3D compatibility_{};std::size_t jointCount_{};
    float parameter_{},phase_{},transitionDuration_{},transitionElapsed_{};
    bool transitioning_{},transitionSnapshotValid_{};
    Skeleton3D skeletonDefinition_{};AnimationClip3D layerClip_{};std::vector<float> layerWeights_{};float layerWeight_{};bool layerEnabled_{};
    AnimationPose3D outputScratch_{},baseScratch_{},sampleA_{},sampleB_{},transitionFrom_{},layerPose_{};
    AnimationGraphStats3D stats_{};std::size_t lastReleasedBytes_{};std::uint64_t trimCount_{};
    AnimationGraphRootMotionSettings3D rootMotionSettings_{};
    std::array<RootMotionLeaf,MaximumRootMotionLeaves> rootLeaves_{},rootCandidateLeaves_{};
    std::size_t rootLeafCount_{},rootCandidateLeafCount_{},rootJoint_{};
    double rootTransitionElapsed_{},rootTransitionDuration_{};
    double rootCandidateTransitionElapsed_{};
    std::uint64_t rootLastTick_{},rootActivationSerial_{},rootGeneration_{1},rootPreviewSerial_{};
    bool rootMotionPrepared_{},rootAnchored_{},rootPreviewPending_{};
    AnimationPose3D rootCandidatePose_{};
    std::array<AnimationPose3D,MaximumRootMotionLeaves> rootLeafPoses_{};
    AnimationGraphRootMotionResult3D rootPreviewResult_{};
    std::uint64_t rootPreviewBaseStateHash_{};
};
}
