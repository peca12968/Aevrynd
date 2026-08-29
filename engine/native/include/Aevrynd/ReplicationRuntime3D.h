#pragma once
#include "NetworkTransport3D.h"
#include "ReplicationProtocol3D.h"
#include <deque>
#include <map>
#include <optional>
#include <set>

namespace Aevrynd {
struct ReplicationRuntimeSettings3D {std::uint32_t tickRate{60};std::size_t maximumClients{8},maximumInputHistory{256},maximumSnapshotHistory{64},maximumPacketsPerTick{256},maximumSnapshotEntities{4096};std::int32_t interestLoadRadius{2},interestRetainRadius{3};float interestChunkSize{32.f},movementPerTick{.1f};};
struct ReplicationRuntimeStats3D {std::uint64_t ticks{},inputsReceived{},duplicateInputs{},staleInputs{},futureInputs{},snapshotsSent{},fullSnapshots{},deltaSnapshots{},acks{},reconciliations{},replayedInputs{},historyDrops{},packetRejects{};};
struct ReplicationPlayerState3D {NetworkEntityId3D entity{};GlobalPosition3D position{};Vec3 velocity{};std::uint64_t tick{};};
struct ReplicationServerClientTelemetry3D {std::uint32_t clientId{};std::uint64_t interestEpoch{},interestEntered{},interestExited{},entityReentries{},entityDespawns{};std::size_t interestChunks{},visibleEntities{},lastSnapshotEntities{},lastSnapshotDespawns{};GlobalPosition3D authoritativePosition{};};

class ReplicationServer3D {
public:
 ReplicationServer3D(LoopbackNetworkTransport3D& transport,NetworkEndpoint3D endpoint,std::uint64_t sessionEpoch,ReplicationRuntimeSettings3D settings={});
 bool AddClient(std::uint32_t clientId,NetworkEndpoint3D endpoint,GlobalPosition3D spawn={});
 bool SetClientPosition(std::uint32_t clientId,GlobalPosition3D position);
 void Tick();
 std::optional<ReplicationPlayerState3D> Player(std::uint32_t clientId)const;
 std::optional<ReplicationServerClientTelemetry3D> ClientTelemetry(std::uint32_t clientId)const;
 std::size_t ClientCount()const noexcept{return clients_.size();}
 ReplicationRuntimeStats3D Stats()const{return stats_;}
private:
 struct Client {NetworkEndpoint3D endpoint{};ReplicationPlayerState3D player{};std::map<std::uint64_t,ReplicationInputCommand3D> inputs{};std::deque<ReplicationSnapshot3D> snapshots{};std::set<std::pair<std::int64_t,std::int64_t>> interest{};std::set<NetworkEntityId3D> visible{},everVisible{};std::uint64_t lastInputSequence{},lastInputTick{},ackSnapshotTick{},interestEpoch{},interestEntered{},interestExited{},entityReentries{},entityDespawns{};std::size_t lastSnapshotEntities{},lastSnapshotDespawns{};};
 void Receive();void SendSnapshots();
 LoopbackNetworkTransport3D& transport_;NetworkEndpoint3D endpoint_{};std::uint64_t sessionEpoch_{},tick_{};ReplicationRuntimeSettings3D settings_{};std::map<std::uint32_t,Client> clients_{};ReplicationRuntimeStats3D stats_{};
};

class ReplicationClient3D {
public:
 ReplicationClient3D(LoopbackNetworkTransport3D& transport,NetworkEndpoint3D endpoint,NetworkEndpoint3D server,std::uint64_t sessionEpoch,std::uint32_t clientId,ReplicationRuntimeSettings3D settings={});
 bool SubmitInput(std::int16_t moveX,std::int16_t moveZ,bool jump=false);
 void Tick();
 ReplicationPlayerState3D Predicted()const{return predicted_;}
 std::optional<ReplicationPlayerState3D> Remote(NetworkEntityId3D id,float alpha)const;
 ReplicationRuntimeStats3D Stats()const{return stats_;}
private:
 struct PredictedFrame {ReplicationInputCommand3D input{};ReplicationPlayerState3D state{};};
 void Receive();void Apply(const ReplicationSnapshot3D& snapshot);void Simulate(ReplicationPlayerState3D& state,const ReplicationInputCommand3D& input)const;
 LoopbackNetworkTransport3D& transport_;NetworkEndpoint3D endpoint_{},server_{};std::uint64_t sessionEpoch_{},tick_{},sequence_{},latestSnapshotTick_{};std::uint32_t clientId_{};ReplicationRuntimeSettings3D settings_{};ReplicationInputCommand3D pending_{};ReplicationPlayerState3D predicted_{};std::deque<PredictedFrame> history_{};std::map<NetworkEntityId3D,std::pair<ReplicationPlayerState3D,ReplicationPlayerState3D>> remote_{};ReplicationRuntimeStats3D stats_{};
};
}
