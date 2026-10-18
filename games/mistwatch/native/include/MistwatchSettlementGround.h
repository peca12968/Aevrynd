#pragma once
#include "MistwatchEntranceRoadDomain.h"
#include "MistwatchWorld.h"
#include <Aevrynd/TerrainLayerCoverage3D.h>
#include <string_view>

namespace Mistwatch::SettlementGround {
inline constexpr const char* Profile="workshop-ground-domain-v1";
// 正常玩家使用已驗收的地坪，忽略殘留診斷旗標；有效捕捉保留明確舊版對照。
inline std::wstring_view ResolveProfile(bool validCapture,std::wstring_view requested) {
    if(!validCapture)return L"workshop-ground-domain-v1";
    if(requested.empty()||requested==L"original")return L"original";
    if(requested==L"workshop-ground-domain-v1")return L"workshop-ground-domain-v1";
    throw std::invalid_argument("SETTLEMENT_INVALID_CAPTURE_PROFILE");
}
inline constexpr std::size_t MaxMovedTriangles=512,MaxBoundaryVertices=128,MaxChangedVertices=1024;
// 既有道路基層是 forest-ground-04；共用落葉與岩石槽，不新增泥土貼圖。
struct Bounds {float west{},east{},south{},north{},doorX{},centerZ{};};
inline Bounds Region() {
    // 世界作者盒為常數，避免每個候選頂點重建碰撞盒 vector。
    static const Bounds bounds=[] {
        const auto roof=WorkshopSolids().at(0);
        return Bounds{roof.center.x-roof.extents.x-.5f,roof.center.x+roof.extents.x+3.5f,
            roof.center.z-roof.extents.z-.5f,roof.center.z+roof.extents.z+.5f,
            roof.center.x+roof.extents.x,roof.center.z};
    }();
    return bounds;
}
inline bool Inside(const Bounds& b,float x,float z) {
    if(x<b.west||x>b.east||z<b.south||z>b.north)return false;
    // 棚屋矩形地坪沿真正東側入口向外收束，不用隨機圓斑。
    const float extension=(std::max)(0.f,x-b.doorX);
    const float halfWidth=(b.north-b.south)*.5f-extension/3.5f;
    return std::abs(z-b.centerZ)<=halfWidth;
}
inline bool Inside(float x,float z) {return Inside(Region(),x,z);}
inline bool InInfluence(const Bounds& b,float x,float z) {
    const float f=EntranceRoadDomainFadeDistance;
    return x>=b.west-f&&x<=b.east+f&&z>=b.south-f&&z<=b.north+f;
}
inline bool InInfluence(float x,float z) {return InInfluence(Region(),x,z);}
struct Result {
    EntranceRoadDomainResult partition;
    std::vector<std::uint32_t> movedSourceTriangles,newBoundaryVertices;
    std::size_t changedForestVertices{},changedRoadVertices{};
};
inline bool SameFloat(float a,float b) noexcept {
    return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);
}
inline bool SameVertex(const Aevrynd::Vertex3D& a,const Aevrynd::Vertex3D& b) noexcept {
    return SameFloat(a.x,b.x)&&SameFloat(a.y,b.y)&&SameFloat(a.z,b.z)&&a.normal==b.normal&&
        a.color==b.color&&SameFloat(a.u,b.u)&&SameFloat(a.v,b.v);
}
inline bool SameMesh(const Aevrynd::Mesh3D& a,const Aevrynd::Mesh3D& b) {
    return a.indices==b.indices&&a.vertices.size()==b.vertices.size()&&
        std::equal(a.vertices.begin(),a.vertices.end(),b.vertices.begin(),SameVertex);
}
inline Result Create(const Aevrynd::Mesh3D& source,const EntranceRoadDomainResult& entrance) {
    const auto bounds=Region();
    // 完整重建核對，拒絕來源不符或已套用過的分域；沒有寫入呼叫端資源。
    const auto canonical=CreateEntranceRoadDomain(source);
    if(!SameMesh(canonical.forest,entrance.forest)||!SameMesh(canonical.road,entrance.road)||
        canonical.triangleIsRoad!=entrance.triangleIsRoad||
        canonical.sharedBoundaryVertices!=entrance.sharedBoundaryVertices)
        throw std::invalid_argument("SETTLEMENT_INPUT_DOMAIN_MISMATCH");
    Result out;out.partition=canonical;
    auto& p=out.partition;p.forest.indices.clear();
    std::vector<std::uint8_t> membership(source.vertices.size()),localMembership(source.vertices.size());
    for(std::size_t t=0;t<canonical.triangleIsRoad.size();++t){
        bool local=canonical.triangleIsRoad[t]==0;
        for(unsigned c=0;c<3;++c){const auto& v=source.vertices[source.indices[t*3+c]];
            local=local&&Inside(bounds,v.x,v.z);}
        if(local){out.movedSourceTriangles.push_back(std::uint32_t(t));p.triangleIsRoad[t]=1;}
        const bool road=p.triangleIsRoad[t]!=0;
        for(unsigned c=0;c<3;++c){const auto index=source.indices[t*3+c];
            // 原入口道路索引保留完整前綴，新土坪僅追加尾端；森林保留原相對次序。
            if(local)p.road.indices.push_back(index);
            else if(!road)p.forest.indices.push_back(index);
            membership[index]=std::uint8_t(membership[index]|(road?2u:1u));
            localMembership[index]=std::uint8_t(localMembership[index]|(local?2u:1u));
        }
    }
    if(out.movedSourceTriangles.empty()||out.movedSourceTriangles.size()>MaxMovedTriangles)
        throw std::invalid_argument("SETTLEMENT_TRIANGLE_BUDGET");
    p.sharedBoundaryVertices.clear();
    for(std::size_t i=0;i<membership.size();++i){
        if(membership[i]==3u)p.sharedBoundaryVertices.push_back(std::uint32_t(i));
        if(localMembership[i]==3u)out.newBoundaryVertices.push_back(std::uint32_t(i));
    }
    if(out.newBoundaryVertices.empty()||out.newBoundaryVertices.size()>MaxBoundaryVertices)
        throw std::invalid_argument("SETTLEMENT_BOUNDARY_BUDGET");
    for(std::size_t i=0;i<source.vertices.size();++i){const auto& v=source.vertices[i];
        if(!InInfluence(bounds,v.x,v.z))continue;
        float distance=EntranceRoadDomainFadeDistance;
        for(const auto index:out.newBoundaryVertices){const auto& b=source.vertices[index];
            distance=(std::min)(distance,std::hypot(v.x-b.x,v.z-b.z));}
        const float fade=ForestFloorSmooth(distance/EntranceRoadDomainFadeDistance);
        const float available=(std::max)(0.f,1.f-v.v-EntranceRoadDomainCoverageMargin);
        if((membership[i]&1u)!=0&&fade<1.f)
            p.forest.vertices[i].u=available*(1.f-fade)+canonical.forest.vertices[i].u*fade;
        if((localMembership[i]&2u)!=0)
            p.road.vertices[i].u=available*(1.f-.9f*fade);
        if(localMembership[i]==3u)p.forest.vertices[i].u=p.road.vertices[i].u=available;
        for(auto* value:{&p.forest.vertices[i].u,&p.road.vertices[i].u}){
            if(*value+v.v>1.f)*value=1.f-v.v;
            Aevrynd::Vec2 encoded;
            if(!Aevrynd::TryEncodeTerrainLayerCoverage3D(*value,v.v,encoded))
                throw std::invalid_argument("SETTLEMENT_COVERAGE_INVALID");
        }
    }
    auto& receipt=p.receipt;receipt=canonical.receipt;
    receipt.forestChangedVertices=receipt.roadChangedVertices=0;
    for(std::size_t i=0;i<source.vertices.size();++i){
        out.changedForestVertices+=!SameFloat(p.forest.vertices[i].u,canonical.forest.vertices[i].u);
        out.changedRoadVertices+=!SameFloat(p.road.vertices[i].u,canonical.road.vertices[i].u);
        receipt.forestChangedVertices+=!SameFloat(p.forest.vertices[i].u,source.vertices[i].u);
        receipt.roadChangedVertices+=!SameFloat(p.road.vertices[i].u,source.vertices[i].u);
    }
    if(out.changedForestVertices+out.changedRoadVertices>MaxChangedVertices)
        throw std::invalid_argument("SETTLEMENT_CHANGED_VERTEX_BUDGET");
    receipt.forestTriangles=p.forest.indices.size()/3;receipt.roadTriangles=p.road.indices.size()/3;
    receipt.sharedVertices=p.sharedBoundaryVertices.size();
    receipt.forestFingerprint=EntranceRoadDomainDetail::Fingerprint(p.forest);
    receipt.roadFingerprint=EntranceRoadDomainDetail::Fingerprint(p.road);
    return out;
}
}
