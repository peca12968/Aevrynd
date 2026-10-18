#pragma once
#include "MistwatchFallenSeatSupport.h"
#include "MistwatchEntranceWorldAuthor.h"
#include "MistwatchForestCoverRetention.h"
#include "MistwatchCanopyCollision.h"
#include "MistwatchRootStemTerrain.h"
#include "MistwatchTrailLitter.h"
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>

namespace Mistwatch::AuthoredQualification {
namespace W=::Mistwatch::FallenWorldSeat;
inline constexpr const char* WorldCohort="entrance-authored-habitat-v1";
inline constexpr std::uint64_t OriginalCoverIdentity=6588246535391004864ull;
struct AssetPin {const char* path;std::size_t bytes;const char* sha;};
inline constexpr std::array<AssetPin,4> CanopyPins{{
    {"mistwatch-canopy-preview/meshes/island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh",159612,"B964D1F7B16FB3602FF038AE111C79D0BFC029185896E95DA97CC1ACB4F59145"},
    {"mistwatch-canopy-preview/meshes/island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh",21375028,"1773F7705A43DA3B3673D0D34C66C2C4FEFC47952970CB84665A774D6A54F871"},
    {"mistwatch-canopy-preview/meshes/island-tree-01-lod2-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh",2709952,"5D32528C7604D041A0730755DDF765B111F82B43847C5F4AED0FE386E99D6188"},
    {"mistwatch-canopy-preview/meshes/island-tree-01-lod2-trunk-root-stem.aemesh",165000,"96EA5581E6A5D2A0C3260DEEBE614A4FA9BCCA0A77772E5EAE42E0C88730BC7C"}}};
inline constexpr std::array<AssetPin,4> FernPins{{
    {"mistwatch-ferns/meshes/fern-02-lod0-node0-mesh0-primitive0-material0-fern-02.aemesh",75104,"83BEE63C50758AF9833AAF8CB0ABC06000DF21C412F4971BD582D64ADD4EAE81"},
    {"mistwatch-ferns/meshes/fern-02-lod0-node1-mesh1-primitive0-material0-fern-02.aemesh",70896,"07DAAD96517DA59466BE5551A6460F0943E5142B55CF2F7F0254938D92BA0306"},
    {"mistwatch-ferns/meshes/fern-02-lod0-node2-mesh2-primitive0-material0-fern-02.aemesh",24656,"78702057068C309D6C559F223FB282C29BAC32B8C7B40987C14338F5F65E7E5A"},
    {"mistwatch-ferns/meshes/fern-02-lod0-node3-mesh3-primitive0-material0-fern-02.aemesh",25628,"243A1BA9863EE362F8BEBCDDA7BA0DC44D847FF8684CAC327B68A422C26663F8"}}};
inline constexpr std::array<AssetPin,5> GrassPins{{
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node0-mesh0-primitive0-material0-grass-medium-02.aemesh",26644,"84C8C7DA8EEB335B1BD60415531A30798933C688FF0D3A241ECA1958FB9C2E56"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node1-mesh1-primitive0-material0-grass-medium-02.aemesh",65236,"CE3EB8A245F7C36682FCABB106EBB56E79BEAE95D6345DF330AC2D2DD5F76D05"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node2-mesh2-primitive0-material0-grass-medium-02.aemesh",44612,"0AA49B54653A3B69A266CC7575AC8FF405A968E220BAC500A6BA3FAA3D432363"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node3-mesh3-primitive0-material0-grass-medium-02.aemesh",91400,"78ED34130EAC919ED41F02918676E68D4060713C52C570B9E4ABEBCD1AA47C5B"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node4-mesh4-primitive0-material0-grass-medium-02.aemesh",63160,"6B95323ACE660F307C8C1EBAB617B2D87118E773FAB4925B4B452B232389A689"}}};
inline constexpr AssetPin ShrubPin{"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh",3704316,"C46ECA29C84D6EA9DB320444D01ED4F16403A2A7F50729178E70C7886B28BC5B"};

namespace Detail {
inline void Need(bool ok,const char* gate){if(!ok)throw std::runtime_error(gate);}
inline bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline bool Same(W::V a,W::V b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
inline bool Same(const W::Pose& a,const W::Pose& b){return Same(a.position,b.position)&&Same(a.rotation,b.rotation)&&Same(a.scale,b.scale);}
inline bool Same(const W::Box& a,const W::Box& b){return Same(a.center,b.center)&&Same(a.extents,b.extents);}
inline bool Same(const Aevrynd::Mesh3D& a,const Aevrynd::Mesh3D& b){return a.indices==b.indices&&a.vertices.size()==b.vertices.size()&&
    (a.vertices.empty()||std::memcmp(a.vertices.data(),b.vertices.data(),a.vertices.size()*sizeof(Aevrynd::Vertex3D))==0);}
struct Hash {
    std::uint64_t value{14695981039346656037ull};
    void Word(std::uint32_t v){for(unsigned b=0;b<4;++b){value^=(v>>(b*8))&255u;value*=1099511628211ull;}}
    void Wide(std::uint64_t v){Word(static_cast<std::uint32_t>(v));Word(static_cast<std::uint32_t>(v>>32));}
    void Count(std::size_t v){Wide(static_cast<std::uint64_t>(v));}
    void Float(float v){Need(std::isfinite(v),"AUTHORED_NONFINITE");Word(std::bit_cast<std::uint32_t>(v));}
    void Vector(W::V v){Float(v.x);Float(v.y);Float(v.z);}
    void Pose(const W::Pose& v){Vector(v.position);Vector(v.rotation);Vector(v.scale);}
    void Box(const W::Box& v){Vector(v.center);Vector(v.extents);Need(v.extents.x>=0&&v.extents.y>=0&&v.extents.z>=0,"AUTHORED_NEGATIVE_EXTENT");}
};
inline std::uint64_t MeshIdentity(const Aevrynd::Mesh3D& mesh){
    Need(mesh.Valid()&&mesh.vertices.size()<=600000&&mesh.indices.size()<=1800000,"AUTHORED_ASSET_TOPOLOGY");
    Hash hash;hash.Word(static_cast<unsigned>(mesh.vertices.size()));hash.Word(static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})hash.Float(f);hash.Word(v.color);hash.Word(v.normal);}
    for(auto i:mesh.indices){Need(i<mesh.vertices.size(),"AUTHORED_ASSET_INDEX");hash.Word(i);}return hash.value;
}
inline Aevrynd::Mesh3D LoadPinned(const std::filesystem::path& path,const AssetPin& pin){
    Need(pin.bytes>16&&pin.bytes<=33554432&&std::filesystem::file_size(path)==pin.bytes,"AUTHORED_ASSET_BYTES");
    std::ifstream stream(path,std::ios::binary);Need(bool(stream),"AUTHORED_ASSET_OPEN");std::vector<std::byte> raw(pin.bytes);
    stream.read(reinterpret_cast<char*>(raw.data()),static_cast<std::streamsize>(raw.size()));
    Need(stream.gcount()==static_cast<std::streamsize>(raw.size()),"AUTHORED_ASSET_READ");
    std::array<std::byte,32> digest{};Need(Aevrynd::WorldDeltaSha2563D(raw,digest),"AUTHORED_ASSET_SHA_FAILED");
    std::ostringstream hex;hex<<std::uppercase<<std::hex<<std::setfill('0');for(auto b:digest)hex<<std::setw(2)<<std::to_integer<unsigned>(b);
    Need(hex.str()==pin.sha,"AUTHORED_ASSET_SHA_MISMATCH");auto loaded=Aevrynd::LoadAevryndMeshFile(path);
    Need(bool(loaded),"AUTHORED_ASSET_NATIVE_LOAD");static_assert(sizeof(Aevrynd::Vertex3D)==28&&std::endian::native==std::endian::little);
    std::array<std::uint32_t,4> words{};std::memcpy(words.data(),raw.data(),16);
    const auto vb=loaded.mesh.vertices.size()*sizeof(Aevrynd::Vertex3D),ib=loaded.mesh.indices.size()*sizeof(std::uint32_t);
    Need(words[0]==0x334d4541u&&words[1]==1&&words[2]==loaded.mesh.vertices.size()&&words[3]==loaded.mesh.indices.size()&&
        raw.size()==16+vb+ib&&std::memcmp(raw.data()+16,loaded.mesh.vertices.data(),vb)==0&&
        std::memcmp(raw.data()+16+vb,loaded.mesh.indices.data(),ib)==0,"AUTHORED_RAW_NATIVE_BITS");
    (void)MeshIdentity(loaded.mesh);return std::move(loaded.mesh);
}
inline void SameBoxes(std::span<const W::Box> a,std::span<const W::Box> b,const char* gate){
    Need(a.size()==b.size(),gate);for(std::size_t i=0;i<a.size();++i)Need(Same(a[i],b[i]),gate);
}
inline std::uint64_t BoxesIdentity(std::span<const W::Box> boxes){Hash h;for(const auto& b:boxes)h.Box(b);return h.value;}
inline std::uint64_t TypedIdentity(std::span<const W::Solid> world){Hash h;h.Count(world.size());
    for(const auto& s:world){h.Word(static_cast<unsigned>(s.role));h.Word(s.id);h.Box(s.box);}return h.value;}
// 獨立核准清單，不使用 production EntranceWithdrawals 或其輸出 ID 當 oracle。
inline bool Removed(W::Role role,unsigned id){
    switch(role){case W::Role::MatureFir:return id==7||id==47||id==63||id==94||id==95;
    case W::Role::ForestProp:return id==12||id==17||id==31;
    case W::Role::Sapling:return id==15||id==18;
    case W::Role::Middle:return id==16;default:return false;}
}
inline W::TypedWorld Filter(const W::TypedWorld& original){W::TypedWorld out;
    for(const auto& s:original)if(!Removed(s.role,s.id))out.push_back(s);return out;}
inline void SameTyped(std::span<const W::Solid> a,std::span<const W::Solid> b,const char* gate){
    Need(a.size()==b.size(),gate);for(std::size_t i=0;i<a.size();++i)Need(a[i].role==b[i].role&&a[i].id==b[i].id&&Same(a[i].box,b[i].box),gate);
}
inline std::uint64_t CoverIdentity(const ForestCoverResult& cover){Hash h;h.Count(cover.plants.size());h.Count(cover.patches.size());
    h.Word(cover.reachedBudget?1u:0u);h.Word(cover.rejectedClearance);h.Word(cover.rejectedTerrain);h.Word(cover.rejectedSpacing);
    for(const auto& p:cover.patches){h.Vector(p.centre);h.Float(p.radius);h.Float(p.aspect);h.Float(p.yaw);h.Word(p.requested);h.Word(p.accepted);}
    for(const auto& p:cover.plants){h.Pose(p.transform);h.Float(p.radius);h.Word(p.variant);h.Word(p.patch);}return h.value;
}
inline std::uint64_t NativeCoverIdentity(const ForestCoverResult& cover){Hash h;
    for(const auto& p:cover.plants){const auto r=p.transform.position;for(float f:{r.x,r.y,r.z,ForestCoverScale(r),r.x+r.z})h.Float(f);h.Word(p.variant);}return h.value;
}
inline std::uint64_t RetentionIdentity(const ForestCoverRetentionResult& r){Hash h;h.Wide(CoverIdentity(r.cover));
    for(auto value:{r.retained,r.rejected,r.filled})h.Word(value);
    for(auto value:{r.originalInputFingerprint,r.finalInputFingerprint,r.provenanceFingerprint,r.fingerprint})h.Wide(value);
    h.Count(r.provenance.size());for(const auto& p:r.provenance){h.Word(static_cast<unsigned>(p.origin));h.Word(p.sourcePlant);h.Word(p.sourcePatch);}
    h.Count(r.rejections.size());for(const auto& p:r.rejections){h.Word(p.sourcePlant);h.Word(p.sourcePatch);h.Word(static_cast<unsigned>(p.gate));}return h.value;
}
inline std::uint64_t LitterIdentity(const TrailLitterResult& r){Hash h;h.Wide(MeshIdentity(r.branches));h.Wide(MeshIdentity(r.stones));
    h.Word(r.branchCount);h.Word(r.stoneCount);h.Word(r.rejectedClearance);h.Word(r.rejectedBudget);h.Word(r.reachedBudget?1u:0u);h.Wide(r.fingerprint);
    h.Count(r.patches.size());for(const auto& p:r.patches){h.Vector(p.centre);h.Float(p.halfX);h.Float(p.halfZ);h.Word(p.branchCount);h.Word(p.stoneCount);}return h.value;
}
inline std::uint64_t ColonyIdentity(const FernColonyResult& r){Hash h;h.Word(r.reachedBudget?1u:0u);
    for(auto n:{r.rejectedClearance,r.rejectedTerrain,r.rejectedExistingFern,r.rejectedGrass,r.rejectedSpacing})h.Word(n);
    h.Count(r.colonies.size());for(const auto& p:r.colonies){h.Vector(p.centre);h.Float(p.radius);h.Word(p.grassPatch);h.Word(p.requested);h.Word(p.accepted);}
    h.Count(r.plants.size());for(const auto& p:r.plants){h.Pose(p.transform);h.Float(p.radius);h.Word(p.variant);h.Word(p.colony);h.Word(p.grassPatch);}return h.value;
}
inline std::uint64_t ShrubIdentity(const ShrubCanopyResult& r){Hash h;h.Word(r.reachedBudget?1u:0u);h.Wide(r.fingerprint);
    for(auto n:{r.attempted,r.rejectedClearance,r.rejectedTerrain,r.rejectedVegetation})h.Word(n);
    for(const auto& v:r.variants){h.Wide(MeshIdentity(v.stems));h.Wide(MeshIdentity(v.leaves));
        for(float f:{v.radius,v.height,v.minimumY,v.maximumY})h.Float(f);h.Word(v.leafCount);h.Word(v.stemCount);h.Count(v.attachments.size());
        for(const auto& a:v.attachments){h.Word(a.vertex);h.Vector(a.centre);h.Float(a.branchRadius);}}
    h.Count(r.plants.size());for(const auto& p:r.plants){h.Pose(p.pose);h.Word(p.variant);h.Float(p.radius);}return h.value;
}
inline bool SameCover(const ForestCoverResult& a,const ForestCoverResult& b){
    if(a.plants.size()!=b.plants.size()||a.patches.size()!=b.patches.size()||a.reachedBudget!=b.reachedBudget||
        a.rejectedClearance!=b.rejectedClearance||a.rejectedTerrain!=b.rejectedTerrain||a.rejectedSpacing!=b.rejectedSpacing)return false;
    for(std::size_t i=0;i<a.plants.size();++i){const auto& x=a.plants[i];const auto& y=b.plants[i];
        if(!Same(x.transform,y.transform)||!Same(x.radius,y.radius)||x.variant!=y.variant||x.patch!=y.patch)return false;}
    for(std::size_t i=0;i<a.patches.size();++i){const auto& x=a.patches[i];const auto& y=b.patches[i];
        if(!Same(x.centre,y.centre)||!Same(x.radius,y.radius)||!Same(x.aspect,y.aspect)||!Same(x.yaw,y.yaw)||x.requested!=y.requested||x.accepted!=y.accepted)return false;}return true;
}
inline bool SameRetention(const ForestCoverRetentionResult& a,const ForestCoverRetentionResult& b){
    if(!SameCover(a.cover,b.cover)||a.retained!=b.retained||a.rejected!=b.rejected||a.filled!=b.filled||a.originalInputFingerprint!=b.originalInputFingerprint||
        a.finalInputFingerprint!=b.finalInputFingerprint||a.provenanceFingerprint!=b.provenanceFingerprint||a.fingerprint!=b.fingerprint||
        a.provenance.size()!=b.provenance.size()||a.rejections.size()!=b.rejections.size())return false;
    for(std::size_t i=0;i<a.provenance.size();++i){const auto& x=a.provenance[i];const auto& y=b.provenance[i];if(x.origin!=y.origin||x.sourcePlant!=y.sourcePlant||x.sourcePatch!=y.sourcePatch)return false;}
    for(std::size_t i=0;i<a.rejections.size();++i){const auto& x=a.rejections[i];const auto& y=b.rejections[i];if(x.gate!=y.gate||x.sourcePlant!=y.sourcePlant||x.sourcePatch!=y.sourcePatch)return false;}return true;
}
inline bool SameLitter(const TrailLitterResult& a,const TrailLitterResult& b){
    if(a.reachedBudget!=b.reachedBudget||a.branchCount!=b.branchCount||a.stoneCount!=b.stoneCount||a.rejectedClearance!=b.rejectedClearance||
        a.rejectedBudget!=b.rejectedBudget||a.fingerprint!=b.fingerprint||a.patches.size()!=b.patches.size()||!Same(a.branches,b.branches)||!Same(a.stones,b.stones))return false;
    for(std::size_t i=0;i<a.patches.size();++i){const auto& x=a.patches[i];const auto& y=b.patches[i];if(!Same(x.centre,y.centre)||!Same(x.halfX,y.halfX)||
        !Same(x.halfZ,y.halfZ)||x.branchCount!=y.branchCount||x.stoneCount!=y.stoneCount)return false;}return true;
}
inline bool SameColonies(const FernColonyResult& a,const FernColonyResult& b){
    if(a.reachedBudget!=b.reachedBudget||a.rejectedClearance!=b.rejectedClearance||a.rejectedTerrain!=b.rejectedTerrain||a.rejectedExistingFern!=b.rejectedExistingFern||
        a.rejectedGrass!=b.rejectedGrass||a.rejectedSpacing!=b.rejectedSpacing||a.plants.size()!=b.plants.size()||a.colonies.size()!=b.colonies.size())return false;
    for(std::size_t i=0;i<a.plants.size();++i){const auto& x=a.plants[i];const auto& y=b.plants[i];if(!Same(x.transform,y.transform)||!Same(x.radius,y.radius)||
        x.variant!=y.variant||x.colony!=y.colony||x.grassPatch!=y.grassPatch)return false;}
    for(std::size_t i=0;i<a.colonies.size();++i){const auto& x=a.colonies[i];const auto& y=b.colonies[i];if(!Same(x.centre,y.centre)||!Same(x.radius,y.radius)||
        x.grassPatch!=y.grassPatch||x.requested!=y.requested||x.accepted!=y.accepted)return false;}return true;
}
inline bool SameShrubs(const ShrubCanopyResult& a,const ShrubCanopyResult& b){
    if(a.reachedBudget!=b.reachedBudget||a.fingerprint!=b.fingerprint||a.attempted!=b.attempted||a.rejectedClearance!=b.rejectedClearance||
        a.rejectedTerrain!=b.rejectedTerrain||a.rejectedVegetation!=b.rejectedVegetation||a.plants.size()!=b.plants.size())return false;
    for(unsigned i=0;i<4;++i){const auto& x=a.variants[i];const auto& y=b.variants[i];
        if(!Same(x.stems,y.stems)||!Same(x.leaves,y.leaves)||!Same(x.radius,y.radius)||!Same(x.height,y.height)||!Same(x.minimumY,y.minimumY)||
            !Same(x.maximumY,y.maximumY)||x.leafCount!=y.leafCount||x.stemCount!=y.stemCount||x.attachments.size()!=y.attachments.size())return false;
        for(std::size_t j=0;j<x.attachments.size();++j){const auto& p=x.attachments[j];const auto& q=y.attachments[j];
            if(p.vertex!=q.vertex||!Same(p.centre,q.centre)||!Same(p.branchRadius,q.branchRadius))return false;}}
    for(std::size_t i=0;i<a.plants.size();++i){const auto& x=a.plants[i];const auto& y=b.plants[i];if(!Same(x.pose,y.pose)||x.variant!=y.variant||!Same(x.radius,y.radius))return false;}return true;
}
}

struct Assets {
    std::array<Aevrynd::Mesh3D,3> canopy;
    Aevrynd::Mesh3D derivedStem,shrubLeaf;
    std::array<Aevrynd::Mesh3D,4> fern;
    std::array<Aevrynd::Mesh3D,5> grass;
    std::array<FernColonyAsset,4> fernAssets;
    std::array<ForestCoverAsset,5> grassAssets;
    std::array<std::uint64_t,5> grassIdentities{};
    std::uint64_t fingerprint{};
};
inline std::uint64_t AssetsFingerprint(const Assets& a){Detail::Hash h;
    for(const auto& m:a.canopy)h.Wide(Detail::MeshIdentity(m));h.Wide(Detail::MeshIdentity(a.derivedStem));h.Wide(Detail::MeshIdentity(a.shrubLeaf));
    for(unsigned i=0;i<4;++i){h.Wide(Detail::MeshIdentity(a.fern[i]));const auto measured=FernColonyAssetFromMesh(a.fern[i]),stored=a.fernAssets[i];
        Detail::Need(Detail::Same(measured.radius,stored.radius)&&Detail::Same(measured.height,stored.height)&&Detail::Same(measured.minimumY,stored.minimumY)&&
            measured.vertices==stored.vertices&&measured.triangles==stored.triangles,"AUTHORED_FERN_ASSET_STALE");}
    for(unsigned i=0;i<5;++i){const auto id=Detail::MeshIdentity(a.grass[i]);h.Wide(id);const auto m=ForestCoverAssetFromMesh(a.grass[i]),s=a.grassAssets[i];
        Detail::Need(id==a.grassIdentities[i]&&Detail::Same(m.radius,s.radius)&&Detail::Same(m.height,s.height)&&
            Detail::Same(m.minimumY,s.minimumY)&&Detail::Same(m.maximumY,s.maximumY),"AUTHORED_GRASS_ASSET_STALE");}
    return h.value;
}
inline Assets LoadAssets(const std::filesystem::path& root,const std::filesystem::path& generatedRootStem={}){
    Detail::Need(root.is_absolute(),"AUTHORED_ASSET_ROOT_ABSOLUTE_REQUIRED");Assets a;
    for(unsigned i=0;i<3;++i)a.canopy[i]=Detail::LoadPinned(root/CanopyPins[i].path,CanopyPins[i]);
    // 與 runtime qualification 的 generated-root-stem 一致，非空參數是產物目錄。
    a.derivedStem=Detail::LoadPinned(generatedRootStem.empty()?root/CanopyPins[3].path:
        generatedRootStem/"island-tree-01-lod2-trunk-root-stem.aemesh",CanopyPins[3]);
    for(unsigned i=0;i<4;++i){a.fern[i]=Detail::LoadPinned(root/FernPins[i].path,FernPins[i]);NormalizeFernAsset(a.fern[i]);a.fernAssets[i]=FernColonyAssetFromMesh(a.fern[i]);}
    for(unsigned i=0;i<5;++i){a.grass[i]=Detail::LoadPinned(root/GrassPins[i].path,GrassPins[i]);NormalizeGrassAsset(a.grass[i]);
        Detail::Need(a.grass[i].vertices.size()==GrassVertexCounts[i]&&a.grass[i].indices.size()==GrassTriangleCounts[i]*3ull,"AUTHORED_FIVE_GRASS_COUNTS");
        a.grassAssets[i]=ForestCoverAssetFromMesh(a.grass[i]);a.grassIdentities[i]=Detail::MeshIdentity(a.grass[i]);}
    a.shrubLeaf=Detail::LoadPinned(root/ShrubPin.path,ShrubPin);a.fingerprint=AssetsFingerprint(a);return a;
}

struct SourceContext {
    MatureFirLayout firs;std::vector<ForestProp> props;ForestUnderstory understory;
    std::vector<BroadleafTree> broadleaf;MiddleLayerResult middle;
    CanopyPopulation canopy;std::array<W::TypedWorld,2> legacyWorlds;
    std::uint64_t fingerprint{};
};
inline std::uint64_t SourceFingerprint(const SourceContext& s){Detail::Hash h;
    h.Count(s.firs.firs.size());h.Wide(s.firs.fingerprint);h.Word(s.firs.reachedBudget?1u:0u);
    for(const auto& p:s.firs.firs){h.Vector(p.root);h.Pose(p.render);h.Box(p.collider);h.Word(p.patch);}
    h.Count(s.firs.roots.size());for(auto p:s.firs.roots)h.Vector(p);
    h.Count(s.props.size());for(const auto& p:s.props){h.Word(p.mesh);h.Pose(p.transform);h.Box(p.collider);}
    for(const auto* list:{&s.understory.ferns,&s.understory.saplings}){h.Count(list->size());for(const auto& p:*list)h.Pose(p);}
    h.Count(s.broadleaf.size());for(const auto& p:s.broadleaf){h.Pose(p.transform);h.Box(p.trunk);}
    h.Count(s.middle.trees.size());h.Word(s.middle.reachedBudget?1u:0u);for(const auto& p:s.middle.trees){h.Pose(p.transform);h.Box(p.trunk);}
    h.Count(s.canopy.plants.size());h.Wide(s.canopy.fingerprint);h.Word(s.canopy.reachedBudget?1u:0u);
    for(const auto& p:s.canopy.plants){h.Pose(p.pose);h.Box(p.lower);h.Float(p.crownRadius);h.Float(p.rootRadius);h.Wide(Detail::MeshIdentity(p.rootedTrunk));}
    for(const auto& world:s.legacyWorlds)h.Wide(Detail::TypedIdentity(world));return h.value;
}
inline SourceContext CreateSourceContext(const Aevrynd::HeightField3D& terrain,const Assets& assets,W::Budget& budget){
    budget.Time();Detail::Need(assets.fingerprint&&AssetsFingerprint(assets)==assets.fingerprint,"AUTHORED_ASSETS_CHANGED");SourceContext s;
    s.firs=CreateMatureFirLayout(terrain,FirLayoutSelector::ForestWood);s.props=ForestProps(terrain,s.firs.roots);
    s.understory=CreateUnderstory(terrain,s.firs.roots,s.props);s.broadleaf=CreateBroadleafTrees(terrain,s.firs.roots,s.props,s.understory);
    W::VerifyCurrentBaseline(s.firs,s.props,s.understory,s.broadleaf,terrain);
    for(unsigned state=0;state<2;++state){s.legacyWorlds[state]=W::BaseWorld(state!=0,s.firs,s.props,s.understory,s.broadleaf);
        Detail::SameBoxes(W::Boxes(s.legacyWorlds[state]),WorldSolids(state!=0,FirLayoutSelector::ForestWood),"AUTHORED_ORIGINAL_BASE_WORLD");}
    const std::array<float,4> radii{{assets.fernAssets[0].radius,assets.fernAssets[1].radius,assets.fernAssets[2].radius,assets.fernAssets[3].radius}};
    // Main 中層及原四株先使用舊 cylinder world；不得先映射掃描倒木。
    s.middle=CreateMiddleLayer(terrain,s.firs.roots,s.props,s.understory,s.broadleaf,W::Boxes(s.legacyWorlds[0]),24,radii);
    Detail::Need(s.middle.reachedBudget&&s.middle.trees.size()==24&&MiddleLayerTransformFingerprint(s.middle.trees)==10337542786750772359ull,"AUTHORED_ORIGINAL_MIDDLE");
    for(auto& world:s.legacyWorlds)for(unsigned id=0;id<24;++id)world.push_back({W::Role::Middle,id,s.middle.trees[id].trunk});
    const auto geometry=MeasureCanopy(assets.canopy);
    Detail::Need(geometry.triangles==606094&&assets.derivedStem.vertices.size()==3608&&assets.derivedStem.indices.size()==15990,"AUTHORED_ORIGINAL_CANOPY_ASSET_COUNTS");
    s.canopy=CreateCanopyPopulation(assets.canopy,geometry,terrain,W::Boxes(s.legacyWorlds[0]),s.understory,{},{},assets.fernAssets,{});
    Detail::Need(s.canopy.reachedBudget&&s.canopy.plants.size()==4&&s.canopy.fingerprint==6584434718760703877ull,"AUTHORED_ORIGINAL_FOUR_CANOPY");
    s.fingerprint=SourceFingerprint(s);budget.Time();return s;
}

struct Context {
    SourceContext source;EntranceWorldAuthor author;CanopyPopulation canopy;EntranceCanopyMetadata canopyMetadata;
    std::vector<W::Box> sourceWood,wood;
    std::array<W::TypedWorld,2> sourceMapped,worlds;
    std::array<std::vector<W::Box>,2> base,vegetation,extras;
    ForestCoverResult sourceCover;ForestCoverRetentionResult retention;
    TrailLitterResult litter;FernColonyResult colonies;ShrubCanopyResult shrubs;
    std::array<std::uint64_t,2> typedFingerprints{},worldFingerprints{};
    std::uint64_t sourceCoverFingerprint{},coverFingerprint{},retentionFingerprint{},litterFingerprint{},colonyFingerprint{},shrubFingerprint{},fingerprint{};
};

inline void ValidateSourceContext(const SourceContext& s,const Aevrynd::HeightField3D& terrain){
    Detail::Need(s.fingerprint&&SourceFingerprint(s)==s.fingerprint,"AUTHORED_SOURCE_CHANGED");W::VerifyCurrentBaseline(s.firs,s.props,s.understory,s.broadleaf,terrain);
    Detail::Need(s.middle.trees.size()==24&&s.canopy.plants.size()==4&&s.canopy.fingerprint==6584434718760703877ull,"AUTHORED_SOURCE_COUNTS");
    for(unsigned state=0;state<2;++state){auto expected=W::BaseWorld(state!=0,s.firs,s.props,s.understory,s.broadleaf);
        for(unsigned id=0;id<24;++id)expected.push_back({W::Role::Middle,id,s.middle.trees[id].trunk});
        Detail::SameTyped(expected,s.legacyWorlds[state],"AUTHORED_SOURCE_TYPED_ROLES");}
}
inline void ValidateAuthor(const EntranceWorldAuthor& a,const SourceContext& s,const Aevrynd::HeightField3D& terrain){
    using Detail::Need;Need(a.applied&&a.fallenProfile==FallenLogProfile::ScannedStraightV1&&a.firs.firs.size()==95&&a.firs.roots.size()==95&&
        a.props.size()==33&&a.understory.saplings.size()==40&&a.understory.ferns.size()==420&&a.middle.trees.size()==23&&a.broadleaf.size()==12&&
        a.firSourceIds.size()==95&&a.propSourceIds.size()==33&&a.saplingSourceIds.size()==40&&a.middleSourceIds.size()==23,"AUTHORED_AUTHOR_COUNTS");
    Need(a.sourceFirFingerprint==8236695966185867318ull&&a.sourceMiddleFingerprint==10337542786750772359ull,"AUTHORED_AUTHOR_SOURCE_FP");
    std::size_t at{};
    for(unsigned id=0;id<100;++id)if(!Detail::Removed(W::Role::MatureFir,id)){
        const auto& x=s.firs.firs[id];const auto& y=a.firs.firs[at];Need(a.firSourceIds[at]==id&&Detail::Same(x.root,y.root)&&
            Detail::Same(s.firs.roots[id],a.firs.roots[at])&&Detail::Same(x.render,y.render)&&Detail::Same(x.collider,y.collider)&&x.patch==y.patch,"AUTHORED_FIR_SOURCE_BITS");++at;}
    const auto mapped=ApplyFallenLogProfile(s.props,FirLayoutSelector::ForestWood,FallenLogProfile::ScannedStraightV1);at=0;
    for(unsigned id=0;id<36;++id)if(!Detail::Removed(W::Role::ForestProp,id)){
        Need(a.propSourceIds[at]==id&&FallenLogDetail::Words(mapped[id])==FallenLogDetail::Words(a.props[at]),"AUTHORED_PROP_SOURCE_BITS");++at;}
    at=0;for(unsigned id=0;id<42;++id)if(!Detail::Removed(W::Role::Sapling,id)){
        Need(a.saplingSourceIds[at]==id&&Detail::Same(s.understory.saplings[id],a.understory.saplings[at]),"AUTHORED_SAPLING_SOURCE_BITS");++at;}
    at=0;for(unsigned id=0;id<24;++id)if(!Detail::Removed(W::Role::Middle,id)){
        Need(a.middleSourceIds[at]==id&&Detail::Same(s.middle.trees[id].transform,a.middle.trees[at].transform)&&
            Detail::Same(s.middle.trees[id].trunk,a.middle.trees[at].trunk),"AUTHORED_MIDDLE_SOURCE_BITS");++at;}
    for(unsigned id=0;id<12;++id)Need(Detail::Same(s.broadleaf[id].transform,a.broadleaf[id].transform)&&
        Detail::Same(s.broadleaf[id].trunk,a.broadleaf[id].trunk),"AUTHORED_BROADLEAF_SOURCE_BITS");
    // 不讀 production 的移動表，獨立驗五株且其餘415株完全不變。
    for(unsigned id=0;id<420;++id){auto expected=s.understory.ferns[id];float dx{},dz{};
        switch(id){case 114:dx=-1.5f;dz=2;break;case 127:dx=2;dz=2;break;case 281:dx=1.5f;dz=-2;break;
            case 287:dx=-1.5f;dz=-1;break;case 399:dx=-2;dz=1.5f;break;default:break;}
        if(dx!=0){expected.position.x+=dx;expected.position.z+=dz;expected.position.y=terrain.SampleHeight(expected.position.x,expected.position.z);}
        Need(Detail::Same(expected,a.understory.ferns[id]),"AUTHORED_FERN_SOURCE_BITS");}
}
inline W::TypedWorld IndependentMappedSource(const SourceContext& source,unsigned state){
    Detail::Need(state<2,"AUTHORED_WORLD_DOMAIN");auto expected=W::BaseWorld(state!=0,source.firs,source.props,source.understory,source.broadleaf);
    for(auto& solid:expected)if(solid.role==W::Role::ForestProp&&source.props[solid.id].FallenLog()){
        const auto& placements=FallenLogPlacements();const auto found=std::find_if(placements.begin(),placements.end(),
            [&](const auto& p){return p.propId==solid.id;});Detail::Need(found!=placements.end(),"AUTHORED_ORIGINAL_LOG_ROLE");solid.box=found->collider;}
    for(unsigned id=0;id<source.middle.trees.size();++id)expected.push_back({W::Role::Middle,id,source.middle.trees[id].trunk});return expected;
}
inline void ValidateTypedWorld(const Context& c,unsigned state){
    Detail::Need(state<2&&c.wood.size()==8617&&c.sourceWood.size()==4335,"AUTHORED_WORLD_DOMAIN");
    Detail::SameTyped(c.sourceMapped[state],IndependentMappedSource(c.source,state),"AUTHORED_MAPPED_SOURCE_ROLES");
    auto expected=Detail::Filter(c.sourceMapped[state]);Detail::Need(expected.size()==236u+state,"AUTHORED_FILTERED_SOURCE_COUNT");
    const auto expectedVegetation=W::Boxes(expected);const auto expectedBase=std::span<const W::Box>(expectedVegetation).first(213u+state);
    Detail::SameBoxes(c.author.WorldSolids(state!=0),expectedBase,"AUTHORED_BASE_INDEPENDENT_ORACLE");
    Detail::SameBoxes(c.author.VegetationSolids(state!=0),expectedVegetation,"AUTHORED_VEGETATION_INDEPENDENT_ORACLE");
    Detail::SameBoxes(c.base[state],expectedBase,"AUTHORED_BASE_SNAPSHOT_CHANGED");
    Detail::SameBoxes(c.vegetation[state],expectedVegetation,"AUTHORED_VEGETATION_SNAPSHOT_CHANGED");
    std::vector<W::Box> expectedExtras;for(const auto& s:expected)if(s.role==W::Role::Middle)expectedExtras.push_back(s.box);
    expectedExtras.insert(expectedExtras.end(),c.wood.begin(),c.wood.end());Detail::SameBoxes(c.extras[state],expectedExtras,"AUTHORED_EXTRAS_CHANGED");
    for(unsigned id=0;id<c.wood.size();++id)expected.push_back({W::Role::CanopyWood,id,c.wood[id]});
    Detail::SameTyped(c.worlds[state],expected,"AUTHORED_TYPED_WORLD_CHANGED");
    Detail::Need(c.worlds[state].size()==8853u+state&&Detail::TypedIdentity(c.worlds[state])==c.typedFingerprints[state]&&
        Detail::BoxesIdentity(W::Boxes(c.worlds[state]))==c.worldFingerprints[state],"AUTHORED_FULL_WORLD_FP");
}

inline void ValidateRetentionMappings(const Context& c,const Aevrynd::HeightField3D& terrain,const Assets& assets,W::Budget& budget){
    using Detail::Need;const auto& r=c.retention;const auto& grass=r.cover;
    Need(c.sourceCover.reachedBudget&&c.sourceCover.plants.size()==3600&&Detail::NativeCoverIdentity(c.sourceCover)==OriginalCoverIdentity&&
        grass.reachedBudget&&grass.plants.size()==3600&&r.retained+r.rejected==3600&&r.retained+r.filled==3600&&
        r.provenance.size()==3600&&r.rejections.size()==r.rejected,"AUTHORED_RETENTION_COUNTS");
    const ForestCoverSettings settings;Need(settings.minimumNormalY==.88f&&settings.maximumFootprintHeightDelta==.24f&&
        settings.rootSeparationRatio==.65f&&settings.maximumPatches==400&&settings.attemptsPerPatch==240,"AUTHORED_UNCHANGED_COVER_GATES");
    std::vector<unsigned> sourceUse(3600),patchCounts(grass.patches.size());unsigned retained{},filled{};
    for(const auto& rejected:r.rejections){Need(rejected.sourcePlant<3600&&rejected.sourcePatch==c.sourceCover.plants[rejected.sourcePlant].patch&&
        static_cast<unsigned>(rejected.gate)<=static_cast<unsigned>(ForestCoverRetentionGate::Spacing)&&!sourceUse[rejected.sourcePlant]++,"AUTHORED_RETENTION_REJECTION_MAPPING");
        const auto& old=c.sourceCover.plants[rejected.sourcePlant];Need(old.variant<5,"AUTHORED_RETENTION_SOURCE_VARIANT");const auto root=old.transform.position;
        const auto& a=assets.grassAssets[old.variant];const auto volume=CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.maximumY);
        ForestCoverRetentionGate observed=ForestCoverRetentionGate::Domain;bool safe=false;
        if(root.x>=-32&&root.x<=32&&root.z>=-10&&root.z<=88){observed=ForestCoverRetentionGate::GroundAnchor;
            if(Detail::Same(root.y,terrain.SampleHeight(root.x,root.z))){observed=ForestCoverRetentionGate::Clearance;
                if(ForestCoverClearance(root,old.radius,c.vegetation[0])){observed=ForestCoverRetentionGate::Volume;
                    if(CanopyVolume::ClearsCanopyWood(old.transform,volume,c.vegetation[0])&&CanopyVolume::ClearsCanopyWood(old.transform,volume,c.wood)){
                        observed=ForestCoverRetentionGate::Terrain;if(ForestCoverFootprint(terrain,root,old.radius,settings)){
                            observed=ForestCoverRetentionGate::Spacing;safe=true;
                            for(std::size_t i=0;i<grass.plants.size();++i){const auto& origin=r.provenance[i];
                                if(origin.origin!=ForestCoverPlantOrigin::Retained||origin.sourcePlant>=rejected.sourcePlant)continue;
                                const auto& accepted=grass.plants[i];budget.Op();if(std::hypot(root.x-accepted.transform.position.x,root.z-accepted.transform.position.z)<
                                    .65f*(old.radius+accepted.radius)){safe=false;break;}}
                        }}}}}
        Need(!safe&&observed==rejected.gate,"AUTHORED_REJECTION_FIRST_GATE_EVIDENCE");}
    for(std::size_t i=0;i<grass.plants.size();++i){budget.Op();const auto& p=grass.plants[i];const auto& origin=r.provenance[i];
        Need(p.variant<5&&p.patch<grass.patches.size(),"AUTHORED_RETENTION_PLANT_DOMAIN");++patchCounts[p.patch];
        const float scale=ForestCoverScale(p.transform.position);Need(Detail::Same(p.transform.scale,W::V{scale,scale,scale})&&
            Detail::Same(p.transform.rotation,W::V{0,p.transform.position.x+p.transform.position.z,0})&&
            Detail::Same(p.radius,assets.grassAssets[p.variant].radius*scale)&&Detail::Same(p.transform.position.y,
                terrain.SampleHeight(p.transform.position.x,p.transform.position.z)),"AUTHORED_RETENTION_CANONICAL_POSE");
        if(origin.origin==ForestCoverPlantOrigin::Retained){Need(origin.sourcePlant<3600&&!sourceUse[origin.sourcePlant]++,"AUTHORED_RETAINED_SOURCE_DUPLICATE");
            const auto& old=c.sourceCover.plants[origin.sourcePlant];Need(Detail::Same(p.transform,old.transform)&&p.variant==old.variant&&
                Detail::Same(p.radius,old.radius)&&origin.sourcePatch==old.patch,"AUTHORED_RETAINED_SOURCE_BITS");++retained;
        }else{Need(origin.sourcePlant==(std::numeric_limits<unsigned>::max)()&&
            (origin.origin==ForestCoverPlantOrigin::OriginalPatchFill||origin.origin==ForestCoverPlantOrigin::NewPatchFill),"AUTHORED_FILL_SOURCE_ALIAS");
            if(origin.origin==ForestCoverPlantOrigin::OriginalPatchFill)Need(origin.sourcePatch<c.sourceCover.patches.size(),"AUTHORED_FILL_SOURCE_PATCH");
            else Need(origin.sourcePatch>=c.sourceCover.patches.size()&&origin.sourcePatch<c.sourceCover.patches.size()+settings.maximumPatches,"AUTHORED_NEW_PATCH_SOURCE_DOMAIN");++filled;}
        if(origin.origin!=ForestCoverPlantOrigin::NewPatchFill){Need(origin.sourcePatch<c.sourceCover.patches.size(),"AUTHORED_ORIGINAL_PATCH_SOURCE_DOMAIN");
            const auto& x=c.sourceCover.patches[origin.sourcePatch];const auto& y=grass.patches[p.patch];Need(Detail::Same(x.centre,y.centre)&&
                Detail::Same(x.radius,y.radius)&&Detail::Same(x.aspect,y.aspect)&&Detail::Same(x.yaw,y.yaw)&&x.requested==y.requested,"AUTHORED_PATCH_GEOMETRY_REMAP");}
        Need(ForestCoverFootprint(terrain,p.transform.position,p.radius,settings),"AUTHORED_RETENTION_FOOTPRINT");
        const auto& a=assets.grassAssets[p.variant];const auto local=CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.maximumY);
        for(unsigned state=0;state<2;++state){Need(ForestCoverClearance(p.transform.position,p.radius,c.vegetation[state])&&
            CanopyVolume::ClearsCanopyWood(p.transform,local,c.vegetation[state])&&CanopyVolume::ClearsCanopyWood(p.transform,local,c.wood),"AUTHORED_RETENTION_TWO_WORLD_FULL_VOLUME");}
        for(std::size_t j=0;j<i;++j){budget.Op();const auto& other=grass.plants[j];Need(std::hypot(p.transform.position.x-other.transform.position.x,
            p.transform.position.z-other.transform.position.z)>=.65f*(p.radius+other.radius),"AUTHORED_RETENTION_ALL_PAIR_SPACING");}
    }
    for(auto used:sourceUse)Need(used==1,"AUTHORED_RETENTION_SOURCE_UNACCOUNTED");
    for(std::size_t i=0;i<patchCounts.size();++i)Need(patchCounts[i]==grass.patches[i].accepted&&patchCounts[i]>0,"AUTHORED_RETENTION_PATCH_COUNTS");
    Need(retained==r.retained&&filled==r.filled&&Detail::CoverIdentity(grass)==c.coverFingerprint&&
        Detail::RetentionIdentity(r)==c.retentionFingerprint,"AUTHORED_RETENTION_INDEPENDENT_FP");budget.Time();
}

inline std::uint64_t ContextFingerprint(const Context& c){Detail::Hash h;h.Wide(SourceFingerprint(c.source));h.Wide(c.author.fingerprint);
    h.Count(c.canopy.plants.size());h.Wide(c.canopy.fingerprint);h.Word(c.canopy.reachedBudget?1u:0u);
    for(auto n:{c.canopy.attempts,c.canopy.clearanceRejected,c.canopy.rootRejected,c.canopy.spacingRejected,c.canopy.meshRejected})h.Word(n);
    h.Wide(c.canopy.groundChecks);for(const auto& p:c.canopy.plants){h.Pose(p.pose);h.Box(p.lower);h.Float(p.crownRadius);h.Float(p.rootRadius);h.Wide(Detail::MeshIdentity(p.rootedTrunk));}
    h.Wide(c.canopyMetadata.originalFingerprint);h.Wide(c.canopyMetadata.faceChecks);h.Wide(c.canopyMetadata.exactOverlayFaces);
    h.Wide(c.canopyMetadata.woodParts);h.Word(c.canopyMetadata.appended);
    for(const auto& w:c.worlds)h.Wide(Detail::TypedIdentity(w));h.Wide(Detail::BoxesIdentity(c.sourceWood));h.Wide(Detail::BoxesIdentity(c.wood));
    h.Wide(Detail::CoverIdentity(c.sourceCover));h.Wide(Detail::RetentionIdentity(c.retention));h.Wide(Detail::LitterIdentity(c.litter));
    h.Wide(Detail::ColonyIdentity(c.colonies));h.Wide(Detail::ShrubIdentity(c.shrubs));return h.value;
}
inline void ValidateDownstreamVolumes(const Context& c,const Aevrynd::HeightField3D& terrain,const Assets& assets,W::Budget& budget){
    using Detail::Need;std::vector<unsigned> counts(c.colonies.colonies.size());
    for(std::size_t i=0;i<c.colonies.plants.size();++i){budget.Op();const auto& p=c.colonies.plants[i];
        Need(p.variant<4&&p.colony<c.colonies.colonies.size()&&p.grassPatch<c.retention.cover.patches.size(),"AUTHORED_COLONY_DOMAIN");++counts[p.colony];
        const auto& a=assets.fernAssets[p.variant];Need(Detail::Same(p.radius,a.radius*p.transform.scale.x)&&
            Detail::Same(p.transform.position.y,terrain.SampleHeight(p.transform.position.x,p.transform.position.z))&&
            ForestCoverFootprint(terrain,p.transform.position,p.radius,{}),"AUTHORED_COLONY_FOOTPRINT");
        const auto local=CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.height);
        for(unsigned state=0;state<2;++state)Need(ForestCoverClearance(p.transform.position,p.radius,c.vegetation[state])&&
            CanopyVolume::ClearsCanopyWood(p.transform,local,c.vegetation[state])&&CanopyVolume::ClearsCanopyWood(p.transform,local,c.wood),"AUTHORED_COLONY_TWO_WORLD_VOLUME");
        Need(FernColonyClearsExisting(p.transform.position,p.radius,c.author.understory.ferns,assets.fernAssets,.65f),"AUTHORED_COLONY_EXISTING_FERNS");
        for(const auto& g:c.retention.cover.plants){budget.Op();Need(std::hypot(p.transform.position.x-g.transform.position.x,
            p.transform.position.z-g.transform.position.z)>=.65f*(p.radius+g.radius),"AUTHORED_COLONY_GRASS_SPACING");}
        for(std::size_t j=0;j<i;++j){budget.Op();const auto& q=c.colonies.plants[j];Need(std::hypot(p.transform.position.x-q.transform.position.x,
            p.transform.position.z-q.transform.position.z)>=.65f*(p.radius+q.radius),"AUTHORED_COLONY_SELF_SPACING");}
    }
    for(std::size_t i=0;i<counts.size();++i)Need(counts[i]>0&&counts[i]==c.colonies.colonies[i].accepted,"AUTHORED_COLONY_ACCEPTED_COUNTS");
    for(std::size_t i=0;i<c.shrubs.plants.size();++i){budget.Op();const auto& p=c.shrubs.plants[i];Need(p.variant<4,"AUTHORED_SHRUB_VARIANT");
        const auto& a=c.shrubs.variants[p.variant];const auto r=p.pose.position;
        Need(Detail::Same(p.radius,a.radius*p.pose.scale.x)&&Detail::Same(r.y,terrain.SampleHeight(r.x,r.z))&&
            ForestCoverFootprint(terrain,r,p.radius,{}),"AUTHORED_SHRUB_FOOTPRINT");
        const auto local=CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.maximumY);
        for(unsigned state=0;state<2;++state)Need(ForestCoverClearance(r,p.radius,c.vegetation[state])&&
            CanopyVolume::ClearsCanopyWood(p.pose,local,c.vegetation[state])&&CanopyVolume::ClearsCanopyWood(p.pose,local,c.wood),"AUTHORED_SHRUB_TWO_WORLD_VOLUME");
        for(std::size_t j=0;j<c.author.understory.ferns.size();++j){budget.Op();const auto& q=c.author.understory.ferns[j];
            Need(std::hypot(r.x-q.position.x,r.z-q.position.z)>=p.radius+assets.fernAssets[j%4].radius*q.scale.x+.03f,"AUTHORED_SHRUB_ORIGINAL_FERN_SPACING");}
        for(const auto& q:c.colonies.plants){budget.Op();Need(std::hypot(r.x-q.transform.position.x,r.z-q.transform.position.z)>=p.radius+q.radius+.03f,"AUTHORED_SHRUB_COLONY_SPACING");}
        for(const auto& q:c.retention.cover.plants){budget.Op();Need(std::hypot(r.x-q.transform.position.x,r.z-q.transform.position.z)>=p.radius+q.radius+.03f,"AUTHORED_SHRUB_GRASS_SPACING");}
        for(std::size_t j=0;j<i;++j){budget.Op();const auto& q=c.shrubs.plants[j];Need(std::hypot(r.x-q.pose.position.x,r.z-q.pose.position.z)>=p.radius+q.radius+.3f,"AUTHORED_SHRUB_SELF_SPACING");}
    }
    budget.Time();
}
inline void ValidateContext(const Context& c,const Aevrynd::HeightField3D& terrain,const Assets& assets,W::Budget& budget){
    Detail::Need(assets.fingerprint&&AssetsFingerprint(assets)==assets.fingerprint,"AUTHORED_ASSETS_CHANGED");ValidateSourceContext(c.source,terrain);
    ValidateAuthor(c.author,c.source,terrain);Detail::Need(c.canopy.plants.size()==8&&c.canopy.reachedBudget&&c.canopyMetadata.appended==4&&
        c.canopyMetadata.originalFingerprint==6584434718760703877ull&&CanopyVolume::ValidWoodParts(c.wood)&&
        CanopyVolume::ValidWoodParts(c.sourceWood),"AUTHORED_FINAL_CANOPY_COUNTS");
    Detail::SameBoxes(std::span<const W::Box>(c.wood).first(4335),c.sourceWood,"AUTHORED_ORIGINAL_WOOD_PREFIX");
    for(unsigned state=0;state<2;++state)ValidateTypedWorld(c,state);ValidateRetentionMappings(c,terrain,assets,budget);
    Detail::Need(c.litter.reachedBudget&&c.litter.patches.size()==48&&c.colonies.reachedBudget&&c.colonies.plants.size()==300&&
        c.shrubs.reachedBudget&&c.shrubs.plants.size()==64,"AUTHORED_DOWNSTREAM_QUOTAS");
    Detail::Need(c.sourceCoverFingerprint==Detail::CoverIdentity(c.sourceCover)&&c.litterFingerprint==Detail::LitterIdentity(c.litter)&&
        c.colonyFingerprint==Detail::ColonyIdentity(c.colonies)&&c.shrubFingerprint==Detail::ShrubIdentity(c.shrubs)&&
        c.fingerprint==ContextFingerprint(c),"AUTHORED_CONTEXT_FP_CHANGED");
    ValidateDownstreamVolumes(c,terrain,assets,budget);
}

inline Context CreateContext(const Aevrynd::HeightField3D& terrain,const Assets& assets,const SourceContext& source,
    std::span<const W::Placement> qualifiedScan,W::Budget& budget){
    budget.Time();ValidateSourceContext(source,terrain);Detail::Need(assets.fingerprint&&AssetsFingerprint(assets)==assets.fingerprint,"AUTHORED_ASSETS_CHANGED");
    W::VerifyCurrentActiveRoles(qualifiedScan);Context c;c.source=source;
    // 上層保留正式原四倒木 overlay 證書；此處只核對同一完整固定姿態，絕不替代資格計算。
    const auto& fixed=FallenLogPlacements();for(unsigned i=0;i<4;++i)Detail::Need(qualifiedScan[i].propId==fixed[i].propId&&
        Detail::Same(qualifiedScan[i].anchor,fixed[i].authoredAnchor)&&Detail::Same(qualifiedScan[i].pose,fixed[i].pose)&&
        Detail::Same(qualifiedScan[i].collider,fixed[i].collider),"AUTHORED_QUALIFIED_LOG_MAPPING_CHANGED");
    for(unsigned state=0;state<2;++state){c.sourceMapped[state]=W::Bind(source.legacyWorlds[state],state!=0,source.props,qualifiedScan);
        auto expected=WorldSolids(state!=0,FirLayoutSelector::ForestWood,FallenLogProfile::ScannedStraightV1);
        for(const auto& p:source.middle.trees)expected.push_back(p.trunk);Detail::SameBoxes(W::Boxes(c.sourceMapped[state]),expected,"AUTHORED_SCANNED_SOURCE_WORLD");}
    c.author=CreateEntranceWorldAuthor(terrain,source.firs,source.props,source.understory,source.broadleaf,source.middle,assets.fernAssets,FallenLogProfile::ScannedStraightV1);
    ValidateAuthor(c.author,source,terrain);const auto geometry=MeasureCanopy(assets.canopy);
    // Append 當刻 Main 尚未加任何木格，輸入必須是236作者 vegetation。
    c.canopy=AppendEntranceCanopy(source.canopy,assets.canopy,geometry,assets.derivedStem,terrain,c.author.VegetationSolids(false),
        c.author.understory,assets.fernAssets,c.canopyMetadata);
    for(unsigned i=0;i<c.canopy.plants.size();++i){budget.Time();auto& p=c.canopy.plants[i];
        p.rootedTrunk=PlantDerivedRootStem(assets.derivedStem,geometry,p.pose,terrain);p.lower=ExtendRootCollision(p.lower,p.rootedTrunk,p.pose);
        const auto proxy=CreateCanopyWoodProxy(p.rootedTrunk,assets.canopy[2],p.pose);
        Detail::Need(proxy.spacing==.15&&CanopyVolume::ValidWoodParts(proxy.parts)&&c.wood.size()+proxy.parts.size()<=9000,"AUTHORED_WOOD_PROXY_DOMAIN");
        if(i<4){const auto& old=source.canopy.plants[i];const auto oldRoot=PlantDerivedRootStem(assets.derivedStem,geometry,old.pose,terrain);
            const auto oldProxy=CreateCanopyWoodProxy(oldRoot,assets.canopy[2],old.pose);
            Detail::Need(Detail::Same(old.pose,p.pose)&&Detail::Same(oldRoot,p.rootedTrunk)&&Detail::Same(ExtendRootCollision(old.lower,oldRoot,old.pose),p.lower),"AUTHORED_ORIGINAL_FOUR_ROOT_BITS");
            Detail::SameBoxes(proxy.parts,oldProxy.parts,"AUTHORED_ORIGINAL_FOUR_PROXY_BITS");c.sourceWood.insert(c.sourceWood.end(),oldProxy.parts.begin(),oldProxy.parts.end());}
        c.wood.insert(c.wood.end(),proxy.parts.begin(),proxy.parts.end());}
    Detail::Need(c.sourceWood.size()==4335&&c.wood.size()==8617,"AUTHORED_EIGHT_FULL_WOOD_COUNT");ValidateEntranceFernWood(c.author,assets.fernAssets,c.wood);
    for(unsigned state=0;state<2;++state){c.worlds[state]=Detail::Filter(c.sourceMapped[state]);c.vegetation[state]=W::Boxes(c.worlds[state]);
        c.base[state].assign(c.vegetation[state].begin(),c.vegetation[state].begin()+213+state);
        for(const auto& s:c.worlds[state])if(s.role==W::Role::Middle)c.extras[state].push_back(s.box);
        c.extras[state].insert(c.extras[state].end(),c.wood.begin(),c.wood.end());
        for(unsigned id=0;id<c.wood.size();++id)c.worlds[state].push_back({W::Role::CanopyWood,id,c.wood[id]});
        c.typedFingerprints[state]=Detail::TypedIdentity(c.worlds[state]);c.worldFingerprints[state]=Detail::BoxesIdentity(W::Boxes(c.worlds[state]));ValidateTypedWorld(c,state);}
    const auto originalSolids=W::Boxes(c.sourceMapped[0]);
    c.sourceCover=CreateForestCover(terrain,source.firs.roots,source.understory,originalSolids,assets.grassAssets,{},c.sourceWood,true);
    Detail::Need(c.sourceCover.reachedBudget&&Detail::NativeCoverIdentity(c.sourceCover)==OriginalCoverIdentity,"AUTHORED_ORIGINAL_COVER_CERTIFICATE");
    ForestCoverWorldView oldView{terrain,source.firs.roots,source.understory,originalSolids,c.sourceWood,4335,0};
    ForestCoverWorldView newView{terrain,c.author.firs.roots,c.author.understory,c.vegetation[0],c.wood,8617,0};
    oldView.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(oldView);newView.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(newView);
    c.retention=CreateForestCoverRetained(c.sourceCover,oldView,newView,assets.grassAssets,assets.grassIdentities);
    c.litter=CreateTrailLitter(terrain,W::Boxes(c.worlds[0]));
    c.colonies=CreateFernColonies(terrain,c.retention.cover,c.author.understory.ferns,c.vegetation[0],assets.fernAssets,{},c.wood,true);
    c.shrubs=CreateShrubCanopy(assets.shrubLeaf,terrain,c.vegetation[0],c.author.understory,c.retention.cover,c.colonies,assets.fernAssets,64,c.wood,true);
    c.sourceCoverFingerprint=Detail::CoverIdentity(c.sourceCover);c.coverFingerprint=Detail::CoverIdentity(c.retention.cover);c.retentionFingerprint=Detail::RetentionIdentity(c.retention);
    c.litterFingerprint=Detail::LitterIdentity(c.litter);c.colonyFingerprint=Detail::ColonyIdentity(c.colonies);c.shrubFingerprint=Detail::ShrubIdentity(c.shrubs);
    c.fingerprint=ContextFingerprint(c);ValidateContext(c,terrain,assets,budget);return c;
}

inline void RepeatControls(const Context& c,const Aevrynd::HeightField3D& terrain,const Assets& assets,W::Budget& budget){
    const auto originalSolids=W::Boxes(c.sourceMapped[0]);ForestCoverWorldView oldView{terrain,c.source.firs.roots,c.source.understory,originalSolids,c.sourceWood,4335,0};
    ForestCoverWorldView newView{terrain,c.author.firs.roots,c.author.understory,c.vegetation[0],c.wood,8617,0};
    oldView.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(oldView);newView.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(newView);
    const auto sourceRepeat=CreateForestCover(terrain,c.source.firs.roots,c.source.understory,originalSolids,assets.grassAssets,{},c.sourceWood,true);
    const auto retainedRepeat=CreateForestCoverRetained(sourceRepeat,oldView,newView,assets.grassAssets,assets.grassIdentities);budget.Time();
    const auto litter=CreateTrailLitter(terrain,W::Boxes(c.worlds[0]));
    const auto colonies=CreateFernColonies(terrain,retainedRepeat.cover,c.author.understory.ferns,c.vegetation[0],assets.fernAssets,{},c.wood,true);
    const auto shrubs=CreateShrubCanopy(assets.shrubLeaf,terrain,c.vegetation[0],c.author.understory,retainedRepeat.cover,colonies,assets.fernAssets,64,c.wood,true);
    Detail::Need(Detail::SameCover(sourceRepeat,c.sourceCover)&&Detail::SameRetention(retainedRepeat,c.retention)&&
        Detail::SameLitter(litter,c.litter)&&Detail::SameColonies(colonies,c.colonies)&&Detail::SameShrubs(shrubs,c.shrubs),"AUTHORED_FULL_REPEAT_BITS_CHANGED");
    Detail::Need(Detail::CoverIdentity(sourceRepeat)==c.sourceCoverFingerprint&&Detail::RetentionIdentity(retainedRepeat)==c.retentionFingerprint&&
        Detail::LitterIdentity(litter)==c.litterFingerprint&&Detail::ColonyIdentity(colonies)==c.colonyFingerprint&&Detail::ShrubIdentity(shrubs)==c.shrubFingerprint,"AUTHORED_FULL_REPEATS_CHANGED");
    budget.Time();
}
}
