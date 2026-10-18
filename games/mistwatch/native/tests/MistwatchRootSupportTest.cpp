#include "MistwatchRootSupport.h"
#include "MistwatchTerrainPlanting.h"
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace Aevrynd;using namespace Mistwatch;
namespace D=::Mistwatch::LandscapeSpeciesDetail;namespace S=::Mistwatch::RootSupportDetail;
namespace {
unsigned positives{},negatives{};
void Require(bool value,const char* label){if(!value)throw std::runtime_error(label);}
template<class Action>void Reject(Action action,const char* label){bool caught{};try{action();}catch(const std::invalid_argument& e){
    Require(std::string_view(e.what())==label,"ROOT_SUPPORT_WRONG_NEGATIVE_REASON");caught=true;}Require(caught,"ROOT_SUPPORT_NEGATIVE_ACCEPTED");++negatives;}
void Word(std::uint64_t& h,std::uint32_t word){for(unsigned i=0;i<4;++i){h^=(word>>(8*i))&255u;h*=1099511628211ull;}}
std::uint64_t Identity(const Mesh3D& mesh){std::uint64_t h=14695981039346656037ull;Word(h,static_cast<unsigned>(mesh.vertices.size()));Word(h,static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})Word(h,std::bit_cast<std::uint32_t>(f));Word(h,v.normal);Word(h,v.color);}for(const auto i:mesh.indices)Word(h,i);return h;}
bool SameFloat(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
void SameProof(const RootSupportProof& a,const RootSupportProof& b){Require(a.profile=="source-material-foot-v1"&&b.profile==a.profile&&a.result==b.result&&
    SameFloat(a.minimumLowerGap,b.minimumLowerGap)&&SameFloat(a.maximumFootGap,b.maximumFootGap)&&a.sourceFootFaces==b.sourceFootFaces&&a.sourceFootTriangles==b.sourceFootTriangles&&
    a.sourceFootSegments==b.sourceFootSegments&&a.sourceFootPoints==b.sourceFootPoints&&a.sourceRimSegments==b.sourceRimSegments&&a.sourceNonmanifoldEdges==b.sourceNonmanifoldEdges&&a.lowerFaces==b.lowerFaces&&
    a.sourceMeshFingerprint==b.sourceMeshFingerprint&&a.derivedMeshFingerprint==b.derivedMeshFingerprint&&a.sourceFootFingerprint==b.sourceFootFingerprint,"ROOT_SUPPORT_REFERENCE_BITS");}
RootSupportProof Paired(const Transform3D& pose,const Mesh3D& source,const Mesh3D& final,const Mesh3D& ground){
    const auto sourceSeal=Identity(source),finalSeal=Identity(final),groundSeal=Identity(ground);const D::Surface surface(ground);
    const auto reference=RootSupportGround(pose,source,final,surface,false),pruned=RootSupportGround(pose,source,final,surface,true);SameProof(reference,pruned);
    Require(Identity(source)==sourceSeal&&Identity(final)==finalSeal&&Identity(ground)==groundSeal,"ROOT_SUPPORT_CONST_FULL_MESH_NORMAL_IDENTITY");
    Require(reference.sourceMeshFingerprint==sourceSeal&&reference.derivedMeshFingerprint==finalSeal,"ROOT_SUPPORT_TRUE_IDENTITIES");return pruned;
}
Mesh3D Plane(float slope=0,float size=2){Mesh3D mesh;const auto n=PackNormalSnorm8({0,1,0});
    for(const auto p:std::array<Vec3,4>{{{0,0,0},{0,0,size},{size,size*slope,0},{size,size*slope,size}}})mesh.vertices.push_back({p.x,p.y,p.z,n,0xffffffffu,.2f,.8f});
    mesh.indices={0,1,2,2,1,3};return mesh;
}
Mesh3D Trunk(float width=.2f){Mesh3D mesh;const auto n=PackNormalSnorm8({1,0,0});
    for(float y:{0.f,1.f})for(const auto p:std::array<Vec3,4>{{{-width,0,-width},{-width,0,width},{width,0,width},{width,0,-width}}})
        mesh.vertices.push_back({p.x,y,p.z,n,0xffb0b0b0u,.25f,.75f});
    for(unsigned i=0;i<4;++i){const unsigned j=(i+1)%4;mesh.indices.insert(mesh.indices.end(),{i,j,i+4,j,j+4,i+4});}return mesh;
}
void Floor(Mesh3D& mesh,float height){const auto start=static_cast<unsigned>(mesh.vertices.size());const auto n=PackNormalSnorm8({0,1,0});
    for(const auto p:std::array<Vec3,3>{{{-.4f,height,-.4f},{-.4f,height,.4f},{.4f,height,-.4f}}})mesh.vertices.push_back({p.x,p.y,p.z,n,0xffb0b0b0u,.25f,.75f});
    mesh.indices.insert(mesh.indices.end(),{start,start+1,start+2});
}
Mesh3D Peak(float height){Mesh3D mesh;const auto n=PackNormalSnorm8({0,1,0});
    for(const auto p:std::array<Vec3,5>{{{.6f,0,.6f},{.6f,0,1.4f},{1.4f,0,1.4f},{1.4f,0,.6f},{1,height,1}}})mesh.vertices.push_back({p.x,p.y,p.z,n,0xffffffffu,0,0});
    mesh.indices={0,1,4,1,2,4,2,3,4,3,0,4};return mesh;
}
void Positive(float slope,float size=2,float centre=1,float scale=1,float yaw=0){const auto ground=Plane(slope,size),source=Trunk();const D::Surface surface(ground);
    const Transform3D pose{{centre,centre*slope,centre},{0,yaw,0},{scale,scale,scale}};
    const auto final=TerrainPlantingDetail::RootTrunkToLandscape(source,pose,surface);const auto result=Paired(pose,source,final,ground);
    Require(result.result==RootSupportResult::Safe&&result.sourceRimSegments==4&&result.sourceFootFaces==8&&result.sourceFootTriangles==12&&result.lowerFaces==8,"ROOT_SUPPORT_TRUE_SLOPE_POSITIVE");
    Require(result.minimumLowerGap>=-.04f&&result.maximumFootGap<=.04f&&result.evaluatedIntersections>0,"ROOT_SUPPORT_TRUE_SIGNED_GAP_DOMAIN");
    const auto originalBand=S::BuildFootProvenance(source,pose);const auto cut=D::R::Float(.035f)/D::R::Float(scale);unsigned cuts{};
    for(const auto& polygon:originalBand)for(const auto& weights:polygon.barycentric){D::R sum;for(const auto& w:weights)sum=sum+w;
        Require(Compare(sum,D::R(1))==0,"ROOT_SUPPORT_SOURCE_WEIGHT_SUM");const auto rest=S::Weighted(D::Triangle(source,polygon.sourceFace),weights);
        Require(Compare(rest[1],cut)<=0,"ROOT_SUPPORT_SOURCE_CUT_DOMAIN");if(Compare(rest[1],cut)==0)++cuts;}
    Require(cuts==16,"ROOT_SUPPORT_CROSS_EDGE_LITERAL_CUTS");
    if(slope!=0)Require(!SameFloat(source.vertices[0].y,final.vertices[0].y)&&SameFloat(source.vertices[4].y,final.vertices[4].y)&&source.vertices[4].normal!=final.vertices[4].normal,
        "ROOT_SUPPORT_REAL_DERIVER_UPPER_NORMAL");
    ++positives;std::cout<<"ROOT_SUPPORT_POSITIVE slope="<<slope<<" scale="<<scale<<" yaw="<<yaw<<" minimum_lower_word="<<std::bit_cast<std::uint32_t>(result.minimumLowerGap)
        <<" maximum_foot_word="<<std::bit_cast<std::uint32_t>(result.maximumFootGap)<<" foot_faces="<<result.sourceFootFaces<<" foot_triangles="<<result.sourceFootTriangles
        <<" rim_segments="<<result.sourceRimSegments<<" skipped_lower="<<result.skippedLowerFaces<<'\n';
}
void Controls(){Positive(0);Positive(.02f);Positive(.1f);Positive(.1f,16,8);Positive(.1f,2,1,1.05f,.3f);
    const auto ground=Plane(.1f),source=Trunk();const D::Surface surface(ground);const Transform3D pose{{1,.1f,1},{},{1,1,1}};
    const auto final=TerrainPlantingDetail::RootTrunkToLandscape(source,pose,surface);
    Require(D::Ground(pose,final,surface,false).result==D::GroundResult::Ground&&D::Ground(pose,final,surface,true).result==D::GroundResult::Ground,"ROOT_SUPPORT_LEGACY_HORIZONTAL_REJECTION_RETAINED");++negatives;
    for(float shift:{.05f,.10f,-.05f}){auto moved=pose;moved.position.y+=shift;Require(Paired(moved,source,final,ground).result==RootSupportResult::Ground,"ROOT_SUPPORT_WHOLE_ROOT_SHIFT_REJECTION");++negatives;}
    const auto steep=Plane(.3f);const D::Surface steepSurface(steep);const Transform3D steepPose{{1,.3f,1},{},{1,1,1}};
    const auto steepFinal=TerrainPlantingDetail::RootTrunkToLandscape(source,steepPose,steepSurface);
    Require(Paired(steepPose,source,steepFinal,steep).result==RootSupportResult::Slope,"ROOT_SUPPORT_ACTUAL_SLOPE_REJECTION");++negatives;
    auto missing=ground;missing.indices.resize(3);Require(Paired(pose,source,final,missing).result==RootSupportResult::Ground,"ROOT_SUPPORT_MISSING_SURFACE_REJECTION");++negatives;
    const auto flat=Plane();const Transform3D edgePose{{0,0,1},{},{1,1,1}};Require(Paired(edgePose,source,source,flat).result==RootSupportResult::Ground,"ROOT_SUPPORT_CLOSED_OUTER_EDGE_REJECTION");++negatives;
    auto tangent=source;tangent.vertices[0].y=.035f;tangent.vertices[1].y=.035f;const Transform3D closedPose{{1,0,1},{},{1,1,1}};
    const auto touching=S::BuildFootProvenance(tangent,closedPose);constexpr std::array<unsigned,8> touchingFaces{0,3,6,9,12,15,18,21},touchingSizes{2,1,3,3,4,3,3,1};
    Require(touching.size()==8,"ROOT_SUPPORT_CLOSED_SOURCE_LITERAL_COUNT");for(unsigned i=0;i<8;++i)
        Require(touching[i].sourceFace==touchingFaces[i]&&touching[i].barycentric.size()==touchingSizes[i],"ROOT_SUPPORT_CLOSED_SOURCE_LITERAL_IDS_DOMAIN");
    const auto closed=Paired(closedPose,tangent,tangent,flat);Require(closed.result==RootSupportResult::Safe&&closed.sourceFootFaces==8&&closed.sourceFootPoints==2&&closed.sourceFootSegments==1,
        "ROOT_SUPPORT_CLOSED_SOURCE_POINT_SEGMENT");++positives;
    auto movedPoint=tangent;movedPoint.vertices[0].y+=.05f;Require(Paired(closedPose,tangent,movedPoint,flat).result==RootSupportResult::Ground,"ROOT_SUPPORT_CLOSED_POINT_FLOAT_REJECTION");++negatives;
    auto floor=Trunk(.4f);Floor(floor,.036f);const auto peak=Peak(.08f);const Transform3D flatPose{{1,0,1},{},{1,1,1}};
    // 三個來源頂點皆安全，真地表內部峰值仍必須被完整 lower 面找出。
    const D::Surface peakSurface(peak);for(unsigned i=8;i<11;++i){const auto p=TransformPoint(flatPose.Matrix(),D::Vertex(floor,i));
        Require(Compare(D::Abs(D::R::Float(p.y)-peakSurface.At(p)),D::R::Float(.04f))<=0,"ROOT_SUPPORT_INTERIOR_VERTEX_CONTROL");}
    Require(Paired(flatPose,floor,floor,peak).result==RootSupportResult::Ground,"ROOT_SUPPORT_FULL_LOWER_INTERIOR_PIERCING");++negatives;
    auto valleyFoot=Trunk(.4f);Floor(valleyFoot,.02f);const auto valley=Peak(-.08f);
    Require(Paired(flatPose,valleyFoot,valleyFoot,valley).result==RootSupportResult::Ground,"ROOT_SUPPORT_FULL_FOOT_INTERIOR_VALLEY");++negatives;
    auto invalid=pose;invalid.scale.z=2;Reject([&]{(void)RootSupportGround(invalid,source,final,surface);},"ROOT_SUPPORT_POSE");
    invalid=pose;invalid.position.x=std::numeric_limits<float>::quiet_NaN();Reject([&]{(void)RootSupportGround(invalid,source,final,surface);},"ROOT_SUPPORT_POSE");
    auto bad=final;std::swap_ranges(bad.indices.begin(),bad.indices.begin()+3,bad.indices.begin()+3);Reject([&]{(void)RootSupportGround(pose,source,bad,surface);},"ROOT_SUPPORT_TOPOLOGY");
    bad=final;bad.vertices[0].x+=.01f;Reject([&]{(void)RootSupportGround(pose,source,bad,surface);},"ROOT_SUPPORT_XZ");
    bad=final;bad.vertices[0].u+=.01f;Reject([&]{(void)RootSupportGround(pose,source,bad,surface);},"ROOT_SUPPORT_MATERIAL_IDENTITY");
    bad=final;bad.vertices[0].color^=1u;Reject([&]{(void)RootSupportGround(pose,source,bad,surface);},"ROOT_SUPPORT_MATERIAL_IDENTITY");
    bad=final;bad.vertices[0].normal^=0x01000000u;Reject([&]{(void)RootSupportGround(pose,source,bad,surface);},"ROOT_SUPPORT_NORMAL_HANDEDNESS");
    bad=final;bad.vertices[4].y+=.01f;Reject([&]{(void)RootSupportGround(pose,source,bad,surface);},"ROOT_SUPPORT_UPPER_POSITION");
    bad=final;bad.vertices[0].y=std::numeric_limits<float>::infinity();Reject([&]{(void)RootSupportGround(pose,source,bad,surface);},"LANDSCAPE_SPECIES_NONFINITE_MESH");
    bad=source;bad.vertices[0].y=2;Reject([&]{(void)RootSupportGround(pose,source,bad,surface);},"ROOT_SUPPORT_FACE_ORIENTATION");
    auto seam=source;seam.vertices.push_back(seam.vertices[0]);seam.indices[0]=8;auto split=seam;split.vertices[8].y=.01f;
    Reject([&]{(void)RootSupportGround(pose,seam,split,surface);},"ROOT_SUPPORT_SPLIT_WELD");
    auto noRim=source;noRim.indices.erase(noRim.indices.begin(),noRim.indices.begin()+6);Reject([&]{(void)S::SourceRim(noRim,pose);},"ROOT_SUPPORT_RIM_CROSSES_FOOT");
    auto records=S::BuildFootProvenance(source,pose);auto wrong=records;wrong.clear();Reject([&]{S::ValidateFootProvenance(source,pose,wrong);},"ROOT_SUPPORT_FOOT_PROVENANCE");
    wrong=records;wrong.pop_back();Reject([&]{S::ValidateFootProvenance(source,pose,wrong);},"ROOT_SUPPORT_FOOT_PROVENANCE");
    wrong=records;wrong.push_back(records.front());Reject([&]{S::ValidateFootProvenance(source,pose,wrong);},"ROOT_SUPPORT_FOOT_PROVENANCE");
    wrong=records;wrong[0].sourceFace+=3;Reject([&]{S::ValidateFootProvenance(source,pose,wrong);},"ROOT_SUPPORT_FOOT_PROVENANCE");
    wrong=records;wrong[0].barycentric[0][0]=D::R(2);Reject([&]{S::ValidateFootProvenance(source,pose,wrong);},"ROOT_SUPPORT_FOOT_PROVENANCE");
    const auto triangle=D::Triangle(source,0),finalTriangle=D::Triangle(final,0);const auto cut=D::R::Float(.035f);
    Reject([&]{(void)S::MapFootPoint(triangle,finalTriangle,D::P{{D::R(2),-D::R(1),D::R{}}},cut);},"ROOT_SUPPORT_FOOT_WEIGHTS");
    Reject([&]{(void)S::MapFootPoint(triangle,finalTriangle,D::P{{D::R{},D::R{},D::R(1)}},cut);},"ROOT_SUPPORT_FOOT_DOMAIN");
}
void ActualSource(const std::filesystem::path& assets){const auto path=assets/"mistwatch-broadleaf/meshes"/BroadleafMeshName(2);
    Require(assets.is_absolute()&&std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)==57760,"ROOT_SUPPORT_ACTUAL_ASSET_BYTES");
    std::ifstream input(path,std::ios::binary);std::vector<std::byte> raw(57760);input.read(reinterpret_cast<char*>(raw.data()),57760);Require(bool(input),"ROOT_SUPPORT_ACTUAL_ASSET_READ");
    std::array<std::byte,32> digest;Require(WorldDeltaSha2563D(raw,digest),"ROOT_SUPPORT_ACTUAL_ASSET_HASH");std::ostringstream sha;sha<<std::hex<<std::setfill('0');
    for(const auto b:digest)sha<<std::setw(2)<<std::to_integer<unsigned>(b);Require(sha.str()=="1c765173ebc17a3be167c35ef1b8df8329fb4cf98f468f4da13fd42533b677fd","ROOT_SUPPORT_ACTUAL_ASSET_IDENTITY");
    auto loaded=LoadAevryndMeshFile(path);Require(bool(loaded),"ROOT_SUPPORT_ACTUAL_ASSET_LOAD");RootBroadleafMesh(loaded.mesh);const auto source=std::move(loaded.mesh);const auto seal=Identity(source);
    Require(source.vertices.size()==1329&&source.indices.size()==5133,"ROOT_SUPPORT_ACTUAL_SOURCE_COUNTS");
    const Transform3D pose{{},{0,.3f,0},{1.05f,1.05f,1.05f}};S::ValidateInput(source,source,pose);const auto foot=S::BuildFootProvenance(source,pose);S::ValidateFootProvenance(source,pose,foot);
    unsigned nonmanifold{};const auto rim=S::SourceRim(source,pose,&nonmanifold);Require(rim.size()==5&&nonmanifold==51,"ROOT_SUPPORT_ACTUAL_RIM_LITERAL");
    Require(Identity(source)==seal,"ROOT_SUPPORT_ACTUAL_SOURCE_NORMAL_MUTATED");
    std::cout<<"ROOT_SUPPORT_ACTUAL_SOURCE_PROVENANCE_PASS vertices=1329 indices=5133 rim_segments="<<rim.size()<<" nonmanifold_edges="<<nonmanifold
        <<" foot_faces="<<foot.size()<<" source_fingerprint="<<seal<<" final_root_support=false native=false GPU=false art=false\n";
}
// 固定原 ordinal 的有限真森林診斷；新足帶 profile 不冒稱舊解析坡度／水平帶資格。
constexpr std::array<unsigned,42> ForestOrdinals{177,212,496,429,519,436,423,410,37,256,465,205,48,171,58,455,
    266,363,443,34,460,392,53,220,222,271,219,409,500,400,386,473,419,155,206,458,170,368,99,26,345,193};
constexpr std::array<std::size_t,3> ForestAssetBytes{163400,3704316,57760};
constexpr std::array<std::string_view,3> ForestAssetSha{
    "4182c1d2d3e726f5c77dd84adf59ea5d4e022d49b1f211b34b488dbc165b68cc",
    "c46eca29c84d6ea9db320444d01ed4f16403a2a7f50729178e70c7886b28bc5b",
    "1c765173ebc17a3be167c35ef1b8df8329fb4cf98f468f4da13fd42533b677fd"};
void PinForestAsset(const std::filesystem::path& path,unsigned slot){
    Require(slot<3&&std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)==ForestAssetBytes[slot],"ROOT_SUPPORT_FOREST_ASSET_BYTES");
    std::ifstream input(path,std::ios::binary);std::vector<std::byte> raw(ForestAssetBytes[slot]);
    input.read(reinterpret_cast<char*>(raw.data()),static_cast<std::streamsize>(raw.size()));Require(bool(input),"ROOT_SUPPORT_FOREST_ASSET_READ");
    std::array<std::byte,32> digest;Require(WorldDeltaSha2563D(raw,digest),"ROOT_SUPPORT_FOREST_ASSET_HASH");std::ostringstream sha;sha<<std::hex<<std::setfill('0');
    for(const auto b:digest)sha<<std::setw(2)<<std::to_integer<unsigned>(b);Require(sha.str()==ForestAssetSha[slot],"ROOT_SUPPORT_FOREST_ASSET_IDENTITY");
}
std::uint64_t ForestPoseIdentity(std::span<const Transform3D> roots){std::uint64_t h=14695981039346656037ull;
    Word(h,static_cast<unsigned>(roots.size()));for(unsigned i=0;i<roots.size();++i){Word(h,i);for(const auto w:TerrainPlantingPoseWords(roots[i]))Word(h,w);}return h;
}
void VerifyForestDerived(const Mesh3D& source,const Mesh3D& derived){
    Require(source.indices==derived.indices&&source.vertices.size()==derived.vertices.size(),"ROOT_SUPPORT_FOREST_DERIVED_TOPOLOGY");
    std::vector<Vec3> normals(derived.vertices.size());std::vector<bool> touched(derived.vertices.size());
    for(unsigned f=0;f<derived.indices.size();f+=3){const auto a=derived.indices[f],b=derived.indices[f+1],c=derived.indices[f+2];
        const auto normal=Cross(D::Vertex(derived,b)-D::Vertex(derived,a),D::Vertex(derived,c)-D::Vertex(derived,a));
        Require(D::Finite(normal)&&Length(normal)>0,"ROOT_SUPPORT_FOREST_DERIVED_FACE");
        normals[a]=normals[a]+normal;normals[b]=normals[b]+normal;normals[c]=normals[c]+normal;
        if(!SameFloat(source.vertices[a].y,derived.vertices[a].y)||!SameFloat(source.vertices[b].y,derived.vertices[b].y)||!SameFloat(source.vertices[c].y,derived.vertices[c].y))
            touched[a]=touched[b]=touched[c]=true;}
    for(unsigned i=0;i<source.vertices.size();++i){const auto& a=source.vertices[i];const auto& b=derived.vertices[i];
        Require(SameFloat(a.x,b.x)&&SameFloat(a.z,b.z)&&SameFloat(a.u,b.u)&&SameFloat(a.v,b.v)&&a.color==b.color&&
            (a.normal&0xff000000u)==(b.normal&0xff000000u),"ROOT_SUPPORT_FOREST_DERIVED_MATERIAL_XZ");
        if(a.y>=.35f)Require(SameFloat(a.y,b.y),"ROOT_SUPPORT_FOREST_DERIVED_UPPER_POSITION");
        if(touched[i]){Require(D::Finite(normals[i])&&Length(normals[i])>1.e-8f,"ROOT_SUPPORT_FOREST_DERIVED_NORMAL_DOMAIN");
            Require(b.normal==PackNormalSnorm8(Normalize(normals[i]),UnpackNormalSnorm8(a.normal).w),"ROOT_SUPPORT_FOREST_DERIVED_NORMAL_VALUE");}
        else Require(a.normal==b.normal,"ROOT_SUPPORT_FOREST_UNTOUCHED_NORMAL");}
}
std::array<std::uint32_t,7> ForestBoundsWords(const MeshBounds3D& b){return{std::bit_cast<std::uint32_t>(b.center.x),std::bit_cast<std::uint32_t>(b.center.y),
    std::bit_cast<std::uint32_t>(b.center.z),std::bit_cast<std::uint32_t>(b.radius),std::bit_cast<std::uint32_t>(b.extents.x),
    std::bit_cast<std::uint32_t>(b.extents.y),std::bit_cast<std::uint32_t>(b.extents.z)};}
MeshBounds3D ForestCompleteBounds(const std::array<Mesh3D,3>& source,const Mesh3D& derived,const Transform3D& pose,bool& actualOutside){
    Vec3 low{10000,10000,10000},high{-10000,-10000,-10000},worldLow=low,worldHigh=high;const auto matrix=pose.Matrix();
    for(unsigned slot=0;slot<3;++slot){const auto& mesh=slot==2?derived:source[slot];for(const auto& v:mesh.vertices){
        low.x=(std::min)(low.x,v.x);low.y=(std::min)(low.y,v.y);low.z=(std::min)(low.z,v.z);
        high.x=(std::max)(high.x,v.x);high.y=(std::max)(high.y,v.y);high.z=(std::max)(high.z,v.z);
        const auto p=TransformPoint(matrix,{v.x,v.y,v.z});Require(D::Finite(p),"ROOT_SUPPORT_FOREST_CROWN_NONFINITE");
        worldLow.x=(std::min)(worldLow.x,p.x);worldLow.y=(std::min)(worldLow.y,p.y);worldLow.z=(std::min)(worldLow.z,p.z);
        worldHigh.x=(std::max)(worldHigh.x,p.x);worldHigh.y=(std::max)(worldHigh.y,p.y);worldHigh.z=(std::max)(worldHigh.z,p.z);}}
    const auto half=(high-low)*.5f;const MeshBounds3D measured{(low+high)*.5f,Length(half),half};
    Require(ForestBoundsWords(measured)==ForestBoundsWords(TerrainPlantingDetail::FinalBounds(source,derived)),"ROOT_SUPPORT_FOREST_COMPLETE_BOUNDS");
    actualOutside=worldHigh.x< -53.f||worldLow.x>53.f||worldHigh.z< -23.f||worldLow.z>103.f;
    return TransformBounds(measured,pose);
}
const char* ForestSupportResultName(RootSupportResult result){switch(result){case RootSupportResult::Safe:return "safe";
    case RootSupportResult::Slope:return "actual-slope";case RootSupportResult::Ground:return "ground-gap-or-coverage";}return "invalid";}
int ForestSupport(const std::filesystem::path& assets){Require(assets.is_absolute()&&std::filesystem::is_directory(assets),"ROOT_SUPPORT_FOREST_ASSETS_ABSOLUTE");
    std::array<Mesh3D,3> sources;std::array<std::uint64_t,3> sourceSeals{};
    constexpr std::array<std::size_t,3> vertexCounts{4306,99050,1329},indexCounts{10704,232725,5133};
    for(unsigned slot=0;slot<3;++slot){const auto path=assets/"mistwatch-broadleaf/meshes"/BroadleafMeshName(slot);PinForestAsset(path,slot);
        auto loaded=LoadAevryndMeshFile(path);Require(bool(loaded),"ROOT_SUPPORT_FOREST_SOURCE_LOAD");RootBroadleafMesh(loaded.mesh);sources[slot]=std::move(loaded.mesh);
        Require(sources[slot].vertices.size()==vertexCounts[slot]&&sources[slot].indices.size()==indexCounts[slot],"ROOT_SUPPORT_FOREST_SOURCE_COUNTS");sourceSeals[slot]=Identity(sources[slot]);}
    (void)D::Measure(sources);const auto core=CreateTerrain();const auto landscape=CreateLandscapeMesh(core);const auto roots=LandscapeTrees(core);
    const auto rootsBefore=roots;const auto coreMesh=core.CreateMesh(GroundMeshSettings());const auto coreSeal=Identity(coreMesh),landSeal=Identity(landscape),rootsSeal=ForestPoseIdentity(roots);
    Require(roots.size()==520&&rootsSeal==11953736207267797409ull,"ROOT_SUPPORT_FOREST_LITERAL_520_SEAL");
    const auto original=CreateTerrainPlantingPlan(core,landscape,roots,sources,11953736207267797409ull);
    Require(original.complete&&original.plants.empty()&&original.firstMissingAnchor==~0u,"ROOT_SUPPORT_FOREST_ORIGINAL_FAST_PROFILE");
    for(unsigned i=0;i<520;++i){Require(original.instances[i].sourceOrdinal==i&&original.instances[i].species==LandscapeTreeSpecies::Fir&&
        TerrainPlantingPoseWords(original.instances[i].render)==TerrainPlantingPoseWords(roots[i]),"ROOT_SUPPORT_FOREST_ORIGINAL_POSE_WORDS");
        for(unsigned j=0;j<i;++j)Require(std::hypot(roots[i].position.x-roots[j].position.x,roots[i].position.z-roots[j].position.z)>=5.f,"ROOT_SUPPORT_FOREST_ORIGINAL_5M");}
    const D::Surface surface(landscape);std::array<bool,520> used{},seen{};auto instances=original.instances;std::vector<MeshBounds3D> selectedBounds;
    std::vector<unsigned> selectedOrdinals;std::uint64_t derivedSeal=14695981039346656037ull;unsigned safeCount{},pairedCount{},evaluated{},skipped{},derivedRejected{};
    std::cout<<std::setprecision(std::numeric_limits<float>::max_digits10)
        <<"ROOT_SUPPORT_FOREST_BEGIN profile=source-root-material-foot-actual-surface-v1 root_support_profile=source-material-foot-v1"
        <<" candidates=42 target=16 source_pose_fingerprint="<<rootsSeal<<" landscape_fingerprint="<<landSeal
        <<" analytic_097_is_diagnostic=true legacy_profiles_unchanged=true native=false GPU=false art=false\n"<<std::flush;
    for(unsigned candidate=0;candidate<ForestOrdinals.size();++candidate){const unsigned ordinal=ForestOrdinals[candidate];
        Require(ordinal<520&&!seen[ordinal],"ROOT_SUPPORT_FOREST_LITERAL_42_UNIQUE");seen[ordinal]=true;auto pose=roots[ordinal];pose.scale={1.05f,1.05f,1.05f};
        const auto root=surface.At(pose.position);pose.position.y=static_cast<float>(root.Approximate());const auto error=D::Abs(root-D::R::Float(pose.position.y));
        Require(Compare(error,D::R::Float(.00005f))<=0,"ROOT_SUPPORT_FOREST_ROOT_ROUNDING");
        Require(SameFloat(pose.position.x,roots[ordinal].position.x)&&SameFloat(pose.position.z,roots[ordinal].position.z)&&
            SameFloat(pose.rotation.y,roots[ordinal].rotation.y)&&pose.rotation.x==0&&pose.rotation.z==0,"ROOT_SUPPORT_FOREST_SOURCE_XZ_YAW");
        const auto nearest=TerrainPlantingDetail::NearestOrdinal(roots,used,pose.position);Require(nearest==ordinal&&
            (pose.position.x<0)==(roots[ordinal].position.x<0)&&std::hypot(pose.position.x-roots[ordinal].position.x,pose.position.z-roots[ordinal].position.z)<=12.f,
            "ROOT_SUPPORT_FOREST_SELF_UNIQUE_12M");
        for(unsigned j=0;j<520;++j)if(j!=ordinal)Require(std::hypot(pose.position.x-roots[j].position.x,pose.position.z-roots[j].position.z)>=5.f,
            "ROOT_SUPPORT_FOREST_REPLACEMENT_5M");
        const float analyticNormal=LandscapeNormal(core,pose.position.x,pose.position.z).y;
        std::cout<<"ROOT_SUPPORT_FOREST_CANDIDATE_BEGIN candidate="<<candidate<<" source_ordinal="<<ordinal<<" analytic_normal_y="<<analyticNormal
            <<" source_y_word="<<std::bit_cast<std::uint32_t>(roots[ordinal].position.y)<<" actual_y_word="<<std::bit_cast<std::uint32_t>(pose.position.y)
            <<" root_height_delta="<<pose.position.y-roots[ordinal].position.y<<" root_error="<<error.Approximate()<<'\n'<<std::flush;
        const auto recordGeometryRejection=[&](const char* reason,const Mesh3D* mesh){++derivedRejected;
            Require(Identity(sources[2])==sourceSeals[2],"ROOT_SUPPORT_FOREST_REJECTED_SOURCE_MUTATION");
            std::cout<<"ROOT_SUPPORT_FOREST_ROW candidate="<<candidate<<" source_ordinal="<<ordinal<<" result=derived-invalid selection_reason="<<reason
                <<" selected=false support_evaluated=false x="<<pose.position.x<<" y="<<pose.position.y<<" z="<<pose.position.z
                <<" yaw="<<pose.rotation.y<<" scale="<<pose.scale.x<<" pose_words=";for(const auto word:TerrainPlantingPoseWords(pose))std::cout<<word<<',';
            std::cout<<" analytic_normal_y="<<analyticNormal<<" analytic_097_is_diagnostic=true root_error="<<error.Approximate()
                <<" mesh_fingerprint="<<(mesh?Identity(*mesh):0ull)<<" vertices="<<(mesh?mesh->vertices.size():0u)<<" indices="<<(mesh?mesh->indices.size():0u)
                <<" extrema_valid=false native=false GPU=false art=false\n"<<std::flush;};
        Mesh3D derived;
        // 只有兩個真幾何退化標籤可列為個別候選拒絕；來源／數值／其他例外仍直接失敗。
        try{derived=TerrainPlantingDetail::RootTrunkToLandscape(sources[2],pose,surface);}
        catch(const std::invalid_argument& e){const std::string_view label=e.what();
            if(label!="TERRAIN_PLANTING_ROOT_DEGENERATE_FACE"&&label!="TERRAIN_PLANTING_ROOT_DEGENERATE_NORMAL")throw;
            recordGeometryRejection(e.what(),nullptr);continue;}
        VerifyForestDerived(sources[2],derived);
        bool actualOutside{};const auto bounds=ForestCompleteBounds(sources,derived,pose,actualOutside);const bool outside=D::OutsideCore(bounds)&&actualOutside;
        RootSupportProof proof;
        // 全面翻轉檢查的明確拒絕不當成 Safe；未知 invalid_argument 不吞掉。
        try{proof=RootSupportGround(pose,sources[2],derived,surface,true);}
        catch(const std::invalid_argument& e){if(std::string_view(e.what())!="ROOT_SUPPORT_FACE_ORIENTATION")throw;
            recordGeometryRejection(e.what(),&derived);continue;}
        evaluated+=proof.evaluatedIntersections;skipped+=proof.skippedLowerFaces;
        Require(proof.sourceMeshFingerprint==sourceSeals[2]&&proof.derivedMeshFingerprint==Identity(derived),"ROOT_SUPPORT_FOREST_TRUE_DERIVED_IDENTITIES");
        if(proof.result==RootSupportResult::Safe){++safeCount;Require(proof.minimumLowerGap>=-.04f&&proof.maximumFootGap<=.04f&&proof.sourceRimSegments==5&&
                proof.sourceNonmanifoldEdges==51,"ROOT_SUPPORT_FOREST_SAFE_TRUE_GAPS_RIM");
            if(!pairedCount){std::cout<<"ROOT_SUPPORT_FOREST_REFERENCE_BEGIN source_ordinal="<<ordinal<<'\n'<<std::flush;
                const auto reference=RootSupportGround(pose,sources[2],derived,surface,false);SameProof(reference,proof);++pairedCount;
                std::cout<<"ROOT_SUPPORT_FOREST_REFERENCE_PASS source_ordinal="<<ordinal<<" reference_intersections="<<reference.evaluatedIntersections
                    <<" pruned_intersections="<<proof.evaluatedIntersections<<" skipped_lower="<<proof.skippedLowerFaces
                    <<" minimum_lower_word="<<std::bit_cast<std::uint32_t>(proof.minimumLowerGap)
                    <<" maximum_foot_word="<<std::bit_cast<std::uint32_t>(proof.maximumFootGap)<<'\n'<<std::flush;}}
        bool overlap{};for(const auto& previous:selectedBounds)overlap|=D::Overlap(bounds,previous);
        const bool selected=proof.result==RootSupportResult::Safe&&outside&&!overlap&&selectedOrdinals.size()<16;
        const char* reason=proof.result!=RootSupportResult::Safe?ForestSupportResultName(proof.result):!outside?"crown-core":overlap?"crown-overlap":
            selectedOrdinals.size()>=16?"target-already-full":"selected";
        if(selected){used[ordinal]=true;selectedOrdinals.push_back(ordinal);selectedBounds.push_back(bounds);instances[ordinal]={ordinal,LandscapeTreeSpecies::Broadleaf,pose};
            Word(derivedSeal,candidate);Word(derivedSeal,ordinal);Word(derivedSeal,static_cast<std::uint32_t>(proof.derivedMeshFingerprint));
            Word(derivedSeal,static_cast<std::uint32_t>(proof.derivedMeshFingerprint>>32));}
        std::cout<<"ROOT_SUPPORT_FOREST_ROW candidate="<<candidate<<" source_ordinal="<<ordinal<<" result="<<ForestSupportResultName(proof.result)
            <<" selection_reason="<<reason<<" selected="<<selected<<" outside_core="<<outside<<" crown_overlap="<<overlap
            <<" x="<<pose.position.x<<" y="<<pose.position.y<<" z="<<pose.position.z<<" yaw="<<pose.rotation.y<<" scale="<<pose.scale.x<<" pose_words=";
        for(const auto word:TerrainPlantingPoseWords(pose))std::cout<<word<<',';
        std::cout<<" analytic_normal_y="<<analyticNormal<<" analytic_097_pass="<<(analyticNormal>=.97f)<<" analytic_097_is_diagnostic=true"
            <<" actual_full_footprint_097_required=true actual_full_support_pass="<<(proof.result==RootSupportResult::Safe)<<" root_error="<<error.Approximate()
            <<" mesh_fingerprint="<<proof.derivedMeshFingerprint<<" source_foot_fingerprint="<<proof.sourceFootFingerprint
            <<" vertices="<<derived.vertices.size()<<" indices="<<derived.indices.size()<<" extrema_valid="<<(proof.result==RootSupportResult::Safe)
            <<" minimum_lower_word="<<std::bit_cast<std::uint32_t>(proof.minimumLowerGap)<<" maximum_foot_word="<<std::bit_cast<std::uint32_t>(proof.maximumFootGap)
            <<" foot_faces="<<proof.sourceFootFaces<<" foot_triangles="<<proof.sourceFootTriangles<<" foot_segments="<<proof.sourceFootSegments
            <<" foot_points="<<proof.sourceFootPoints<<" rim_segments="<<proof.sourceRimSegments<<" source_nonmanifold_edges="<<proof.sourceNonmanifoldEdges
            <<" lower_faces="<<proof.lowerFaces<<" evaluated_intersections="<<proof.evaluatedIntersections<<" skipped_lower="<<proof.skippedLowerFaces
            <<" bounds_words=";for(const auto word:ForestBoundsWords(bounds))std::cout<<word<<',';std::cout<<'\n'<<std::flush;
        Require(Identity(sources[2])==sourceSeals[2]&&Identity(derived)==proof.derivedMeshFingerprint,"ROOT_SUPPORT_FOREST_PAIRED_CONST_IDENTITY");
    }
    Require((safeCount==0&&pairedCount==0)||(safeCount>0&&pairedCount==1),"ROOT_SUPPORT_FOREST_EXACT_ONE_SAFE_REFERENCE");
    std::uint64_t renderSeal=14695981039346656037ull;for(unsigned i=0;i<520;++i){Require(instances[i].sourceOrdinal==i,"ROOT_SUPPORT_FOREST_FINAL_ORDINAL");
        if(!used[i])Require(instances[i].species==LandscapeTreeSpecies::Fir&&TerrainPlantingPoseWords(instances[i].render)==TerrainPlantingPoseWords(rootsBefore[i]),
            "ROOT_SUPPORT_FOREST_FINAL_UNSELECTED_9WORDS");
        else Require(instances[i].species==LandscapeTreeSpecies::Broadleaf&&SameFloat(instances[i].render.position.x,rootsBefore[i].position.x)&&
            SameFloat(instances[i].render.position.z,rootsBefore[i].position.z)&&SameFloat(instances[i].render.rotation.y,rootsBefore[i].rotation.y)&&
            instances[i].render.scale.x==1.05f&&instances[i].render.scale.y==1.05f&&instances[i].render.scale.z==1.05f,"ROOT_SUPPORT_FOREST_FINAL_SELECTED_POSE");
        Require(TerrainPlantingPoseWords(roots[i])==TerrainPlantingPoseWords(rootsBefore[i]),"ROOT_SUPPORT_FOREST_SOURCE_END_9WORDS");
        Word(renderSeal,i);Word(renderSeal,static_cast<unsigned>(instances[i].species));for(const auto word:TerrainPlantingPoseWords(instances[i].render))Word(renderSeal,word);}
    for(unsigned i=0;i<selectedBounds.size();++i)for(unsigned j=0;j<i;++j)Require(!D::Overlap(selectedBounds[i],selectedBounds[j]),"ROOT_SUPPORT_FOREST_FINAL_CROWN_OVERLAP");
    Require(ForestPoseIdentity(roots)==rootsSeal&&Identity(landscape)==landSeal&&Identity(core.CreateMesh(GroundMeshSettings()))==coreSeal,"ROOT_SUPPORT_FOREST_SOURCE_TERRAIN_UNCHANGED");
    for(unsigned slot=0;slot<3;++slot){Require(Identity(sources[slot])==sourceSeals[slot],"ROOT_SUPPORT_FOREST_SOURCE_NORMAL_END_IDENTITY");
        PinForestAsset(assets/"mistwatch-broadleaf/meshes"/BroadleafMeshName(slot),slot);}
    const bool complete=selectedOrdinals.size()==16;
    std::cout<<(complete?"ROOT_SUPPORT_FOREST_ACTUAL16_PASS":"ROOT_SUPPORT_FOREST_SHORTFALL")
        <<" profile=source-root-material-foot-actual-surface-v1 target=16 actual="<<selectedOrdinals.size()<<" diagnosed=42 safe="<<safeCount<<" paired="<<pairedCount
        <<" source_pose_fingerprint="<<rootsSeal<<" landscape_fingerprint="<<landSeal<<" render_fingerprint="<<renderSeal<<" derived_fingerprint="<<derivedSeal
        <<" evaluated_intersections="<<evaluated<<" skipped_lower="<<skipped<<" derived_rejected="<<derivedRejected<<" selected_ordinals=";
    for(const auto ordinal:selectedOrdinals)std::cout<<ordinal<<',';
    std::cout<<" source_unchanged=true terrain_unchanged=true legacy_horizontal_base_unchanged=true native=false GPU=false art=false full_qualification=false\n"<<std::flush;
    return complete?0:2;
}
}
int wmain(int argc,wchar_t** argv){try{
    Require(argc==1||(argc==3&&(std::wstring_view(argv[1])==L"--runtime-assets"||std::wstring_view(argv[1])==L"--forest-support")),"USAGE [--runtime-assets|--forest-support ABS]");Controls();
    if(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets")ActualSource(std::filesystem::path(argv[2]).lexically_normal());
    std::cout<<"ROOT_SUPPORT_MATERIAL_FOOT_CPU_PASS profile=source-material-foot-v1 positives="<<positives<<" negatives="<<negatives
        <<" legacy_horizontal_base_unchanged=true actual16=false native=false GPU=false art=false full_qualification=false\n";
    if(argc==3&&std::wstring_view(argv[1])==L"--forest-support")return ForestSupport(std::filesystem::path(argv[2]).lexically_normal());return 0;
}catch(const std::exception& e){std::cerr<<"ROOT_SUPPORT_FIRST_DIVERGENCE "<<e.what()<<" native=false GPU=false art=false\n";return 1;}}
