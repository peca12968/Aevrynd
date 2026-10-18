#pragma once
#include "SkinnedModel3D.h"
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace Aevrynd {
struct SkinnedModelAssetResult {
    SkinnedModel3D model;
    std::string error;
    explicit operator bool()const{return error.empty()&&model.Valid();}
};

struct SkinnedSubmesh3D {
    std::uint32_t firstIndex{},indexCount{},materialSlot{},flags{};
};

struct NamedAnimationClip3D {
    std::string name;
    AnimationClip3D clip;
};

enum class SkinnedMaterialAlphaMode3D : std::uint8_t {Opaque,Mask,Blend};

struct SkinnedMaterialDefinition3D {
    Color baseColorFactor{1,1,1,1};
    Vec3 emissiveFactor{};
    float emissiveStrength{1},roughnessFactor{1},metallicFactor{1},alphaCutoff{.5f},normalScale{1},occlusionStrength{1};
    SkinnedMaterialAlphaMode3D alphaMode{SkinnedMaterialAlphaMode3D::Opaque};
    bool doubleSided{},unlit{};
};

struct SkinnedModelAssetData {
    SkinnedModel3D model;
    std::vector<std::string> jointNames;
    std::vector<SkinnedSubmesh3D> submeshes;
    std::vector<std::string> materialSlotNames;
    // 空陣列代表舊格式未攜帶材質因子；非空時與材質槽一一對應。
    std::vector<SkinnedMaterialDefinition3D> materials;
    std::vector<NamedAnimationClip3D> clips;
};

struct SkinnedModelAssetLimits {
    SkinnedModelLimits3D model{};
    std::size_t maximumFileBytes{64*1024*1024},maximumStringBytes{1024*1024};
    std::size_t maximumSubmeshes{256},maximumClips{256},maximumTracks{32768},maximumKeyframes{1000000};
};

struct SkinnedModelAssetDetailedResult {
    SkinnedModelAssetData asset;
    std::string error;
    explicit operator bool()const{return error.empty();}
};

bool EncodeSkinnedModelAsset(const SkinnedModel3D& model,std::vector<std::byte>& output,
    std::string* error=nullptr,const SkinnedModelLimits3D& limits={});
SkinnedModelAssetResult LoadSkinnedModelAsset(std::span<const std::byte> source,
    const SkinnedModelLimits3D& limits={});
SkinnedModelAssetResult LoadSkinnedModelAssetFile(const std::filesystem::path& path,
    const SkinnedModelLimits3D& limits={});
bool SaveSkinnedModelAssetFile(const std::filesystem::path& path,const SkinnedModel3D& model,
    std::string* error=nullptr,const SkinnedModelLimits3D& limits={});
bool EncodeSkinnedModelAssetV2(const SkinnedModelAssetData& asset,std::vector<std::byte>& output,
    std::string* error=nullptr,const SkinnedModelAssetLimits& limits={});
SkinnedModelAssetDetailedResult LoadSkinnedModelAssetDetailed(std::span<const std::byte> source,
    const SkinnedModelAssetLimits& limits={});
SkinnedModelAssetDetailedResult LoadSkinnedModelAssetFileDetailed(const std::filesystem::path& path,
    const SkinnedModelAssetLimits& limits={});
}
