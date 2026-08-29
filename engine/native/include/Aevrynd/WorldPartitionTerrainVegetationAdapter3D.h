#pragma once
#include "Engine.h"
#include "HeightField3D.h"
#include "SceneECS3D.h"
#include "Vegetation3D.h"
#include "WorldPartition3D.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace Aevrynd {
struct TerrainVegetationChunkSettings3D {
    float chunkSize{32.f};
    unsigned terrainCells{16};
    float terrainAmplitude{10.f};
    std::uint32_t vegetationPerChunk{32};
    std::array<Mesh3D,3> vegetationLods{};
    MaterialTextureSetDefinition3D terrainTextures{},vegetationTextures{};
};

struct TerrainVegetationChunkPayload3D {
    Mesh3D terrain{};
    std::vector<VegetationInstance3D> vegetation{};
    std::size_t cpuBytes{},conservativeGpuBytes{};
};

class TerrainVegetationWorldChunkSource3D final:public IWorldChunkSource3D {
public:
    explicit TerrainVegetationWorldChunkSource3D(TerrainVegetationChunkSettings3D settings={},std::uint32_t workers=2);
    ~TerrainVegetationWorldChunkSource3D()override;
    bool BeginLoad(WorldChunkRequest3D request,std::stop_token cancellation)override;
    bool TryPopCompletion(WorldChunkLoadCompletion3D& completion)override;
    void WaitIdle()override;
    std::size_t TrimUnusedMemory()override;
private:
    struct Work {WorldChunkRequest3D request{};std::stop_token cancellation{};};
    void Worker(std::stop_token stop);
    TerrainVegetationChunkSettings3D settings_{};
    std::mutex mutex_{};std::condition_variable_any wake_{},idle_{};
    std::deque<Work> work_{};std::deque<WorldChunkLoadCompletion3D> completions_{};
    std::vector<std::jthread> workers_{};std::size_t active_{};
};

class TerrainVegetationWorldChunkActivationTarget3D final:public IWorldChunkActivationTarget3D {
public:
    TerrainVegetationWorldChunkActivationTarget3D(Engine& engine,SceneECSRegistry3D& scene,ChunkedVegetationWorld3D& vegetation,TerrainVegetationChunkSettings3D settings={});
    ~TerrainVegetationWorldChunkActivationTarget3D()override;
    std::size_t PreparationBytes(WorldChunkRequest3D,const WorldChunkPayload3D& payload)const override{return payload.cpuBytes;}
    std::size_t PhysicalAllocationBytes(WorldChunkRequest3D,const WorldChunkPayload3D& payload)const override;
    WorldChunkActivation3D Activate(WorldChunkRequest3D request,const WorldChunkPayload3D& payload)override;
    bool IsActivationComplete(WorldChunkActivation3D activation)const override;
    void Rollback(WorldChunkActivation3D activation)override;
    WorldChunkActivation3D BeginRetire(WorldChunkActivation3D activation)override;
    bool IsRetirementComplete(WorldChunkActivation3D retirement)const override;
    void WaitIdle()override;
    std::size_t TrimUnusedMemory()override;
    void SetActivationFailureCheckpointForTesting(int value)noexcept{failureCheckpoint_=value;}
    std::size_t PendingRetirements()const noexcept;
private:
    // GPU、ECS 與植被發布只能由建立 target 的主執行緒操作；跨執行緒呼叫一律 fail-closed。
    bool OnOwnerThread()const noexcept{return std::this_thread::get_id()==ownerThread_;}
    struct SharedVegetation {std::array<MeshResource3D,3> meshes{};MaterialTextureSetHandle3D textures{};AssetLease3D lease{};std::size_t references{},physicalBytes{};};
    struct Record {WorldChunkRequest3D request{};MeshResource3D terrain{};MaterialTextureSetHandle3D terrainTextures{};SceneEntity3D terrainEntity{};VegetationChunkHandle3D vegetation{};GpuUploadReceipt3D receipt{};std::size_t physicalBytes{};bool retiring{},released{};};
    bool EnsureShared(const TerrainVegetationChunkPayload3D& payload,std::size_t& physicalBytes);
    void ReleaseRecord(Record& record);
    Engine& engine_;SceneECSRegistry3D& scene_;ChunkedVegetationWorld3D& vegetation_;TerrainVegetationChunkSettings3D settings_{};
    SharedVegetation shared_{};mutable std::unordered_map<std::uint64_t,Record> records_{};std::thread::id ownerThread_{};std::uint64_t next_{1};int failureCheckpoint_{-1};
};
}
