#pragma once
#include "Math.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
};

class OcclusionField2D {
public:
    explicit OcclusionField2D(std::size_t initialCapacity = 0) { occluders_.reserve(initialCapacity); }
    void Clear() { occluders_.clear(); stats_.queries = stats_.tests = stats_.hits = 0; }
    void Reserve(std::size_t capacity) { occluders_.reserve(capacity); }
    void Add(VerticalOccluder2D occluder) { occluders_.push_back(occluder); stats_.peakOccluders = (std::max)(stats_.peakOccluders, occluders_.size()); }
    [[nodiscard]] float Query(Vec2 targetFoot, float targetRadius) {
        ++stats_.queries;
        float amount{};
        for (const VerticalOccluder2D& occluder : occluders_) { ++stats_.tests; amount = (std::max)(amount, VerticalOcclusionAmount2D(targetFoot, targetRadius, occluder)); }
        if (amount > 0.f) ++stats_.hits;
        return amount;
    }
    [[nodiscard]] OcclusionField2DStats Stats() const { OcclusionField2DStats result = stats_; result.occluders = occluders_.size(); result.capacity = occluders_.capacity(); return result; }
private:
    std::vector<VerticalOccluder2D> occluders_{};
    OcclusionField2DStats stats_{};
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
    explicit DepthBatch2D(std::size_t initialCapacity = 0) { entries_.reserve(initialCapacity); }
    void Clear() { entries_.clear(); }
    void Reserve(std::size_t capacity) { entries_.reserve(capacity); }
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
    static constexpr std::size_t SmallBatchLimit = 32;
private:
    std::vector<DepthEntry> entries_{};
    DepthBatch2DStats stats_{};
};
}
