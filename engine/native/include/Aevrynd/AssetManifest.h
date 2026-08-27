#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Aevrynd {

enum class AssetKind : uint8_t {Texture,Material,Mesh};
enum class TextureUsage : uint8_t {BaseColor,Normal,Roughness,Metallic,AmbientOcclusion,Height,Emissive,Opacity,PackedOrm,Subsurface,Clearcoat};
enum class TextureResolutionTier : uint8_t {Far,General,Near,Hero};

struct AssetSourceMetadata {
    std::string uri;
    std::string sha256;
};

struct AssetLicenseMetadata {
    std::string spdxIdentifier{"CC0-1.0"};
    bool commercialUseAllowed{true};
};

struct TextureMetadata {
    TextureUsage usage{TextureUsage::BaseColor};
    TextureResolutionTier resolutionTier{TextureResolutionTier::General};
    uint32_t width{};
    uint32_t height{};
};

struct AssetLodMetadata {
    std::string group;
    uint32_t level{};
    float switchDistance{};
    float geometricError{};
};

struct AssetAtlasMetadata {
    std::string atlas;
    uint32_t x{},y{},width{},height{};
};

struct AssetTrimSheetMetadata {
    std::string sheet;
    uint32_t strip{};
};

struct AssetPbrMetadata {
    bool metallicRoughnessWorkflow{true};
    float normalScale{1.f};
    float roughnessFactor{1.f};
    float metallicFactor{};
};

struct AssetMeshOptimizationMetadata {
    uint32_t vertexCount{};
    uint32_t indexCount{};
    uint32_t materialSlots{};
    bool vertexCacheOptimized{};
    bool overdrawOptimized{};
    bool hasTangents{};
    bool hasCollision{};
};

struct AssetManifestEntry {
    std::string id;
    std::string path;
    AssetKind kind{AssetKind::Texture};
    AssetSourceMetadata source;
    AssetLicenseMetadata license;
    TextureMetadata texture;
    AssetLodMetadata lod;
    AssetAtlasMetadata atlas;
    AssetTrimSheetMetadata trimSheet;
    AssetPbrMetadata pbr;
    AssetMeshOptimizationMetadata mesh;
    std::string cacheKey;
};

struct AssetManifest {
    static constexpr uint32_t CurrentVersion=1;
    uint32_t version{CurrentVersion};
    std::vector<AssetManifestEntry> entries;
};

struct AssetManifestLimits {
    size_t maxEntries{4096};
    size_t maxStringBytes{4096};
    size_t maxMetadataBytes{4*1024*1024};
    uint32_t maxTextureDimension{4096};
    uint32_t maxLodLevel{15};
    uint32_t maxMeshVertices{16*1024*1024};
    uint32_t maxMaterialSlots{256};
};

enum class AssetManifestErrorCode : uint8_t {
    None,UnsupportedVersion,LimitExceeded,InvalidIdentifier,InvalidPath,PathEscapesRoot,
    InvalidSource,InvalidHash,LicenseNotCc0,CommercialUseDenied,InvalidTexture,
    InvalidLod,InvalidAtlas,InvalidTrimSheet,InvalidPbr,InvalidMesh,DuplicateIdentifier,DuplicatePath
};

struct AssetManifestError {
    AssetManifestErrorCode code{AssetManifestErrorCode::None};
    size_t entryIndex{};
    std::string message;
    explicit operator bool()const{return code!=AssetManifestErrorCode::None;}
};

class AssetManifestValidator {
public:
    static constexpr uint32_t CacheSchemaVersion=2;
    static constexpr uint32_t CookerPolicyVersion=1;
    // 此函式只處理詞法正規化；cooker 開檔時仍須以最終實體路徑阻擋 junction、symlink 與 reparse point 逃逸。
    static bool NormalizeRelativePath(std::string_view input,std::string& output);
    static bool ValidateAndNormalize(const AssetManifest& input,AssetManifest& output,
        AssetManifestError* error=nullptr,const AssetManifestLimits& limits={});
    static std::string BuildIncrementalCacheKey(const AssetManifestEntry& entry);
    static uint32_t TierMinimumDimension(TextureResolutionTier tier);
    static uint32_t TierMaximumDimension(TextureResolutionTier tier);
};

}
