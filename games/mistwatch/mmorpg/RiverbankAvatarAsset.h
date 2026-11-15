#pragma once

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

    // 目前 GPU 蒙皮支援不透明與遮罩幾何。保留虹膜，先略過兩層半透明反射網格。
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

inline std::vector<Aevrynd::Material3D> RiverbankAvatarMaterials(const Aevrynd::SkinnedModelAssetData& asset) {
    using namespace Aevrynd;
    using Shading = CharacterShadingModel3D;
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
        if (name.starts_with("AX textile | ")) material.shadingModel = Shading::Cloth;
        if (name == "AX owned | Iris radial pigment" || name == "AX owned | Sclera") material.shadingModel = Shading::Eye;
        materials.push_back(material);
    }
    return materials;
}
}
