#include "MistwatchEntranceCanopy.h"
#include "MistwatchEntranceWorldAuthor.h"
#include "MistwatchForestCoverRetention.h"
#include "MistwatchForestGrassShape.h"
#include "MistwatchNettleMix.h"
#include "MistwatchForestHabitatFloor.h"
#include "MistwatchLoadPreparation.h"
#include "MistwatchMiddleLayer.h"
#include "MistwatchFallenLogProfile.h"
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace Aevrynd;
using namespace Mistwatch;
namespace {
void Need(bool value,const char* gate){if(!value)throw std::runtime_error(gate);}
struct AssetPin {const char* relative;std::size_t bytes;const char* sha;};
constexpr AssetPin CanopyPins[]{
    {"mistwatch-canopy-preview/meshes/island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh",159612,"B964D1F7B16FB3602FF038AE111C79D0BFC029185896E95DA97CC1ACB4F59145"},
    {"mistwatch-canopy-preview/meshes/island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh",21375028,"1773F7705A43DA3B3673D0D34C66C2C4FEFC47952970CB84665A774D6A54F871"},
    {"mistwatch-canopy-preview/meshes/island-tree-01-lod2-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh",2709952,"5D32528C7604D041A0730755DDF765B111F82B43847C5F4AED0FE386E99D6188"},
    {"mistwatch-canopy-preview/meshes/island-tree-01-lod2-trunk-root-stem.aemesh",165000,"96EA5581E6A5D2A0C3260DEEBE614A4FA9BCCA0A77772E5EAE42E0C88730BC7C"}};
constexpr AssetPin FernPins[]{
    {"mistwatch-ferns/meshes/fern-02-lod0-node0-mesh0-primitive0-material0-fern-02.aemesh",75104,"83BEE63C50758AF9833AAF8CB0ABC06000DF21C412F4971BD582D64ADD4EAE81"},
    {"mistwatch-ferns/meshes/fern-02-lod0-node1-mesh1-primitive0-material0-fern-02.aemesh",70896,"07DAAD96517DA59466BE5551A6460F0943E5142B55CF2F7F0254938D92BA0306"},
    {"mistwatch-ferns/meshes/fern-02-lod0-node2-mesh2-primitive0-material0-fern-02.aemesh",24656,"78702057068C309D6C559F223FB282C29BAC32B8C7B40987C14338F5F65E7E5A"},
    {"mistwatch-ferns/meshes/fern-02-lod0-node3-mesh3-primitive0-material0-fern-02.aemesh",25628,"243A1BA9863EE362F8BEBCDDA7BA0DC44D847FF8684CAC327B68A422C26663F8"}};
constexpr AssetPin GrassPins[]{
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node0-mesh0-primitive0-material0-grass-medium-02.aemesh",26644,"84C8C7DA8EEB335B1BD60415531A30798933C688FF0D3A241ECA1958FB9C2E56"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node1-mesh1-primitive0-material0-grass-medium-02.aemesh",65236,"CE3EB8A245F7C36682FCABB106EBB56E79BEAE95D6345DF330AC2D2DD5F76D05"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node2-mesh2-primitive0-material0-grass-medium-02.aemesh",44612,"0AA49B54653A3B69A266CC7575AC8FF405A968E220BAC500A6BA3FAA3D432363"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node3-mesh3-primitive0-material0-grass-medium-02.aemesh",91400,"78ED34130EAC919ED41F02918676E68D4060713C52C570B9E4ABEBCD1AA47C5B"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node4-mesh4-primitive0-material0-grass-medium-02.aemesh",63160,"6B95323ACE660F307C8C1EBAB617B2D87118E773FAB4925B4B452B232389A689"}};
constexpr AssetPin ShrubPin{"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh",3704316,"C46ECA29C84D6EA9DB320444D01ED4F16403A2A7F50729178E70C7886B28BC5B"};
constexpr AssetPin NettlePins[]{
    {"mistwatch-nettle/meshes/nettle-plant-lod0-node0-mesh0-primitive0-material0-nettle-plant.aemesh",199148,"91556C49EB48AC45EE556975A08E4C31B4EC25B94E80208751C62FEB2F7EF195"},
    {"mistwatch-nettle/meshes/nettle-plant-lod0-node1-mesh1-primitive0-material0-nettle-plant.aemesh",189392,"4DA84D00D7EE5C86A3396D0522518661EB98E0A2E589907B3CFE6B4864D8117F"},
    {"mistwatch-nettle/meshes/nettle-plant-lod0-node2-mesh2-primitive0-material0-nettle-plant.aemesh",119728,"5C5D1C571D6FFD0CCF75D969C37ED34D8164D127C89EF28D2F94957548AB44D6"},
    {"mistwatch-nettle/meshes/nettle-plant-lod0-node3-mesh3-primitive0-material0-nettle-plant.aemesh",36844,"550AB1456DF945E1AF708B3D3B5C8613FE9D8FDCDE90CA4FD5AA69655BE706BC"},
    {"mistwatch-nettle/meshes/nettle-plant-lod0-node4-mesh4-primitive0-material0-nettle-plant.aemesh",231076,"D1043C22537C243329BD8610EC14692234F54AF0FD5AE49132D0B7540F550E58"},
    {"mistwatch-nettle/meshes/nettle-plant-lod0-node5-mesh5-primitive0-material0-nettle-plant.aemesh",272200,"ADDE7AA47A39F321ED4FA8AB8BC336D4E42EF319FF6F7CEF3BAF068F65F53369"}};
Mesh3D Load(const AssetPin& pin){
    const auto path=std::filesystem::path(MISTWATCH_RUNTIME_ASSETS)/pin.relative;
    Need(std::filesystem::file_size(path)==pin.bytes,"actual-asset-byte-count");
    std::ifstream stream(path,std::ios::binary);Need(bool(stream),"actual-asset-open");
    std::vector<std::byte> raw(pin.bytes);stream.read(reinterpret_cast<char*>(raw.data()),std::streamsize(raw.size()));
    Need(stream.gcount()==std::streamsize(raw.size()),"actual-asset-read");
    std::array<std::byte,32> digest{};Need(WorldDeltaSha2563D(raw,digest),"actual-asset-SHA-failed");
    std::ostringstream hex;hex<<std::uppercase<<std::hex<<std::setfill('0');
    for(auto byte:digest)hex<<std::setw(2)<<std::to_integer<unsigned>(byte);
    Need(hex.str()==pin.sha,"actual-asset-SHA-mismatch");
    auto loaded=LoadAevryndMeshFile(path);Need(bool(loaded),"actual-asset-native-load");
    static_assert(sizeof(Vertex3D)==28);
    std::array<std::uint32_t,4> header{};std::memcpy(header.data(),raw.data(),16);
    const auto vertexBytes=loaded.mesh.vertices.size()*sizeof(Vertex3D),indexBytes=loaded.mesh.indices.size()*sizeof(std::uint32_t);
    Need(header[0]==0x334d4541u&&header[1]==1&&header[2]==loaded.mesh.vertices.size()&&header[3]==loaded.mesh.indices.size()&&
        raw.size()==16+vertexBytes+indexBytes&&std::memcmp(raw.data()+16,loaded.mesh.vertices.data(),vertexBytes)==0&&
        std::memcmp(raw.data()+16+vertexBytes,loaded.mesh.indices.data(),indexBytes)==0,"raw-asset-to-native-mesh-identity");
    std::cout<<"PIN "<<pin.relative<<" bytes="<<pin.bytes<<" sha256="<<pin.sha<<'\n';
    return std::move(loaded.mesh);
}
bool Same(Vec3 a,Vec3 b){return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(b.x)&&
    std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(b.y)&&std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(b.z);}
bool Same(const Transform3D& a,const Transform3D& b){return Same(a.position,b.position)&&Same(a.rotation,b.rotation)&&Same(a.scale,b.scale);}
bool Same(const BoxCollider3D& a,const BoxCollider3D& b){return Same(a.center,b.center)&&Same(a.extents,b.extents);}
bool Same(const Mesh3D& a,const Mesh3D& b){return a.indices==b.indices&&a.vertices.size()==b.vertices.size()&&
    std::memcmp(a.vertices.data(),b.vertices.data(),a.vertices.size()*sizeof(Vertex3D))==0;}
std::uint64_t ColliderHash(std::span<const BoxCollider3D> boxes){std::uint64_t hash=14695981039346656037ull;
    for(const auto& b:boxes)for(float value:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z}){
        const auto word=std::bit_cast<std::uint32_t>(value);for(unsigned k=0;k<4;++k){hash^=(word>>(k*8))&255u;hash*=1099511628211ull;}}
    return hash;}
template<class Work>void Reject(Work work,const char* text){bool rejected{};try{work();}catch(const std::exception&){rejected=true;}Need(rejected,text);}
std::uint64_t ActualGrassIdentity(const Mesh3D& mesh){
    std::uint64_t hash=14695981039346656037ull;
    const auto word=[&](std::uint32_t v){for(unsigned byte=0;byte<4;++byte){hash^=(v>>(byte*8))&255u;hash*=1099511628211ull;}};
    word(unsigned(mesh.vertices.size()));word(unsigned(mesh.indices.size()));
    for(const auto& v:mesh.vertices){
        for(float f:{v.x,v.y,v.z,v.u,v.v})word(std::bit_cast<std::uint32_t>(f));
        word(v.color);word(v.normal);
    }
    for(auto index:mesh.indices)word(index);
    return hash;
}
void CoverRegions(const char* name,const ForestCoverResult& cover){
    std::array<unsigned,3> counts{};
    for(const auto& plant:cover.plants){const auto p=plant.transform.position;
        counts[0]+=p.z>=-10.f&&p.z<=20.f&&std::abs(p.x)>=4.5f&&std::abs(p.x)<=12.f;
        counts[1]+=p.z>=0.f&&p.z<=26.f&&std::abs(p.x)>=4.5f&&std::abs(p.x)<=16.f;
        counts[2]+=p.x>=-24.f&&p.x<=-8.f&&p.z>=20.f&&p.z<=52.f;
    }
    std::cout<<"COVER_REGION mode="<<name<<" entrance_shoulder="<<counts[0]
        <<" wide_shoulder="<<counts[1]<<" dry_bank="<<counts[2]<<" patches="<<cover.patches.size()<<'\n';
}
// 預期值來自未修改的原 generator 及既有實拍收據；新增人口不從 native 新結果讀回作期望值。
void PreserveFour(const CanopyPopulation& before,const CanopyPopulation& after){
    Need(before.fingerprint==6584434718760703877ull&&before.plants.size()==4&&after.plants.size()==8,"four-plus-four-cohort");
    for(std::size_t i=0;i<4;++i){const auto& a=before.plants[i];const auto& b=after.plants[i];
        Need(Same(a.pose,b.pose)&&Same(a.lower,b.lower)&&a.crownRadius==b.crownRadius&&a.rootRadius==b.rootRadius&&
            Same(a.rootedTrunk,b.rootedTrunk),"original-four-full-bit-identity-changed");}
}
// 完整面／真地形驗證用現有 RootGroundOverlay，不使用候選 CheckGround 的通過旗標。
void GeometryGround(const Mesh3D& mesh,const Transform3D& pose,const HeightField3D& terrain,float gate,bool cap){
    const auto m=pose.Matrix();float highest=-(std::numeric_limits<float>::infinity)();
    Vec3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};std::vector<Vec3> vertices;
    for(const auto& vertex:mesh.vertices){const auto p=TransformPoint(m,{vertex.x,vertex.y,vertex.z});
        Need(Session::Finite(p)&&terrain.Contains(p.x,p.z),"independent-full-mesh-terrain-domain");vertices.push_back(p);Expand(low,high,p);}
    const auto& grid=terrain.Settings();
    const int x0=std::clamp(int(std::floor((low.x-grid.origin.x)/grid.cellSize)),0,int(grid.width)-2);
    const int z0=std::clamp(int(std::floor((low.z-grid.origin.z)/grid.cellSize)),0,int(grid.depth)-2);
    const int x1=std::clamp(int(std::ceil((high.x-grid.origin.x)/grid.cellSize)),1,int(grid.width)-1);
    const int z1=std::clamp(int(std::ceil((high.z-grid.origin.z)/grid.cellSize)),1,int(grid.depth)-1);
    for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x)highest=(std::max)(highest,terrain.Height(unsigned(x),unsigned(z)));
    unsigned capFaces{};
    for(std::size_t i=0;i<mesh.indices.size();i+=3){const std::array<Vec3,3> face{{vertices[mesh.indices[i]],vertices[mesh.indices[i+1]],vertices[mesh.indices[i+2]]}};
        const bool capFace=cap&&i/3>=5256u;
        if(!capFace&&(std::min)({face[0].y,face[1].y,face[2].y})-highest>=gate)continue;
        const auto range=RootGroundOverlay(face,terrain);Need(range.minimum>=gate,"independent-full-triangle-penetration");
        if(capFace){++capFaces;Need(range.maximum<=-.005f&&Cross(face[1]-face[0],face[2]-face[0]).y<0,"independent-root-cap-ground-seal");}
    }
    if(cap)Need(capFaces==74,"actual-complete-cap-face-count");
}
void ActualPlantWood(const Mesh3D& mesh,const Transform3D& pose,std::span<const BoxCollider3D> wood){
    const auto m=pose.Matrix();Vec3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};
    for(const auto& v:mesh.vertices)Expand(low,high,TransformPoint(m,{v.x,v.y,v.z}));
    for(const auto& b:wood){const auto a=b.center-b.extents,c=b.center+b.extents;
        Need(high.x<a.x||low.x>c.x||high.y<a.y||low.y>c.y||high.z<a.z||low.z>c.z,"actual-static-vegetation-vs-full-wood-grid");}
}
void BodyCorridor(Vec3 a,Vec3 b,const HeightField3D& terrain,std::span<const BoxCollider3D> newWood){
    const unsigned steps=unsigned(std::ceil(Length(b-a)/.1f));Need(steps<=1500,"finite-route-budget");
    for(unsigned k=0;k<=steps;++k){auto feet=a+(b-a)*(steps?float(k)/float(steps):0.f);
        feet.y=terrain.SampleHeight(feet.x,feet.z);const auto body=feet+Vec3{0,.9f,0},eye=feet+Vec3{0,1.6f,0};
        for(const auto& box:newWood){const auto d=body-box.center,e=eye-box.center;
            Need(std::abs(d.x)>=.45f+box.extents.x||std::abs(d.y)>=.9f+box.extents.y||std::abs(d.z)>=.45f+box.extents.z,"new-wood-blocks-mission-body-corridor");
            Need(std::abs(e.x)>=.25f+box.extents.x||std::abs(e.y)>=.25f+box.extents.y||std::abs(e.z)>=.25f+box.extents.z,"new-wood-blocks-mission-eye-clearance");}
    }
}

// 新世界期望退讓 ID 直接來自 Root 核准清單，不從候選函式輸出倒填。
void SharedWorldChecks(const EntranceWorldAuthor& author,const MatureFirLayout& sourceFirs,
    std::span<const ForestProp> sourceProps,const ForestUnderstory& sourceUnderstory,
    std::span<const BroadleafTree> sourceBroadleaf,const MiddleLayerResult& sourceMiddle,
    const HeightField3D& terrain){
    constexpr std::array<unsigned,5> removedFir{{7,47,63,94,95}};
    constexpr std::array<unsigned,3> removedRock{{12,17,31}};
    constexpr std::array<unsigned,2> removedSapling{{15,18}};
    const auto removed=[](auto ids,unsigned id){return std::find(ids.begin(),ids.end(),id)!=ids.end();};
    Need(author.applied&&author.firs.firs.size()==95&&author.firs.roots.size()==95&&author.props.size()==33&&
        author.understory.saplings.size()==40&&author.middle.trees.size()==23&&author.broadleaf.size()==12&&
        author.understory.ferns.size()==420,"authored-cohort-true-counts");
    const auto mappedProps=ApplyFallenLogProfile(sourceProps,FirLayoutSelector::ForestWood,FallenLogProfile::ScannedStraightV1);
    std::size_t j{};
    for(unsigned id=0;id<100;++id)if(!removed(removedFir,id)){
        const auto& before=sourceFirs.firs[id];const auto& after=author.firs.firs[j];
        Need(author.firSourceIds[j]==id&&Same(before.root,after.root)&&Same(sourceFirs.roots[id],author.firs.roots[j])&&
            Same(before.render,after.render)&&Same(before.collider,after.collider)&&before.patch==after.patch,
            "every-retained-fir-render-root-body-bit-identity");++j;
    }
    j=0;
    for(unsigned id=0;id<36;++id)if(!removed(removedRock,id)){
        Need(author.propSourceIds[j]==id&&FallenLogDetail::Words(author.props[j])==
            FallenLogDetail::Words(mappedProps[id]),"every-retained-prop-source-role-full-identity");++j;
    }
    unsigned retainedLogs{};
    for(std::size_t i=0;i<author.props.size();++i)if(author.props[i].FallenLog()){
        const unsigned source=author.propSourceIds[i];Need(source==0||source==8||source==34||source==35,
            "fallen-scene-id-original-source-role");++retainedLogs;
    }
    Need(retainedLogs==4,"all-four-scanned-logs-preserved");
    j=0;
    for(unsigned id=0;id<42;++id)if(!removed(removedSapling,id)){
        Need(author.saplingSourceIds[j]==id&&Same(author.understory.saplings[j],sourceUnderstory.saplings[id])&&
            Same(SaplingCollider(author.understory.saplings[j]),SaplingCollider(sourceUnderstory.saplings[id])),
            "every-retained-sapling-render-body-bit-identity");++j;
    }
    j=0;
    for(unsigned id=0;id<24;++id)if(id!=16){
        Need(author.middleSourceIds[j]==id&&Same(author.middle.trees[j].transform,sourceMiddle.trees[id].transform)&&
            Same(author.middle.trees[j].trunk,sourceMiddle.trees[id].trunk),"every-retained-middle-render-body-bit-identity");++j;
    }
    for(unsigned id=0;id<12;++id)Need(Same(author.broadleaf[id].transform,sourceBroadleaf[id].transform)&&
        Same(author.broadleaf[id].trunk,sourceBroadleaf[id].trunk),"all-retained-broadleaf-bits");
    for(unsigned id=0;id<420;++id){
        const auto& before=sourceUnderstory.ferns[id];const auto& after=author.understory.ferns[id];
        float dx{},dz{};
        switch(id){case 114:dx=-1.5f;dz=2;break;case 127:dx=2;dz=2;break;
            case 281:dx=1.5f;dz=-2;break;case 287:dx=-1.5f;dz=-1;break;
            case 399:dx=-2;dz=1.5f;break;}
        if(dx==0)Need(Same(before,after),"unmoved-415-fern-full-bit-identity");
        else{
            Need(after.position.x==before.position.x+dx&&after.position.z==before.position.z+dz&&
                after.position.y==terrain.SampleHeight(after.position.x,after.position.z)&&
                Same(before.rotation,after.rotation)&&Same(before.scale,after.scale),"five-exact-fern-author-relocations");
        }
    }
    // 由原完整世界序列逐角色 ID 獨立刪除；橋面前插只移動序號，不改 source ID。
    for(bool repaired:{false,true}){
        const auto original=WorldSolids(repaired,FirLayoutSelector::ForestWood,FallenLogProfile::ScannedStraightV1);
        const std::size_t mission=33+(repaired?1:0);std::vector<BoxCollider3D> expected;
        expected.insert(expected.end(),original.begin(),original.begin()+mission);
        for(unsigned id=0;id<100;++id)if(!removed(removedFir,id))expected.push_back(original[mission+id]);
        for(unsigned id=0;id<36;++id)if(!removed(removedRock,id))expected.push_back(original[mission+100+id]);
        for(unsigned id=0;id<42;++id)if(!removed(removedSapling,id))expected.push_back(original[mission+136+id]);
        for(unsigned id=0;id<12;++id)expected.push_back(original[mission+178+id]);
        const auto runtime=author.WorldSolids(repaired);
        Need(runtime.size()==expected.size()&&runtime.size()==213+(repaired?1:0),"runtime-bridge-filtered-role-counts");
        for(std::size_t i=0;i<expected.size();++i)Need(Same(expected[i],runtime[i]),"runtime-body-camera-every-source-box");
        for(unsigned id=0;id<24;++id)if(id!=16)expected.push_back(sourceMiddle.trees[id].trunk);
        const auto vegetation=author.VegetationSolids(repaired);Need(vegetation.size()==expected.size(),"vegetation-shared-world-count");
        for(std::size_t i=0;i<expected.size();++i)Need(Same(expected[i],vegetation[i]),"vegetation-every-runtime-source-box");
    }
}

// 以既有有效 SaveGame／Explorer／Boar fixture 實際預檢及交換提交，不只呼叫 WorldSolids。
void AuthoredLoadCommit(const EntranceWorldAuthor& author,const HeightField3D& terrain,
    std::span<const BoxCollider3D> wood){
    SaveGame saved;Need(saved.Valid(),"existing-valid-load-snapshot");const auto serialized=saved.Serialize();
    CharacterController3D controller;Explorer explorer;Boar boar;OrbitCamera orbit{-.65f,.3f,6};
    const LoadPreparationInputs inputs{controller,explorer,boar,orbit};
    // 保留未提供作者指標的既有 overload 及原 Legacy 路徑。
    auto legacy=PrepareSavedGame(saved,inputs,terrain);
    if(!legacy)std::cout<<"LOAD_FIRST_DIVERGENCE route=default-legacy error="<<int(legacy.error)<<'\n';
    Need(bool(legacy),"default-legacy-actual-load-preparation");
    const auto oldWorld=WorldSolids(false);
    Need(legacy.candidate->solids.size()==oldWorld.size(),"default-legacy-load-world-count");
    for(std::size_t i=0;i<oldWorld.size();++i)
        Need(Same(legacy.candidate->solids[i],oldWorld[i]),"default-legacy-load-every-box");
    std::vector<BoxCollider3D> extra;for(const auto& tree:author.middle.trees)extra.push_back(tree.trunk);
    extra.insert(extra.end(),wood.begin(),wood.end());
    auto expected=author.VegetationSolids(false);expected.insert(expected.end(),wood.begin(),wood.end());
    auto prepared=PrepareSavedGame(saved,inputs,terrain,{},extra,FirLayoutSelector::ForestWood,
        FallenLogProfile::ScannedStraightV1,&author);
    if(!prepared)std::cout<<"LOAD_FIRST_DIVERGENCE route=authored error="<<int(prepared.error)<<'\n';
    Need(bool(prepared),"authored-actual-load-preparation");
    Need(saved.Serialize()==serialized&&prepared.candidate->snapshot.Serialize()==serialized,
        "authored-preparation-snapshot-unchanged");
    Need(prepared.candidate->solids.size()==expected.size(),"authored-load-runtime-middle-wood-count");
    for(std::size_t i=0;i<expected.size();++i)
        Need(Same(prepared.candidate->solids[i],expected[i]),"authored-load-every-runtime-middle-wood-box");
    Need(prepared.candidate->cameraResult.accepted&&prepared.candidate->cameraResult.subjectVisible&&
        prepared.candidate->cameraResult.status==ThirdPersonCameraMotionStatus3D::ResetCut,
        "authored-load-actual-camera-validation");
    std::vector<BoxCollider3D> liveSolids=oldWorld;ThirdPersonCameraMotion3D motion;
    ThirdPersonFramingResult3D framing;ThirdPersonCameraMotionResult3D result;Camera3D rendered;
    SwapPreparedSavedGame(*prepared.candidate,{saved.session,saved.encounter,saved.evasion,controller,explorer,
        boar,orbit,liveSolids,motion,framing,result,rendered});
    Need(saved.Serialize()==serialized&&Same(controller.Position(),saved.session.feet),
        "authored-commit-preserves-valid-snapshot-feet");
    Need(liveSolids.size()==expected.size(),"authored-commit-no-old-box-revival-count");
    for(std::size_t i=0;i<expected.size();++i)
        Need(Same(liveSolids[i],expected[i]),"authored-commit-no-old-box-revival-every-box");
    Need(result.accepted&&result.subjectVisible&&result.status==ThirdPersonCameraMotionStatus3D::ResetCut,
        "authored-commit-keeps-validated-camera");
}

}
int main(){try{
    std::array<Mesh3D,3> meshes;for(unsigned i=0;i<3;++i)meshes[i]=Load(CanopyPins[i]);
    const auto derived=Load(CanopyPins[3]);const auto geometry=MeasureCanopy(meshes);
    Need(geometry.triangles==606094&&derived.vertices.size()==3608&&derived.indices.size()==15990,"frozen-canopy-source-counts");
    const auto terrain=CreateTerrain();const auto firs=CreateMatureFirLayout(terrain,FirLayoutSelector::ForestWood);
    Need(firs.reachedBudget&&firs.fingerprint==8236695966185867318ull&&firs.firs.size()==100,"original-fir-world-identity");
    const auto props=ForestProps(terrain,firs.roots);
    const auto understory=CreateUnderstory(terrain,firs.roots,props);
    const auto broadleaf=CreateBroadleafTrees(terrain,firs.roots,props,understory);
    std::array<Mesh3D,4> fernMeshes;std::array<FernColonyAsset,4> fernAssets;
    for(unsigned i=0;i<4;++i){fernMeshes[i]=Load(FernPins[i]);NormalizeFernAsset(fernMeshes[i]);fernAssets[i]=FernColonyAssetFromMesh(fernMeshes[i]);}
    auto authorSolids=WorldSolids(false,FirLayoutSelector::ForestWood);
    const std::array<float,4> radii{{fernAssets[0].radius,fernAssets[1].radius,fernAssets[2].radius,fernAssets[3].radius}};
    const auto middle=CreateMiddleLayer(terrain,firs.roots,props,understory,broadleaf,authorSolids,24,radii);
    Need(middle.reachedBudget&&middle.trees.size()==24&&MiddleLayerTransformFingerprint(middle.trees)==10337542786750772359ull,"original-middle-layer-world-identity");
    for(const auto& p:middle.trees)authorSolids.push_back(p.trunk);
    const auto original=CreateCanopyPopulation(meshes,geometry,terrain,authorSolids,understory,{},{},fernAssets,{});
    Need(original.reachedBudget&&original.fingerprint==6584434718760703877ull,"untouched-generator-original-four");
    std::cout<<std::setprecision(10)<<"ORIGINAL_FOUR fingerprint="<<original.fingerprint<<'\n';
    for(std::size_t i=0;i<original.plants.size();++i){const auto& p=original.plants[i];std::cout<<"ORIGINAL_POSE index="<<i<<" xyz="<<p.pose.position.x<<','<<p.pose.position.y<<','<<p.pose.position.z<<" yaw="<<p.pose.rotation.y<<" scale="<<p.pose.scale.x<<'\n';}
    // 首差即停止：不改錨點或替換模型，不以成功的舊四株冒充新八株完成。
    const auto author=CreateEntranceWorldAuthor(terrain,firs,props,understory,broadleaf,middle,fernAssets,
        FallenLogProfile::ScannedStraightV1);
    SharedWorldChecks(author,firs,props,understory,broadleaf,middle,terrain);
    Need(firs.firs.size()==100&&firs.roots.size()==100&&props.size()==36&&understory.ferns.size()==420&&
        understory.saplings.size()==42&&middle.trees.size()==24,"original-source-cohorts-not-mutated");
    auto invalidSource=firs;invalidSource.firs[7].render.position.x=
        std::nextafter(invalidSource.firs[7].render.position.x,INFINITY);
    Reject([&]{(void)CreateEntranceWorldAuthor(terrain,invalidSource,props,understory,broadleaf,middle,
        fernAssets,FallenLogProfile::ScannedStraightV1);},"stale-fir-author-role-accepted");
    auto invalidFern=understory;invalidFern.ferns[399].rotation.y=
        std::nextafter(invalidFern.ferns[399].rotation.y,INFINITY);
    Reject([&]{(void)CreateEntranceWorldAuthor(terrain,firs,props,invalidFern,broadleaf,middle,
        fernAssets,FallenLogProfile::ScannedStraightV1);},"stale-fern-author-role-accepted");
    auto invalidProps=props;invalidProps[12].transform.position.x=
        std::nextafter(invalidProps[12].transform.position.x,INFINITY);
    Reject([&]{(void)CreateEntranceWorldAuthor(terrain,firs,invalidProps,understory,broadleaf,middle,
        fernAssets,FallenLogProfile::ScannedStraightV1);},"stale-rock-author-role-accepted");
    Reject([&]{(void)EntranceWorldAuthor{}.WorldSolids(false);},"unapplied-author-world-accepted");
    auto live=author.VegetationSolids(false);
    for(std::size_t i=0;i<EntranceCanopyAnchors.size();++i){const auto a=EntranceCanopyAnchors[i];std::cout<<"REQUESTED_ANCHOR index="<<i<<" x="<<a.x<<" z="<<a.z<<" yaw="<<a.yaw<<" scale="<<a.scale<<'\n';}
    std::cout<<std::flush;
    EntranceCanopyMetadata metadata;const auto population=AppendEntranceCanopy(original,meshes,geometry,derived,terrain,live,author.understory,fernAssets,metadata);
    PreserveFour(original,population);Need(metadata.appended==4&&metadata.originalFingerprint==original.fingerprint,"actual-four-authored-appended");
    // 負控不執行錨點搜尋；所有無效輸入須在配置第一株之前被拒絕。
    auto badOriginal=original;badOriginal.fingerprint=0;
    Reject([&]{EntranceCanopyMetadata m;(void)AppendEntranceCanopy(badOriginal,meshes,geometry,derived,terrain,live,author.understory,fernAssets,m);},"bad-original-fingerprint-accepted");
    badOriginal=original;badOriginal.plants[0].pose.position.x=std::nextafter(badOriginal.plants[0].pose.position.x,INFINITY);
    Reject([&]{EntranceCanopyMetadata m;(void)AppendEntranceCanopy(badOriginal,meshes,geometry,derived,terrain,live,author.understory,fernAssets,m);},"original-pose-one-ULP-accepted");
    auto stale=geometry;stale.maximum.x+=1;
    Reject([&]{EntranceCanopyMetadata m;(void)AppendEntranceCanopy(original,meshes,stale,derived,terrain,live,author.understory,fernAssets,m);},"stale-complete-crown-accepted");
    auto invalid=live;invalid[0].center.x=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{EntranceCanopyMetadata m;(void)AppendEntranceCanopy(original,meshes,geometry,derived,terrain,invalid,author.understory,fernAssets,m);},"nonfinite-existing-collider-accepted");
    auto missing=derived;missing.indices.pop_back();
    Reject([&]{EntranceCanopyMetadata m;(void)AppendEntranceCanopy(original,meshes,geometry,missing,terrain,live,author.understory,fernAssets,m);},"missing-root-cap-index-accepted");
    std::vector<BoxCollider3D> wood,newWood,originalWood;std::uint64_t triangles{},surface{},enclosed{};unsigned newLeft{},newRight{};
    for(std::size_t i=0;i<population.plants.size();++i){const auto& p=population.plants[i];
        const auto root=PlantDerivedRootStem(derived,geometry,p.pose,terrain);
        const auto box=ExtendRootCollision(p.lower,root,p.pose);
        Need(box.extents.x==p.lower.extents.x&&box.extents.z==p.lower.extents.z&&box.center.x==p.lower.center.x&&box.center.z==p.lower.center.z,"horizontal-root-collision-shrank-or-moved");
        Need(root.indices==derived.indices,"actual-root-topology-changed");
        for(std::size_t j=0;j<root.vertices.size();++j){const auto& a=derived.vertices[j];const auto& b=root.vertices[j];
            Need(a.x==b.x&&a.z==b.z&&a.u==b.u&&a.v==b.v&&a.color==b.color,"root-horizontal-UV-color-changed");
            if(a.y-geometry.rootMinimum>=.35f)Need(a.y==b.y&&a.normal==b.normal,"upper-root-stem-changed");}
        if(i<4){const auto expected=PlantDerivedRootStem(derived,geometry,original.plants[i].pose,terrain);
            Need(Same(expected,root)&&Same(box,ExtendRootCollision(original.plants[i].lower,expected,original.plants[i].pose)),"original-four-derived-root-or-box-bit-identity");}
        else{const auto a=EntranceCanopyAnchors[i-4];Need(p.pose.position.x==a.x&&p.pose.position.z==a.z&&p.pose.rotation.y==a.yaw&&p.pose.scale.x==a.scale,"actual-authored-anchor-pose-not-used");
            newLeft+=p.pose.position.x<0;newRight+=p.pose.position.x>0;
            for(std::size_t j=0;j<i;++j)Need(std::hypot(p.pose.position.x-population.plants[j].pose.position.x,p.pose.position.z-population.plants[j].pose.position.z)>=p.crownRadius+population.plants[j].crownRadius+.5f,"independent-crown-spacing");}
        GeometryGround(root,p.pose,terrain,-.04f,true);GeometryGround(meshes[1],p.pose,terrain,.04f,false);GeometryGround(meshes[2],p.pose,terrain,-.04f,false);
        const auto proxy=CreateCanopyWoodProxy(root,meshes[2],p.pose);
        Need(!proxy.parts.empty()&&proxy.spacing==.15&&CanopyVolume::ValidWoodParts(proxy.parts),"actual-full-wood-grid-required");
        if(i<4){const auto expected=CreateCanopyWoodProxy(PlantDerivedRootStem(derived,geometry,original.plants[i].pose,terrain),meshes[2],original.plants[i].pose);
            Need(proxy.parts.size()==expected.parts.size()&&proxy.fingerprint==expected.fingerprint,"original-four-wood-proxy-identity");
            for(std::size_t j=0;j<proxy.parts.size();++j)Need(Same(proxy.parts[j],expected.parts[j]),"original-four-every-wood-box-bits");}
        else newWood.insert(newWood.end(),proxy.parts.begin(),proxy.parts.end());
        if(i<4)originalWood.insert(originalWood.end(),proxy.parts.begin(),proxy.parts.end());
        Need(wood.size()+proxy.parts.size()<=9000,"unchanged-full-world-wood-budget");
        wood.insert(wood.end(),proxy.parts.begin(),proxy.parts.end());triangles+=proxy.sourceTriangles;surface+=proxy.surfaceCells;enclosed+=proxy.enclosedGridCells;
        if(i==3)Need(wood.size()==4335&&triangles==304768&&surface==18192&&enclosed==178,"existing-real-four-wood-observation-drift");
        std::cout<<"ACTUAL_TREE index="<<i<<" root="<<p.pose.position.x<<','<<p.pose.position.y<<','<<p.pose.position.z<<" wood_parts="<<proxy.parts.size()<<" wood_fp="<<proxy.fingerprint<<'\n';
    }
    Need(newLeft==2&&newRight==2&&newWood.size()==metadata.woodParts,"actual-two-patches-world-cohort");
    ValidateEntranceFernWood(author,fernAssets,wood);
    // 使用剛完成完整幾何核驗的真八株與95杉，獨立產生 native 對照地表證書。
    std::vector<Vec3> habitatCanopyRoots;
    for(const auto& plant:population.plants)habitatCanopyRoots.push_back(plant.pose.position);
    const ForestHabitatRoots habitatRoots{author.firs.roots,habitatCanopyRoots};
    auto habitatMesh=CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    auto habitatLandscape=CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    const auto originalHabitatMesh=habitatMesh,originalHabitatLandscape=habitatLandscape;
    const auto habitat=ApplyForestHabitatFloor(habitatMesh,terrain,habitatRoots);
    const auto habitatFar=ApplyForestHabitatFloor(habitatLandscape,terrain,habitatRoots,true);
    Need(habitat.changed>10000&&habitat.soilDominant>1000&&habitat.mossDominant>1000&&habitatFar.changed==0,
        "actual-habitat-variety-and-distant-boundary");
    const auto checkHabitatGeometry=[&](const Mesh3D& before,const Mesh3D& after){
        Need(before.indices==after.indices&&before.vertices.size()==after.vertices.size(),"habitat-topology-changed");
        for(std::size_t i=0;i<before.vertices.size();++i){const auto& a=before.vertices[i];const auto& b=after.vertices[i];
            Need(Same(Vec3{a.x,a.y,a.z},Vec3{b.x,b.y,b.z})&&a.color==b.color&&a.normal==b.normal&&
                std::bit_cast<std::uint32_t>(a.v)==std::bit_cast<std::uint32_t>(b.v),
                "habitat-height-normal-color-or-original-rock-changed");
            Need(b.u>=a.u&&b.u+b.v<=1.000001f,"habitat-weights-not-normalized");
            if((a.z>32&&a.z<40)||std::abs(a.x)>=50||a.z<=-20||a.z>=100)
                Need(std::bit_cast<std::uint32_t>(a.u)==std::bit_cast<std::uint32_t>(b.u),
                    "habitat-bridge-boundary-or-landscape-changed");
        }
    };
    checkHabitatGeometry(originalHabitatMesh,habitatMesh);checkHabitatGeometry(originalHabitatLandscape,habitatLandscape);
    std::cout<<"HABITAT_FLOOR_ACTUAL fir_roots="<<author.firs.roots.size()<<" canopy_roots="<<habitatCanopyRoots.size()
        <<" terrain_fp="<<habitat.fingerprint<<" landscape_fp="<<habitatFar.fingerprint
        <<" changed="<<habitat.changed<<" soil_dominant="<<habitat.soilDominant<<" moss_dominant="<<habitat.mossDominant
        <<" landscape_changed="<<habitatFar.changed<<" soil_sum="<<habitat.soilSum<<" rock_sum="<<habitat.rockSum<<'\n';
    AuthoredLoadCommit(author,terrain,wood);
    BodyCorridor({0,0,-8},{0,0,110},terrain,newWood);
    for(const auto p:Session::repairSites)BodyCorridor({0,0,p.z},p,terrain,newWood);
    for(const auto p:{Session::fuse,Session::generator,Session::rangerLog})BodyCorridor({0,0,p.z},p,terrain,newWood);
    for(const auto p:Session::relaySites)BodyCorridor({0,0,p.z},p,terrain,newWood);
    std::array<Mesh3D,5> grassMeshes;std::array<ForestCoverAsset,5> grassAssets;
    std::array<std::uint64_t,5> grassIdentities{};
    for(unsigned i=0;i<5;++i){grassMeshes[i]=Load(GrassPins[i]);NormalizeGrassAsset(grassMeshes[i]);grassAssets[i]=ForestCoverAssetFromMesh(grassMeshes[i]);grassIdentities[i]=ActualGrassIdentity(grassMeshes[i]);}
    std::array<Mesh3D,5> curvedGrassMeshes;
    for(unsigned i=0;i<5;++i){
        curvedGrassMeshes[i]=CreateForestGrassShape(grassMeshes[i],i);
        Need(curvedGrassMeshes[i].indices==grassMeshes[i].indices&&curvedGrassMeshes[i].vertices.size()==grassMeshes[i].vertices.size(),
            "candidate-must-preserve-source-topology");
        const auto sourceBounds=MeasureForestGrassShapeBounds(grassMeshes[i]);
        const auto candidateBounds=MeasureForestGrassShapeBounds(curvedGrassMeshes[i]);
        Need(candidateBounds.rootMaximumRadius<=sourceBounds.rootMaximumRadius&&
            candidateBounds.minimum.y>=sourceBounds.minimum.y&&candidateBounds.maximum.y<=sourceBounds.maximum.y,
            "candidate-must-fit-original-certified-root-volume");
        Need(ForestGrassShapeFingerprint(curvedGrassMeshes[i])!=ForestGrassShapeFingerprint(grassMeshes[i]),
            "candidate-must-change-real-geometry");
        std::cout<<"GRASS_SHAPE_ACTUAL variant="<<i<<" profile="<<ForestGrassShapeProfile
            <<" source_mesh_fp="<<ForestGrassShapeFingerprint(grassMeshes[i])
            <<" render_mesh_fp="<<ForestGrassShapeFingerprint(curvedGrassMeshes[i])
            <<" source_height="<<sourceBounds.maximum.y<<" render_height="<<candidateBounds.maximum.y<<'\n';
    }
    auto originalGrassSolids=WorldSolids(false,FirLayoutSelector::ForestWood,FallenLogProfile::ScannedStraightV1);
    for(const auto& tree:middle.trees)originalGrassSolids.push_back(tree.trunk);
    Need(originalGrassSolids.size()==247&&originalWood.size()==4335&&live.size()==236&&wood.size()==8617,
        "retention-full-original-and-final-world-cohorts");
    const auto sourceGrass=CreateForestCover(terrain,firs.roots,understory,originalGrassSolids,grassAssets,{},originalWood,true);
    std::uint64_t originalGrassHash=14695981039346656037ull;
    for(const auto& plant:sourceGrass.plants){const auto p=plant.transform.position;
        const auto mix=[&](std::uint32_t word){for(unsigned b=0;b<4;++b){originalGrassHash^=(word>>(b*8))&255u;originalGrassHash*=1099511628211ull;}};
        for(float v:{p.x,p.y,p.z,ForestCoverScale(p),p.x+p.z})mix(std::bit_cast<std::uint32_t>(v));mix(plant.variant);
    }
    Need(sourceGrass.reachedBudget&&originalGrassHash==6588246535391004864ull,"certified-original-four-grass-cohort");
    const auto rerolledGrass=CreateForestCover(terrain,author.firs.roots,author.understory,live,grassAssets,{},wood,true);
    ForestCoverWorldView sourceView{terrain,firs.roots,understory,originalGrassSolids,originalWood,4335,0};
    ForestCoverWorldView finalView{terrain,author.firs.roots,author.understory,live,wood,8617,0};
    sourceView.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(sourceView);
    finalView.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(finalView);
    const auto retention=CreateForestCoverRetained(sourceGrass,sourceView,finalView,grassAssets,grassIdentities);
    const auto repeatedRetention=CreateForestCoverRetained(sourceGrass,sourceView,finalView,grassAssets,grassIdentities);
    const auto& grass=retention.cover;
    Need(grass.reachedBudget&&retention.retained+retention.rejected==sourceGrass.plants.size()&&
        retention.retained+retention.filled==grass.plants.size()&&retention.fingerprint==repeatedRetention.fingerprint,
        "retention-complete-population-and-repeat");
    Need(retention.provenance.size()==grass.plants.size(),"retention-complete-source-provenance");
    std::vector<bool> seen(sourceGrass.plants.size());std::vector<unsigned> patchCounts(grass.patches.size());
    unsigned observedRetained{};std::array<unsigned,5> variants{};
    for(std::size_t i=0;i<grass.plants.size();++i){const auto& p=grass.plants[i];const auto& origin=retention.provenance[i];
        Need(p.variant<5&&p.patch<grass.patches.size(),"retention-valid-active-patch-and-variant");
        ++patchCounts[p.patch];++variants[p.variant];
        Need(p.radius==grassAssets[p.variant].radius*p.transform.scale.x&&
            Same(p.transform.scale,Vec3{ForestCoverScale(p.transform.position),ForestCoverScale(p.transform.position),ForestCoverScale(p.transform.position)})&&
            Same(p.transform.rotation,Vec3{0,p.transform.position.x+p.transform.position.z,0}),"retention-original-model-pose-radius");
        Need(ForestCoverFootprint(terrain,p.transform.position,p.radius,{})&&ForestCoverClearance(p.transform.position,p.radius,live),
            "retention-independent-final-terrain-mission-clearance");
        ActualPlantWood(grassMeshes[p.variant],p.transform,live);ActualPlantWood(grassMeshes[p.variant],p.transform,wood);
        ActualPlantWood(curvedGrassMeshes[p.variant],p.transform,live);ActualPlantWood(curvedGrassMeshes[p.variant],p.transform,wood);
        for(std::size_t j=0;j<i;++j)Need(std::hypot(p.transform.position.x-grass.plants[j].transform.position.x,
            p.transform.position.z-grass.plants[j].transform.position.z)>=.65f*(p.radius+grass.plants[j].radius),
            "retention-independent-all-pair-root-spacing");
        if(origin.origin==ForestCoverPlantOrigin::Retained){
            Need(origin.sourcePlant<sourceGrass.plants.size(),"retention-source-ordinal-domain");
            Need(!seen[origin.sourcePlant],"retention-source-plant-used-twice");seen[origin.sourcePlant]=true;++observedRetained;
            const auto& before=sourceGrass.plants[origin.sourcePlant];
            Need(Same(before.transform,p.transform)&&before.variant==p.variant&&
                std::bit_cast<std::uint32_t>(before.radius)==std::bit_cast<std::uint32_t>(p.radius)&&origin.sourcePatch==before.patch,
                "retention-every-original-pose-variant-radius-source-id-preserved");
        }else Need(origin.sourcePlant==(std::numeric_limits<unsigned>::max)(),"fill-must-not-alias-source-plant");
    }
    Need(observedRetained==retention.retained,"retention-source-id-retained-count");
    auto repairedGrassSolids=author.VegetationSolids(true);
    for(const auto& p:grass.plants)ActualPlantWood(curvedGrassMeshes[p.variant],p.transform,repairedGrassSolids);
    std::array<Mesh3D,6> nettleMeshes;std::array<NettleAssetBounds,6> nettleBounds;
    for(unsigned i=0;i<6;++i){nettleMeshes[i]=Load(NettlePins[i]);NormalizeNettleAsset(nettleMeshes[i],i);
        nettleBounds[i]=MeasureNettleAssetBounds(nettleMeshes[i]);}
    auto nettleBefore=live,nettleAfter=repairedGrassSolids;
    nettleBefore.insert(nettleBefore.end(),wood.begin(),wood.end());nettleAfter.insert(nettleAfter.end(),wood.begin(),wood.end());
    const auto mixed=CreateNettleMix(grass,terrain,nettleBounds,live,nettleBefore,nettleAfter);
    const auto repeatedMix=CreateNettleMix(grass,terrain,nettleBounds,live,nettleBefore,nettleAfter);
    Need(mixed.renderVariants==repeatedMix.renderVariants&&mixed.fingerprint==repeatedMix.fingerprint&&
        mixed.renderVariants.size()==3600&&mixed.accepted>0&&mixed.accepted+mixed.rejected==1200&&mixed.worldChecks==2400,
        "nettle-real-world-finite-repeat-selection");
    unsigned mixedObserved{};std::array<unsigned,6> mixedVariants{};
    for(std::size_t i=0;i<grass.plants.size();++i){const auto variant=mixed.renderVariants[i];
        if(variant==6)continue;
        Need(i%3==0&&variant==std::array<unsigned,3>{0,1,4}[(i/3)%3],"nettle-real-canonical-source-role");
        const auto& pose=grass.plants[i].transform;const auto& asset=nettleBounds[variant];
        Need(asset.radius*pose.scale.x+.19f<=grass.plants[i].radius+.0000001f&&
            ForestCoverFootprint(terrain,pose.position,asset.radius*pose.scale.x+.19f,{})&&
            ForestCoverClearance(pose.position,asset.radius*pose.scale.x+.19f,live),"nettle-independent-terrain-and-original-spacing-envelope");
        Vec3 minimum{INFINITY,INFINITY,INFINITY},maximum{-INFINITY,-INFINITY,-INFINITY};
        const auto matrix=pose.Matrix();for(const auto& v:nettleMeshes[variant].vertices)Expand(minimum,maximum,TransformPoint(matrix,{v.x,v.y,v.z}));
        // 從實際全頂點 AABB 加入 shader 解析最大風位移，不重用候選 helper 的包絡公式。
        minimum=minimum-Vec3{.1755f,0,.0715f};maximum=maximum+Vec3{.1755f,.0104f,.0715f};
        for(const auto complete:{std::span<const BoxCollider3D>(nettleBefore),std::span<const BoxCollider3D>(nettleAfter)})
            for(const auto& box:complete){const auto low=box.center-box.extents-Vec3{.03f,.03f,.03f};
                const auto high=box.center+box.extents+Vec3{.03f,.03f,.03f};
                Need(maximum.x<low.x||minimum.x>high.x||maximum.y<low.y||minimum.y>high.y||maximum.z<low.z||minimum.z>high.z,
                    "nettle-independent-all-vertices-and-analytic-wind-vs-two-worlds");}
        ++mixedObserved;++mixedVariants[variant];
    }
    Need(mixedObserved==mixed.accepted&&mixedVariants==mixed.variants,"nettle-complete-mixed-population-counts");
    auto missingNettleWorld=nettleBefore;missingNettleWorld.pop_back();
    Reject([&]{(void)CreateNettleMix(grass,terrain,nettleBounds,live,missingNettleWorld,nettleAfter);},"nettle-missing-complete-world-accepted");
    auto corruptNettleBounds=nettleBounds;corruptNettleBounds[0].radius=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{(void)CreateNettleMix(grass,terrain,corruptNettleBounds,live,nettleBefore,nettleAfter);},"nettle-nonfinite-asset-volume-accepted");
    auto corruptNettleRoot=grass;corruptNettleRoot.plants[0].transform.position.y+=.01f;
    Reject([&]{(void)CreateNettleMix(corruptNettleRoot,terrain,nettleBounds,live,nettleBefore,nettleAfter);},"nettle-invalid-ground-anchor-accepted");
    std::cout<<"NETTLE_MIX_WORLD_PASS profile="<<NettleMixProfile<<" canonical_roots=3600 accepted="<<mixed.accepted
        <<" rejected="<<mixed.rejected<<" world_checks="<<mixed.worldChecks<<" fingerprint="<<mixed.fingerprint
        <<" complete_before="<<nettleBefore.size()<<" complete_after="<<nettleAfter.size()
        <<" analytic_wind_horizontal=.19 analytic_wind_upward=.011 GPU_or_art_pass=0\n";
    std::cout<<"GRASS_SHAPE_WORLD_PASS profile="<<ForestGrassShapeProfile
        <<" plants="<<grass.plants.size()<<" both_bridge_states=1 wood_parts="<<wood.size()
        <<" root_variant_pose_population_unchanged=1 GPU_or_art_pass=0\n";
    for(std::size_t i=0;i<grass.patches.size();++i)Need(patchCounts[i]==grass.patches[i].accepted&&patchCounts[i]>0,
        "retention-active-patch-exact-accepted-count");
    for(auto count:variants)Need(count>0,"retention-five-real-mesh-variants");
    CoverRegions("original-four",sourceGrass);CoverRegions("eight-rerolled",rerolledGrass);CoverRegions("eight-retained",grass);
    std::cout<<"COVER_RETENTION retained="<<retention.retained<<" rejected="<<retention.rejected
        <<" fill="<<retention.filled<<" provenance_fp="<<retention.fingerprint
        <<" original_world_fp="<<sourceView.expectedInputFingerprint<<" final_world_fp="<<finalView.expectedInputFingerprint<<'\n';
    const auto fern=CreateFernColonies(terrain,grass,author.understory.ferns,live,fernAssets,{},wood,true);
    const auto leaf=Load(ShrubPin);const auto shrub=CreateShrubCanopy(leaf,terrain,live,author.understory,grass,fern,fernAssets,64,wood,true);
    Need(grass.reachedBudget&&grass.plants.size()==3600&&fern.reachedBudget&&fern.plants.size()==300&&
        shrub.reachedBudget&&shrub.plants.size()==64,"new-world-existing-decoration-budgets");
    for(const auto& p:grass.plants)ActualPlantWood(grassMeshes[p.variant],p.transform,wood);
    for(const auto& p:fern.plants)ActualPlantWood(fernMeshes[p.variant],p.transform,wood);
    for(std::size_t i=0;i<author.understory.ferns.size();++i)ActualPlantWood(fernMeshes[i%4],author.understory.ferns[i],wood);
    for(const auto& p:shrub.plants){ActualPlantWood(shrub.variants[p.variant].stems,p.pose,wood);ActualPlantWood(shrub.variants[p.variant].leaves,p.pose,wood);}
    live.insert(live.end(),wood.begin(),wood.end());
    std::cout<<"ENTRANCE_AUTHORED_WORLD_CPU_PASS author_revision=entrance-authored-world-v1 source_firs=100 source_middle=24 authored_firs=95 authored_middle=23 withdrawn_roles=11 fern_relocated=5 fern_retained=420 original_four_preserved=1 authored_load_commit=1 default_legacy_load=1 authored_additions=4 total=8 population_fp="<<population.fingerprint
        <<" authored_world_fp="<<author.fingerprint<<" world_collider_fp="<<ColliderHash(live)<<" wood_parts="<<wood.size()<<" wood_triangles="<<triangles
        <<" surface_cells="<<surface<<" enclosed_cells="<<enclosed<<" complete_faces="<<metadata.faceChecks
        <<" exact_overlays="<<metadata.exactOverlayFaces<<" grass="<<grass.plants.size()<<" fern="<<fern.plants.size()
        <<" shrubs="<<shrub.plants.size()<<" dynamic_wind_unverified=1 manual_play_unverified=1 visual_acceptance_unverified=1\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"ENTRANCE_AUTHORED_WORLD_CPU_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
