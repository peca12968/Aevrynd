#pragma once
#include "MistwatchRangerRoofBand.h"
#include <array>
#include <bit>
#include <cstring>
#include <stdexcept>

namespace Mistwatch::RangerHutLining {
struct Result { Aevrynd::Mesh3D mesh; std::vector<RangerHutPart> parts; };
namespace Detail {
using namespace Aevrynd;
inline void Need(bool value,const char* text){if(!value)throw std::runtime_error(text);}
inline bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline bool Point(Vec3 a,Vec3 b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
inline bool Part(const RangerHutPart& a,const RangerHutPart& b){
    return a.kind==b.kind&&a.firstIndex==b.firstIndex&&a.indexCount==b.indexCount&&Point(a.centre,b.centre);
}
inline float Axis(Vec3 p,unsigned axis){return axis==0?p.x:p.z;}
inline void SetAxis(Vec3& p,unsigned axis,float value){if(axis==0)p.x=value;else p.z=value;}
inline float Axis(const Vertex3D& v,unsigned axis){return axis==0?v.x:v.z;}
inline void SetAxis(Vertex3D& v,unsigned axis,float value){if(axis==0)v.x=value;else v.z=value;}
// 固定五個來源append位置；不是Host種類、厚度或高度的泛用selector。
struct Slot {std::size_t solid,part,base,first;unsigned axis;float outward;Vec3 offset,extents;};
inline constexpr std::array<Slot,5> Slots{{
    {1,1,24,36,0,1,{2.4f,1.3f,0},{.1f,1.3f,3}},
    {3,26,2352,3240,2,-1,{0,.45f,-2.9f},{2.5f,.45f,.1f}},
    {4,36,3240,4464,2,-1,{0,2.4f,-2.9f},{2.5f,.2f,.1f}},
    {8,43,3624,5004,2,1,{0,.45f,2.9f},{2.5f,.45f,.1f}},
    {9,53,4512,6228,2,1,{0,2.4f,2.9f},{2.5f,.2f,.1f}}
}};
struct Placement {Vec3 oldCentre,newCentre,size;};
inline Placement SourcePlacement(const Slot& slot,const std::vector<BoxCollider3D>& solids){
    Need(solids.size()==13,"V1274 source solid membership drift");
    const auto& wall=solids[slot.solid];
    Need(Point(wall.center,Session::rangerLog+slot.offset)&&Point(wall.extents,slot.extents),
         "V1274 fixed source wall fields drift");
    const auto centre=wall.center-Session::rangerLog;const auto wallSize=wall.extents*2.f;
    auto old=centre,size=wallSize;
    SetAxis(old,slot.axis,Axis(centre,slot.axis)-slot.outward*.078f);
    SetAxis(size,slot.axis,.036f);
    // 來源12mm板縫也是固定退襯量；取現有板內面，不從觀測候選選深度。
    const float boardInner=Axis(centre,slot.axis)-slot.outward*((Axis(wallSize,slot.axis)-.06f)*.5f);
    auto next=old;SetAxis(next,slot.axis,boardInner+slot.outward*(.012f+.036f*.5f));
    return {old,next,size};
}
}

// 只接受已完成正式RoofBand的來源。全部驗證及寫入都在回傳物件內；不改輸入。
inline Result ApplyRecessedLinings(const Aevrynd::Mesh3D& source,const std::vector<RangerHutPart>& parts){
    using namespace Detail;
    static_assert(sizeof(Vertex3D)==28);
    std::vector<RangerHutPart> canonicalParts;const auto combined=CreateRangerHut({},&canonicalParts);
    const auto canonical=RangerRoofBand::ApplyFixedRoofBand(combined,canonicalParts);
    Need(source.Valid()&&source.vertices.size()==6864&&source.indices.size()==9600&&parts.size()==132&&
         source.vertices.size()==canonical.vertices.size()&&source.indices==canonical.indices&&parts.size()==canonicalParts.size(),
         "V1274 source membership/index drift");
    for(std::size_t p=0;p<parts.size();++p)Need(Part(parts[p],canonicalParts[p]),"V1274 source canonical part drift");
    for(std::size_t i=0;i<source.vertices.size();++i)
        Need(std::memcmp(&source.vertices[i],&canonical.vertices[i],28)==0,"V1274 source canonical record drift");
    Need(RangerRoofBand::Detail::UvDomain(source),"V1274 source finite/signed UV drift");
    const auto solids=RangerHutSolids();const auto box=Mesh3D::CreateUnitBox();
    Need(box.vertices.size()==24&&box.indices.size()==36,"V1274 source box drift");
    std::array<Placement,5> placements{};std::vector<int> owned(source.vertices.size(),-1);
    for(std::size_t s=0;s<Slots.size();++s){const auto& slot=Slots[s];const auto place=SourcePlacement(slot,solids);
        placements[s]=place;const auto& part=parts[slot.part];
        Need(part.kind==RangerHutPartKind::Host&&part.firstIndex==slot.first&&part.indexCount==36&&Point(part.centre,place.oldCentre),
             "V1274 source lining append identity drift");
        std::size_t matches{};for(const auto& p:parts)if(p.kind==RangerHutPartKind::Host&&Point(p.centre,place.oldCentre))++matches;
        Need(matches==1,"V1274 ambiguous source lining identity");
        const auto transform=CreateTransformMatrix(place.oldCentre,{},place.size),normal=CreateNormalMatrix(transform);
        for(std::size_t k=0;k<36;++k)
            Need(source.indices[slot.first+k]==slot.base+box.indices[k],"V1274 source closed box index drift");
        for(std::size_t k=0;k<24;++k){const auto i=slot.base+k;const auto& local=box.vertices[k];const auto& old=source.vertices[i];
            const auto point=TransformPoint(transform,{local.x,local.y,local.z});const auto n=UnpackNormalSnorm8(local.normal);
            Need(owned[i]==-1&&Point({old.x,old.y,old.z},point)&&
                 old.normal==PackNormalSnorm8(TransformNormal(normal,{n.x,n.y,n.z}))&&old.color==PackColorRgba8({.38f,.35f,.3f,1}),
                 "V1274 source lining shape/packed normal/color drift");
            owned[i]=static_cast<int>(s);
        }
    }
    // 已授權的120筆不可被其他部件共用，否則單軸變更會洩漏到非owned域。
    for(std::size_t p=0;p<parts.size();++p)for(std::size_t k=0;k<parts[p].indexCount;++k){
        const auto i=source.indices[parts[p].firstIndex+k];
        if(owned[i]>=0)Need(Slots[static_cast<std::size_t>(owned[i])].part==p,"V1274 shared protected alias drift");
    }
    Result result{source,parts};
    for(std::size_t s=0;s<Slots.size();++s){const auto& slot=Slots[s];const auto& place=placements[s];
        const auto transform=CreateTransformMatrix(place.newCentre,{},place.size);
        for(std::size_t k=0;k<24;++k){const auto& local=box.vertices[k];
            const auto point=TransformPoint(transform,{local.x,local.y,local.z});
            SetAxis(result.mesh.vertices[slot.base+k],slot.axis,Axis(point,slot.axis));
        }
        SetAxis(result.parts[slot.part].centre,slot.axis,Axis(place.newCentre,slot.axis));
    }
    Need(RangerRoofBand::Detail::UvDomain(result.mesh),"V1274 candidate finite/signed UV gate failed");
    auto inverse=result;
    for(std::size_t i=0;i<source.vertices.size();++i){
        if(owned[i]>=0){const auto& slot=Slots[static_cast<std::size_t>(owned[i])];SetAxis(inverse.mesh.vertices[i],slot.axis,Axis(source.vertices[i],slot.axis));}
        Need(std::memcmp(&inverse.mesh.vertices[i],&source.vertices[i],28)==0,"V1274 nonowned record/whole inverse failed");
    }
    for(const auto& slot:Slots)SetAxis(inverse.parts[slot.part].centre,slot.axis,Axis(parts[slot.part].centre,slot.axis));
    for(std::size_t p=0;p<parts.size();++p)Need(Part(inverse.parts[p],parts[p]),"V1274 nonowned part/whole inverse failed");
    Need(result.mesh.indices==source.indices,"V1274 index closure failed");
    const auto a=result.mesh.Bounds(),b=source.Bounds();
    Need(Point(a.center,b.center)&&Point(a.extents,b.extents)&&Bits(a.radius,b.radius),"V1274 whole bounds closure failed");
    result.mesh.InvalidateRaycastIndex();return result;
}
inline Aevrynd::Mesh3D CreateRangerHutLining(){
    std::vector<RangerHutPart> parts;(void)CreateRangerHut({},&parts);
    const auto source=RangerRoofBand::CreateRangerHutRoofBand();
    return ApplyRecessedLinings(source,parts).mesh;
}
}
