#pragma once
#include "Aevrynd/Mesh3D.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace Mistwatch {
namespace LeafClusterDetail {
struct Envelope {Aevrynd::Vec3 minimum{},maximum{};};
inline void Need(bool valid,const char* reason) {
    if(!valid)throw std::runtime_error(reason);
}
// 每個語義 u32 先 XOR 再乘質數；不是標準逐 byte FNV，也不是資安簽章。
// 常數直接由固定 AEM3 payload 獨立計算；不含檔案標頭或 C++ padding。
inline std::uint64_t Fingerprint(const Aevrynd::Mesh3D& mesh) {
    std::uint64_t hash=14695981039346656037ull;
    const auto word=[&](std::uint32_t value){hash^=value;hash*=1099511628211ull;};
    for(const auto& v:mesh.vertices){
        word(std::bit_cast<std::uint32_t>(v.x));
        word(std::bit_cast<std::uint32_t>(v.y));
        word(std::bit_cast<std::uint32_t>(v.z));
        word(v.normal);word(v.color);
        word(std::bit_cast<std::uint32_t>(v.u));
        word(std::bit_cast<std::uint32_t>(v.v));
    }
    for(const auto index:mesh.indices)word(index);
    return hash;
}
inline Envelope CheckData(const Aevrynd::Mesh3D& mesh) {
    Need(!mesh.vertices.empty()&&!mesh.indices.empty()&&mesh.indices.size()%3u==0,
        "LEAF_CLUSTER_INVALID_TOPOLOGY");
    Envelope envelope{{mesh.vertices.front().x,mesh.vertices.front().y,mesh.vertices.front().z},
        {mesh.vertices.front().x,mesh.vertices.front().y,mesh.vertices.front().z}};
    for(const auto& v:mesh.vertices){
        Need(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&
            std::isfinite(v.u)&&std::isfinite(v.v),"LEAF_CLUSTER_NONFINITE_VERTEX");
        Need((v.normal>>24u)==127u,"LEAF_CLUSTER_CHANGED_WIND_WEIGHT");
        envelope.minimum.x=(std::min)(envelope.minimum.x,v.x);
        envelope.minimum.y=(std::min)(envelope.minimum.y,v.y);
        envelope.minimum.z=(std::min)(envelope.minimum.z,v.z);
        envelope.maximum.x=(std::max)(envelope.maximum.x,v.x);
        envelope.maximum.y=(std::max)(envelope.maximum.y,v.y);
        envelope.maximum.z=(std::max)(envelope.maximum.z,v.z);
    }
    for(const auto index:mesh.indices)
        Need(index<mesh.vertices.size(),"LEAF_CLUSTER_INDEX_OUT_OF_RANGE");
    return envelope;
}
}

// 僅識別這一版真資產及其來源；地形、材質、動畫、美術與效能另行驗證。
inline void ValidateLeafCluster(const Aevrynd::Mesh3D& candidate,
    const Aevrynd::Mesh3D& original) {
    using LeafClusterDetail::Need;
    Need(original.vertices.size()==536244u&&original.indices.size()==530015u*3u,
        "LEAF_CLUSTER_SOURCE_SIZE_MISMATCH");
    const auto bounds=LeafClusterDetail::CheckData(original);
    Need(LeafClusterDetail::Fingerprint(original)==0xC9E34DC94E664B52ull,
        "LEAF_CLUSTER_SOURCE_FINGERPRINT_MISMATCH");
    Need(candidate.vertices.size()==158509u&&candidate.indices.size()==161589u*3u,
        "LEAF_CLUSTER_CANDIDATE_SIZE_MISMATCH");
    (void)LeafClusterDetail::CheckData(candidate);
    // 逐點包含，不用中心／半徑或抽樣代替完整來源 AABB。
    for(const auto& v:candidate.vertices)
        Need(v.x>=bounds.minimum.x&&v.x<=bounds.maximum.x&&
            v.y>=bounds.minimum.y&&v.y<=bounds.maximum.y&&
            v.z>=bounds.minimum.z&&v.z<=bounds.maximum.z,
            "LEAF_CLUSTER_OUTSIDE_SOURCE_AABB");
    Need(LeafClusterDetail::Fingerprint(candidate)==0x42F04FBD3D012AB2ull,
        "LEAF_CLUSTER_CANDIDATE_FINGERPRINT_MISMATCH");
}
}
