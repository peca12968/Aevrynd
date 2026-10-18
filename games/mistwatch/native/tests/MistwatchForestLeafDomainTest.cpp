#include "MistwatchForestLeafDomain.h"
#include "MistwatchForestHabitatFloor.h"
#include "MistwatchWestCliffRelief.h"
#include "MistwatchCliffCoverage.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace Aevrynd;
namespace F=Mistwatch::ForestLeafDomain;
namespace S=Mistwatch::SettlementSurface;
unsigned checks{},negatives{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Geometry(const Vertex3D& a,const Vertex3D& b){
    return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z)&&a.normal==b.normal&&a.color==b.color;
}
bool Vertex(const Vertex3D& a,const Vertex3D& b){return Geometry(a,b)&&Bits(a.u,b.u)&&Bits(a.v,b.v);}
bool Mesh(const Mesh3D& a,const Mesh3D& b){
    return a.indices==b.indices&&a.vertices.size()==b.vertices.size()&&
        std::equal(a.vertices.begin(),a.vertices.end(),b.vertices.begin(),Vertex);
}
void Mix(std::uint64_t& h,std::uint32_t word){for(unsigned b=0;b<4;++b){h^=(word>>(8*b))&255u;h*=1099511628211ull;}}
std::uint64_t Full(const Mesh3D& mesh){
    std::uint64_t h=14695981039346656037ull;
    Mix(h,static_cast<std::uint32_t>(mesh.vertices.size()));Mix(h,static_cast<std::uint32_t>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z})Mix(h,std::bit_cast<std::uint32_t>(f));
        Mix(h,v.normal);Mix(h,v.color);Mix(h,std::bit_cast<std::uint32_t>(v.u));Mix(h,std::bit_cast<std::uint32_t>(v.v));}
    for(auto index:mesh.indices)Mix(h,index);return h;
}
bool Split(const S::Result& a,const S::Result& b){
    return Mesh(a.forest,b.forest)&&Mesh(a.entrance,b.entrance)&&Mesh(a.gravel,b.gravel)&&
        a.sourceDomains==b.sourceDomains&&a.gravelSourceTriangles==b.gravelSourceTriangles&&
        a.gravelBoundaryVertices==b.gravelBoundaryVertices&&a.fingerprints==b.fingerprints;
}
S::Result Make(const Mesh3D& source){
    const auto entrance=Mistwatch::CreateEntranceRoadDomain(source);
    const auto settlement=Mistwatch::SettlementGround::Create(source,entrance);
    return S::Create(source,entrance,settlement);
}
void Reseal(S::Result& split){split.fingerprints={Full(split.forest),Full(split.entrance),Full(split.gravel)};}
bool Receipt(const F::Receipt& a,const F::Receipt& b){
    return a.sourceOriginal==b.sourceOriginal&&a.sourceLeaf==b.sourceLeaf&&
        a.beforeFingerprint==b.beforeFingerprint&&a.afterFingerprint==b.afterFingerprint&&
        a.candidateFingerprint==b.candidateFingerprint&&a.referenced==b.referenced&&a.changed==b.changed&&
        a.seams==b.seams&&a.unusedPreserved==b.unusedPreserved&&a.bridgePreserved==b.bridgePreserved&&
        a.boundaryPreserved==b.boundaryPreserved;
}
template<class Work>void Reject(Work work,const char* label){
    bool bad=false;try{work();}catch(const std::invalid_argument&){bad=true;}
    Need(bad,label);++negatives;
}
template<class Work>void RejectExact(Work work,std::string_view reason){
    bool bad=false;try{work();}catch(const std::invalid_argument& error){
        Need(std::string_view(error.what())==reason,"wrong policy rejection reason");bad=true;}
    Need(bad,"invalid policy accepted");++negatives;
}
void Policies(){
    const std::wstring leaf=L"leaf-coverage-strength-v1";
    for(bool capture:{false,true})for(bool replay:{false,true}){
        Need(!F::Resolve(capture,replay,L""),"blank policy changes original context");
        Need(!F::Resolve(capture,replay,L"original"),"original policy changes context");
        if(capture!=replay)Need(F::Resolve(capture,replay,leaf),"candidate requires exact isolated context");
        else RejectExact([&]{(void)F::Resolve(capture,replay,leaf);},"LEAF_DOMAIN_ISOLATED_CONTEXT");
    }
    const std::vector<std::wstring> invalid{L"unknown",L"Original",L"leaf-coverage-strength",L"leaf-coverage-strength-v2",
        L"LEAF-COVERAGE-STRENGTH-V1",L" leaf-coverage-strength-v1",L"leaf-coverage-strength-v1 ",
        L"leaf-coverage-strength-v1-extra",L"leaf-coverage-strength-v1\n",L"original\t",L" ",L"\r",L"\x1f",
        std::wstring(65536,L'x'),leaf+std::wstring(1,L'\0'),std::wstring(L"original\0suffix",15)};
    for(const auto& name:invalid)for(bool capture:{false,true})for(bool replay:{false,true})
        RejectExact([&]{(void)F::Resolve(capture,replay,std::wstring_view(name.data(),name.size()));},"LEAF_DOMAIN_PROFILE");
}
using Triangle=std::array<std::uint32_t,3>;
Triangle At(const Mesh3D& mesh,std::size_t t){return {mesh.indices[t*3],mesh.indices[t*3+1],mesh.indices[t*3+2]};}
struct Incidence {std::vector<unsigned> mask;std::size_t referenced{},seams{},unused{},bridge{},boundary{};};
Incidence IndependentlyQualify(const Mesh3D& source,const S::Result& split){
    std::map<Triangle,std::size_t> originals;
    for(std::size_t t=0;t<source.indices.size()/3;++t)Need(originals.emplace(At(source,t),t).second,"source triangle unique");
    std::vector<unsigned> seen(source.indices.size()/3);
    Incidence out;out.mask.resize(source.vertices.size());
    const std::array<const Mesh3D*,3> meshes{&split.forest,&split.entrance,&split.gravel};
    Need(split.sourceDomains.size()==seen.size(),"source domain metadata count");
    for(unsigned domain=0;domain<meshes.size();++domain){const auto& mesh=*meshes[domain];
        Need(mesh.Valid()&&mesh.vertices.size()==source.vertices.size(),"domain full source vertex table");
        Need(split.fingerprints[domain]==Full(mesh),"independent full seal");
        for(std::size_t i=0;i<source.vertices.size();++i){
            Need(Geometry(mesh.vertices[i],source.vertices[i])&&Bits(mesh.vertices[i].v,source.vertices[i].v),"domain source geometry and rock bits");
            Vec2 encoded;Need(TryEncodeTerrainLayerCoverage3D(mesh.vertices[i].u,mesh.vertices[i].v,encoded),"normalized domain coverage");
        }
        bool first=true;std::size_t prior{};
        for(std::size_t t=0;t<mesh.indices.size()/3;++t){
            const auto tri=At(mesh,t);const auto found=originals.find(tri);Need(found!=originals.end(),"exact source triangle ordering and winding");
            const auto id=found->second;Need(++seen[id]==1,"domain triangles disjoint");
            Need(first||id>prior,"domain relative source order");first=false;prior=id;
            Need(static_cast<unsigned>(split.sourceDomains[id])==domain,"actual domain metadata");
            if(domain==2)Need(t<split.gravelSourceTriangles.size()&&split.gravelSourceTriangles[t]==id,"actual gravel source ID");
            for(auto vertex:tri)out.mask[vertex]|=1u<<domain;
        }
    }
    Need(std::all_of(seen.begin(),seen.end(),[](auto n){return n==1;}),"complete source partition");
    Need(split.gravelSourceTriangles.size()==split.gravel.indices.size()/3,"gravel metadata size");
    std::vector<std::uint32_t> gravelBoundary;
    for(std::size_t i=0;i<out.mask.size();++i){const auto& v=source.vertices[i];const unsigned m=out.mask[i];
        if((m&4u)&&(m&3u))gravelBoundary.push_back(static_cast<std::uint32_t>(i));
        if(!(m&1u)){++out.unused;continue;}
        ++out.referenced;out.bridge+=v.z>32.f&&v.z<40.f;
        out.boundary+=std::abs(v.x)==50.f||v.z==-20.f||v.z==100.f;
        if(m&6u){++out.seams;
            if(m&2u)Need(Vertex(split.forest.vertices[i],split.entrance.vertices[i]),"forest entrance seam exact fields");
            if(m&4u)Need(Vertex(split.forest.vertices[i],split.gravel.vertices[i]),"forest gravel seam exact fields");
        }
    }
    Need(gravelBoundary==split.gravelBoundaryVertices&&!gravelBoundary.empty(),"independent gravel boundary metadata");
    Need(out.referenced&&out.seams&&out.unused&&out.bridge&&out.boundary,"nonempty independent incidence classes");
    return out;
}
std::size_t Find(const Mesh3D& mesh,const auto& predicate){
    for(std::size_t i=0;i<mesh.vertices.size();++i)if(predicate(i,mesh.vertices[i]))return i;
    throw std::runtime_error("required test vertex missing");
}
void PerturbSoil(Vertex3D& v){
    const float before=v.u;
    v.u=v.u>0.f?v.u*.5f:(1.f-v.v)*.5f;
    Need(!Bits(before,v.u),"negative mutation must change bits");
}
void NegativeCases(const Mesh3D& a,const Mesh3D& b,const S::Result& x,const S::Result& y,const Incidence& incidence){
    const auto source=[&](auto mutate,const char* why){auto bad=b;mutate(bad);const auto preserved=bad;
        Reject([&]{(void)F::Create(a,bad,x,y);},why);Need(Mesh(bad,preserved),"rejected source modified");};
    source([](auto& m){m.vertices[0].x=std::nextafter(m.vertices[0].x,0.f);},"source X bit mutation accepted");
    source([](auto& m){m.vertices[0].y=std::nextafter(m.vertices[0].y,std::numeric_limits<float>::infinity());},"source Y bit mutation accepted");
    source([](auto& m){m.vertices[0].z=std::nextafter(m.vertices[0].z,0.f);},"source Z bit mutation accepted");
    source([](auto& m){m.vertices[0].normal^=1u;},"source normal mutation accepted");
    source([](auto& m){m.vertices[0].color^=1u;},"source color mutation accepted");
    source([](auto& m){m.vertices[0].v=std::nextafter(m.vertices[0].v,1.f);},"source rock mutation accepted");
    const auto positive=Find(a,[](auto,const auto& v){return v.u>.01f;});
    source([&](auto& m){m.vertices[positive].u=a.vertices[positive].u*.5f;},"source soil decrease accepted");
    source([](auto& m){m.vertices[0].u=std::numeric_limits<float>::quiet_NaN();},"source NaN accepted");
    source([](auto& m){m.vertices[0].u=std::numeric_limits<float>::infinity();},"source infinite UV accepted");
    source([](auto& m){m.vertices[0].u=-.1f;},"source negative soil accepted");
    source([](auto& m){m.vertices[0].u=1.01f;},"source soil overflow accepted");
    const auto rock=Find(b,[](auto,const auto& v){return v.v>0.f;});
    source([&](auto& m){m.vertices[rock].u=1.f;},"source coverage sum overflow accepted");
    source([](auto& m){m.vertices.pop_back();},"source vertex count accepted");
    source([](auto& m){m.indices.pop_back();},"source index count accepted");
    source([](auto& m){m.indices[0]=m.indices[1];},"source triangle mutation accepted");
    source([](auto& m){std::swap(m.indices[0],m.indices[1]);},"source winding mutation accepted");
    source([](auto& m){m.vertices.resize(100001,m.vertices.front());},"source vertex budget overflow accepted");
    source([](auto& m){m.indices.resize(600003,0);},"source index budget overflow accepted");
    Reject([&]{auto bad=a;bad.vertices.front().u=std::numeric_limits<float>::quiet_NaN();(void)F::Create(bad,b,x,y);},"original source NaN accepted");
    Reject([&]{(void)F::Create(Mesh3D{},b,x,y);},"empty original source accepted");
    const auto bridge=Find(b,[](auto,const auto& v){return v.z>32.f&&v.z<40.f&&v.u+v.v<.98f;});
    const auto boundary=Find(b,[](auto,const auto& v){return (std::abs(v.x)==50.f||v.z==-20.f||v.z==100.f)&&v.u+v.v<.98f;});
    source([&](auto& m){m.vertices[bridge].u+=(1.f-m.vertices[bridge].v-m.vertices[bridge].u)*.25f;},"protected bridge source accepted");
    source([&](auto& m){m.vertices[boundary].u+=(1.f-m.vertices[boundary].v-m.vertices[boundary].u)*.25f;},"protected border source accepted");
    const auto mutateSplit=[&](bool original,auto mutate,const char* why){
        auto bad=original?x:y;mutate(bad);const auto preserved=bad;
        Reject([&]{if(original)(void)F::Create(a,b,bad,y);else (void)F::Create(a,b,x,bad);},why);
        Need(Split(bad,preserved),"rejected split mesh or metadata modified");
    };
    for(bool original:{false,true}){
        for(unsigned slot=0;slot<3;++slot)mutateSplit(original,[&](auto& s){s.fingerprints[slot]^=1u;},"forged split seal accepted");
        mutateSplit(original,[](auto& s){s.sourceDomains.pop_back();},"wrong domain metadata count accepted");
        mutateSplit(original,[](auto& s){s.sourceDomains[0]=static_cast<S::Domain>(255);},"invalid domain enum accepted");
        mutateSplit(original,[](auto& s){s.sourceDomains[0]=S::Domain::Entrance;},"wrong valid domain membership accepted");
        mutateSplit(original,[](auto& s){s.gravelSourceTriangles.front()^=1u;},"forged gravel source ID accepted");
        mutateSplit(original,[](auto& s){s.gravelSourceTriangles.pop_back();},"missing gravel source ID accepted");
        mutateSplit(original,[](auto& s){s.gravelBoundaryVertices.pop_back();},"missing gravel boundary accepted");
        mutateSplit(original,[](auto& s){s.gravelBoundaryVertices.front()=0;},"foreign gravel boundary accepted");
        mutateSplit(original,[](auto& s){s.forest.indices.pop_back();Reseal(s);},"resealed forest index count accepted");
        mutateSplit(original,[](auto& s){s.forest.indices[0]=s.forest.indices[1];Reseal(s);},"resealed corrupt forest triangle accepted");
        mutateSplit(original,[](auto& s){s.entrance.indices[0]=s.entrance.indices[1];Reseal(s);},"resealed corrupt road triangle accepted");
        mutateSplit(original,[](auto& s){s.gravel.indices[0]=s.gravel.indices[1];Reseal(s);},"resealed corrupt gravel triangle accepted");
        mutateSplit(original,[](auto& s){s.gravel.indices[0]=static_cast<std::uint32_t>(s.gravel.vertices.size());Reseal(s);},"resealed out-of-range gravel index accepted");
        mutateSplit(original,[](auto& s){s.forest.vertices.pop_back();Reseal(s);},"resealed wrong vertex table accepted");
        const auto roadRef=x.entrance.indices.front(),gravelRef=x.gravel.indices.front();
        mutateSplit(original,[&](auto& s){PerturbSoil(s.entrance.vertices[roadRef]);Reseal(s);},"resealed referenced road UV corruption accepted");
        mutateSplit(original,[&](auto& s){s.gravel.vertices[gravelRef].color^=1u;Reseal(s);},"resealed referenced gravel color corruption accepted");
        const auto seam=Find(a,[&](auto i,const auto&){return (incidence.mask[i]&1u)&&(incidence.mask[i]&6u);});
        mutateSplit(original,[&](auto& s){PerturbSoil(s.forest.vertices[seam]);Reseal(s);},"resealed seam corruption accepted");
        mutateSplit(original,[](auto& s){s.forest.vertices.front().u=std::numeric_limits<float>::quiet_NaN();Reseal(s);},"resealed split NaN accepted");
    }
    Reject([&]{(void)F::Create(a,b,y,x);},"original/candidate split swap accepted");
    RejectExact([&]{(void)F::Create(a,a,x,x);},"LEAF_DOMAIN_NO_GAIN_OR_PROTECTION");
    // 道路內部 source U 雖有增加，該域會覆寫 referenced U；森林未引用差異不能假算增益。
    auto unusedOnly=a;
    const auto unused=Find(a,[&](auto i,const auto& v){return incidence.mask[i]==2u&&v.u+v.v<.95f;});
    unusedOnly.vertices[unused].u+=.25f*(1.f-unusedOnly.vertices[unused].v-unusedOnly.vertices[unused].u);
    Need(unusedOnly.vertices[unused].u>a.vertices[unused].u,"unused-only source gain exists");
    const auto unusedSplit=Make(unusedOnly);
    Need(!Bits(unusedSplit.forest.vertices[unused].u,x.forest.vertices[unused].u),"unused candidate buffer really differs");
    for(std::size_t i=0;i<incidence.mask.size();++i){
        if(incidence.mask[i]&1u)Need(Vertex(x.forest.vertices[i],unusedSplit.forest.vertices[i]),"unused fixture must not change referenced forest");
        if(incidence.mask[i]&2u)Need(Vertex(x.entrance.vertices[i],unusedSplit.entrance.vertices[i]),"unused fixture road reference overwritten");
        if(incidence.mask[i]&4u)Need(Vertex(x.gravel.vertices[i],unusedSplit.gravel.vertices[i]),"unused fixture gravel reference preserved");
    }
    RejectExact([&]{(void)F::Create(a,unusedOnly,x,unusedSplit);},"LEAF_DOMAIN_NO_GAIN_OR_PROTECTION");
}
void FinalSurface(const HeightField3D& terrain,const Mesh3D& original,const Mesh3D& leaf,const Incidence& incidence){
    const auto canonicalBefore=terrain;
    auto derived=Mistwatch::WestCliffRelief::Build(terrain);
    const auto derivedBefore=derived.terrain;
    auto old=original,candidate=leaf;
    const auto oldReceipt=Mistwatch::WestCliffRelief::UpdateMesh(old,terrain,derived.terrain);
    const auto leafReceipt=Mistwatch::WestCliffRelief::UpdateMesh(candidate,terrain,derived.terrain);
    Need(oldReceipt.changedVertices==leafReceipt.changedVertices&&oldReceipt.changedPositions==leafReceipt.changedPositions&&
        oldReceipt.changedNormals==leafReceipt.changedNormals&&oldReceipt.changedColors==leafReceipt.changedColors,"same relief mapping receipt");
    const auto covered=Mistwatch::CliffCoverage::Create(old,derived.terrain,Mistwatch::CliffCoverage::Domain::Core);
    const auto leafCovered=Mistwatch::CliffCoverage::Create(candidate,derived.terrain,Mistwatch::CliffCoverage::Domain::Core);
    Need(covered.mesh.indices==leafCovered.mesh.indices&&covered.mesh.vertices.size()==leafCovered.mesh.vertices.size(),"final cliff topology equality");
    Need(covered.receipt.afterFingerprint==Full(covered.mesh)&&leafCovered.receipt.afterFingerprint==Full(leafCovered.mesh),"actual final full receipts");
    Need(covered.receipt.afterGeometryFingerprint==leafCovered.receipt.afterGeometryFingerprint,"final exact geometry FP equality");
    std::size_t finalGain{},changedY{};
    for(std::size_t i=0;i<old.vertices.size();++i){const auto& a=covered.mesh.vertices[i];const auto& b=leafCovered.mesh.vertices[i];
        Need(Geometry(a,b)&&Bits(a.v,b.v),"final cliff geometry or rock changed");
        Need(b.u>=a.u,"final cliff soil decreased");Vec2 encoded;
        Need(TryEncodeTerrainLayerCoverage3D(a.u,a.v,encoded)&&TryEncodeTerrainLayerCoverage3D(b.u,b.v,encoded),"final normalized coverage");
        changedY+=!Bits(original.vertices[i].y,a.y);
        if((incidence.mask[i]&1u)&&b.u>a.u)++finalGain;
        if(!(incidence.mask[i]&1u)||(incidence.mask[i]&6u)||(a.z>32.f&&a.z<40.f)||
            std::abs(a.x)==50.f||a.z==-20.f||a.z==100.f)Need(Vertex(a,b),"final unused/seam/bridge/border changed");
    }
    Need(finalGain>0&&changedY>0,"true leaf gain survives actual relief and cliff");
    for(unsigned z=0;z<terrain.Settings().depth;++z)for(unsigned x=0;x<terrain.Settings().width;++x){
        Need(Bits(terrain.Height(x,z),canonicalBefore.Height(x,z)),"canonical terrain mutated");
        Need(Bits(derived.terrain.Height(x,z),derivedBefore.Height(x,z)),"derived terrain mutated");
    }
    std::cout<<"FOREST_LEAF_FINAL_CPU observed_gain="<<finalGain<<" relief_changed_y="<<changedY
        <<" rock_preserved=1 geometry_preserved=1 native=0 art=0\n";
}
void Fixture(const HeightField3D& terrain,Mistwatch::ForestHabitatRoots roots,unsigned number){
    const auto base=Mistwatch::CreateTerrainMesh(terrain,true,Mistwatch::ForestFloorMaskSelector::ForestWood,Mistwatch::BankCoverageSelector::Patchy);
    auto original=base,leaf=base;
    (void)Mistwatch::ApplyForestHabitatFloor(original,terrain,roots,false,Mistwatch::ForestHabitatFloorProfile::Original);
    (void)Mistwatch::ApplyForestHabitatFloor(leaf,terrain,roots,false,Mistwatch::ForestHabitatFloorProfile::LeafCoverageStrengthV1);
    const auto originalSaved=original,leafSaved=leaf;
    const auto old=Make(original),candidate=Make(leaf),oldSaved=old,candidateSaved=candidate;
    const auto incidence=IndependentlyQualify(original,old),leafIncidence=IndependentlyQualify(leaf,candidate);
    Need(incidence.mask==leafIncidence.mask,"leaf cannot change partition membership");
    const auto actual=F::Create(original,leaf,old,candidate),again=F::Create(original,leaf,old,candidate);
    Need(Mesh(actual.mesh,again.mesh)&&Receipt(actual.receipt,again.receipt),"repeated output and all receipt fields bits");
    Need(actual.mesh.indices==old.forest.indices&&actual.mesh.vertices.size()==old.forest.vertices.size(),"actual forest topology");
    std::size_t changed{},unusedDifferent{};
    for(std::size_t i=0;i<original.vertices.size();++i){const auto& a=old.forest.vertices[i];const auto& b=actual.mesh.vertices[i];const auto& c=candidate.forest.vertices[i];
        Need(Geometry(a,b)&&Bits(a.v,b.v)&&b.u>=a.u,"actual forest only monotonic soil");
        Vec2 encoded;Need(TryEncodeTerrainLayerCoverage3D(b.u,b.v,encoded),"actual normalized forest");
        if(incidence.mask[i]&1u){
            Need(Vertex(b,c),"actual referenced forest must equal canonical leaf domain");
            changed+=!Bits(a.u,b.u);
            if(incidence.mask[i]&6u)Need(Vertex(a,b),"actual shared seam unchanged");
            if(a.z>32.f&&a.z<40.f)Need(Vertex(a,b),"actual referenced bridge unchanged");
            if(std::abs(a.x)==50.f||a.z==-20.f||a.z==100.f)Need(Vertex(a,b),"actual referenced border unchanged");
        }else{
            Need(Vertex(a,b),"actual unused forest vertex remains original");
            unusedDifferent+=!Bits(a.u,c.u);
        }
        if(incidence.mask[i]&2u)Need(Vertex(old.entrance.vertices[i],candidate.entrance.vertices[i]),"road referenced all fields equal");
        if(incidence.mask[i]&4u)Need(Vertex(old.gravel.vertices[i],candidate.gravel.vertices[i]),"gravel referenced all fields equal");
    }
    Need(changed>0&&unusedDifferent>0,"actual ref gain and unused-only difference both exercised");
    const auto& receipt=actual.receipt;
    Need(receipt.sourceOriginal==Full(original)&&receipt.sourceLeaf==Full(leaf)&&
        receipt.beforeFingerprint==Full(old.forest)&&receipt.afterFingerprint==Full(actual.mesh)&&
        receipt.candidateFingerprint==Full(candidate.forest),"independent source and actual output seals");
    Need(receipt.referenced==incidence.referenced&&receipt.changed==changed&&receipt.seams==incidence.seams&&
        receipt.unusedPreserved==incidence.unused,"independent actual count receipts");
    const auto bridgeCount=std::count_if(old.forest.vertices.begin(),old.forest.vertices.end(),[](const auto& v){return v.z>32.f&&v.z<40.f;});
    const auto boundaryCount=std::count_if(old.forest.vertices.begin(),old.forest.vertices.end(),[](const auto& v){return std::abs(v.x)==50.f||v.z==-20.f||v.z==100.f;});
    Need(receipt.bridgePreserved==std::size_t(bridgeCount)&&receipt.boundaryPreserved==std::size_t(boundaryCount),"independent whole-buffer protected receipts");
    Need(Mesh(original,originalSaved)&&Mesh(leaf,leafSaved)&&Split(old,oldSaved)&&Split(candidate,candidateSaved),"const input meshes and metadata changed");
    FinalSurface(terrain,old.forest,actual.mesh,incidence);
    auto ringOriginal=Mistwatch::CreateLandscapeMesh(terrain,true,Mistwatch::ForestFloorMaskSelector::ForestWood,Mistwatch::BankCoverageSelector::Patchy);
    auto ringLeaf=ringOriginal;
    (void)Mistwatch::ApplyForestHabitatFloor(ringOriginal,terrain,roots,true,Mistwatch::ForestHabitatFloorProfile::Original);
    (void)Mistwatch::ApplyForestHabitatFloor(ringLeaf,terrain,roots,true,Mistwatch::ForestHabitatFloorProfile::LeafCoverageStrengthV1);
    Need(Mesh(ringOriginal,ringLeaf),"leaf strength cannot alter helper distant ring");
    if(number==0)NegativeCases(original,leaf,old,candidate,incidence);
    Need(Mesh(original,originalSaved)&&Mesh(leaf,leafSaved)&&Split(old,oldSaved)&&Split(candidate,candidateSaved),"negative controls changed immutable inputs");
    std::cout<<"FOREST_LEAF_DOMAIN_FIXTURE id="<<number<<" referenced="<<incidence.referenced<<" changed="<<changed
        <<" seams="<<incidence.seams<<" unused_preserved="<<incidence.unused<<" unused_candidate_differences="<<unusedDifferent<<'\n';
}
}
int main(){try{
    Policies();
    const auto terrain=Mistwatch::CreateTerrain();
    const auto root=[&](float x,float z){return Vec3{x,terrain.SampleHeight(x,z),z};};
    const std::array<Vec3,2> fir0{root(-15,5),root(18,20)};
    const std::array<Vec3,1> canopy0{root(12,8)};
    const std::array<Vec3,3> fir1{root(-20,14),root(22,51),root(-16,76)};
    const std::array<Vec3,2> canopy1{root(-12,22),root(15,69)};
    Fixture(terrain,{fir0,canopy0},0);Fixture(terrain,{fir1,canopy1},1);
    std::cout<<"FOREST_LEAF_DOMAIN_CPU_CONTRACT_OK fixtures=2 checks="<<checks<<" negatives="<<negatives
        <<" native_verified=0 physical_verified=0 art_accepted=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"FOREST_LEAF_DOMAIN_FIRST_DIFFERENCE "<<error.what()<<'\n';return 1;}}
