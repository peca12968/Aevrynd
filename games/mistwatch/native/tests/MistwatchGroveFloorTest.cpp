#include "MistwatchGroveFloor.h"
#include <cstring>
#include <iostream>
#include <limits>
using namespace Aevrynd;
namespace G=Mistwatch::GroveFloor;
namespace {unsigned checks{};
void Need(bool value){++checks;if(!value)throw std::runtime_error("GROVE_FLOOR_ASSERT");}
template<class F>void Reject(F&& fn){bool rejected{};try{fn();}catch(const std::invalid_argument&){rejected=true;}Need(rejected);}
}
int main(){try{
    Need(G::Resolve(true,false,false,L""));Need(!G::Resolve(false,false,false,L""));
    Need(!G::Resolve(true,false,true,L"original"));Need(G::Resolve(true,false,true,L"rooted-grove-litter-v1"));
    Reject([]{G::Resolve(true,true,true,L"");});Reject([]{G::Resolve(true,false,false,L"original");});
    Reject([]{G::Resolve(false,false,true,L"rooted-grove-litter-v1");});Reject([]{G::Resolve(true,true,false,L"unknown");});
    Mesh3D source;
    for(unsigned z=0;z<17;++z)for(unsigned x=0;x<17;++x)source.vertices.push_back({-22.f+float(x)*.5f,0,46.f+float(z)*.5f,~0u,0x778899ffu,.2f,.15f});
    for(unsigned z=0;z<16;++z)for(unsigned x=0;x<16;++x){const auto a=z*17+x;source.indices.insert(source.indices.end(),{a,a+17,a+1,a+1,a+17,a+18});}
    source.vertices.push_back({0,0,0,~0u,~0u,.2f,.15f});
    const auto original=source;const std::array<Vec3,8> roots{{{-18,0,50},{-28,0,82.5f},{-40.5f,0,77.5f},{-43,0,85},{20.5f,0,62.5f},{35.5f,0,62.5f},{23,0,70},{38,0,75}}};
    const std::array<std::uint32_t,1> road{144},gravel{145};
    const auto a=G::Apply(source,roots,road,gravel),b=G::Apply(source,roots,road,gravel);
    Need(a.receipt.changed>100&&a.receipt.referenced==289&&a.receipt.protectedVertices==2&&a.receipt.maximumIncrease>0);
    Need(a.mesh.indices==original.indices&&source.indices==original.indices);
    Need(std::memcmp(source.vertices.data(),original.vertices.data(),source.vertices.size()*sizeof(Vertex3D))==0);
    Need(std::memcmp(a.mesh.vertices.data(),b.mesh.vertices.data(),source.vertices.size()*sizeof(Vertex3D))==0);
    for(std::size_t id=0;id<source.vertices.size();++id){const auto& before=source.vertices[id];const auto& after=a.mesh.vertices[id];
        Need(G::Detail::Bits(before.x,after.x)&&G::Detail::Bits(before.y,after.y)&&G::Detail::Bits(before.z,after.z)&&before.normal==after.normal&&before.color==after.color&&G::Detail::Bits(before.v,after.v));
        Vec2 encoded{};Need(after.u>=before.u&&TryEncodeTerrainLayerCoverage3D(after.u,after.v,encoded));
        if(id==144||id==145||id==289)Need(std::memcmp(&before,&after,sizeof(Vertex3D))==0);
    }
    Reject([&]{G::Apply(source,std::span<const Vec3>(roots.data(),7),road,gravel);});
    auto invalidRoots=roots;invalidRoots[0].x=std::numeric_limits<float>::quiet_NaN();Reject([&]{G::Apply(source,invalidRoots,road,gravel);});
    const std::array<std::uint32_t,1> badIndex{100000};Reject([&]{G::Apply(source,roots,badIndex,gravel);});
    auto invalid=source;invalid.vertices[0].u=1;Reject([&]{G::Apply(invalid,roots,road,gravel);});
    invalid=source;invalid.indices[0]=100000;Reject([&]{G::Apply(invalid,roots,road,gravel);});
    std::cout<<"GROVE_FLOOR_POLICY_PASS checks="<<checks<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 20;}}
