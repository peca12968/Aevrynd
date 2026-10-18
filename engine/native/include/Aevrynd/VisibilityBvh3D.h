#pragma once
#include "Camera.h"
#include "Mesh3D.h"
#include "Memory.h"
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>
#include <vector>

namespace Aevrynd {
using VisibilityEntityKey3D=std::uint64_t;
inline VisibilityEntityKey3D AllocateVisibilityEntityKey3D(VisibilityEntityKey3D& cursor){if(cursor==(std::numeric_limits<VisibilityEntityKey3D>::max)())return 0;return ++cursor;}
struct VisibilityBvhItem3D {VisibilityEntityKey3D key{};MeshBounds3D bounds{};std::uint32_t payload{};};
struct VisibilityBvhQuery3D {Camera3D camera{};float aspect{1.f},maximumDistance{},shadowDistance{};bool frustumCulling{true};};
struct VisibilityBvhStats3D {std::size_t objects{},nodes{},candidates{},nodesTested{},nodesRejected{},distanceRejectedObjects{},frustumRejectedObjects{},refittedLeaves{};std::uint64_t builds{},refits{},queries{};};

class VisibilityBvh3D {
public:
    bool Build(std::span<const VisibilityBvhItem3D> items);
    bool Update(VisibilityEntityKey3D key,MeshBounds3D bounds);
    bool Refit();
    std::span<const std::uint32_t> Query(const VisibilityBvhQuery3D& query)const;
    void Clear();
    bool Reserve(std::size_t objects);
    std::size_t TrimUnusedMemory();
    const VisibilityBvhStats3D& Stats()const{return stats_;}
    ReusableMemoryStats MemoryStats()const;
private:
    struct Entry {VisibilityEntityKey3D key{};MeshBounds3D bounds{},pendingBounds{};std::uint32_t payload{},leaf{};bool dirty{};};
    struct Node {MeshBounds3D bounds{};std::uint32_t parent{~0u},left{},right{},first{},count{},subtreeCount{};};
    std::uint32_t BuildNode(std::uint32_t first,std::uint32_t count,std::uint32_t parent);
    static MeshBounds3D Merge(MeshBounds3D a,MeshBounds3D b);
    static bool Valid(MeshBounds3D bounds);
    std::vector<Entry> entries_{};
    std::unordered_map<VisibilityEntityKey3D,std::uint32_t> keyToEntry_{};
    std::vector<std::uint32_t> denseKeyToEntry_{};
    VisibilityEntityKey3D denseKeyBase_{};
    std::vector<Node> nodes_{};
    mutable std::vector<std::uint32_t> traversal_{},candidates_{};
    std::vector<MeshBounds3D> refitBounds_{};
    std::size_t retainedObjects_{},lastReleasedBytes_{};
    std::uint64_t trimCount_{};
    mutable VisibilityBvhStats3D stats_{};
};
}
