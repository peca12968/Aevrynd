#include <Aevrynd/FirstPersonArmsSkinnedMesh3D.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <new>
#include <string>
#include <string_view>

namespace Aevrynd {
namespace {
enum class Side : std::uint8_t { Left, Right };

std::string NormalizeJointName(std::string_view name) {
    std::string normalized;
    normalized.reserve(name.size());
    for (const unsigned char value : name) {
        if (std::isalnum(value)) normalized.push_back(static_cast<char>(std::tolower(value)));
    }
    return normalized;
}

bool EndsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

struct JointLookup3D {
    FirstPersonArmsMeshStatus3D status{FirstPersonArmsMeshStatus3D::MissingJoint};
    std::uint16_t index{};
};

JointLookup3D FindJoint(const std::vector<std::string>& names, Side side,
                       std::span<const std::string_view> roles) {
    const std::string_view sideName = side == Side::Left ? "left" : "right";
    for (const std::string_view role : roles) {
        std::vector<std::size_t> matches;
        const std::string sideRole = std::string(sideName) + std::string(role);
        const std::string roleSide = std::string(role) + std::string(sideName);
        for (std::size_t index = 0; index < names.size(); ++index) {
            const std::string normalized = NormalizeJointName(names[index]);
            if (normalized == sideRole || normalized == roleSide || EndsWith(normalized, sideRole) ||
                EndsWith(normalized, roleSide)) {
                matches.push_back(index);
            }
        }
        if (matches.size() > 1) return {FirstPersonArmsMeshStatus3D::AmbiguousJoint, {}};
        if (matches.size() == 1) {
            if (matches.front() > std::numeric_limits<std::uint16_t>::max())
                return {FirstPersonArmsMeshStatus3D::InvalidSource, {}};
            return {FirstPersonArmsMeshStatus3D::Complete,
                    static_cast<std::uint16_t>(matches.front())};
        }
    }
    return {};
}

bool IsDescendantOf(const Skeleton3D& skeleton, std::size_t child, std::size_t ancestor) {
    if (child >= skeleton.joints.size() || ancestor >= skeleton.joints.size()) return false;
    std::size_t remaining = skeleton.joints.size();
    int cursor = static_cast<int>(child);
    while (cursor >= 0 && remaining--) {
        if (static_cast<std::size_t>(cursor) == ancestor) return true;
        cursor = skeleton.joints[static_cast<std::size_t>(cursor)].parent;
    }
    return false;
}

struct ArmChain3D {
    FirstPersonArmsMeshStatus3D status{FirstPersonArmsMeshStatus3D::MissingJoint};
    std::vector<std::uint16_t> joints;
};

ArmChain3D ResolveArmChain(const SkinnedModelAssetData& source, Side side) {
    constexpr std::array<std::string_view, 2> Shoulder{"shoulder", "clavicle"};
    constexpr std::array<std::string_view, 2> Upper{"upperarm", "arm"};
    constexpr std::array<std::string_view, 3> Lower{"lowerarm", "forearm", "elbow"};
    constexpr std::array<std::string_view, 2> Hand{"hand", "wrist"};
    const JointLookup3D shoulder = FindJoint(source.jointNames, side, Shoulder);
    const JointLookup3D upper = FindJoint(source.jointNames, side, Upper);
    const JointLookup3D lower = FindJoint(source.jointNames, side, Lower);
    const JointLookup3D hand = FindJoint(source.jointNames, side, Hand);
    for (const JointLookup3D lookup : {shoulder, upper, lower, hand})
        if (lookup.status != FirstPersonArmsMeshStatus3D::Complete) return {lookup.status, {}};
    const Skeleton3D& skeleton = source.model.skeleton;
    if (!IsDescendantOf(skeleton, upper.index, shoulder.index) ||
        !IsDescendantOf(skeleton, lower.index, upper.index) ||
        !IsDescendantOf(skeleton, hand.index, lower.index))
        return {FirstPersonArmsMeshStatus3D::InvalidHierarchy, {}};
    ArmChain3D result{FirstPersonArmsMeshStatus3D::Complete,
                      {shoulder.index, upper.index, lower.index, hand.index}};
    for (std::size_t joint = 0; joint < skeleton.joints.size(); ++joint) {
        if (joint != hand.index && IsDescendantOf(skeleton, joint, hand.index))
            result.joints.push_back(static_cast<std::uint16_t>(joint));
    }
    std::sort(result.joints.begin(), result.joints.end());
    result.joints.erase(std::unique(result.joints.begin(), result.joints.end()), result.joints.end());
    return result;
}

float ArmWeight(const SkinInfluences3D& influence, const std::vector<std::uint16_t>& joints) {
    std::uint32_t weight{};
    for (std::size_t slot = 0; slot < influence.weights.size(); ++slot) {
        if (std::binary_search(joints.begin(), joints.end(), influence.joints[slot]))
            weight += influence.weights[slot];
    }
    return static_cast<float>(weight) / 65535.f;
}
}

FirstPersonArmsMeshResult3D BuildFirstPersonArmsSkinnedMesh3D(
    const SkinnedModelAssetData& source, const FirstPersonArmsMeshSettings3D& settings) {
    FirstPersonArmsMeshResult3D result;
    if (!source.model.Valid() || source.jointNames.size() != source.model.skeleton.joints.size())
        return result;
    if (!std::isfinite(settings.minimumVertexArmWeight) ||
        settings.minimumVertexArmWeight <= .5f || settings.minimumVertexArmWeight > 1.f) {
        result.status = FirstPersonArmsMeshStatus3D::InvalidSettings;
        return result;
    }
    const ArmChain3D left = ResolveArmChain(source, Side::Left);
    if (left.status != FirstPersonArmsMeshStatus3D::Complete) {
        result.status = left.status;
        return result;
    }
    const ArmChain3D right = ResolveArmChain(source, Side::Right);
    if (right.status != FirstPersonArmsMeshStatus3D::Complete) {
        result.status = right.status;
        return result;
    }
    result.leftArmJoints = left.joints;
    result.rightArmJoints = right.joints;

    std::vector<SkinnedSubmesh3D> sourceRanges = source.submeshes;
    if (sourceRanges.empty()) {
        sourceRanges.push_back({0, static_cast<std::uint32_t>(source.model.bindMesh.indices.size()), 0, 0});
    }
    std::size_t rangeCursor{};
    for (const SkinnedSubmesh3D& range : sourceRanges) {
        if (range.firstIndex != rangeCursor || range.indexCount == 0 || range.indexCount % 3 != 0 ||
            std::size_t(range.firstIndex) + range.indexCount > source.model.bindMesh.indices.size()) {
            result.status = FirstPersonArmsMeshStatus3D::InvalidSubmeshRange;
            return result;
        }
        rangeCursor += range.indexCount;
    }
    if (rangeCursor != source.model.bindMesh.indices.size()) {
        result.status = FirstPersonArmsMeshStatus3D::InvalidSubmeshRange;
        return result;
    }

    try {
        result.model.skeleton = source.model.skeleton;
        result.model.inverseBindMatrices = source.model.inverseBindMatrices;
        result.model.skeletonCompatibilityId = source.model.skeletonCompatibilityId;
        std::vector<std::uint32_t> remap(source.model.bindMesh.vertices.size(),
                                         std::numeric_limits<std::uint32_t>::max());
        auto appendVertex = [&](std::uint32_t sourceIndex) {
            std::uint32_t& mapped = remap[sourceIndex];
            if (mapped != std::numeric_limits<std::uint32_t>::max()) return mapped;
            mapped = static_cast<std::uint32_t>(result.model.bindMesh.vertices.size());
            result.model.bindMesh.vertices.push_back(source.model.bindMesh.vertices[sourceIndex]);
            result.model.influences.push_back(source.model.influences[sourceIndex]);
            if (!source.model.tangents.empty()) result.model.tangents.push_back(source.model.tangents[sourceIndex]);
            result.sourceVertexIndices.push_back(sourceIndex);
            return mapped;
        };

        for (const SkinnedSubmesh3D& sourceRange : sourceRanges) {
            const std::uint32_t firstOutput =
                static_cast<std::uint32_t>(result.model.bindMesh.indices.size());
            const std::size_t end = std::size_t(sourceRange.firstIndex) + sourceRange.indexCount;
            for (std::size_t offset = sourceRange.firstIndex; offset < end; offset += 3) {
                ++result.stats.sourceTriangles;
                const std::array<std::uint32_t, 3> triangle{
                    source.model.bindMesh.indices[offset], source.model.bindMesh.indices[offset + 1],
                    source.model.bindMesh.indices[offset + 2]};
                bool isLeft = true;
                bool isRight = true;
                for (const std::uint32_t vertex : triangle) {
                    if (vertex >= source.model.influences.size()) {
                        result.status = FirstPersonArmsMeshStatus3D::InvalidInfluences;
                        return result;
                    }
                    const SkinInfluences3D& influences = source.model.influences[vertex];
                    isLeft = isLeft && ArmWeight(influences, result.leftArmJoints) >=
                                           settings.minimumVertexArmWeight;
                    isRight = isRight && ArmWeight(influences, result.rightArmJoints) >=
                                             settings.minimumVertexArmWeight;
                }
                if (!isLeft && !isRight) {
                    ++result.stats.rejectedTriangles;
                    continue;
                }
                if (isLeft) ++result.stats.leftArmTriangles;
                else ++result.stats.rightArmTriangles;
                ++result.stats.retainedTriangles;
                for (const std::uint32_t vertex : triangle)
                    result.model.bindMesh.indices.push_back(appendVertex(vertex));
            }
            const std::uint32_t outputCount =
                static_cast<std::uint32_t>(result.model.bindMesh.indices.size()) - firstOutput;
            if (outputCount)
                result.submeshes.push_back(
                    {firstOutput, outputCount, sourceRange.materialSlot, sourceRange.flags});
        }
        if (!result.stats.leftArmTriangles) {
            result.status = FirstPersonArmsMeshStatus3D::EmptyLeftArm;
            return result;
        }
        if (!result.stats.rightArmTriangles) {
            result.status = FirstPersonArmsMeshStatus3D::EmptyRightArm;
            return result;
        }
        if (!result.stats.retainedTriangles || !result.model.Valid()) {
            result.status = FirstPersonArmsMeshStatus3D::EmptyResult;
            return result;
        }
        result.status = FirstPersonArmsMeshStatus3D::Complete;
        return result;
    } catch (const std::bad_alloc&) {
        result.status = FirstPersonArmsMeshStatus3D::AllocationFailure;
        return result;
    }
}
}
