#pragma once
#include "MistwatchWorkshopVisual.h"

namespace Mistwatch::WorkshopFrame {
using namespace Aevrynd;
inline constexpr const char* Profile="framed-workshop-v1";
inline constexpr unsigned FrameMembers=54;
inline constexpr unsigned Vertices=WorkshopVisual::Vertices+FrameMembers*96u;
inline constexpr unsigned Indices=WorkshopVisual::Indices+FrameMembers*132u;
struct PartRange {
    unsigned part{};
    std::size_t firstVertex{},vertexCount{},firstIndex{},indexCount{};
    bool frame{};
};
inline std::array<PartRange,58> PartRanges() {
    std::array<PartRange,58> result{};
    std::size_t vertex{},index{};
    for(unsigned part=0;part<4;++part){const auto boards=WorkshopVisual::BoardCounts[part];
        result[part]={part,vertex,boards*96u,index,boards*132u,false};vertex+=boards*96u;index+=boards*132u;}
    for(unsigned frame=0;frame<FrameMembers;++frame){result[4+frame]={1+frame/18,vertex,96,index,132,true};vertex+=96;index+=132;}
    return result;
}
inline std::uint64_t Fingerprint(const Mesh3D& mesh) {
    std::uint64_t hash=14695981039346656037ull;
    const auto word=[&](std::uint32_t value){for(unsigned byte=0;byte<4;++byte){hash^=(value>>(byte*8))&255u;hash*=1099511628211ull;}};
    word(static_cast<std::uint32_t>(mesh.vertices.size()));word(static_cast<std::uint32_t>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float field:{v.x,v.y,v.z,v.u,v.v})word(std::bit_cast<std::uint32_t>(field));word(v.normal);word(v.color);}
    for(auto index:mesh.indices)word(index);return hash;
}
namespace Detail {
inline Vec3 Point(const Vertex3D& v){return {v.x,v.y,v.z};}
inline void RecalculateNormals(Mesh3D& mesh,std::size_t first,std::size_t count) {
    // 厚度非等比縮放後，以實際小倒角三角面重算法線，不以一般向量門檻抹掉小面積。
    for(std::size_t i=first;i<first+count;i+=3){const auto a=mesh.indices[i],b=mesh.indices[i+1],c=mesh.indices[i+2];Vec3 normal{};
        if(!TryComputeTriangleNormal(Point(mesh.vertices[a]),Point(mesh.vertices[b]),Point(mesh.vertices[c]),normal))
            throw std::runtime_error("WORKSHOP_FRAME_DEGENERATE_INSET");
        for(auto index:{a,b,c})mesh.vertices[index].normal=PackNormalSnorm8(normal);}
}
inline void AppendMember(Mesh3D& mesh,unsigned ordinal,Vec3 centre,Vec3 size,Vec3 along,Vec3 up,Vec3 normal) {
    const auto first=mesh.vertices.size();
    if(!AppendBeveledBox3D(mesh,centre,size,.002f,{.64f,.61f,.56f,1},along,up,normal))
        throw std::runtime_error("WORKSHOP_FRAME_MEMBER_GEOMETRY");
    const std::array<Vec3,3> axes{along,up,normal};const std::array<float,3> lengths{size.x,size.y,size.z};
    const auto band=WorkshopVisual::Bands[ordinal%4];
    for(std::size_t i=first;i<mesh.vertices.size();++i){auto& v=mesh.vertices[i];const auto packed=UnpackNormalSnorm8(v.normal);
        const Vec3 n{packed.x,packed.y,packed.z};unsigned dominant{};
        for(unsigned axis=1;axis<3;++axis)if(std::abs(Dot(n,axes[axis]))>std::abs(Dot(n,axes[dominant])))dominant=axis;
        unsigned transverse=(dominant+1)%3,longitudinal=(dominant+2)%3;
        if(lengths[transverse]>lengths[longitudinal])std::swap(transverse,longitudinal);
        const auto p=Point(v)-centre;
        // 木紋V沿每根構件的長軸；端面則使用剩餘兩軸，保留全部倒角的非零UV面積。
        v.u=(band.x+(Dot(p,axes[transverse])/lengths[transverse]+.5f)*(band.y-band.x))/WorkshopVisual::StaticDecodedScale;
        v.v=(.13f*float(ordinal%5)+(Dot(p,axes[longitudinal])+lengths[longitudinal]*.5f)/WorkshopVisual::LongitudinalMeters)/WorkshopVisual::StaticDecodedScale;
    }
}
}
inline Mesh3D Create() {
    auto result=WorkshopVisual::Create();const auto solids=WorkshopSolids();const auto ranges=PartRanges();
    if(solids.size()!=4||result.vertices.size()!=WorkshopVisual::Vertices||result.indices.size()!=WorkshopVisual::Indices)
        throw std::runtime_error("WORKSHOP_FRAME_BASELINE_MEMBERSHIP");
    result.vertices.reserve(Vertices);result.indices.reserve(Indices);
    // 原屋頂頂點、索引與序列完整保留。牆板只沿厚度退縮，露出梁柱實際深度。
    for(unsigned part=1;part<4;++part){const auto& range=ranges[part];const auto centre=solids[part].center;
        for(std::size_t i=range.firstVertex;i<range.firstVertex+range.vertexCount;++i){auto& v=result.vertices[i];
            if(part==1)v.x=centre.x+(v.x-centre.x)*.8f;else v.z=centre.z+(v.z-centre.z)*.8f;}
        Detail::RecalculateNormals(result,range.firstIndex,range.indexCount);
    }
    unsigned ordinal{};
    for(unsigned part=1;part<4;++part){const auto box=solids[part];const bool west=part==1;
        const Vec3 along=west?Vec3{0,0,1}:Vec3{1,0,0},up{0,1,0},normal=Cross(along,up);
        const float half=west?box.extents.z:box.extents.x;
        // 西牆端柱退讓南北牆的內框厚度，避免跨牆轉角的框架互穿。
        const float postCentre=half-(west?.30f:.12f);
        for(float side:{-1.f,1.f}){const auto centre=box.center+normal*(side*.09f);
            for(float offset:{-postCentre,0.f,postCentre})
                Detail::AppendMember(result,ordinal++,centre+along*offset,{.18f,2.54f,.018f},along,up,normal);
            // 每道橫梁拆成兩段，與立柱端面至少相隔1cm，不做重疊表面遮蓋。
            for(float offset:{-1.12f,1.12f})for(float sign:{-1.f,1.f})
                Detail::AppendMember(result,ordinal++,centre+up*offset+along*(sign*postCentre*.5f),
                    {postCentre-.20f,.18f,.018f},along,up,normal);
            for(float sign:{-1.f,1.f}){const auto a=centre+along*(sign*.20f)+up*(-.94f);
                const auto b=centre+along*(sign*(postCentre-.17f))+up*.94f;
                const auto axis=Normalize(b-a),across=Cross(normal,axis);
                Detail::AppendMember(result,ordinal++,(a+b)*.5f,{Length(b-a),.14f,.018f},axis,across,normal);}
        }
    }
    if(ordinal!=FrameMembers||result.vertices.size()!=Vertices||result.indices.size()!=Indices||!result.Valid())
        throw std::runtime_error("WORKSHOP_FRAME_OUTPUT_MEMBERSHIP");
    result.InvalidateRaycastIndex();return result;
}
}
