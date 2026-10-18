#include "MistwatchShoulderGrove.h"
#include "MistwatchLandscapeStandFill.h"
#include <Aevrynd/MeshLoader.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
using namespace Aevrynd;
namespace G=Mistwatch::ShoulderGrove;
namespace {
unsigned checks{},negatives{};
void Need(bool condition,const char* error){++checks;if(!condition)throw std::runtime_error(error);}
template<class F>void Reject(F action,std::string_view expected){bool rejected=false;try{action();}catch(const std::invalid_argument& e){rejected=std::string_view(e.what())==expected;}
    Need(rejected,"NEGATIVE_WRONG_RESULT");++negatives;}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool SameVertex(const Vertex3D& a,const Vertex3D& b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&Same(a.u,b.u)&&Same(a.v,b.v)&&a.normal==b.normal&&a.color==b.color;}
void Export(const std::filesystem::path& directory,const G::Plan& plan){std::filesystem::create_directories(directory);
    for(const auto& plant:plan.plants){std::ofstream file(directory/(std::to_string(plant.sourceOrdinal)+".meshwords"),std::ios::binary);
        const auto word=[&](std::uint32_t w){for(unsigned i=0;i<4;++i)file.put(static_cast<char>((w>>(8*i))&255u));};
        word(static_cast<std::uint32_t>(plant.derivedTrunk.vertices.size()));word(static_cast<std::uint32_t>(plant.derivedTrunk.indices.size()));
        for(const auto& v:plant.derivedTrunk.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})word(std::bit_cast<std::uint32_t>(f));word(v.normal);word(v.color);}
        for(auto index:plant.derivedTrunk.indices)word(index);Need(bool(file),"EXPORT_FAILED");}
}
}
int main(int argc,char** argv){try{
    Need((argc==3||argc==5)&&std::string_view(argv[1])=="--runtime-assets","USAGE --runtime-assets ABS [--export ABS]");
    const std::filesystem::path assets(argv[2]);std::array<Mesh3D,3> sources;
    for(unsigned s=0;s<3;++s){auto loaded=LoadAevryndMeshFile(assets/"mistwatch-broadleaf/meshes"/Mistwatch::BroadleafMeshName(s));Need(bool(loaded),"SOURCE_MISSING");sources[s]=std::move(loaded.mesh);Mistwatch::RootBroadleafMesh(sources[s]);}
    Vec3 minimum{10000,10000,10000},maximum{-10000,-10000,-10000};constexpr const char* names[]{"bark","twig","dead-branches","trunk-c"};
    for(unsigned s=0;s<5;++s){const unsigned part=s==4?1:s;const std::string lod=s==4?"1":"2";
        auto loaded=LoadAevryndMeshFile(assets/"mistwatch-forest/meshes"/("fir-tree-01-c-lod"+lod+"-node0-mesh0-primitive"+std::to_string(part)+"-material"+std::to_string(part)+"-fir-tree-01-"+names[part]+".aemesh"));Need(bool(loaded),"FIR_MISSING");
        for(const auto& v:loaded.mesh.vertices){minimum.x=(std::min)(minimum.x,v.x);minimum.y=(std::min)(minimum.y,v.y);minimum.z=(std::min)(minimum.z,v.z);maximum.x=(std::max)(maximum.x,v.x);maximum.y=(std::max)(maximum.y,v.y);maximum.z=(std::max)(maximum.z,v.z);}}
    const auto extents=(maximum-minimum)*.5f;const MeshBounds3D bounds{(maximum+minimum)*.5f,Length(extents),extents};
    const auto terrain=Mistwatch::CreateTerrain();const auto stand=Mistwatch::LandscapeStandFill::Create(terrain,Mistwatch::LandscapeTrees(terrain),bounds);
    Need(stand.complete&&stand.combined.size()==616,"CANONICAL_POPULATION");const auto& original=stand.combined;auto ground=Mistwatch::CreateLandscapeMesh(terrain,true);
    const auto began=std::chrono::steady_clock::now();const auto plan=G::Create(original,ground,sources);const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();
    Need(plan.complete&&plan.plants.size()==16&&plan.retained.size()==600,"PLAN_COUNTS");
    Need(plan.sourcePoseFingerprint==1933729647080683908ull&&plan.surfaceGeometryFingerprint==17244599893343078654ull&&plan.surfaceFingerprint==2121963744324747060ull,"FROZEN_IDENTITY");
    constexpr std::array<unsigned,16> ordinals{235,6,436,573,433,100,66,545,575,391,541,571,285,578,566,579};
    constexpr std::array<std::uint64_t,16> seals{8305683904997595336ull,4576343100416652527ull,17658589159608056674ull,18314462051744997052ull,9523901141517119521ull,4568797487356781801ull,11284198039482448281ull,12390619038603015289ull,14111920704987928494ull,14732817429382224190ull,2026199826059131453ull,1059522342294624709ull,10284864402819725348ull,2716571659489332128ull,1610727810827736688ull,8339766140566148889ull};
    std::set<unsigned> unique;std::array<unsigned,4> quota{};unsigned standReplacements=0;
    for(std::size_t n=0;n<16;++n){const auto& p=plan.plants[n];Need(p.sourceOrdinal==ordinals[n]&&unique.insert(p.sourceOrdinal).second,"FROZEN_ORDINAL_ORDER");
        Need(p.derivedFingerprint==seals[n]&&plan.derivedFingerprints[n]==seals[n],"FROZEN_MESH_SEAL");++quota[p.region];standReplacements+=p.sourceOrdinal>=520;
        Need(G::IsSelected(plan,p.sourceOrdinal),"SELECTED_MAP");const auto& m=p.derivedTrunk;
        for(std::size_t v=0;v<sources[2].vertices.size();++v){const auto& a=sources[2].vertices[v];const auto& b=m.vertices[v];
            Need(Same(a.x,b.x)&&Same(a.z,b.z)&&Same(a.u,b.u)&&Same(a.v,b.v)&&a.color==b.color,"SOURCE_MATERIAL_XZ_IDENTITY");
            if(a.y>=1.f)Need(SameVertex(a,b),"HIGH_PREFIX_BIT_EXACT");}
        Need((m.vertices[799].normal&0x00ffffffu)!=0,"AUTHORED_NORMAL_799_NONZERO");}
    Need(quota==std::array<unsigned,4>{4,4,4,4}&&standReplacements==8,"QUOTA_AND_STAND_MAPPING");
    unsigned retained=0;for(unsigned n=0;n<616;++n){Need(G::IsSelected(plan,n)==unique.contains(n),"SELECTED_EXACT");if(!unique.contains(n)){
        const auto& p=plan.retained[retained++];Need(p.sourceOrdinal==n&&G::Detail::PoseWords(p.pose)==G::Detail::PoseWords(original[n]),"RETAINED_600_ORDER_POSE");}}
    // 地表外觀允許更新，接地幾何不變；完整指紋仍須追蹤真正上傳的屬性。
    auto materialGround=ground;materialGround.vertices[0].u+=.125f;materialGround.vertices[0].normal^=1u;materialGround.vertices[0].color^=1u;
    const auto materialPlan=G::Create(original,materialGround,sources);Need(materialPlan.surfaceGeometryFingerprint==plan.surfaceGeometryFingerprint&&materialPlan.surfaceFingerprint!=plan.surfaceFingerprint,"MATERIAL_GEOMETRY_IDENTITY_SEPARATION");
    auto roots=original;roots.pop_back();Reject([&]{(void)G::Create(roots,ground,sources);},"SHOULDER_POPULATION_COUNT");
    roots=original;roots[0].position.x+=.01f;Reject([&]{(void)G::Create(roots,ground,sources);},"SHOULDER_POPULATION_POSE_ORDER");
    roots=original;std::swap(roots[0],roots[1]);Reject([&]{(void)G::Create(roots,ground,sources);},"SHOULDER_POPULATION_POSE_ORDER");
    for(unsigned s=0;s<3;++s){auto bad=sources;bad[s].vertices[0].x+=.001f;Reject([&]{(void)G::Create(original,ground,bad);},"SHOULDER_SOURCE_SEAL");}
    {auto bad=sources;bad[2].vertices[0].u+=.001f;Reject([&]{(void)G::Create(original,ground,bad);},"SHOULDER_SOURCE_SEAL");}
    {auto bad=ground;bad.vertices[0].y+=.001f;Reject([&]{(void)G::Create(original,bad,sources);},"SHOULDER_TERRAIN_GEOMETRY");}
    {auto bad=ground;std::swap(bad.indices[0],bad.indices[1]);Reject([&]{(void)G::Create(original,bad,sources);},"SHOULDER_TERRAIN_GEOMETRY");}
    {auto bad=plan;bad.plants[1].sourceOrdinal=bad.plants[0].sourceOrdinal;Reject([&]{G::Validate(bad,original,ground,sources);},"SHOULDER_DUPLICATE_ORDINAL");}
    {auto bad=plan;bad.plants[0].derivedTrunk.vertices[799].normal=0x7f000000u;Reject([&]{G::Validate(bad,original,ground,sources);},"SHOULDER_DERIVED_NORMAL");}
    {auto bad=plan;bad.plants[0].derivedTrunk.vertices[799].y+=.001f;Reject([&]{G::Validate(bad,original,ground,sources);},"SHOULDER_DERIVED_SEAL");}
    {auto bad=plan;bad.retained[0].pose.position.x+=.001f;Reject([&]{G::Validate(bad,original,ground,sources);},"SHOULDER_RETAINED_IDENTITY");}
    {auto bad=plan;bad.selected[0]=true;Reject([&]{G::Validate(bad,original,ground,sources);},"SHOULDER_QUOTA_SELECTION");}
    {auto bad=plan;bad.plants[0].region=1;Reject([&]{G::Validate(bad,original,ground,sources);},"SHOULDER_PLANT_RECIPE");}
    Reject([&]{(void)G::IsSelected(plan,616);},"SHOULDER_ORDINAL_RANGE");
    Reject([&]{G::Validate(plan,original,materialGround,sources);},"SHOULDER_PLAN_IDENTITY");
    Need(!G::DefaultEnabled&&G::Profile=="shoulder-deformed-root-v1","EXPLICIT_CANDIDATE_ONLY");
    if(argc==5){Need(std::string_view(argv[3])=="--export","EXPORT_ARGUMENT");Export(argv[4],plan);}
    std::cout<<"SHOULDER_RECIPE_CPU_PASS checks="<<checks<<" negatives="<<negatives<<" plants=16 retained=600 create_seconds="<<elapsed<<" candidate_only=1\n";return 0;
}catch(const std::exception& e){std::cerr<<"SHOULDER_RECIPE_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}}
