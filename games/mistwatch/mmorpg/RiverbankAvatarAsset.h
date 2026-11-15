#pragma once

#include "RiverbankClass.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <stdexcept>

namespace Mistwatch {
inline Aevrynd::SkinnedModelAssetData LoadRiverbankAvatarAsset(const std::filesystem::path& path) {
    using namespace Aevrynd;
    auto loaded = LoadSkinnedModelAssetFileDetailed(path);
    if (!loaded) throw std::runtime_error("Riverbank avatar load failed: " + loaded.error);
    auto& asset = loaded.asset;
    if (asset.submeshes.size() != 20 || asset.materials.size() != 14)
        throw std::runtime_error("Riverbank avatar model and material set differ from the verified export");

    // The current GPU skin path supports opaque and masked geometry. Preserve the iris and omit only
    // the two translucent reflection layers from the draw mesh until the renderer supports them.
    const auto& sourceIndices = asset.model.bindMesh.indices;
    std::vector<std::uint32_t> drawIndices;
    std::vector<SkinnedSubmesh3D> drawParts;
    drawIndices.reserve(sourceIndices.size());
    drawParts.reserve(asset.submeshes.size());
    for (auto part : asset.submeshes) {
        if (asset.materials[part.materialSlot].alphaMode == SkinnedMaterialAlphaMode3D::Blend) {
            if (asset.materialSlotNames[part.materialSlot] != "AX owned | Corneal Fresnel reflection")
                throw std::runtime_error("Unexpected transparent material on Riverbank avatar");
            continue;
        }
        const auto start = sourceIndices.begin() + part.firstIndex;
        part.firstIndex = static_cast<std::uint32_t>(drawIndices.size());
        drawIndices.insert(drawIndices.end(), start, start + part.indexCount);
        drawParts.push_back(part);
    }
    if (drawParts.size() != 18) throw std::runtime_error("Riverbank avatar eye surface inventory changed");
    asset.model.bindMesh.indices = std::move(drawIndices);
    asset.submeshes = std::move(drawParts);
    return std::move(asset);
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
