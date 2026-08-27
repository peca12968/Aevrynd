#pragma once
#include "Scene2D.h"
#include <span>

namespace Aevrynd {
struct SceneEntry25D {
    Vec2 foot{};
    float depth{};
    float occlusionRadius{};
    float visibilityRadius{};
    float occlusionAmount{};
    std::size_t index{};
    std::size_t group{};
};

struct Scene25DStats {
    std::size_t entries{};
    std::size_t submittedEntries{};
    std::size_t submittedOccluders{};
    std::size_t visibilityCulled{};
    std::size_t earlyVisibilityCulled{};
    std::size_t earlyOccludersCulled{};
    std::size_t occlusionEntries{};
    std::size_t capacity{};
    std::size_t peakEntries{};
    std::uint64_t finalizations{};
    std::uint64_t visibilityFinalizations{};
    std::uint64_t smallSorts{};
    std::uint64_t largeSorts{};
    std::uint64_t orderedSortSkips{};
    std::uint64_t adaptiveInsertionSorts{};
    std::uint64_t fullStableSorts{};
    std::uint64_t adjacentInversions{};
    OcclusionField2DStats occlusion{};
};

class Scene25D {
public:
    explicit Scene25D(std::size_t entryCapacity=0,std::size_t occluderCapacity=0):occlusion_(occluderCapacity){Reserve(entryCapacity);}
    void Clear(){entries_.clear();occlusion_.Clear();pendingSubmittedEntries_=pendingSubmittedOccluders_=0;pendingEarlyVisibilityCulled_=pendingEarlyOccludersCulled_=0;prefilterMaximumOcclusionRadius_=0;prefilterActive_=prefilterOccluders_=false;}
    void BeginFrame(Bounds2D view,float maximumOcclusionTargetRadius=-1.f){Clear();prefilterActive_=NormalizeView(view,prefilterView_);prefilterOccluders_=prefilterActive_&&std::isfinite(maximumOcclusionTargetRadius)&&maximumOcclusionTargetRadius>=0.f;if(prefilterOccluders_)prefilterMaximumOcclusionRadius_=maximumOcclusionTargetRadius;}
    void Reserve(std::size_t capacity){retainedCapacity_=capacity;entries_.reserve(capacity);}
    void AddOccluder(VerticalOccluder2D occluder){++pendingSubmittedOccluders_;if(prefilterOccluders_&&OutsideView(occluder,prefilterMaximumOcclusionRadius_,prefilterView_)){++pendingEarlyOccludersCulled_;return;}occlusion_.Add(occluder);}
    void Add(Vec2 foot,std::size_t index,std::size_t group=0,float occlusionRadius=0.f,float depthBias=0.f,float visibilityRadius=0.f){++pendingSubmittedEntries_;const float safeVisibilityRadius=std::isfinite(visibilityRadius)?(std::max)(0.f,visibilityRadius):0.f;if(prefilterActive_&&OutsideView(foot,safeVisibilityRadius,prefilterView_)){++pendingEarlyVisibilityCulled_;return;}entries_.push_back({foot,std::isfinite(foot.y+depthBias)?foot.y+depthBias:0.f,std::isfinite(occlusionRadius)?(std::max)(0.f,occlusionRadius):0.f,safeVisibilityRadius,0.f,index,group});stats_.peakEntries=(std::max)(stats_.peakEntries,entries_.size());}
    void Finalize(){FinalizeInternal(nullptr);}
    void Finalize(Bounds2D view){FinalizeInternal(&view);}
    [[nodiscard]] std::span<const SceneEntry25D> Entries()const{return entries_;}
    [[nodiscard]] Scene25DStats Stats()const{Scene25DStats result=stats_;result.entries=entries_.size();result.capacity=entries_.capacity();result.occlusion=occlusion_.Stats();return result;}
    [[nodiscard]] ReusableMemoryStats MemoryStats()const{const ReusableMemoryStats occlusionMemory=occlusion_.MemoryStats();const std::size_t entryBytes=entries_.capacity()*sizeof(SceneEntry25D),target=(std::max)(entries_.size(),retainedCapacity_),reclaimable=entries_.capacity()>target?(entries_.capacity()-target)*sizeof(SceneEntry25D):0;return{entryBytes+occlusionMemory.retainedCapacityBytes,reclaimable+occlusionMemory.reclaimableCapacityBytes,lastReleasedBytes_,trimCount_};}
    std::size_t TrimUnusedMemory(){const std::size_t before=MemoryStats().retainedCapacityBytes;std::vector<SceneEntry25D> replacement;replacement.reserve((std::max)(entries_.size(),retainedCapacity_));replacement.insert(replacement.end(),entries_.begin(),entries_.end());entries_.swap(replacement);occlusion_.TrimUnusedMemory();const std::size_t after=MemoryStats().retainedCapacityBytes;lastReleasedBytes_=before>after?before-after:0;++trimCount_;return lastReleasedBytes_;}
    static constexpr std::size_t SmallBatchLimit=32;
    static constexpr std::size_t AdaptiveInsertionInversionLimit=8;
private:
    static bool NormalizeView(Bounds2D view,Bounds2D& normalized){if(!std::isfinite(view.minimum.x)||!std::isfinite(view.minimum.y)||!std::isfinite(view.maximum.x)||!std::isfinite(view.maximum.y))return false;normalized={{(std::min)(view.minimum.x,view.maximum.x),(std::min)(view.minimum.y,view.maximum.y)},{(std::max)(view.minimum.x,view.maximum.x),(std::max)(view.minimum.y,view.maximum.y)}};return true;}
    static bool OutsideView(Vec2 foot,float radius,Bounds2D view){return!std::isfinite(foot.x)||!std::isfinite(foot.y)||foot.x+radius<view.minimum.x||foot.x-radius>view.maximum.x||foot.y+radius<view.minimum.y||foot.y-radius>view.maximum.y;}
    static bool OutsideView(VerticalOccluder2D occluder,float targetRadius,Bounds2D view){if(!std::isfinite(occluder.foot.x)||!std::isfinite(occluder.foot.y)||!std::isfinite(occluder.halfWidth)||!std::isfinite(occluder.height)||!std::isfinite(occluder.depthPadding))return false;const float halfWidth=std::abs(occluder.halfWidth)+targetRadius,height=std::abs(occluder.height)+targetRadius,front=(std::max)(0.f,occluder.depthPadding)+targetRadius;return occluder.foot.x+halfWidth<view.minimum.x||occluder.foot.x-halfWidth>view.maximum.x||occluder.foot.y+front<view.minimum.y||occluder.foot.y-height>view.maximum.y;}
    static void InsertionSortEntries(std::vector<SceneEntry25D>& entries){for(std::size_t index=1;index<entries.size();++index){SceneEntry25D current=entries[index];std::size_t destination=index;while(destination>0&&current.depth<entries[destination-1].depth){entries[destination]=entries[destination-1];--destination;}entries[destination]=current;}}
    void SortEntries(){if(entries_.size()<=SmallBatchLimit){++stats_.smallSorts;InsertionSortEntries(entries_);return;}++stats_.largeSorts;std::size_t inversions{};for(std::size_t index=1;index<entries_.size();++index)inversions+=entries_[index].depth<entries_[index-1].depth;stats_.adjacentInversions+=inversions;if(!inversions){++stats_.orderedSortSkips;return;}if(inversions<=AdaptiveInsertionInversionLimit){++stats_.adaptiveInsertionSorts;InsertionSortEntries(entries_);return;}++stats_.fullStableSorts;std::stable_sort(entries_.begin(),entries_.end(),[](const SceneEntry25D& left,const SceneEntry25D& right){return left.depth<right.depth;});}
    void FinalizeInternal(const Bounds2D* view){stats_.submittedEntries=pendingSubmittedEntries_;stats_.submittedOccluders=pendingSubmittedOccluders_;stats_.earlyVisibilityCulled=pendingEarlyVisibilityCulled_;stats_.earlyOccludersCulled=pendingEarlyOccludersCulled_;stats_.visibilityCulled=pendingEarlyVisibilityCulled_;Bounds2D normalizedView{};const bool postfilter=view&&NormalizeView(*view,normalizedView);if(prefilterActive_||postfilter)++stats_.visibilityFinalizations;if(postfilter){const auto end=std::remove_if(entries_.begin(),entries_.end(),[=](const SceneEntry25D& entry){return OutsideView(entry.foot,entry.visibilityRadius,normalizedView);});stats_.visibilityCulled+=std::size_t(entries_.end()-end);entries_.erase(end,entries_.end());}stats_.occlusionEntries=std::count_if(entries_.begin(),entries_.end(),[](const SceneEntry25D& entry){return entry.occlusionRadius>0.f;});occlusion_.PrepareQueries(stats_.occlusionEntries);for(SceneEntry25D& entry:entries_)entry.occlusionAmount=entry.occlusionRadius>0.f?occlusion_.Query(entry.foot,entry.occlusionRadius):0.f;++stats_.finalizations;SortEntries();}
    std::vector<SceneEntry25D> entries_{};
    OcclusionField2D occlusion_{};
    Scene25DStats stats_{};
    Bounds2D prefilterView_{};
    std::size_t retainedCapacity_{},lastReleasedBytes_{};
    std::size_t pendingSubmittedEntries_{},pendingSubmittedOccluders_{},pendingEarlyVisibilityCulled_{},pendingEarlyOccludersCulled_{};
    std::uint64_t trimCount_{};
    float prefilterMaximumOcclusionRadius_{};
    bool prefilterActive_{},prefilterOccluders_{};
};
}
