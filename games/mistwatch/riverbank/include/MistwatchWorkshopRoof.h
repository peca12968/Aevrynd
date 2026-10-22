#pragma once
#include "MistwatchWorkshopFrame.h"

namespace Mistwatch::WorkshopRoof {
using namespace Aevrynd;
inline constexpr const char* Profile="workshop-gabled-roof-candidate-v1";
inline constexpr float EaveHeight=2.92f,RidgeSurfaceHeight=3.76f,MaximumHeight=3.83f;
inline constexpr float HalfSlopeSpan=1.94f,RoofHalfLength=2.48f;
inline constexpr unsigned RoofBoards=40,BeveledMembers=47,GableBoards=28;
inline constexpr std::size_t AddedVertices=BeveledMembers*96u+GableBoards*24u;
inline constexpr std::size_t AddedIndices=BeveledMembers*132u+GableBoards*36u;
enum class PartKind { SlopeBoard,Eave,Ridge,Rafter,GableBoard };
struct Part {PartKind kind{};std::size_t firstVertex{},vertexCount{},firstIndex{},indexCount{};};
// 僅供相機的保守包圍盒；不冒稱原角色碰撞已覆蓋新屋脊，也不寫入 WorldSolids。
inline BoxCollider3D CameraRoofSolid() {
    return {Session::fuse+Vec3{0,3.35f,0},{2.5f,.50f,2.f}};
}
namespace Detail {
inline void GableBoard(Mesh3D& mesh,unsigned ordinal,float side,float lowZ,float highZ) {
    const auto p=Session::fuse;
    const float lowX=side>0?2.42f:-2.46f,highX=side>0?2.46f:-2.42f;
    const auto top=[](float z){return 3.72f-(3.72f-2.87f)*std::abs(z)/1.90f;};
    const float bottom=2.86f,leftTop=top(lowZ),rightTop=top(highZ);
    if(!(leftTop>bottom&&rightTop>bottom))throw std::runtime_error("WORKSHOP_ROOF_GABLE_HEIGHT");
    const std::array<Vec3,8> points{{
        p+Vec3{lowX,bottom,lowZ},p+Vec3{lowX,bottom,highZ},p+Vec3{lowX,rightTop,highZ},p+Vec3{lowX,leftTop,lowZ},
        p+Vec3{highX,bottom,lowZ},p+Vec3{highX,bottom,highZ},p+Vec3{highX,rightTop,highZ},p+Vec3{highX,leftTop,lowZ}}};
    Vec3 centre{};for(auto v:points)centre=centre+v;centre=centre*.125f;
    const auto band=WorkshopVisual::Bands[ordinal%4];
    const auto face=[&](std::array<unsigned,4> ids) {
        Vec3 sum{};for(auto id:ids)sum=sum+(points[id]-centre);
        if(Dot(Cross(points[ids[1]]-points[ids[0]],points[ids[2]]-points[ids[0]]),sum)<0)
            std::reverse(ids.begin()+1,ids.end());
        Vec3 n{};if(!TryComputeTriangleNormal(points[ids[0]],points[ids[1]],points[ids[2]],n))
            throw std::runtime_error("WORKSHOP_ROOF_GABLE_FACE");
        const auto first=static_cast<std::uint32_t>(mesh.vertices.size());
        for(auto id:ids){const auto v=points[id],local=v-p;float across{},along{};
            // 山牆正反面木紋沿直立板；厚邊仍保留非零 UV 面積，不旋轉或偏移原材質。
            if(std::abs(n.x)>.5f){across=(local.z-lowZ)/(highZ-lowZ);along=local.y-bottom;}
            else if(std::abs(n.y)>=std::abs(n.z)){across=(local.x-lowX)/(highX-lowX);along=local.z-lowZ;}
            else{across=(local.x-lowX)/(highX-lowX);along=local.y-bottom;}
            const float u=(band.x+across*(band.y-band.x))/WorkshopVisual::StaticDecodedScale;
            const float vv=(.13f*float(ordinal%5)+along/WorkshopVisual::LongitudinalMeters)/WorkshopVisual::StaticDecodedScale;
            mesh.vertices.push_back({v.x,v.y,v.z,PackNormalSnorm8(n),PackColorRgba8({.64f,.61f,.56f,1}),u,vv});
        }
        mesh.indices.insert(mesh.indices.end(),{first,first+1,first+2,first,first+2,first+3});
    };
    face({0,1,2,3});face({4,7,6,5});face({0,4,5,1});
    face({3,2,6,7});face({0,3,7,4});face({1,5,6,2});
}
}
inline Mesh3D Create(std::vector<Part>* outputParts=nullptr) {
    auto result=WorkshopFrame::Create();const auto p=Session::fuse;
    if(result.vertices.size()!=12480||result.indices.size()!=17160)
        throw std::runtime_error("WORKSHOP_ROOF_ORIGINAL_MEMBERSHIP");
    std::vector<Part> parts;parts.reserve(BeveledMembers+GableBoards);unsigned ordinal=WorkshopFrame::FrameMembers;
    const auto member=[&](PartKind kind,Vec3 centre,Vec3 size,Vec3 a=Vec3{1,0,0},Vec3 b=Vec3{0,1,0},Vec3 c=Vec3{0,0,1}) {
        Part part{kind,result.vertices.size(),0,result.indices.size(),0};
        WorkshopFrame::Detail::AppendMember(result,ordinal++,p+centre,size,a,b,c);
        part.vertexCount=result.vertices.size()-part.firstVertex;part.indexCount=result.indices.size()-part.firstIndex;parts.push_back(part);
    };
    const float rise=RidgeSurfaceHeight-EaveHeight,slopeLength=std::hypot(HalfSlopeSpan,rise);
    const float pitch=2.f*RoofHalfLength/20.f;
    for(float side:{-1.f,1.f}){
        const Vec3 across{1,0,0},along{0,rise/slopeLength,-side*HalfSlopeSpan/slopeLength},normal=Cross(across,along);
        for(unsigned board=0;board<20;++board)
            member(PartKind::SlopeBoard,{-RoofHalfLength+(float(board)+.5f)*pitch,(EaveHeight+RidgeSurfaceHeight)*.5f,side*HalfSlopeSpan*.5f},
                {pitch-.006f,slopeLength,.07f},across,along,normal);
    }
    for(float side:{-1.f,1.f})member(PartKind::Eave,{0,2.94f,side*1.935f},{4.96f,.16f,.12f});
    member(PartKind::Ridge,{0,3.78f,0},{4.96f,.10f,.20f});
    // 東西山牆斜椽沿坡面下方；與山牆板形成真厚度接合，不侵入舊底板以下。
    for(float end:{-1.f,1.f})for(float side:{-1.f,1.f}){
        const Vec3 along{0,rise/slopeLength,-side*HalfSlopeSpan/slopeLength};
        const Vec3 across{0,side*HalfSlopeSpan/slopeLength,rise/slopeLength};
        const float middle=.885f,height=EaveHeight+rise*(1.f-middle/HalfSlopeSpan)-.10f;
        member(PartKind::Rafter,{end*2.47f,height,side*middle},{1.53f*slopeLength/HalfSlopeSpan,.12f,.036f},along,across,Cross(along,across));
    }
    for(float end:{-1.f,1.f})for(unsigned board=0;board<14;++board){
        const float lo=-1.90f+3.80f*float(board)/14.f+.002f;
        const float hi=-1.90f+3.80f*float(board+1)/14.f-.002f;
        Part part{PartKind::GableBoard,result.vertices.size(),0,result.indices.size(),0};
        Detail::GableBoard(result,ordinal++,end,lo,hi);
        part.vertexCount=result.vertices.size()-part.firstVertex;part.indexCount=result.indices.size()-part.firstIndex;parts.push_back(part);
    }
    if(result.vertices.size()!=WorkshopFrame::Vertices+AddedVertices||result.indices.size()!=WorkshopFrame::Indices+AddedIndices||
        parts.size()!=BeveledMembers+GableBoards||!result.Valid())throw std::runtime_error("WORKSHOP_ROOF_COMPLETE_MEMBERSHIP");
    result.InvalidateRaycastIndex();if(outputParts)*outputParts=std::move(parts);return result;
}
}
