#pragma once
#include "Math.h"
#include "Memory.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace Aevrynd {
struct WorldChunkCoordinate3D {
    int x{},z{};
    constexpr bool operator==(const WorldChunkCoordinate3D&)const=default;
};

struct WorldChunk3D {
    WorldChunkCoordinate3D coordinate{};
    Vec3 center{};
    uint32_t seed{};
    float observerDistanceSquared{};
};

struct WorldStreamingSettings3D {
    float chunkSize{32.f};
    unsigned loadRadius{1};
    unsigned retainRadius{2};
    std::size_t maximumLoadsPerUpdate{2};
    std::size_t maximumUnloadsPerUpdate{4};
    uint32_t worldSeed{0x4a17u};
};

struct WorldStreamingStats3D {
    WorldChunkCoordinate3D observerChunk{};
    std::size_t desiredChunks{},residentChunks{},queuedChunks{},lastLoaded{},lastUnloaded{},peakResidentChunks{};
    std::size_t loadScratchCapacity{},unloadScratchCapacity{};
    uint64_t totalLoaded{},totalUnloaded{},updates{};
};

class WorldStreamer3D {
public:
    explicit WorldStreamer3D(WorldStreamingSettings3D settings={}){Configure(settings);}
    void Configure(WorldStreamingSettings3D settings);
    void Reset();
    std::size_t TrimUnusedMemory();
    ReusableMemoryStats MemoryStats()const;
    WorldStreamingStats3D Update(Vec3 observerPosition);
    bool Contains(WorldChunkCoordinate3D coordinate)const;
    std::span<const WorldChunk3D> ResidentChunks()const{return residentView_;}
    std::span<const WorldChunk3D> LoadedChunks()const{return loadedView_;}
    std::span<const WorldChunk3D> UnloadedChunks()const{return unloadedView_;}
    const WorldStreamingSettings3D& Settings()const{return settings_;}
    WorldStreamingStats3D Stats()const{return stats_;}
    static WorldChunkCoordinate3D CoordinateAt(Vec3 position,float chunkSize=32.f);
    static Vec3 ChunkCenter(WorldChunkCoordinate3D coordinate,float chunkSize=32.f);
    static uint32_t ChunkSeed(WorldChunkCoordinate3D coordinate,uint32_t worldSeed=0x4a17u);
private:
    struct CoordinateHash {std::size_t operator()(WorldChunkCoordinate3D coordinate)const noexcept;};
    WorldStreamingSettings3D settings_{};
    WorldStreamingStats3D stats_{};
    std::unordered_map<WorldChunkCoordinate3D,WorldChunk3D,CoordinateHash> resident_;
    std::vector<WorldChunk3D> residentView_,loadedView_,unloadedView_;
    std::vector<WorldChunk3D> loadScratch_;
    std::vector<WorldChunkCoordinate3D> unloadScratch_;
    std::size_t lastReleasedBytes_{};
    uint64_t trimCount_{};
};
}
