#pragma once

#include "MaterialLibrary3D.h"
#include "MeshLoader.h"
#include "SceneECS3D.h"
#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Aevrynd {

struct StaticMeshAssetDefinition3D {
    std::string name;
    std::filesystem::path meshPath;
    ObjLoadOptions3D objOptions{};
    MaterialAssetDefinition3D material{};
    MeshLodChainSettings3D lodSettings{};
    std::array<float,MaximumGeneratedMeshLodLevels3D> lodSwitchDistances{24.f,64.f,128.f};
    std::array<float,MaximumGeneratedMeshLodLevels3D> lodTransitionWidths{4.f,8.f,12.f};
    MeshLodDistanceMetric3D lodDistanceMetric{MeshLodDistanceMetric3D::BoundsSurface};
    MeshLodCpuRetention3D cpuRetention{MeshLodCpuRetention3D::Level0Only};
    bool generateLods{true};
};

struct StaticMeshAssetLibraryStats3D {
    std::size_t assets{},materialReferences{},meshLevels{},sourceTriangles{},residentTriangles{},liveInstances{};
    std::uint64_t loadRequests{},cacheHits{},loadFailures{},nameConflicts{},batchRollbacks{},unloaded{};
    std::uint64_t instancesCreated{},instancesDestroyed{},externallyDestroyedInstances{},instanceFailures{},unloadRejections{};
};

struct StaticMeshAssetLibraryLimits3D {
    std::size_t maximumAssets{4096},maximumInstances{1024*1024},maximumSourceFileBytes{256*1024*1024};
};

class StaticMeshAssetLibrary3D {
public:
    StaticMeshAssetLibrary3D()=default;
    StaticMeshAssetLibrary3D(const StaticMeshAssetLibrary3D&)=delete;
    StaticMeshAssetLibrary3D& operator=(const StaticMeshAssetLibrary3D&)=delete;
    bool Initialize(Engine& engine,SceneECSRegistry3D& registry,std::filesystem::path assetRoot={},StaticMeshAssetLibraryLimits3D limits={});
    bool Load(StaticMeshAssetDefinition3D definition);
    bool LoadAll(std::span<const StaticMeshAssetDefinition3D> definitions);
    bool Unload(std::string_view name);
    void Shutdown();
    [[nodiscard]] bool Contains(std::string_view name)const;
    SceneEntity3D Instantiate(std::string_view name,Transform3D transform={},std::uint32_t layer=1,bool visible=true);
    bool DestroyInstance(SceneEntity3D entity);
    std::size_t ClearInstances();
    [[nodiscard]] std::uint32_t LevelCount(std::string_view name)const;
    [[nodiscard]] StaticMeshAssetLibraryStats3D Stats()const;
    [[nodiscard]] bool Initialized()const{return engine_!=nullptr;}
    [[nodiscard]] const std::filesystem::path& AssetRoot()const{return assetRoot_;}
private:
    struct Entry {
        StaticMeshAssetDefinition3D definition{};
        MeshLodResourceChain3D meshes{};
        Material3D material{};
    };
    struct Instance {SceneEntity3D entity{};std::string assetName;};
    StaticMeshAssetDefinition3D Resolve(StaticMeshAssetDefinition3D definition)const;
    std::optional<SceneMeshRenderable3D> CreateRenderable(std::string_view name,std::uint32_t layer,bool visible)const;
    void CollectDeadInstances();
    void DestroyMeshes(Entry& entry);
    Engine* engine_{};
    SceneECSRegistry3D* registry_{};
    std::filesystem::path assetRoot_{};
    StaticMeshAssetLibraryLimits3D limits_{};
    MaterialLibrary3D materials_{};
    std::unordered_map<std::string,Entry> entries_{};
    std::unordered_map<std::string,std::size_t> materialReferences_{};
    std::vector<Instance> instances_{};
    StaticMeshAssetLibraryStats3D stats_{};
};

}
