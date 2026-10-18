#pragma once
#include "MistwatchWorkshopRoof.h"
#include <span>

namespace Mistwatch::WorkshopWoodMetric {
using namespace Aevrynd;
inline constexpr const char* Profile="workshop-face-metric-wood-candidate-v2";
// 同一材質的 U/V 均為 5.2 公尺一週期；包含原厚屋頂的端面，不只考慮山牆寬。
// 較窄取材帶容不下時拒絕，不縮放／裁切冒充米制。
inline constexpr double MetersPerTile=5.2;
struct D {double x{},y{},z{};D operator+(D b)const{return{x+b.x,y+b.y,z+b.z};}D operator-(D b)const{return{x-b.x,y-b.y,z-b.z};}D operator*(double k)const{return{x*k,y*k,z*k};}};
inline double DotD(D a,D b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline D Point(const Vertex3D& v){return{v.x,v.y,v.z};}
inline D Unit(D v){const auto length=std::sqrt(DotD(v,v));if(!std::isfinite(length)||length<1.e-12)throw std::runtime_error("METRIC_INVALID_AXIS");return v*(1/length);}
struct Face {std::size_t member{},firstVertex{},vertexCount{},firstIndex{},indexCount{};unsigned band{};double width{},phase{};};
struct Result {Mesh3D mesh;std::vector<WorkshopRoof::Part> roofParts;std::vector<Face> faces;};
inline bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline bool SameGeometry(const Mesh3D& a,const Mesh3D& b){
    if(a.vertices.size()!=b.vertices.size()||a.indices!=b.indices)return false;
    for(std::size_t i=0;i<a.vertices.size();++i){const auto& x=a.vertices[i];const auto& y=b.vertices[i];if(!Bits(x.x,y.x)||!Bits(x.y,y.y)||!Bits(x.z,y.z)||x.normal!=y.normal||x.color!=y.color)return false;}return true;
}
inline bool SameMesh(const Mesh3D& a,const Mesh3D& b){if(!SameGeometry(a,b))return false;for(std::size_t i=0;i<a.vertices.size();++i)if(!Bits(a.vertices[i].u,b.vertices[i].u)||!Bits(a.vertices[i].v,b.vertices[i].v))return false;return true;}
inline bool SameParts(std::span<const WorkshopRoof::Part> a,std::span<const WorkshopRoof::Part> b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(a[i].kind!=b[i].kind||a[i].firstVertex!=b[i].firstVertex||a[i].vertexCount!=b[i].vertexCount||a[i].firstIndex!=b[i].firstIndex||a[i].indexCount!=b[i].indexCount)return false;return true;}
inline std::uint32_t Mix(std::uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline Result Create(const Mesh3D& source,std::span<const WorkshopRoof::Part> parts,double meters=MetersPerTile){
    if(!std::isfinite(meters)||meters<=0||meters>10)throw std::invalid_argument("METRIC_INVALID_PERIOD");
    std::vector<WorkshopRoof::Part> canonicalParts;const auto canonical=WorkshopRoof::Create(&canonicalParts);
    if(!SameMesh(source,canonical)||!SameParts(parts,canonicalParts))throw std::invalid_argument("METRIC_NONCANONICAL_SOURCE");
    Result out{source,canonicalParts,{}};out.faces.reserve(4770);std::size_t vertex{},index{};unsigned previous=4;
    for(std::size_t member=0;member<205;++member){const auto hash=Mix(static_cast<std::uint32_t>(member)+0x574f4f44u);unsigned band=hash%4;if(band==previous)band=(band+1+(hash>>8)%3)%4;previous=band;
        const auto strip=WorkshopVisual::Bands[band];const double phase=double((hash>>12)&4095u)/4096.;
        const auto memberFirst=vertex;const std::size_t memberVertices=member<177?96:24;D origin{};
        for(std::size_t j=0;j<memberVertices;++j)origin=origin+Point(source.vertices[memberFirst+j]);origin=origin*(1./double(memberVertices));
        const unsigned count=member<177?26:6;
        for(unsigned f=0;f<count;++f){const std::size_t vc=member<177&&f>=18?3:4,ic=vc==3?3:6;
            const auto& a=source.vertices[source.indices[index]];const auto& b=source.vertices[source.indices[index+1]];const auto& c=source.vertices[source.indices[index+2]];
            const auto e1=Point(b)-Point(a),e2=Point(c)-Point(a);const double du1=double(b.u)-a.u,du2=double(c.u)-a.u,dv1=double(b.v)-a.v,dv2=double(c.v)-a.v,det=du1*dv2-du2*dv1;
            if(!std::isfinite(det)||det==0)throw std::runtime_error("METRIC_SOURCE_UV_DEGENERATE");
            // 由實際三角面取原木紋 V 方向，再建立真正正交的面內 U；不繼承舊投影長度／端面假 .05m。
            const auto oldU=(e1*dv2-e2*dv1)*(1/det),oldV=(e2*du1-e1*du2)*(1/det);const auto vAxis=Unit(oldV),uAxis=Unit(oldU-vAxis*DotD(oldU,vAxis));
            double lo=1.e9,hi=-1.e9;
            for(std::size_t j=vertex;j<vertex+vc;++j){const double across=DotD(Point(source.vertices[j])-origin,uAxis);lo=(std::min)(lo,across);hi=(std::max)(hi,across);}
            const double width=hi-lo,guard=2./1024.;
            if(!(width>0)||width/meters>double(strip.y)-strip.x-2*guard)throw std::runtime_error("METRIC_SAFE_BAND_INCOMPATIBLE member="+std::to_string(member)+" face="+std::to_string(f)+" width="+std::to_string(width));
            for(std::size_t j=vertex;j<vertex+vc;++j){const auto p=Point(source.vertices[j])-origin;auto& v=out.mesh.vertices[j];
                v.u=float(((double(strip.x)+strip.y)*.5+(DotD(p,uAxis)-(lo+hi)*.5)/meters)/double(WorkshopVisual::StaticDecodedScale));
                v.v=float((phase+DotD(p,vAxis)/meters)/double(WorkshopVisual::StaticDecodedScale));}
            out.faces.push_back({member,vertex,vc,index,ic,band,width,phase});vertex+=vc;index+=ic;
        }
    }
    if(vertex!=17664||index!=24372||out.faces.size()!=4770||!SameGeometry(source,out.mesh))throw std::runtime_error("METRIC_MEMBERSHIP");
    out.mesh.InvalidateRaycastIndex();return out;
}
inline bool Validate(const Mesh3D& source,std::span<const WorkshopRoof::Part> parts,const Result& result)noexcept try{
    const auto expected=Create(source,parts);if(!SameMesh(expected.mesh,result.mesh)||!SameParts(expected.roofParts,result.roofParts)||expected.faces.size()!=result.faces.size())return false;
    for(std::size_t i=0;i<expected.faces.size();++i){const auto& a=expected.faces[i];const auto& b=result.faces[i];if(a.member!=b.member||a.firstVertex!=b.firstVertex||a.vertexCount!=b.vertexCount||a.firstIndex!=b.firstIndex||a.indexCount!=b.indexCount||a.band!=b.band||a.width!=b.width||a.phase!=b.phase)return false;}return true;
}catch(...){return false;}
}
