#pragma once

#include "MaterialLibrary3D.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Aevrynd {

enum class CookedRenderManifestErrorCode3D : std::uint8_t {
    None,
    IoFailure,
    InputLimitExceeded,
    InvalidJson,
    InvalidSchema,
    InvalidPath,
    DuplicateEntry,
    InvalidSha256,
    FileMissing,
    FileSizeMismatch,
    FileHashMismatch,
    ReferenceMissing,
    UnreferencedFile
};

struct CookedRenderManifestError3D {
    CookedRenderManifestErrorCode3D code{CookedRenderManifestErrorCode3D::None};
    std::size_t offset{};
    std::string message;
    explicit operator bool() const noexcept {
        return code != CookedRenderManifestErrorCode3D::None;
    }
};

struct CookedRenderManifestLimits3D {
    std::size_t maximumJsonBytes{4u * 1024u * 1024u};
    std::size_t maximumFiles{4096};
    std::size_t maximumMaterials{1024};
    std::size_t maximumMeshParts{1024};
    std::size_t maximumStringBytes{4096};
    std::uint64_t maximumFileBytes{512ull * 1024ull * 1024ull};
    std::uint64_t maximumAggregateBytes{2ull * 1024ull * 1024ull * 1024ull};
};

struct CookedRenderFile3D {
    std::string path;
    std::string sha256;
    std::uint64_t bytes{};
};

struct CookedRenderMaterial3D {
    std::string id;
    std::string assetId;
    std::string slot;
    std::string baseColorPath;
    std::string surfacePath;
    std::string normalPath;
    std::string baseColorSha256;
    std::string surfaceSha256;
    std::string normalSha256;
    std::uint64_t baseColorBytes{}, surfaceBytes{}, normalBytes{};
    MaterialSurfaceMapEncoding3D surfaceEncoding{
        MaterialSurfaceMapEncoding3D::PackedOrm};
    MaterialAlphaMode3D alphaMode{MaterialAlphaMode3D::Opaque};

    [[nodiscard]] MaterialAssetDefinition3D Definition(
        std::uint32_t maximumResolution = 4096) const;
};

struct CookedRenderMeshPart3D {
    std::string id;
    std::string assetId;
    std::string materialId;
    std::array<std::string, 3> lodPaths{};
};

struct CookedRenderIbl3D {
    std::string specularPath;
    std::string irradiancePath;
    std::string brdfLutPath;
};

struct CookedRenderManifestStats3D {
    std::size_t files{}, materials{}, meshParts{}, referencedFiles{};
    std::uint64_t aggregateBytes{}, fingerprint{};
};

class CookedRenderManifest3D {
public:
    bool LoadFile(const std::filesystem::path &manifestPath,
                  CookedRenderManifestError3D *error = nullptr,
                  const CookedRenderManifestLimits3D &limits = {});
    bool LoadJson(std::string_view json, const std::filesystem::path &assetRoot,
                  CookedRenderManifestError3D *error = nullptr,
                  const CookedRenderManifestLimits3D &limits = {});

    [[nodiscard]] const CookedRenderFile3D *
    FindFile(std::string_view path) const noexcept;
    [[nodiscard]] const CookedRenderMaterial3D *
    FindMaterial(std::string_view id) const noexcept;
    [[nodiscard]] const CookedRenderMeshPart3D *
    FindMeshPart(std::string_view id) const noexcept;
    [[nodiscard]] std::span<const CookedRenderFile3D> Files() const noexcept {
        return files_;
    }
    [[nodiscard]] std::span<const CookedRenderMaterial3D>
    Materials() const noexcept {
        return materials_;
    }
    [[nodiscard]] std::span<const CookedRenderMeshPart3D>
    MeshParts() const noexcept {
        return meshParts_;
    }
    [[nodiscard]] const CookedRenderIbl3D &Ibl() const noexcept { return ibl_; }
    [[nodiscard]] const CookedRenderManifestStats3D &Stats() const noexcept {
        return stats_;
    }
    [[nodiscard]] const std::filesystem::path &AssetRoot() const noexcept {
        return assetRoot_;
    }
    void Clear() noexcept;

private:
    std::filesystem::path assetRoot_;
    std::vector<CookedRenderFile3D> files_;
    std::vector<CookedRenderMaterial3D> materials_;
    std::vector<CookedRenderMeshPart3D> meshParts_;
    CookedRenderIbl3D ibl_{};
    CookedRenderManifestStats3D stats_{};
    // 驗證後持續拒絕 payload 寫入/刪除，直到 manifest Clear/析構。
    std::vector<std::shared_ptr<void>> pinnedFiles_{};
};

} // namespace Aevrynd
