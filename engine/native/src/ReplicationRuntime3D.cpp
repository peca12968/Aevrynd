#include "Aevrynd/ReplicationRuntime3D.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Aevrynd {namespace {
bool Valid(const ReplicationRuntimeSettings3D&s){return s.tickRate&&s.maximumClients&&s.maximumInputHistory&&s.maximumSnapshotHistory&&s.maximumPacketsPerTick&&s.maximumSnapshotEntities&&s.interestLoadRadius>=0&&s.interestRetainRadius>=s.interestLoadRadius&&std::isfinite(s.interestChunkSize)&&s.interestChunkSize>0&&std::isfinite(s.movementPerTick)&&s.movementPerTick>0&&s.movementLateGraceTicks<=s.maximumInputHistory&&std::isfinite(s.sprintMovementMultiplier)&&s.sprintMovementMultiplier>=1.f&&std::isfinite(s.movementPerTick*s.sprintMovementMultiplier);}
bool ChunkOf(GlobalPosition3D p,float size,std::pair<std::int64_t,std::int64_t>&out){if(!NormalizeGlobalPosition3D(p))return false;const long double x=static_cast<long double>(p.chunk.x)*32+p.offset.x,z=static_cast<long double>(p.chunk.z)*32+p.offset.z,cx=std::floor(x/size),cz=std::floor(z/size);if(cx<(std::numeric_limits<std::int64_t>::min)()||cx>(std::numeric_limits<std::int64_t>::max)()||cz<(std::numeric_limits<std::int64_t>::min)()||cz>(std::numeric_limits<std::int64_t>::max)())return false;out={static_cast<std::int64_t>(cx),static_cast<std::int64_t>(cz)};return true;}
bool Contains(const std::set<std::pair<std::int64_t,std::int64_t>>&set,GlobalPosition3D p,float size){std::pair<std::int64_t,std::int64_t> key{};return ChunkOf(p,size,key)&&set.contains(key);}
void Move(ReplicationPlayerState3D& state,const ReplicationInputCommand3D& input,float scale,
          const ReplicationMovementConstraint3D& constraint={},std::optional<Vec3> serverStep={}) {
    Vec3 delta=serverStep.value_or(Vec3{float(input.moveX)/32767.f*scale,0,float(input.moveZ)/32767.f*scale});
    if(delta.y!=0.f||!std::isfinite(delta.x)||!std::isfinite(delta.z))delta={};
    if(constraint&&(delta.x!=0.f||delta.z!=0.f)){
        const auto proposal=constraint(state,delta);
        const auto squared=[](Vec3 v){return double(v.x)*v.x+double(v.z)*v.z;};
        const double proposed=squared(proposal);
        delta=proposal.y==0.f&&std::isfinite(proposed)&&proposed<=squared(delta)*1.000001?proposal:Vec3{};
    }
    GlobalPosition3D next{};
    if(AddGlobalPosition3D(state.position,delta,next)){state.position=next;state.velocity=delta;}
    state.tick=input.tick;
}
}
ReplicationServer3D::ReplicationServer3D(LoopbackNetworkTransport3D&t,NetworkEndpoint3D e,std::uint64_t epoch,ReplicationRuntimeSettings3D s):transport_(t),endpoint_(e),sessionEpoch_(epoch),settings_(s){if(!e||!epoch||!Valid(s))throw std::invalid_argument("Invalid replication server settings");}
bool ReplicationServer3D::AddClient(std::uint32_t id,NetworkEndpoint3D endpoint,GlobalPosition3D spawn){if(!id||!endpoint||clients_.size()>=settings_.maximumClients||clients_.contains(id)||!NormalizeGlobalPosition3D(spawn))return false;Client c{};c.endpoint=endpoint;c.player={{sessionEpoch_,id},spawn,{},0};clients_.emplace(id,std::move(c));return true;}
bool ReplicationServer3D::RemoveClient(std::uint32_t id){return clients_.erase(id)!=0;}
bool ReplicationServer3D::SetClientEntityFlags(std::uint32_t id,std::uint32_t flags){
    auto client=clients_.find(id);
    if(client==clients_.end()||(flags&1u))return false;
    client->second.entityFlags=flags;
    return true;
}
bool ReplicationServer3D::SetClientMovementEnabled(std::uint32_t id,bool enabled){
    auto client=clients_.find(id);
    if(client==clients_.end())return false;
    client->second.movementEnabled=enabled;
    if(!enabled)client->second.inputs.clear();
    return true;
}
bool ReplicationServer3D::SetClientPosition(std::uint32_t id,GlobalPosition3D position){auto found=clients_.find(id);if(found==clients_.end()||!NormalizeGlobalPosition3D(position))return false;found->second.player.position=position;found->second.player.velocity={};return true;}
void ReplicationServer3D::Receive(){
    NetworkPacket3D packet;
    for(std::size_t n=0;n<settings_.maximumPacketsPerTick&&transport_.TryReceive(endpoint_,packet);++n){
        ReplicationMessage3D message;
        if(!DecodeReplicationPacket3D(packet.bytes,message)){++stats_.packetRejects;continue;}
        if(auto* input=std::get_if<ReplicationInputCommand3D>(&message)){
            const auto found=clients_.find(input->clientId);
            if(found==clients_.end()||packet.source!=found->second.endpoint||input->sessionEpoch!=sessionEpoch_){
                ++stats_.packetRejects;continue;
            }
            auto& client=found->second;
            if(tick_>settings_.maximumInputHistory&&input->tick<tick_-settings_.maximumInputHistory){
                ++stats_.staleInputs;continue;
            }
            if(input->tick>tick_&&input->tick-tick_>settings_.maximumInputHistory){++stats_.futureInputs;continue;}
            if(input->sequence==(std::numeric_limits<std::uint64_t>::max)()||input->sequence<=client.lastInputSequence||
               input->sequence-client.lastInputSequence>settings_.maximumInputHistory){
                ++stats_.duplicateInputs;continue;
            }
            if(input->tick<=tick_&&settings_.movementLateGraceTicks){
                if(tick_-input->tick>=settings_.movementLateGraceTicks){
                    // 只記住已過期的序號，讓延遲恢復後的新指令可以繼續執行。
                    client.lastInputSequence=input->sequence;
                    ++stats_.staleInputs;continue;
                }
                // 遲到的移動只可放進下一個可執行週期，同一週期仍只消耗一次移動。
                input->tick=tick_+1;
            }
            if(client.inputs.contains(input->tick)){++stats_.duplicateInputs;continue;}
            client.inputs.emplace(input->tick,*input);
            client.lastInputSequence=input->sequence;
            while(client.inputs.size()>settings_.maximumInputHistory){
                client.inputs.erase(client.inputs.begin());
                ++stats_.historyDrops;
            }
            ++stats_.inputsReceived;
        }else if(auto* ack=std::get_if<ReplicationAck3D>(&message)){
            const auto found=clients_.find(ack->clientId);
            if(found==clients_.end()||packet.source!=found->second.endpoint||ack->sessionEpoch!=sessionEpoch_){
                ++stats_.packetRejects;continue;
            }
            auto& client=found->second;
            if(ack->snapshotTick>tick_||ack->snapshotTick<=client.ackSnapshotTick||
               std::ranges::none_of(client.snapshots,[&](const auto& snapshot){return snapshot.serverTick==ack->snapshotTick;})){
                ++stats_.packetRejects;continue;
            }
            client.ackSnapshotTick=ack->snapshotTick;
            ++stats_.acks;
        }else{
            ++stats_.packetRejects;
        }
    }
}
void ReplicationServer3D::SendSnapshots(){for(auto&[id,c]:clients_){std::pair<std::int64_t,std::int64_t> center{};if(!ChunkOf(c.player.position,settings_.interestChunkSize,center)){++stats_.packetRejects;continue;}std::set<std::pair<std::int64_t,std::int64_t>> next;for(const auto&v:c.interest)if((std::max)(std::llabs(v.first-center.first),std::llabs(v.second-center.second))<=settings_.interestRetainRadius)next.insert(v);for(int z=-settings_.interestLoadRadius;z<=settings_.interestLoadRadius;++z)for(int x=-settings_.interestLoadRadius;x<=settings_.interestLoadRadius;++x)next.emplace(center.first+x,center.second+z);if(next!=c.interest){for(const auto&chunk:next)if(!c.interest.contains(chunk))++c.interestEntered;for(const auto&chunk:c.interest)if(!next.contains(chunk))++c.interestExited;c.interest=std::move(next);if(c.interestEpoch==(std::numeric_limits<std::uint64_t>::max)()){++stats_.packetRejects;continue;}++c.interestEpoch;}ReplicationSnapshot3D s{};s.sessionEpoch=sessionEpoch_;s.serverTick=tick_;s.clientId=id;s.interestEpoch=c.interestEpoch;s.originGeneration=1;s.baselineTick=c.ackSnapshotTick;const auto baseline=std::ranges::find_if(c.snapshots,[&](const auto&x){return x.serverTick==c.ackSnapshotTick;});s.full=c.ackSnapshotTick==0||baseline==c.snapshots.end();std::set<NetworkEntityId3D> visible;for(const auto&[otherId,other]:clients_){if(!Contains(c.interest,other.player.position,settings_.interestChunkSize))continue;visible.insert(other.player.entity);if(visible.size()>settings_.maximumSnapshotEntities){++stats_.packetRejects;break;}ReplicatedEntityState3D state{other.player.entity,other.player.position,other.player.velocity,other.entityFlags|(otherId==id?1u:0u)};bool changed=s.full;if(!changed){const auto previous=std::ranges::find_if(baseline->entities,[&](const auto&value){return value.id==state.id;});changed=previous==baseline->entities.end()||previous->position!=state.position||previous->velocity.x!=state.velocity.x||previous->velocity.y!=state.velocity.y||previous->velocity.z!=state.velocity.z||previous->flags!=state.flags;}if(changed)s.entities.push_back(state);}for(auto old:c.visible)if(!visible.contains(old))s.despawned.push_back(old);for(const auto entity:visible){if(!c.visible.contains(entity)&&c.everVisible.contains(entity))++c.entityReentries;c.everVisible.insert(entity);}c.entityDespawns+=s.despawned.size();c.visible=std::move(visible);std::vector<std::byte> bytes;if(s.entities.size()>settings_.maximumSnapshotEntities||!EncodeReplicationPacket3D(s,bytes)||!transport_.Send(endpoint_,c.endpoint,bytes)){++stats_.packetRejects;continue;}c.lastSnapshotEntities=s.entities.size();c.lastSnapshotDespawns=s.despawned.size();ReplicationSnapshot3D stored=s;stored.entities.clear();for(const auto&[otherId,other]:clients_)if(c.visible.contains(other.player.entity))stored.entities.push_back({other.player.entity,other.player.position,other.player.velocity,other.entityFlags|(otherId==id?1u:0u)});c.snapshots.push_back(std::move(stored));while(c.snapshots.size()>settings_.maximumSnapshotHistory)c.snapshots.pop_front();++stats_.snapshotsSent;if(s.full)++stats_.fullSnapshots;else ++stats_.deltaSnapshots;}}
void ReplicationServer3D::Tick(const ReplicationMovementConstraint3D& constraint,const ReplicationServerStep3D& serverStep,
                               const ReplicationVerticalStep3D& verticalStep){
    Receive();
    ++tick_;
    for(auto&[id,c]:clients_){
        ReplicationInputCommand3D input{sessionEpoch_,tick_,c.lastInputSequence,id,0,0,0,false};
        bool applied=false;
        if(auto found=c.inputs.find(tick_);found!=c.inputs.end()){
            if(c.movementEnabled){input=found->second;applied=true;}
            c.inputs.erase(found);
            c.lastInputTick=tick_;
        }
        const auto step=serverStep&&c.movementEnabled?serverStep(c.player):std::nullopt;
        Move(c.player,input,settings_.movementPerTick*(input.sprint?settings_.sprintMovementMultiplier:1.f),constraint,step);
        if(verticalStep){
            const float deltaY=verticalStep(c.player,applied?&input:nullptr,c.movementEnabled);
            GlobalPosition3D next{};
            if(deltaY!=0.f&&std::isfinite(deltaY)&&AddGlobalPosition3D(c.player.position,{0,deltaY,0},next)){
                c.player.position=next;
                c.player.velocity.y=deltaY;
            }
        }
    }
    SendSnapshots();
    ++stats_.ticks;
}
std::optional<ReplicationPlayerState3D> ReplicationServer3D::Player(std::uint32_t id)const{auto found=clients_.find(id);if(found==clients_.end())return{};return found->second.player;}
std::optional<ReplicationServerClientTelemetry3D> ReplicationServer3D::ClientTelemetry(std::uint32_t id)const{auto found=clients_.find(id);if(found==clients_.end())return{};const auto&client=found->second;return ReplicationServerClientTelemetry3D{id,client.interestEpoch,client.interestEntered,client.interestExited,client.entityReentries,client.entityDespawns,client.interest.size(),client.visible.size(),client.lastSnapshotEntities,client.lastSnapshotDespawns,client.player.position};}

ReplicationClient3D::ReplicationClient3D(LoopbackNetworkTransport3D&t,NetworkEndpoint3D e,NetworkEndpoint3D server,std::uint64_t epoch,std::uint32_t id,ReplicationRuntimeSettings3D s):transport_(t),endpoint_(e),server_(server),sessionEpoch_(epoch),clientId_(id),settings_(s){if(!e||!server||!epoch||!id||!Valid(s))throw std::invalid_argument("Invalid replication client settings");predicted_.entity={epoch,id};}
bool ReplicationClient3D::SubmitInput(std::int16_t x,std::int16_t z,bool jump){pending_={sessionEpoch_,tick_+1,sequence_+1,clientId_,x,z,0,jump};return true;}
void ReplicationClient3D::Simulate(ReplicationPlayerState3D&s,const ReplicationInputCommand3D&i)const{Move(s,i,settings_.movementPerTick*(i.sprint?settings_.sprintMovementMultiplier:1.f));}
void ReplicationClient3D::Apply(const ReplicationSnapshot3D&s){if(s.sessionEpoch!=sessionEpoch_||s.clientId!=clientId_||s.serverTick<=latestSnapshotTick_)return;latestSnapshotTick_=s.serverTick;const ReplicatedEntityState3D* own{};for(const auto&e:s.entities){ReplicationPlayerState3D state{e.id,e.position,e.velocity,s.serverTick};if(e.id==predicted_.entity)own=&e;else{auto&pair=remote_[e.id];pair.first=pair.second.entity?pair.second:state;pair.second=state;}}for(auto id:s.despawned)remote_.erase(id);if(own){predicted_={own->id,own->position,own->velocity,s.serverTick};while(!history_.empty()&&history_.front().input.tick<=s.serverTick)history_.pop_front();for(auto&frame:history_){Simulate(predicted_,frame.input);frame.state=predicted_;++stats_.replayedInputs;}++stats_.reconciliations;}ReplicationAck3D ack{sessionEpoch_,s.serverTick,predicted_.tick,clientId_};std::vector<std::byte>bytes;if(EncodeReplicationPacket3D(ack,bytes))transport_.Send(endpoint_,server_,bytes);}
void ReplicationClient3D::Receive(){NetworkPacket3D packet;for(std::size_t n=0;n<settings_.maximumPacketsPerTick&&transport_.TryReceive(endpoint_,packet);++n){ReplicationMessage3D message;if(packet.source!=server_||!DecodeReplicationPacket3D(packet.bytes,message)){++stats_.packetRejects;continue;}if(auto*s=std::get_if<ReplicationSnapshot3D>(&message))Apply(*s);else ++stats_.packetRejects;}}
void ReplicationClient3D::Tick(){Receive();++tick_;ReplicationInputCommand3D input=pending_;if(input.tick!=tick_)input={sessionEpoch_,tick_,sequence_+1,clientId_,0,0,0,false};input.sequence=++sequence_;std::vector<std::byte>bytes;if(EncodeReplicationPacket3D(input,bytes))transport_.Send(endpoint_,server_,bytes);Simulate(predicted_,input);history_.push_back({input,predicted_});while(history_.size()>settings_.maximumInputHistory){history_.pop_front();++stats_.historyDrops;}pending_={};++stats_.ticks;}
std::optional<ReplicationPlayerState3D> ReplicationClient3D::Remote(NetworkEntityId3D id,float alpha)const{auto found=remote_.find(id);if(found==remote_.end())return{};alpha=(std::clamp)(std::isfinite(alpha)?alpha:0.f,0.f,1.f);auto value=found->second.second;Vec3 a{},b{};WorldOrigin3D origin{};origin.position=found->second.first.position;if(ToLocalPosition3D(found->second.first.position,origin,a)&&ToLocalPosition3D(found->second.second.position,origin,b)){GlobalPosition3D p;if(ToGlobalPosition3D(a+(b-a)*alpha,origin,p))value.position=p;}return value;}
}
