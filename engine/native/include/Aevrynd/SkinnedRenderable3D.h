#pragma once
#include "Mesh3D.h"
#include "Memory.h"
#include "SkinnedModelAsset.h"
#include <span>
#include <array>
#include <vector>

namespace Aevrynd {
class D3D12Renderer;
class Engine;
struct SkinnedRenderableHistoryTestAccess3D;

struct SkinnedRenderablePartKey3D {
    std::uint64_t value{};
    constexpr explicit operator bool()const noexcept{return value!=0;}
    constexpr bool operator==(const SkinnedRenderablePartKey3D&)const noexcept=default;
};

// 將同一骨架姿勢的所有子網格視為單一交易，避免容量不足時只畫出部分角色。
bool SubmitSkinnedRenderable3D(DrawList& draw,SkinnedMeshHandle3D mesh,
    SkinPaletteRange3D palette,Matrix4 transform,std::uint32_t totalIndexCount,
    std::span<const SkinnedSubmesh3D> submeshes,
    std::span<const Material3D> materialBindings,SkinPaletteRange3D previousPalette={},
    Matrix4 previousTransform=Matrix4::Identity(),bool motionValid=false,
    std::uint32_t submissionGroupId=0);

struct SkinnedRenderablePart3D {
    SkinnedMeshHandle3D mesh{};
    Matrix4 transform{Matrix4::Identity()},previousTransform{Matrix4::Identity()};
    std::uint32_t totalIndexCount{};
    std::span<const SkinnedSubmesh3D> submeshes{};
    std::span<const Material3D> materialBindings{};
    SkinnedRenderablePartKey3D key{};
    bool motionValid{};
};

enum class SkinnedRenderableGroupFailure3D : std::uint8_t {None,Empty,InvalidPartKey,DuplicatePartKey,UnknownSkeleton,SkeletonMismatch,CurrentPaletteMismatch,PreviousPaletteMismatch,Range,Material,Capacity};
struct SkinnedRenderableGroupStats3D {
    std::uint64_t acceptedGroups{},acceptedParts{},acceptedInstances{},rejectedGroups{};
    std::uint64_t unknownSkeletonRejections{},skeletonMismatchRejections{},currentPaletteMismatchRejections{},previousPaletteMismatchRejections{};
    std::uint64_t rangeRejections{},materialRejections{},capacityRejections{};
    std::uint64_t invalidPartKeyRejections{},duplicatePartKeyRejections{};
    SkinnedRenderableGroupFailure3D lastFailure{SkinnedRenderableGroupFailure3D::None};
};

bool SubmitSkinnedRenderableGroup3D(DrawList& draw,SkinPaletteRange3D palette,
    std::span<const SkinnedRenderablePart3D> parts,SkinPaletteRange3D previousPalette={},
    bool motionValid=false,SkinnedRenderableGroupStats3D* stats=nullptr,
    SkinnedSubmissionReceipt3D* receipt=nullptr);

struct SkinnedMotionHistorySample3D {
    SkinPaletteRange3D previousPalette{};
    Matrix4 previousTransform{Matrix4::Identity()};
    bool motionValid{};
};

// 保存上一個實際提交的骨架姿勢，讓遊戲不必自行管理 palette 世代與回退規則。
class SkinnedMotionHistory3D {
public:
    SkinnedMotionHistorySample3D Advance(DrawList& draw,SkinPaletteRange3D currentPalette,
        Matrix4 currentTransform,bool reset=false);
    void Invalidate()noexcept{valid_=false;previousCompatibilityId_={};}
    bool Valid()const noexcept{return valid_;}
    ReusableMemoryStats MemoryStats()const noexcept;
    std::size_t TrimUnusedMemory();
private:
    std::vector<SkinPaletteJoint3D> previousJoints_{};
    Matrix4 previousTransform_{Matrix4::Identity()};
    SkeletonCompatibilityId3D previousCompatibilityId_{};
    std::size_t lastReleasedBytes_{};
    std::uint64_t trimCount_{};
    bool valid_{};
};

// 以穩定零件鍵保存群組歷史；排序與增減裝備不會讓 transform 串錯零件。
class SkinnedRenderableGroupHistory3D {
public:
    static constexpr std::size_t MaximumParts=64;
    bool Submit(DrawList& draw,SkinPaletteRange3D currentPalette,
        std::span<const SkinnedRenderablePart3D> parts,bool reset=false,
        SkinnedRenderableGroupStats3D* stats=nullptr);
    SkinnedSubmissionResult3D ResolveSubmitted(const D3D12Renderer& renderer)noexcept;
    SkinnedSubmissionResult3D ResolveSubmitted(const Engine& engine)noexcept;
    void DiscardSubmitted()noexcept{pending_=false;motionReset_=true;invalidateRequested_=false;}
    bool HasPendingSubmission()const noexcept{return pending_;}
    void Invalidate()noexcept{motionReset_=true;if(pending_){invalidateRequested_=true;return;}valid_=false;partCount_=pendingPartCount_=0;previousCompatibilityId_={};pendingCompatibilityId_={};}
    bool Valid()const noexcept{return valid_;}
    std::size_t PartCount()const noexcept{return partCount_;}
    ReusableMemoryStats MemoryStats()const noexcept;
    std::size_t TrimUnusedMemory();
private:
    friend struct SkinnedRenderableHistoryTestAccess3D;
    bool CommitSubmittedInternal()noexcept;
    struct PartSnapshot {
        SkinnedRenderablePartKey3D key{};
        SkinnedMeshHandle3D mesh{};
        Matrix4 transform{Matrix4::Identity()};
    };
    std::vector<SkinPaletteJoint3D> previousJoints_{};
    std::vector<SkinPaletteJoint3D> pendingJoints_{};
    std::array<PartSnapshot,MaximumParts> parts_{};
    std::array<PartSnapshot,MaximumParts> pendingParts_{};
    SkeletonCompatibilityId3D previousCompatibilityId_{};
    SkeletonCompatibilityId3D pendingCompatibilityId_{};
    std::size_t partCount_{},pendingPartCount_{},lastReleasedBytes_{};
    std::uint64_t trimCount_{};
    bool valid_{},pending_{},motionReset_{},invalidateRequested_{};
    SkinnedSubmissionReceipt3D pendingReceipt_{};
};

#if defined(AEVRYND_TESTING)
struct SkinnedRenderableHistoryTestAccess3D {static bool Commit(SkinnedRenderableGroupHistory3D& history)noexcept{return history.CommitSubmittedInternal();}};
#endif

}
