#pragma once

#include "AssetManifest.h"
#include "PbrMaterialCooker3D.h"
#include "PhotorealAssetProfile3D.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Aevrynd {

enum class PhotorealMaterialImportErrorCode3D : std::uint8_t {
    None, InputLimitExceeded, InvalidJson, InvalidSchema, RuntimeManifestRejected, AssetNotFound,
    LicenseRejected, UnsafePath, FileMissing, FileHashMismatch, UnsupportedImage, ChannelMismatch,
    NormalConventionUnsupported, ManifestRejected, CookRejected
};

struct PhotorealMaterialImportError3D {
    PhotorealMaterialImportErrorCode3D code{PhotorealMaterialImportErrorCode3D::None};
    std::size_t offset{};
    std::string message;
    explicit operator bool() const noexcept { return code != PhotorealMaterialImportErrorCode3D::None; }
};

enum class PhotorealTextureBakeOperation3D : std::uint8_t {
    BaseColorSrgb, InvertNormalGreenToDirectX, PreservePackedArm,
    PackAmbientOcclusionRoughnessMetallicScalar
};

struct PhotorealTextureBakeRequest3D {
    std::string sourcePath;
    std::string sourceSha256;
    std::string secondarySourcePath;
    std::string secondarySourceSha256;
    std::string outputPath;
    std::string outputAssetId;
    TextureUsage usage{TextureUsage::BaseColor};
    PhotorealTextureBakeOperation3D operation{PhotorealTextureBakeOperation3D::BaseColorSrgb};
    std::uint32_t width{};
    std::uint32_t height{};
    std::string blockFormat;
    float metallicScalar{};
    bool completeMipChain{true};
};

struct PhotorealMaterialImportPlan3D {
    std::string assetId;
    AssetManifest manifest;
    PbrMaterialCookPlan3D cookPlan;
    std::vector<PhotorealTextureBakeRequest3D> textureBakes;
    std::size_t verifiedSourceBytes{};
};

struct PhotorealMaterialImportLimits3D {
    std::size_t maximumJsonBytes{256u * 1024u};
    std::size_t maximumMaterials{64};
    std::size_t maximumSourceBytes{256u * 1024u * 1024u};
    std::uint32_t maximumTextureDimension{4096};
    std::size_t maximumResidentBytes{256u * 1024u * 1024u};
};

class PhotorealMaterialImport3D {
public:
    static bool BuildPlan(const PhotorealRuntimeManifest3D& runtimeManifest, std::string_view assetId,
        const std::filesystem::path& sidecarPath,
        PhotorealMaterialImportPlan3D& output, PhotorealMaterialImportError3D* error = nullptr,
        const PhotorealMaterialImportLimits3D& limits = {});
};

} // namespace Aevrynd
