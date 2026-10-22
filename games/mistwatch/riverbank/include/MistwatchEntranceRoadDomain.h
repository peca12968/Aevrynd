#pragma once
#include <Aevrynd/Mesh3D.h>
#include "MistwatchForestFloorMask.h"
#include <array>
#include <bit>
#include <limits>
#include <set>

namespace Mistwatch {
inline constexpr const char* EntranceRoadDomainProfile="entrance-road-material-domain-v1";
inline constexpr float EntranceRoadDomainFadeDistance=.75f;
inline constexpr float EntranceRoadDomainCoverageMargin=1.f/4096.f;
struct EntranceRoadDomainReceipt {
    std::uint64_t sourceFingerprint{},forestFingerprint{},roadFingerprint{};
    std::size_t sourceTriangles{},forestTriangles{},roadTriangles{},sharedVertices{};
    std::size_t forestChangedVertices{},roadChangedVertices{};
};
struct EntranceRoadDomainResult {
    Aevrynd::Mesh3D forest,road;
    std::vector<std::uint32_t> sharedBoundaryVertices;
    std::vector<std::uint8_t> triangleIsRoad;
    EntranceRoadDomainReceipt receipt;
};
namespace EntranceRoadDomainDetail {
inline void Mix(std::uint64_t& hash,std::uint32_t value) noexcept {
    for(unsigned byte=0;byte<4;++byte){hash^=(value>>(byte*8))&255u;hash*=1099511628211ull;}
}
inline std::uint64_t Fingerprint(const Aevrynd::Mesh3D& mesh) noexcept {
    std::uint64_t hash=14695981039346656037ull;
    Mix(hash,std::uint32_t(mesh.vertices.size()));Mix(hash,std::uint32_t(mesh.indices.size()));
    for(const auto& v:mesh.vertices){
        for(const float word:{v.x,v.y,v.z})Mix(hash,std::bit_cast<std::uint32_t>(word));
        Mix(hash,v.normal);Mix(hash,v.color);
        Mix(hash,std::bit_cast<std::uint32_t>(v.u));Mix(hash,std::bit_cast<std::uint32_t>(v.v));
    }
    for(const auto index:mesh.indices)Mix(hash,index);
    return hash;
}
inline bool Inside(float x,float z) noexcept {
    return z>=-10.f&&z<=28.f&&std::abs(x-.35f*std::sin(z*.19f))<2.10f+.15f*std::sin(z*.31f);
}
inline void Validate(const Aevrynd::Mesh3D& mesh) {
    if(mesh.vertices.size()>100000||mesh.indices.size()>600000||!mesh.Valid())
        throw std::invalid_argument("ENTRANCE_ROAD_INVALID_MESH");
    for(const auto& v:mesh.vertices){
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||std::abs(v.x)>50.f||
            std::abs(v.y)>10000.f||v.z< -20.f||v.z>100.f)
            throw std::invalid_argument("ENTRANCE_ROAD_INVALID_VERTEX");
        if(!std::isfinite(v.u)||!std::isfinite(v.v)||v.u<0.f||v.v<0.f||v.u+v.v>1.f)
            throw std::invalid_argument("ENTRANCE_ROAD_INVALID_COVERAGE");
    }
    std::set<std::array<std::uint32_t,3>> unique;
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        std::array<std::uint32_t,3> indices{mesh.indices[i],mesh.indices[i+1],mesh.indices[i+2]};
        const auto& a=mesh.vertices[indices[0]];const auto& b=mesh.vertices[indices[1]];
        const auto& c=mesh.vertices[indices[2]];
        const double area=(double(b.x)-a.x)*(double(c.z)-a.z)-(double(b.z)-a.z)*(double(c.x)-a.x);
        if(std::abs(area)<1e-10||std::hypot(a.x-b.x,a.z-b.z)>1.f||
            std::hypot(b.x-c.x,b.z-c.z)>1.f||std::hypot(c.x-a.x,c.z-a.z)>1.f)
            throw std::invalid_argument("ENTRANCE_ROAD_UNSUPPORTED_TRIANGLE");
        std::sort(indices.begin(),indices.end());
        if(!unique.insert(indices).second)throw std::invalid_argument("ENTRANCE_ROAD_DUPLICATE_TRIANGLE");
    }
}
}
inline EntranceRoadDomainResult CreateEntranceRoadDomain(const Aevrynd::Mesh3D& source) {
    // 先核對完整輸入，再建立兩個互斥索引集；原地形及碰撞資料不會被寫入。
    EntranceRoadDomainDetail::Validate(source);
    EntranceRoadDomainResult out;
    out.forest.vertices=source.vertices;out.road.vertices=source.vertices;
    out.triangleIsRoad.reserve(source.indices.size()/3);
    std::vector<std::uint8_t> membership(source.vertices.size());
    for(std::size_t i=0;i<source.indices.size();i+=3){
        bool road=true;
        for(unsigned corner=0;corner<3;++corner){const auto& v=source.vertices[source.indices[i+corner]];
            road=road&&EntranceRoadDomainDetail::Inside(v.x,v.z);}
        out.triangleIsRoad.push_back(std::uint8_t(road?1u:0u));
        auto& destination=road?out.road.indices:out.forest.indices;
        for(unsigned corner=0;corner<3;++corner){const auto index=source.indices[i+corner];
            destination.push_back(index);membership[index]=std::uint8_t(membership[index]|(road?2u:1u));}
    }
    if(out.road.indices.empty()||out.forest.indices.empty())
        throw std::invalid_argument("ENTRANCE_ROAD_BOTH_DOMAINS_REQUIRED");
    for(std::size_t i=0;i<membership.size();++i)if(membership[i]==3u)
        out.sharedBoundaryVertices.push_back(std::uint32_t(i));
    if(out.sharedBoundaryVertices.empty()||out.sharedBoundaryVertices.size()>2048)
        throw std::invalid_argument("ENTRANCE_ROAD_BOUNDED_SEAM_REQUIRED");
    for(std::size_t i=0;i<source.vertices.size();++i){
        const auto& v=source.vertices[i];
        // 交界共用落葉及岩石；保留極小底層餘量，避免像素插值捨入使土岩總和超過一。
        float distance=EntranceRoadDomainFadeDistance;
        if(std::abs(v.x)<4.f&&v.z>=-11.f&&v.z<=29.f)
            for(const auto boundary:out.sharedBoundaryVertices){const auto& s=source.vertices[boundary];
                distance=(std::min)(distance,std::hypot(v.x-s.x,v.z-s.z));}
        const float fade=ForestFloorSmooth(distance/EntranceRoadDomainFadeDistance);
        const float available=(std::max)(0.f,1.f-v.v-EntranceRoadDomainCoverageMargin);
        if((membership[i]&1u)!=0&&fade<1.f)
            out.forest.vertices[i].u=available*(1.f-fade)+v.u*fade;
        if((membership[i]&2u)!=0)
            out.road.vertices[i].u=(1.f-.95f*CentralTrailVisualWeight(v.x,v.z,ForestFloorMaskSelector::ForestWood)*fade)*available;
        if(membership[i]==3u)out.forest.vertices[i].u=out.road.vertices[i].u=available;
        // 浮點加法的最後一位不得讓既有覆蓋解碼超出正規化範圍。
        for(auto* value:{&out.forest.vertices[i].u,&out.road.vertices[i].u})
            if(*value+v.v>1.f)*value=1.f-v.v;
        if(std::bit_cast<std::uint32_t>(v.u)!=std::bit_cast<std::uint32_t>(out.forest.vertices[i].u))
            ++out.receipt.forestChangedVertices;
        if(std::bit_cast<std::uint32_t>(v.u)!=std::bit_cast<std::uint32_t>(out.road.vertices[i].u))
            ++out.receipt.roadChangedVertices;
    }
    auto& receipt=out.receipt;
    receipt.sourceTriangles=source.indices.size()/3;receipt.forestTriangles=out.forest.indices.size()/3;
    receipt.roadTriangles=out.road.indices.size()/3;receipt.sharedVertices=out.sharedBoundaryVertices.size();
    receipt.sourceFingerprint=EntranceRoadDomainDetail::Fingerprint(source);
    receipt.forestFingerprint=EntranceRoadDomainDetail::Fingerprint(out.forest);
    receipt.roadFingerprint=EntranceRoadDomainDetail::Fingerprint(out.road);
    return out;
}
}
