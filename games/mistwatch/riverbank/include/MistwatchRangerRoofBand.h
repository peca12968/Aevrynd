#pragma once
#include "MistwatchRangerHut.h"
#include <bit>
#include <cstring>
#include <limits>

namespace Mistwatch::RangerRoofBand {
namespace Detail {
using namespace Aevrynd;
using Part=RangerHutPart;
inline bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline bool PointBits(Vec3 a,Vec3 b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
inline bool PartBits(const Part& a,const Part& b){
    return a.kind==b.kind&&a.firstIndex==b.firstIndex&&a.indexCount==b.indexCount&&PointBits(a.centre,b.centre);
}
inline void Need(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
inline double Jacobian(const Mesh3D& mesh,std::size_t first){
    const auto& a=mesh.vertices[mesh.indices[first]];const auto& b=mesh.vertices[mesh.indices[first+1]];
    const auto& c=mesh.vertices[mesh.indices[first+2]];
    return (double(b.u)-a.u)*(double(c.v)-a.v)-(double(b.v)-a.v)*(double(c.u)-a.u);
}
inline bool UvDomain(const Mesh3D& mesh){
    if(!mesh.Valid())return false;
    for(const auto& v:mesh.vertices)
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v))return false;
    for(std::size_t first=0;first<mesh.indices.size();first+=3){
        const double j=Jacobian(mesh,first);
        if(!std::isfinite(j)||j<=1.e-9)return false;
    }
    return true;
}
// 重播原append的虛擬partNumber；不是Combined的parts vector ordinal。
inline std::size_t FirstRoofSourcePart(){
    const auto solids=RangerHutSolids();Need(solids.size()==13,"V1212 source solids count drift");
    std::size_t ordinal=1;
    for(std::size_t i=1;i<solids.size();++i){
        if(i==2)continue;
        const auto size=solids[i].extents*2.f;
        if(size.x<.4f&&size.z<.4f){++ordinal;continue;}
        const auto rows=static_cast<unsigned>(std::round(size.y/.2f));
        const float span=size.x<size.z?size.z:size.x;
        ordinal+=1u+std::size_t(rows)*(span>3.f?3u:1u);
    }
    ordinal+=3u+2u*(4u+2u*2u)+2u;
    Need(ordinal==114,"V1212 source append ordinal drift");return ordinal;
}
struct Axes {unsigned grain{},across{};float sign{};};
inline Axes FaceAxes(Vec3 normal,Vec3 size){
    const std::array lengths{size.x,size.y,size.z},normals{normal.x,normal.y,normal.z};
    unsigned plane=0;for(unsigned axis=1;axis<3;++axis)if(std::abs(normals[axis])>std::abs(normals[plane]))plane=axis;
    unsigned grain=plane==0?1u:0u;
    for(unsigned axis=0;axis<3;++axis)if(axis!=plane&&lengths[axis]>lengths[grain])grain=axis;
    const unsigned across=3u-plane-grain;Vec3 u{},v{};
    if(across==0)u.x=1;else if(across==1)u.y=1;else u.z=1;
    if(grain==0)v.x=1;else if(grain==1)v.y=1;else v.z=1;
    return {grain,across,Dot(Cross(u,v),normal)<0?-1.f:1.f};
}
inline float FixedU(const Vertex3D& local,Vec3 size){
    const auto n=UnpackNormalSnorm8(local.normal);const auto axes=FaceAxes({n.x,n.y,n.z},size);
    const std::array positions{local.x,local.y,local.z};
    const float t=Clamp(positions[axes.across]*axes.sign+.5f,0.f,1.f);
    return 932.f/1024.f+t*(74.f/1024.f);
}
struct RoofRange {std::size_t part{},base{};};
inline std::array<RoofRange,24> SourceRanges(const Mesh3D& source,const std::vector<Part>& parts){
    Need(source.vertices.size()==6864&&source.indices.size()==9600&&parts.size()==132&&UvDomain(source),
         "V1212 source counts/finite/positive UV drift");
    const auto box=Mesh3D::CreateUnitBox();Need(box.vertices.size()==24&&box.indices.size()==36,"V1212 unit box drift");
    std::size_t covered{};
    for(const auto& part:parts){
        Need(part.firstIndex==covered&&part.indexCount>0&&part.indexCount%3==0&&
             part.indexCount<=source.indices.size()-covered,"V1212 source part coverage drift");covered+=part.indexCount;
    }
    Need(covered==source.indices.size(),"V1212 incomplete source coverage");
    const std::size_t firstSourcePart=FirstRoofSourcePart();
    constexpr std::array<Vec2,4> bands{{{132.f/1024.f,216.f/1024.f},{360.f/1024.f,436.f/1024.f},
        {694.f/1024.f,758.f/1024.f},{932.f/1024.f,1006.f/1024.f}}};
    Need(HutWornPlankBands.size()==bands.size(),"V1212 source atlas band count drift");
    for(std::size_t i=0;i<bands.size();++i)
        Need(Bits(HutWornPlankBands[i].x,bands[i].x)&&Bits(HutWornPlankBands[i].y,bands[i].y),"V1212 source atlas bands drift");
    std::array<RoofRange,24> ranges{};std::vector<bool> owned(source.vertices.size());
    const float angle=std::atan(.85f/2.48f);const Vec3 size{std::hypot(2.48f,.85f),.1f,.494f};
    for(unsigned ordinal=0;ordinal<24;++ordinal){
        const float side=ordinal<12?-1.f:1.f;
        const Vec3 center{side*1.24f,3.335f,-2.75f+.5f*float(ordinal%12u)};
        std::size_t match=parts.size(),matches{};
        for(std::size_t p=0;p<parts.size();++p)if(parts[p].kind==RangerHutPartKind::Roof&&PointBits(parts[p].centre,center)){
            match=p;++matches;
        }
        Need(matches==1,"V1212 main roof source center absent/ambiguous");const auto& part=parts[match];
        Need(part.indexCount==36,"V1212 main roof primitive drift");
        const std::size_t base=source.indices[part.firstIndex];
        Need(base<=source.vertices.size()-24,"V1212 roof vertex range drift");
        const auto matrix=CreateTransformMatrix(center,{0,0,-side*angle},size);const auto normals=CreateNormalMatrix(matrix);
        const auto band=bands[(firstSourcePart+ordinal)%4u];
        for(std::size_t k=0;k<box.indices.size();++k)
            Need(source.indices[part.firstIndex+k]==base+box.indices[k],"V1212 roof source index order drift");
        for(std::size_t k=0;k<box.vertices.size();++k){
            const auto& local=box.vertices[k];const auto& actual=source.vertices[base+k];
            const auto n=UnpackNormalSnorm8(local.normal);const Vec3 normal{n.x,n.y,n.z};
            const auto point=TransformPoint(matrix,{local.x,local.y,local.z});
            const auto axes=FaceAxes(normal,size);const std::array positions{local.x,local.y,local.z},lengths{size.x,size.y,size.z};
            const float t=Clamp(positions[axes.across]*axes.sign+.5f,0.f,1.f);
            const float oldU=band.x+t*(band.y-band.x);
            const float oldV=Clamp(positions[axes.grain]+.5f,0.f,1.f)*lengths[axes.grain]/1.4f+.17f*float((firstSourcePart+ordinal)%5u);
            Need(!owned[base+k]&&PointBits({actual.x,actual.y,actual.z},point)&&
                 actual.normal==PackNormalSnorm8(TransformNormal(normals,normal))&&actual.color==PackColorRgba8({.76f,.75f,.71f,1})&&
                 Bits(actual.u,oldU)&&Bits(actual.v,oldV),"V1212 roof source shape/normal/color/UV drift");
            owned[base+k]=true;
        }
        ranges[ordinal]={match,base};
    }
    // 每筆owned record只能被其自己的完整36-index部件引用。
    for(std::size_t p=0;p<parts.size();++p){const auto& part=parts[p];
        for(std::size_t k=0;k<part.indexCount;++k){const auto index=source.indices[part.firstIndex+k];if(!owned[index])continue;
            bool own=false;for(const auto& range:ranges)if(range.part==p&&index>=range.base&&index-range.base<24)own=true;
            Need(own,"V1212 shared roof/protected vertex alias drift");
        }
    }
    return ranges;
}
}

// 輸入是原Combined；所有檢查先完成，回傳值才擁有新U。沒有in-place部分寫入。
inline Aevrynd::Mesh3D ApplyFixedRoofBand(const Aevrynd::Mesh3D& source,const std::vector<RangerHutPart>& parts){
    using namespace Detail;
    std::vector<Part> canonicalParts;const auto canonical=CreateRangerHut({},&canonicalParts);
    static_assert(sizeof(Vertex3D)==28);
    Need(source.vertices.size()==canonical.vertices.size()&&source.indices==canonical.indices&&parts.size()==canonicalParts.size(),
         "V1212 source canonical membership drift");
    for(std::size_t i=0;i<parts.size();++i)Need(PartBits(parts[i],canonicalParts[i]),"V1212 source canonical part drift");
    for(std::size_t i=0;i<source.vertices.size();++i)
        Need(std::memcmp(&source.vertices[i],&canonical.vertices[i],28)==0,"V1212 source canonical vertex drift");
    const auto ranges=SourceRanges(source,parts);Mesh3D result=source;
    const auto box=Mesh3D::CreateUnitBox();const Vec3 size{std::hypot(2.48f,.85f),.1f,.494f};
    std::vector<bool> owned(source.vertices.size());
    for(const auto& range:ranges)for(std::size_t k=0;k<24;++k){
        const auto i=range.base+k;result.vertices[i].u=FixedU(box.vertices[k],size);owned[i]=true;
    }
    Need(UvDomain(result),"V1212 fixed band positive float32 UV gate failed");
    auto inverse=result;
    for(std::size_t i=0;i<result.vertices.size();++i){
        const auto& before=source.vertices[i];const auto& after=result.vertices[i];
        Need(std::memcmp(&before,&after,20)==0&&Bits(before.v,after.v)&&
             (owned[i]||Bits(before.u,after.u)),"V1212 nonowned byte closure failed");
        if(owned[i])inverse.vertices[i].u=before.u;
        Need(std::memcmp(&inverse.vertices[i],&before,28)==0,"V1212 whole vertex inverse failed");
    }
    Need(result.indices==source.indices,"V1212 index closure failed");
    const auto a=result.Bounds(),b=source.Bounds();
    Need(PointBits(a.center,b.center)&&PointBits(a.extents,b.extents)&&Bits(a.radius,b.radius),"V1212 bounds closure failed");
    return result;
}
inline Aevrynd::Mesh3D CreateRangerHutRoofBand(){
    std::vector<RangerHutPart> parts;const auto source=CreateRangerHut({},&parts);
    return ApplyFixedRoofBand(source,parts);
}
}
