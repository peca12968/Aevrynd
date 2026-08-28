#pragma once
#include "Mesh3D.h"
#include "Memory.h"
#include "SkinnedModelAsset.h"
#include <span>
#include <vector>

namespace Aevrynd {

// 將同一骨架姿勢的所有子網格視為單一交易，避免容量不足時只畫出部分角色。
bool SubmitSkinnedRenderable3D(DrawList& draw,SkinnedMeshHandle3D mesh,
    SkinPaletteRange3D palette,Matrix4 transform,std::uint32_t totalIndexCount,
    std::span<const SkinnedSubmesh3D> submeshes,
    std::span<const Material3D> materialBindings,SkinPaletteRange3D previousPalette={},
    Matrix4 previousTransform=Matrix4::Identity(),bool motionValid=false);

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

}
