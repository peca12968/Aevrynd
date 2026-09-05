#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Aevrynd {

enum class CharacterAssetProvenanceErrorCode3D : std::uint8_t {
    None,
    IoFailure,
    InputLimitExceeded,
    InvalidJson,
    InvalidSchema,
    InvalidPath,
    DuplicateIdentity,
    IntegrityMismatch,
    ConcurrentModification
};

enum class CharacterAssetProvenanceBarrier3D : std::uint8_t {
    RuntimeRootPinned,
    ModelPacksPinned,
    AssetPackPinned,
    CookedAssetPinned
};

using CharacterAssetProvenanceBarrierCallback3D =
    void (*)(CharacterAssetProvenanceBarrier3D, void*) noexcept;

struct CharacterAssetProvenanceError3D {
    CharacterAssetProvenanceErrorCode3D code{CharacterAssetProvenanceErrorCode3D::None};
    std::size_t offset{};
    std::string message;
    explicit operator bool() const noexcept { return code != CharacterAssetProvenanceErrorCode3D::None; }
};

struct CharacterAssetProvenanceTrustAnchor3D {
    std::uint64_t manifestBytes{};
    std::string manifestSha256;
};

struct CharacterAssetArtifactReceipt3D {
    std::string identity;
    std::string role;
    std::string path;
    std::uint64_t bytes{};
    std::string sha256;
    bool operator==(const CharacterAssetArtifactReceipt3D&) const = default;
};

struct CharacterAssetProvenanceReceipt3D {
    std::string assetId;
    std::string canonicalRuntimePath;
    std::string generatorId;
    std::string converterId;
    std::string skeletonIdentity;
    std::uint32_t jointCount{};
    std::vector<std::string> materials;
    std::vector<std::string> lods;
    std::vector<std::string> authoredAnimations;
    std::vector<std::string> runtimeFallbacks;
    std::vector<CharacterAssetArtifactReceipt3D> artifacts;
    std::string manifestSha256;
    std::uint64_t manifestBytes{};
    std::vector<std::byte> immutableCookedAeskin;
    bool passed{};
    bool operator==(const CharacterAssetProvenanceReceipt3D&) const = default;
};

struct CharacterAssetProvenanceLimits3D {
    std::size_t maximumManifestBytes{1024u * 1024u};
    std::size_t maximumStringBytes{4096u};
    std::size_t maximumCollectionEntries{1024u};
    std::uint64_t maximumArtifactBytes{64ull * 1024ull * 1024ull};
    CharacterAssetProvenanceBarrierCallback3D barrierCallback{};
    void* barrierContext{};
};

// 驗證期間拒絕任何寫入共享；只有整份 manifest 與所有 artifact 通過後才覆寫 output。
bool ValidateCharacterAssetProvenance3D(
    const std::filesystem::path& expectedRuntimeRoot,
    const std::filesystem::path& manifestPath,
    const CharacterAssetProvenanceTrustAnchor3D& trustAnchor,
    CharacterAssetProvenanceReceipt3D& output,
    CharacterAssetProvenanceError3D* error = nullptr,
    const CharacterAssetProvenanceLimits3D& limits = {});

} // namespace Aevrynd
