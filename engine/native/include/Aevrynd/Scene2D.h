#pragma once
#include "Math.h"
#include "Memory.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace Aevrynd {
struct DepthEntry {
    float depth{};
    std::size_t index{};
    std::size_t group{};
};

struct VerticalOccluder2D {
    Vec2 foot{};
    float halfWidth{24.f};
    float height{64.f};
    float depthPadding{6.f};
};

inline float VerticalOcclusionAmount2D(Vec2 targetFoot, float targetRadius, VerticalOccluder2D occluder) {
    if (!std::isfinite(targetFoot.x) || !std::isfinite(targetFoot.y) || !std::isfinite(targetRadius) || !std::isfinite(occluder.foot.x) || !std::isfinite(occluder.foot.y) || !std::isfinite(occluder.halfWidth) || !std::isfinite(occluder.height) || !std::isfinite(occluder.depthPadding)) return 0.f;
    targetRadius = (std::max)(0.f, targetRadius);
    occluder.halfWidth = (std::max)(.001f, std::abs(occluder.halfWidth));
    occluder.height = (std::max)(.001f, std::abs(occluder.height));
    occluder.depthPadding = (std::max)(0.f, occluder.depthPadding);
    const float depth = occluder.foot.y - targetFoot.y;
    if (depth < -occluder.depthPadding - targetRadius || depth > occluder.height + targetRadius) return 0.f;
    const float horizontalReach = occluder.halfWidth + targetRadius;
    float horizontal = 1.f - Clamp(std::abs(targetFoot.x - occluder.foot.x) / horizontalReach, 0.f, 1.f);
    horizontal = horizontal * horizontal * (3.f - 2.f * horizontal);
    const float topFade = Clamp((occluder.height + targetRadius - depth) / (occluder.height * .35f + targetRadius), 0.f, 1.f);
    const float frontFade = Clamp((depth + occluder.depthPadding + targetRadius) / (occluder.depthPadding + targetRadius + 1.f), 0.f, 1.f);
    return horizontal * (std::min)(topFade, frontFade);
}

struct OcclusionField2DStats {
    std::size_t occluders{};
    std::size_t capacity{};
    std::size_t peakOccluders{};
    std::uint64_t queries{};
    std::uint64_t tests{};
    std::uint64_t hits{};
    std::uint64_t broadphaseRejected{};
    std::uint64_t indexBuilds{};
    std::uint64_t linearQueries{};
    std::uint64_t indexedQueries{};
    std::uint64_t preparedBatches{};
    std::uint64_t indexedBatches{};
    std::uint64_t indexReuses{};
};

class OcclusionField2D {
public:
    explicit OcclusionField2D(std::size_t initialCapacity = 0) { Reserve(initialCapacity); }
    void Clear() { occluders_.clear(); maximumHalfWidth_=0.f;indexDirty_=true;preferIndexedQueries_=false;stats_.queries=stats_.tests=stats_.hits=stats_.broadphaseRejected=stats_.indexBuilds=stats_.linearQueries=stats_.indexedQueries=stats_.preparedBatches=stats_.indexedBatches=stats_.indexReuses=0; }
    void Reserve(std::size_t capacity) { retainedCapacity_ = capacity; occluders_.reserve(capacity); sortedIndices_.reserve(capacity); }
    void Add(VerticalOccluder2D occluder) { occluders_.push_back(occluder);indexDirty_=true;preferIndexedQueries_=false;stats_.peakOccluders = (std::max)(stats_.peakOccluders, occluders_.size()); }
    void PrepareQueries(std::size_t expectedQueries) { ++stats_.preparedBatches;preferIndexedQueries_=!occluders_.empty()&&expectedQueries&&(occluders_.size()>IndexThreshold||expectedQueries>IndexedBatchWorkThreshold/occluders_.size());if(preferIndexedQueries_){EnsureIndex();++stats_.indexedBatches;} }
    [[nodiscard]] float Query(Vec2 targetFoot, float targetRadius) {
        ++stats_.queries;
        if(!std::isfinite(targetFoot.x)||!std::isfinite(targetFoot.y)||!std::isfinite(targetRadius))return 0.f;
        float amount{};
        if(occluders_.size()<=IndexThreshold&&!preferIndexedQueries_){++stats_.linearQueries;for(const VerticalOccluder2D& occluder:occluders_){++stats_.tests;amount=(std::max)(amount,VerticalOcclusionAmount2D(targetFoot,targetRadius,occluder));}}
        else{
            EnsureIndex();++stats_.indexedQueries;targetRadius=(std::max)(0.f,targetRadius);const float reach=maximumHalfWidth_+targetRadius,minimumX=targetFoot.x-reach,maximumX=targetFoot.x+reach;
            const auto lower=std::lower_bound(sortedIndices_.begin(),sortedIndices_.end(),minimumX,[this](std::uint32_t index,float value){return SortPosition(index)<value;});
            const auto upper=std::upper_bound(lower,sortedIndices_.end(),maximumX,[this](float value,std::uint32_t index){return value<SortPosition(index);});
            const std::uint64_t candidates=std::uint64_t(upper-lower);stats_.broadphaseRejected+=occluders_.size()-candidates;
            for(auto iterator=lower;iterator!=upper;++iterator){++stats_.tests;amount=(std::max)(amount,VerticalOcclusionAmount2D(targetFoot,targetRadius,occluders_[*iterator]));}
        }
        if (amount > 0.f) ++stats_.hits;
        return amount;
    }
    [[nodiscard]] OcclusionField2DStats Stats() const { OcclusionField2DStats result = stats_; result.occluders = occluders_.size(); result.capacity = occluders_.capacity(); return result; }
    [[nodiscard]] ReusableMemoryStats MemoryStats() const { const std::size_t target=(std::max)(occluders_.size(),retainedCapacity_),indexTarget=indexDirty_?retainedCapacity_:(std::max)(sortedIndices_.size(),retainedCapacity_),bytes=occluders_.capacity()*sizeof(VerticalOccluder2D)+sortedIndices_.capacity()*sizeof(std::uint32_t),reclaimable=(occluders_.capacity()>target?(occluders_.capacity()-target)*sizeof(VerticalOccluder2D):0)+(sortedIndices_.capacity()>indexTarget?(sortedIndices_.capacity()-indexTarget)*sizeof(std::uint32_t):0);return{bytes,reclaimable,lastReleasedBytes_,trimCount_}; }
    std::size_t TrimUnusedMemory() { const std::size_t before=MemoryStats().retainedCapacityBytes;if(indexDirty_)sortedIndices_.clear();std::vector<VerticalOccluder2D> occluderReplacement;occluderReplacement.reserve((std::max)(occluders_.size(),retainedCapacity_));occluderReplacement.insert(occluderReplacement.end(),occluders_.begin(),occluders_.end());occluders_.swap(occluderReplacement);std::vector<std::uint32_t> indexReplacement;indexReplacement.reserve((std::max)(sortedIndices_.size(),retainedCapacity_));indexReplacement.insert(indexReplacement.end(),sortedIndices_.begin(),sortedIndices_.end());sortedIndices_.swap(indexReplacement);const std::size_t after=MemoryStats().retainedCapacityBytes;lastReleasedBytes_=before>after?before-after:0;++trimCount_;return lastReleasedBytes_; }
    static constexpr std::size_t IndexThreshold=32;
    static constexpr std::size_t IndexedBatchWorkThreshold=2048;
private:
    [[nodiscard]] float SortPosition(std::uint32_t index)const{const float value=occluders_[index].foot.x;return std::isfinite(value)?value:0.f;}
    void EnsureIndex(){if(!indexDirty_)return;const auto ordered=[this](std::uint32_t left,std::uint32_t right){const float leftPosition=SortPosition(left),rightPosition=SortPosition(right);return leftPosition!=rightPosition?leftPosition<rightPosition:left<right;};const bool reusable=sortedIndices_.size()==occluders_.size()&&std::all_of(sortedIndices_.begin(),sortedIndices_.end(),[this](std::uint32_t index){return index<occluders_.size();})&&std::is_sorted(sortedIndices_.begin(),sortedIndices_.end(),ordered);if(reusable)++stats_.indexReuses;else{sortedIndices_.resize(occluders_.size());std::iota(sortedIndices_.begin(),sortedIndices_.end(),std::uint32_t{});std::sort(sortedIndices_.begin(),sortedIndices_.end(),ordered);++stats_.indexBuilds;}maximumHalfWidth_=0.f;for(const VerticalOccluder2D& occluder:occluders_)if(std::isfinite(occluder.halfWidth))maximumHalfWidth_=(std::max)(maximumHalfWidth_,std::abs(occluder.halfWidth));indexDirty_=false;}
    std::vector<VerticalOccluder2D> occluders_{};
    std::vector<std::uint32_t> sortedIndices_{};
    OcclusionField2DStats stats_{};
    std::size_t retainedCapacity_{},lastReleasedBytes_{};
    std::uint64_t trimCount_{};
    float maximumHalfWidth_{};
    bool indexDirty_{true},preferIndexedQueries_{};
};

// 小型批次使用穩定插入排序，避免額外配置並維持相同深度的提交順序。
template<class Entries>
constexpr void SortDepthEntries(Entries& entries) {
    for (std::size_t index = 1; index < entries.size(); ++index) {
        DepthEntry current = entries[index];
        std::size_t destination = index;
        while (destination > 0 && current.depth < entries[destination - 1].depth) {
            entries[destination] = entries[destination - 1];
            --destination;
        }
        entries[destination] = current;
    }
}

class MotionState2D {
public:
    void Reset(Vec2 position) { previous_ = current_ = position; }
    void Synchronize() { previous_ = current_; }
    void Advance(Vec2 position) { previous_ = current_; current_ = position; }
    [[nodiscard]] Vec2 Previous() const { return previous_; }
    [[nodiscard]] Vec2 Current() const { return current_; }
    [[nodiscard]] Vec2 Sample(float alpha) const { return previous_ + (current_ - previous_) * Clamp(alpha, 0.f, 1.f); }
private:
    Vec2 previous_{};
    Vec2 current_{};
};

struct DepthBatch2DStats {
    std::size_t entries{};
    std::size_t capacity{};
    std::size_t peakEntries{};
    std::uint64_t sorts{};
    std::uint64_t smallSorts{};
    std::uint64_t largeSorts{};
};

class DepthBatch2D {
public:
    explicit DepthBatch2D(std::size_t initialCapacity = 0) { Reserve(initialCapacity); }
    void Clear() { entries_.clear(); }
    void Reserve(std::size_t capacity) { retainedCapacity_ = capacity; entries_.reserve(capacity); }
    void Add(float depth, std::size_t index, std::size_t group = 0) {
        entries_.push_back({std::isfinite(depth) ? depth : 0.f, index, group});
        stats_.peakEntries = (std::max)(stats_.peakEntries, entries_.size());
    }
    void Sort() {
        ++stats_.sorts;
        if (entries_.size() <= SmallBatchLimit) {
            ++stats_.smallSorts;
            SortDepthEntries(entries_);
        } else {
            ++stats_.largeSorts;
            std::stable_sort(entries_.begin(), entries_.end(), [](const DepthEntry& left, const DepthEntry& right) { return left.depth < right.depth; });
        }
    }
    [[nodiscard]] const std::vector<DepthEntry>& Entries() const { return entries_; }
    [[nodiscard]] DepthBatch2DStats Stats() const {
        DepthBatch2DStats result = stats_;
        result.entries = entries_.size();
        result.capacity = entries_.capacity();
        return result;
    }
    [[nodiscard]] ReusableMemoryStats MemoryStats() const { const std::size_t bytes=entries_.capacity()*sizeof(DepthEntry),target=(std::max)(entries_.size(),retainedCapacity_),reclaimable=entries_.capacity()>target?(entries_.capacity()-target)*sizeof(DepthEntry):0;return{bytes,reclaimable,lastReleasedBytes_,trimCount_}; }
    std::size_t TrimUnusedMemory() { const std::size_t before=MemoryStats().retainedCapacityBytes;std::vector<DepthEntry> replacement;replacement.reserve((std::max)(entries_.size(),retainedCapacity_));replacement.insert(replacement.end(),entries_.begin(),entries_.end());entries_.swap(replacement);const std::size_t after=MemoryStats().retainedCapacityBytes;lastReleasedBytes_=before>after?before-after:0;++trimCount_;return lastReleasedBytes_; }
    static constexpr std::size_t SmallBatchLimit = 32;
private:
    std::vector<DepthEntry> entries_{};
    DepthBatch2DStats stats_{};
    std::size_t retainedCapacity_{},lastReleasedBytes_{};
    std::uint64_t trimCount_{};
};
}
