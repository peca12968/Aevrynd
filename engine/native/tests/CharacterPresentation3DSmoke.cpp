#include <Aevrynd/CharacterPresentation3D.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <set>

using namespace Aevrynd;

namespace {
std::uint64_t HashPose(const AnimationPose3D& pose) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const Matrix4& matrix : pose.globalMatrices) {
        for (const auto& row : matrix.values) {
            for (float value : row) {
                const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
                for (std::size_t index = 0; index < sizeof(value); ++index) {
                    hash ^= bytes[index];
                    hash *= 1099511628211ull;
                }
            }
        }
    }
    return hash;
}

bool CheckCharacter(const std::filesystem::path& path, std::uint64_t expectedVertices,
                    std::uint64_t expectedIndices) {
    const auto loaded = LoadSkinnedModelAssetFileDetailed(path);
    constexpr std::size_t ExpectedJointCount = 24;
    if (!loaded || loaded.asset.model.bindMesh.vertices.size() != expectedVertices ||
        loaded.asset.model.bindMesh.indices.size() != expectedIndices ||
        loaded.asset.model.skeleton.joints.size() != ExpectedJointCount || loaded.asset.submeshes.size() != 9 ||
        loaded.asset.materialSlotNames.size() != 9) {
        std::cerr << "F2/M2 production asset contract failed: " << path << '\n';
        return false;
    }
    HumanoidSkeletonMap3D rig;
    FallbackHumanoidAnimationSet3D fallback;
    std::string error;
    if (!ResolveCharacterRig3D(loaded.asset, rig) ||
        !CreateFallbackHumanoidAnimationSet3D(loaded.asset, rig, fallback, &error)) {
        std::cerr << "Character semantic/fallback contract failed: " << error << '\n';
        return false;
    }
    std::set<std::uint64_t> hashes;
    AnimationPose3D pose;
    for (const AnimationClip3D& clip : fallback.clips) {
        SampleAnimationClipInto(loaded.asset.model.skeleton, clip, clip.duration * .47f, pose);
        if (pose.globalMatrices.size() != ExpectedJointCount) return false;
        hashes.insert(HashPose(pose));
    }
    if (hashes.size() != fallback.clips.size()) {
        std::cerr << "Fallback character poses were not semantically distinct\n";
        return false;
    }
    CharacterEquipmentAsset3D sword;
    if (!CreateCharacterEquipmentAsset3D(loaded.asset, rig, CharacterEquipmentShape3D::Sword,
                                         sword, &error) ||
        sword.submeshes.size() != 2 || sword.materialSlots != std::array<std::uint32_t, 2>{0, 1}) {
        std::cerr << "Sword PBR equipment contract failed: " << error << '\n';
        return false;
    }
    std::uint32_t covered{};
    for (const SkinnedSubmesh3D& range : sword.submeshes) covered += range.indexCount;
    if (covered != sword.model.bindMesh.indices.size()) return false;
    Matrix4 socket = CreateScaleMatrix({7, 7, 7});
    if (!CharacterSocketTransform3D(loaded.asset.model.skeleton, rig,
                                    HumanoidJoint3D::RightHand, pose,
                                    CreateTranslationMatrix({2, 3, 4}), socket)) return false;
    const Matrix4 sentinel = socket;
    AnimationPose3D invalid = pose;
    invalid.globalMatrices.pop_back();
    bool socketChanged{};
    if (CharacterSocketTransform3D(loaded.asset.model.skeleton, rig,
                                   HumanoidJoint3D::RightHand, invalid,
                                   Matrix4::Identity(), socket)) socketChanged = true;
    for (std::size_t row = 0; row < 4; ++row) for (std::size_t column = 0; column < 4; ++column)
        socketChanged = socketChanged || socket[row][column] != sentinel[row][column];
    if (socketChanged) {
        std::cerr << "Semantic socket failure was not atomic\n";
        return false;
    }
    constexpr std::size_t Iterations = 10000;
    std::vector<long long> samples;
    samples.reserve(Iterations);
    for (std::size_t iteration = 0; iteration < Iterations; ++iteration) {
        const auto start = std::chrono::steady_clock::now();
        SampleAnimationClipInto(loaded.asset.model.skeleton,
                                fallback.clips[iteration % fallback.clips.size()],
                                float(iteration % 100) * .007f, pose);
        samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start).count());
    }
    std::sort(samples.begin(), samples.end());
    const auto p95 = samples[(samples.size() * 95 + 99) / 100 - 1];
    if (p95 > 500000) {
        std::cerr << "Fallback animation p95 gate failed: " << p95 << "ns\n";
        return false;
    }
    std::cout << path.filename().string() << " slots=9 clips=11 swordRanges=2 p95Ns=" << p95 << '\n';
    return true;
}
}

int main() {
    const std::filesystem::path root{AEVRYND_SOURCE_DIR};
    const auto assets = root / "games" / "character_lab" / "assets";
    if (!std::filesystem::is_regular_file(assets / "model-packs" / "CC0-HUMAN-BASE-MESHES.md") ||
        !std::filesystem::is_regular_file(assets / "material-packs" / "original" / "LICENSE.txt") ||
        !std::filesystem::is_regular_file(assets / "material-packs" / "original" / "manifest.json")) {
        std::cerr << "CC0 character runtime provenance is incomplete\n";
        return 1;
    }
    if (!CheckCharacter(assets / "model-packs" / "realistic-female" / "realistic-female.aeskin",
                        20174, 102180) ||
        !CheckCharacter(assets / "model-packs" / "realistic-male" / "realistic-male.aeskin",
                        20066, 101688)) return 2;
}
