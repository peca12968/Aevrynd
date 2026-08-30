#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Aevrynd {

enum class PhotorealAssetErrorCode3D : std::uint8_t {
    None,
    IoFailure,
    InputLimitExceeded,
    InvalidJson,
    InvalidSchema,
    InvalidCatalog,
    DuplicateAsset,
    LicenseRejected,
    InvalidReceipt,
    ReceiptMismatch,
    RuntimeManifestRejected,
    RuntimeAssetIncomplete,
    MaterialChannelsIncomplete,
    InvalidSelection,
    AssetNotFound,
    AlreadyResident,
    CpuBudgetExceeded,
    GpuBudgetExceeded
};

struct PhotorealAssetError3D {
    PhotorealAssetErrorCode3D code{PhotorealAssetErrorCode3D::None};
    std::size_t offset{};
    std::string message;
    explicit operator bool()const noexcept{return code!=PhotorealAssetErrorCode3D::None;}
};

enum class PhotorealTextureTier3D : std::uint8_t {Far512,General1K,Near2K,Hero4K};
enum class PhotorealImportance3D : std::uint8_t {Background,Standard,Important,Hero};
enum class PhotorealAssetKind3D : std::uint8_t {PbrMaterial,MeshPbr,Hdri,Unknown};

enum class PhotorealMaterialChannel3D : std::uint32_t {
    None=0,
    BaseColor=1u<<0,
    NormalDx=1u<<1,
    NormalGl=1u<<2,
    Roughness=1u<<3,
    Metallic=1u<<4,
    AmbientOcclusion=1u<<5,
    Displacement=1u<<6,
    Emissive=1u<<7,
    Opacity=1u<<8,
    PackedArm=1u<<9
};

constexpr PhotorealMaterialChannel3D operator|(PhotorealMaterialChannel3D left,PhotorealMaterialChannel3D right)noexcept{
    return static_cast<PhotorealMaterialChannel3D>(static_cast<std::uint32_t>(left)|static_cast<std::uint32_t>(right));
}
constexpr PhotorealMaterialChannel3D operator&(PhotorealMaterialChannel3D left,PhotorealMaterialChannel3D right)noexcept{
    return static_cast<PhotorealMaterialChannel3D>(static_cast<std::uint32_t>(left)&static_cast<std::uint32_t>(right));
}

struct PhotorealCatalogAsset3D {
    std::string id;
    std::string source;
    std::string category;
    PhotorealAssetKind3D kind{PhotorealAssetKind3D::Unknown};
    std::string officialPage;
    std::string preferredResolution;
    std::string preferredFormat;
    std::string optimizationHint;
    std::uint64_t estimatedBytes{};
    bool firstBatch{};
    bool atlasRecommended{};
    bool trimSheetRecommended{};
};

struct PhotorealImportReceipt3D {
    std::string assetId;
    std::string source;
    std::string sha256;
    std::uint64_t bytes{};
    std::uint64_t uncompressedBytes{};
    PhotorealMaterialChannel3D channels{PhotorealMaterialChannel3D::None};
    bool passed{};
};

struct PhotorealCatalogStats3D {
    std::size_t assetCount{};
    std::size_t firstBatchCount{};
    std::uint64_t firstBatchEstimatedBytes{};
    std::uint64_t downloadBudgetBytes{};
    std::uint64_t fingerprint{};
};

struct PhotorealCatalogLimits3D {
    std::size_t maximumJsonBytes{4u*1024u*1024u};
    std::size_t maximumAssets{4096};
    std::size_t maximumStringBytes{4096};
    std::uint64_t maximumDownloadBudgetBytes{16ull*1024ull*1024ull*1024ull};
};

class PhotorealAssetCatalog3D {
public:
    bool LoadFile(const std::filesystem::path& path,PhotorealAssetError3D* error=nullptr,
        const PhotorealCatalogLimits3D& limits={});
    bool LoadJson(std::string_view json,PhotorealAssetError3D* error=nullptr,
        const PhotorealCatalogLimits3D& limits={});
    static bool LoadReceiptFile(const std::filesystem::path& path,PhotorealImportReceipt3D& output,
        PhotorealAssetError3D* error=nullptr,std::size_t maximumJsonBytes=256u*1024u);
    static bool LoadReceiptJson(std::string_view json,PhotorealImportReceipt3D& output,
        PhotorealAssetError3D* error=nullptr,std::size_t maximumJsonBytes=256u*1024u);
    [[nodiscard]] const PhotorealCatalogAsset3D* Find(std::string_view id)const noexcept;
    [[nodiscard]] std::span<const PhotorealCatalogAsset3D> Assets()const noexcept{return assets_;}
    [[nodiscard]] const std::optional<PhotorealImportReceipt3D>& EmbeddedPreflight()const noexcept{return preflight_;}
    [[nodiscard]] const PhotorealCatalogStats3D& Stats()const noexcept{return stats_;}
    void Clear()noexcept;
private:
    std::vector<PhotorealCatalogAsset3D> assets_;
    std::optional<PhotorealImportReceipt3D> preflight_;
    PhotorealCatalogStats3D stats_{};
};

struct PhotorealRuntimeAssetReceipt3D {
    std::string id;
    std::string source;
    PhotorealAssetKind3D kind{PhotorealAssetKind3D::Unknown};
    PhotorealMaterialChannel3D channels{PhotorealMaterialChannel3D::None};
    std::vector<std::string> acceptedSha256;
    std::uint64_t runtimeBytes{};
    std::uint32_t lodLevels{};
    bool licensePassed{};
    bool memoryGatePassed{};
    bool hashesPassed{};
    bool channelsPassed{};
    bool lodReceiptPassed{};
};

struct PhotorealRuntimeManifestStats3D {
    std::size_t assetCount{};
    std::uint64_t runtimeBytes{};
    std::uint64_t fingerprint{};
    bool memoryGatePassed{};
    bool allAssetsPassed{};
};

class PhotorealRuntimeManifest3D {
public:
    bool LoadFile(const std::filesystem::path& path,PhotorealAssetError3D* error=nullptr,
        std::size_t maximumJsonBytes=4u*1024u*1024u);
    bool LoadJson(std::string_view json,PhotorealAssetError3D* error=nullptr,
        std::size_t maximumJsonBytes=4u*1024u*1024u);
    [[nodiscard]] const PhotorealRuntimeAssetReceipt3D* Find(std::string_view id)const noexcept;
    [[nodiscard]] std::span<const PhotorealRuntimeAssetReceipt3D> Assets()const noexcept{return assets_;}
    [[nodiscard]] const PhotorealRuntimeManifestStats3D& Stats()const noexcept{return stats_;}
    [[nodiscard]] bool Passed()const noexcept{return stats_.memoryGatePassed&&stats_.allAssetsPassed;}
    void Clear()noexcept;
private:
    std::vector<PhotorealRuntimeAssetReceipt3D> assets_;
    PhotorealRuntimeManifestStats3D stats_{};
};

struct PhotorealAssetProfileSettings3D {
    std::uint64_t cpuBudgetBytes{512ull*1024ull*1024ull};
    std::uint64_t gpuBudgetBytes{768ull*1024ull*1024ull};
    std::uint32_t maximumAnisotropy{16};
    std::uint32_t maximumTextureDimension{4096};
    std::uint32_t maximumNearMeshTriangles{250000};
    std::uint32_t maximumFarMeshTriangles{8000};
    bool ultraDevelopmentQuality{true};
    bool requireRuntimeManifest{true};
};

struct PhotorealAssetObservation3D {
    float distanceMeters{};
    float projectedCoverage{1.f};
    PhotorealImportance3D importance{PhotorealImportance3D::Standard};
};

struct PhotorealAssetSelection3D {
    PhotorealTextureTier3D textureTier{PhotorealTextureTier3D::General1K};
    std::uint32_t textureDimension{1024};
    std::uint32_t anisotropy{8};
    std::uint32_t firstResidentMip{};
    std::uint32_t residentMipCount{11};
    std::uint32_t meshLod{};
    std::uint32_t triangleBudget{80000};
    std::uint64_t estimatedGpuBytes{};
};

struct PhotorealAssetResidencyRequest3D {
    std::string assetId;
    PhotorealImportReceipt3D receipt;
    PhotorealAssetSelection3D selection;
    PhotorealImportance3D importance{PhotorealImportance3D::Standard};
    std::uint64_t cpuBytes{};
    std::uint64_t gpuBytes{};
};

struct PhotorealAssetResidencyReceipt3D {
    std::uint64_t sequence{};
    std::uint64_t catalogFingerprint{};
    std::string assetId;
    std::uint64_t cpuBytes{};
    std::uint64_t gpuBytes{};
    std::uint32_t meshLod{};
    std::uint32_t textureDimension{};
    bool loaded{};
    bool unloaded{};
    explicit operator bool()const noexcept{return sequence&&catalogFingerprint&&!assetId.empty()&&(loaded!=unloaded);}
};

struct PhotorealAssetTrimReceipt3D {
    std::uint64_t sequence{};
    std::uint64_t catalogFingerprint{};
    std::uint64_t cpuBytesBefore{};
    std::uint64_t gpuBytesBefore{};
    std::uint64_t cpuBytesAfter{};
    std::uint64_t gpuBytesAfter{};
    std::uint64_t releasedCpuBytes{};
    std::uint64_t releasedGpuBytes{};
    std::vector<std::string> unloadedAssetIds;
};

struct PhotorealAssetProfileStats3D {
    std::size_t residentAssets{};
    std::uint64_t residentCpuBytes{};
    std::uint64_t residentGpuBytes{};
    std::uint64_t peakCpuBytes{};
    std::uint64_t peakGpuBytes{};
    std::uint64_t loadCount{};
    std::uint64_t unloadCount{};
    std::uint64_t trimCount{};
    std::uint64_t rejectedLoads{};
};

class PhotorealAssetProfile3D {
public:
    bool Initialize(const PhotorealAssetCatalog3D& catalog,const PhotorealAssetProfileSettings3D& settings={},
        PhotorealAssetError3D* error=nullptr);
    bool Initialize(const PhotorealAssetCatalog3D& catalog,const PhotorealRuntimeManifest3D& runtimeManifest,
        const PhotorealAssetProfileSettings3D& settings={},PhotorealAssetError3D* error=nullptr);
    [[nodiscard]] PhotorealAssetSelection3D Select(const PhotorealCatalogAsset3D& asset,
        const PhotorealAssetObservation3D& observation)const noexcept;
    bool Admit(const PhotorealAssetResidencyRequest3D& request,PhotorealAssetResidencyReceipt3D& receipt,
        PhotorealAssetError3D* error=nullptr);
    bool Touch(std::string_view assetId,std::uint64_t frameIndex)noexcept;
    bool Unload(std::string_view assetId,PhotorealAssetResidencyReceipt3D& receipt,
        PhotorealAssetError3D* error=nullptr);
    PhotorealAssetTrimReceipt3D Trim(std::uint64_t targetCpuBytes,std::uint64_t targetGpuBytes,
        std::span<const std::string_view> protectedAssetIds={});
    [[nodiscard]] bool IsResident(std::string_view assetId)const noexcept;
    [[nodiscard]] const PhotorealAssetProfileStats3D& Stats()const noexcept{return stats_;}
    [[nodiscard]] const PhotorealAssetProfileSettings3D& Settings()const noexcept{return settings_;}
    [[nodiscard]] std::uint64_t CatalogFingerprint()const noexcept{return catalogFingerprint_;}
    static bool ValidateMaterialChannels(const PhotorealCatalogAsset3D& asset,
        const PhotorealImportReceipt3D& receipt,PhotorealAssetError3D* error=nullptr);
private:
    struct ResidentAsset3D {
        std::string id;
        PhotorealImportance3D importance{PhotorealImportance3D::Standard};
        std::uint64_t cpuBytes{};
        std::uint64_t gpuBytes{};
        std::uint64_t lastTouchedFrame{};
        std::uint32_t meshLod{};
        std::uint32_t textureDimension{};
    };
    PhotorealAssetCatalog3D catalog_{};
    PhotorealRuntimeManifest3D runtimeManifest_{};
    bool initialized_{};
    bool runtimeManifestBound_{};
    PhotorealAssetProfileSettings3D settings_{};
    std::vector<ResidentAsset3D> resident_;
    PhotorealAssetProfileStats3D stats_{};
    std::uint64_t catalogFingerprint_{};
    std::uint64_t sequence_{};
};

}
