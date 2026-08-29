#pragma once
#include "ReplicationRuntime3D.h"
#include "SceneECS3D.h"
#include "WorldStreamer3D.h"
#include <map>
#include <set>
#include <functional>

namespace Aevrynd {
struct ReplicatedSceneState3D {SceneEntity3D entity{};GlobalPosition3D previous{},current{};std::uint64_t tick{};};
class ReplicatedSceneRegistry3D {
public:
 explicit ReplicatedSceneRegistry3D(SceneECSRegistry3D& scene):scene_(scene){}
 bool SpawnAtomic(std::span<const std::pair<NetworkEntityId3D,SceneEntityDefinition3D>> definitions,WorldOrigin3D origin);
 bool DespawnAtomic(std::span<const NetworkEntityId3D> ids);
 bool Apply(NetworkEntityId3D id,GlobalPosition3D position,std::uint64_t tick,WorldOrigin3D origin);
 std::optional<ReplicatedSceneState3D> Query(NetworkEntityId3D id)const;
 void Clear();
private:SceneECSRegistry3D& scene_;std::map<NetworkEntityId3D,ReplicatedSceneState3D> entities_{};
};
struct ReplicationInterestSettings3D {std::int32_t loadRadius{2},retainRadius{3};float chunkSize{32.f};};
struct ReplicationInterestDelta3D {std::uint64_t epoch{};std::vector<WorldChunkCoordinate3D> entered{},left{};};
class ReplicationChunkInterest3D {
public:explicit ReplicationChunkInterest3D(ReplicationInterestSettings3D settings={});bool Update(GlobalPosition3D observer,ReplicationInterestDelta3D& delta);std::span<const WorldChunkCoordinate3D> Chunks()const{return view_;}
private:ReplicationInterestSettings3D settings_{};std::set<std::pair<int,int>> chunks_{};std::vector<WorldChunkCoordinate3D> view_{};std::uint64_t epoch_{};
};
class IDurableMutationTarget3D {public:virtual~IDurableMutationTarget3D()=default;virtual bool Prepare(std::uint64_t transaction,const WorldChunkDeltaSnapshot3D& snapshot)=0;virtual void Commit(std::uint64_t transaction,const WorldChunkDeltaSnapshot3D& snapshot)noexcept=0;virtual void Abort(std::uint64_t transaction)noexcept=0;};
struct DurableMutationAuthorizationContext3D {std::uint64_t sessionEpoch{},serverTick{};std::uint32_t clientId{};WorldDeltaChunkKey3D key{};std::uint64_t targetGeneration{},baseVersion{};};
using DurableMutationAuthorize3D=std::function<bool(const ReplicationMutationRequest3D&,const DurableMutationAuthorizationContext3D&)>;
struct DurableMutationAuthoritySettings3D {std::uint64_t sessionEpoch{1};std::size_t maximumPending{128},maximumCompleted{256};DurableMutationAuthorize3D authorize{};};
struct DurableMutationAuthorityStats3D {std::size_t pending{},completed{};std::uint64_t submitted{},durable{},duplicates{},rejected{},unauthorized{},saveFailures{},committed{};bool closed{};};
class DurableMutationAuthority3D {
public:
 // 中文：外觀層僅限建立它的主執行緒；背景工作只接觸共享完成狀態。
 DurableMutationAuthority3D(WorldChunkDeltaStore3D& store,std::shared_ptr<IDurableMutationTarget3D> target,DurableMutationAuthoritySettings3D settings={});
 ~DurableMutationAuthority3D();
 bool Submit(ReplicationMutationRequest3D request,WorldChunkDeltaSnapshot3D current,std::uint64_t serverTick);
 void Poll();bool Close();bool TryPopReceipt(ReplicationMutationReceipt3D& receipt);DurableMutationAuthorityStats3D Stats()const{return stats_;}
 std::size_t PendingCapacity()const noexcept{return settings_.maximumPending;}std::size_t CompletedCapacity()const noexcept{return settings_.maximumCompleted;}
private:struct TerminalState;struct Pending{ReplicationMutationRequest3D request{};WorldChunkDeltaSnapshot3D snapshot{};std::uint64_t ioId{};std::shared_ptr<TerminalState>terminal{};};struct CompletionState;std::shared_ptr<CompletionState> completionState_{};WorldChunkDeltaStore3D::SaveOwnerLease saveOwner_{};std::shared_ptr<IDurableMutationTarget3D>target_{};DurableMutationAuthoritySettings3D settings_{};std::map<std::uint64_t,Pending> pending_{};std::map<std::pair<std::uint32_t,std::uint64_t>,ReplicationMutationReceipt3D> dedup_{};std::deque<std::pair<std::uint32_t,std::uint64_t>> dedupOrder_{};std::deque<ReplicationMutationReceipt3D> completed_{};std::uint64_t nextIo_{1};bool closed_{};DurableMutationAuthorityStats3D stats_{};
};
}
