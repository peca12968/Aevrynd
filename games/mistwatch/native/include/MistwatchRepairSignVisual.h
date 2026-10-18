#pragma once
#include "MistwatchSession.h"
#include <Aevrynd/BeveledBox3D.h>
#include <Aevrynd/CappedCylinder3D.h>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

namespace Mistwatch::RepairSign {
enum class Primitive : unsigned {BeveledBox,CappedHead};
struct Part {unsigned group;Primitive primitive;Aevrynd::Vec3 center,size;float bevel;};
// 共面裝配Z皆用二進位有理數：背柱／木板／漆牌／螺頭不藏體積交叉。
inline constexpr std::array<Part,6> Source{{
    {0,Primitive::BeveledBox,{0,.6f,9.f/256.f},{.1f,1.2f,5.f/128.f},.004f},
    {0,Primitive::BeveledBox,{0,1.036f,-1.f/256.f},{.68f,.16f,5.f/128.f},.003f},
    {0,Primitive::BeveledBox,{0,1.204f,-1.f/256.f},{.68f,.16f,5.f/128.f},.003f},
    {1,Primitive::BeveledBox,{0,1.204f,-13.f/512.f},{.26f,.106f,1.f/256.f},.001f},
    {2,Primitive::CappedHead,{-.085f,1.204f,-1.f/32.f},{.02f,.02f,1.f/128.f},0},
    {2,Primitive::CappedHead,{ .085f,1.204f,-1.f/32.f},{.02f,.02f,1.f/128.f},0}
}};
inline constexpr std::array<unsigned,3> VertexCounts{288,96,100},IndexCounts{396,132,288};
inline constexpr unsigned TotalVertices=484,TotalTriangles=272,HeadSides=12;
inline constexpr float StaticScale=2048.f*(16.f/32767.f);
struct Range {unsigned group{},firstVertex{},vertexCount{},firstIndex{},indexCount{};};
struct Geometry {std::array<Aevrynd::Mesh3D,3> meshes;std::array<Range,6> parts{};};

inline Aevrynd::Vec3 Anchor(unsigned site){
    if(site>=Session::repairSites.size())throw std::invalid_argument("Repair sign site");
    return Session::repairSites[site]+Aevrynd::Vec3{site==0?2.8f:1.3f,0,0};
}
inline bool Visible(unsigned repairMaterials,unsigned site){
    if(site>=Session::repairSites.size())throw std::invalid_argument("Repair sign visible site");
    return !(repairMaterials&(1u<<site));
}
inline std::array<Aevrynd::Material3D,3> Materials(Aevrynd::Material3D originalWood){
    if(!originalWood.textures)throw std::invalid_argument("Repair sign original wood textures absent");
    originalWood.useUv=true;originalWood.textureScale=1.f;
    return {originalWood,Aevrynd::Material3D{{1,1,1,1},{.72f,0,0,1}},
        Aevrynd::Material3D{{1,1,1,1},{.65f,.7f,0,1}}};
}
namespace Detail {
inline void WoodUv(Aevrynd::Mesh3D& mesh,std::size_t begin,unsigned part){
    using namespace Aevrynd;
    Vec3 lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
    for(std::size_t i=begin;i<mesh.vertices.size();++i){const auto& p=mesh.vertices[i];
        lo.x=(std::min)(lo.x,p.x);hi.x=(std::max)(hi.x,p.x);lo.y=(std::min)(lo.y,p.y);hi.y=(std::max)(hi.y,p.y);
        lo.z=(std::min)(lo.z,p.z);hi.z=(std::max)(hi.z,p.z);}
    const auto q=[](float p,float a,float b){const float t=(p-a)/(b-a);
        if(!std::isfinite(t)||t<0||t>1)throw std::runtime_error("Repair sign wood UV extent");return t;};
    const float phase=.1f+.13f*float(part);
    for(std::size_t i=begin;i<mesh.vertices.size();++i){auto& p=mesh.vertices[i];const auto n=UnpackNormalSnorm8(p.normal);
        const float x=std::abs(n.x),y=std::abs(n.y),z=std::abs(n.z);float along{},across{};
        // 原WoodSiding006木紋沿U；柱沿Y、兩板沿X，V窄帶避開圖內接縫。
        if(part==0){
            if(y>=x&&y>=z){along=p.x-lo.x;across=q(p.z,lo.z,hi.z);}
            else{along=p.y-lo.y;across=x>=z?q(p.z,lo.z,hi.z):q(p.x,lo.x,hi.x);}
        }else{
            if(x>=y&&x>=z){along=p.z-lo.z;across=q(p.y,lo.y,hi.y);}
            else{along=p.x-lo.x;across=y>=z?q(p.z,lo.z,hi.z):q(p.y,lo.y,hi.y);}
        }
        p.u=(phase+along*.5f)/StaticScale;p.v=(.025f+.05f*across)/StaticScale;
    }
}
inline void Head(Aevrynd::Mesh3D& out,const Part& part){
    using namespace Aevrynd;
    const auto unit=CreateUnitCappedCylinder3D(HeadSides);const auto base=static_cast<unsigned>(out.vertices.size());
    for(auto p:unit.vertices){
        const auto z=std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(p.normal>>16));
        const auto minusZ=std::bit_cast<std::uint8_t>(static_cast<std::int8_t>(-int(z)));
        p.normal=(p.normal&0xff0000ffu)|(std::uint32_t(minusZ)<<8)|((p.normal&0xff00u)<<8);
        const auto original=p;
        p.x=part.center.x+original.x*part.size.x;p.y=part.center.y-original.z*part.size.y;p.z=part.center.z+original.y*part.size.z;
        p.color=PackColorRgba8({.62f,.65f,.68f,1});out.vertices.push_back(p);
    }
    for(const auto index:unit.indices)out.indices.push_back(base+index);out.InvalidateRaycastIndex();
}
}
// 初始化時只建一次三個共享mesh；不讀／寫session，不提交scene、不建collider。
[[nodiscard]] inline Geometry Create(){
    using namespace Aevrynd;
    Geometry result;
    for(unsigned g=0;g<3;++g){result.meshes[g].vertices.reserve(VertexCounts[g]);result.meshes[g].indices.reserve(IndexCounts[g]);}
    for(unsigned i=0;i<Source.size();++i){const auto& part=Source[i];auto& mesh=result.meshes[part.group];auto& range=result.parts[i];
        range.group=part.group;range.firstVertex=static_cast<unsigned>(mesh.vertices.size());range.firstIndex=static_cast<unsigned>(mesh.indices.size());
        if(part.primitive==Primitive::BeveledBox){
            const Color color=part.group?Color{.78f,.58f,.15f,1}:Color{1,1,1,1};
            if(!AppendBeveledBox3D(mesh,part.center,part.size,part.bevel,color))throw std::runtime_error("Repair sign bevel part");
            if(part.group==0)Detail::WoodUv(mesh,range.firstVertex,i);
        }else Detail::Head(mesh,part);
        range.vertexCount=static_cast<unsigned>(mesh.vertices.size())-range.firstVertex;
        range.indexCount=static_cast<unsigned>(mesh.indices.size())-range.firstIndex;
    }
    for(unsigned g=0;g<3;++g)if(!result.meshes[g].Valid()||result.meshes[g].vertices.size()!=VertexCounts[g]||result.meshes[g].indices.size()!=IndexCounts[g])
        throw std::runtime_error("Repair sign fixed counts");
    return result;
}
}
