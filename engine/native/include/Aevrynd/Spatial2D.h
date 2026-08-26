#pragma once
#include "Math.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace Aevrynd {
struct PathSample2D {
    Vec2 position{};
    Vec2 tangent{1.f, 0.f};
    float distance{};
    float normalizedDistance{};
    std::size_t segment{};
};

class PolylinePath2D {
public:
    void SetPoints(std::span<const Vec2> points, bool closed = false);
    void Clear();
    PathSample2D Sample(float normalizedDistance) const;
    PathSample2D SampleDistance(float distance) const;
    PathSample2D ClosestPoint(Vec2 point) const;
    const std::vector<Vec2>& Points() const { return points_; }
    float Length() const { return totalLength_; }
    std::size_t SegmentCount() const { return segmentLengths_.size(); }
    bool Closed() const { return closed_; }
    explicit operator bool() const { return !segmentLengths_.empty(); }
private:
    std::vector<Vec2> points_;
    std::vector<float> segmentLengths_;
    std::vector<float> cumulativeLengths_;
    float totalLength_{};
    bool closed_{};
};

struct SpatialEntry2D {
    uint64_t id{};
    std::size_t userIndex{};
    Vec2 position{};
    float radius{};
    uint32_t mask{0xffffffffu};
};

struct SpatialHit2D {
    uint64_t id{};
    std::size_t userIndex{};
    Vec2 position{};
    float distanceSquared{};
};

struct SpatialIndex2DStats {
    std::size_t entries{};
    std::size_t occupiedCells{};
    std::size_t peakEntries{};
    std::size_t peakOccupiedCells{};
    uint64_t queries{};
    uint64_t candidates{};
    uint64_t matches{};
};

class SpatialIndex2D {
public:
    explicit SpatialIndex2D(float cellSize = 64.f);
    void Clear(bool resetStats = false);
    void Reserve(std::size_t entryCapacity, std::size_t cellCapacity = 0);
    void SetCellSize(float cellSize);
    void Insert(uint64_t id, std::size_t userIndex, Vec2 position, float radius = 0.f, uint32_t mask = 0xffffffffu);
    std::size_t QueryCircle(Vec2 center, float radius, uint32_t mask, std::vector<SpatialHit2D>& results) const;
    std::optional<SpatialHit2D> Nearest(Vec2 center, float radius, uint32_t mask = 0xffffffffu) const;
    const SpatialEntry2D* Find(uint64_t id) const;
    float CellSize() const { return cellSize_; }
    SpatialIndex2DStats Stats() const { return stats_; }
private:
    struct Cell {
        uint64_t key{};
        std::vector<uint32_t> entries;
    };
    static uint64_t CellKey(int x, int y);
    float cellSize_{64.f};
    float maximumRadius_{};
    std::vector<SpatialEntry2D> entries_;
    std::vector<Cell> cells_;
    std::unordered_map<uint64_t, uint32_t> cellLookup_;
    std::unordered_map<uint64_t, uint32_t> idLookup_;
    std::size_t activeCellCount_{};
    mutable SpatialIndex2DStats stats_{};
};
}
