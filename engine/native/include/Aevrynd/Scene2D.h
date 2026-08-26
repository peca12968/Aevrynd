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
