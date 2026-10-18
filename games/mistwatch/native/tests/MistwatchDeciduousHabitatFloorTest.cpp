#include "MistwatchDeciduousHabitatFloor.h"
#include "fallen/MistwatchAuthoredWorldQualification.h"
#include <iostream>
#include <iomanip>
#include <string_view>
namespace {
using namespace Mistwatch;
using namespace Aevrynd;
namespace Q=::Mistwatch::AuthoredQualification;
namespace W=::Mistwatch::FallenWorldSeat;
void Need(bool value,const char* gate){if(!value)throw std::runtime_error(gate);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
void SameGeometry(const Mesh3D& a,const Mesh3D& b){
    Need(a.indices==b.indices&&a.vertices.size()==b.vertices.size(),"Topology changed");
    for(std::size_t i=0;i<a.vertices.size();++i){const auto& x=a.vertices[i];const auto& y=b.vertices[i];
        Need(Same(x.x,y.x)&&Same(x.y,y.y)&&Same(x.z,y.z)&&x.normal==y.normal&&x.color==y.color&&Same(x.v,y.v),
            "Nonsoil field changed");}}
std::vector<W::Placement> Mappings(){std::vector<W::Placement> out;
    for(const auto& fixed:FallenLogPlacements()){W::Placement p;p.propId=fixed.propId;p.anchor=fixed.authoredAnchor;
        p.pose=fixed.pose;p.collider=fixed.collider;out.push_back(std::move(p));}return out;}
void Receipt(const char* profile,const char* kind,const ForestHabitatFloorReceipt& r){
    std::cout<<"DECIDUOUS_MESH profile="<<profile<<" kind="<<kind<<" fingerprint="<<r.fingerprint
        <<" vertices="<<r.vertices<<" changed="<<r.changed<<" soil_dominant="<<r.soilDominant
        <<" moss_dominant="<<r.mossDominant<<" bridge="<<r.bridgePreserved<<'\n';}
}
int wmain(int argc,wchar_t** argv)try{
    Need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","Usage --runtime-assets ABS");
    const auto root=std::filesystem::path(argv[2]).lexically_normal();Need(root.is_absolute(),"Absolute runtime required");
    const auto terrain=CreateTerrain();W::Budget budget;const auto assets=Q::LoadAssets(root);
    const auto source=Q::CreateSourceContext(terrain,assets,budget);
    const auto context=Q::CreateContext(terrain,assets,source,Mappings(),budget);
    const auto contextSeal=Q::ContextFingerprint(context),sourceSeal=Q::SourceFingerprint(source);
    const auto plan=CreateDeciduousHabitatPlan(context.author.broadleaf,context.author.middle.trees,context.author.middleSourceIds);
    Need(plan.roots.size()==35,"Actual root census missing");
    // 根姿態獨立對照真 author，不讓 producer 的錯誤根表同時充當自己的 oracle。
    for(unsigned i=0;i<35;++i){const auto& r=plan.roots[i];const bool core=i<12;const unsigned index=core?i:i-12;
        const auto& actual=core?context.author.broadleaf[index]:context.author.middle.trees[index];
        Need(r.role==(core?0u:1u)&&r.sourceId==(core?index:context.author.middleSourceIds[index])&&
            W::PoseBits(r.pose)==W::PoseBits(actual.transform)&&
            Same(r.radius,BroadleafBounds().extents.z*actual.transform.scale.x),"Actual author root mapping changed");}
    std::cout<<std::setprecision(9)<<"DECIDUOUS_ROOT_PLAN fingerprint="<<plan.fingerprint<<" broadleaf=12 middle=23\n";
    for(const auto& r:plan.roots)std::cout<<"DECIDUOUS_ROOT role="<<r.role<<" source_id="<<r.sourceId<<" position="
        <<r.pose.position.x<<','<<r.pose.position.y<<','<<r.pose.position.z<<" radius="<<r.radius<<'\n';
    std::vector<Vec3> crowns;for(const auto& p:context.canopy.plants)crowns.push_back(p.pose.position);
    const ForestHabitatRoots oldRoots{context.author.firs.roots,crowns};
    auto floor=CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    auto landscapeMesh=CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    const auto oldFloor=ApplyForestHabitatFloor(floor,terrain,oldRoots);
    const auto oldFar=ApplyForestHabitatFloor(landscapeMesh,terrain,oldRoots,true);
    Receipt("original","terrain",oldFloor);Receipt("original","landscape",oldFar);
    const auto before=floor,beforeFar=landscapeMesh;auto repeat=before;
    const auto candidate=ApplyDeciduousHabitatFloor(floor,terrain,plan);
    const auto repeated=ApplyDeciduousHabitatFloor(repeat,terrain,plan);
    const auto candidateFar=ApplyDeciduousHabitatFloor(landscapeMesh,terrain,plan,true);
    SameGeometry(before,floor);SameGeometry(beforeFar,landscapeMesh);
    Need(Q::Detail::Same(beforeFar,landscapeMesh),"All distant landscape UV bits changed");
    Need(Q::Detail::Same(floor,repeat)&&candidate.fingerprint==repeated.fingerprint,"Repeat drift");
    std::size_t changed{},unchanged{},nearFive{};double added{};
    for(std::size_t i=0;i<before.vertices.size();++i){const auto& a=before.vertices[i];const auto& b=floor.vertices[i];
        Need(b.u>=a.u&&b.u+b.v<=1.000001f,"Normalized layers failed");
        const bool protectedArea=(a.z>32.f&&a.z<40.f)||std::abs(a.x)>=50.f||a.z<=-20.f||a.z>=100.f;
        if(protectedArea)Need(Same(a.u,b.u),"Bridge or boundary changed");
        // 獨立逐頂點公式，不以 producer 的 Coverage 呼叫作 oracle。
        float expected=a.u;bool inside=false;
        if(!protectedArea){const auto normal=terrain.SampleNormal(a.x,a.z);
            const float bank=ForestWoodBankSedimentWeight(a.x,a.z,normal,RiverSedimentWeight(a.x,a.z));
            const float edge=ForestFloorSmooth((std::min)({50.f-std::abs(a.x),a.z+20.f,100.f-a.z})/5.f);
            for(const auto& r:plan.roots){const auto p=r.pose.position;
                const float distance=std::hypot(a.x-p.x,a.z-p.z);inside|=distance<r.radius;
                const float leaf=.85f*ForestFloorSmooth(1.f-(distance-.30f)/(r.radius-.30f));
                expected=(std::max)(expected,leaf*edge*(1.f-bank)*(1.f-a.v));}}
        Need(Same(expected,b.u),"Independent per-vertex leaf formula failed");
        if(!Same(a.u,b.u)){++changed;Need(inside,"Changed outside all root zones");
            const auto p=context.author.broadleaf[5].transform.position;
            nearFive+=std::hypot(a.x-p.x,a.z-p.z)<plan.roots[5].radius;}
        else ++unchanged;added+=double(b.u)-double(a.u);
    }
    Need(changed==candidate.changed&&changed>0&&unchanged>0&&added>0&&candidateFar.changed==0&&nearFive>0,
        "No local root effect or remote mountain changed");
    Need(candidate.rockSum==oldFloor.rockSum,"Rock weight changed");
    unsigned controls{};
    const auto reject=[&](const DeciduousHabitatPlan& broken,const char* gate){auto untouched=before;bool rejected=false;
        try{(void)ApplyDeciduousHabitatFloor(untouched,terrain,broken);}catch(const std::invalid_argument& e){
            Need(std::string_view(e.what()).find(gate)!=std::string_view::npos,"Wrong negative-control gate");rejected=true;}
        Need(rejected&&Q::Detail::Same(untouched,before),"Invalid input partially painted");++controls;};
    auto broken=plan;broken.roots.pop_back();reject(broken,"actual-root-census");
    broken=plan;broken.roots[12].sourceId=16;reject(broken,"actual-source-role-order");
    broken=plan;broken.roots[0].radius*=2;reject(broken,"authored-leaf-radius");
    broken=plan;broken.fingerprint^=1;reject(broken,"root-plan-identity");
    broken=plan;broken.roots[0].pose.position.x=std::numeric_limits<float>::quiet_NaN();reject(broken,"actual-root-pose-domain");
    broken=plan;broken.roots[0].pose.position.y+=1;broken.fingerprint=DeciduousHabitatDetail::Identity(broken.roots);
    reject(broken,"actual-root-ground-source");
    const auto rejectMesh=[&](Mesh3D brokenMesh,const char* gate){const auto untouched=brokenMesh;bool rejected=false;
        try{(void)ApplyDeciduousHabitatFloor(brokenMesh,terrain,plan);}catch(const std::invalid_argument& e){
            Need(std::string_view(e.what()).find(gate)!=std::string_view::npos,"Wrong mesh negative-control gate");rejected=true;}
        Need(rejected&&brokenMesh.indices==untouched.indices&&brokenMesh.vertices.size()==untouched.vertices.size(),"Invalid mesh topology altered");
        for(std::size_t i=0;i<brokenMesh.vertices.size();++i){const auto& a=brokenMesh.vertices[i];const auto& b=untouched.vertices[i];
            Need(Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&Same(a.u,b.u)&&Same(a.v,b.v)&&
                a.color==b.color&&a.normal==b.normal,"Invalid mesh partially painted");}++controls;};
    auto badMesh=before;badMesh.vertices.back().u=std::numeric_limits<float>::quiet_NaN();rejectMesh(badMesh,"actual-mesh-layer-domain");
    badMesh=before;badMesh.indices.back()=static_cast<std::uint32_t>(before.vertices.size());rejectMesh(badMesh,"actual-mesh-validity");
    Receipt("root-cohorts-v1","terrain",candidate);Receipt("root-cohorts-v1","landscape",candidateFar);
    Need(Q::ContextFingerprint(context)==contextSeal&&Q::SourceFingerprint(source)==sourceSeal,"Source or collision context drift");
    std::cout<<"DECIDUOUS_HABITAT_CPU_PASS context_fingerprint="<<contextSeal<<" changed="<<changed<<" unchanged="<<unchanged
        <<" near_broadleaf_5="<<nearFive<<" negative_controls="<<controls<<" terrain_geometry_unchanged=1 world_unchanged=1"
        <<" native=0 art=0 published=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"DECIDUOUS_HABITAT_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
