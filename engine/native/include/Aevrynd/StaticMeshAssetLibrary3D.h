#pragma once

#include "Hlod3D.h"
#include "MaterialLibrary3D.h"
#include "MeshLoader.h"
#include "SceneECS3D.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
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

inline constexpr std::size_t MaximumHlodProxyLogicalNameBytes3D=160;
struct PreparedHlodProxyAsset3D {
    StableObjectId3D worldId{};
    GlobalChunkCoordinate3D chunk{};
    HlodClusterId3D clusterId{};
    std::uint64_t clusterGeneration{};
    std::string logicalName{};
    PreparedStaticMeshAsset3D prepared{};
    std::size_t reservedPhysicalBytes{};
    explicit operator bool()const noexcept{return bool(worldId)&&bool(clusterId)&&clusterGeneration&&!logicalName.empty()&&bool(prepared)&&reservedPhysicalBytes!=SIZE_MAX&&canonicalIdentity_;}
private:
    std::uint64_t canonicalIdentity_{};
    friend class StaticMeshAssetLibrary3D;
    friend bool ValidatePreparedHlodProxyDescriptor3D(
        const PreparedHlodProxyAsset3D&) noexcept;
};
[[nodiscard]] bool ValidatePreparedHlodProxyDescriptor3D(
    const PreparedHlodProxyAsset3D& value) noexcept;

struct StaticMeshAssetLibraryLifetime3D {
    std::uint64_t ownerIdentity{};
    std::atomic<std::uint64_t> generation{1};
    std::atomic<bool> active{true};
};

struct StaticMeshRenderableLease3D {
    [[nodiscard]] bool Valid()const noexcept{
        return bool(renderable_.assetLease)&&owner_&&ownerIdentity_&&
            owner_->ownerIdentity==ownerIdentity_&&owner_->active.load(std::memory_order_acquire)&&
            owner_->generation.load(std::memory_order_acquire)==generation_;
    }
    explicit operator bool()const noexcept{return Valid();}
    [[nodiscard]] const SceneMeshRenderable3D& Renderable()const noexcept{return renderable_;}
    [[nodiscard]] std::uint64_t OwnerIdentity()const noexcept{return ownerIdentity_;}
    [[nodiscard]] std::uint64_t Generation()const noexcept{return generation_;}
    [[nodiscard]] std::shared_ptr<const StaticMeshAssetLibraryLifetime3D> LifetimeToken()const noexcept{return owner_;}
    void Reset()noexcept{renderable_={};owner_.reset();ownerIdentity_=0;generation_=0;}
private:
    friend class StaticMeshAssetLibrary3D;
    SceneMeshRenderable3D renderable_{};
    std::shared_ptr<const StaticMeshAssetLibraryLifetime3D> owner_{};
    std::uint64_t ownerIdentity_{},generation_{};
};

struct HlodProxyLease3D {
    HlodProxyLease3D()=default;
    HlodProxyLease3D(const HlodProxyLease3D&)=delete;
    HlodProxyLease3D& operator=(const HlodProxyLease3D&)=delete;
    HlodProxyLease3D(HlodProxyLease3D&& other)noexcept{Swap(other);}
    HlodProxyLease3D& operator=(HlodProxyLease3D&& other)noexcept{if(this!=&other)Swap(other);return*this;}
    StableObjectId3D worldId{};
    GlobalChunkCoordinate3D chunk{};
    HlodClusterId3D clusterId{};
    std::uint64_t clusterGeneration{};
    std::string logicalName{};
    StaticMeshRenderableLease3D renderable{};
    std::size_t actualPhysicalBytes{};
    // renderable 已在 alias unload 前釋放；即使 unload 暫時失敗，lease 仍須保持可重試。
    [[nodiscard]] bool Valid()const noexcept{return bool(worldId)&&bool(clusterId)&&clusterGeneration&&!logicalName.empty()&&canonicalIdentity_&&owner_&&ownerIdentity_==owner_->ownerIdentity&&ownerGeneration_==owner_->generation.load(std::memory_order_acquire)&&owner_->active.load(std::memory_order_acquire);}
    explicit operator bool()const noexcept{return Valid();}
    void Reset()noexcept{worldId={};chunk={};clusterId={};clusterGeneration=0;logicalName.clear();renderable.Reset();actualPhysicalBytes=0;owner_.reset();ownerIdentity_=ownerGeneration_=canonicalIdentity_=0;}
private:
    void Swap(HlodProxyLease3D& other)noexcept{using std::swap;swap(worldId,other.worldId);swap(chunk,other.chunk);swap(clusterId,other.clusterId);swap(clusterGeneration,other.clusterGeneration);swap(logicalName,other.logicalName);swap(renderable,other.renderable);swap(actualPhysicalBytes,other.actualPhysicalBytes);swap(owner_,other.owner_);swap(ownerIdentity_,other.ownerIdentity_);swap(ownerGeneration_,other.ownerGeneration_);swap(canonicalIdentity_,other.canonicalIdentity_);}
    friend class StaticMeshAssetLibrary3D;
    std::shared_ptr<const StaticMeshAssetLibraryLifetime3D> owner_{};
    std::uint64_t ownerIdentity_{},ownerGeneration_{},canonicalIdentity_{};
};

class StaticMeshAssetLibrary3D {
public:
    StaticMeshAssetLibrary3D()=default;
    StaticMeshAssetLibrary3D(const StaticMeshAssetLibrary3D&)=delete;
    StaticMeshAssetLibrary3D& operator=(const StaticMeshAssetLibrary3D&)=delete;
    bool Initialize(Engine& engine,SceneECSRegistry3D& registry,std::filesystem::path assetRoot={},StaticMeshAssetLibraryLimits3D limits={});
    [[nodiscard]] std::optional<PreparedStaticMeshAsset3D> PrepareCpu(StaticMeshAssetDefinition3D definition)const;
    bool AcquirePrepared(PreparedStaticMeshAsset3D prepared);
    [[nodiscard]] bool ContainsPreparedPhysical(const PreparedStaticMeshAsset3D& prepared)const;
    [[nodiscard]] std::optional<PreparedHlodProxyAsset3D> PrepareHlodProxyCpu3D(
        StableObjectId3D worldId,GlobalChunkCoordinate3D chunk,HlodClusterId3D clusterId,
        std::uint64_t clusterGeneration,StaticMeshAssetDefinition3D definition)const;
    [[nodiscard]] std::size_t EstimatePreparedPhysicalAllocationBytes3D(const PreparedStaticMeshAsset3D& prepared)const;
    bool AcquireHlodProxyAtomic3D(PreparedHlodProxyAsset3D prepared,HlodProxyLease3D& output);
    bool ReleaseHlodProxyAlias3D(HlodProxyLease3D& lease);
    void SetHlodProxyFailureCheckpointForTesting(int checkpoint)noexcept{hlodProxyFailureCheckpoint_=checkpoint;}
    bool Load(StaticMeshAssetDefinition3D definition);
    bool LoadAll(std::span<const StaticMeshAssetDefinition3D> definitions);
    bool Unload(std::string_view name);
    bool Shutdown();
    [[nodiscard]] bool Contains(std::string_view name)const;
    [[nodiscard]] std::optional<StaticMeshRenderableLease3D> ResolveRenderable(
        std::string_view name,std::uint32_t layer=1,bool visible=true)const;
    SceneEntity3D Instantiate(std::string_view name,Transform3D transform={},std::uint32_t layer=1,bool visible=true);
    bool DestroyInstance(SceneEntity3D entity);
    std::size_t ClearInstances();
    [[nodiscard]] std::uint32_t LevelCount(std::string_view name)const;
    [[nodiscard]] StaticMeshAssetLibraryStats3D Stats()const;
    [[nodiscard]] bool Initialized()const;
    [[nodiscard]] std::filesystem::path AssetRoot()const;
private:
    class OperationGuard {
    public:
        OperationGuard(const StaticMeshAssetLibrary3D& owner,bool shutdown=false)noexcept;
        ~OperationGuard();
        explicit operator bool()const noexcept{return active_;}
    private:
        const StaticMeshAssetLibrary3D* owner_{};
        bool shutdown_{},active_{};
    };
    bool BeginOperation(bool shutdown)const noexcept;
    void EndOperation(bool shutdown)const noexcept;
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
    bool AdvanceHlodProxyFailureCheckpoint()noexcept;
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
    std::shared_ptr<StaticMeshAssetLibraryLifetime3D> ownerState_{};
    // 實際 mutex 只保護 gate；Engine、Material 與 Registry 呼叫均在 unlock 區執行。
    mutable std::mutex operationMutex_{};
    mutable std::condition_variable operationChanged_{};
    mutable std::thread::id operationOwner_{};
    mutable std::size_t operationDepth_{};
    mutable std::uint64_t operationTicket_{},stateGeneration_{1};
    mutable std::size_t shutdownRequests_{};
    mutable bool operationActive_{};
    int hlodProxyFailureCheckpoint_{-1};
};

}
