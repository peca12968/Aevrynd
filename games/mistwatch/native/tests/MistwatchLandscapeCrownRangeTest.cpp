#include "MistwatchLandscapeCrownRange.h"
#include "MistwatchLandscapeTwigDraw.h"
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <string_view>

namespace {
using namespace Aevrynd;
namespace R=Mistwatch::LandscapeCrownRange;
unsigned checks{},negatives{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
void CheckPolicy(R::Policy actual,std::string_view label,float distance,float width){
    Need(actual.label==label&&Bits(actual.switchDistance,distance)&&Bits(actual.transitionWidth,width),"exact policy values");
}
void Profiles(){
    for(const auto requested:std::array<std::wstring_view,10>{L"",L"original",L"extended-100m-v1",L"extended-100m-v2",
        L"extended-100m-v1 ",L" extended-100m-v1",L"EXTENDED-100M-V1",L"crown-distance-lod-v1",L"100",L"invalid"}){
        CheckPolicy(R::Resolve(false,requested),"original",60.f,16.f);
    }
    CheckPolicy(R::Resolve(true,L""),"original",60.f,16.f);
    CheckPolicy(R::Resolve(true,L"original"),"original",60.f,16.f);
    CheckPolicy(R::Resolve(true,L"extended-100m-v1"),"extended-100m-v1",100.f,24.f);
    for(const auto requested:std::array<std::wstring_view,8>{L"extended-100m-v2",L"extended-100m-v1 ",L" extended-100m-v1",
        L"EXTENDED-100M-V1",L"crown-distance-lod-v1",L"100",L"invalid",std::wstring_view(L"original\0suffix",15)}){
        bool rejected=false;
        try{(void)R::Resolve(true,requested);}catch(const std::invalid_argument& e){
            Need(std::string_view(e.what())=="LANDSCAPE_CROWN_RANGE_PROFILE","exact profile rejection");rejected=true;
        }
        Need(rejected,"invalid capture profile accepted");++negatives;
        CheckPolicy(R::Resolve(false,requested),"original",60.f,16.f);
    }
}
std::array<MeshLodLevel3D,2> Levels(R::Policy policy){
    MeshResource3D first{},second{};
    first.handle={10,101,102,120,15048};first.vertexCount=120;first.indexCount=15048;
    second.handle={11,201,202,80,3732};second.vertexCount=80;second.indexCount=3732;
    first.bounds={{0,0,0},std::sqrt(26.f),{3,4,1}};
    second.bounds={{1,0,0},std::sqrt(157.f),{2,3,12}};
    return {{{first,{},policy.switchDistance,policy.transitionWidth},{second,{}}}};
}
void Selection(const MeshLodChainSelection3D& s,bool transition,std::size_t primary,float farCoverage){
    Need(s.transition==transition&&s.primaryLevel==primary&&Bits(s.farCoverage,farCoverage),"exact LOD selection");
    Need(s.nearLevel==(!transition&&primary==1?1u:0u)&&s.farLevel==1,"LOD endpoint indices");
}
void Boundaries(R::Policy policy,float start,float midpoint,float end){
    const auto levels=Levels(policy);
    const auto local=Mistwatch::LandscapeTwigDraw::UnionBounds(levels[0].mesh,levels[1].mesh);
    Need(local.center.x==0&&local.center.y==0&&local.center.z==0&&local.extents.x==3&&local.extents.y==4&&
        local.extents.z==12&&local.radius==13,"conservative union exact independent bounds");
    // 明確使用聯集球面距離，不能偷偷改為中心距離或只用其中一個 LOD 的邊界。
    for(const auto metric:{MeshLodDistanceMetric3D::Center,MeshLodDistanceMetric3D::BoundsSurface}){
        const float radius=metric==MeshLodDistanceMetric3D::BoundsSurface?local.radius:0.f;
        const auto evaluate=[&](float distance){return EvaluateMeshLodChain3D(distance+radius,radius,levels,metric);};
        Selection(evaluate(0.f),false,0,0.f);
        Selection(evaluate(start-1.f),false,0,0.f);
        Selection(evaluate(start),false,0,0.f);
        Selection(evaluate((start+midpoint)*.5f),true,0,.15625f);
        Selection(evaluate(midpoint),true,1,.5f);
        Selection(evaluate((midpoint+end)*.5f),true,1,.84375f);
        Selection(evaluate(end),false,1,1.f);
        Selection(evaluate(end+1.f),false,1,1.f);
    }
    Selection(EvaluateMeshLodChain3D(5.f,local.radius,levels,MeshLodDistanceMetric3D::BoundsSurface),false,0,0.f);
    const Transform3D pose{{7,3,-9},{0,.7f,0},{2,2,2}};
    const auto world=TransformSphereBounds(local,pose);
    Need(world.radius==26.f&&world.center.x==7&&world.center.y==3&&world.center.z==-9,"union world scale and center");
    const auto matrix=pose.Matrix();
    for(const auto& level:levels)for(const float x:{-1.f,1.f})for(const float y:{-1.f,1.f})for(const float z:{-1.f,1.f}){
        const auto& b=level.mesh.bounds;
        const auto corner=b.center+Vec3{x*b.extents.x,y*b.extents.y,z*b.extents.z};
        Need(std::abs(corner.x-local.center.x)<=local.extents.x&&std::abs(corner.y-local.center.y)<=local.extents.y&&
            std::abs(corner.z-local.center.z)<=local.extents.z,"union contains both source AABBs");
        Need(Length(TransformPoint(matrix,corner)-world.center)<=world.radius+.00001f,"world sphere contains transformed corners");
    }
    Selection(EvaluateMeshLodChain3D(start+world.radius,world.radius,levels,MeshLodDistanceMetric3D::BoundsSurface),false,0,0.f);
    const auto wrongMetric=EvaluateMeshLodChain3D(start+world.radius,world.radius,levels,MeshLodDistanceMetric3D::Center);
    Need(wrongMetric.primaryLevel==1&&!wrongMetric.transition,"fixture detects accidental center metric");
}
void SameBounds(const MeshBounds3D& a,const MeshBounds3D& b){
    Need(Bits(a.center.x,b.center.x)&&Bits(a.center.y,b.center.y)&&Bits(a.center.z,b.center.z)&&Bits(a.radius,b.radius)&&
        Bits(a.extents.x,b.extents.x)&&Bits(a.extents.y,b.extents.y)&&Bits(a.extents.z,b.extents.z),"policy leaves union bit identical");
}
void ComparePolicies(){
    const auto original=Levels(R::Resolve(true,L"original"));
    const auto extended=Levels(R::Resolve(true,L"extended-100m-v1"));
    const auto a=Mistwatch::LandscapeTwigDraw::UnionBounds(original[0].mesh,original[1].mesh);
    const auto b=Mistwatch::LandscapeTwigDraw::UnionBounds(extended[0].mesh,extended[1].mesh);
    SameBounds(a,b);
    for(std::size_t i=0;i<2;++i){
        SameBounds(original[i].mesh.bounds,extended[i].mesh.bounds);
        const auto& x=original[i].mesh.handle;const auto& y=extended[i].mesh.handle;
        Need(x.value==y.value&&x.contentPrimary==y.contentPrimary&&x.contentSecondary==y.contentSecondary&&
            x.vertexCount==y.vertexCount&&x.indexCount==y.indexCount,"policy does not substitute mesh identities");
    }
    Selection(EvaluateMeshLodChain3D(80.f+a.radius,a.radius,original,MeshLodDistanceMetric3D::BoundsSurface),false,1,1.f);
    Selection(EvaluateMeshLodChain3D(80.f+b.radius,b.radius,extended,MeshLodDistanceMetric3D::BoundsSurface),false,0,0.f);
}
}
int main(){try{
    Profiles();Boundaries(R::Resolve(true,L"original"),52.f,60.f,68.f);
    Boundaries(R::Resolve(true,L"extended-100m-v1"),88.f,100.f,112.f);ComparePolicies();
    std::cout<<"LANDSCAPE_CROWN_RANGE_CPU_PASS checks="<<checks<<" negatives="<<negatives
        <<" original_switch=60 original_width=16 extended_switch=100 extended_width=24 native=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"LANDSCAPE_CROWN_RANGE_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}}
