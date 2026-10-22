#pragma once
#include "MistwatchWorkshopRoof.h"
#include <array>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

namespace Mistwatch {
namespace WorkshopRoofCameraDetail {
inline constexpr double MaximumSlabWidth=.10;
inline constexpr std::size_t MaximumBoxes=2048;
inline constexpr std::uint64_t VisualFingerprint=358593194902208623ull;
struct Point {double x{},y{},z{};};
inline Point Vertex(const Aevrynd::Vertex3D& v){return{v.x,v.y,v.z};}
// 三角形經兩個平行半空間裁切最多五個頂點；固定容量另留退化邊界餘裕。
inline std::size_t Clip(std::array<Point,12>& polygon,std::size_t count,double z,bool lower){
    if(!count)return 0;
    std::array<Point,12> next{};std::size_t size{};
    auto previous=polygon[count-1];bool previousInside=lower?previous.z>=z:previous.z<=z;
    for(std::size_t i=0;i<count;++i){const auto current=polygon[i];const bool inside=lower?current.z>=z:current.z<=z;
        if(previousInside!=inside){const double amount=(z-previous.z)/(current.z-previous.z);
            if(size==next.size())return 0;
            next[size++]={previous.x+(current.x-previous.x)*amount,previous.y+(current.y-previous.y)*amount,z};
        }
        if(inside){if(size==next.size())return 0;next[size++]=current;}
        previous=current;previousInside=inside;
    }
    polygon=next;return size;
}
inline bool AxisBounds(double low,double high,float& center,float& extent){
    const float negative=-std::numeric_limits<float>::infinity(),positive=std::numeric_limits<float>::infinity();
    // 先把實際極值往外取一個float，再以已捨入的中心求足夠半徑。
    const float lo=std::nextafter(float(low),negative),hi=std::nextafter(float(high),positive);
    center=float((double(lo)+double(hi))*.5);
    extent=std::nextafter(float(std::max(double(center)-double(lo),double(hi)-double(center))),positive);
    return std::isfinite(center)&&std::isfinite(extent)&&extent>0&&
        double(center)-double(extent)<=low&&double(center)+double(extent)>=high&&
        double(center-extent)<=low&&double(center+extent)>=high;
}
inline WorkshopRoof::PartKind Kind(std::size_t i){
    if(i<40)return WorkshopRoof::PartKind::SlopeBoard;
    if(i<42)return WorkshopRoof::PartKind::Eave;
    if(i==42)return WorkshopRoof::PartKind::Ridge;
    if(i<47)return WorkshopRoof::PartKind::Rafter;
    return WorkshopRoof::PartKind::GableBoard;
}
}

// 僅相機的保守遮擋／避障代理，禁止加入角色 WorldSolids 或作為行走支撐。
// 每個構件按Z分成不超過10cm的裁切區段；不用整根斜板的大盒填滿坡面上空。
// 輸出依原Part順序再依Z遞增；失敗保留output，成功才一次交換。
inline bool BuildWorkshopRoofCamera(const Aevrynd::Mesh3D& mesh,std::span<const WorkshopRoof::Part> parts,
                                    std::vector<Aevrynd::BoxCollider3D>& output)noexcept try{
    using namespace WorkshopRoofCameraDetail;
    if(parts.size()!=75||mesh.vertices.size()!=17664||mesh.indices.size()!=24372||!mesh.Valid()||
       WorkshopFrame::Fingerprint(mesh)!=VisualFingerprint)return false;
    std::size_t nextVertex=12480,nextIndex=17160;
    for(std::size_t i=0;i<parts.size();++i){const auto& p=parts[i];
        if(p.kind!=Kind(i)||p.firstVertex!=nextVertex||p.firstIndex!=nextIndex||p.vertexCount!=(i<47?96u:24u)||p.indexCount!=(i<47?132u:36u))return false;
        if(p.firstVertex>mesh.vertices.size()||p.vertexCount>mesh.vertices.size()-p.firstVertex||
           p.firstIndex>mesh.indices.size()||p.indexCount>mesh.indices.size()-p.firstIndex)return false;
        for(std::size_t j=p.firstIndex;j<p.firstIndex+p.indexCount;++j)
            if(mesh.indices[j]<p.firstVertex||mesh.indices[j]>=p.firstVertex+p.vertexCount)return false;
        nextVertex+=p.vertexCount;nextIndex+=p.indexCount;
    }
    if(nextVertex!=mesh.vertices.size()||nextIndex!=mesh.indices.size())return false;
    std::vector<Aevrynd::BoxCollider3D> candidate;candidate.reserve(MaximumBoxes);
    for(const auto& p:parts){
        double lowZ=std::numeric_limits<double>::infinity(),highZ=-lowZ;
        for(std::size_t i=p.firstVertex;i<p.firstVertex+p.vertexCount;++i){const auto v=Vertex(mesh.vertices[i]);
            if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))return false;
            lowZ=std::min(lowZ,v.z);highZ=std::max(highZ,v.z);
        }
        const double rawCount=std::max(1.,std::ceil((highZ-lowZ)/MaximumSlabWidth));
        if(!std::isfinite(rawCount)||rawCount>double(MaximumBoxes-candidate.size()))return false;
        const auto slices=static_cast<std::size_t>(rawCount);
        for(std::size_t slice=0;slice<slices;++slice){
            const double lower=lowZ+(highZ-lowZ)*double(slice)/double(slices);
            const double upper=slice+1==slices?highZ:lowZ+(highZ-lowZ)*double(slice+1)/double(slices);
            const double infinity=std::numeric_limits<double>::infinity();Point low{infinity,infinity,infinity},high{-infinity,-infinity,-infinity};
            bool any=false;
            for(std::size_t i=p.firstIndex;i<p.firstIndex+p.indexCount;i+=3){
                std::array<Point,12> polygon{};for(std::size_t j=0;j<3;++j)polygon[j]=Vertex(mesh.vertices[mesh.indices[i+j]]);
                auto count=Clip(polygon,3,lower,true);count=Clip(polygon,count,upper,false);
                for(std::size_t j=0;j<count;++j){const auto v=polygon[j];any=true;
                    low={std::min(low.x,v.x),std::min(low.y,v.y),std::min(low.z,v.z)};
                    high={std::max(high.x,v.x),std::max(high.y,v.y),std::max(high.z,v.z)};
                }
            }
            if(!any)return false;
            Aevrynd::BoxCollider3D box;
            if(!AxisBounds(low.x,high.x,box.center.x,box.extents.x)||!AxisBounds(low.y,high.y,box.center.y,box.extents.y)||
               !AxisBounds(low.z,high.z,box.center.z,box.extents.z))return false;
            candidate.push_back(box);
        }
    }
    output.swap(candidate);return true;
}catch(...){return false;}
}
