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

bool EncodeSkinnedModelAsset(const SkinnedModel3D& model,std::vector<std::byte>& output,
    std::string* error=nullptr,const SkinnedModelLimits3D& limits={});
SkinnedModelAssetResult LoadSkinnedModelAsset(std::span<const std::byte> source,
    const SkinnedModelLimits3D& limits={});
SkinnedModelAssetResult LoadSkinnedModelAssetFile(const std::filesystem::path& path,
    const SkinnedModelLimits3D& limits={});
bool SaveSkinnedModelAssetFile(const std::filesystem::path& path,const SkinnedModel3D& model,
    std::string* error=nullptr,const SkinnedModelLimits3D& limits={});
}
