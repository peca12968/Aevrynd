// 僅供負向回歸：封存 m2926 全構件 UV 錯誤算法，禁止作為正式渲染來源。
// 原算法來源 SHA256：E21C37E752E22024E5783C36B15D35743C4A8B5773775B84123DB1100F1BF73F。
// 下方算法原文保留；獨立世界距離測試必須拒絕其薄側面、端面與取材帶縮放錯誤。
#pragma once
#include "MistwatchWorkshopRoof.h"
#include <span>

namespace Mistwatch::WorkshopWoodUv {
using namespace Aevrynd;
inline constexpr const char* Profile="workshop-metric-wood-uv-candidate-v1";
inline constexpr float LongitudinalMeters=3.2f;
inline constexpr std::size_t MemberCount=205;
struct Range {std::size_t firstVertex{},vertexCount{},firstIndex{},indexCount{};float width{};};
struct Mapping {unsigned band{};float low{},high{},phase{};};
struct Result {Mesh3D mesh;std::vector<WorkshopRoof::Part> roofParts;std::array<Range,MemberCount> ranges;std::array<Mapping,MemberCount> mappings;};
inline bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline bool SameGeometry(const Mesh3D& a,const Mesh3D& b){
    if(a.vertices.size()!=b.vertices.size()||a.indices!=b.indices)return false;
    for(std::size_t i=0;i<a.vertices.size();++i){const auto& x=a.vertices[i];const auto& y=b.vertices[i];
        if(!Bits(x.x,y.x)||!Bits(x.y,y.y)||!Bits(x.z,y.z)||x.normal!=y.normal||x.color!=y.color)return false;}
    return true;
}
inline bool SameMesh(const Mesh3D& a,const Mesh3D& b){
    if(!SameGeometry(a,b))return false;
    for(std::size_t i=0;i<a.vertices.size();++i)if(!Bits(a.vertices[i].u,b.vertices[i].u)||!Bits(a.vertices[i].v,b.vertices[i].v))return false;
    return true;
}
inline bool SameParts(std::span<const WorkshopRoof::Part> a,std::span<const WorkshopRoof::Part> b){
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)if(a[i].kind!=b[i].kind||a[i].firstVertex!=b[i].firstVertex||a[i].vertexCount!=b[i].vertexCount||a[i].firstIndex!=b[i].firstIndex||a[i].indexCount!=b[i].indexCount)return false;
    return true;
}
inline std::array<Range,MemberCount> Ranges(){
    std::array<Range,MemberCount> out{};std::size_t n{},v{},i{};
    const auto add=[&](std::size_t vc,std::size_t ic,float width){out[n++]={v,vc,i,ic,width};v+=vc;i+=ic;};
    for(unsigned part=0;part<4;++part)for(unsigned board=0;board<WorkshopVisual::BoardCounts[part];++board)
        add(96,132,(part==1?4.f:5.f)/float(WorkshopVisual::BoardCounts[part])-WorkshopVisual::Seam);
    for(unsigned frame=0;frame<54;++frame)add(96,132,frame%9>=7?.14f:.18f);
    for(unsigned member=0;member<47;++member)add(96,132,member<40?.242f:member<42?.16f:member==42?.20f:.12f);
    for(unsigned board=0;board<28;++board)add(24,36,3.8f/14.f-.004f);
    if(n!=MemberCount||v!=17664||i!=24372)throw std::runtime_error("WOOD_UV_RANGE_MEMBERSHIP");return out;
}
inline std::uint32_t Mix(std::uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline Result Create(const Mesh3D& source,std::span<const WorkshopRoof::Part> parts){
    // 原作者工廠與既有指紋完全不變；候選不接受只有相同點數的替代來源。
    std::vector<WorkshopRoof::Part> canonicalParts;const auto canonical=WorkshopRoof::Create(&canonicalParts);
    if(!SameMesh(source,canonical)||!SameParts(parts,canonicalParts))throw std::invalid_argument("WOOD_UV_NONCANONICAL_SOURCE");
    Result out{source,canonicalParts,Ranges(),{}};unsigned previous=4;
    for(std::size_t member=0;member<MemberCount;++member){const auto& range=out.ranges[member];
        const auto hash=Mix(static_cast<std::uint32_t>(member)+0x574f4f44u);unsigned band=hash%4;
        if(band==previous)band=(band+1+(hash>>8)%3)%4;previous=band;
        const auto bounds=WorkshopVisual::Bands[band];constexpr float guard=2.f/1024.f;
        const float span=(std::min)(bounds.y-bounds.x-2.f*guard,range.width/LongitudinalMeters);
        const float centre=(bounds.x+bounds.y)*.5f;const float phase=float((hash>>12)&4095u)/4096.f;
        out.mappings[member]={band,centre-span*.5f,centre+span*.5f,phase};
        float low=100.f,high=-100.f,minV=100.f;
        for(std::size_t j=range.firstVertex;j<range.firstVertex+range.vertexCount;++j){const auto& v=source.vertices[j];low=(std::min)(low,v.u);high=(std::max)(high,v.u);minV=(std::min)(minV,v.v);}
        if(!(high>low))throw std::runtime_error("WOOD_UV_SOURCE_WIDTH");
        // 正向仿射變換保留每個面的紋理方向、倒角連續性與 UV 面積符號；不鏡射法線。
        for(std::size_t j=range.firstVertex;j<range.firstVertex+range.vertexCount;++j){auto& v=out.mesh.vertices[j];
            v.u=(centre-span*.5f+(v.u-low)/(high-low)*span)/WorkshopVisual::StaticDecodedScale;
            v.v=(v.v-minV)*(WorkshopVisual::LongitudinalMeters/LongitudinalMeters)+phase/WorkshopVisual::StaticDecodedScale;
        }
    }
    if(!SameGeometry(source,out.mesh)||!out.mesh.Valid())throw std::runtime_error("WOOD_UV_GEOMETRY_DRIFT");
    for(std::size_t i=0;i<out.mesh.indices.size();i+=3){const auto area=[&](const Mesh3D& mesh){const auto& a=mesh.vertices[mesh.indices[i]];const auto& b=mesh.vertices[mesh.indices[i+1]];const auto& c=mesh.vertices[mesh.indices[i+2]];return (double(b.u)-a.u)*(double(c.v)-a.v)-(double(c.u)-a.u)*(double(b.v)-a.v);};
        const auto old=area(source),now=area(out.mesh);if(!std::isfinite(now)||now==0||old*now<=0)throw std::runtime_error("WOOD_UV_ORIENTATION");}
    out.mesh.InvalidateRaycastIndex();return out;
}
inline bool Validate(const Mesh3D& source,std::span<const WorkshopRoof::Part> parts,const Result& candidate)noexcept try{
    const auto expected=Create(source,parts);if(!SameMesh(expected.mesh,candidate.mesh)||!SameParts(expected.roofParts,candidate.roofParts))return false;
    for(std::size_t i=0;i<MemberCount;++i){const auto& a=expected.ranges[i];const auto& b=candidate.ranges[i];const auto& x=expected.mappings[i];const auto& y=candidate.mappings[i];
        if(a.firstVertex!=b.firstVertex||a.vertexCount!=b.vertexCount||a.firstIndex!=b.firstIndex||a.indexCount!=b.indexCount||!Bits(a.width,b.width)||x.band!=y.band||!Bits(x.low,y.low)||!Bits(x.high,y.high)||!Bits(x.phase,y.phase))return false;}
    return true;
}catch(...){return false;}
}
