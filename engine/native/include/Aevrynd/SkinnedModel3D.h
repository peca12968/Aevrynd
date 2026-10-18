#pragma once
#include "Animation3D.h"
#include "Memory.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Aevrynd {
struct SkinInfluences3D {
    std::array<std::uint16_t,4> joints{};
    std::array<std::uint16_t,4> weights{};
};

struct SkinnedModelLimits3D {
    std::size_t maximumVertices{100000};
    std::size_t maximumIndices{300000};
    std::size_t maximumJoints{128};
    std::size_t maximumWorkspaceBytes{16*1024*1024};
};

struct SkinnedModel3D {
    Mesh3D bindMesh;
    Skeleton3D skeleton;
    std::vector<SkinInfluences3D> influences;
    std::vector<Matrix4> inverseBindMatrices;
    // 空陣列代表載入舊資產；每筆為 xyz snorm8 與 w 切線手性。
    std::vector<std::uint32_t> tangents;
    SkeletonCompatibilityId3D skeletonCompatibilityId{};
    [[nodiscard]] bool Valid(const SkinnedModelLimits3D& limits={})const;
};

enum class SkeletonCompatibilityProvenance3D : std::uint8_t {NamedAsset,LegacySynthetic,Structural};
SkeletonCompatibilityId3D ComputeSkeletonCompatibilityId3D(std::span<const std::string> jointNames,
    const Skeleton3D& skeleton,std::span<const Matrix4> inverseBindMatrices,
    SkeletonCompatibilityProvenance3D provenance=SkeletonCompatibilityProvenance3D::NamedAsset);
SkeletonCompatibilityId3D ComputeStructuralSkeletonCompatibilityId3D(const Skeleton3D& skeleton,
    std::span<const Matrix4> inverseBindMatrices);

struct SkinningStats3D {
    std::size_t vertices{};
    std::size_t influences{};
    std::size_t outputBytes{};
};

class SkinningWorkspace3D {
public:
    [[nodiscard]] ReusableMemoryStats MemoryStats()const;
    std::size_t TrimUnusedMemory();
private:
    friend bool SkinModelInto(const SkinnedModel3D&,const AnimationPose3D&,
        SkinningWorkspace3D&,Mesh3D&,SkinningStats3D*,const SkinnedModelLimits3D&);
    friend SkinPaletteRange3D AddSkinPaletteFromPose(DrawList&,const SkinnedModel3D&,
        const AnimationPose3D&,SkinningWorkspace3D&,const SkinnedModelLimits3D&);
    Mesh3D scratch_{};
    std::vector<Matrix4> skinMatrices_{};
    std::vector<Matrix4> normalMatrices_{};
    std::vector<SkinPaletteJoint3D> paletteJoints_{};
    std::size_t lastReleasedBytes_{};
    std::uint64_t trimCount_{};
};

bool SkinModelInto(const SkinnedModel3D& model,const AnimationPose3D& pose,
    SkinningWorkspace3D& workspace,Mesh3D& output,SkinningStats3D* stats=nullptr,
    const SkinnedModelLimits3D& limits={});
bool BuildGpuSkinnedVertices(const SkinnedModel3D& model,std::vector<SkinnedVertex3D>& output,
    const SkinnedModelLimits3D& limits={});
SkinPaletteRange3D AddSkinPaletteFromPose(DrawList& draw,const SkinnedModel3D& model,
    const AnimationPose3D& pose,SkinningWorkspace3D& workspace,
    const SkinnedModelLimits3D& limits={});
}
