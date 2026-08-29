#pragma once
#include "Engine.h"
#include "StaticMeshAssetLibrary3D.h"
#include "WorldPartition3D.h"
#include <deque>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace Aevrynd {
struct WorldPartitionStaticMeshTemplate3D {StaticMeshAssetDefinition3D asset{};Transform3D localTransform{};std::uint32_t layer{1};};

class StaticMeshWorldChunkSource3D final:public IWorldChunkSource3D {
public:
 struct Payload;
 StaticMeshWorldChunkSource3D(StaticMeshAssetLibrary3D& library,WorldPartitionStaticMeshTemplate3D value,uint32_t workers=2);
 ~StaticMeshWorldChunkSource3D()override;
 bool BeginLoad(WorldChunkRequest3D request,std::stop_token cancellation)override;
 bool TryPopCompletion(WorldChunkLoadCompletion3D& completion)override;
 void WaitIdle()override;
 std::size_t TrimUnusedMemory()override;
private:
 struct Work {WorldChunkRequest3D request{};std::stop_token cancellation;};void Worker(std::stop_token stop);
 StaticMeshAssetLibrary3D& library_;WorldPartitionStaticMeshTemplate3D template_;std::mutex mutex_;std::condition_variable_any wake_,idle_;std::deque<Work> work_;std::deque<WorldChunkLoadCompletion3D> completions_;std::vector<std::jthread> workers_;size_t active_{};
};

class StaticMeshWorldChunkActivationTarget3D final:public IWorldChunkActivationTarget3D {
public:
 StaticMeshWorldChunkActivationTarget3D(Engine& engine,StaticMeshAssetLibrary3D& library):engine_(engine),library_(library){}
 std::size_t PreparationBytes(WorldChunkRequest3D request,const WorldChunkPayload3D& payload)const override;
 std::size_t PhysicalAllocationBytes(WorldChunkRequest3D request,const WorldChunkPayload3D& payload)const override;
 WorldChunkActivation3D Activate(WorldChunkRequest3D request,const WorldChunkPayload3D& payload)override;
 bool IsActivationComplete(WorldChunkActivation3D activation)const override;
 void Rollback(WorldChunkActivation3D activation)override;
 WorldChunkActivation3D BeginRetire(WorldChunkActivation3D activation)override;
 bool IsRetirementComplete(WorldChunkActivation3D retirement)const override;
 void WaitIdle()override;
 std::size_t TrimUnusedMemory()override;
 void SetActivationFailureCheckpointForTesting(int checkpoint)noexcept{activationFailureCheckpoint_=checkpoint;}
 void SetNextActivationIdForTesting(uint64_t value)noexcept{next_=value;}
 std::size_t PendingRetirements()const noexcept;
private:
 struct Record {std::string assetName;SceneEntity3D entity{};GpuUploadReceipt3D upload{};std::size_t physicalBytes{};bool retiring{},unloadCompleted{};};
 Engine& engine_;StaticMeshAssetLibrary3D& library_;uint64_t next_{1};int activationFailureCheckpoint_{-1};mutable std::unordered_map<uint64_t,Record> records_;
};
}
