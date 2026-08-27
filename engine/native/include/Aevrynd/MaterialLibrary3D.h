#pragma once
#include "Engine.h"
#include "Mesh3D.h"
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Aevrynd {
struct PbrMaterialCookPlan3D;
struct MaterialAssetDefinition3D {
    std::string name;
    MaterialTextureSetDefinition3D textures{};
    Material3D material{};
};

struct MaterialLibraryStats3D {
    std::size_t materials{},uniqueTextureSets{};
    uint64_t loadRequests{},cacheHits{},loadFailures{},nameConflicts{},batchRollbacks{},unloaded{};
};

class MaterialLibrary3D {
public:
    MaterialLibrary3D()=default;
    MaterialLibrary3D(const MaterialLibrary3D&)=delete;
    MaterialLibrary3D& operator=(const MaterialLibrary3D&)=delete;
    bool Initialize(Engine& engine,std::filesystem::path assetRoot={});
    bool Load(MaterialAssetDefinition3D definition);
    bool LoadAll(std::span<const MaterialAssetDefinition3D> definitions);
    bool LoadCookPlan(const PbrMaterialCookPlan3D& plan);
    bool Unload(std::string_view name);
    void Shutdown();
    const Material3D* Find(std::string_view name)const;
    MaterialTextureSetHandle3D Textures(std::string_view name)const;
    MaterialLibraryStats3D Stats()const;
    bool Initialized()const{return engine_!=nullptr;}
    const std::filesystem::path& AssetRoot()const{return assetRoot_;}
private:
    struct Entry {MaterialAssetDefinition3D definition{};Material3D material{};};
    MaterialAssetDefinition3D Resolve(MaterialAssetDefinition3D definition)const;
    Engine* engine_{};
    std::filesystem::path assetRoot_{};
    std::unordered_map<std::string,Entry> entries_{};
    MaterialLibraryStats3D stats_{};
};
}
