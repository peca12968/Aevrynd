#include "Aevrynd/ECS.h"

namespace Aevrynd {

Entity EntityRegistry::Create(){
    uint32_t index{};
    if(!freeIndices_.empty()){
        index=freeIndices_.back();freeIndices_.pop_back();alive_[index]=1;
    }else{
        if(generations_.size()>=MaxEntities)throw std::length_error("Entity registry capacity exceeded");
        index=uint32_t(generations_.size());
        generations_.push_back(1);
        try{alive_.push_back(1);deferredFlags_.push_back(0);}catch(...){if(alive_.size()>generations_.size()-1)alive_.pop_back();generations_.pop_back();throw;}
    }
    ++livingCount_;
    return{index,generations_[index]};
}

bool EntityRegistry::Destroy(Entity entity){
    if(!IsAlive(entity))return false;
    uint32_t& generation=generations_[entity.index];
    const bool reusable=generation!=std::numeric_limits<uint32_t>::max();
    if(reusable)freeIndices_.push_back(entity.index);
    deferredFlags_[entity.index]=0;
    alive_[entity.index]=0;
    if(reusable)++generation;
    --livingCount_;
    return true;
}

bool EntityRegistry::IsAlive(Entity entity)const{return entity&&entity.index<generations_.size()&&alive_[entity.index]&&generations_[entity.index]==entity.generation;}

bool EntityRegistry::DeferDestroy(Entity entity){if(!IsAlive(entity)||deferredFlags_[entity.index])return false;deferredDestroy_.push_back(entity);deferredFlags_[entity.index]=1;return true;}

size_t EntityRegistry::FlushDeferredDestroy(){freeIndices_.reserve(freeIndices_.size()+deferredDestroy_.size());size_t destroyed=0;for(Entity entity:deferredDestroy_){if(entity.index<deferredFlags_.size())deferredFlags_[entity.index]=0;destroyed+=Destroy(entity);}deferredDestroy_.clear();return destroyed;}

ECSMemoryStats EntityRegistry::MemoryStats()const{
    const size_t resident=generations_.size()*sizeof(uint32_t)+alive_.size()*sizeof(uint8_t)+deferredFlags_.size()*sizeof(uint8_t)+freeIndices_.size()*sizeof(uint32_t)+deferredDestroy_.size()*sizeof(Entity);
    const size_t capacity=generations_.capacity()*sizeof(uint32_t)+alive_.capacity()*sizeof(uint8_t)+deferredFlags_.capacity()*sizeof(uint8_t)+freeIndices_.capacity()*sizeof(uint32_t)+deferredDestroy_.capacity()*sizeof(Entity);
    return{resident,capacity,capacity-resident,lastReleasedBytes_,trimCount_};
}

size_t EntityRegistry::TrimMemory(){
    const size_t before=MemoryStats().capacityBytes;
    generations_.shrink_to_fit();alive_.shrink_to_fit();deferredFlags_.shrink_to_fit();freeIndices_.shrink_to_fit();deferredDestroy_.shrink_to_fit();
    const size_t after=MemoryStats().capacityBytes;
    lastReleasedBytes_=before>after?before-after:0;++trimCount_;
    return lastReleasedBytes_;
}

}
