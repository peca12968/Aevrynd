#pragma once
#include "AssetManifest.h"
#include "MaterialLibrary3D.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace Aevrynd {
enum class MaterialQualityTier3D:uint8_t {Low,Medium,High,Ultra};
struct PbrMaterialCookOptions3D {
    MaterialQualityTier3D quality{MaterialQualityTier3D::Ultra};
    size_t maximumResidentBytes{256*1024*1024};
    size_t maximumMaterials{1024};
};
enum class PbrMaterialCookErrorCode:uint8_t {
    None,InvalidOptions,InvalidManifest,MaterialLimitExceeded,InvalidMaterial,MissingTexture,
    TextureUsageMismatch,NormalConventionUnsupported,ResidentBudgetExceeded
};
struct PbrMaterialCookError3D {
    PbrMaterialCookErrorCode code{PbrMaterialCookErrorCode::None};
    size_t entryIndex{std::numeric_limits<size_t>::max()};
    std::string material;
    std::string message;
    explicit operator bool()const{return code!=PbrMaterialCookErrorCode::None;}
};
struct PbrMaterialCookPlan3D {
    std::vector<MaterialAssetDefinition3D> materials;
    size_t estimatedResidentBytes{};
    size_t referencedTextures{};
    size_t uniqueTextureSets{};
    uint32_t maximumResolution{};
};
class PbrMaterialCooker3D {
public:
    static uint32_t QualityMaximumResolution(MaterialQualityTier3D quality);
    static size_t EstimateBcMipBytes(uint32_t width,uint32_t height,uint32_t maximumResolution,size_t blockBytes);
    static bool Cook(const AssetManifest& manifest,PbrMaterialCookPlan3D& output,
        PbrMaterialCookError3D* error=nullptr,const PbrMaterialCookOptions3D& options={});
};
}
