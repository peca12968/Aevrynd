#pragma once
#include "MistwatchWorkshopRoof.h"
#include <Aevrynd/ConvexAabbSweep3D.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace Mistwatch::WorkshopRoofCollision {
using namespace Aevrynd;
namespace Visual=WorkshopRoof;
inline constexpr std::size_t MemberCount=75,OriginalVertices=12480,OriginalIndices=17160;
inline constexpr std::size_t TotalVertices=17664,TotalIndices=24372;
inline constexpr std::uint64_t ExpectedVisualFingerprint=358593194902208623ull;
struct Member {
    Visual::Part source{};
    std::array<Vec3,ConvexAabbSweep3D::MaximumPoints> points{};
    std::array<std::size_t,ConvexAabbSweep3D::MaximumPoints> firstSourceVertex{};
    std::array<std::uint8_t,96> sourceVertexToPoint{};
    std::size_t pointCount{};
    // 原頂點的精確分量界；僅可供 broadphase 排除，不能當成支撐面。
    Vec3 minimum{},maximum{};
    ConvexAabbSweep3D hull{};
};
inline Vec3 VertexPoint(const Vertex3D& v)noexcept{return{v.x,v.y,v.z};}
inline bool SamePoint(Vec3 a,Vec3 b)noexcept{return a.x==b.x&&a.y==b.y&&a.z==b.z;}
inline bool FinitePoint(Vec3 p)noexcept{return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline Visual::PartKind ExpectedKind(std::size_t member)noexcept{
    if(member<40)return Visual::PartKind::SlopeBoard;
    if(member<42)return Visual::PartKind::Eave;
    if(member==42)return Visual::PartKind::Ridge;
    if(member<47)return Visual::PartKind::Rafter;
    return Visual::PartKind::GableBoard;
}
// 只取候選新增的 75 個構件，不加入相機包圍盒、不改原模型或控制器。
// 不近似焊接：每個凸包點保留首個來源索引，全部重複頂點另存對應表。
// 失敗保留 output；準備期間允許配置，建成後的引擎查詢不配置。
inline bool Build(const Mesh3D& mesh,std::span<const Visual::Part> parts,std::vector<Member>& output)noexcept try{
    if(parts.size()!=MemberCount||mesh.vertices.size()!=TotalVertices||mesh.indices.size()!=TotalIndices||!mesh.Valid())return false;
    std::size_t nextVertex=OriginalVertices,nextIndex=OriginalIndices;
    for(std::size_t ordinal=0;ordinal<parts.size();++ordinal){
        const auto& part=parts[ordinal];const bool beveled=ordinal<47;
        if(part.kind!=ExpectedKind(ordinal)||part.firstVertex!=nextVertex||part.firstIndex!=nextIndex||
           part.vertexCount!=(beveled?96u:24u)||part.indexCount!=(beveled?132u:36u))return false;
        if(part.firstVertex>mesh.vertices.size()||part.vertexCount>mesh.vertices.size()-part.firstVertex||
           part.firstIndex>mesh.indices.size()||part.indexCount>mesh.indices.size()-part.firstIndex)return false;
        for(std::size_t j=part.firstIndex;j<part.firstIndex+part.indexCount;++j)
            if(mesh.indices[j]<part.firstVertex||mesh.indices[j]>=part.firstVertex+part.vertexCount)return false;
        nextVertex+=part.vertexCount;nextIndex+=part.indexCount;
    }
    if(nextVertex!=TotalVertices||nextIndex!=TotalIndices)return false;
    std::vector<Member> candidate;candidate.reserve(MemberCount);
    for(std::size_t ordinal=0;ordinal<parts.size();++ordinal){
        candidate.emplace_back();auto& member=candidate.back();member.source=parts[ordinal];
        for(std::size_t j=0;j<member.source.vertexCount;++j){
            const auto index=member.source.firstVertex+j;const auto point=VertexPoint(mesh.vertices[index]);
            if(!FinitePoint(point))return false;
            if(j==0)member.minimum=member.maximum=point;
            else{
                member.minimum={std::min(member.minimum.x,point.x),std::min(member.minimum.y,point.y),std::min(member.minimum.z,point.z)};
                member.maximum={std::max(member.maximum.x,point.x),std::max(member.maximum.y,point.y),std::max(member.maximum.z,point.z)};
            }
            std::size_t welded{};while(welded<member.pointCount&&!SamePoint(member.points[welded],point))++welded;
            if(welded==member.pointCount){
                if(member.pointCount==member.points.size())return false;
                member.points[welded]=point;member.firstSourceVertex[welded]=index;++member.pointCount;
            }
            member.sourceVertexToPoint[j]=static_cast<std::uint8_t>(welded);
        }
        // 已讀取的原建模流程：倒角盒 24 個角點，山牆板 8 個角點。
        if(member.pointCount!=(ordinal<47?24u:8u)||!member.hull.Prepare(std::span(member.points).first(member.pointCount)))return false;
    }
    // 固定候選的點與拓撲皆須匹配；相同點數不代表仍是同一視覺資產。
    if(WorkshopFrame::Fingerprint(mesh)!=ExpectedVisualFingerprint)return false;
    output.swap(candidate);return true;
}catch(...){return false;}
}
