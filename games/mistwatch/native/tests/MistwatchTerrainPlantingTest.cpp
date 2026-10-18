#include "MistwatchTerrainPlanting.h"
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace Aevrynd;
using namespace Mistwatch;
namespace D=::Mistwatch::LandscapeSpeciesDetail;
namespace P=::Mistwatch::TerrainPlantingDetail;
namespace {
unsigned negatives{};
void Require(bool value,const char* label){if(!value)throw std::runtime_error(label);}
void Word(std::uint64_t& h,std::uint32_t w){for(unsigned i=0;i<4;++i){h^=(w>>(8*i))&255u;h*=1099511628211ull;}}
std::array<std::uint32_t,9> PoseWords(const Transform3D& p){return{std::bit_cast<std::uint32_t>(p.position.x),std::bit_cast<std::uint32_t>(p.position.y),
    std::bit_cast<std::uint32_t>(p.position.z),std::bit_cast<std::uint32_t>(p.rotation.x),std::bit_cast<std::uint32_t>(p.rotation.y),
    std::bit_cast<std::uint32_t>(p.rotation.z),std::bit_cast<std::uint32_t>(p.scale.x),std::bit_cast<std::uint32_t>(p.scale.y),std::bit_cast<std::uint32_t>(p.scale.z)};}
std::array<std::uint32_t,7> VertexWords(const Vertex3D& v){return{std::bit_cast<std::uint32_t>(v.x),std::bit_cast<std::uint32_t>(v.y),
    std::bit_cast<std::uint32_t>(v.z),v.normal,v.color,std::bit_cast<std::uint32_t>(v.u),std::bit_cast<std::uint32_t>(v.v)};}
std::uint64_t MeshIdentity(const Mesh3D& mesh){std::uint64_t h=14695981039346656037ull;Word(h,static_cast<unsigned>(mesh.vertices.size()));Word(h,static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})Word(h,std::bit_cast<std::uint32_t>(f));Word(h,v.normal);Word(h,v.color);}for(const auto i:mesh.indices)Word(h,i);return h;}
std::uint64_t SourceIdentity(std::span<const Transform3D> roots){std::uint64_t h=14695981039346656037ull;Word(h,static_cast<unsigned>(roots.size()));
    for(unsigned i=0;i<roots.size();++i){Word(h,i);for(const auto w:PoseWords(roots[i]))Word(h,w);}return h;}
std::uint64_t RenderIdentity(const TerrainPlantingPlan& plan){std::uint64_t h=14695981039346656037ull;
    for(const auto& p:plan.instances){Word(h,p.sourceOrdinal);Word(h,static_cast<unsigned>(p.species));for(const auto w:PoseWords(p.render))Word(h,w);}return h;}
template<class Action>void Reject(Action action,const char* label){bool rejected{};try{action();}catch(const std::invalid_argument& error){
    Require(std::string_view(error.what())==label,"TERRAIN_PLANTING_NEGATIVE_WRONG_REASON");rejected=true;}Require(rejected,"TERRAIN_PLANTING_NEGATIVE_ACCEPTED");++negatives;}
void Pin(const std::filesystem::path& path,std::size_t bytes,std::string_view expected){Require(std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)==bytes,"TERRAIN_PLANTING_ASSET_BYTES");
    std::ifstream input(path,std::ios::binary);std::vector<std::byte> raw(bytes);input.read(reinterpret_cast<char*>(raw.data()),static_cast<std::streamsize>(bytes));Require(bool(input),"TERRAIN_PLANTING_ASSET_READ");
    std::array<std::byte,32> digest;Require(WorldDeltaSha2563D(raw,digest),"TERRAIN_PLANTING_ASSET_HASH");std::ostringstream out;out<<std::hex<<std::setfill('0');
    for(const auto b:digest)out<<std::setw(2)<<std::to_integer<unsigned>(b);Require(out.str()==expected,"TERRAIN_PLANTING_ASSET_IDENTITY");}
bool SameFloat(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
std::array<std::uint32_t,7> BoundsWords(const MeshBounds3D& b){return{std::bit_cast<std::uint32_t>(b.center.x),std::bit_cast<std::uint32_t>(b.center.y),
    std::bit_cast<std::uint32_t>(b.center.z),std::bit_cast<std::uint32_t>(b.radius),std::bit_cast<std::uint32_t>(b.extents.x),
    std::bit_cast<std::uint32_t>(b.extents.y),std::bit_cast<std::uint32_t>(b.extents.z)};}
void SameProof(const D::GroundProof& a,const D::GroundProof& b){Require(a.result==b.result&&SameFloat(a.minimumLowerGap,b.minimumLowerGap)&&SameFloat(a.maximumBaseGap,b.maximumBaseGap),
    "TERRAIN_PLANTING_GROUND_RESULT_BITS");}
void VerifyDerived(const Mesh3D& source,const Mesh3D& derived){
    Require(source.vertices.size()==derived.vertices.size()&&source.indices==derived.indices,"TERRAIN_PLANTING_DERIVED_TOPOLOGY");
    std::vector<Vec3> normals(derived.vertices.size());std::vector<bool> touched(derived.vertices.size());
    for(unsigned f=0;f<derived.indices.size();f+=3){const auto a=derived.indices[f],b=derived.indices[f+1],c=derived.indices[f+2];
        const auto point=[&](unsigned i){const auto& v=derived.vertices[i];return Vec3{v.x,v.y,v.z};};
        const auto normal=Cross(point(b)-point(a),point(c)-point(a));Require(Length(normal)>0,"TERRAIN_PLANTING_DERIVED_NONZERO_FACE");
        normals[a]=normals[a]+normal;normals[b]=normals[b]+normal;normals[c]=normals[c]+normal;
        if(!SameFloat(source.vertices[a].y,derived.vertices[a].y)||!SameFloat(source.vertices[b].y,derived.vertices[b].y)||!SameFloat(source.vertices[c].y,derived.vertices[c].y))touched[a]=touched[b]=touched[c]=true;}
    for(unsigned i=0;i<source.vertices.size();++i){const auto& a=source.vertices[i];const auto& b=derived.vertices[i];
        Require(SameFloat(a.x,b.x)&&SameFloat(a.z,b.z)&&SameFloat(a.u,b.u)&&SameFloat(a.v,b.v)&&a.color==b.color&&
            (a.normal&0xff000000u)==(b.normal&0xff000000u),"TERRAIN_PLANTING_DERIVED_XZ_UV_COLOR_HANDEDNESS");
        if(a.y>=.35f)Require(SameFloat(a.y,b.y),"TERRAIN_PLANTING_UPPER_POSITION_CHANGED");
        if(touched[i])Require(b.normal==PackNormalSnorm8(Normalize(normals[i]),UnpackNormalSnorm8(a.normal).w),"TERRAIN_PLANTING_CONNECTED_FACE_NORMAL");
        else Require(a.normal==b.normal,"TERRAIN_PLANTING_UNAFFECTED_NORMAL_CHANGED");}
}
Mesh3D Plane(float slope=0){Mesh3D mesh;const auto n=PackNormalSnorm8({0,1,0});
    for(const auto p:std::array<Vec3,4>{{{0,0,0},{0,0,2},{2,2*slope,0},{2,2*slope,2}}})mesh.vertices.push_back({p.x,p.y,p.z,n,0xffffffffu,0,0});mesh.indices={0,1,2,2,1,3};return mesh;}
Mesh3D Trunk(){Mesh3D mesh;const auto n=PackNormalSnorm8({1,0,0});
    for(float y:{0.f,1.f})for(const auto p:std::array<Vec3,4>{{{-.2f,0,-.2f},{-.2f,0,.2f},{.2f,0,.2f},{.2f,0,-.2f}}})mesh.vertices.push_back({p.x,y,p.z,n,0xffffffffu,.25f,.75f});
    for(unsigned i=0;i<4;++i){const unsigned j=(i+1)%4;mesh.indices.insert(mesh.indices.end(),{i,j,i+4,j,j+4,i+4});}return mesh;}
void Controls(){const auto flat=Plane(),slope=Plane(.02f),source=Trunk();const D::Surface flatSurface(flat),slopeSurface(slope);
    const Transform3D pose{{1,.02f,1},{},{1,1,1}};const auto derived=P::RootTrunkToLandscape(source,pose,slopeSurface);VerifyDerived(source,derived);
    Require(!SameFloat(derived.vertices[0].y,source.vertices[0].y),"TERRAIN_PLANTING_ACTUAL_ROOT_DEFORMATION");
    Require(SameFloat(derived.vertices[4].y,source.vertices[4].y)&&derived.vertices[4].normal!=source.vertices[4].normal,
        "TERRAIN_PLANTING_UPPER_ADJACENT_NORMAL_CONTROL");
    const auto a=D::Ground(pose,derived,slopeSurface,false),b=D::Ground(pose,derived,slopeSurface,true);SameProof(a,b);Require(a.result==D::GroundResult::Safe,"TERRAIN_PLANTING_ROOTED_SLOPE_CONTROL");
    std::cout<<"TERRAIN_PLANTING_GENTLE_SLOPE_CONTROL_PASS slope=.02 minimum_lower="<<a.minimumLowerGap<<" maximum_base="<<a.maximumBaseGap<<'\n'<<std::flush;
    // 原先 10% 斜面夾具的低側水平裁切帶懸空約 55mm，保留真拒絕，不放寬 40mm 門。
    const auto band=Plane(.1f);const D::Surface bandSurface(band);const Transform3D bandPose{{1,.1f,1},{},{1,1,1}};
    const auto bandDerived=P::RootTrunkToLandscape(source,bandPose,bandSurface);
    const auto bandReference=D::Ground(bandPose,bandDerived,bandSurface,false),bandPruned=D::Ground(bandPose,bandDerived,bandSurface,true);
    SameProof(bandReference,bandPruned);Require(bandReference.result==D::GroundResult::Ground,"TERRAIN_PLANTING_BASE_BAND_LIMIT_CONTROL");++negatives;
    auto floating=pose;floating.position.y+=.05f;Require(D::Ground(floating,derived,slopeSurface).result==D::GroundResult::Ground,"TERRAIN_PLANTING_FLOAT_CONTROL");++negatives;
    auto penetrating=pose;penetrating.position.y-=.05f;Require(D::Ground(penetrating,derived,slopeSurface).result==D::GroundResult::Ground,"TERRAIN_PLANTING_PENETRATION_CONTROL");++negatives;
    const auto steep=Plane(.3f);const D::Surface steepSurface(steep);const Transform3D steepPose{{1,.3f,1},{},{1,1,1}};
    const auto steepDerived=P::RootTrunkToLandscape(source,steepPose,steepSurface);Require(D::Ground(steepPose,steepDerived,steepSurface).result==D::GroundResult::Slope,"TERRAIN_PLANTING_SLOPE_NOT_RELAXED");++negatives;
    auto invalidPose=pose;invalidPose.scale.x=-1;Reject([&]{(void)P::RootTrunkToLandscape(source,invalidPose,slopeSurface);},"TERRAIN_PLANTING_ROOT_POSE");
    auto invalid=source;invalid.vertices[0].x=std::numeric_limits<float>::quiet_NaN();Reject([&]{(void)P::RootTrunkToLandscape(invalid,pose,flatSurface);},"LANDSCAPE_SPECIES_NONFINITE_MESH");
}
constexpr std::array<TerrainPlantingAnchor,16> ExpectedAnchors{{
    {-58.5f,2,.30f,1.04f},{-58.5f,16,2.20f,1.08f},{59.5f,9,5.08f,1.05f},{59.5f,23,.90f,1.07f},
    {-58.5f,-12,1.40f,1.06f},{-58.5f,30,3.40f,1.03f},{-58.5f,44,5.60f,1.08f},{-58.5f,58,.70f,1.05f},
    {-58.5f,72,2.70f,1.04f},{-58.5f,88,4.80f,1.07f},{59.5f,-5,2.00f,1.06f},{59.5f,37,4.10f,1.03f},
    {59.5f,51,.10f,1.08f},{59.5f,65,2.90f,1.05f},{59.5f,79,4.90f,1.04f},{59.5f,93,1.70f,1.07f}}};
void CheckPlan(const TerrainPlantingPlan& plan,std::span<const Transform3D> roots,const std::array<Mesh3D,3>& meshes){
    Require(plan.sourcePoseFingerprint==SourceIdentity(roots)&&plan.renderFingerprint==RenderIdentity(plan),"TERRAIN_PLANTING_PLAN_IDENTITIES");
    Require(plan.plants.size()<=16&&plan.complete==(plan.plants.size()==16),"TERRAIN_PLANTING_EXPLICIT_TARGET16");std::array<bool,520> selected{};unsigned first=~0u;
    std::array<bool,520> used{};
    for(unsigned i=0;i<16;++i){const auto& anchor=ExpectedAnchors[i];const auto& diagnostic=plan.diagnostics[i];unsigned nearest=~0u;double distance=144.;
        for(unsigned j=0;j<520;++j){if(used[j]||(roots[j].position.x<0)!=(anchor.x<0))continue;
            const double dx=double(anchor.x)-roots[j].position.x,dz=double(anchor.z)-roots[j].position.z,squared=dx*dx+dz*dz;
            if(squared<distance||(squared==distance&&(nearest==~0u||j<nearest))){distance=squared;nearest=j;}}
        Require(diagnostic.sourceOrdinal==nearest,"TERRAIN_PLANTING_ACTUAL_NEAREST_ORDINAL");
        if(diagnostic.result==TerrainPlantingResult::Accepted){Require(nearest<520,"TERRAIN_PLANTING_ACCEPTED_NO_SOURCE");used[nearest]=true;}
        else if(first==~0u)first=i;}
    Require(plan.firstMissingAnchor==first,"TERRAIN_PLANTING_FIRST_FAILURE");
    for(const auto& plant:plan.plants){Require(plant.anchorIndex<16&&plant.sourceOrdinal<520&&!selected[plant.sourceOrdinal],"TERRAIN_PLANTING_UNIQUE_ORDINAL");selected[plant.sourceOrdinal]=true;
        Require(plan.diagnostics[plant.anchorIndex].result==TerrainPlantingResult::Accepted&&plan.diagnostics[plant.anchorIndex].sourceOrdinal==plant.sourceOrdinal&&
            PoseWords(plan.instances[plant.sourceOrdinal].render)==PoseWords(plant.pose),"TERRAIN_PLANTING_ACCEPTED_RENDER_LINK");
        SameProof(plan.diagnostics[plant.anchorIndex].ground,plant.ground);
        const auto expected=ExpectedAnchors[plant.anchorIndex];const auto& p=plant.pose;
        Require(SameFloat(p.position.x,expected.x)&&SameFloat(p.position.z,expected.z)&&SameFloat(p.rotation.y,expected.yaw)&&p.rotation.x==0&&p.rotation.z==0&&
            SameFloat(p.scale.x,expected.scale)&&SameFloat(p.scale.y,expected.scale)&&SameFloat(p.scale.z,expected.scale),"TERRAIN_PLANTING_LITERAL_AUTHOR_POSE");
        Require(std::hypot(p.position.x-roots[plant.sourceOrdinal].position.x,p.position.z-roots[plant.sourceOrdinal].position.z)<=12.f,"TERRAIN_PLANTING_REPLACEMENT_DISTANCE");
        Require(PoseWords(p)==TerrainPlantingPoseWords(p)&&plant.meshFingerprint==MeshIdentity(plant.derivedTrunk),"TERRAIN_PLANTING_TRUE_MESH_POSE_IDENTITY");
        VerifyDerived(meshes[2],plant.derivedTrunk);Require(plant.ground.result==D::GroundResult::Safe,"TERRAIN_PLANTING_ACCEPTED_GROUND");
        Vec3 low{10000,10000,10000},high{-10000,-10000,-10000};
        for(unsigned slot=0;slot<3;++slot){const auto& mesh=slot==2?plant.derivedTrunk:meshes[slot];for(const auto& v:mesh.vertices){
            low.x=(std::min)(low.x,v.x);low.y=(std::min)(low.y,v.y);low.z=(std::min)(low.z,v.z);
            high.x=(std::max)(high.x,v.x);high.y=(std::max)(high.y,v.y);high.z=(std::max)(high.z,v.z);}}
        const auto half=(high-low)*.5f;const MeshBounds3D measured{(low+high)*.5f,Length(half),half};
        Require(BoundsWords(plant.finalBounds)==BoundsWords(TransformBounds(measured,p)),"TERRAIN_PLANTING_COMPLETE_DERIVED_BOUNDS");
        Require(plant.finalBounds.center.x+plant.finalBounds.extents.x< -53.f||plant.finalBounds.center.x-plant.finalBounds.extents.x>53.f||
            plant.finalBounds.center.z+plant.finalBounds.extents.z< -23.f||plant.finalBounds.center.z-plant.finalBounds.extents.z>103.f,
            "TERRAIN_PLANTING_FULL_BOUNDS_OUTSIDE_CORE");
        float minX=10000,maxX=-10000,minZ=10000,maxZ=-10000;
        const float cs=std::cos(p.rotation.y),sn=std::sin(p.rotation.y);
        for(unsigned slot=0;slot<3;++slot){const auto& mesh=slot==2?plant.derivedTrunk:meshes[slot];for(const auto& v:mesh.vertices){
            const float x=p.position.x+p.scale.x*(cs*v.x+sn*v.z),z=p.position.z+p.scale.x*(-sn*v.x+cs*v.z);
            minX=(std::min)(minX,x);maxX=(std::max)(maxX,x);minZ=(std::min)(minZ,z);maxZ=(std::max)(maxZ,z);}}
        Require(maxX< -53.f||minX>53.f||maxZ< -23.f||minZ>103.f,"TERRAIN_PLANTING_REAL_COMPLETE_CROWN_OUTSIDE_CORE");}
    std::uint64_t derivedIdentity=14695981039346656037ull;for(const auto& plant:plan.plants){Word(derivedIdentity,plant.anchorIndex);Word(derivedIdentity,plant.sourceOrdinal);
        Word(derivedIdentity,static_cast<std::uint32_t>(plant.meshFingerprint));Word(derivedIdentity,static_cast<std::uint32_t>(plant.meshFingerprint>>32));}
    Require(plan.derivedFingerprint==derivedIdentity,"TERRAIN_PLANTING_COMPLETE_DERIVED_IDENTITY");
    for(unsigned i=0;i<520;++i){Require(plan.instances[i].sourceOrdinal==i,"TERRAIN_PLANTING_SOURCE_ORDINAL_CHANGED");
        if(!selected[i])Require(plan.instances[i].species==LandscapeTreeSpecies::Fir&&PoseWords(plan.instances[i].render)==PoseWords(roots[i]),"TERRAIN_PLANTING_NONSELECTED_POSE_CHANGED");
        else Require(plan.instances[i].species==LandscapeTreeSpecies::Broadleaf,"TERRAIN_PLANTING_SELECTED_SPECIES");
        for(unsigned j=0;j<i;++j)Require(std::hypot(plan.instances[i].render.position.x-plan.instances[j].render.position.x,
            plan.instances[i].render.position.z-plan.instances[j].render.position.z)>=5.f,"TERRAIN_PLANTING_RETAINED_ROOT_SPACING");}
}
void Repeat(const TerrainPlantingPlan& a,const TerrainPlantingPlan& b){Require(a.complete==b.complete&&a.firstMissingAnchor==b.firstMissingAnchor&&
    a.sourcePoseFingerprint==b.sourcePoseFingerprint&&a.renderFingerprint==b.renderFingerprint&&a.derivedFingerprint==b.derivedFingerprint&&a.plants.size()==b.plants.size(),"TERRAIN_PLANTING_REPEAT_IDENTITIES");
    for(unsigned i=0;i<520;++i)Require(a.instances[i].sourceOrdinal==b.instances[i].sourceOrdinal&&a.instances[i].species==b.instances[i].species&&PoseWords(a.instances[i].render)==PoseWords(b.instances[i].render),"TERRAIN_PLANTING_REPEAT_POSE_WORDS");
    for(unsigned i=0;i<16;++i){const auto& x=a.diagnostics[i];const auto& y=b.diagnostics[i];Require(x.result==y.result&&x.sourceOrdinal==y.sourceOrdinal&&
        SameFloat(x.sourceDistance,y.sourceDistance)&&SameFloat(x.slope,y.slope)&&SameFloat(x.rootError,y.rootError)&&x.meshFingerprint==y.meshFingerprint,
        "TERRAIN_PLANTING_REPEAT_DIAGNOSTICS");SameProof(x.ground,y.ground);Require(x.ground.evaluatedIntersections==y.ground.evaluatedIntersections&&x.ground.skippedLowerFaces==y.ground.skippedLowerFaces,"TERRAIN_PLANTING_REPEAT_COMPUTATION");}
    for(unsigned i=0;i<a.plants.size();++i){const auto& x=a.plants[i];const auto& y=b.plants[i];Require(x.sourceOrdinal==y.sourceOrdinal&&x.anchorIndex==y.anchorIndex&&
        PoseWords(x.pose)==PoseWords(y.pose)&&BoundsWords(x.finalBounds)==BoundsWords(y.finalBounds)&&x.meshFingerprint==y.meshFingerprint&&x.derivedTrunk.indices==y.derivedTrunk.indices&&x.derivedTrunk.vertices.size()==y.derivedTrunk.vertices.size(),"TERRAIN_PLANTING_REPEAT_DERIVED");
        for(unsigned v=0;v<x.derivedTrunk.vertices.size();++v)Require(VertexWords(x.derivedTrunk.vertices[v])==VertexWords(y.derivedTrunk.vertices[v]),"TERRAIN_PLANTING_REPEAT_DERIVED_VERTEX_WORDS");}
}
// 有限原樹位的真三角支撐盤點；矩形腳印不是最終樹幹證書，也不是渲染成功。
void Inventory(std::span<const Transform3D> roots,const std::array<Mesh3D,3>& meshes,const Mesh3D& landscape){
    const D::Surface surface(landscape);const auto sourceBounds=D::Measure(meshes),lower=BroadleafLowerTrunkBounds();unsigned suitable{};
    std::cout<<std::setprecision(std::numeric_limits<float>::max_digits10);
    for(unsigned ordinal=0;ordinal<roots.size();++ordinal){auto pose=roots[ordinal];pose.scale={1.05f,1.05f,1.05f};
        const auto root=surface.At(pose.position);pose.position.y=static_cast<float>(root.Approximate());
        const auto matrix=pose.Matrix();std::array<D::P,4> box;
        for(unsigned c=0;c<4;++c){const float x=lower.center.x+(c==0||c==3?-lower.extents.x:lower.extents.x);
            const float z=lower.center.z+(c<2?-lower.extents.z:lower.extents.z);
            box[c]=D::Exact::ToExact(TransformPoint(matrix,{x,0,z}));}
        float minimumNormal=1;D::R maximumDelta;unsigned intersections{};bool covered=true;
        for(unsigned k=1;k<3;++k){const D::T footprint{box[0],box[k],box[k+1]};D::R area;
            const auto expected=D::Abs(D::Area(footprint[0],footprint[1],footprint[2]));
            for(const auto f:surface.Query(footprint)){const auto triangle=surface.Face(f);const auto polygon=D::PlanarIntersection(footprint,triangle);
                for(const auto& p:polygon){const auto delta=D::Abs(D::Height(triangle,p)-root);
                    if(Compare(delta,maximumDelta)>0)maximumDelta=delta;++intersections;}
                bool nonzero{};for(unsigned i=1;i+1<polygon.size();++i)nonzero|=D::Area(polygon[0],polygon[i],polygon[i+1]).Sign()!=0;
                if(nonzero){minimumNormal=(std::min)(minimumNormal,surface.NormalY(f));const auto hull=D::Hull(polygon);
                    for(unsigned i=1;i+1<hull.size();++i)area=area+D::Abs(D::Area(hull[0],hull[i],hull[i+1]));}}
            covered&=Compare(area,expected)==0;}
        const auto bounds=TransformBounds(sourceBounds,pose);const bool outside=D::OutsideCore(bounds);
        const bool eligible=covered&&outside&&minimumNormal>=.97f&&Compare(maximumDelta,D::R::Float(.10f))<=0;
        suitable+=eligible;
        std::cout<<"TERRAIN_SUPPORT_ROW ordinal="<<ordinal<<" x="<<pose.position.x<<" y="<<pose.position.y<<" z="<<pose.position.z
            <<" yaw="<<pose.rotation.y<<" scale="<<pose.scale.x<<" normal_y="<<minimumNormal<<" maximum_root_delta="<<maximumDelta.Approximate()
            <<" covered="<<covered<<" outside_core="<<outside<<" box_eligible="<<eligible<<" intersections="<<intersections<<'\n';
    }
    std::cout<<"TERRAIN_SUPPORT_INVENTORY_PASS source=520 box_eligible="<<suitable<<" final_trunk_verified=false native=false GPU=false art=false\n"<<std::flush;
}
}
int wmain(int argc,wchar_t** argv){try{
    Require(argc==3&&(std::wstring_view(argv[1])==L"--runtime-assets"||std::wstring_view(argv[1])==L"--support-inventory"),"USAGE --runtime-assets|--support-inventory ABS");const auto assets=std::filesystem::path(argv[2]).lexically_normal();
    Require(assets.is_absolute()&&std::filesystem::is_directory(assets),"TERRAIN_PLANTING_RUNTIME_ROOT");
    constexpr std::array<std::size_t,3> bytes{163400,3704316,57760};constexpr std::array<std::string_view,3> hashes{
        "4182c1d2d3e726f5c77dd84adf59ea5d4e022d49b1f211b34b488dbc165b68cc","c46eca29c84d6ea9db320444d01ed4f16403a2a7f50729178e70c7886b28bc5b","1c765173ebc17a3be167c35ef1b8df8329fb4cf98f468f4da13fd42533b677fd"};
    std::array<Mesh3D,3> meshes;for(unsigned slot=0;slot<3;++slot){const auto path=assets/"mistwatch-broadleaf/meshes"/BroadleafMeshName(slot);Pin(path,bytes[slot],hashes[slot]);
        auto loaded=LoadAevryndMeshFile(path);Require(bool(loaded),"TERRAIN_PLANTING_SOURCE_LOAD");RootBroadleafMesh(loaded.mesh);meshes[slot]=std::move(loaded.mesh);}
    const auto core=CreateTerrain();const auto landscape=CreateLandscapeMesh(core);const auto roots=LandscapeTrees(core);const auto seal=SourceIdentity(roots),landSeal=MeshIdentity(landscape),coreSeal=MeshIdentity(core.CreateMesh(GroundMeshSettings()));
    std::array<std::uint64_t,3> meshSeals;for(unsigned i=0;i<3;++i)meshSeals[i]=MeshIdentity(meshes[i]);Controls();
    const auto original=CreateTerrainPlantingPlan(core,landscape,roots,meshes,seal);Require(original.complete&&original.plants.empty()&&original.firstMissingAnchor==~0u,"TERRAIN_PLANTING_ORIGINAL_FAST_EMPTY");
    for(unsigned i=0;i<520;++i)Require(original.instances[i].species==LandscapeTreeSpecies::Fir&&PoseWords(original.instances[i].render)==PoseWords(roots[i]),"TERRAIN_PLANTING_ORIGINAL_520_WORDS");
    Reject([&]{(void)CreateTerrainPlantingPlan(core,landscape,roots,meshes,seal,static_cast<TerrainPlantingProfile>(2));},"TERRAIN_PLANTING_INVALID_PROFILE");
    auto bad=roots;bad.pop_back();Reject([&]{(void)CreateTerrainPlantingPlan(core,landscape,bad,meshes,seal);},"TERRAIN_PLANTING_SOURCE_COUNT");
    bad=roots;bad[0].position.y=std::numeric_limits<float>::quiet_NaN();Reject([&]{(void)CreateTerrainPlantingPlan(core,landscape,bad,meshes,seal);},"TERRAIN_PLANTING_SOURCE_POSE");
    bad=roots;std::swap(bad[0],bad[1]);Reject([&]{(void)CreateTerrainPlantingPlan(core,landscape,bad,meshes,seal);},"TERRAIN_PLANTING_SOURCE_SEAL");
    bad=roots;bad[0].position.x=0;bad[0].position.z=0;Reject([&]{(void)CreateTerrainPlantingPlan(core,landscape,bad,meshes,SourceIdentity(bad));},"TERRAIN_PLANTING_SOURCE_DOMAIN");
    bad=roots;bad[1].position=bad[0].position;Reject([&]{(void)CreateTerrainPlantingPlan(core,landscape,bad,meshes,SourceIdentity(bad));},"TERRAIN_PLANTING_SOURCE_SPACING");
    std::cout<<"TERRAIN_PLANTING_ORIGINAL_CPU_PASS source=520 negatives="<<negatives<<" source_fingerprint="<<seal<<" native=false GPU=false art=false\n"<<std::flush;
    if(std::wstring_view(argv[1])==L"--support-inventory"){
        Inventory(roots,meshes,landscape);
        Require(SourceIdentity(roots)==seal&&MeshIdentity(landscape)==landSeal&&MeshIdentity(core.CreateMesh(GroundMeshSettings()))==coreSeal,"TERRAIN_SUPPORT_SOURCE_DRIFT");
        for(unsigned i=0;i<3;++i)Require(MeshIdentity(meshes[i])==meshSeals[i],"TERRAIN_SUPPORT_ASSET_DRIFT");return 0;
    }
    const auto candidate=CreateTerrainPlantingPlan(core,landscape,roots,meshes,seal,TerrainPlantingProfile::AuthoredEdgeRootedV1);CheckPlan(candidate,roots,meshes);
    for(unsigned i=0;i<16;++i){const auto& d=candidate.diagnostics[i];std::cout<<"TERRAIN_PLANTING_ANCHOR index="<<i<<" result="<<TerrainPlantingResultName(d.result)<<" ordinal="<<d.sourceOrdinal
        <<" slope="<<d.slope<<" root_error="<<d.rootError<<" source_distance="<<d.sourceDistance<<" mesh_fingerprint="<<d.meshFingerprint
        <<" ground="<<static_cast<unsigned>(d.ground.result)<<" evaluated_intersections="<<d.ground.evaluatedIntersections<<" skipped_lower_faces="<<d.ground.skippedLowerFaces<<'\n';}
    for(const auto& plant:candidate.plants){std::cout<<"TERRAIN_PLANTING_POSE anchor="<<plant.anchorIndex<<" source_ordinal="<<plant.sourceOrdinal<<" pose_words=";
        for(const auto w:PoseWords(plant.pose))std::cout<<w<<',';std::cout<<" mesh_fingerprint="<<plant.meshFingerprint
            <<" vertices="<<plant.derivedTrunk.vertices.size()<<" indices="<<plant.derivedTrunk.indices.size()
            <<" minimum_lower_word="<<std::bit_cast<std::uint32_t>(plant.ground.minimumLowerGap)
            <<" maximum_base_word="<<std::bit_cast<std::uint32_t>(plant.ground.maximumBaseGap)<<'\n';}std::cout<<std::flush;
    const auto again=CreateTerrainPlantingPlan(core,landscape,roots,meshes,seal,TerrainPlantingProfile::AuthoredEdgeRootedV1);CheckPlan(again,roots,meshes);Repeat(candidate,again);
    unsigned actualReference{};if(!candidate.plants.empty()){const auto& tree=candidate.plants.front();const D::Surface surface(landscape);
        const auto reference=D::Ground(tree.pose,tree.derivedTrunk,surface,false),pruned=D::Ground(tree.pose,tree.derivedTrunk,surface,true);SameProof(reference,pruned);
        Require(reference.result==D::GroundResult::Safe,"TERRAIN_PLANTING_REAL_ROOT_REFERENCE");++actualReference;
        std::cout<<"TERRAIN_PLANTING_REAL_ROOT_REFERENCE_PASS ordinal="<<tree.sourceOrdinal<<" min_word="<<std::bit_cast<std::uint32_t>(reference.minimumLowerGap)
            <<" max_word="<<std::bit_cast<std::uint32_t>(reference.maximumBaseGap)<<" reference_intersections="<<reference.evaluatedIntersections
            <<" pruned_intersections="<<pruned.evaluatedIntersections<<" skipped="<<pruned.skippedLowerFaces<<'\n';}
    Require(SourceIdentity(roots)==seal&&MeshIdentity(landscape)==landSeal&&MeshIdentity(core.CreateMesh(GroundMeshSettings()))==coreSeal,"TERRAIN_PLANTING_WORLD_MUTATED");
    for(unsigned i=0;i<3;++i)Require(MeshIdentity(meshes[i])==meshSeals[i],"TERRAIN_PLANTING_SOURCE_MUTATED");
    std::cout<<(candidate.complete?"TERRAIN_PLANTING_ACTUAL16_PASS":"TERRAIN_PLANTING_16_SHORTFALL")<<" target=16 actual="<<candidate.plants.size()<<" first_anchor="<<candidate.firstMissingAnchor
        <<" render_fingerprint="<<candidate.renderFingerprint<<" derived_fingerprint="<<candidate.derivedFingerprint<<" real_reference_controls="<<actualReference
        <<" native=false GPU=false art=false full_qualification=false\n";return candidate.complete?0:2;
}catch(const std::exception& error){std::cerr<<"TERRAIN_PLANTING_FIRST_DIVERGENCE "<<error.what()<<" native=false GPU=false art=false\n";return 1;}}
