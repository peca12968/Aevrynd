#pragma once
#include "Math.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Aevrynd {

struct TerrainClipmapSettings3D {
    Vec3 origin{};
    std::uint32_t chunksX{1},chunksZ{1};
    float chunkWorldSize{64.f};
    std::span<const float> distanceBands{};
    float hysteresis{8.f};
    std::size_t maximumChunks{65536},maximumUpdatesPerFrame{256},maximumLodLevels{16};
};

struct TerrainChunkLod3D {std::uint32_t x{},z{},lod{};};
struct TerrainClipmapStats3D {std::size_t chunkCount{},changedChunks{},deferredChunks{};std::uint32_t maximumLod{};std::uint64_t updateSerial{};};

class TerrainClipmap3D {
public:
    bool Configure(const TerrainClipmapSettings3D& settings);
    bool Update(Vec3 observer);
    std::span<const TerrainChunkLod3D> Chunks()const{return chunks_;}
    const TerrainClipmapStats3D& Stats()const{return stats_;}
    std::size_t CapacityBytes()const;
private:
    std::size_t Index(std::size_t x,std::size_t z)const{return z*settings_.chunksX+x;}
    TerrainClipmapSettings3D settings_{};
    std::vector<float> bands_;
    std::vector<TerrainChunkLod3D> chunks_;
    std::vector<std::uint32_t> desiredLods_;
    TerrainClipmapStats3D stats_{};
};

}
