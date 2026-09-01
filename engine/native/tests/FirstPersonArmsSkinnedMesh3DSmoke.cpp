#include <Aevrynd/FirstPersonArmsSkinnedMesh3D.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>

using namespace Aevrynd;

namespace {
bool SameMatrices(const std::vector<Matrix4>& left, const std::vector<Matrix4>& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index)
        for (std::size_t row = 0; row < 4; ++row)
            for (std::size_t column = 0; column < 4; ++column)
                if (left[index][row][column] != right[index][row][column]) return false;
    return true;
}

float JointSetWeight(const SkinInfluences3D& influence,
                     const std::vector<std::uint16_t>& joints) {
    std::uint32_t total{};
    for (std::size_t slot = 0; slot < influence.weights.size(); ++slot)
        if (std::find(joints.begin(), joints.end(), influence.joints[slot]) != joints.end())
            total += influence.weights[slot];
    return static_cast<float>(total) / 65535.f;
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::vector<std::uint16_t> ForbiddenBodyJoints(const SkinnedModelAssetData& asset) {
    std::vector<std::uint16_t> result;
    for (std::size_t index = 0; index < asset.jointNames.size(); ++index) {
        const std::string name = Lower(asset.jointNames[index]);
        for (const std::string_view token :
             {"root", "hips", "leg", "foot", "spine", "chest", "neck", "head"}) {
            if (name.find(token) != std::string::npos) {
                result.push_back(static_cast<std::uint16_t>(index));
                break;
            }
        }
    }
    return result;
}

using Triangle = std::array<std::uint32_t, 3>;

Triangle Canonical(Triangle triangle) {
    std::sort(triangle.begin(), triangle.end());
    return triangle;
}

bool CheckAsset(const std::filesystem::path& path) {
    const SkinnedModelAssetDetailedResult loaded = LoadSkinnedModelAssetFileDetailed(path);
    if (!loaded) {
        std::cerr << "load: " << loaded.error << '\n';
        return false;
    }
    constexpr float Threshold = .55f;
    const FirstPersonArmsMeshResult3D derived =
        BuildFirstPersonArmsSkinnedMesh3D(loaded.asset, {Threshold});
    if (!derived || !derived.model.Valid() || derived.model.bindMesh.indices.empty() ||
        derived.model.bindMesh.vertices.empty() || derived.stats.leftArmTriangles == 0 ||
        derived.stats.rightArmTriangles == 0 ||
        derived.stats.retainedTriangles * 3 != derived.model.bindMesh.indices.size() ||
        derived.stats.sourceTriangles != loaded.asset.model.bindMesh.indices.size() / 3 ||
        derived.stats.rejectedTriangles + derived.stats.retainedTriangles !=
            derived.stats.sourceTriangles ||
        derived.model.bindMesh.vertices.size() >= loaded.asset.model.bindMesh.vertices.size() ||
        derived.model.skeleton.joints.size() != loaded.asset.model.skeleton.joints.size() ||
        !SameMatrices(derived.model.inverseBindMatrices, loaded.asset.model.inverseBindMatrices) ||
        derived.model.skeletonCompatibilityId != loaded.asset.model.skeletonCompatibilityId ||
        derived.sourceVertexIndices.size() != derived.model.bindMesh.vertices.size()) {
        std::cerr << "derived contract status=" << static_cast<unsigned>(derived.status) << '\n';
        return false;
    }

    std::uint32_t rangeCursor{};
    for (const SkinnedSubmesh3D& range : derived.submeshes) {
        if (range.firstIndex != rangeCursor || !range.indexCount || range.indexCount % 3 ||
            std::size_t(range.firstIndex) + range.indexCount >
                derived.model.bindMesh.indices.size() ||
            range.materialSlot >= loaded.asset.materialSlotNames.size())
            return false;
        rangeCursor += range.indexCount;
    }
    if (rangeCursor != derived.model.bindMesh.indices.size()) return false;

    const MeshBounds3D bounds = derived.model.bindMesh.Bounds();
    if (!std::isfinite(bounds.center.x) || !std::isfinite(bounds.center.y) ||
        !std::isfinite(bounds.center.z) || !std::isfinite(bounds.radius) ||
        !std::isfinite(bounds.extents.x) || !std::isfinite(bounds.extents.y) ||
        !std::isfinite(bounds.extents.z) || bounds.radius <= 0.f || bounds.extents.x <= 0.f ||
        bounds.extents.y <= 0.f || bounds.extents.z <= 0.f)
        return false;

    const std::vector<std::uint16_t> forbidden = ForbiddenBodyJoints(loaded.asset);
    std::set<Triangle> forbiddenSourceTriangles;
    const auto& sourceIndices = loaded.asset.model.bindMesh.indices;
    for (std::size_t offset = 0; offset < sourceIndices.size(); offset += 3) {
        bool allForbidden = true;
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const SkinInfluences3D& influence =
                loaded.asset.model.influences[sourceIndices[offset + corner]];
            allForbidden = allForbidden && JointSetWeight(influence, forbidden) >= Threshold;
        }
        if (allForbidden)
            forbiddenSourceTriangles.insert(Canonical(
                {sourceIndices[offset], sourceIndices[offset + 1], sourceIndices[offset + 2]}));
    }
    if (forbiddenSourceTriangles.empty()) return false;

    for (std::size_t offset = 0; offset < derived.model.bindMesh.indices.size(); offset += 3) {
        bool left = true;
        bool right = true;
        Triangle original{};
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const std::uint32_t compact = derived.model.bindMesh.indices[offset + corner];
            if (compact >= derived.model.influences.size()) return false;
            const SkinInfluences3D& influence = derived.model.influences[compact];
            left = left && JointSetWeight(influence, derived.leftArmJoints) >= Threshold;
            right = right && JointSetWeight(influence, derived.rightArmJoints) >= Threshold;
            original[corner] = derived.sourceVertexIndices[compact];
        }
        if ((!left && !right) || forbiddenSourceTriangles.contains(Canonical(original))) return false;
    }

    SkinnedModelAssetData missing = loaded.asset;
    const auto hand = std::find(missing.jointNames.begin(), missing.jointNames.end(), "LeftHand");
    if (hand == missing.jointNames.end()) return false;
    *hand = "MissingRequiredJoint";
    const FirstPersonArmsMeshResult3D rejected = BuildFirstPersonArmsSkinnedMesh3D(missing);
    if (rejected.status != FirstPersonArmsMeshStatus3D::MissingJoint || rejected.Complete())
        return false;

    std::cout << path.filename().string() << " sourceTriangles=" << derived.stats.sourceTriangles
              << " retained=" << derived.stats.retainedTriangles
              << " left=" << derived.stats.leftArmTriangles
              << " right=" << derived.stats.rightArmTriangles
              << " vertices=" << derived.model.bindMesh.vertices.size() << '\n';
    return true;
}
}

int main() {
    const std::filesystem::path root{AEVRYND_SOURCE_DIR};
    const std::filesystem::path assets = root / "games" / "character_lab" / "assets" /
                                         "model-packs";
    if (!CheckAsset(assets / "realistic-female" / "realistic-female.aeskin")) return 1;
    if (!CheckAsset(assets / "realistic-male" / "realistic-male.aeskin")) return 2;
    return 0;
}
