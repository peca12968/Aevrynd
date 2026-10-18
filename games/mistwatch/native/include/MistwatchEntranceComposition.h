#pragma once
#include "MistwatchEntranceWorldAuthor.h"

namespace Mistwatch {
inline constexpr std::string_view RightForegroundProfile="entrance-right-foreground-v1";
struct EntranceCompositionResult {
    EntranceWorldAuthor author;
    std::uint64_t originalFingerprint{},finalPoseFingerprint{};
    unsigned relocatedTrees{},relocatedFerns{};
};
namespace EntranceCompositionDetail {
using namespace Aevrynd;
using EntranceWorldDetail::PoseWords;
using EntranceWorldDetail::BoxWords;
inline void Need(bool value,const char* gate,unsigned id=0){
    if(!value)throw std::invalid_argument("ENTRANCE_COMPOSITION_FIRST_DIVERGENCE id="+std::to_string(id)+" gate="+gate);
}
inline std::uint64_t AuthorFingerprint(const EntranceWorldAuthor& author){
    std::uint64_t h=14695981039346656037ull;
    for(const auto& box:author.VegetationSolids(false))for(auto w:BoxWords(box))Detail::Mix(h,w);
    for(unsigned i=0;i<author.understory.ferns.size();++i){
        for(auto w:PoseWords(author.understory.ferns[i]))Detail::Mix(h,w);Detail::Mix(h,i%4);}
    return h;
}
inline std::uint64_t MeshIdentity(const Mesh3D& mesh){
    Need(mesh.Valid(),"actual-mesh-topology");std::uint64_t h=14695981039346656037ull;
    Detail::Mix(h,static_cast<unsigned>(mesh.vertices.size()));Detail::Mix(h,static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v}){
        Need(std::isfinite(f),"actual-mesh-finite");Detail::Mix(h,std::bit_cast<std::uint32_t>(f));}
        Detail::Mix(h,v.color);Detail::Mix(h,v.normal);}
    for(auto index:mesh.indices)Detail::Mix(h,index);return h;
}
inline bool Touch(const BoxCollider3D& a,const BoxCollider3D& b,float margin=.03f){
    return std::abs(a.center.x-b.center.x)<=a.extents.x+b.extents.x+margin&&
        std::abs(a.center.y-b.center.y)<=a.extents.y+b.extents.y+margin&&
        std::abs(a.center.z-b.center.z)<=a.extents.z+b.extents.z+margin;
}
inline BoxCollider3D Crown(const std::array<Mesh3D,3>& meshes,const Transform3D& pose){
    Vec3 lo{10000,10000,10000},hi{-10000,-10000,-10000};const auto matrix=pose.Matrix();
    for(const auto& mesh:meshes)for(const auto& v:mesh.vertices){const auto p=TransformPoint(matrix,{v.x,v.y,v.z});
        Need(Session::Finite(p),"actual-crown-finite");
        lo.x=(std::min)(lo.x,p.x);lo.y=(std::min)(lo.y,p.y);lo.z=(std::min)(lo.z,p.z);
        hi.x=(std::max)(hi.x,p.x);hi.y=(std::max)(hi.y,p.y);hi.z=(std::max)(hi.z,p.z);}
    return {(lo+hi)*.5f,(hi-lo)*.5f};
}
}
// 固定作者版本只搬一株闊葉與兩株蕨；所有檢查在副本完成後才交付同一世界狀態。
inline EntranceCompositionResult CreateRightForegroundComposition(const EntranceWorldAuthor& original,
    const Aevrynd::HeightField3D& terrain,const std::array<Aevrynd::Mesh3D,3>& meshes,
    const std::array<FernColonyAsset,4>& fernAssets,std::span<const Aevrynd::BoxCollider3D> wood){
    using namespace Aevrynd;using namespace EntranceCompositionDetail;
    Need(original.applied&&original.fallenProfile==FallenLogProfile::ScannedStraightV1&&
        original.broadleaf.size()==12&&original.middle.trees.size()==23&&original.firs.firs.size()==95&&
        original.props.size()==33&&original.understory.saplings.size()==40&&original.understory.ferns.size()==420,
        "canonical-populations");
    Need(original.fingerprint==7152938878276451026ull&&AuthorFingerprint(original)==original.fingerprint,"canonical-author-identity");
    Need(wood.size()==8617&&CanopyVolume::ValidWoodParts(wood),"complete-eight-canopy-wood");
    constexpr std::array<std::uint64_t,3> identities{14566862847592192531ull,15506843208519736896ull,17761613164067314083ull};
    for(unsigned i=0;i<3;++i)Need(MeshIdentity(meshes[i])==identities[i],"rooted-actual-mesh-identity",i);
    constexpr std::array<std::uint32_t,9> treeWords{1099254682u,1064920615u,3240233186u,0u,1081392860u,0u,1064543266u,1064543266u,1064543266u};
    constexpr std::array<unsigned,2> fernIds{188,332};
    constexpr std::array<std::array<std::uint32_t,9>,2> fernWords{{
        {1092317348u,0u,1081705916u,0u,1077962914u,0u,1059157712u,1059157712u,1059157712u},
        {1090854028u,0u,1081407980u,0u,1078338892u,0u,1059481454u,1059481454u,1059481454u}}};
    Need(PoseWords(original.broadleaf[9].transform)==treeWords,"source-tree-pose",9);
    for(unsigned i=0;i<2;++i)Need(PoseWords(original.understory.ferns[fernIds[i]])==fernWords[i],"source-fern-pose",fernIds[i]);
    EntranceCompositionResult out;out.author=original;out.originalFingerprint=original.fingerprint;
    auto& tree=out.author.broadleaf[9];tree.transform.position={9,terrain.SampleHeight(9,4)-.045f,4};
    const auto stem=TransformBounds(BroadleafLowerTrunkBounds(),tree.transform);tree.trunk={stem.center,stem.extents};
    const auto policy=TransformBounds(BroadleafBounds(),tree.transform);
    const auto actual=Crown(meshes,tree.transform);
    Need(terrain.Contains(9,4)&&terrain.SampleNormal(9,4).y>=.97f&&
        PropClearsObjectives({policy.center,policy.extents})&&BroadleafClearsSettlement(policy)&&
        PropClearsObjectives(actual)&&BroadleafClearsSettlement({actual.center,Length(actual.extents),actual.extents}),"tree-mission-slope");
    for(float dx:{-1.f,1.f})for(float dz:{-1.f,1.f}){
        const float x=stem.center.x+dx*stem.extents.x,z=stem.center.z+dz*stem.extents.z;
        Need(terrain.Contains(x,z)&&std::abs(terrain.SampleHeight(x,z)-terrain.SampleHeight(9,4))<=.1f,"tree-footprint");}
    for(const auto r:original.firs.roots)Need(std::hypot(r.x-9,r.z-4)>=3.f,"tree-fir-spacing");
    for(const auto& prop:original.props)Need(std::abs(stem.center.x-prop.collider.center.x)>=stem.extents.x+prop.collider.extents.x+.4f||
        std::abs(stem.center.z-prop.collider.center.z)>=stem.extents.z+prop.collider.extents.z+.4f,"tree-prop-spacing");
    for(const auto& p:original.understory.saplings)Need(std::hypot(p.position.x-9,p.position.z-4)>=1.5f,"tree-sapling-spacing");
    for(unsigned i=0;i<12;++i)if(i!=9){const auto p=original.broadleaf[i].transform.position;
        Need(std::hypot(p.x-9,p.z-4)>=5.f,"tree-broadleaf-spacing",i);}
    for(const auto& b:wood)Need(!Touch(actual,b)&&!Touch(tree.trunk,b),"tree-actual-crown-wood");
    for(const auto& b:original.mission)Need(!Touch(actual,b)&&!Touch(tree.trunk,b),"tree-actual-crown-mission");
    Need(!Touch(actual,{{0,-.2f,36},{2,.2f,4}}),"tree-repaired-bridge");
    for(unsigned i=0;i<original.middle.trees.size();++i)Need(!Touch(actual,original.middle.trees[i].trunk)&&
        !Touch(tree.trunk,original.middle.trees[i].trunk),"tree-middle-body",i);
    // 固定群落修正：188後移已通過預選；332前移以避開原石頭8與蕨308，不搜尋座標。
    for(auto id:fernIds){auto& fern=out.author.understory.ferns[id];fern.position.x+=2.f;
        fern.position.z+=id==188?-2.f:1.f;
        fern.position.y=terrain.SampleHeight(fern.position.x,fern.position.z);}
    const auto solids=out.author.VegetationSolids(false);
    for(auto id:fernIds){const auto& fern=out.author.understory.ferns[id];const auto p=fern.position;
        const auto& asset=fernAssets[id%4];const float radius=asset.radius*fern.scale.x;
        Need(Session::Finite(p)&&terrain.Contains(p.x,p.z)&&terrain.SampleNormal(p.x,p.z).y>=.88f&&
            PropClearsObjectives({p,{.9f,1,.9f}})&&ForestCoverFootprint(terrain,p,radius,{}),"fern-mission-footprint",id);
        for(const auto r:out.author.firs.roots)Need(std::hypot(p.x-r.x,p.z-r.z)>=1.f,"fern-fir-spacing",id);
        for(const auto& prop:out.author.props)Need(std::abs(p.x-prop.collider.center.x)>=prop.collider.extents.x+.9f||
            std::abs(p.z-prop.collider.center.z)>=prop.collider.extents.z+.9f,"fern-prop-spacing",id);
        for(const auto& other:out.author.understory.saplings)Need(std::hypot(p.x-other.position.x,p.z-other.position.z)>=1.f,"fern-sapling-spacing",id);
        for(unsigned j=0;j<420;++j)if(j!=id){const auto& other=out.author.understory.ferns[j];
            Need(std::hypot(p.x-other.position.x,p.z-other.position.z)>=(std::max)(1.f,.65f*(radius+fernAssets[j%4].radius*other.scale.x)),"fern-neighbor-spacing",j);}
        const auto volume=CanopyVolume::ConservativeLocalVolume(asset.radius,asset.minimumY,asset.height);
        Need(CanopyVolume::ClearsCanopyWood(fern,volume,solids)&&CanopyVolume::ClearsCanopyWood(fern,volume,wood),"fern-complete-world-volume",id);
    }
    for(unsigned i=0;i<420;++i){const auto& fern=out.author.understory.ferns[i];const auto p=fern.position;
        Need(std::abs(stem.center.x-p.x)>=stem.extents.x+.4f||std::abs(stem.center.z-p.z)>=stem.extents.z+.4f,"tree-all-fern-margin",i);
        const auto& a=fernAssets[i%4];const auto volume=CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.height);
        Need(CanopyVolume::ClearsCanopyWood(fern,volume,std::span<const BoxCollider3D>(&tree.trunk,1)),"tree-all-fern-volume",i);
    }
    ValidateEntranceFernWood(out.author,fernAssets,wood);
    out.author.fingerprint=AuthorFingerprint(out.author);
    out.finalPoseFingerprint=14695981039346656037ull;
    for(const auto& t:out.author.broadleaf)for(auto w:PoseWords(t.transform))Detail::Mix(out.finalPoseFingerprint,w);
    for(const auto& p:out.author.understory.ferns)for(auto w:PoseWords(p))Detail::Mix(out.finalPoseFingerprint,w);
    out.relocatedTrees=1;out.relocatedFerns=2;return out;
}
}
