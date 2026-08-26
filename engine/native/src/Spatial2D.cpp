#include <Aevrynd/Spatial2D.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Aevrynd {
namespace {
float LengthSquared(Vec2 value) {
    return value.x * value.x + value.y * value.y;
}
}

void PolylinePath2D::SetPoints(std::span<const Vec2> points, bool closed) {
    Clear();
    points_.assign(points.begin(), points.end());
    closed_ = closed && points_.size() > 2;
    if (points_.size() < 2) return;
    const std::size_t count = points_.size() - 1 + (closed_ ? 1 : 0);
    segmentLengths_.reserve(count);
    cumulativeLengths_.reserve(count + 1);
    cumulativeLengths_.push_back(0.f);
    for (std::size_t index = 0; index < count; ++index) {
        const float length = Distance(points_[index], points_[(index + 1) % points_.size()]);
        segmentLengths_.push_back(length);
        totalLength_ += length;
        cumulativeLengths_.push_back(totalLength_);
    }
}

void PolylinePath2D::Clear() {
    points_.clear();
    segmentLengths_.clear();
    cumulativeLengths_.clear();
    totalLength_ = 0.f;
    closed_ = false;
}

PathSample2D PolylinePath2D::Sample(float normalizedDistance) const {
    if (!*this) return points_.empty() ? PathSample2D{} : PathSample2D{points_.front()};
    const float normalized = closed_ ? normalizedDistance - std::floor(normalizedDistance) : Clamp(normalizedDistance, 0.f, 1.f);
    PathSample2D sample = SampleDistance(normalized * totalLength_);
    sample.normalizedDistance = normalized;
    return sample;
}

PathSample2D PolylinePath2D::SampleDistance(float distance) const {
    if (!*this) return points_.empty() ? PathSample2D{} : PathSample2D{points_.front()};
    float target = distance;
    if (closed_) {
        target = std::fmod(target, totalLength_);
        if (target < 0.f) target += totalLength_;
    } else target = Clamp(target, 0.f, totalLength_);
    auto upper = std::upper_bound(cumulativeLengths_.begin(), cumulativeLengths_.end(), target);
    std::size_t segment = upper == cumulativeLengths_.begin() ? 0 : std::size_t(upper - cumulativeLengths_.begin() - 1);
    segment = (std::min)(segment, segmentLengths_.size() - 1);
    const float length = segmentLengths_[segment];
    const float amount = length > .0001f ? (target - cumulativeLengths_[segment]) / length : 0.f;
    const Vec2 start = points_[segment];
    const Vec2 end = points_[(segment + 1) % points_.size()];
    return {start + (end - start) * amount, Normalize(end - start), target, totalLength_ > 0.f ? target / totalLength_ : 0.f, segment};
}

PathSample2D PolylinePath2D::ClosestPoint(Vec2 point) const {
    if (!*this) return points_.empty() ? PathSample2D{} : PathSample2D{points_.front()};
    float bestDistanceSquared = std::numeric_limits<float>::infinity();
    PathSample2D best{};
    for (std::size_t segment = 0; segment < segmentLengths_.size(); ++segment) {
        const Vec2 start = points_[segment];
        const Vec2 delta = points_[(segment + 1) % points_.size()] - start;
        const float lengthSquared = LengthSquared(delta);
        const float amount = lengthSquared > .0001f ? Clamp(((point.x - start.x) * delta.x + (point.y - start.y) * delta.y) / lengthSquared, 0.f, 1.f) : 0.f;
        const Vec2 position = start + delta * amount;
        const float distanceSquared = LengthSquared(point - position);
        if (distanceSquared >= bestDistanceSquared) continue;
        bestDistanceSquared = distanceSquared;
        const float distance = cumulativeLengths_[segment] + segmentLengths_[segment] * amount;
        best = {position, Normalize(delta), distance, totalLength_ > 0.f ? distance / totalLength_ : 0.f, segment};
    }
    return best;
}

SpatialIndex2D::SpatialIndex2D(float cellSize) {
    SetCellSize(cellSize);
}

void SpatialIndex2D::Clear(bool resetStats) {
    entries_.clear();
    for (Cell& cell : cells_) cell.entries.clear();
    cellLookup_.clear();
    idLookup_.clear();
    activeCellCount_ = 0;
    maximumRadius_ = 0.f;
    if (resetStats) stats_ = {};
    else {
        stats_.entries = 0;
        stats_.occupiedCells = 0;
    }
}

void SpatialIndex2D::Reserve(std::size_t entryCapacity, std::size_t cellCapacity) {
    entries_.reserve(entryCapacity);
    idLookup_.reserve(entryCapacity);
    const std::size_t cells = cellCapacity ? cellCapacity : entryCapacity;
    cells_.reserve(cells);
    cellLookup_.reserve(cells);
}

void SpatialIndex2D::SetCellSize(float cellSize) {
    cellSize_ = (std::max)(1.f, cellSize);
    Clear();
}

uint64_t SpatialIndex2D::CellKey(int x, int y) {
    return uint64_t(uint32_t(x)) << 32 | uint32_t(y);
}

void SpatialIndex2D::Insert(uint64_t id, std::size_t userIndex, Vec2 position, float radius, uint32_t mask) {
    if (!mask || idLookup_.contains(id)) return;
    const uint32_t entryIndex = uint32_t(entries_.size());
    entries_.push_back({id, userIndex, position, (std::max)(0.f, radius), mask});
    idLookup_[id] = entryIndex;
    const int x = int(std::floor(position.x / cellSize_));
    const int y = int(std::floor(position.y / cellSize_));
    const uint64_t key = CellKey(x, y);
    auto found = cellLookup_.find(key);
    uint32_t cellIndex{};
    if (found == cellLookup_.end()) {
        cellIndex = uint32_t(activeCellCount_++);
        if (cellIndex == cells_.size()) cells_.push_back({key, {}});
        else cells_[cellIndex].key = key;
        cellLookup_[key] = cellIndex;
    } else cellIndex = found->second;
    cells_[cellIndex].entries.push_back(entryIndex);
    maximumRadius_ = (std::max)(maximumRadius_, (std::max)(0.f, radius));
    stats_.entries = entries_.size();
    stats_.occupiedCells = activeCellCount_;
    stats_.peakEntries = (std::max)(stats_.peakEntries, stats_.entries);
    stats_.peakOccupiedCells = (std::max)(stats_.peakOccupiedCells, stats_.occupiedCells);
}

std::size_t SpatialIndex2D::QueryCircle(Vec2 center, float radius, uint32_t mask, std::vector<SpatialHit2D>& results) const {
    results.clear();
    ++stats_.queries;
    if (!mask || entries_.empty()) return 0;
    const float queryRadius = (std::max)(0.f, radius);
    const float cellRadius = queryRadius + maximumRadius_;
    const int minimumX = int(std::floor((center.x - cellRadius) / cellSize_));
    const int maximumX = int(std::floor((center.x + cellRadius) / cellSize_));
    const int minimumY = int(std::floor((center.y - cellRadius) / cellSize_));
    const int maximumY = int(std::floor((center.y + cellRadius) / cellSize_));
    for (int y = minimumY; y <= maximumY; ++y) for (int x = minimumX; x <= maximumX; ++x) {
        const auto cell = cellLookup_.find(CellKey(x, y));
        if (cell == cellLookup_.end()) continue;
        for (uint32_t entryIndex : cells_[cell->second].entries) {
            const SpatialEntry2D& entry = entries_[entryIndex];
            ++stats_.candidates;
            if (!(entry.mask & mask)) continue;
            const float distanceSquared = LengthSquared(entry.position - center);
            const float combinedRadius = queryRadius + entry.radius;
            if (distanceSquared > combinedRadius * combinedRadius) continue;
            results.push_back({entry.id, entry.userIndex, entry.position, distanceSquared});
            ++stats_.matches;
        }
    }
    return results.size();
}

std::optional<SpatialHit2D> SpatialIndex2D::Nearest(Vec2 center, float radius, uint32_t mask) const {
    std::vector<SpatialHit2D> hits;
    QueryCircle(center, radius, mask, hits);
    if (hits.empty()) return std::nullopt;
    return *std::min_element(hits.begin(), hits.end(), [](const SpatialHit2D& left, const SpatialHit2D& right) { return left.distanceSquared < right.distanceSquared; });
}

const SpatialEntry2D* SpatialIndex2D::Find(uint64_t id) const {
    const auto found = idLookup_.find(id);
    return found == idLookup_.end() ? nullptr : &entries_[found->second];
}
}
