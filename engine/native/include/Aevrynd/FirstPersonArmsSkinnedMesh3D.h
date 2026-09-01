#pragma once

#include "SkinnedModelAsset.h"

#include <cstdint>
#include <vector>

namespace Aevrynd {
enum class FirstPersonArmsMeshStatus3D : std::uint8_t {
    Complete,
    InvalidSource,
    InvalidSettings,
    MissingJoint,
    AmbiguousJoint,
    InvalidHierarchy,
    InvalidSubmeshRange,
    InvalidInfluences,
    EmptyLeftArm,
    EmptyRightArm,
    EmptyResult,
    AllocationFailure
};

struct FirstPersonArmsMeshSettings3D {
    // 三角形的每個頂點都必須由同一側手臂鏈提供至少此比例的總蒙皮權重。
    float minimumVertexArmWeight{.55f};
};

struct FirstPersonArmsMeshStats3D {
    std::uint32_t sourceTriangles{};
    std::uint32_t retainedTriangles{};
    std::uint32_t rejectedTriangles{};
    std::uint32_t leftArmTriangles{};
    std::uint32_t rightArmTriangles{};
};

struct FirstPersonArmsMeshResult3D {
    FirstPersonArmsMeshStatus3D status{FirstPersonArmsMeshStatus3D::InvalidSource};
    SkinnedModel3D model;
    std::vector<SkinnedSubmesh3D> submeshes;
    FirstPersonArmsMeshStats3D stats;
    std::vector<std::uint16_t> leftArmJoints;
    std::vector<std::uint16_t> rightArmJoints;
    // 每個壓縮後頂點對應的原始頂點索引，供 typed evidence 與離線稽核使用。
    std::vector<std::uint32_t> sourceVertexIndices;

    [[nodiscard]] bool Complete() const { return status == FirstPersonArmsMeshStatus3D::Complete; }
    explicit operator bool() const { return Complete(); }
};

[[nodiscard]] FirstPersonArmsMeshResult3D BuildFirstPersonArmsSkinnedMesh3D(
    const SkinnedModelAssetData& source,
    const FirstPersonArmsMeshSettings3D& settings = {});
}
