#include "MistwatchAuthoredLandscapePlanting.h"
#include "fallen/MistwatchAuthoredWorldQualification.h"
#include "MistwatchForestHabitatFloor.h"
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace Aevrynd;using namespace Mistwatch;
namespace Q=::Mistwatch::AuthoredQualification;namespace W=::Mistwatch::FallenWorldSeat;namespace D=::Mistwatch::LandscapeSpeciesDetail;
namespace {
unsigned negatives{};
void Require(bool value,const char* label){if(!value)throw std::runtime_error(label);}
void Word(std::uint64_t& h,std::uint32_t value){for(unsigned i=0;i<4;++i){h^=(value>>(8*i))&255u;h*=1099511628211ull;}}
std::uint64_t Identity(const Mesh3D& mesh){std::uint64_t h=14695981039346656037ull;
    Word(h,static_cast<unsigned>(mesh.vertices.size()));Word(h,static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})Word(h,std::bit_cast<std::uint32_t>(f));Word(h,v.normal);Word(h,v.color);}
    for(const auto i:mesh.indices)Word(h,i);return h;
}
std::array<std::uint32_t,9> PoseWords(const Transform3D& p){return{
    std::bit_cast<std::uint32_t>(p.position.x),std::bit_cast<std::uint32_t>(p.position.y),std::bit_cast<std::uint32_t>(p.position.z),
    std::bit_cast<std::uint32_t>(p.rotation.x),std::bit_cast<std::uint32_t>(p.rotation.y),std::bit_cast<std::uint32_t>(p.rotation.z),
    std::bit_cast<std::uint32_t>(p.scale.x),std::bit_cast<std::uint32_t>(p.scale.y),std::bit_cast<std::uint32_t>(p.scale.z)};}
std::array<std::uint32_t,7> BoundsWords(const MeshBounds3D& b){return{std::bit_cast<std::uint32_t>(b.center.x),std::bit_cast<std::uint32_t>(b.center.y),
    std::bit_cast<std::uint32_t>(b.center.z),std::bit_cast<std::uint32_t>(b.radius),std::bit_cast<std::uint32_t>(b.extents.x),
    std::bit_cast<std::uint32_t>(b.extents.y),std::bit_cast<std::uint32_t>(b.extents.z)};}
std::uint64_t SourceIdentity(std::span<const Transform3D> roots){std::uint64_t h=14695981039346656037ull;Word(h,static_cast<unsigned>(roots.size()));
    for(unsigned i=0;i<roots.size();++i){Word(h,i);for(const auto w:PoseWords(roots[i]))Word(h,w);}return h;}
template<class Action>void Reject(Action action,const char* label){bool rejected{};
    try{action();}catch(const std::invalid_argument& e){Require(std::string_view(e.what())==label,"AUTHORED_LANDSCAPE_WRONG_NEGATIVE_REASON");rejected=true;}
    Require(rejected,"AUTHORED_LANDSCAPE_NEGATIVE_ACCEPTED");++negatives;std::cout<<"AUTHORED_LANDSCAPE_CONTROL reason="<<label<<'\n'<<std::flush;
}
constexpr std::array<std::uint32_t,16> ExpectedOrdinals{177,212,496,429,519,436,423,410,37,256,465,205,48,171,58,455};
constexpr std::array<std::uint64_t,3> ExpectedSources{12442856667128630539ull,11046199509515305128ull,12155645616207841595ull};
// 來自 m2432 已封存作者輸入；只作 CPU 回歸基準，不冒稱 Main/GPU 證書。
constexpr std::array<std::uint64_t,16> ExpectedDerived{
    16633552231186724724ull,837436015637420646ull,8350550293930311536ull,10977264126315131883ull,
    4303723205706517558ull,1153389365095245433ull,9689005468799460144ull,10150239788622762402ull,
    8609539485105110787ull,14625692966124409171ull,15898706439770552689ull,5964158133103210919ull,
    15811333325597017665ull,4687599156396709410ull,8417697898108922335ull,8113443408190402464ull};
constexpr std::array<std::uint32_t,16> ExpectedYWords{
    1050345381u,1096015741u,1093728836u,1061753418u,1094258866u,1083936365u,1101161342u,1100501092u,
    1100384570u,1100164728u,1095937194u,1105205326u,1105344409u,1095619384u,1100457333u,1105703522u};
constexpr std::array<std::uint32_t,16> ExpectedMinimumWords{
    2983950944u,3133952612u,3030079915u,2984054176u,3032556608u,880939264u,898891008u,3039832277u,
    896234944u,3049100821u,879009877u,3108151066u,3022999040u,880809515u,872242517u,896251904u};
constexpr std::array<std::uint32_t,16> ExpectedMaximumWords{
    1024566522u,1024561021u,1024539761u,1024811549u,1024486920u,1025467122u,1024475364u,1024516502u,
    1024496266u,1024582474u,1024905157u,1024743963u,1024590530u,1024733950u,1024523406u,1024553089u};
constexpr std::array<Q::AssetPin,3> Pins{{
    {"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive0-material0-tree-small-02-branches.aemesh",163400,
        "4182C1D2D3E726F5C77DD84ADF59EA5D4E022D49B1F211B34B488DBC165B68CC"},
    {"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh",3704316,
        "C46ECA29C84D6EA9DB320444D01ED4F16403A2A7F50729178E70C7886B28BC5B"},
    {"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive2-material2-tree-small-02-trunk.aemesh",57760,
        "1C765173EBC17A3BE167C35EF1B8DF8329FB4CF98F468F4DA13FD42533B677FD"}}};
std::vector<W::Placement> FixedMappings(){std::vector<W::Placement> out;
    // 只用既有正式固定倒木來源建立 Main context；不執行或冒稱四倒木 full-overlay 資格。
    for(const auto& fixed:FallenLogPlacements()){W::Placement p;p.propId=fixed.propId;p.anchor=fixed.authoredAnchor;p.pose=fixed.pose;p.collider=fixed.collider;out.push_back(std::move(p));}return out;
}
void Controls(const std::vector<Transform3D>& roots,const Mesh3D& landscape,const std::array<Mesh3D,3>& sources){
    auto badRoots=roots;badRoots.pop_back();Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(badRoots,landscape,sources);},"AUTHORED_LANDSCAPE_SOURCE_COUNT");
    badRoots=roots;badRoots[0].position.x=std::numeric_limits<float>::quiet_NaN();Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(badRoots,landscape,sources);},"AUTHORED_LANDSCAPE_SOURCE_POSE");
    badRoots=roots;badRoots[0].rotation.x=.1f;Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(badRoots,landscape,sources);},"AUTHORED_LANDSCAPE_SOURCE_POSE");
    badRoots=roots;badRoots[0].position.x=0;badRoots[0].position.z=0;Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(badRoots,landscape,sources);},"AUTHORED_LANDSCAPE_SOURCE_DOMAIN");
    badRoots=roots;badRoots[1].position=badRoots[0].position;Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(badRoots,landscape,sources);},"AUTHORED_LANDSCAPE_SOURCE_SPACING");
    badRoots=roots;std::swap(badRoots[0],badRoots[1]);Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(badRoots,landscape,sources);},"AUTHORED_LANDSCAPE_SOURCE_SEAL");
    auto badMesh=landscape;badMesh.vertices[0].u=std::numeric_limits<float>::quiet_NaN();Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(roots,badMesh,sources);},"AUTHORED_LANDSCAPE_SURFACE_FIELD");
    badMesh=landscape;badMesh.vertices[0].u=2;Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(roots,badMesh,sources);},"AUTHORED_LANDSCAPE_MATERIAL_DOMAIN");
    badMesh=landscape;badMesh.indices[0]=static_cast<std::uint32_t>(badMesh.vertices.size());Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(roots,badMesh,sources);},"AUTHORED_LANDSCAPE_SURFACE_TOPOLOGY");
    badMesh=landscape;badMesh.vertices[0].y=std::nextafter(badMesh.vertices[0].y,(std::numeric_limits<float>::infinity)());
    Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(roots,badMesh,sources);},"AUTHORED_LANDSCAPE_SURFACE_GEOMETRY");
    auto badSources=sources;badSources[2].vertices.pop_back();Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(roots,landscape,badSources);},"AUTHORED_LANDSCAPE_MESH_COUNTS");
    badSources=sources;badSources[1].vertices[0].u=std::nextafter(badSources[1].vertices[0].u,(std::numeric_limits<float>::infinity)());
    Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(roots,landscape,badSources);},"AUTHORED_LANDSCAPE_MESH_SEAL");
    badSources=sources;badSources[0].vertices[0].normal^=1u;Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(roots,landscape,badSources);},"AUTHORED_LANDSCAPE_MESH_SEAL");
    badSources=sources;for(auto& mesh:badSources)RootBroadleafMesh(mesh);Reject([&]{(void)CreateAuthoredLandscapePlantingPlan(roots,landscape,badSources);},"AUTHORED_LANDSCAPE_MESH_SEAL");
}
void VerifyDerived(const Mesh3D& source,const Mesh3D& derived){Require(source.indices==derived.indices&&source.vertices.size()==derived.vertices.size(),"AUTHORED_LANDSCAPE_TEST_DERIVED_TOPOLOGY");
    std::vector<Vec3> normals(derived.vertices.size());std::vector<bool> touched(derived.vertices.size());
    const auto same=[](float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);};
    for(unsigned f=0;f<derived.indices.size();f+=3){const auto a=derived.indices[f],b=derived.indices[f+1],c=derived.indices[f+2];
        const auto n=Cross(D::Vertex(derived,b)-D::Vertex(derived,a),D::Vertex(derived,c)-D::Vertex(derived,a));Require(D::Finite(n)&&Length(n)>0,"AUTHORED_LANDSCAPE_TEST_DERIVED_FACE");
        normals[a]=normals[a]+n;normals[b]=normals[b]+n;normals[c]=normals[c]+n;
        if(!same(source.vertices[a].y,derived.vertices[a].y)||!same(source.vertices[b].y,derived.vertices[b].y)||!same(source.vertices[c].y,derived.vertices[c].y))touched[a]=touched[b]=touched[c]=true;}
    for(unsigned i=0;i<source.vertices.size();++i){const auto& a=source.vertices[i];const auto& b=derived.vertices[i];
        Require(same(a.x,b.x)&&same(a.z,b.z)&&same(a.u,b.u)&&same(a.v,b.v)&&a.color==b.color&&(a.normal&0xff000000u)==(b.normal&0xff000000u),"AUTHORED_LANDSCAPE_TEST_MATERIAL_XZ");
        if(a.y>=.35f)Require(same(a.y,b.y),"AUTHORED_LANDSCAPE_TEST_UPPER_POSITION");
        if(touched[i])Require(b.normal==PackNormalSnorm8(Normalize(normals[i]),UnpackNormalSnorm8(a.normal).w),"AUTHORED_LANDSCAPE_TEST_ADJACENT_NORMAL");
        else Require(a.normal==b.normal,"AUTHORED_LANDSCAPE_TEST_UNTOUCHED_NORMAL");}
}
MeshBounds3D IndependentBounds(const std::array<Mesh3D,3>& sources,const Mesh3D& derived,const Transform3D& pose){
    Vec3 low{10000,10000,10000},high{-10000,-10000,-10000},worldLow=low,worldHigh=high;const auto matrix=pose.Matrix();
    for(unsigned slot=0;slot<3;++slot){const auto& mesh=slot==2?derived:sources[slot];for(const auto& v:mesh.vertices){
        low.x=(std::min)(low.x,v.x);low.y=(std::min)(low.y,v.y);low.z=(std::min)(low.z,v.z);
        high.x=(std::max)(high.x,v.x);high.y=(std::max)(high.y,v.y);high.z=(std::max)(high.z,v.z);
        const auto world=TransformPoint(matrix,{v.x,v.y,v.z});worldLow.x=(std::min)(worldLow.x,world.x);worldLow.z=(std::min)(worldLow.z,world.z);
        worldHigh.x=(std::max)(worldHigh.x,world.x);worldHigh.z=(std::max)(worldHigh.z,world.z);}}
    Require(worldHigh.x< -53.f||worldLow.x>53.f||worldHigh.z< -23.f||worldLow.z>103.f,"AUTHORED_LANDSCAPE_TEST_ALL_WORLD_VERTICES_OUTSIDE");
    const auto half=(high-low)*.5f;return TransformBounds({(low+high)*.5f,Length(half),half},pose);
}
void CompareProof(const RootSupportProof& a,const RootSupportProof& b){Require(a.profile==b.profile&&a.result==b.result&&
    std::bit_cast<std::uint32_t>(a.minimumLowerGap)==std::bit_cast<std::uint32_t>(b.minimumLowerGap)&&
    std::bit_cast<std::uint32_t>(a.maximumFootGap)==std::bit_cast<std::uint32_t>(b.maximumFootGap)&&
    a.sourceFootFaces==b.sourceFootFaces&&a.sourceFootTriangles==b.sourceFootTriangles&&a.sourceFootSegments==b.sourceFootSegments&&a.sourceFootPoints==b.sourceFootPoints&&
    a.sourceRimSegments==b.sourceRimSegments&&a.sourceNonmanifoldEdges==b.sourceNonmanifoldEdges&&a.lowerFaces==b.lowerFaces&&
    a.sourceMeshFingerprint==b.sourceMeshFingerprint&&a.derivedMeshFingerprint==b.derivedMeshFingerprint&&a.sourceFootFingerprint==b.sourceFootFingerprint,
    "AUTHORED_LANDSCAPE_TEST_REFERENCE_PROOF_BITS");}
}
int wmain(int argc,wchar_t** argv){try{
    Require(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","USAGE --runtime-assets ABS");const auto root=std::filesystem::path(argv[2]).lexically_normal();
    Require(root.is_absolute(),"AUTHORED_LANDSCAPE_TEST_RUNTIME_ROOT");std::array<Mesh3D,3> sources;
    for(unsigned slot=0;slot<3;++slot){sources[slot]=Q::Detail::LoadPinned(root/Pins[slot].path,Pins[slot]);RootBroadleafMesh(sources[slot]);
        Require(Identity(sources[slot])==ExpectedSources[slot]&&AuthoredLandscapeSourceFingerprints[slot]==ExpectedSources[slot],"AUTHORED_LANDSCAPE_TEST_TRUE_SOURCE_FNV");
        std::cout<<"AUTHORED_LANDSCAPE_SOURCE slot="<<slot<<" source_fingerprint="<<Identity(sources[slot])<<" raw_sha="<<Pins[slot].sha<<" rooted_once=true\n"<<std::flush;}
    Require(AuthoredLandscapeOrdinals==ExpectedOrdinals&&AuthoredLandscapePlantingProfile=="authored-source-foot-v1","AUTHORED_LANDSCAPE_TEST_LITERAL_PROFILE");
    const auto terrain=CreateTerrain();const auto roots=LandscapeTrees(terrain),rootsBefore=roots;Require(SourceIdentity(roots)==11953736207267797409ull,"AUTHORED_LANDSCAPE_TEST_LITERAL_SOURCE");
    std::cout<<"AUTHORED_LANDSCAPE_MAIN_MATERIAL_CONTEXT_BEGIN native=false GPU=false art=false\n"<<std::flush;
    W::Budget budget;const auto assets=Q::LoadAssets(root);const auto sourceContext=Q::CreateSourceContext(terrain,assets,budget);
    const auto context=Q::CreateContext(terrain,assets,sourceContext,FixedMappings(),budget);
    Require(context.author.firs.roots.size()==95&&context.canopy.plants.size()==8,"AUTHORED_LANDSCAPE_TEST_FINAL_HABITAT_ROOTS");
    std::vector<Vec3> canopyRoots;for(const auto& plant:context.canopy.plants)canopyRoots.push_back(plant.pose.position);
    auto landscape=CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    const auto habitat=ApplyForestHabitatFloor(landscape,terrain,{context.author.firs.roots,canopyRoots},true,ForestHabitatFloorProfile::Original);
    const auto surfaceSeal=Identity(landscape);Require(surfaceSeal!=2121963744324747060ull,"AUTHORED_LANDSCAPE_TEST_MATERIAL_NOT_CANONICAL");
    std::cout<<"AUTHORED_LANDSCAPE_MAIN_MATERIAL_CONTEXT_READY surface_fingerprint="<<surfaceSeal<<" habitat_fingerprint="<<habitat.fingerprint
        <<" fir_roots=95 canopy_roots=8 profile=Original native=false GPU=false art=false\n"<<std::flush;
    Controls(roots,landscape,sources);
    std::cout<<"AUTHORED_LANDSCAPE_16_PLAN_BEGIN\n"<<std::flush;const auto plan=CreateAuthoredLandscapePlantingPlan(roots,landscape,sources);
    Require(plan.complete&&plan.plants.size()==16&&plan.originalPoseFingerprint==11953736207267797409ull&&plan.surfaceFingerprint==surfaceSeal&&
        plan.sourceFingerprints==ExpectedSources,"AUTHORED_LANDSCAPE_TEST_COMPLETE_IDENTITIES");
    std::array<bool,520> selected{};std::uint64_t derivedSeal=14695981039346656037ull;
    for(unsigned i=0;i<16;++i){const auto& plant=plan.plants[i];Require(plant.sourceOrdinal==ExpectedOrdinals[i]&&!selected[plant.sourceOrdinal],"AUTHORED_LANDSCAPE_TEST_FIXED_ORDER");
        selected[plant.sourceOrdinal]=true;auto expected=roots[ExpectedOrdinals[i]];expected.position.y=std::bit_cast<float>(ExpectedYWords[i]);expected.scale={1.05f,1.05f,1.05f};
        Require(PoseWords(plant.pose)==PoseWords(expected),"AUTHORED_LANDSCAPE_TEST_LITERAL_POSE_WORDS");VerifyDerived(sources[2],plant.derivedTrunk);
        Require(Identity(plant.derivedTrunk)==ExpectedDerived[i]&&plant.proof.derivedMeshFingerprint==ExpectedDerived[i]&&
            plant.proof.sourceMeshFingerprint==ExpectedSources[2]&&plant.proof.sourceFootFingerprint==17965770522934596195ull&&
            plant.proof.profile=="source-material-foot-v1"&&plant.proof.result==RootSupportResult::Safe&&plant.proof.sourceRimSegments==5&&
            plant.proof.sourceNonmanifoldEdges==51&&plant.proof.sourceFootFaces==19&&plant.proof.sourceFootTriangles==25&&
            std::bit_cast<std::uint32_t>(plant.proof.minimumLowerGap)==ExpectedMinimumWords[i]&&
            std::bit_cast<std::uint32_t>(plant.proof.maximumFootGap)==ExpectedMaximumWords[i],"AUTHORED_LANDSCAPE_TEST_LITERAL_TRUE_PROOF");
        Require(BoundsWords(plant.bounds)==BoundsWords(IndependentBounds(sources,plant.derivedTrunk,plant.pose)),"AUTHORED_LANDSCAPE_TEST_COMPLETE_BOUNDS");
        for(unsigned j=0;j<i;++j){const auto& b=plan.plants[j].bounds;const auto& a=plant.bounds;
            Require(std::abs(a.center.x-b.center.x)>a.extents.x+b.extents.x||std::abs(a.center.z-b.center.z)>a.extents.z+b.extents.z,"AUTHORED_LANDSCAPE_TEST_FULL_CROWN_OVERLAP");}
        Word(derivedSeal,i);Word(derivedSeal,plant.sourceOrdinal);Word(derivedSeal,static_cast<std::uint32_t>(ExpectedDerived[i]));Word(derivedSeal,static_cast<std::uint32_t>(ExpectedDerived[i]>>32));
        std::cout<<"AUTHORED_LANDSCAPE_ROW slot="<<i<<" source_ordinal="<<plant.sourceOrdinal<<" pose_words=";for(const auto word:PoseWords(plant.pose))std::cout<<word<<',';
        std::cout<<" mesh_fingerprint="<<Identity(plant.derivedTrunk)<<" source_foot_fingerprint="<<plant.proof.sourceFootFingerprint
            <<" vertices="<<plant.derivedTrunk.vertices.size()<<" indices="<<plant.derivedTrunk.indices.size()
            <<" minimum_lower_word="<<ExpectedMinimumWords[i]<<" maximum_foot_word="<<ExpectedMaximumWords[i]<<" bounds_words=";
        for(const auto word:BoundsWords(plant.bounds))std::cout<<word<<',';std::cout<<'\n'<<std::flush;}
    const D::Surface surface(landscape);const auto& first=plan.plants.front();const auto repeated=TerrainPlantingDetail::RootTrunkToLandscape(sources[2],first.pose,surface);
    Require(Identity(repeated)==ExpectedDerived[0],"AUTHORED_LANDSCAPE_TEST_FIRST_DERIVE_REPEAT");
    CompareProof(RootSupportGround(first.pose,sources[2],first.derivedTrunk,surface,false),first.proof);
    std::uint64_t renderSeal=14695981039346656037ull;unsigned untouched{};
    for(unsigned i=0;i<520;++i){Require(PoseWords(roots[i])==PoseWords(rootsBefore[i]),"AUTHORED_LANDSCAPE_TEST_ORIGINAL_END_WORDS");
        Word(renderSeal,i);Word(renderSeal,selected[i]?1u:0u);const auto found=std::find_if(plan.plants.begin(),plan.plants.end(),[&](const auto& plant){return plant.sourceOrdinal==i;});
        const auto& p=selected[i]?found->pose:roots[i];for(const auto word:PoseWords(p))Word(renderSeal,word);if(!selected[i])++untouched;
        for(unsigned j=0;j<i;++j)Require(std::hypot(roots[i].position.x-roots[j].position.x,roots[i].position.z-roots[j].position.z)>=5.f,"AUTHORED_LANDSCAPE_TEST_ALL_ROOT_SPACING");}
    Require(untouched==504&&renderSeal==plan.renderFingerprint&&derivedSeal==plan.derivedFingerprint&&Identity(landscape)==surfaceSeal&&
        SourceIdentity(roots)==11953736207267797409ull,"AUTHORED_LANDSCAPE_TEST_END_IDENTITIES");
    for(unsigned slot=0;slot<3;++slot){Require(Identity(sources[slot])==ExpectedSources[slot],"AUTHORED_LANDSCAPE_TEST_SOURCE_END_FNV");
        const auto rawAgain=Q::Detail::LoadPinned(root/Pins[slot].path,Pins[slot]);Require(rawAgain.vertices.size()==sources[slot].vertices.size(),"AUTHORED_LANDSCAPE_TEST_RAW_SOURCE_END");}
    std::cout<<"AUTHORED_LANDSCAPE_PLANTING_CPU_PASS profile=authored-source-foot-v1 plants=16 untouched=504 negatives="<<negatives
        <<" original_pose_fingerprint="<<plan.originalPoseFingerprint<<" surface_fingerprint="<<plan.surfaceFingerprint
        <<" render_fingerprint="<<plan.renderFingerprint<<" derived_fingerprint="<<plan.derivedFingerprint
        <<" first_reference=true material_recipe=forest-wood-patchy-authored-habitat-original native=false GPU=false art=false full_qualification=false\n";return 0;
}catch(const std::exception& e){std::cerr<<"AUTHORED_LANDSCAPE_PLANTING_FIRST_DIVERGENCE "<<e.what()<<" native=false GPU=false art=false\n";return 1;}}
