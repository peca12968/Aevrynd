#pragma once
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/TerrainLayerCoverage3D.h>
#include <bit>
#include <cmath>
#include <span>
#include <stdexcept>
#include <string_view>

namespace Mistwatch::GroveFloor {
struct Receipt {std::size_t referenced{},protectedVertices{},changed{};float maximumIncrease{};};
struct Result {Aevrynd::Mesh3D mesh;Receipt receipt;};
inline bool Resolve(bool forest,bool capture,bool replay,std::wstring_view token){
    if(capture&&replay)throw std::invalid_argument("GROVE_FLOOR_MODE_CONFLICT");
    if(token.empty())return forest;
    if(!forest||capture==replay||(token!=L"original"&&token!=L"rooted-grove-litter-v1"))
        throw std::invalid_argument("GROVE_FLOOR_CONTEXT");
    return token==L"rooted-grove-litter-v1";
}
namespace Detail {
inline void Need(bool value,const char* message){if(!value)throw std::invalid_argument(message);}
inline float Smooth(float value){const float t=std::clamp(value,0.f,1.f);return t*t*(3.f-2.f*t);}
inline bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline float Noise(float x,float z){
    const float gx=x/1.8f,gz=z/1.8f;const int ix=int(std::floor(gx)),iz=int(std::floor(gz));
    const auto sample=[](int a,int b){std::uint32_t word=std::uint32_t(a)*0x9e3779b9u^std::uint32_t(b)*0x85ebca6bu^0x47524f56u;
        word^=word>>16;word*=0x7feb352du;word^=word>>15;return float(word>>8)/16777216.f;};
    const float tx=Smooth(gx-float(ix)),tz=Smooth(gz-float(iz));
    return std::lerp(std::lerp(sample(ix,iz),sample(ix+1,iz),tx),std::lerp(sample(ix,iz+1),sample(ix+1,iz+1),tx),tz);
}
inline float Coverage(float x,float z,std::span<const Aevrynd::Vec3> roots){
    float result{};
    for(std::size_t id=0;id<roots.size();++id){
        const float dx=x-roots[id].x,dz=z-roots[id].z;
        // 小群落留出苔地間隙；多尺度邊界不以同心圓或額外貼面掩蓋既有地表。
        const float radius=4.7f*(.82f+.30f*Noise(x+float(id)*7.f,z));
        const float distance=std::hypot(dx/1.12f,dz/.94f);
        result=(std::max)(result,.92f*Smooth((radius-distance)/1.7f));
    }
    return result;
}
}
// 只在真正森林引用且未與道路／碎石共用的頂點提高既有落葉層；原位置、法線、岩層及拓撲逐項封存。
inline Result Apply(const Aevrynd::Mesh3D& source,std::span<const Aevrynd::Vec3> roots,
    std::span<const std::uint32_t> road,std::span<const std::uint32_t> gravel){
    using namespace Aevrynd;using Detail::Need;
    Need(source.Valid()&&source.vertices.size()<=100000&&source.indices.size()<=600000&&roots.size()==8,"GROVE_FLOOR_INPUT_SCOPE");
    for(const auto root:roots)Need(std::isfinite(root.x)&&std::isfinite(root.y)&&std::isfinite(root.z)&&
        std::abs(root.x)<50.f&&root.z>=40.f&&root.z<100.f,"GROVE_FLOOR_ACTUAL_ROOT");
    std::vector<std::uint8_t> membership(source.vertices.size());
    for(const auto index:source.indices)membership[index]|=1u;
    for(const auto refs:{road,gravel})for(const auto index:refs){Need(index<membership.size(),"GROVE_FLOOR_PROTECTED_INDEX");membership[index]|=2u;}
    Result result{source,{}};
    for(std::size_t id=0;id<source.vertices.size();++id){const auto& a=source.vertices[id];auto& b=result.mesh.vertices[id];Vec2 encoded{};
        Need(std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)&&TryEncodeTerrainLayerCoverage3D(a.u,a.v,encoded),"GROVE_FLOOR_SOURCE_VERTEX");
        if(!(membership[id]&1u))continue;++result.receipt.referenced;
        if((membership[id]&2u)||(a.z>32.f&&a.z<40.f)||std::abs(a.x)>=50.f||a.z<=-20.f||a.z>=100.f){++result.receipt.protectedVertices;continue;}
        const float leaf=(std::max)(a.u,Detail::Coverage(a.x,a.z,roots)*(1.f-a.v));
        Need(TryEncodeTerrainLayerCoverage3D(leaf,a.v,encoded),"GROVE_FLOOR_NORMALIZED_LAYERS");b.u=leaf;
        if(!Detail::Bits(a.u,b.u)){++result.receipt.changed;result.receipt.maximumIncrease=(std::max)(result.receipt.maximumIncrease,b.u-a.u);}
        Need(Detail::Bits(a.x,b.x)&&Detail::Bits(a.y,b.y)&&Detail::Bits(a.z,b.z)&&a.normal==b.normal&&a.color==b.color&&Detail::Bits(a.v,b.v),"GROVE_FLOOR_NON_LEAF_MUTATION");
    }
    Need(result.mesh.indices==source.indices,"GROVE_FLOOR_TOPOLOGY_MUTATION");
    return result;
}
}
