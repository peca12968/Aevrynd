#include "MistwatchLandscapeSpecies.h"
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string_view>

using namespace Aevrynd;
using namespace Mistwatch;
namespace D=::Mistwatch::LandscapeSpeciesDetail;
namespace {
unsigned negatives{};
void Require(bool value,const char* label){if(!value)throw std::runtime_error(label);}
void Word(std::uint64_t& hash,std::uint32_t value){for(unsigned byte=0;byte<4;++byte){hash^=(value>>(8*byte))&255u;hash*=1099511628211ull;}}
std::array<std::uint32_t,9> Words(const Transform3D& p){return{std::bit_cast<std::uint32_t>(p.position.x),std::bit_cast<std::uint32_t>(p.position.y),
    std::bit_cast<std::uint32_t>(p.position.z),std::bit_cast<std::uint32_t>(p.rotation.x),std::bit_cast<std::uint32_t>(p.rotation.y),
    std::bit_cast<std::uint32_t>(p.rotation.z),std::bit_cast<std::uint32_t>(p.scale.x),std::bit_cast<std::uint32_t>(p.scale.y),std::bit_cast<std::uint32_t>(p.scale.z)};}
std::uint64_t RootIdentity(std::span<const Transform3D> roots){std::uint64_t h=14695981039346656037ull;Word(h,static_cast<unsigned>(roots.size()));
    for(unsigned i=0;i<roots.size();++i){Word(h,i);for(const auto w:Words(roots[i]))Word(h,w);}return h;}
std::uint64_t MeshIdentity(const Mesh3D& mesh){std::uint64_t h=14695981039346656037ull;Word(h,static_cast<unsigned>(mesh.vertices.size()));
    Word(h,static_cast<unsigned>(mesh.indices.size()));for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})Word(h,std::bit_cast<std::uint32_t>(f));
        Word(h,v.normal);Word(h,v.color);}for(const auto i:mesh.indices)Word(h,i);return h;}
std::uint64_t RenderIdentity(const LandscapeSpeciesPlan& plan){std::uint64_t h=14695981039346656037ull;
    for(const auto& p:plan.instances){Word(h,p.sourceOrdinal);Word(h,static_cast<unsigned>(p.species));for(const auto w:Words(p.render))Word(h,w);}return h;}
template<class Action>void Reject(Action action,const char* expected){bool rejected{};
    try{action();}catch(const std::invalid_argument& error){Require(std::string_view(error.what())==expected,"LANDSCAPE_SPECIES_NEGATIVE_WRONG_REASON");rejected=true;}
    Require(rejected,"LANDSCAPE_SPECIES_NEGATIVE_ACCEPTED");++negatives;
}
void Pin(const std::filesystem::path& path,std::size_t bytes,std::string_view sha){
    Require(std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)==bytes,"LANDSCAPE_SPECIES_ASSET_BYTES");
    std::ifstream in(path,std::ios::binary);Require(bool(in),"LANDSCAPE_SPECIES_ASSET_READ");std::vector<std::byte> raw(bytes);
    in.read(reinterpret_cast<char*>(raw.data()),static_cast<std::streamsize>(bytes));Require(bool(in),"LANDSCAPE_SPECIES_ASSET_READ");
    std::array<std::byte,32> digest;Require(WorldDeltaSha2563D(raw,digest),"LANDSCAPE_SPECIES_ASSET_SHA");
    std::ostringstream out;out<<std::hex<<std::setfill('0');for(const auto b:digest)out<<std::setw(2)<<std::to_integer<unsigned>(b);
    Require(out.str()==sha,"LANDSCAPE_SPECIES_ASSET_SHA_MISMATCH");
}
Mesh3D Plane(float slope=0){Mesh3D m;const auto normal=PackNormalSnorm8({0,1,0});const auto color=PackColorRgba8({1,1,1,1});
    for(Vec3 p:std::array<Vec3,4>{{{0,0,0},{0,0,2},{2,2*slope,0},{2,2*slope,2}}})m.vertices.push_back({p.x,p.y,p.z,normal,color,0,0});
    m.indices={0,1,2,2,1,3};return m;}
Mesh3D SmallTrunk(){Mesh3D m;const auto normal=PackNormalSnorm8({0,1,0});const auto color=PackColorRgba8({1,1,1,1});
    for(float y:{0.f,1.f})for(Vec3 p:std::array<Vec3,4>{{{-.2f,0,-.2f},{-.2f,0,.2f},{.2f,0,.2f},{.2f,0,-.2f}}})m.vertices.push_back({p.x,y,p.z,normal,color,0,0});
    for(unsigned i=0;i<4;++i){const auto next=(i+1)%4;m.indices.insert(m.indices.end(),{i,next,i+4,next,next+4,i+4});}return m;}
D::GroundProof PairedGround(const Transform3D& pose,const Mesh3D& trunk,const D::Surface& surface,D::GroundProof* referenceOutput=nullptr){
    const auto reference=D::Ground(pose,trunk,surface,false),optimized=D::Ground(pose,trunk,surface,true);
    if(referenceOutput)*referenceOutput=reference;
    Require(reference.result==optimized.result&&
        std::bit_cast<std::uint32_t>(reference.minimumLowerGap)==std::bit_cast<std::uint32_t>(optimized.minimumLowerGap)&&
        std::bit_cast<std::uint32_t>(reference.maximumBaseGap)==std::bit_cast<std::uint32_t>(optimized.maximumBaseGap),
        "LANDSCAPE_SPECIES_REFERENCE_PRUNED_RESULT");
    Require(reference.skippedLowerFaces==0&&optimized.evaluatedIntersections<=reference.evaluatedIntersections,
        "LANDSCAPE_SPECIES_REFERENCE_PRUNED_DIAGNOSTICS");return optimized;
}
void AppendFace(Mesh3D& mesh,std::array<Vec3,3> vertices){const auto first=static_cast<unsigned>(mesh.vertices.size());
    const auto n=PackNormalSnorm8({0,1,0}),c=PackColorRgba8({1,1,1,1});
    for(const auto v:vertices)mesh.vertices.push_back({v.x,v.y,v.z,n,c,0,0});mesh.indices.insert(mesh.indices.end(),{first,first+1,first+2});
}
void GeometryControls(){
    const auto plane=Plane();const D::Surface surface(plane);const auto trunk=SmallTrunk();Transform3D pose{{1,0,1},{},{1,1,1}};
    const auto safe=PairedGround(pose,trunk,surface);Require(safe.result==D::GroundResult::Safe&&safe.skippedLowerFaces>0,
        "LANDSCAPE_SPECIES_FLAT_GROUND_PRUNING_CONTROL");
    pose.position.y=.041f;Require(PairedGround(pose,trunk,surface).result==D::GroundResult::Ground,"LANDSCAPE_SPECIES_FLOAT_CONTROL");++negatives;
    pose.position.y=-.041f;Require(PairedGround(pose,trunk,surface).result==D::GroundResult::Ground,"LANDSCAPE_SPECIES_PENETRATION_CONTROL");++negatives;
    const auto slope=Plane(.30f);const D::Surface sloped(slope);pose.position.y=.30f;
    Require(PairedGround(pose,trunk,sloped).result==D::GroundResult::Slope,"LANDSCAPE_SPECIES_SLOPE_CONTROL");++negatives;
    pose.position={1,0,1};auto deeper=trunk;AppendFace(deeper,{{{.1f,-.03f,.1f},{.1f,.4f,.15f},{.15f,.4f,.1f}}});
    const auto deep=PairedGround(pose,deeper,surface);Require(deep.result==D::GroundResult::Safe&&deep.minimumLowerGap<-.029f,
        "LANDSCAPE_SPECIES_LATER_MINIMUM_PRUNED");
    auto verticalTrunk=trunk;AppendFace(verticalTrunk,{{{.1f,-.05f,.1f},{.1f,.3f,.1f},{.1f,.3f,.15f}}});
    Require(PairedGround(pose,verticalTrunk,surface).result==D::GroundResult::Ground,"LANDSCAPE_SPECIES_VERTICAL_PRUNING_CONTROL");++negatives;
    pose.position={.2f,0,.2f};Require(PairedGround(pose,trunk,surface).result==D::GroundResult::Safe,"LANDSCAPE_SPECIES_BOUNDARY_PRUNING_CONTROL");
    pose.position={0,0,0};Require(PairedGround(pose,trunk,surface).result==D::GroundResult::Ground,"LANDSCAPE_SPECIES_MISSING_SUPPORT_PRUNING_CONTROL");++negatives;
    pose.position={1,.006f,1};Require(PairedGround(pose,trunk,surface).result==D::GroundResult::Ground,"LANDSCAPE_SPECIES_BASE_MAXIMUM_NOT_PRUNED");++negatives;
    const D::T vertical{{D::Exact::ToExact({.5f,-.05f,.5f}),D::Exact::ToExact({.5f,.3f,.5f}),D::Exact::ToExact({.5f,.3f,1.5f})}};
    const auto verticalRange=D::Overlay(vertical,surface);
    Require(verticalRange.points&&verticalRange.minimum.Approximate()<-.049,"LANDSCAPE_SPECIES_VERTICAL_SEGMENT_MISSED");
    Mesh3D peak;const auto n=PackNormalSnorm8({0,1,0}),c=PackColorRgba8({1,1,1,1});
    for(unsigned z=0;z<3;++z)for(unsigned x=0;x<3;++x)peak.vertices.push_back({float(x),(x==1&&z==1)?1.f:0.f,float(z),n,c,0,0});
    for(unsigned z=0;z<2;++z)for(unsigned x=0;x<2;++x){const unsigned a=z*3+x;peak.indices.insert(peak.indices.end(),{a,a+3,a+1,a+1,a+3,a+4});}
    const D::Surface peakSurface(peak);const D::T bridge{{D::Exact::ToExact({.25f,.1f,.25f}),D::Exact::ToExact({1.75f,.1f,.25f}),D::Exact::ToExact({1.f,.1f,1.75f})}};
    const auto interior=D::Overlay(bridge,peakSurface);Require(interior.points&&interior.minimum.Approximate()<-.8,"LANDSCAPE_SPECIES_INTERIOR_PEAK_MISSED");++negatives;
    pose.position={1,0,1};Require(PairedGround(pose,trunk,peakSurface).result==D::GroundResult::Ground,"LANDSCAPE_SPECIES_INTERIOR_PEAK_PRUNING_CONTROL");++negatives;
    Require(!D::OutsideCore({{54,1,0},1,{2,1,2}})&&D::OutsideCore({{60,1,0},1,{2,1,2}}),"LANDSCAPE_SPECIES_COMPLETE_CROWN_CONTROL");++negatives;
    auto invalid=plane;invalid.indices[0]=100;
    Reject([&]{const D::Surface bad(invalid);(void)bad;},"LANDSCAPE_SPECIES_INVALID_MESH");
    invalid=plane;invalid.vertices[0].x=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{const D::Surface bad(invalid);(void)bad;},"LANDSCAPE_SPECIES_NONFINITE_MESH");
    invalid=plane;invalid.indices[1]=invalid.indices[0];
    Reject([&]{const D::Surface bad(invalid);(void)bad;},"LANDSCAPE_SPECIES_DEGENERATE_FACE");
    Reject([&]{(void)surface.At({3,0,3});},"LANDSCAPE_SPECIES_ROOT_SURFACE_MISSING");
}
void IndependentRoots(const Mesh3D& mesh,std::span<const Transform3D> roots){
    Require(roots.size()==520,"LANDSCAPE_SPECIES_LITERAL_520");
    for(std::size_t i=0;i<roots.size();++i){const auto& r=roots[i];Require(r.scale.x>=.5f&&r.scale.x<=.85f&&r.scale.x==r.scale.y&&r.scale.x==r.scale.z,"LANDSCAPE_SPECIES_INDEPENDENT_SCALE");
        for(std::size_t j=0;j<i;++j)Require(std::hypot(r.position.x-roots[j].position.x,r.position.z-roots[j].position.z)>=5.f,"LANDSCAPE_SPECIES_INDEPENDENT_SPACING");
        bool found{};for(std::size_t f=0;f<mesh.indices.size();f+=3){const auto a=D::Vertex(mesh,mesh.indices[f]),b=D::Vertex(mesh,mesh.indices[f+1]),c=D::Vertex(mesh,mesh.indices[f+2]);
            const double x=r.position.x,z=r.position.z;
            if(x<(std::min)({a.x,b.x,c.x})||x>(std::max)({a.x,b.x,c.x})||z<(std::min)({a.z,b.z,c.z})||z>(std::max)({a.z,b.z,c.z}))continue;
            const double det=(double(b.z)-c.z)*(double(a.x)-c.x)+(double(c.x)-b.x)*(double(a.z)-c.z);
            const double wa=((double(b.z)-c.z)*(x-c.x)+(double(c.x)-b.x)*(z-c.z))/det;
            const double wb=((double(c.z)-a.z)*(x-c.x)+(double(a.x)-c.x)*(z-c.z))/det,wc=1-wa-wb;
            if(wa<0||wb<0||wc<0)continue;
            Require(std::abs(wa*a.y+wb*b.y+wc*c.y-r.position.y)<=.00005,"LANDSCAPE_SPECIES_INDEPENDENT_ROOT_TRIANGLE");found=true;break;}
        Require(found,"LANDSCAPE_SPECIES_INDEPENDENT_ROOT_MISSING");
    }
}
void CheckPlan(const LandscapeSpeciesPlan& p,std::span<const Transform3D> roots,LandscapeSpeciesProfile profile){
    Require(p.sourcePoseFingerprint==RootIdentity(roots)&&p.renderSpeciesFingerprint==RenderIdentity(p),"LANDSCAPE_SPECIES_INDEPENDENT_IDENTITY");
    std::array<bool,520> selected{};unsigned count{};
    for(unsigned slot=0;slot<16;++slot){const auto id=p.selectedSourceOrdinals[slot];if(id==~0u)continue;
        Require(id<520&&!selected[id]&&(roots[id].position.x<0)==(slot<8),"LANDSCAPE_SPECIES_UNIQUE_SIDE_ORDINAL");selected[id]=true;++count;}
    Require(count==p.selected,"LANDSCAPE_SPECIES_SELECTED_COUNT");
    for(unsigned i=0;i<520;++i){const auto& actual=p.instances[i];Require(actual.sourceOrdinal==i,"LANDSCAPE_SPECIES_ORDINAL_CHANGED");
        if(!selected[i])Require(actual.species==LandscapeTreeSpecies::Fir&&Words(actual.render)==Words(roots[i]),"LANDSCAPE_SPECIES_NONSELECTED_CHANGED");
        else{Require(actual.species==LandscapeTreeSpecies::Broadleaf,"LANDSCAPE_SPECIES_SPECIES_CHANGED");auto expected=roots[i];const float scale=expected.scale.x*1.4f;expected.scale={scale,scale,scale};
            Require(Words(actual.render)==Words(expected),"LANDSCAPE_SPECIES_ROOT_YAW_SCALE_CHANGED");
            const float cs=std::cos(expected.rotation.y),sn=std::sin(expected.rotation.y);const auto local=BroadleafBounds();
            // 獨立旋轉全冠八個角並量完整 AABB；不以單角未入核心冒充完整冠幅通過。
            float minX=10000,maxX=-10000,minZ=10000,maxZ=-10000;
            for(float x:{-1.f,1.f})for(float y:{-1.f,1.f})for(float z:{-1.f,1.f}){const auto v=local.center+Vec3{x*local.extents.x,y*local.extents.y,z*local.extents.z};
                const float wx=expected.position.x+scale*(cs*v.x+sn*v.z),wz=expected.position.z+scale*(-sn*v.x+cs*v.z);
                minX=(std::min)(minX,wx);maxX=(std::max)(maxX,wx);minZ=(std::min)(minZ,wz);maxZ=(std::max)(maxZ,wz);}
            Require(maxX< -53.f||minX>53.f||maxZ< -23.f||minZ>103.f,"LANDSCAPE_SPECIES_INDEPENDENT_CORE_CROWN");}
    }
    if(profile==LandscapeSpeciesProfile::Original)Require(p.reachedBudget&&count==0&&p.firstMissingSlot==~0u&&p.groundEvaluations==0&&
        p.groundCacheHits==0&&p.evaluatedIntersections==0&&p.skippedLowerFaces==0,"LANDSCAPE_SPECIES_ORIGINAL_PROFILE");
    else{Require(p.reachedBudget==(count==16)&&((count==16)==(p.firstMissingSlot==~0u)),"LANDSCAPE_SPECIES_16_STATUS");
        Require(p.groundEvaluations<=520&&p.groundEvaluations+p.groundCacheHits<=p.considered,"LANDSCAPE_SPECIES_CALL_LOCAL_CACHE_ACCOUNTING");}
}
void RepeatPlan(const LandscapeSpeciesPlan& first,const LandscapeSpeciesPlan& second){
    Require(first.sourcePoseFingerprint==second.sourcePoseFingerprint&&first.renderSpeciesFingerprint==second.renderSpeciesFingerprint&&
        first.selectedSourceOrdinals==second.selectedSourceOrdinals&&first.selected==second.selected&&first.reachedBudget==second.reachedBudget&&
        first.firstMissingSlot==second.firstMissingSlot&&first.considered==second.considered&&first.rejectedBand==second.rejectedBand&&
        first.rejectedBounds==second.rejectedBounds&&first.rejectedSlope==second.rejectedSlope&&first.rejectedGround==second.rejectedGround&&
        first.rejectedOverlap==second.rejectedOverlap&&first.groundEvaluations==second.groundEvaluations&&first.groundCacheHits==second.groundCacheHits&&
        first.evaluatedIntersections==second.evaluatedIntersections&&first.skippedLowerFaces==second.skippedLowerFaces,"LANDSCAPE_SPECIES_NONDETERMINISTIC");
    for(unsigned i=0;i<520;++i)Require(first.instances[i].sourceOrdinal==second.instances[i].sourceOrdinal&&
        first.instances[i].species==second.instances[i].species&&Words(first.instances[i].render)==Words(second.instances[i].render),
        "LANDSCAPE_SPECIES_REPEAT_POSE_WORDS");
    Require(std::bit_cast<std::uint32_t>(first.maximumRootMeshError)==std::bit_cast<std::uint32_t>(second.maximumRootMeshError)&&
        std::bit_cast<std::uint32_t>(first.minimumLowerFaceGap)==std::bit_cast<std::uint32_t>(second.minimumLowerFaceGap)&&
        std::bit_cast<std::uint32_t>(first.maximumBaseStripGap)==std::bit_cast<std::uint32_t>(second.maximumBaseStripGap),
        "LANDSCAPE_SPECIES_REPEAT_GROUND_WORDS");
}
}
int wmain(int argc,wchar_t** argv){try{
    Require(argc==5&&std::wstring_view(argv[1])==L"--runtime-assets"&&std::wstring_view(argv[3])==L"--mode","USAGE --runtime-assets ABS --mode original|candidate");
    const auto root=std::filesystem::path(argv[2]).lexically_normal();Require(root.is_absolute()&&std::filesystem::is_directory(root),"LANDSCAPE_SPECIES_RUNTIME_ROOT");
    const auto mode=std::wstring_view(argv[4]);Require(mode==L"original"||mode==L"candidate","LANDSCAPE_SPECIES_INVALID_MODE");
    constexpr std::array<std::size_t,3> bytes{163400,3704316,57760};
    constexpr std::array<std::string_view,3> sha{"4182c1d2d3e726f5c77dd84adf59ea5d4e022d49b1f211b34b488dbc165b68cc",
        "c46eca29c84d6ea9db320444d01ed4f16403a2a7f50729178e70c7886b28bc5b","1c765173ebc17a3be167c35ef1b8df8329fb4cf98f468f4da13fd42533b677fd"};
    std::array<Mesh3D,3> meshes;for(unsigned i=0;i<3;++i){const auto path=root/"mistwatch-broadleaf/meshes"/BroadleafMeshName(i);Pin(path,bytes[i],sha[i]);
        auto loaded=LoadAevryndMeshFile(path);Require(bool(loaded),"LANDSCAPE_SPECIES_LOAD_FAILED");RootBroadleafMesh(loaded.mesh);meshes[i]=std::move(loaded.mesh);}
    const auto terrain=CreateTerrain();const auto landscape=CreateLandscapeMesh(terrain);const auto roots=LandscapeTrees(terrain);
    const auto seal=RootIdentity(roots),landSeal=MeshIdentity(landscape),coreSeal=MeshIdentity(terrain.CreateMesh(GroundMeshSettings()));
    std::array<std::uint64_t,3> meshSeals;for(unsigned i=0;i<3;++i)meshSeals[i]=MeshIdentity(meshes[i]);
    IndependentRoots(landscape,roots);GeometryControls();
    const auto original=CreateLandscapeSpeciesPlan(terrain,landscape,roots,meshes,seal);CheckPlan(original,roots,LandscapeSpeciesProfile::Original);
    const auto originalAgain=CreateLandscapeSpeciesPlan(terrain,landscape,roots,meshes,seal);RepeatPlan(original,originalAgain);
    auto bad=roots;bad.pop_back();Reject([&]{(void)CreateLandscapeSpeciesPlan(terrain,landscape,bad,meshes,seal);},"LANDSCAPE_SPECIES_SOURCE_POPULATION");
    bad=roots;bad.push_back(roots[0]);Reject([&]{(void)CreateLandscapeSpeciesPlan(terrain,landscape,bad,meshes,seal);},"LANDSCAPE_SPECIES_SOURCE_POPULATION");
    for(const auto profile:{static_cast<LandscapeSpeciesProfile>(2),static_cast<LandscapeSpeciesProfile>(255)})
        Reject([&]{(void)CreateLandscapeSpeciesPlan(terrain,landscape,roots,meshes,seal,profile);},"LANDSCAPE_SPECIES_INVALID_PROFILE");
    bad=roots;bad[0].position.y=std::numeric_limits<float>::quiet_NaN();Reject([&]{(void)CreateLandscapeSpeciesPlan(terrain,landscape,bad,meshes,seal);},"LANDSCAPE_SPECIES_SOURCE_POSE");
    bad=roots;bad[0].rotation.y=std::numeric_limits<float>::infinity();Reject([&]{(void)CreateLandscapeSpeciesPlan(terrain,landscape,bad,meshes,seal);},"LANDSCAPE_SPECIES_SOURCE_POSE");
    bad=roots;bad[0].scale={.49f,.49f,.49f};Reject([&]{(void)CreateLandscapeSpeciesPlan(terrain,landscape,bad,meshes,seal);},"LANDSCAPE_SPECIES_SOURCE_POSE");
    bad=roots;bad[0].scale.y+=.01f;Reject([&]{(void)CreateLandscapeSpeciesPlan(terrain,landscape,bad,meshes,seal);},"LANDSCAPE_SPECIES_SOURCE_POSE");
    bad=roots;bad[0].rotation.x=.001f;Reject([&]{(void)CreateLandscapeSpeciesPlan(terrain,landscape,bad,meshes,seal);},"LANDSCAPE_SPECIES_SOURCE_POSE");
    bad=roots;bad[1]=bad[0];Reject([&]{(void)CreateLandscapeSpeciesPlan(terrain,landscape,bad,meshes,seal);},"LANDSCAPE_SPECIES_SOURCE_SPACING");
    bad=roots;std::swap(bad[0],bad[1]);Reject([&]{(void)CreateLandscapeSpeciesPlan(terrain,landscape,bad,meshes,seal);},"LANDSCAPE_SPECIES_SOURCE_SEAL");
    bad=roots;bad[0].position.y+=.0001f;Reject([&]{(void)CreateLandscapeSpeciesPlan(terrain,landscape,bad,meshes,RootIdentity(bad));},"LANDSCAPE_SPECIES_SOURCE_ROOT_GAP");
    std::cout<<"LANDSCAPE_SPECIES_ORIGINAL_CPU_PASS source=520 source_fingerprint="<<seal<<" negatives="<<negatives<<" native=false GPU=false art=false\n"<<std::flush;
    LandscapeSpeciesPlan result=original;unsigned realGroundReferenceControls{},realReferenceOrdinal{~0u};D::GroundProof realReference,realPruned;
    if(mode==L"candidate"){result=CreateLandscapeSpeciesPlan(terrain,landscape,roots,meshes,seal,LandscapeSpeciesProfile::EdgeMixedBroadleafV1);
        CheckPlan(result,roots,LandscapeSpeciesProfile::EdgeMixedBroadleafV1);const auto again=CreateLandscapeSpeciesPlan(terrain,landscape,roots,meshes,seal,LandscapeSpeciesProfile::EdgeMixedBroadleafV1);
        RepeatPlan(result,again);
        // 有實際合格根時，限一株真已釘選 trunk／提交地表完整 reference 對照。
        // 零株時不冒充已做實物對照，最終 shortfall 仍 exit2。
        const D::Surface actualSurface(landscape);
        for(const auto id:result.selectedSourceOrdinals)if(id!=~0u){
            realPruned=PairedGround(result.instances[id].render,meshes[2],actualSurface,&realReference);
            Require(realPruned.result==D::GroundResult::Safe,"LANDSCAPE_SPECIES_REAL_REFERENCE_GROUND");
            realReferenceOrdinal=id;++realGroundReferenceControls;break;}}
    Require(RootIdentity(roots)==seal&&MeshIdentity(landscape)==landSeal&&MeshIdentity(terrain.CreateMesh(GroundMeshSettings()))==coreSeal,"LANDSCAPE_SPECIES_WORLD_MUTATED");
    for(unsigned i=0;i<3;++i)Require(MeshIdentity(meshes[i])==meshSeals[i],"LANDSCAPE_SPECIES_SOURCE_MUTATED");
    if(mode==L"candidate"){
        if(realGroundReferenceControls)std::cout<<"LANDSCAPE_SPECIES_REAL_GROUND_REFERENCE_PASS ordinal="<<realReferenceOrdinal
            <<" reference_min_word="<<std::bit_cast<std::uint32_t>(realReference.minimumLowerGap)<<" reference_max_word="<<std::bit_cast<std::uint32_t>(realReference.maximumBaseGap)
            <<" pruned_min_word="<<std::bit_cast<std::uint32_t>(realPruned.minimumLowerGap)<<" pruned_max_word="<<std::bit_cast<std::uint32_t>(realPruned.maximumBaseGap)
            <<" reference_evaluated_intersections="<<realReference.evaluatedIntersections<<" pruned_evaluated_intersections="<<realPruned.evaluatedIntersections
            <<" pruned_skipped_lower_faces="<<realPruned.skippedLowerFaces<<" native=false GPU=false art=false\n";
        std::cout<<(result.reachedBudget?"LANDSCAPE_SPECIES_ACTUAL16_PASS":"LANDSCAPE_SPECIES_16_SHORTFALL")<<" target=16 actual="<<result.selected
            <<" first_slot="<<result.firstMissingSlot<<" considered="<<result.considered<<" bounds="<<result.rejectedBounds<<" slope="<<result.rejectedSlope
            <<" ground="<<result.rejectedGround<<" overlap="<<result.rejectedOverlap<<" ground_evaluations="<<result.groundEvaluations<<" ground_cache_hits="<<result.groundCacheHits
            <<" evaluated_intersections="<<result.evaluatedIntersections<<" skipped_lower_faces="<<result.skippedLowerFaces
            <<" real_ground_reference_controls="<<realGroundReferenceControls<<" fingerprint="<<result.renderSpeciesFingerprint<<" selected=";
        for(const auto id:result.selectedSourceOrdinals)std::cout<<id<<',';
        std::cout<<" native=false GPU=false art=false full_qualification=false\n";if(!result.reachedBudget)return 2;
    }
    return 0;
}catch(const std::exception& error){std::cerr<<"LANDSCAPE_SPECIES_FIRST_DIVERGENCE "<<error.what()<<" native=false GPU=false art=false\n";return 1;}}
