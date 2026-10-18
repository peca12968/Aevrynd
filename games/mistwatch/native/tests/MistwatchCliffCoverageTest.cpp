#include "MistwatchCliffCoverage.h"
#include "MistwatchForestHabitatFloor.h"
#include "MistwatchSettlementSurface.h"
#include <iostream>
#include <limits>
#include <map>

namespace {
namespace C=Mistwatch::CliffCoverage;
unsigned checks{};
void Need(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(const Aevrynd::Mesh3D& a,const Aevrynd::Mesh3D& b){return Mistwatch::SettlementGround::SameMesh(a,b);}
void Check(const Aevrynd::Mesh3D& before,const C::Result& result,const Aevrynd::HeightField3D& core) {
    const auto& after=result.mesh;Need(before.indices==after.indices,"Indices changed");
    Need(before.vertices.size()==after.vertices.size(),"Vertex count changed");
    Need(result.receipt.beforeGeometryFingerprint==result.receipt.afterGeometryFingerprint,"Geometry seal changed");
    std::size_t outside{},east{},protectedCount{},full{},zero{};
    for(std::size_t i=0;i<before.vertices.size();++i){
        const auto& a=before.vertices[i];const auto& b=after.vertices[i];
        Need(Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z)&&a.normal==b.normal&&a.color==b.color,"Non-UV payload changed");
        const bool same=Bits(a.u,b.u)&&Bits(a.v,b.v);
        const bool outsideDomain=a.x<=-56.f||a.x>=-40.f||a.z<=50.f||a.z>=66.f;
        if(outsideDomain){++outside;Need(same,"Outside geographic domain changed");}
        if(a.x>0){++east;Need(same,"East terrain changed");}
        if(C::Protected(a.x,a.y,a.z)){++protectedCount;Need(same,"Water or road changed");}
        const float mask=C::Mask(core,a.x,a.y,a.z);
        if(mask==0){++zero;Need(same,"Zero mask bits changed");}
        else {
            Need(std::abs(double(b.u)-double(a.u)*(1-double(mask)))<1e-7,"Soil blend mismatch");
            Need(std::abs(double(b.v)-(double(a.v)*(1-double(mask))+.95*double(mask)))<1e-7,"Rock blend mismatch");
        }
        Need(std::isfinite(b.u)&&std::isfinite(b.v)&&b.u>=0&&b.v>=0&&b.u+b.v<=1,"Invalid encoded coverage");
        if(mask==1){++full;Need(Bits(b.u,0)&&Bits(b.v,.95f),"Full mask organic retreat failed");
            // 與 Shader 相同的三層合成；任意原比例的有機層總和都不超過 5%。
            for(const auto weights:{Aevrynd::Vec3{1,0,0},Aevrynd::Vec3{0,1,0},Aevrynd::Vec3{.2f,.3f,.5f}}){
                const float p=1-b.u-b.v,soil=weights.y*p+b.u,grass=weights.x*p,rock=weights.z*p+b.v;
                Need(grass+soil<=.050001f&&rock>=.949999f&&std::abs(grass+soil+rock-1)<1e-6,"Shader organic residue");
            }
        }
    }
    Need(outside&&east&&protectedCount&&full&&zero,"Expected geographic test cohorts missing");
    Need(full==result.receipt.fullMask&&zero==result.receipt.zeroMask,"Receipt count mismatch");
    Need(result.receipt.beforeFingerprint==Mistwatch::EntranceRoadDomainDetail::Fingerprint(before)&&
        result.receipt.afterFingerprint==Mistwatch::EntranceRoadDomainDetail::Fingerprint(after),"Full seal mismatch");
    std::cout<<"changed="<<result.receipt.changed<<" full="<<full<<" zero="<<zero<<" protected="<<protectedCount<<" geometry="<<result.receipt.beforeGeometryFingerprint<<'\n';
}
template<class F>void Reject(Aevrynd::Mesh3D input,const Aevrynd::HeightField3D& core,F mutate,C::Domain domain=C::Domain::Core,C::Profile profile=C::Profile::WestCliffV1){
    mutate(input);const auto copy=input;bool threw=false;
    try{(void)C::Create(input,core,domain,profile);}catch(const std::invalid_argument&){threw=true;}
    Need(threw,"Invalid input accepted");
    // 以逐位元封印確認含 NaN 的原輸入也未被改寫。
    Need(Mistwatch::EntranceRoadDomainDetail::Fingerprint(input)==Mistwatch::EntranceRoadDomainDetail::Fingerprint(copy),"Failure mutated input");
}
}
int main(){try{
    using namespace Mistwatch;
    const auto core=CreateTerrain();
    auto source=CreateTerrainMesh(core,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    auto ring=CreateLandscapeMesh(core,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    ApplyForestHabitatFloor(source,core,{});ApplyForestHabitatFloor(ring,core,{},true);
    const auto entrance=CreateEntranceRoadDomain(source);const auto settlement=SettlementGround::Create(source,entrance);
    const auto split=SettlementSurface::Create(source,entrance,settlement);
    const auto roadBefore=split.entrance,gravelBefore=split.gravel;
    const auto result=C::Create(split.forest,core,C::Domain::Core);
    const auto ringResult=C::Create(ring,core,C::Domain::Ring);
    Check(split.forest,result,core);Check(ring,ringResult,core);
    Need(Same(split.entrance,roadBefore)&&Same(split.gravel,gravelBefore),"Other split meshes changed");
    Need(Same(C::Create(split.forest,core,C::Domain::Core,C::Profile::Original).mesh,split.forest),"Original profile changed bits");
    Need(Same(C::Create(split.forest,core,C::Domain::Core).mesh,result.mesh),"Not deterministic");
    std::map<float,std::size_t> seam;
    for(std::size_t i=0;i<result.mesh.vertices.size();++i)if(result.mesh.vertices[i].x==-50.f)seam.emplace(result.mesh.vertices[i].z,i);
    unsigned seamCount{},changedSeam{};
    for(std::size_t i=0;i<ringResult.mesh.vertices.size();++i){const auto& b=ringResult.mesh.vertices[i];
        if(b.x!=-50.f||b.z< -20||b.z>100)continue;
        const auto& a=result.mesh.vertices[seam.at(b.z)];
        Need(Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z)&&a.normal==b.normal,"Canonical seam geometry differs");
        Need(Bits(a.u,b.u)&&Bits(a.v,b.v),"Core/ring seam coverage differs");++seamCount;
        changedSeam+=!Bits(b.v,ring.vertices[i].v);
    }
    Need(seamCount==241&&changedSeam>0,"Shared seam not exercised");
    // 水域、道路與工棚保護由真地理來源計算，不由攝影機決定。
    Need(C::Protected(-48,-5,57)&&C::Mask(core,-48,-5,57)==0,"Underwater not protected");
    Need(C::Protected(0,0,57),"Central trail not protected");
    for(float x:{-56.f,-40.f})Need(C::GeographicMask(x,57)==0,"X boundary not zero");
    for(float z:{50.f,66.f})Need(C::GeographicMask(-48,z)==0,"Z boundary not zero");
    Need(C::GeographicMask(-55.999f,57)<.00001f&&C::GeographicMask(-54,57)==1,"Feather discontinuity");
    auto signedZero=split.forest;signedZero.vertices.front().u=-0.f;signedZero.vertices.front().v=0;
    Need(Bits(C::Create(signedZero,core,C::Domain::Core).mesh.vertices.front().u,-0.f),"Zero mask signed zero lost");
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    Reject(source,core,[&](auto& m){m.vertices.back().u=nan;});
    Reject(source,core,[&](auto& m){m.vertices.back().x=nan;});
    Reject(source,core,[](auto& m){m.vertices.back().u=-.1f;});
    Reject(source,core,[](auto& m){m.vertices.back().u=.8f;m.vertices.back().v=.8f;});
    Reject(source,core,[](auto& m){m.vertices.back().normal=0;});
    Reject(source,core,[](auto& m){m.indices.back()=static_cast<std::uint32_t>(m.vertices.size());});
    Reject(source,core,[](auto& m){m.indices[1]=m.indices[0];});
    Reject(source,core,[](auto& m){m.vertices.clear();});
    Reject(source,core,[](auto&){},static_cast<C::Domain>(255));
    Reject(source,core,[](auto&){},C::Domain::Core,static_cast<C::Profile>(255));
    Reject(source,core,[](auto&){},C::Domain::Ring);
    Reject(ring,core,[](auto&){},C::Domain::Core);
    const Aevrynd::HeightField3D wrongCore;
    Reject(source,wrongCore,[](auto&){});
    std::cout<<"CLIFF_COVERAGE_PASS checks="<<checks<<" seam="<<seamCount<<" changedSeam="<<changedSeam<<" negative=13\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"CLIFF_COVERAGE_FAIL "<<e.what()<<'\n';return 1;}}
