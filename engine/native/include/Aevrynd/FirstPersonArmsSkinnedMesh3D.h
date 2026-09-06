#pragma once

#include "SkinnedModelAsset.h"

#include <cstdint>
#include <array>
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

enum class FirstPersonArmSide3D : std::uint8_t { Left, Right };
enum class FirstPersonArmsCutStatus3D : std::uint8_t {
    InvalidProvenance, NoCutBoundary, AmbiguousTopology, Complete
};
struct FirstPersonArmsTriangleProvenance3D {
    std::uint32_t sourceFirstIndex{};
    FirstPersonArmSide3D side{};
};
struct FirstPersonArmsCutEdge3D {
    std::array<std::uint32_t,2> sourceVertices{},compactVertices{};
    std::uint32_t retainedSourceFirstIndex{},rejectedSourceFirstIndex{};
    FirstPersonArmSide3D side{};
};
struct FirstPersonArmsBoundaryEdge3D {
    std::array<std::uint32_t,2> sourceVertices{},compactVertices{};
    std::uint32_t sourceFirstIndex{};
    FirstPersonArmSide3D side{};
};
struct FirstPersonArmsCoincidentSeam3D {
    // 同向且位置與權重完全相同的兩條保留邊；記錄警告，不宣稱繞序已修復。
    std::array<FirstPersonArmsBoundaryEdge3D,2> edges;
};
struct FirstPersonArmsCutTopology3D {
    FirstPersonArmsCutStatus3D status{FirstPersonArmsCutStatus3D::InvalidProvenance};
    std::vector<FirstPersonArmsCutEdge3D> edges;
    std::vector<FirstPersonArmsBoundaryEdge3D> retainedBoundaries;
    std::vector<FirstPersonArmsCoincidentSeam3D> retainedSameDirectionSeams;
    std::uint32_t leftCutEdges{},rightCutEdges{},sourceBoundaryEdges{},retainedSourceBoundaryEdges{},seamPairs{},
        ambiguousEdges{},influenceMismatchEdges{},degenerateTriangles{},openCutVertices{};
    std::uint32_t relevantAmbiguousEdges{},rejectedOnlyWarningEdges{},combinedOpenVertices{};
    bool surfaceManifold{},boundaryInventoryComplete{};
    // 僅代表來源萃取拓樸可追溯；不代表切口位於視口外或已通過影像驗收。
    [[nodiscard]] bool Available()const noexcept{return status==FirstPersonArmsCutStatus3D::Complete;}
    // 完整外露邊清單包含切口與原始保留邊界；不代表所有來源表面皆為流形。
    [[nodiscard]] bool BoundaryInventoryAvailable()const noexcept{return boundaryInventoryComplete;}
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
    std::vector<FirstPersonArmsTriangleProvenance3D> retainedTriangleProvenance;
    float minimumVertexArmWeight{.55f};
    FirstPersonArmsCutTopology3D cutTopology;

    [[nodiscard]] bool Complete() const { return status == FirstPersonArmsMeshStatus3D::Complete; }
    explicit operator bool() const { return Complete(); }
};

[[nodiscard]] FirstPersonArmsCutTopology3D BuildFirstPersonArmsCutTopology3D(
    const SkinnedModelAssetData& source,const FirstPersonArmsMeshResult3D& arms);

[[nodiscard]] FirstPersonArmsMeshResult3D BuildFirstPersonArmsSkinnedMesh3D(
    const SkinnedModelAssetData& source,
    const FirstPersonArmsMeshSettings3D& settings = {});
}
