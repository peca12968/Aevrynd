#include "MistwatchMixedLandscapeCanopy.h"
#include "MistwatchCliffCoverage.h"
#include "MistwatchBroadleaf.h"
#include "fallen/MistwatchAuthoredWorldQualification.h"
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

namespace {
using namespace Aevrynd;
namespace M=Mistwatch::MixedLandscapeCanopy;
namespace S=Mistwatch::LandscapeStandFill;
namespace Q=Mistwatch::AuthoredQualification;
unsigned negatives{};
void Need(bool yes,const char* what){if(!yes)throw std::runtime_error(what);}
template<class F>void Reject(F&& action,const char* reason){
    try{action();}catch(const std::invalid_argument& e){
        Need(std::string_view(e.what())==reason,"wrong rejection reason");++negatives;return;
    }
    throw std::runtime_error("negative accepted");
}
void Word(std::uint64_t& h,std::uint32_t value){
    for(unsigned i=0;i<4;++i){h^=(value>>(8*i))&255u;h*=1099511628211ull;}
}
std::array<float,9> Fields(const Transform3D& p){
    return {p.position.x,p.position.y,p.position.z,p.rotation.x,p.rotation.y,p.rotation.z,p.scale.x,p.scale.y,p.scale.z};
}
bool SamePose(const Transform3D& a,const Transform3D& b){
    const auto x=Fields(a),y=Fields(b);
    for(unsigned i=0;i<x.size();++i)if(std::bit_cast<std::uint32_t>(x[i])!=std::bit_cast<std::uint32_t>(y[i]))return false;
    return true;
}
std::array<std::uint32_t,7> BoundsWords(const MeshBounds3D& b){
    return {std::bit_cast<std::uint32_t>(b.center.x),std::bit_cast<std::uint32_t>(b.center.y),
        std::bit_cast<std::uint32_t>(b.center.z),std::bit_cast<std::uint32_t>(b.radius),
        std::bit_cast<std::uint32_t>(b.extents.x),std::bit_cast<std::uint32_t>(b.extents.y),std::bit_cast<std::uint32_t>(b.extents.z)};
}
void SameProof(const Mistwatch::RootSupportProof& a,const Mistwatch::RootSupportProof& b){
    Need(a.profile==b.profile&&a.result==b.result&&
        std::bit_cast<std::uint32_t>(a.minimumLowerGap)==std::bit_cast<std::uint32_t>(b.minimumLowerGap)&&
        std::bit_cast<std::uint32_t>(a.maximumFootGap)==std::bit_cast<std::uint32_t>(b.maximumFootGap)&&
        a.sourceFootFaces==b.sourceFootFaces&&a.sourceFootTriangles==b.sourceFootTriangles&&
        a.sourceFootSegments==b.sourceFootSegments&&a.sourceFootPoints==b.sourceFootPoints&&
        a.sourceRimSegments==b.sourceRimSegments&&a.sourceNonmanifoldEdges==b.sourceNonmanifoldEdges&&
        a.lowerFaces==b.lowerFaces&&a.sourceMeshFingerprint==b.sourceMeshFingerprint&&
        a.derivedMeshFingerprint==b.derivedMeshFingerprint&&a.sourceFootFingerprint==b.sourceFootFingerprint,
        "deterministic full support proof");
}
std::uint64_t StandSeal(const S::Plan& p){
    std::uint64_t h=14695981039346656037ull;Word(h,static_cast<std::uint32_t>(p.combined.size()));
    for(const auto& pose:p.combined)for(float f:Fields(pose))Word(h,std::bit_cast<std::uint32_t>(f));
    for(auto b:p.addedBands)Word(h,b);return h;
}
std::uint64_t MeshSeal(const Mesh3D& mesh){
    std::uint64_t h=14695981039346656037ull;
    Word(h,static_cast<unsigned>(mesh.vertices.size()));Word(h,static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})Word(h,std::bit_cast<std::uint32_t>(f));Word(h,v.normal);Word(h,v.color);}
    for(auto i:mesh.indices)Word(h,i);return h;
}
MeshBounds3D ActualFirUnion(const std::filesystem::path& root){
    // 包含四個 LOD2 材質 slot 與 near twig 的全部真實頂點；不用盒子或假樹代替。
    constexpr std::array<const char*,4> names{"bark","twig","dead-branches","trunk-c"};
    Vec3 lo{10000,10000,10000},hi{-10000,-10000,-10000};
    for(unsigned slot=0;slot<5;++slot){const unsigned part=slot==4?1:slot;const std::string lod=slot==4?"1":"2";
        const auto path=root/"mistwatch-forest"/"meshes"/("fir-tree-01-c-lod"+lod+"-node0-mesh0-primitive"+
            std::to_string(part)+"-material"+std::to_string(part)+"-fir-tree-01-"+names[part]+".aemesh");
        const auto loaded=LoadAevryndMeshFile(path);Need(bool(loaded)&&loaded.mesh.Valid(),"real fir source missing");
        for(const auto& v:loaded.mesh.vertices){Need(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"nonfinite fir source");
            lo.x=(std::min)(lo.x,v.x);lo.y=(std::min)(lo.y,v.y);lo.z=(std::min)(lo.z,v.z);
            hi.x=(std::max)(hi.x,v.x);hi.y=(std::max)(hi.y,v.y);hi.z=(std::max)(hi.z,v.z);}
    }
    const auto extents=(hi-lo)*.5f;return {(hi+lo)*.5f,Length(extents),extents};
}
constexpr std::array<Q::AssetPin,3> Pins{{
    {"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive0-material0-tree-small-02-branches.aemesh",163400,
        "4182C1D2D3E726F5C77DD84ADF59EA5D4E022D49B1F211B34B488DBC165B68CC"},
    {"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh",3704316,
        "C46ECA29C84D6EA9DB320444D01ED4F16403A2A7F50729178E70C7886B28BC5B"},
    {"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive2-material2-tree-small-02-trunk.aemesh",57760,
        "1C765173EBC17A3BE167C35EF1B8DF8329FB4CF98F468F4DA13FD42533B677FD"}}};
void ResolverControls(){
    Need(M::Profile=="mixed-edge-canopy-v1","literal profile");
    for(bool capture:{false,true})for(bool replay:{false,true}){
        Need(!M::Resolve(capture,replay,L"")&&!M::Resolve(capture,replay,L"original"),"default changed");
        if(capture!=replay)Need(M::Resolve(capture,replay,L"mixed-edge-canopy-v1"),"explicit mode rejected");
        else Reject([&]{(void)M::Resolve(capture,replay,L"mixed-edge-canopy-v1");},"MIXED_CANOPY_REQUIRED_CONTEXT");
        for(auto value:{L"Mixed-edge-canopy-v1",L"mixed-edge-canopy-v2",L" mixed-edge-canopy-v1",L"mixed-edge-canopy-v1 ",L"1",L"stand",L"authored-source-foot-v1"})
            Reject([&]{(void)M::Resolve(capture,replay,value);},"MIXED_CANOPY_PROFILE");
    }
    const std::wstring embedded(L"mixed-edge-canopy-v1\0suffix",26);
    Reject([&]{(void)M::Resolve(true,false,embedded);},"MIXED_CANOPY_PROFILE");
}
void StandControls(const HeightField3D& terrain,const std::vector<Transform3D>& base,
    const MeshBounds3D& bounds,const S::Plan& stand){
    const auto check=[&](const S::Plan& p){M::ValidateStand(terrain,base,bounds,p);};
    check(stand);auto bad=stand;bad.complete=false;
    Reject([&]{check(bad);},"MIXED_STAND_INCOMPLETE");
    bad=stand;bad.combined.pop_back();Reject([&]{check(bad);},"MIXED_STAND_COUNT");
    bad=stand;bad.combined.push_back(stand.combined.back());Reject([&]{check(bad);},"MIXED_STAND_COUNT");
    bad=stand;bad.addedBands.pop_back();Reject([&]{check(bad);},"MIXED_STAND_COUNT");
    bad=stand;bad.addedBands.push_back(0);Reject([&]{check(bad);},"MIXED_STAND_COUNT");
    bad=stand;bad.sourceFingerprint^=1;Reject([&]{check(bad);},"MIXED_STAND_FINGERPRINT");
    bad=stand;bad.fingerprint^=1;Reject([&]{check(bad);},"MIXED_STAND_FINGERPRINT");
    bad=stand;++bad.generated;Reject([&]{check(bad);},"MIXED_STAND_GENERATED");
    bad=stand;bad.addedBands[0]=6;Reject([&]{check(bad);},"MIXED_STAND_BANDS");
    bad=stand;bad.addedBands[0]=1;Reject([&]{check(bad);},"MIXED_STAND_BANDS");
    constexpr std::array<unsigned S::Counts::*,6> counters{&S::Counts::proposals,&S::Counts::accepted,
        &S::Counts::slope,&S::Counts::core,&S::Counts::edge,&S::Counts::spacing};
    for(unsigned band=0;band<6;++band)for(auto field:counters){
        bad=stand;++(bad.bands[band].*field);Reject([&]{check(bad);},"MIXED_STAND_COUNTERS");
    }
    // 原始前綴與新增尾端各九項 float 均必須逐位比較，即使攻擊者留下原封印也不可通過。
    for(unsigned index:{0u,519u,520u,615u})for(unsigned component=0;component<9;++component){
        bad=stand;auto& p=bad.combined[index];std::array<float*,9> fields{&p.position.x,&p.position.y,&p.position.z,
            &p.rotation.x,&p.rotation.y,&p.rotation.z,&p.scale.x,&p.scale.y,&p.scale.z};
        *fields[component]=std::nextafter(*fields[component],(std::numeric_limits<float>::infinity)());
        Reject([&]{check(bad);},"MIXED_STAND_POSE");
    }
    bad=stand;bad.combined.back().position.x+=.01f;bad.fingerprint=StandSeal(bad);
    Reject([&]{check(bad);},"MIXED_STAND_FINGERPRINT");
    bad=stand;bad.combined.back().position.y=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{check(bad);},"MIXED_STAND_POSE");
    bad=S::Create(terrain,base,bounds,0);bad.complete=true;
    Reject([&]{check(bad);},"MIXED_STAND_COUNT");
    auto changed=base;changed[0].rotation.y+=.01f;
    Reject([&]{M::ValidateStand(terrain,changed,bounds,stand);},"STAND_SOURCE_POSE");
    changed=base;changed.pop_back();
    Reject([&]{M::ValidateStand(terrain,changed,bounds,stand);},"STAND_SOURCE_COUNT");
    const HeightField3D wrong;
    Reject([&]{M::ValidateStand(wrong,base,bounds,stand);},"STAND_TERRAIN_LAYOUT");
    auto invalid=bounds;invalid.extents.x=0;
    Reject([&]{M::ValidateStand(terrain,base,invalid,stand);},"STAND_UNION_BOUNDS");
    invalid=bounds;invalid.radius=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{M::ValidateStand(terrain,base,invalid,stand);},"STAND_UNION_BOUNDS");
    invalid=bounds;invalid.extents={1000,1000,1000};invalid.radius=2000;
    Reject([&]{M::ValidateStand(terrain,base,invalid,stand);},"MIXED_STAND_CANONICAL_INCOMPLETE");
}
}
int wmain(int argc,wchar_t** argv){try{
    Need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","usage: --runtime-assets ABS_DIRECTORY");
    const std::filesystem::path root(argv[2]);Need(root.is_absolute(),"absolute runtime assets required");
    ResolverControls();
    const auto bounds=ActualFirUnion(root),savedBounds=bounds;
    const auto terrain=Mistwatch::CreateTerrain();const auto base=Mistwatch::LandscapeTrees(terrain),savedBase=base;
    const auto stand=S::Create(terrain,base,bounds);const auto savedStand=stand;
    Need(stand.complete&&stand.combined.size()==616&&stand.addedBands.size()==96&&stand.fingerprint==StandSeal(stand),"canonical stand");
    StandControls(terrain,base,bounds,stand);
    std::array<Mesh3D,3> sources;std::array<std::uint64_t,3> sourceSeals{};
    for(unsigned slot=0;slot<3;++slot){
        sources[slot]=Q::Detail::LoadPinned(root/Pins[slot].path,Pins[slot]);Mistwatch::RootBroadleafMesh(sources[slot]);
        sourceSeals[slot]=MeshSeal(sources[slot]);
        Need(sourceSeals[slot]==Mistwatch::AuthoredLandscapeSourceFingerprints[slot],"real rooted source identity");
    }
    // 使用正式地形產生器與岩壁材質覆蓋結果；本 CPU 組合測試不冒稱 Main/GPU/美術驗收。
    const auto originalSurface=Mistwatch::CreateLandscapeMesh(terrain,true,Mistwatch::ForestFloorMaskSelector::ForestWood,Mistwatch::BankCoverageSelector::Patchy);
    const auto finalSurface=Mistwatch::CliffCoverage::Create(originalSurface,terrain,Mistwatch::CliffCoverage::Domain::Ring).mesh;
    const auto surfaceSeal=MeshSeal(finalSurface);
    Need(surfaceSeal!=MeshSeal(originalSurface),"actual final coverage not exercised");
    auto badStand=stand;badStand.complete=false;
    Reject([&]{(void)M::Create(terrain,base,bounds,badStand,finalSurface,sources);},"MIXED_STAND_INCOMPLETE");
    auto badSurface=finalSurface;badSurface.vertices[0].u=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{(void)M::Create(terrain,base,bounds,stand,badSurface,sources);},"AUTHORED_LANDSCAPE_SURFACE_FIELD");
    auto badSources=sources;badSources[0].vertices[0].color^=1u;
    Reject([&]{(void)M::Create(terrain,base,bounds,stand,finalSurface,badSources);},"AUTHORED_LANDSCAPE_MESH_SEAL");
    std::cout<<"MIXED_CANOPY_CPU_PLAN_BEGIN native=false GPU=false art=false\n"<<std::flush;
    const auto plan=M::Create(terrain,base,bounds,stand,finalSurface,sources);
    std::cout<<"MIXED_CANOPY_CPU_REPEAT_BEGIN\n"<<std::flush;
    const auto again=M::Create(terrain,base,bounds,stand,finalSurface,sources);
    Need(plan.complete&&again.complete&&plan.plants.size()==16&&again.plants.size()==16,"full 16 mapping");
    Need(plan.originalPoseFingerprint==11953736207267797409ull&&plan.originalPoseFingerprint==again.originalPoseFingerprint&&
        plan.surfaceFingerprint==surfaceSeal&&again.surfaceFingerprint==surfaceSeal&&plan.sourceFingerprints==sourceSeals&&
        again.sourceFingerprints==sourceSeals&&plan.renderFingerprint==again.renderFingerprint&&
        plan.derivedFingerprint==again.derivedFingerprint,"deterministic full identities");
    std::array<bool,520> selected{};
    for(unsigned i=0;i<16;++i){const auto& a=plan.plants[i];const auto& b=again.plants[i];
        Need(a.sourceOrdinal==Mistwatch::AuthoredLandscapeOrdinals[i]&&a.sourceOrdinal<520&&!selected[a.sourceOrdinal],"unique original ordinal");
        selected[a.sourceOrdinal]=true;
        Need(a.sourceOrdinal==b.sourceOrdinal&&SamePose(a.pose,b.pose)&&MeshSeal(a.derivedTrunk)==MeshSeal(b.derivedTrunk),"deterministic mapping and derived mesh");
        Need(BoundsWords(a.bounds)==BoundsWords(b.bounds),"deterministic full tree bounds");SameProof(a.proof,b.proof);
        Need(a.proof.result==Mistwatch::RootSupportResult::Safe&&b.proof.result==Mistwatch::RootSupportResult::Safe&&
            a.proof.sourceMeshFingerprint==sourceSeals[2]&&a.proof.derivedMeshFingerprint==MeshSeal(a.derivedTrunk)&&
            a.proof.sourceFootFingerprint==17965770522934596195ull,"true rooted support proof");
        Need(a.pose.position.x==base[a.sourceOrdinal].position.x&&a.pose.position.z==base[a.sourceOrdinal].position.z&&
            a.pose.scale.x==1.05f&&a.pose.scale.y==1.05f&&a.pose.scale.z==1.05f,"separate render mapping");
    }
    for(unsigned i=0;i<616;++i)Need(SamePose(stand.combined[i],savedStand.combined[i]),"stand pose mutated");
    for(unsigned i=0;i<520;++i)Need(SamePose(base[i],savedBase[i]),"base mutated");
    Need(stand.fingerprint==savedStand.fingerprint&&StandSeal(stand)==savedStand.fingerprint&&stand.addedBands==savedStand.addedBands,
        "original 616 seal changed");
    M::ValidateStand(terrain,base,savedBounds,stand);
    for(unsigned i=0;i<3;++i)Need(MeshSeal(sources[i])==sourceSeals[i],"source mutated");
    Need(MeshSeal(finalSurface)==surfaceSeal,"final surface mutated");
    std::cout<<"MIXED_CANOPY_CPU_RESULT profile="<<M::Profile<<" base=520 added=96 total=616 replacements=16 negatives="<<negatives
        <<" stand_fingerprint="<<stand.fingerprint<<" surface_fingerprint="<<surfaceSeal<<" render_fingerprint="<<plan.renderFingerprint
        <<" native_passed=false GPU_passed=false art_passed=false status=WIP\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"MIXED_CANOPY_CPU_FAIL "<<e.what()<<'\n';return 1;}}
