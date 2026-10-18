#include "MistwatchForestHabitatFloor.h"
#include <iostream>
#include <limits>
#include <string_view>

namespace {
using namespace Mistwatch;
using namespace Aevrynd;
void Need(bool okay,const char* gate){if(!okay)throw std::runtime_error(gate);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
template<class Call> void Reject(Call call){bool rejected=false;
    try{call();}catch(const std::exception&){rejected=true;}Need(rejected,"Invalid input accepted");}
}
int main()try{
    const auto terrain=CreateTerrain();
    const std::array<Vec3,2> firs{{{-15,terrain.SampleHeight(-15,5),5},{18,terrain.SampleHeight(18,20),20}}};
    const std::array<Vec3,1> crowns{{{12,terrain.SampleHeight(12,8),8}}};
    const ForestHabitatRoots roots{firs,crowns};
    const auto base=CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    auto mesh=base,repeat=base;
    const auto proof=ApplyForestHabitatFloor(mesh,terrain,roots),again=ApplyForestHabitatFloor(repeat,terrain,roots);
    Need(proof.fingerprint==again.fingerprint&&proof.changed==again.changed,"Repeat changed");
    Need(proof.changed>10000&&proof.soilDominant>1000&&proof.mossDominant>1000,"Material variety incomplete");
    Need(base.indices==mesh.indices&&base.vertices.size()==mesh.vertices.size(),"Topology changed");
    std::size_t bridge{},seams{};double delta{};
    for(std::size_t i=0;i<base.vertices.size();++i){const auto& a=base.vertices[i];const auto& b=mesh.vertices[i];
        Need(Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&a.normal==b.normal&&a.color==b.color,
            "Noncoverage vertex field changed");
        Need(Same(a.v,b.v)&&b.u>=a.u&&b.u+b.v<=1.000001f,"Original rock or normalized layers changed");
        Need(Same(b.u,repeat.vertices[i].u)&&Same(b.v,repeat.vertices[i].v),"Repeated coverage bits differ");
        const auto expected=ForestHabitatFloorCoverage(b.x,b.z,terrain.SampleNormal(b.x,b.z),RiverSedimentWeight(b.x,b.z),roots);
        Need(Same(b.u,expected.soil)&&Same(b.v,expected.rock),"Actual mesh not encoded from habitat");
        if(b.z>32&&b.z<40){++bridge;Need(Same(a.u,b.u)&&Same(a.v,b.v),"Bridge changed");}
        if(b.x== -50||b.x==50||b.z== -20||b.z==100)Need(Same(a.u,b.u)&&Same(a.v,b.v),"Boundary changed");
        delta+=double(b.u)-double(a.u);
    }
    Need(bridge==proof.bridgePreserved&&delta>1000,"No actual coverage change");
    auto landscape=CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    const auto originalFar=landscape;const auto farProof=ApplyForestHabitatFloor(landscape,terrain,roots,true);
    Need(landscape.indices==originalFar.indices,"Landscape topology changed");
    for(std::size_t i=0;i<landscape.vertices.size();++i){const auto& a=originalFar.vertices[i];const auto& b=landscape.vertices[i];
        Need(Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&a.normal==b.normal&&a.color==b.color,"Landscape geometry changed");
        const bool boundary=(b.x== -50||b.x==50||b.z== -20||b.z==100)&&b.x>=-50&&b.x<=50&&b.z>=-20&&b.z<=100;
        if(boundary){const auto ix=unsigned(std::lround((b.x+50)*2)),iz=unsigned(std::lround((b.z+20)*2));
            const auto& core=mesh.vertices[std::size_t(iz)*201+ix];
            Need(Same(core.u,b.u)&&Same(core.v,b.v),"Terrain landscape seam mismatch");++seams;}
        if(std::abs(b.x)>=50||b.z<=-20||b.z>=100)Need(Same(a.u,b.u)&&Same(a.v,b.v),"Distant mountain art changed");
    }
    Need(seams>500&&farProof.changed==0,"Unexpected distant habitat painting");
    const auto underRoot=ForestHabitatFloorCoverage(12,8,{0,1,0},0,roots);
    const auto isolated=ForestHabitatFloorCoverage(12,8,{0,1,0},0,{});
    Need(Same(underRoot.soil,.85f)&&underRoot.soil>isolated.soil,"Real crown root not driving soil");
    unsigned spurs{};
    const auto spur=[&](Vec3 p){const float x=p.x*.5f;
        const auto result=ForestHabitatFloorCoverage(x,p.z,{0,1,0},0,{});
        Need(result.soil>=.78f,"Mission spur not visible");++spurs;};
    for(auto p:Session::supplies)spur(p);for(auto p:Session::repairSites)spur(p);
    for(auto p:{Session::fuse,Session::generator,Session::rangerLog})spur(p);
    for(auto p:Session::relaySites)spur(p);
    const auto river=ForestHabitatFloorCoverage(18,36,{0,1,0},1,roots);
    Need(Same(river.soil,.15f)&&Same(river.rock,.85f),"Full river exact material changed");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{(void)ForestHabitatFloorCoverage(nan,0,{0,1,0},0,roots);});
    Reject([&]{(void)ForestHabitatFloorCoverage(0,0,{0,nan,0},0,roots);});
    Reject([&]{(void)ForestHabitatFloorCoverage(0,0,{0,1,0},1.01f,roots);});
    Reject([&]{(void)ForestHabitatFloorCoverage(1e30f,0,{0,1,0},0,roots);});
    std::array<Vec3,1> bad{{{nan,0,0}}};
    Reject([&]{(void)ForestHabitatFloorCoverage(0,0,{0,1,0},0,{bad,{}});});
    std::array<Vec3,151> tooMany{};
    Reject([&]{(void)ForestHabitatFloorCoverage(0,0,{0,1,0},0,{tooMany,{}});});
    auto badMesh=base;badMesh.vertices.back().x=nan;const auto first=badMesh.vertices.front().u;
    Reject([&]{(void)ApplyForestHabitatFloor(badMesh,terrain,roots);});
    Need(Same(first,badMesh.vertices.front().u),"Invalid mesh partially painted");
    // 原預設與明確 Original 必須逐位元相同；候選不取代正式配方。
    constexpr auto originalProfile=ForestHabitatFloorProfile::Original;
    constexpr auto leafProfile=ForestHabitatFloorProfile::LeafCoverageStrengthV1;
    auto explicitOriginal=base,leaf=base,leafRepeat=base;
    const auto originalProof=ApplyForestHabitatFloor(explicitOriginal,terrain,roots,false,originalProfile);
    const auto leafProof=ApplyForestHabitatFloor(leaf,terrain,roots,false,leafProfile);
    const auto leafAgain=ApplyForestHabitatFloor(leafRepeat,terrain,roots,false,leafProfile);
    Need(originalProof.fingerprint==proof.fingerprint&&originalProof.changed==proof.changed,
        "Explicit Original differs from default");
    Need(leaf.indices==base.indices&&leafRepeat.indices==base.indices&&explicitOriginal.indices==base.indices&&
        leaf.vertices.size()==base.vertices.size()&&leafRepeat.vertices.size()==base.vertices.size()&&
        explicitOriginal.vertices.size()==base.vertices.size(),"Candidate topology changed");
    Need(leafProof.fingerprint==leafAgain.fingerprint&&leafProof.vertices==leafAgain.vertices&&
        leafProof.changed==leafAgain.changed&&leafProof.soilDominant==leafAgain.soilDominant&&
        leafProof.mossDominant==leafAgain.mossDominant&&leafProof.bridgePreserved==leafAgain.bridgePreserved&&
        leafProof.soilSum==leafAgain.soilSum&&leafProof.rockSum==leafAgain.rockSum,"Candidate receipt repeat changed");
    std::size_t leafChanged{},leafUnchanged{},leafBridge{};double leafDelta{};
    for(std::size_t i=0;i<base.vertices.size();++i){
        const auto& a=base.vertices[i];const auto& old=mesh.vertices[i];const auto& b=leaf.vertices[i];
        const auto& explicitVertex=explicitOriginal.vertices[i];const auto& r=leafRepeat.vertices[i];
        Need(Same(explicitVertex.x,old.x)&&Same(explicitVertex.y,old.y)&&Same(explicitVertex.z,old.z)&&
            Same(explicitVertex.u,old.u)&&Same(explicitVertex.v,old.v)&&explicitVertex.color==old.color&&
            explicitVertex.normal==old.normal,"Explicit Original vertex bits changed");
        Need(Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&a.normal==b.normal&&a.color==b.color&&
            Same(a.x,r.x)&&Same(a.y,r.y)&&Same(a.z,r.z)&&a.normal==r.normal&&a.color==r.color,
            "Candidate noncoverage field changed");
        Need(Same(a.v,b.v)&&Same(old.v,b.v)&&Same(b.v,r.v)&&Same(b.u,r.u),
            "Candidate rock or repeated UV bits changed");
        Need(std::isfinite(b.u)&&std::isfinite(b.v)&&b.u>=0.f&&b.v>=0.f&&b.u<=1.f&&b.v<=1.f&&
            b.u+b.v<=1.000001f&&b.u>=old.u,"Candidate normalized layers or monotonic soil failed");
        const auto normal=terrain.SampleNormal(b.x,b.z);const float sediment=RiverSedimentWeight(b.x,b.z);
        const auto expected=ForestHabitatFloorCoverage(b.x,b.z,normal,sediment,roots,leafProfile);
        const auto originalExpected=ForestHabitatFloorCoverage(b.x,b.z,normal,sediment,roots,originalProfile);
        Need(Same(b.u,expected.soil)&&Same(b.v,expected.rock)&&Same(old.u,originalExpected.soil)&&
            Same(old.v,originalExpected.rock),"Candidate public coverage not encoded in mesh");
        if(Same(b.u,old.u))++leafUnchanged;else ++leafChanged;
        leafDelta+=double(b.u)-double(old.u);
        if(b.z>32.f&&b.z<40.f){++leafBridge;Need(Same(a.u,b.u)&&Same(a.v,b.v),"Candidate bridge changed");}
        if(b.x== -50.f||b.x==50.f||b.z== -20.f||b.z==100.f)
            Need(Same(a.u,b.u)&&Same(a.v,b.v),"Candidate boundary changed");
    }
    Need(leafChanged>0&&leafUnchanged>0&&leafDelta>0.0&&leafProof.mossDominant>0&&
        leafProof.soilDominant>=proof.soilDominant&&leafProof.soilSum>=proof.soilSum&&
        leafProof.rockSum==proof.rockSum&&leafBridge==proof.bridgePreserved&&
        leafBridge==leafProof.bridgePreserved&&leafProof.fingerprint!=proof.fingerprint,
        "Candidate did not locally strengthen soil while retaining moss");
    auto leafFar=originalFar,leafFarRepeat=originalFar;
    const auto leafFarProof=ApplyForestHabitatFloor(leafFar,terrain,roots,true,leafProfile);
    const auto leafFarAgain=ApplyForestHabitatFloor(leafFarRepeat,terrain,roots,true,leafProfile);
    Need(leafFar.indices==originalFar.indices&&leafFarRepeat.indices==originalFar.indices&&
        leafFar.vertices.size()==originalFar.vertices.size()&&leafFarRepeat.vertices.size()==originalFar.vertices.size(),
        "Candidate landscape topology changed");
    Need(leafFarProof.fingerprint==leafFarAgain.fingerprint&&leafFarProof.fingerprint==farProof.fingerprint&&
        leafFarProof.changed==0&&leafFarAgain.changed==0,"Candidate painted far landscape");
    std::size_t leafSeams{};
    for(std::size_t i=0;i<originalFar.vertices.size();++i){
        const auto& a=originalFar.vertices[i];const auto& b=leafFar.vertices[i];const auto& r=leafFarRepeat.vertices[i];
        Need(Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&a.color==b.color&&a.normal==b.normal&&
            Same(a.x,r.x)&&Same(a.y,r.y)&&Same(a.z,r.z)&&a.color==r.color&&a.normal==r.normal&&
            Same(a.u,b.u)&&Same(a.v,b.v)&&Same(b.u,r.u)&&Same(b.v,r.v),"Candidate far mesh bits changed");
        const bool boundary=(b.x== -50.f||b.x==50.f||b.z== -20.f||b.z==100.f)&&
            b.x>=-50.f&&b.x<=50.f&&b.z>=-20.f&&b.z<=100.f;
        if(boundary){const auto ix=unsigned(std::lround((b.x+50.f)*2.f)),iz=unsigned(std::lround((b.z+20.f)*2.f));
            const auto& core=leaf.vertices[std::size_t(iz)*201+ix];
            Need(Same(core.u,b.u)&&Same(core.v,b.v),"Candidate terrain landscape seam mismatch");++leafSeams;}
    }
    Need(leafSeams==seams,"Candidate seam count changed");
    const auto leafUnderRoot=ForestHabitatFloorCoverage(12,8,{0,1,0},0,roots,leafProfile);
    const auto leafIsolated=ForestHabitatFloorCoverage(12,8,{0,1,0},0,{},leafProfile);
    Need(Same(leafUnderRoot.soil,(std::max)(.85f,leafIsolated.soil))&&leafUnderRoot.soil>=leafIsolated.soil&&
        Same(leafUnderRoot.rock,underRoot.rock),"Candidate crown root floor changed");
    // 用完整冠根環逐點確認原半徑／中心保護仍生效，不把高噪聲當成根部證據。
    for(const float dx:{0.f,.30f,1.f,2.f,4.2f}){
        const float x=12.f+dx;
        const auto rooted=ForestHabitatFloorCoverage(x,8,{0,1,0},0,roots,leafProfile);
        const auto unrooted=ForestHabitatFloorCoverage(x,8,{0,1,0},0,{},leafProfile);
        const float distance=std::hypot(x-12.f,0.f);
        const float rootFloor=.85f*ForestFloorSmooth(1.f-(distance-.30f)/(4.2f-.30f));
        Need(Same(rooted.soil,(std::max)(unrooted.soil,rootFloor))&&Same(rooted.rock,unrooted.rock),
            "Candidate crown radius changed");
    }
    for(const float dx:{0.f,.30f,1.f,1.8f}){
        const float x=-15.f+dx;
        const auto rooted=ForestHabitatFloorCoverage(x,5,{0,1,0},0,roots,leafProfile);
        const auto unrooted=ForestHabitatFloorCoverage(x,5,{0,1,0},0,{},leafProfile);
        const float distance=std::hypot(x+15.f,0.f);
        const float rootFloor=.85f*ForestFloorSmooth(1.f-(distance-.30f)/(1.8f-.30f));
        Need(Same(rooted.soil,(std::max)(unrooted.soil,rootFloor))&&Same(rooted.rock,unrooted.rock),
            "Candidate fir radius changed");
    }
    unsigned leafSpurs{};
    const auto leafSpur=[&](Vec3 p){const float x=p.x*.5f;
        const auto before=ForestHabitatFloorCoverage(x,p.z,{0,1,0},0,{});
        const auto after=ForestHabitatFloorCoverage(x,p.z,{0,1,0},0,{},leafProfile);
        Need(after.soil>=.78f&&after.soil>=before.soil&&Same(after.rock,before.rock),"Candidate mission spur changed");
        ++leafSpurs;};
    for(auto p:Session::supplies)leafSpur(p);for(auto p:Session::repairSites)leafSpur(p);
    for(auto p:{Session::fuse,Session::generator,Session::rangerLog})leafSpur(p);
    for(auto p:Session::relaySites)leafSpur(p);
    Need(leafSpurs==spurs,"Candidate mission spur count changed");
    const auto leafRiver=ForestHabitatFloorCoverage(18,36,{0,1,0},1,roots,leafProfile);
    Need(Same(leafRiver.soil,river.soil)&&Same(leafRiver.rock,river.rock),"Candidate river material changed");
    unsigned profileInvalid{};
    // 僅指定的非法配方例外算拒絕；其他輸入、資源或執行錯誤不能冒充負控通過。
    const auto rejectProfile=[&](auto call){bool rejected=false;
        try{call();}catch(const std::invalid_argument& error){
            Need(std::string_view(error.what())=="Invalid forest habitat floor profile","Wrong invalid profile reason");
            rejected=true;}
        Need(rejected,"Invalid profile accepted");++profileInvalid;};
    for(const unsigned value:{2u,255u}){
        const auto invalidProfile=static_cast<ForestHabitatFloorProfile>(value);
        for(const Vec3 p:{Vec3{0,0,0},Vec3{18,0,36},Vec3{50,0,100}})
            rejectProfile([&]{(void)ForestHabitatFloorCoverage(p.x,p.z,{0,1,0},0,roots,invalidProfile);});
        for(const bool distant:{false,true}){
            const auto& source=distant?originalFar:base;auto untouched=source;
            rejectProfile([&]{(void)ApplyForestHabitatFloor(untouched,terrain,roots,distant,invalidProfile);});
            Need(untouched.indices==source.indices&&untouched.vertices.size()==source.vertices.size(),
                "Invalid profile partially changed topology");
            for(std::size_t i=0;i<source.vertices.size();++i){const auto& a=source.vertices[i];const auto& b=untouched.vertices[i];
                Need(Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&Same(a.u,b.u)&&Same(a.v,b.v)&&
                    a.color==b.color&&a.normal==b.normal,"Invalid profile partially painted mesh");}
        }
    }
    Need(profileInvalid==10,"Invalid profile controls incomplete");
    std::cout<<"HABITAT_LEAF_STRENGTH_CANDIDATE_UNIT_PASS changed="<<leafChanged<<" unchanged="<<leafUnchanged
        <<" moss_dominant="<<leafProof.mossDominant<<" spurs="<<leafSpurs<<" bridge="<<leafBridge
        <<" seams="<<leafSeams<<" profile_invalid="<<profileInvalid<<" GPU_unverified=1\n";
    std::cout<<"HABITAT_FLOOR_UNIT_PASS vertices="<<proof.vertices<<" changed="<<proof.changed
        <<" soil_dominant="<<proof.soilDominant<<" moss_dominant="<<proof.mossDominant
        <<" spurs="<<spurs<<" bridge="<<bridge<<" seams="<<seams<<" invalid=7 geometry_preserved=1 GPU_unverified=1\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"HABITAT_FLOOR_UNIT_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}
