#pragma once
#include "Scene2D.h"
#include <span>

namespace Aevrynd {
struct SceneEntry25D {
    Vec2 foot{};
    float depth{};
    float occlusionRadius{};
    float occlusionAmount{};
    std::size_t index{};
    std::size_t group{};
};

struct Scene25DStats {
    std::size_t entries{};
    std::size_t capacity{};
    std::size_t peakEntries{};
    std::uint64_t finalizations{};
    std::uint64_t smallSorts{};
    std::uint64_t largeSorts{};
    OcclusionField2DStats occlusion{};
};

class Scene25D {
public:
    explicit Scene25D(std::size_t entryCapacity=0,std::size_t occluderCapacity=0):occlusion_(occluderCapacity){Reserve(entryCapacity);}
    void Clear(){entries_.clear();occlusion_.Clear();}
    void Reserve(std::size_t capacity){retainedCapacity_=capacity;entries_.reserve(capacity);}
    void AddOccluder(VerticalOccluder2D occluder){occlusion_.Add(occluder);}
    void Add(Vec2 foot,std::size_t index,std::size_t group=0,float occlusionRadius=0.f,float depthBias=0.f){entries_.push_back({foot,std::isfinite(foot.y+depthBias)?foot.y+depthBias:0.f,(std::max)(0.f,occlusionRadius),0.f,index,group});stats_.peakEntries=(std::max)(stats_.peakEntries,entries_.size());}
    void Finalize(){for(SceneEntry25D& entry:entries_)entry.occlusionAmount=entry.occlusionRadius>0.f?occlusion_.Query(entry.foot,entry.occlusionRadius):0.f;++stats_.finalizations;if(entries_.size()<=SmallBatchLimit){++stats_.smallSorts;for(std::size_t index=1;index<entries_.size();++index){SceneEntry25D current=entries_[index];std::size_t destination=index;while(destination>0&&current.depth<entries_[destination-1].depth){entries_[destination]=entries_[destination-1];--destination;}entries_[destination]=current;}}else{++stats_.largeSorts;std::stable_sort(entries_.begin(),entries_.end(),[](const SceneEntry25D& left,const SceneEntry25D& right){return left.depth<right.depth;});}}
    [[nodiscard]] std::span<const SceneEntry25D> Entries()const{return entries_;}
    [[nodiscard]] Scene25DStats Stats()const{Scene25DStats result=stats_;result.entries=entries_.size();result.capacity=entries_.capacity();result.occlusion=occlusion_.Stats();return result;}
    [[nodiscard]] ReusableMemoryStats MemoryStats()const{const ReusableMemoryStats occlusionMemory=occlusion_.MemoryStats();const std::size_t entryBytes=entries_.capacity()*sizeof(SceneEntry25D),target=(std::max)(entries_.size(),retainedCapacity_),reclaimable=entries_.capacity()>target?(entries_.capacity()-target)*sizeof(SceneEntry25D):0;return{entryBytes+occlusionMemory.retainedCapacityBytes,reclaimable+occlusionMemory.reclaimableCapacityBytes,lastReleasedBytes_,trimCount_};}
    std::size_t TrimUnusedMemory(){const std::size_t before=MemoryStats().retainedCapacityBytes;std::vector<SceneEntry25D> replacement;replacement.reserve((std::max)(entries_.size(),retainedCapacity_));replacement.insert(replacement.end(),entries_.begin(),entries_.end());entries_.swap(replacement);occlusion_.TrimUnusedMemory();const std::size_t after=MemoryStats().retainedCapacityBytes;lastReleasedBytes_=before>after?before-after:0;++trimCount_;return lastReleasedBytes_;}
    static constexpr std::size_t SmallBatchLimit=32;
private:
    std::vector<SceneEntry25D> entries_{};
    OcclusionField2D occlusion_{};
    Scene25DStats stats_{};
    std::size_t retainedCapacity_{},lastReleasedBytes_{};
    std::uint64_t trimCount_{};
};
}
