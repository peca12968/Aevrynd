#pragma once

#include "Aevrynd/MaterialLibrary3D.h"
#include "Aevrynd/SkeletonSemantics3D.h"
#include "Aevrynd/SkinnedModelAsset.h"

#include <array>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace Aevrynd {

enum class CharacterEquipmentShape3D : std::uint8_t { Shoulder, Sword, Axe };

struct CharacterEquipmentAsset3D {
    SkinnedModel3D model{};
    std::vector<SkinnedSubmesh3D> submeshes{};
    // 0 為金屬，1 為皮革；材質由角色自己的 CC0 sidecar 提供。
    std::array<std::uint32_t, 2> materialSlots{0u, 1u};
};

enum class FallbackHumanoidClip3D : std::uint8_t {
    Idle,
    Walk,
    Run,
    Crouch,
    Jump,
    Fall,
    Attack1,
    Attack2,
    Attack3,
    Hit,
    Death,
    Count
};

struct FallbackHumanoidAnimationSet3D {
    std::array<AnimationClip3D, std::size_t(FallbackHumanoidClip3D::Count)> clips{};
};

// 完整批次載入 AESKIN 對應的 PBR sidecar；失敗會關閉 library 並保持 output 不變。
bool LoadCharacterMaterialBindings3D(
    Engine& engine,
    MaterialLibrary3D& library,
    const SkinnedModelAssetData& asset,
    const std::filesystem::path& materialRoot,
    const std::filesystem::path& sidecarPath,
    std::vector<Material3D>& output,
    std::string* error = nullptr);

bool ResolveCharacterRig3D(
    const SkinnedModelAssetData& asset,
    HumanoidSkeletonMap3D& output,
    SkeletonSemanticError3D* error = nullptr) noexcept;

bool CreateFallbackHumanoidAnimationSet3D(
    const SkinnedModelAssetData& asset,
    const HumanoidSkeletonMap3D& rig,
    FallbackHumanoidAnimationSet3D& output,
    std::string* error = nullptr);

bool CreateCharacterEquipmentAsset3D(
    const SkinnedModelAssetData& character,
    const HumanoidSkeletonMap3D& rig,
    CharacterEquipmentShape3D shape,
    CharacterEquipmentAsset3D& output,
    std::string* error = nullptr);

std::vector<float> CreateCharacterUpperBodyMask3D(
    const Skeleton3D& skeleton,
    const HumanoidSkeletonMap3D& rig);

// socket 使用骨骼樞紐而非掃描蒙皮頂點，確保不同 LOD 仍有一致的 attachment。
bool CharacterSocketTransform3D(
    const Skeleton3D& skeleton,
    const HumanoidSkeletonMap3D& rig,
    HumanoidJoint3D semantic,
    const AnimationPose3D& pose,
    Matrix4 modelToWorld,
    Matrix4& output) noexcept;

} // 命名空間 Aevrynd
