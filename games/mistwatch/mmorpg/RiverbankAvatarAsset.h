#pragma once

#include "RiverbankClass.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <limits>
#include <stdexcept>

namespace Mistwatch {
inline constexpr std::string_view RiverbankBootLeather = "AX footwear | Oiled umber leather";
inline constexpr std::string_view RiverbankBootSole = "AX footwear | Vulcanized sole";

inline bool HasRiverbankBoots(const Aevrynd::SkinnedModelAssetData& asset) {
    return std::ranges::find(asset.materialSlotNames, RiverbankBootSole) != asset.materialSlotNames.end();
}
inline Aevrynd::SkinnedModelAssetData LoadRiverbankAvatarAsset(const std::filesystem::path& path) {
    using namespace Aevrynd;
    auto loaded = LoadSkinnedModelAssetFileDetailed(path);
    if (!loaded) throw std::runtime_error("Riverbank avatar load failed: " + loaded.error);
    auto& asset = loaded.asset;
    const bool boots = HasRiverbankBoots(asset);
    if (boots) {
        for (const auto label : {RiverbankBootLeather, RiverbankBootSole}) {
            const auto material = std::ranges::find(asset.materialSlotNames, label);
            if (material == asset.materialSlotNames.end())
                throw std::runtime_error("Riverbank boot material missing");
            const auto slot = static_cast<std::size_t>(material - asset.materialSlotNames.begin());
            if (asset.materials[slot].alphaMode != SkinnedMaterialAlphaMode3D::Opaque ||
                std::ranges::count(asset.submeshes, slot, &SkinnedSubmesh3D::materialSlot) != 2)
                throw std::runtime_error("Riverbank boot parts differ from the verified export");
        }
    }
    if (asset.submeshes.size() != (boots ? 24u : 20u) || asset.materials.size() != (boots ? 16u : 14u))
        throw std::runtime_error("Riverbank avatar model and material set differ from the verified export");

    // 相同材質的部位共用角色骨架、變換和繪製設定，併為連續索引範圍。
    // 目前保留虹膜，略過 GPU 骨架路徑尚未支援的兩個角膜反射表面。
    const auto& sourceIndices = asset.model.bindMesh.indices;
    std::vector<std::uint32_t> drawIndices;
    std::vector<SkinnedSubmesh3D> drawParts;
    drawIndices.reserve(sourceIndices.size());
    drawParts.reserve(asset.materials.size());
    std::size_t omittedSubmeshes = 0;
    for (std::uint32_t slot = 0; slot < asset.materials.size(); ++slot) {
        if (asset.materials[slot].alphaMode == SkinnedMaterialAlphaMode3D::Blend) {
            if (asset.materialSlotNames[slot] != "AX owned | Corneal Fresnel reflection")
                throw std::runtime_error("Unexpected transparent material on Riverbank avatar");
            omittedSubmeshes += std::ranges::count(asset.submeshes, slot, &SkinnedSubmesh3D::materialSlot);
            continue;
        }
        SkinnedSubmesh3D group{static_cast<std::uint32_t>(drawIndices.size()), 0, slot, 0};
        for (const auto& part : asset.submeshes) {
            if (part.materialSlot != slot) continue;
            if (group.indexCount == 0) group.flags = part.flags;
            if (group.flags != part.flags) throw std::runtime_error("Riverbank avatar material parts use incompatible flags");
            const auto start = sourceIndices.begin() + part.firstIndex;
            drawIndices.insert(drawIndices.end(), start, start + part.indexCount);
            group.indexCount += part.indexCount;
        }
        if (!group.indexCount) throw std::runtime_error("Riverbank avatar material has no geometry");
        drawParts.push_back(group);
    }
    if (omittedSubmeshes != 2 || drawParts.size() != (boots ? 15u : 13u))
        throw std::runtime_error("Riverbank avatar eye surface inventory changed");
    asset.model.bindMesh.indices = std::move(drawIndices);
    asset.submeshes = std::move(drawParts);
    return std::move(asset);
}

inline constexpr std::string_view RiverbankVestMaterial = "AX textile | Warm woven vest";

struct RiverbankAvatarMeshPart {
    Aevrynd::SkinnedModel3D model;
    std::vector<Aevrynd::SkinnedSubmesh3D> submeshes;
};

// 共用骨架及權重。服裝版本只提供該材質使用的頂點與切線。
inline RiverbankAvatarMeshPart CompactRiverbankAvatarPart(
    const Aevrynd::SkinnedModelAssetData& base, const Aevrynd::SkinnedModel3D& appearance, bool vest) {
    RiverbankAvatarMeshPart part;
    part.model.skeleton = base.model.skeleton;
    part.model.inverseBindMatrices = base.model.inverseBindMatrices;
    part.model.skeletonCompatibilityId = base.model.skeletonCompatibilityId;
    const auto& indices = base.model.bindMesh.indices;
    constexpr auto missing = (std::numeric_limits<std::uint32_t>::max)();
    std::vector<std::uint32_t> remap(base.model.bindMesh.vertices.size(), missing);
    for (const auto& surface : base.submeshes) {
        if ((base.materialSlotNames.at(surface.materialSlot) == RiverbankVestMaterial) != vest) continue;
        auto range = surface;
        range.firstIndex = static_cast<std::uint32_t>(part.model.bindMesh.indices.size());
        part.submeshes.push_back(range);
        for (std::size_t offset = 0; offset < surface.indexCount; ++offset) {
            const auto index = indices.at(surface.firstIndex + offset);
            auto& compact = remap.at(index);
            if (compact == missing) {
                compact = static_cast<std::uint32_t>(part.model.bindMesh.vertices.size());
                part.model.bindMesh.vertices.push_back(appearance.bindMesh.vertices.at(index));
                part.model.influences.push_back(base.model.influences.at(index));
                if (!appearance.tangents.empty()) part.model.tangents.push_back(appearance.tangents.at(index));
            }
            part.model.bindMesh.indices.push_back(compact);
        }
    }
    if (part.submeshes.empty() || (vest && part.submeshes.size() != 1))
        throw std::runtime_error("Riverbank avatar material partition is incomplete");
    return part;
}

inline std::vector<Aevrynd::Material3D> RiverbankAvatarMaterials(
    const Aevrynd::SkinnedModelAssetData& asset, RiverbankClass playerClass = RiverbankClass::None) {
    using namespace Aevrynd;
    using Shading = CharacterShadingModel3D;
    // G3 與 P3 服裝的職業配色。只改衣料底色，同一角色的膚色、髮色與其他材質沿用匯出資產。
    static constexpr std::array fabricSlots{
        std::string_view{"AX textile | Cotton knit"}, std::string_view{"AX textile | Warm woven vest"},
        std::string_view{"AX textile | Indigo twill"}};
    static constexpr std::array outfitColors{
        std::array{Color{.052f, .063f, .077f, 1.f}, Color{.30f, .073f, .044f, 1.f}, Color{.052f, .062f, .075f, 1.f}},
        std::array{Color{.071f, .10f, .054f, 1.f}, Color{.091f, .20f, .085f, 1.f}, Color{.058f, .078f, .047f, 1.f}},
        std::array{Color{.106f, .094f, .089f, 1.f}, Color{.56f, .46f, .29f, 1.f}, Color{.115f, .075f, .105f, 1.f}},
    };
    std::vector<Material3D> materials;
    materials.reserve(asset.materials.size());
    for (std::size_t slot = 0; slot < asset.materials.size(); ++slot) {
        const auto& source = asset.materials[slot];
        const auto& name = asset.materialSlotNames[slot];
        Material3D material;
        material.tint = source.baseColorFactor;
        material.surface = {source.roughnessFactor, source.metallicFactor, 0.f, source.occlusionStrength};
        material.alphaMode = source.alphaMode == SkinnedMaterialAlphaMode3D::Blend ? MaterialAlphaMode3D::Blend :
            source.alphaMode == SkinnedMaterialAlphaMode3D::Mask ? MaterialAlphaMode3D::Mask : MaterialAlphaMode3D::Opaque;
        material.alphaCutoff = source.alphaCutoff;
        material.doubleSided = source.doubleSided;
        material.castsShadow = source.alphaMode != SkinnedMaterialAlphaMode3D::Blend;
        if (name == "AX owned | Orbital skin" || name == "AX owned | Inner lid wet margin") material.shadingModel = Shading::Skin;
        if (name == "AX owned | Chestnut keratin" || name == "AX owned | Lash keratin") material.shadingModel = Shading::Hair;
        if (name.starts_with("AX textile | ")) {
            material.shadingModel = Shading::Cloth;
            const auto fabric = std::ranges::find(fabricSlots, name);
            if (fabric != fabricSlots.end() && IsRiverbankClass(playerClass)) {
                const auto role = static_cast<std::size_t>(playerClass) - static_cast<std::size_t>(RiverbankClass::Guardian);
                material.tint = outfitColors.at(role).at(static_cast<std::size_t>(fabric - fabricSlots.begin()));
            }
        }
        if (name == "AX owned | Iris radial pigment" || name == "AX owned | Sclera") material.shadingModel = Shading::Eye;
        materials.push_back(material);
    }
    return materials;
}
}
