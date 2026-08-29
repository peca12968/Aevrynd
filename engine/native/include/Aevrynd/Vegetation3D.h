#pragma once
#include "Camera.h"
#include "Memory.h"
#include "Renderer.h"
#include "VisibilityBvh3D.h"
#include <array>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Aevrynd {
enum class VegetationLod3D : std::uint8_t { Near, Far, Impostor };
struct VegetationAsset3D {
    std::array<MeshHandle3D,3> meshes{};
    float nearEnd{34.f},farEnd{120.f},transitionWidth{10.f};
};
struct VegetationInstance3D {
    VisibilityEntityKey3D key{};
    std::uint32_t asset{};
    Matrix4 transform{Matrix4::Identity()};
    MeshBounds3D bounds{};
    Color tint{1,1,1,1};
};
struct VegetationView3D {
    Camera3D camera{};
    float aspect{1.f},maximumDistance{420.f};
    bool enabled{true},frustumCulling{true};
};
struct VegetationDraw3D {
    MeshHandle3D mesh{};
    Matrix4 transform{Matrix4::Identity()};
    Color tint{1,1,1,1};
    VegetationLod3D lod{VegetationLod3D::Near};
    float lodCoverage{1.f};
    std::uint32_t lodSeed{};
    bool lodInvert{},castsShadow{true};
};
struct VegetationStats3D {
    std::size_t instances{},candidates{},submitted{},nearInstances{},farInstances{},impostorInstances{},transitionInstances{},transitionDrawsDropped{},shadowCasters{};
    std::size_t frustumRejected{},distanceRejected{},missingAssetRejected{};
    std::uint64_t builds{},refits{},removals{},queries{};
};

class VegetationWorld3D {
public:
    bool Build(std::span<const VegetationAsset3D> assets,std::span<const VegetationInstance3D> instances);
    bool Update(VisibilityEntityKey3D key,Matrix4 transform,MeshBounds3D bounds);
    bool Remove(VisibilityEntityKey3D key);
    std::span<const VegetationDraw3D> Query(const VegetationView3D& view);
    void Clear();
    bool Reserve(std::size_t instances);
    std::size_t TrimUnusedMemory();
    const VegetationStats3D& Stats()const{return stats_;}
    ReusableMemoryStats MemoryStats()const;
private:
    std::vector<VegetationAsset3D> assets_{};
    std::vector<VegetationInstance3D> instances_{};
    std::vector<VegetationDraw3D> draws_{};
    std::vector<VegetationDraw3D> transitionDraws_{};
    std::vector<std::size_t> transitionPrimaryIndices_{};
    std::vector<VisibilityBvhItem3D> rebuildItems_{};
    VisibilityBvh3D visibility_{};
    VegetationStats3D stats_{};
    std::size_t lastReleasedBytes_{};
    std::uint64_t trimCount_{};
};

using VegetationChunkKey3D = std::uint64_t;
struct VegetationChunkHandle3D {
    VegetationChunkKey3D key{};
    std::uint64_t generation{};
    explicit operator bool()const{return key!=0&&generation!=0;}
    auto operator<=>(const VegetationChunkHandle3D&)const=default;
};
struct ChunkedVegetationStats3D {
    std::size_t chunks{},instances{},candidates{},submitted{},nearInstances{},farInstances{},impostorInstances{},transitionInstances{},transitionDrawsDropped{},shadowCasters{};
    std::size_t frustumRejected{},distanceRejected{},missingAssetRejected{};
    std::uint64_t generation{},attaches{},detaches{},queries{},duplicateRejects{},overflowRejects{},buildRejects{};
};

class ChunkedVegetationWorld3D {
public:
    bool AttachChunk(VegetationChunkKey3D chunk,std::span<const VegetationAsset3D> assets,std::span<const VegetationInstance3D> instances,VegetationChunkHandle3D* handle=nullptr);
    bool AttachChunkAtomic(VegetationChunkKey3D chunk,std::span<const VegetationAsset3D> assets,std::span<const VegetationInstance3D> instances,VegetationChunkHandle3D* handle=nullptr);
    bool DetachChunk(VegetationChunkHandle3D handle);
    bool DetachChunk(VegetationChunkKey3D chunk);
    bool BeginDetach(VegetationChunkHandle3D handle){return DetachChunk(handle);}
    bool Contains(VegetationChunkKey3D chunk)const;
    VegetationChunkHandle3D Handle(VegetationChunkKey3D chunk)const;
    std::span<const VegetationDraw3D> Query(const VegetationView3D& view);
    void Clear();
    bool Reserve(std::size_t chunks,std::size_t instances);
    std::size_t TrimUnusedMemory();
    const ChunkedVegetationStats3D& Stats()const{return stats_;}
    ReusableMemoryStats MemoryStats()const;
private:
    struct Block {
        VegetationChunkHandle3D handle{};
        std::vector<VisibilityEntityKey3D> keys{};
        VegetationWorld3D world{};
    };
    std::unordered_map<VegetationChunkKey3D,Block> blocks_{};
    std::unordered_set<VisibilityEntityKey3D> instanceKeys_{};
    std::vector<VegetationChunkKey3D> queryOrder_{};
    std::vector<VegetationDraw3D> draws_{};
    ChunkedVegetationStats3D stats_{};
    std::size_t reservedInstances_{},lastReleasedBytes_{};
    std::uint64_t nextGeneration_{1},trimCount_{};
};
}
