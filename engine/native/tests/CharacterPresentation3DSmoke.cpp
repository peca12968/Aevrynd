#include <Aevrynd/CharacterPresentation3D.h>

#include <algorithm>
#include <chrono>
#include <cmath>
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

bool CheckEquipmentAttachment(const SkinnedModelAssetData& character,
                              const HumanoidSkeletonMap3D& rig,
                              const FallbackHumanoidAnimationSet3D& clips) {
    AnimationPose3D bindPose;
    for(const auto& joint:character.model.skeleton.joints)bindPose.localTransforms.push_back(joint.bindTransform);
    BuildAnimationPoseGlobalMatrices3D(character.model.skeleton,bindPose);
    const std::size_t handIndex=rig.Joint(HumanoidJoint3D::RightHand);
    const Matrix4 bindHand=bindPose.globalMatrices[handIndex];
    const Vec3 bindOrigin=TransformPoint(bindHand,{});
    // 真實男女骨架皆具有旋轉；此負案例必須分辨舊的「只加關節位置」錯誤。
    if(Length(TransformPoint(bindHand,{0,.105f,.02f})-(bindOrigin+Vec3{0,.105f,.02f}))<=.0001f)return false;
    SkinningWorkspace3D workspace;
    for(const auto shape:{CharacterEquipmentShape3D::Sword,CharacterEquipmentShape3D::Axe}){
        CharacterEquipmentAsset3D equipment;
        if(!CreateCharacterEquipmentAsset3D(character,rig,shape,equipment))return false;
        if(shape==CharacterEquipmentShape3D::Sword){
            if(!ValidateCharacterSwordGeometry3D(equipment))return false;
            auto emptyTopology=equipment;
            std::fill(emptyTopology.model.bindMesh.indices.begin(),emptyTopology.model.bindMesh.indices.end(),0u);
            if(ValidateCharacterSwordGeometry3D(emptyTopology))return false;
            auto degenerateTriangle=equipment;
            degenerateTriangle.model.bindMesh.indices[1]=degenerateTriangle.model.bindMesh.indices[0];
            if(ValidateCharacterSwordGeometry3D(degenerateTriangle))return false;
            auto reversedWinding=equipment;
            std::swap(reversedWinding.model.bindMesh.indices[0],reversedWinding.model.bindMesh.indices[1]);
            if(ValidateCharacterSwordGeometry3D(reversedWinding))return false;
            auto collapsedShoulder=equipment;
            collapsedShoulder.model.bindMesh.vertices[4]=collapsedShoulder.model.bindMesh.vertices[5];
            if(ValidateCharacterSwordGeometry3D(collapsedShoulder))return false;
            auto reversed=equipment;
            const Vec3 inward=TransformPoint(bindHand,{0,.08f,.02f});
            for(const auto index:reversed.swordGeometry.tipVertexIndices){auto& vertex=reversed.model.bindMesh.vertices[index];vertex.x=inward.x;vertex.y=inward.y;vertex.z=inward.z;}
            if(ValidateCharacterSwordGeometry3D(reversed))return false;
            auto invalidContract=equipment;invalidContract.swordGeometry.bladeTip.y=.65f;
            if(ValidateCharacterSwordGeometry3D(invalidContract))return false;
        }else if(equipment.swordGeometry.available||ValidateCharacterSwordGeometry3D(equipment))return false;
        const Vec3 localGrip=shape==CharacterEquipmentShape3D::Sword?Vec3{0,.105f,.02f}:Vec3{0,-.18f,.02f};
        std::set<std::uint32_t> gripIndices;
        for(const auto& range:equipment.submeshes)if(range.materialSlot==1)
            for(std::size_t offset=range.firstIndex;offset<std::size_t(range.firstIndex)+range.indexCount;++offset)
                gripIndices.insert(equipment.model.bindMesh.indices[offset]);
        if(gripIndices.size()!=(shape==CharacterEquipmentShape3D::Sword?34u:24u))return false;
        for(const auto& clip:clips.clips)for(float phase:{0.f,.47f,.91f}){
            AnimationPose3D equipmentPose;Mesh3D skinned;
            SampleAnimationClipInto(character.model.skeleton,clip,clip.duration*phase,equipmentPose);
            if(!SkinModelInto(equipment.model,equipmentPose,workspace,skinned))return false;
            Vec3 center{};
            for(const auto index:gripIndices){const auto& vertex=skinned.vertices[index];center=center+Vec3{vertex.x,vertex.y,vertex.z};}
            center=center*(1.f/static_cast<float>(gripIndices.size()));
            for(const Matrix4& world:{Matrix4::Identity(),CreateTransformMatrix({2,3,4},{.2f,-.4f,.1f},{1.2f,.8f,1.1f})}){
                const Vec3 actual=TransformPoint(world,center);
                const Vec3 expected=TransformPoint(world*equipmentPose.globalMatrices[handIndex],localGrip);
                const float error=Length(actual-expected);
                if(!std::isfinite(error)||error>.0001f){std::cerr<<"Equipment hand-local grip failed shape="<<unsigned(shape)<<" phase="<<phase<<" errorMeters="<<error<<'\n';return false;}
                if(shape==CharacterEquipmentShape3D::Sword){
                    const auto& geometry=equipment.swordGeometry;
                    const Matrix4 posedHand=world*equipmentPose.globalMatrices[handIndex];
                    const Vec3 expectedTip=TransformPoint(posedHand,geometry.bladeTip);
                    const Vec3 expectedBase=TransformPoint(posedHand,geometry.bladeBase);
                    for(const auto index:geometry.tipVertexIndices){
                        const auto& vertex=skinned.vertices[index];const Vec3 actualTip=TransformPoint(world,{vertex.x,vertex.y,vertex.z});
                        const float tipError=Length(actualTip-expectedTip);
                        if(!std::isfinite(tipError)||tipError>.0001f||Dot(actualTip-expectedBase,expectedBase-actual)<=0.f)return false;
                    }
                }
            }
        }
    }
    return true;
}

bool CheckCharacter(const std::filesystem::path& path, std::uint64_t expectedVertices,
                    std::uint64_t expectedIndices, std::size_t expectedJointCount,
                    bool requireMaleHairAtlas = false) {
    const auto loaded = LoadSkinnedModelAssetFileDetailed(path);
    if (!loaded || loaded.asset.model.bindMesh.vertices.size() != expectedVertices ||
        loaded.asset.model.bindMesh.indices.size() != expectedIndices ||
        loaded.asset.model.skeleton.joints.size() != expectedJointCount || loaded.asset.submeshes.size() != 9 ||
        loaded.asset.materialSlotNames.size() != 9) {
        std::cerr << "F2/M2 production asset contract failed: " << path
                  << " vertices=" << (loaded ? loaded.asset.model.bindMesh.vertices.size() : 0)
                  << " indices=" << (loaded ? loaded.asset.model.bindMesh.indices.size() : 0)
                  << " joints=" << (loaded ? loaded.asset.model.skeleton.joints.size() : 0)
                  << " submeshes=" << (loaded ? loaded.asset.submeshes.size() : 0)
                  << " materials=" << (loaded ? loaded.asset.materialSlotNames.size() : 0) << '\n';
        return false;
    }
    if (requireMaleHairAtlas) {
        const auto hairName = std::ranges::find(loaded.asset.materialSlotNames,
                                               std::string{"DarkBrownHairPBR"});
        if (hairName == loaded.asset.materialSlotNames.end()) return false;
        const std::uint32_t hairSlot = std::uint32_t(hairName - loaded.asset.materialSlotNames.begin());
        std::set<std::uint32_t> hairVertices;
        for (const auto& range : loaded.asset.submeshes) if (range.materialSlot == hairSlot)
            for (std::size_t offset = range.firstIndex;
                 offset < std::size_t(range.firstIndex) + range.indexCount; ++offset)
                hairVertices.insert(loaded.asset.model.bindMesh.indices[offset]);
        std::set<unsigned> strandTiles;
        std::size_t scalpBandVertices{}, strandVertices{};
        float maximumStrandU{};
        for (const std::uint32_t index : hairVertices) {
            const float u = loaded.asset.model.bindMesh.vertices[index].u;
            if (u <= .12501f) ++scalpBandVertices;
            if (u >= .24999f && u <= 1.00001f) {
                ++strandVertices;
                maximumStrandU = (std::max)(maximumStrandU, u);
                strandTiles.insert((std::min)(5u, unsigned((u - .25f) / .125f)));
            }
        }
        if (!scalpBandVertices || strandVertices < 300 || strandTiles.size() != 6 ||
            maximumStrandU < .99999f) {
            std::cerr << "M2 hair atlas contract failed: scalp=" << scalpBandVertices
                      << " strands=" << strandVertices << " tiles=" << strandTiles.size()
                      << " maxU=" << maximumStrandU << '\n';
            return false;
        }
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
        if (pose.globalMatrices.size() != expectedJointCount) return false;
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
    if(!CheckEquipmentAttachment(loaded.asset,rig,fallback))return false;
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
                        20174, 102180, 39) ||
        !CheckCharacter(assets / "model-packs" / "realistic-male" / "realistic-male.aeskin",
                        20066, 101688, 39, true)) return 2;
}
