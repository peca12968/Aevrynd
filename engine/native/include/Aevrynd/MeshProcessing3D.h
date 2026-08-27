#pragma once

#include "Mesh3D.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Aevrynd {

enum class MeshNormalMode3D : uint8_t {AreaWeightedSmooth,HardEdges};
enum class MeshProcessingErrorCode3D : uint8_t {None,EmptyMesh,LimitExceeded,InvalidIndex,InvalidTriangleList,NonFiniteVertex,NoUsableTriangles,InvalidSetting};

struct MeshProcessingLimits3D {
    size_t maximumInputVertices{4*1024*1024};
    size_t maximumInputIndices{12*1024*1024};
    size_t maximumOutputVertices{12*1024*1024};
    size_t maximumOutputIndices{12*1024*1024};
    float maximumAbsoluteCoordinate{1000000.f};
    size_t maximumScratchBytes{512*1024*1024};
};

struct MeshProcessingError3D {
    MeshProcessingErrorCode3D code{MeshProcessingErrorCode3D::None};
    size_t elementIndex{};
    explicit operator bool()const{return code!=MeshProcessingErrorCode3D::None;}
};

struct MeshProcessingStats3D {
    size_t inputVertices{},outputVertices{},inputTriangles{},outputTriangles{};
    size_t removedVertices{},removedDegenerateTriangles{},mergedVertices{};
};

struct MeshLodSettings3D {
    float targetTriangleRatio{.5f};
    uint32_t maximumGridResolution{256};
};

static constexpr uint32_t MaximumGeneratedMeshLodLevels3D=3;
struct MeshLodChainSettings3D {
    std::array<float,MaximumGeneratedMeshLodLevels3D> targetTriangleRatios{.5f,.25f,.125f};
    uint32_t generatedLevelCount{MaximumGeneratedMeshLodLevels3D};
    uint32_t maximumGridResolution{256};
    bool reorderForVertexCache{true};
    uint32_t vertexCacheSize{16};
    size_t maximumTotalOutputTriangles{12*1024*1024};
};
struct MeshLodChainStats3D {
    size_t generatedLevels{},sourceTriangles{},finalTriangles{},totalOutputTriangles{};
    std::array<MeshProcessingStats3D,MaximumGeneratedMeshLodLevels3D> levels{};
};

class MeshProcessing3D {
public:
    // 所有函式只使用呼叫期間的有界暫存空間，不保留跨呼叫記憶體；配置失敗會以例外回報且不修改輸出。
    static constexpr bool RetainsMemory=false;
    static bool Validate(const Mesh3D& input,MeshProcessingError3D* error=nullptr,const MeshProcessingLimits3D& limits={});
    static bool RemoveDegenerateTriangles(const Mesh3D& input,Mesh3D& output,MeshProcessingStats3D* stats=nullptr,MeshProcessingError3D* error=nullptr,float areaEpsilon=1e-10f,const MeshProcessingLimits3D& limits={});
    static bool DeduplicateVertices(const Mesh3D& input,Mesh3D& output,MeshProcessingStats3D* stats=nullptr,MeshProcessingError3D* error=nullptr,const MeshProcessingLimits3D& limits={});
    static bool RebuildNormals(const Mesh3D& input,Mesh3D& output,MeshNormalMode3D mode=MeshNormalMode3D::AreaWeightedSmooth,MeshProcessingStats3D* stats=nullptr,MeshProcessingError3D* error=nullptr,const MeshProcessingLimits3D& limits={});
    static bool GenerateLod(const Mesh3D& input,Mesh3D& output,MeshLodSettings3D settings={},MeshProcessingStats3D* stats=nullptr,MeshProcessingError3D* error=nullptr,const MeshProcessingLimits3D& limits={});
    static bool GenerateLodChain(const Mesh3D& input,std::vector<Mesh3D>& output,MeshLodChainSettings3D settings={},MeshLodChainStats3D* stats=nullptr,MeshProcessingError3D* error=nullptr,const MeshProcessingLimits3D& limits={});
    static bool ReorderForVertexCache(const Mesh3D& input,Mesh3D& output,uint32_t cacheSize=16,MeshProcessingStats3D* stats=nullptr,MeshProcessingError3D* error=nullptr,const MeshProcessingLimits3D& limits={});
};

}
