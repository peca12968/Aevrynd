#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Aevrynd {

struct Entity {
    uint32_t index{std::numeric_limits<uint32_t>::max()};
    uint32_t generation{};
    static constexpr uint32_t InvalidIndex=std::numeric_limits<uint32_t>::max();
    constexpr explicit operator bool()const{return index!=InvalidIndex&&generation!=0;}
    constexpr bool operator==(const Entity&)const=default;
};

struct ECSMemoryStats {
    size_t residentBytes{};
    size_t capacityBytes{};
    size_t reclaimableBytes{};
    size_t lastReleasedBytes{};
    uint64_t trimCount{};
};

class EntityRegistry {
public:
    static constexpr uint32_t MaxEntities=16u*1024u*1024u;
    Entity Create();
    bool Destroy(Entity entity);
    bool IsAlive(Entity entity)const;
    bool DeferDestroy(Entity entity);
    size_t FlushDeferredDestroy();
    size_t LivingCount()const{return livingCount_;}
    size_t SlotCount()const{return generations_.size();}
    size_t DeferredDestroyCount()const{return deferredDestroy_.size();}
    ECSMemoryStats MemoryStats()const;
    size_t TrimMemory();
private:
    std::vector<uint32_t> generations_;
    std::vector<uint8_t> alive_;
    std::vector<uint8_t> deferredFlags_;
    std::vector<uint32_t> freeIndices_;
    std::vector<Entity> deferredDestroy_;
    size_t livingCount_{};
    size_t lastReleasedBytes_{};
    uint64_t trimCount_{};
};

template<class Component>
class SparseSet {
public:
    static constexpr uint32_t MaxSparseIndex=EntityRegistry::MaxEntities-1;
    static_assert(std::is_nothrow_move_constructible_v<Component>,"SparseSet components must be nothrow move constructible");
    static_assert(std::is_nothrow_move_assignable_v<Component>,"SparseSet components must be nothrow move assignable");

    bool Contains(Entity entity)const {
        if(!entity||entity.index>=sparse_.size())return false;
        const uint32_t denseSlot=sparse_[entity.index];
        return denseSlot!=0&&denseSlot-1<entities_.size()&&entities_[denseSlot-1]==entity;
    }

    Component* TryGet(Entity entity){const size_t index=DenseIndex(entity);return index==Missing?nullptr:&components_[index];}
    const Component* TryGet(Entity entity)const{const size_t index=DenseIndex(entity);return index==Missing?nullptr:&components_[index];}

    template<class... Arguments>
    Component& Emplace(Entity entity,Arguments&&... arguments){
        if(!entity||entity.index>MaxSparseIndex)throw std::length_error("Entity index exceeds sparse-set limit");
        if(entity.index>=sparse_.size())sparse_.resize(size_t(entity.index)+1);
        const uint32_t denseSlot=sparse_[entity.index];
        if(denseSlot&&denseSlot-1<entities_.size()){
            const size_t denseIndex=denseSlot-1;
            Component replacement(std::forward<Arguments>(arguments)...);
            components_[denseIndex]=std::move(replacement);
            entities_[denseIndex]=entity;
            return components_[denseIndex];
        }
        if(denseSlot)sparse_[entity.index]=0;
        const size_t denseIndex=entities_.size();
        entities_.push_back(entity);
        try{components_.emplace_back(std::forward<Arguments>(arguments)...);}catch(...){entities_.pop_back();throw;}
        sparse_[entity.index]=uint32_t(denseIndex+1);
        return components_.back();
    }

    bool Remove(Entity entity){const size_t index=DenseIndex(entity);if(index==Missing)return false;RemoveDense(index);return true;}
    size_t RemoveDead(const EntityRegistry& registry){size_t removed=0;for(size_t index=entities_.size();index>0;){--index;if(!registry.IsAlive(entities_[index])){RemoveDense(index);++removed;}}return removed;}
    void Clear(){sparse_.clear();entities_.clear();components_.clear();}
    size_t Size()const{return entities_.size();}
    bool Empty()const{return entities_.empty();}
    std::span<const Entity> Entities()const{return entities_;}
    std::span<Component> Components(){return components_;}
    std::span<const Component> Components()const{return components_;}

    template<class Function>
    void Each(Function&& function){for(size_t index=0;index<entities_.size();++index)std::invoke(function,entities_[index],components_[index]);}
    template<class Function>
    void Each(Function&& function)const{for(size_t index=0;index<entities_.size();++index)std::invoke(function,entities_[index],components_[index]);}

    ECSMemoryStats MemoryStats()const {
        const size_t resident=sparse_.size()*sizeof(uint32_t)+entities_.size()*sizeof(Entity)+components_.size()*sizeof(Component);
        const size_t capacity=sparse_.capacity()*sizeof(uint32_t)+entities_.capacity()*sizeof(Entity)+components_.capacity()*sizeof(Component);
        return{resident,capacity,capacity-resident,lastReleasedBytes_,trimCount_};
    }

    size_t TrimMemory(){
        const size_t before=MemoryStats().capacityBytes;
        while(!sparse_.empty()&&sparse_.back()==0)sparse_.pop_back();
        sparse_.shrink_to_fit();entities_.shrink_to_fit();components_.shrink_to_fit();
        const size_t after=MemoryStats().capacityBytes;
        lastReleasedBytes_=before>after?before-after:0;++trimCount_;
        return lastReleasedBytes_;
    }

private:
    static constexpr size_t Missing=std::numeric_limits<size_t>::max();
    size_t DenseIndex(Entity entity)const {
        if(!entity||entity.index>=sparse_.size())return Missing;
        const uint32_t denseSlot=sparse_[entity.index];
        if(!denseSlot||denseSlot-1>=entities_.size()||entities_[denseSlot-1]!=entity)return Missing;
        return denseSlot-1;
    }
    void RemoveDense(size_t denseIndex){
        const size_t last=entities_.size()-1;
        const Entity removed=entities_[denseIndex];
        if(denseIndex!=last){
            entities_[denseIndex]=entities_[last];
            components_[denseIndex]=std::move(components_[last]);
            sparse_[entities_[denseIndex].index]=uint32_t(denseIndex+1);
        }
        entities_.pop_back();components_.pop_back();sparse_[removed.index]=0;
    }
    std::vector<uint32_t> sparse_;
    std::vector<Entity> entities_;
    std::vector<Component> components_;
    size_t lastReleasedBytes_{};
    uint64_t trimCount_{};
};

template<class Primary,class... Others>
class ComponentView {
public:
    ComponentView(const EntityRegistry& registry,SparseSet<Primary>& primary,SparseSet<Others>&... others):registry_(registry),primary_(primary),others_(&others...){}
    template<class Function>
    void Each(Function&& function){
        primary_.Each([&](Entity entity,Primary& component){
            if(!registry_.IsAlive(entity))return;
            std::apply([&](auto*... sets){
                if((sets->Contains(entity)&&...))std::invoke(function,entity,component,*sets->TryGet(entity)...);
            },others_);
        });
    }
private:
    const EntityRegistry& registry_;
    SparseSet<Primary>& primary_;
    std::tuple<SparseSet<Others>*...> others_;
};

template<class Primary,class... Others>
ComponentView<Primary,Others...> View(const EntityRegistry& registry,SparseSet<Primary>& primary,SparseSet<Others>&... others){return{registry,primary,others...};}

}
