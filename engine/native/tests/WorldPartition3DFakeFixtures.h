#pragma once
#include <Aevrynd/WorldPartition3D.h>
#include <deque>
#include <unordered_map>
#include <utility>
#include <stdexcept>
#include <vector>

namespace Aevrynd::Tests {
class FakeChunkSource3D final:public IWorldChunkSource3D {
 struct Pending{WorldChunkRequest3D request;std::stop_token cancellation;};std::deque<Pending> pending_;std::deque<WorldChunkLoadCompletion3D> completed_;
public:
 bool autoComplete{},throwNext{};size_t defaultCpuBytes{128},defaultGpuBytes{256},peakPending{},beginCalls{},trimCalls{};std::vector<WorldChunkRequest3D> began;
 bool BeginLoad(WorldChunkRequest3D request,std::stop_token cancellation)override{++beginCalls;if(std::exchange(throwNext,false))throw std::runtime_error("Injected source failure");began.push_back(request);pending_.push_back({request,cancellation});peakPending=(std::max)(peakPending,pending_.size());if(autoComplete)CompleteOne(defaultCpuBytes,defaultGpuBytes);return true;}
 bool TryPopCompletion(WorldChunkLoadCompletion3D& value)override{if(completed_.empty())return false;value=std::move(completed_.front());completed_.pop_front();return true;}
 void CompleteOne(size_t cpuBytes=128,size_t gpuBytes=256){if(pending_.empty())return;auto pending=std::move(pending_.front());pending_.pop_front();auto storage=std::make_shared<uint32_t>(pending.request.seed);completed_.push_back({pending.request,{storage,cpuBytes,gpuBytes},true});}
 void CompleteAll(){while(!pending_.empty())CompleteOne();}
 void WaitIdle()override{CompleteAll();}
 size_t TrimUnusedMemory()override{++trimCalls;const size_t before=pending_.size()*sizeof(Pending)+completed_.size()*sizeof(WorldChunkLoadCompletion3D);decltype(pending_){}.swap(pending_);decltype(completed_){}.swap(completed_);return before;}
 size_t PendingCount()const{return pending_.size();}
};

class FakeActivationTarget3D final:public IWorldChunkActivationTarget3D {
 uint64_t next_{1};bool activationComplete_{},retirementComplete_{};std::unordered_map<uint64_t,size_t> bytes_;
public:
 size_t activates{},rollbacks{},retires{},logicalReferences{},physicalAllocations{},trimCalls{};size_t actualGpuBytesOverride{};bool failNext{},sharePhysical{};
 size_t PreparationBytes(WorldChunkRequest3D,const WorldChunkPayload3D& payload)const override{return payload.cpuBytes/2;}
 size_t PhysicalAllocationBytes(WorldChunkRequest3D,const WorldChunkPayload3D& payload)const override{return sharePhysical&&physicalAllocations?0:payload.gpuBytes;}
 WorldChunkActivation3D Activate(WorldChunkRequest3D,const WorldChunkPayload3D& payload)override{++activates;if(std::exchange(failNext,false))return{};const uint64_t id=next_++;const size_t bytes=actualGpuBytesOverride?actualGpuBytesOverride:(sharePhysical&&physicalAllocations?0:payload.gpuBytes);bytes_[id]=bytes;++logicalReferences;if(bytes)++physicalAllocations;return{id,bytes};}
 bool IsActivationComplete(WorldChunkActivation3D)const override{return activationComplete_;}
 void Rollback(WorldChunkActivation3D activation)override{if(bytes_.erase(activation.value)&&logicalReferences)--logicalReferences;++rollbacks;}
 WorldChunkActivation3D BeginRetire(WorldChunkActivation3D activation)override{++retires;return activation;}
 bool IsRetirementComplete(WorldChunkActivation3D)const override{return retirementComplete_;}
 void WaitIdle()override{activationComplete_=retirementComplete_=true;bytes_.clear();}
 size_t TrimUnusedMemory()override{++trimCalls;return 0;}
 void CompleteActivations(){activationComplete_=true;}
 void CompleteRetirements(){retirementComplete_=true;}
 void HoldActivations(){activationComplete_=false;}
 void HoldRetirements(){retirementComplete_=false;}
};
}
