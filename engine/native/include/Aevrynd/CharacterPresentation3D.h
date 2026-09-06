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

// 正式劍的手部局部座標，單位為公尺；握柄與刀刃不可混用。
struct CharacterSwordGeometry3D {
    Vec3 grip{0.f,.105f,.02f};
    Vec3 handleBladeEnd{0.f,.045f,.02f};
    Vec3 handlePommelEnd{0.f,.165f,.02f};
    Vec3 bladeBase{0.f,.08f,.02f};
    Vec3 bladeTip{0.f,-.48f,.02f};
    // 四個刃面共用同一個實際尖端頂點；保留四項讓既有逐刃面驗證介面不變。
    std::array<std::uint32_t,4> tipVertexIndices{16u,16u,16u,16u};
    bool available{};
};

struct CharacterEquipmentAsset3D {
    SkinnedModel3D model{};
    std::vector<SkinnedSubmesh3D> submeshes{};
    // 0 為金屬，1 為皮革；材質由角色自己的 CC0 sidecar 提供。
    std::array<std::uint32_t, 2> materialSlots{0u, 1u};
    CharacterSwordGeometry3D swordGeometry{};
};

// 核對實際 bind 頂點與單一手部蒙皮權重；其他裝備不宣告劍的語意。
bool ValidateCharacterSwordGeometry3D(const CharacterEquipmentAsset3D& equipment) noexcept;

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
