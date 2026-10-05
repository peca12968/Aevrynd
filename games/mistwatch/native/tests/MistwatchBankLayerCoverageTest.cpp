#include "MistwatchWorld.h"
#include <iostream>
#include <stdexcept>
namespace {
float Smooth(float value){value=std::clamp(value,0.f,1.f);return value*value*(3.f-2.f*value);}
// 固定世界域與遮罩獨立重建，不以受測權重函式決定允許區域。
float ExpectedCoverage(float x,float z){
    const float outer=Smooth((std::abs(x)-4.f)/12.f);
    const float center=36.f+3.5f*std::sin(x*.095f)*outer;
    const bool north=z>center;
    const float mask=Smooth((std::abs(x)-16.f)/8.f);if(mask<=0.f)return 0.f;
    const float width=4.f+2.f*outer+4.f*Smooth((std::abs(x)-20.f)/12.f);
    const float distance=std::abs(z-center);if(distance<=width*.5f)return 0.f;
    const float dryDistance=distance-width;
    const float erosion=Smooth(1.f-std::abs(x-(north?37.f:-37.f))/13.f);
    const float bank=north?13.f+2.f*std::cos(x*.08f):13.f+2.f*std::sin(x*.07f);
    const float t=dryDistance/((4.f+(bank-4.f)*Smooth((std::abs(x)-12.f)/12.f))*(1.f+.5f*erosion));
    const float signal=.5f+.5f*std::sin(x*.61f+z*.49f)*std::cos(x*.31f-z*.27f);
    return .95f*mask*Smooth((distance/width-.5f)/.08f)*(1.f-Smooth((t-.45f)/.55f))*(.65f+.35f*Smooth((signal-.30f)/.45f));
}
Aevrynd::Mesh3D ReferenceTerrainMesh(const Aevrynd::HeightField3D& terrain){
    using namespace Aevrynd;using namespace Mistwatch;auto mesh=CreateGroundMesh(terrain);
    for(auto& vertex:mesh.vertices){
        if(vertex.z< -10.f||vertex.z>96.f||(vertex.z>32.f&&vertex.z<40.f))continue;
        const float center=.35f*std::sin(vertex.z*.19f),width=2.8f+.3f*std::sin(vertex.z*.37f);
        float w=std::clamp(1.f-std::abs(vertex.x-center)/width,0.f,1.f);
        w=(std::max)(w,std::clamp(1.f-SettlementTrailDistance(vertex.x,vertex.z)/1.15f,0.f,1.f));
        w=w*w*(3.f-2.f*w);const auto original=UnpackColorRgba8(vertex.color);
        vertex.color=PackColorRgba8({original.r+(.46f-original.r)*w,original.g+(.34f-original.g)*w,original.b+(.22f-original.b)*w,original.a});
    }return mesh;
}
}
int main(){try{
    using namespace Mistwatch;
    const auto terrain=CreateTerrain();const auto before=ReferenceTerrainMesh(terrain),after=CreateTerrainMesh(terrain),repeat=CreateTerrainMesh(terrain);
    if(before.indices!=after.indices||after.indices!=repeat.indices||before.vertices.size()!=after.vertices.size())throw std::runtime_error("Bank coverage topology drift");
    unsigned covered{},neutral{};float peak{};
    for(std::size_t i=0;i<after.vertices.size();++i){const auto& a=before.vertices[i];const auto& b=after.vertices[i];const auto& r=repeat.vertices[i];
        if(a.x!=b.x||a.y!=b.y||a.z!=b.z||a.normal!=b.normal||a.color!=b.color)throw std::runtime_error("Coverage altered geometry/normal/RGB/height alpha");
        const float expected=ExpectedCoverage(b.x,b.z);
        if(!std::isfinite(b.u)||!std::isfinite(b.v)||std::abs(b.u-.15f*expected)>.000001f||std::abs(b.v-.85f*expected)>.000001f||b.u<0.f||b.v<0.f||b.u+b.v>.950001f)throw std::runtime_error("Coverage differs from independent world mask/material mix");
        if(b.u!=r.u||b.v!=r.v||b.color!=r.color)throw std::runtime_error("Coverage repeat drift");
        peak=(std::max)(peak,b.u+b.v);
        if(expected==0.f){if(b.u!=0.f||b.v!=0.f)throw std::runtime_error("Neutral domain coverage leakage");++neutral;}
        else {if(b.y<=RiverHeight)throw std::runtime_error("Coverage vertex is submerged");++covered;}
    }
    for(const auto& vertex:CreateTerrainMesh(terrain,false).vertices)if(vertex.u!=0.f||vertex.v!=0.f)throw std::runtime_error("Legacy Surface received coverage UV");
    if(covered<5000||neutral<30000||peak<.9f)throw std::runtime_error("Exposed outer bank coverage absent/global spill");
    std::cout<<"BANK_LAYER_COVERAGE_PASS; covered="<<covered<<" neutral="<<neutral<<" peak="<<peak
        <<"; mix=0.15 Ground108 + 0.85 rock_surface; geometry/normals/all RGB/height-alpha unchanged; shader material effect verified separately\n";return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<'\n';return 1;}}
