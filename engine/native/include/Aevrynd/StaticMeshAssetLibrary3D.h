#pragma once

#include "MaterialLibrary3D.h"
#include "MeshLoader.h"
#include "SceneECS3D.h"
#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Aevrynd {

struct StaticMeshImportedLod3D {
    std::filesystem::path meshPath;
    ObjLoadOptions3D objOptions{};
};

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
    std::array<StaticMeshImportedLod3D,MaximumGeneratedMeshLodLevels3D> importedLods{};
    std::uint32_t importedLodCount{};
    bool generateLods{true};
};

struct StaticMeshAssetLibraryStats3D {
    std::size_t assets{},materialReferences{},meshLevels{},sourceTriangles{},residentTriangles{},liveInstances{},assetBorrowers{};
    std::uint64_t loadRequests{},cacheHits{},loadFailures{},nameConflicts{},batchRollbacks{},unloaded{};
    std::uint64_t instancesCreated{},instancesDestroyed{},externallyDestroyedInstances{},instanceFailures{},unloadRejections{};
    std::uint64_t generatedAssets{},importedAssets{},singleLevelAssets{},importedLevels{},leaseUnloadRejections{},shutdownRejections{};
};

struct StaticMeshAssetLibraryLimits3D {
    std::size_t maximumAssets{4096},maximumInstances{1024*1024},maximumSourceFileBytes{256*1024*1024};
    std::size_t maximumAggregateSourceBytes{512*1024*1024};
    float maximumBoundsCenterDriftRatio{.08f},minimumBoundsExtentRatio{.70f},maximumBoundsExtentRatio{1.15f};
};

struct StaticMeshAssetKey3D {std::string canonical;std::uint64_t contentIdentity{};explicit operator bool()const{return !canonical.empty()&&contentIdentity!=0;}};
struct PreparedStaticMeshAsset3D {
    StaticMeshAssetDefinition3D definition{};
    std::array<Mesh3D,1+MaximumGeneratedMeshLodLevels3D> cpuMeshes{};
    std::uint32_t levelCount{};
    MeshLodChainStats3D processing{};
    StaticMeshAssetKey3D assetKey{};
    explicit operator bool()const{return !definition.name.empty()&&levelCount&&assetKey;}
};

class StaticMeshAssetLibrary3D {
public:
    StaticMeshAssetLibrary3D()=default;
    StaticMeshAssetLibrary3D(const StaticMeshAssetLibrary3D&)=delete;
    StaticMeshAssetLibrary3D& operator=(const StaticMeshAssetLibrary3D&)=delete;
    bool Initialize(Engine& engine,SceneECSRegistry3D& registry,std::filesystem::path assetRoot={},StaticMeshAssetLibraryLimits3D limits={});
    [[nodiscard]] std::optional<PreparedStaticMeshAsset3D> PrepareCpu(StaticMeshAssetDefinition3D definition)const;
    bool AcquirePrepared(PreparedStaticMeshAsset3D prepared);
    [[nodiscard]] bool ContainsPreparedPhysical(const PreparedStaticMeshAsset3D& prepared)const{return prepared&&physicalAssets_.contains(prepared.assetKey.canonical);}
    bool Load(StaticMeshAssetDefinition3D definition);
    bool LoadAll(std::span<const StaticMeshAssetDefinition3D> definitions);
    bool Unload(std::string_view name);
    bool Shutdown();
    [[nodiscard]] bool Contains(std::string_view name)const;
    SceneEntity3D Instantiate(std::string_view name,Transform3D transform={},std::uint32_t layer=1,bool visible=true);
    bool DestroyInstance(SceneEntity3D entity);
    std::size_t ClearInstances();
    [[nodiscard]] std::uint32_t LevelCount(std::string_view name)const;
    [[nodiscard]] StaticMeshAssetLibraryStats3D Stats()const;
    [[nodiscard]] bool Initialized()const{return engine_!=nullptr;}
    [[nodiscard]] const std::filesystem::path& AssetRoot()const{return assetRoot_;}
private:
    struct PhysicalAssetRecord {MeshLodResourceChain3D meshes{};Material3D material{};std::string materialName;AssetLease3D assetLease{};std::size_t aliasCount{},physicalBytes{};};
    struct Entry {
        StaticMeshAssetDefinition3D definition{};
        std::string assetKey;
    };
    struct Instance {SceneEntity3D entity{};std::string assetName;};
    StaticMeshAssetDefinition3D Resolve(StaticMeshAssetDefinition3D definition)const;
    std::optional<SceneMeshRenderable3D> CreateRenderable(std::string_view name,std::uint32_t layer,bool visible)const;
    void CollectDeadInstances();
    void DestroyPhysical(PhysicalAssetRecord& asset);
    Engine* engine_{};
    SceneECSRegistry3D* registry_{};
    std::filesystem::path assetRoot_{};
    StaticMeshAssetLibraryLimits3D limits_{};
    MaterialLibrary3D materials_{};
    std::unordered_map<std::string,Entry> entries_{};
    std::unordered_map<std::string,std::unique_ptr<PhysicalAssetRecord>> physicalAssets_{};
    std::unordered_map<std::string,std::size_t> materialReferences_{};
    std::vector<Instance> instances_{};
    StaticMeshAssetLibraryStats3D stats_{};
};

}
