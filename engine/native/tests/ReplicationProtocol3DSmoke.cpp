#include <Aevrynd/NetworkTransport3D.h>
#include <Aevrynd/ReplicationProtocol3D.h>
#include <iostream>

using namespace Aevrynd;
int main(){
 std::vector<ReplicationMessage3D> messages;
 messages.emplace_back(ReplicationHandshake3D{7,2,ReplicationProtocolVersion3D});
 messages.emplace_back(ReplicationInputCommand3D{7,99,4,2,123,-456,7,true});
 ReplicationSnapshot3D snapshot{7,100,98,3,5,2,true};snapshot.entities.push_back({{1,2},{{-5,0,8},{31.125f,2.5f,.25f}},{1,2,3},9});snapshot.despawned.push_back({3,4});messages.emplace_back(snapshot);
 messages.emplace_back(ReplicationAck3D{7,100,99,2});
 WorldChunkDeltaOperation3D operation{4,2,100,{8,9},SetTerrainHeightDelta3D{2,3,4.5f}};messages.emplace_back(ReplicationMutationRequest3D{7,22,2,{{10,11},3,1,{-2,0,7}},operation});
 messages.emplace_back(ReplicationMutationReceipt3D{7,22,100,4,2,true,true,{8,9}});
 for(const auto& source:messages){std::vector<std::byte> bytes;ReplicationPacketReceipt3D encoded{},decoded{};if(!EncodeReplicationPacket3D(source,bytes,&encoded)||bytes.empty())return 1;ReplicationMessage3D target;if(!DecodeReplicationPacket3D(bytes,target,&decoded)||target.index()!=source.index()||encoded.sha256!=decoded.sha256||encoded.crc32c!=decoded.crc32c)return 2;auto corrupt=bytes;corrupt.back()^=std::byte{1};if(DecodeReplicationPacket3D(corrupt,target))return 3;for(std::size_t n=0;n<bytes.size();++n)if(DecodeReplicationPacket3D(std::span<const std::byte>(bytes).first(n),target))return 4;}
 ReplicationSnapshot3D invalid=snapshot;invalid.entities[0].velocity.x=std::numeric_limits<float>::quiet_NaN();std::vector<std::byte> bytes;if(EncodeReplicationPacket3D(invalid,bytes))return 5;
 ReplicationCodecLimits3D tiny{};tiny.maximumEntities=0;if(EncodeReplicationPacket3D(snapshot,bytes,nullptr,tiny))return 6;
 LoopbackNetworkSettings3D settings{};settings.maximumEndpoints=3;settings.maximumQueuedPackets=16;settings.maximumQueuedBytes=4096;settings.maximumPacketBytes=1024;settings.seed=77;settings.lossPermille=200;settings.duplicatePermille=300;settings.reorderPermille=400;settings.maximumJitterTicks=3;LoopbackNetworkTransport3D a(settings),b(settings);for(auto endpoint:{NetworkEndpoint3D{1},NetworkEndpoint3D{2},NetworkEndpoint3D{3}})if(!a.RegisterEndpoint(endpoint)||!b.RegisterEndpoint(endpoint))return 7;
 std::array<std::byte,4> payload{};for(int i=0;i<100;++i){if(!a.Send({1},{2},payload)||!b.Send({1},{2},payload))return 8;a.AdvanceTo(i);b.AdvanceTo(i);NetworkPacket3D pa,pb;while(a.TryReceive({2},pa)){if(!b.TryReceive({2},pb)||pa.deliveryTick!=pb.deliveryTick||pa.bytes!=pb.bytes)return 9;}if(b.TryReceive({2},pb))return 10;}a.AdvanceTo(1000);b.AdvanceTo(1000);NetworkPacket3D pa,pb;while(a.TryReceive({2},pa))if(!b.TryReceive({2},pb)||pa.deliveryTick!=pb.deliveryTick)return 11;if(b.TryReceive({2},pb))return 12;if(a.Stats().lost==0||a.Stats().duplicated==0||a.Stats().reordered==0)return 13;
 LoopbackNetworkSettings3D bounded{};bounded.maximumQueuedPackets=1;bounded.maximumQueuedBytes=4;bounded.maximumPacketBytes=4;LoopbackNetworkTransport3D transport(bounded);transport.RegisterEndpoint({1});transport.RegisterEndpoint({2});if(!transport.Send({1},{2},payload)||transport.Send({1},{2},payload)||transport.Stats().backpressureRejected!=1)return 14;transport.Reset();if(transport.Stats().queuedBytes||transport.Stats().endpoints)return 15;
 std::cout<<"messages="<<messages.size()<<" loss="<<a.Stats().lost<<" duplicated="<<a.Stats().duplicated<<" reordered="<<a.Stats().reordered<<'\n';return 0;
}
