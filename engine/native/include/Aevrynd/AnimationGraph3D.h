#pragma once
#include "Animation3D.h"
#include "Memory.h"
#include "Renderer.h"
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

// 圖表刻意不派發 clip 事件；事件系統加入前不會在混合期間重複觸發。
class AnimationGraph3D {
public:
    static constexpr std::size_t MaximumStates=8,MaximumSamplesPerState=8;
    bool Initialize(const Skeleton3D&,SkeletonCompatibilityId3D,std::span<const AnimationGraphState3D>,AnimationGraphError3D* error=nullptr);
    bool SetState(std::size_t state,float duration=0.15f,bool synchronizeNormalizedTime=true);
    void SetParameter(float value)noexcept{parameter_=std::isfinite(value)?value:0.f;}
    bool SetNormalizedPhase(float value)noexcept;
    bool SetUpperBodyLayer(AnimationGraphLayer3D layer,AnimationGraphError3D* error=nullptr);
    void ClearUpperBodyLayer()noexcept{layerEnabled_=false;layerWeight_=0;}
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
private:
    struct OwnedSample {float parameter{};AnimationClip3D clip{};};
    struct StateStorage {std::string name{};std::array<OwnedSample,MaximumSamplesPerState> samples{};std::size_t count{};};
    bool SampleState(const Skeleton3D&,std::size_t,float,AnimationPose3D&,Matrix4);
    float StateDuration(std::size_t state)const noexcept;
    std::array<StateStorage,MaximumStates> states_{};std::size_t stateCount_{},state_{};
    SkeletonCompatibilityId3D compatibility_{};std::size_t jointCount_{};
    float parameter_{},phase_{},transitionDuration_{},transitionElapsed_{};
    bool transitioning_{},transitionSnapshotValid_{};
    Skeleton3D skeletonDefinition_{};AnimationClip3D layerClip_{};std::vector<float> layerWeights_{};float layerWeight_{};bool layerEnabled_{};
    AnimationPose3D outputScratch_{},baseScratch_{},sampleA_{},sampleB_{},transitionFrom_{},layerPose_{};
    AnimationGraphStats3D stats_{};std::size_t lastReleasedBytes_{};std::uint64_t trimCount_{};
};
}
