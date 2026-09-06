#pragma once

#include "Aevrynd/MaterialLibrary3D.h"

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Aevrynd {

enum class SkinnedMaterialSidecarErrorCode3D {
    None,
    InvalidArgument,
    InputTooLarge,
    FileReadFailed,
    InvalidDocument,
    InvalidSchema,
    InvalidValue,
    PathOutsideAssetRoot,
    TextureFileMissing
};

struct SkinnedMaterialSidecarError3D {
    SkinnedMaterialSidecarErrorCode3D code{SkinnedMaterialSidecarErrorCode3D::None};
    std::size_t offset{};
    std::string message;

    explicit operator bool() const { return code != SkinnedMaterialSidecarErrorCode3D::None; }
};

struct SkinnedMaterialSidecarLimits3D {
    std::size_t maximumDocumentBytes{1024u * 1024u};
    std::size_t maximumMaterialSlots{256u};
    unsigned maximumTextureResolution{4096u};
    float maximumTextureScale{64.0f};
};

struct SkinnedMaterialSidecarResult3D {
    std::vector<MaterialAssetDefinition3D> materials;
    std::vector<bool> pbrOverrides;
    std::vector<bool> normalScaleOverrides;
    SkinnedMaterialSidecarError3D error;

    explicit operator bool() const { return !error; }

    // 只有未明確提供 sidecar pbr 的材質才沿用來源 PBR；結構不一致時完全不修改。
    bool PreserveSourcePbr(std::span<const Material3D> sourceMaterials) noexcept;
};

// 解析工作只建立定義；呼叫端可在全部成功後再交給 MaterialLibrary3D，避免部分載入。
SkinnedMaterialSidecarResult3D ParseSkinnedMaterialSidecar3D(
    std::string_view document,
    std::span<const std::string> materialSlotNames,
    const std::filesystem::path& assetRoot,
    const SkinnedMaterialSidecarLimits3D& limits = {});

SkinnedMaterialSidecarResult3D LoadSkinnedMaterialSidecarFile3D(
    const std::filesystem::path& sidecarPath,
    std::span<const std::string> materialSlotNames,
    const std::filesystem::path& assetRoot,
    const SkinnedMaterialSidecarLimits3D& limits = {});

} // 命名空間 Aevrynd
