#include "MistwatchBankCoverage.h"
#include "MistwatchLandscape.h"
#include "MistwatchWorldProfile.h"
#include <Aevrynd/TerrainLayerCoverage3D.h>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
std::size_t checks{};
void Require(bool condition,const char* label){++checks;if(!condition)throw std::runtime_error(label);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Aevrynd::Vec3 a,Aevrynd::Vec3 b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(Mistwatch::ForestFloorCoverage a,Mistwatch::ForestFloorCoverage b){return Same(a.soil,b.soil)&&Same(a.rock,b.rock);}
void SameMesh(const Aevrynd::Mesh3D& a,const Aevrynd::Mesh3D& b,bool uv) {
    Require(a.indices==b.indices,"triangle indices changed");Require(a.vertices.size()==b.vertices.size(),"vertex count changed");
    for(std::size_t index=0;index<a.vertices.size();++index){
        const auto& x=a.vertices[index];const auto& y=b.vertices[index];
        Require(Same(x.x,y.x)&&Same(x.y,y.y)&&Same(x.z,y.z)&&x.normal==y.normal&&x.color==y.color,
            "position normal or color changed");
        if(uv)Require(Same(x.u,y.u)&&Same(x.v,y.v),"legacy UV changed");
    }
}
template<class Operation> void Reject(Operation operation,const char* label) {
    bool rejected=false;try{operation();}catch(const std::invalid_argument&){rejected=true;}Require(rejected,label);
}
}

int main() try {
    using namespace Mistwatch;using namespace Aevrynd;
    const auto terrain=CreateTerrain();
    const auto solidsBefore=WorldSolids(false,FirLayoutSelector::ForestWood);
    const auto repairedBefore=WorldSolids(true,FirLayoutSelector::ForestWood);
    const auto oldCore=CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood);
    const auto oldLandscape=CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::ForestWood);
    const auto start=std::chrono::steady_clock::now();
    const auto core=CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    const auto landscape=CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    SameMesh(oldCore,core,false);SameMesh(oldLandscape,landscape,false);
    SameMesh(core,CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy),true);
    SameMesh(landscape,CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy),true);
    SameMesh(CreateTerrainMesh(terrain,false,ForestFloorMaskSelector::ForestWood),
        CreateTerrainMesh(terrain,false,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy),true);
    SameMesh(CreateLandscapeMesh(terrain,false,ForestFloorMaskSelector::ForestWood),
        CreateLandscapeMesh(terrain,false,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy),true);

    // 真正呼叫三個歷史 profile 的生產路由，不用字串或來源文字當幾何證據。
    for(const auto profile:{WorldProfile::LegacyCore,WorldProfile::LegacyMiddle,WorldProfile::ForestCanopy}) {
        Require(!WorldProfileHasWoodCollision(profile),"old profile acquired wood route");
        const auto floor=WorldProfileHasWoodCollision(profile)?ForestFloorMaskSelector::ForestWood:ForestFloorMaskSelector::Legacy;
        const auto bank=WorldProfileHasWoodCollision(profile)?BankCoverageSelector::Patchy:BankCoverageSelector::Legacy;
        SameMesh(CreateTerrainMesh(terrain,true),CreateTerrainMesh(terrain,true,floor,bank),true);
        SameMesh(CreateLandscapeMesh(terrain,true),CreateLandscapeMesh(terrain,true,floor,bank),true);
    }
    SameMesh(oldCore,CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Legacy),true);
    SameMesh(oldLandscape,CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Legacy),true);
    std::size_t changed{},retreat{},zeroRock{},protectedChecks{},seams{};float maxSum{};
    const auto checkVertex=[&](const Vertex3D& v,Vec3 normal) {
        const float old=RiverSedimentWeight(v.x,v.z);
        const float weight=ForestWoodBankSedimentWeight(v.x,v.z,normal,old);
        Require(Same(weight,ForestWoodBankSedimentWeight(v.x,v.z,normal,old)),"bank repeat mismatch");
        Require(std::isfinite(weight)&&weight>=0.f&&weight<=old,"bounded weight failed");
        const auto expected=ComposeBankFloorCoverage(v.x,v.z,normal,old,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
        Require(std::isfinite(v.u)&&std::isfinite(v.v)&&v.u>=0.f&&v.v>=0.f&&v.u+v.v<=1.000001f,"normalized layer coverage failed");
        Vec2 encoded{};Require(TryEncodeTerrainLayerCoverage3D(expected.soil,expected.rock,encoded),"encoding rejected");
        Require(Same(v.u,encoded.x)&&Same(v.v,encoded.y),"actual mesh missed authored coverage");
        maxSum=(std::max)(maxSum,v.u+v.v);
        if(weight<old)++changed;
        if(old>.05f&&weight==0.f){
            ++retreat;const auto base=ForestFloorAuthorCoverage(v.x,v.z,normal,ForestFloorMaskSelector::ForestWood);
            Require(Same(expected,base),"zero deposit did not restore authored base");
            if(v.v==0.f)++zeroRock;
        }
        if(std::abs(v.x)<=16.f||RiverDistance(v.x,v.z)<=RiverOuterWidth(v.x)*.5f){
            ++protectedChecks;Require(Same(weight,old),"bridge or submerged sediment boundary changed");
        }
    };
    for(const auto& vertex:core.vertices)checkVertex(vertex,terrain.SampleNormal(vertex.x,vertex.z));
    for(const auto& vertex:landscape.vertices){
        checkVertex(vertex,LandscapeNormal(terrain,vertex.x,vertex.z));
        const bool seam=(vertex.x==-50.f||vertex.x==50.f||vertex.z==-20.f||vertex.z==100.f)&&
            vertex.x>=-50.f&&vertex.x<=50.f&&vertex.z>=-20.f&&vertex.z<=100.f;
        if(seam){
            const auto ix=unsigned(std::lround((vertex.x+50.f)*2.f)),iz=unsigned(std::lround((vertex.z+20.f)*2.f));
            const auto& v=core.vertices[std::size_t(iz)*201u+ix];
            Require(Same(vertex.x,v.x)&&Same(vertex.y,v.y)&&Same(vertex.z,v.z)&&vertex.normal==v.normal&&vertex.color==v.color&&
                Same(vertex.u,v.u)&&Same(vertex.v,v.v),"real mesh core landscape seam mismatch");++seams;
        }
    }
    Require(changed>0&&retreat>0&&zeroRock>0,"no actual bank vertex restored base with rock zero");
    Require(protectedChecks>0&&seams==880,"protected or seam check budget mismatch");
    // 連續性有限探針不代替原生視覺驗收；檢查小位置變化未形成硬邊。
    for(unsigned i=0;i<256;++i){
        const float x=18.f+.125f*float(i),z=RiverCenterZ(x)+RiverOuterWidth(x)+4.f;
        const auto a=ComposeBankFloorCoverage(x,z,{0,1,0},RiverSedimentWeight(x,z),ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
        const auto b=ComposeBankFloorCoverage(x+.001f,z,{0,1,0},RiverSedimentWeight(x+.001f,z),ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
        Require(std::abs(a.soil-b.soil)<.01f&&std::abs(a.rock-b.rock)<.01f,"coverage discontinuity probe failed");
    }
    const auto compareSolids=[&](const auto& before,const auto& after){
        Require(before.size()==after.size(),"derived collider count changed");
        for(std::size_t index=0;index<before.size();++index)
            Require(Same(before[index].center,after[index].center)&&Same(before[index].extents,after[index].extents),"derived collider changed");
    };
    compareSolids(solidsBefore,WorldSolids(false,FirLayoutSelector::ForestWood));
    compareSolids(repairedBefore,WorldSolids(true,FirLayoutSelector::ForestWood));
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    for(const float bad:{nan,inf,-inf}) {
        Reject([&]{(void)ForestWoodBankSedimentWeight(bad,36,{0,1,0},.5f);},"invalid X accepted");
        Reject([&]{(void)ForestWoodBankSedimentWeight(28,bad,{0,1,0},.5f);},"invalid Z accepted");
        for(unsigned axis=0;axis<3;++axis){Vec3 n{0,1,0};if(axis==0)n.x=bad;else if(axis==1)n.y=bad;else n.z=bad;
            Reject([&]{(void)ForestWoodBankSedimentWeight(28,44,n,.5f);},"invalid normal accepted");}
        Reject([&]{(void)ForestWoodBankSedimentWeight(28,44,{0,1,0},bad);},"invalid weight accepted");
    }
    for(const float bad:{-.01f,1.01f})Reject([&]{(void)ForestWoodBankSedimentWeight(28,44,{0,1,0},bad);},"out of range weight accepted");
    for(const float bad:{-1.01f,1.01f})Reject([&]{(void)ForestWoodBankSedimentWeight(28,44,{0,bad,0},.5f);},"out of range normal Y accepted");
    Reject([&]{(void)CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::Legacy,BankCoverageSelector::Patchy);},"legacy patch route accepted");
    Reject([&]{(void)CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::ForestWood,static_cast<BankCoverageSelector>(255));},"bad bank selector accepted");
    Reject([&]{(void)CreateTerrainMesh(terrain,true,static_cast<ForestFloorMaskSelector>(255),BankCoverageSelector::Legacy);},"bad floor selector accepted");
    std::cout<<"PASS bank candidate checks="<<checks<<" changed="<<changed<<" base_retreat="<<retreat<<" zero_rock="<<zeroRock
        <<" protected="<<protectedChecks<<" actual_mesh_seams="<<seams<<" max_sum="<<maxSum
        <<" candidate_mesh_author_ms="<<elapsed<<" (single cold observation, not delta/performance acceptance)\n";
    return 0;
} catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE="<<error.what()<<" after_checks="<<checks<<'\n';return 100;}
