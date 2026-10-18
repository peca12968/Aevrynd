#pragma once
#include "MistwatchEntranceCanopy.h"
#include "MistwatchMiddleLayer.h"

namespace Mistwatch {
enum class EntranceRole : unsigned { Fir, Rock, Sapling, Middle };
struct EntranceWithdrawal {
    EntranceRole role;unsigned sourceId;
    std::array<std::uint32_t,9> pose;
    std::array<std::uint32_t,6> box;
    unsigned patch;
};
// m2174 真來源角色；同一表同時決定渲染、根位置、角色、鏡頭及地被的退讓。
inline constexpr std::array<EntranceWithdrawal,11> EntranceWithdrawals{{
    {EntranceRole::Fir,7u,{{0x41042152u,0x00000000u,0x412af26cu,0x00000000u,0x409211f5u,0x00000000u,0x3f2eb6d6u,0x3f2eb6d6u,0x3f2eb6d6u}},{{0x41042152u,0x40066542u,0x412af26cu,0x3e866542u,0x40066542u,0x3e866542u}},1u},
    {EntranceRole::Fir,47u,{{0xc1629eb4u,0x3f9aed29u,0x407ff81du,0x00000000u,0x3dda5122u,0x00000000u,0x3f21a0aeu,0x3f21a0aeu,0x3f21a0aeu}},{{0xc1629eb4u,0x4049caccu,0x407ff81du,0x3e78a86fu,0x3ff8a86fu,0x3e78a86fu}},10u},
    {EntranceRole::Fir,63u,{{0x413c2f61u,0x3f1189e2u,0x40b442dbu,0x00000000u,0x407bc738u,0x00000000u,0x3f486d16u,0x3f486d16u,0x3f486d16u}},{{0x413c2f61u,0x403e8f00u,0x40b442dbu,0x3e9a2c87u,0x401a2c87u,0x3e9a2c87u}},14u},
    {EntranceRole::Fir,94u,{{0x418cf004u,0x3fbc8b12u,0x41d2d029u,0x00000000u,0x40b8b2c6u,0x00000000u,0x3f2091b8u,0x3f2091b8u,0x3f2091b8u}},{{0x418cf004u,0x4059c952u,0x41d2d029u,0x3e770792u,0x3ff70792u,0x3e770792u}},22u},
    {EntranceRole::Fir,95u,{{0x41a3fe65u,0x3fe7f77du,0x41c8cde4u,0x00000000u,0x40a9d254u,0x00000000u,0x3f2db18du,0x3f2db18du,0x3f2db18du}},{{0x41a3fe65u,0x40799804u,0x41c8cde4u,0x3e859c45u,0x40059c45u,0x3e859c45u}},22u},
    {EntranceRole::Rock,12u,{{0xc113a810u,0xbdf5c28fu,0x405ad5b0u,0x00000000u,0x40a949a5u,0x00000000u,0x3f820859u,0x3f7a6054u,0x3f802910u}},{{0xc113a810u,0x3e1848d2u,0x405ad5b0u,0x3f1ca3e0u,0x3e89950du,0x3f11967fu}},0u},
    {EntranceRole::Rock,17u,{{0x4102ca6cu,0xbdf5c28fu,0x40d074f6u,0x00000000u,0x40c49a38u,0x00000000u,0x3fbe3ba1u,0x3fa1b046u,0x3fac0a28u}},{{0x4102ca6cu,0x3e68833eu,0x40d074f6u,0x3f1aa088u,0x3eb1b243u,0x3f3cb582u}},0u},
    {EntranceRole::Rock,31u,{{0x411456fcu,0xbdf5c28fu,0x414a7870u,0x00000000u,0x404f5077u,0x00000000u,0x3f772085u,0x3f5ce046u,0x3f86c2c2u}},{{0x411456fcu,0x3defba2bu,0x414a7870u,0x3ec4f5f0u,0x3e72be5du,0x3f0e7b4bu}},0u},
    {EntranceRole::Sapling,15u,{{0xc196a2f4u,0x3fe98cefu,0xc05d0d42u,0x00000000u,0x3e5c5588u,0x00000000u,0x3e9b7cacu,0x3e9b7cacu,0x3e9b7cacu}},{{0xc196a2f4u,0x4054c678u,0xc05d0d42u,0x3def35f5u,0x3fc00000u,0x3def35f5u}},0u},
    {EntranceRole::Sapling,18u,{{0x4159854cu,0x3f6a1f77u,0x41bdbfb4u,0x00000000u,0x3fc56298u,0x00000000u,0x3e6be4f5u,0x3e6be4f5u,0x3e6be4f5u}},{{0x4159854cu,0x401a87deu,0x41bdbfb4u,0x3db5750cu,0x3fc00000u,0x3db5750cu}},0u},
    {EntranceRole::Middle,16u,{{0xc13a89e4u,0x3ec4ad3au,0xbee08320u,0x00000000u,0x3f2a0703u,0x00000000u,0x3f49e33au,0x3f49e33au,0x3f49e33au}},{{0xc13a4912u,0x3f961cecu,0xbf2d0a9cu,0x3eae118cu,0x3f49e33au,0x3eb18768u}},0u}
}};
struct EntranceFernMove {unsigned sourceId;float dx,dz;std::array<std::uint32_t,9> originalPose;};
// 五株保留原 source index 與 index%4 變體，不刪除、不重生、不壓縮。
inline constexpr std::array<EntranceFernMove,5> EntranceFernMoves{{
    {114u,-1.5f,2.f,{{0x41375a48u,0x3f02e3d7u,0x4116b196u,0x00000000u,0x400e7a54u,0x00000000u,0x3f28afe7u,0x3f28afe7u,0x3f28afe7u}}},
    {127u,2.f,2.f,{{0xc1468e7au,0x3f333144u,0x40385ea8u,0x00000000u,0x40b6a44eu,0x00000000u,0x3f29d514u,0x3f29d514u,0x3f29d514u}}},
    {281u,1.5f,-2.f,{{0xc14f9fcau,0x3f4f9b22u,0x3fd47710u,0x00000000u,0x407bd458u,0x00000000u,0x3f1a2e6eu,0x3f1a2e6eu,0x3f1a2e6eu}}},
    {287u,-1.5f,-1.f,{{0x41859dc4u,0x3fd9bc55u,0x41b7eb42u,0x00000000u,0x3f8f2500u,0x00000000u,0x3f4333d6u,0x3f4333d6u,0x3f4333d6u}}},
    {399u,-2.f,1.5f,{{0xc1a79151u,0x4013aaacu,0xc08e234fu,0x00000000u,0x404ed4e5u,0x00000000u,0x3f2013d3u,0x3f2013d3u,0x3f2013d3u}}}
}};
namespace EntranceWorldDetail {
inline void Need(bool value,const char* gate,unsigned id=0){
    if(!value)throw std::runtime_error("ENTRANCE_WORLD_FIRST_DIVERGENCE source_id="+
        std::to_string(id)+" gate="+gate);
}
inline auto PoseWords(const Transform3D& p){
    std::array<std::uint32_t,9> words{};unsigned i{};
    for(float v:{p.position.x,p.position.y,p.position.z,p.rotation.x,p.rotation.y,p.rotation.z,
        p.scale.x,p.scale.y,p.scale.z})words[i++]=std::bit_cast<std::uint32_t>(v);
    return words;
}
inline auto BoxWords(const BoxCollider3D& p){
    std::array<std::uint32_t,6> words{};unsigned i{};
    for(float v:{p.center.x,p.center.y,p.center.z,p.extents.x,p.extents.y,p.extents.z})
        words[i++]=std::bit_cast<std::uint32_t>(v);
    return words;
}
inline bool Withdraw(EntranceRole role,unsigned id){
    return std::any_of(EntranceWithdrawals.begin(),EntranceWithdrawals.end(),
        [&](const auto& record){return record.role==role&&record.sourceId==id;});
}
inline std::uint64_t FirFingerprint(const MatureFirLayout& layout){
    std::uint64_t hash=14695981039346656037ull;
    for(const auto& fir:layout.firs){
        for(float value:{fir.root.x,fir.root.y,fir.root.z,fir.render.rotation.y,fir.render.scale.x,
            fir.collider.center.x,fir.collider.center.y,fir.collider.center.z,fir.collider.extents.x,
            fir.collider.extents.y,fir.collider.extents.z})Detail::Mix(hash,value);
        Detail::Mix(hash,fir.patch);
    }
    return hash;
}
}
// 所有使用者只取這份已驗原身分的作者狀態；WorldSolids 不再重生被退讓角色。
struct EntranceWorldAuthor {
    bool applied{};
    FallenLogProfile fallenProfile{FallenLogProfile::LegacyCylinder};
    MatureFirLayout firs;
    std::vector<ForestProp> props;
    ForestUnderstory understory;
    std::vector<BroadleafTree> broadleaf;
    MiddleLayerResult middle;
    std::vector<unsigned> firSourceIds,propSourceIds,saplingSourceIds,middleSourceIds;
    std::vector<BoxCollider3D> mission;
    std::uint64_t sourceFirFingerprint{},sourceMiddleFingerprint{},fingerprint{};
    std::vector<BoxCollider3D> WorldSolids(bool repaired)const{
        EntranceWorldDetail::Need(applied&&mission.size()==33,"unapplied-author-world");
        auto result=mission;
        if(repaired)result.push_back({{0,-.2f,36},{2,.2f,4}});
        for(const auto& fir:firs.firs)result.push_back(fir.collider);
        for(const auto& prop:props)result.push_back(prop.collider);
        for(const auto& sapling:understory.saplings)result.push_back(SaplingCollider(sapling));
        for(const auto& tree:broadleaf)result.push_back(tree.trunk);
        return result;
    }
    std::vector<BoxCollider3D> VegetationSolids(bool repaired)const{
        auto result=WorldSolids(repaired);
        for(const auto& tree:middle.trees)result.push_back(tree.trunk);
        return result;
    }
};
inline EntranceWorldAuthor CreateEntranceWorldAuthor(const HeightField3D& terrain,
    const MatureFirLayout& sourceFirs,std::span<const ForestProp> retainedProps,
    const ForestUnderstory& sourceUnderstory,std::span<const BroadleafTree> sourceBroadleaf,
    const MiddleLayerResult& sourceMiddle,const std::array<FernColonyAsset,4>& fernAssets,
    FallenLogProfile profile){
    using namespace EntranceWorldDetail;
    Need(sourceFirs.reachedBudget&&sourceFirs.firs.size()==100&&sourceFirs.roots.size()==100&&
        sourceFirs.fingerprint==8236695966185867318ull&&
        FirFingerprint(sourceFirs)==sourceFirs.fingerprint,"original-fir-full-identity");
    Need(sourceMiddle.reachedBudget&&sourceMiddle.trees.size()==24&&
        MiddleLayerTransformFingerprint(sourceMiddle.trees)==10337542786750772359ull,"original-middle-identity");
    Need(sourceUnderstory.ferns.size()==420&&sourceUnderstory.saplings.size()==42&&sourceBroadleaf.size()==12,
        "original-understory-broadleaf-counts");
    ValidateFallenLogOriginalProps(retainedProps);
    const auto canonicalUnderstory=CreateUnderstory(terrain,sourceFirs.roots,retainedProps);
    Need(canonicalUnderstory.ferns.size()==420&&canonicalUnderstory.saplings.size()==42,"canonical-understory-counts");
    for(unsigned id=0;id<420;++id)Need(PoseWords(sourceUnderstory.ferns[id])==
        PoseWords(canonicalUnderstory.ferns[id]),"original-fern-full-bit-identity",id);
    for(unsigned id=0;id<42;++id)Need(PoseWords(sourceUnderstory.saplings[id])==
        PoseWords(canonicalUnderstory.saplings[id]),"original-sapling-full-bit-identity",id);
    const auto canonicalBroadleaf=CreateBroadleafTrees(terrain,sourceFirs.roots,retainedProps,sourceUnderstory);
    Need(canonicalBroadleaf.size()==12,"canonical-broadleaf-count");
    for(unsigned id=0;id<12;++id)Need(PoseWords(sourceBroadleaf[id].transform)==
        PoseWords(canonicalBroadleaf[id].transform)&&BoxWords(sourceBroadleaf[id].trunk)==
        BoxWords(canonicalBroadleaf[id].trunk),"original-broadleaf-full-bit-identity",id);
    for(unsigned id=0;id<100;++id){
        const auto& fir=sourceFirs.firs[id];
        Need(FallenLogDetail::Same(fir.root,sourceFirs.roots[id])&&
            FallenLogDetail::Same(fir.root,fir.render.position)&&fir.render.rotation.x==0&&fir.render.rotation.z==0&&
            fir.render.scale.x==fir.render.scale.y&&fir.render.scale.x==fir.render.scale.z&&
            BoxWords(fir.collider)==BoxWords(MatureFirCollider(fir.root,fir.render.scale.x)),
            "original-fir-render-root-body-alignment",id);
    }
    // 先驗所有指定角色的原 pose/box；失敗只留局部候選，不半套套用。
    for(const auto& r:EntranceWithdrawals){
        Transform3D pose;BoxCollider3D box;
        switch(r.role){
        case EntranceRole::Fir:
            Need(r.sourceId<100,"fir-source-domain",r.sourceId);
            pose=sourceFirs.firs[r.sourceId].render;box=sourceFirs.firs[r.sourceId].collider;
            Need(sourceFirs.firs[r.sourceId].patch==r.patch,"fir-original-patch",r.sourceId);break;
        case EntranceRole::Rock:
            Need(r.sourceId<retainedProps.size()&&!retainedProps[r.sourceId].FallenLog(),"rock-source-role",r.sourceId);
            pose=retainedProps[r.sourceId].transform;box=retainedProps[r.sourceId].collider;break;
        case EntranceRole::Sapling:
            Need(r.sourceId<42,"sapling-source-domain",r.sourceId);
            pose=sourceUnderstory.saplings[r.sourceId];box=SaplingCollider(pose);break;
        case EntranceRole::Middle:
            Need(r.sourceId<24,"middle-source-domain",r.sourceId);
            pose=sourceMiddle.trees[r.sourceId].transform;box=sourceMiddle.trees[r.sourceId].trunk;break;
        }
        Need(PoseWords(pose)==r.pose&&BoxWords(box)==r.box,"approved-original-role-bits",r.sourceId);
    }
    for(const auto& move:EntranceFernMoves)Need(PoseWords(sourceUnderstory.ferns[move.sourceId])==
        move.originalPose,"approved-original-fern-bits",move.sourceId);
    EntranceWorldAuthor result;
    result.fallenProfile=profile;result.sourceFirFingerprint=sourceFirs.fingerprint;
    result.sourceMiddleFingerprint=MiddleLayerTransformFingerprint(sourceMiddle.trees);
    result.firs=sourceFirs;result.firs.firs.clear();result.firs.roots.clear();
    for(unsigned id=0;id<100;++id)if(!Withdraw(EntranceRole::Fir,id)){
        result.firs.firs.push_back(sourceFirs.firs[id]);result.firs.roots.push_back(sourceFirs.roots[id]);
        result.firSourceIds.push_back(id);
    }
    result.firs.fingerprint=FirFingerprint(result.firs);
    result.firs.reachedBudget=result.firs.firs.size()==95;
    const auto mapped=ApplyFallenLogProfile(retainedProps,FirLayoutSelector::ForestWood,profile);
    for(unsigned id=0;id<mapped.size();++id)if(!Withdraw(EntranceRole::Rock,id)){
        result.props.push_back(mapped[id]);result.propSourceIds.push_back(id);
    }
    result.understory=sourceUnderstory;result.understory.saplings.clear();
    for(unsigned id=0;id<42;++id)if(!Withdraw(EntranceRole::Sapling,id)){
        result.understory.saplings.push_back(sourceUnderstory.saplings[id]);result.saplingSourceIds.push_back(id);
    }
    result.broadleaf.assign(sourceBroadleaf.begin(),sourceBroadleaf.end());
    result.middle=sourceMiddle;result.middle.trees.clear();
    for(unsigned id=0;id<24;++id)if(!Withdraw(EntranceRole::Middle,id)){
        result.middle.trees.push_back(sourceMiddle.trees[id]);result.middleSourceIds.push_back(id);
    }
    result.middle.reachedBudget=result.middle.trees.size()==23;
    const auto originalWorld=WorldSolids(false,FirLayoutSelector::ForestWood,profile);
    Need(originalWorld.size()==223,"original-world-order");
    result.mission.assign(originalWorld.begin(),originalWorld.begin()+33);
    std::size_t index=33;
    const auto originalBox=[&](const BoxCollider3D& box){
        Need(index<originalWorld.size()&&BoxWords(box)==BoxWords(originalWorld[index]),"original-world-every-box",unsigned(index));
        ++index;
    };
    for(const auto& fir:sourceFirs.firs)originalBox(fir.collider);
    for(const auto& prop:mapped)originalBox(prop.collider);
    for(const auto& sapling:sourceUnderstory.saplings)originalBox(SaplingCollider(sapling));
    for(const auto& tree:sourceBroadleaf)originalBox(tree.trunk);
    Need(index==originalWorld.size(),"original-world-complete-box-proof");
    for(const auto& move:EntranceFernMoves){
        auto& fern=result.understory.ferns[move.sourceId];
        fern.position.x+=move.dx;fern.position.z+=move.dz;
        Need(terrain.Contains(fern.position.x,fern.position.z),"relocated-fern-terrain-domain",move.sourceId);
        fern.position.y=terrain.SampleHeight(fern.position.x,fern.position.z);
        Need(Session::Finite(fern.position)&&terrain.SampleNormal(fern.position.x,fern.position.z).y>=.88f&&
            PropClearsObjectives({fern.position,{.9f,1.f,.9f}}),"relocated-fern-original-mission-slope",move.sourceId);
    }
    result.applied=true;
    const auto solids=result.VegetationSolids(false);
    for(const auto& move:EntranceFernMoves){
        const auto& fern=result.understory.ferns[move.sourceId];const auto& asset=fernAssets[move.sourceId%4];
        const float radius=asset.radius*fern.scale.x;
        Need(ForestCoverFootprint(terrain,fern.position,radius,{}),"relocated-fern-original-footprint",move.sourceId);
        for(const auto root:result.firs.roots)Need(std::hypot(root.x-fern.position.x,root.z-fern.position.z)>=1.f,
            "relocated-fern-original-fir-root-spacing",move.sourceId);
        for(const auto& prop:result.props)Need(std::abs(fern.position.x-prop.collider.center.x)>=prop.collider.extents.x+.9f||
            std::abs(fern.position.z-prop.collider.center.z)>=prop.collider.extents.z+.9f,
            "relocated-fern-original-prop-spacing",move.sourceId);
        for(const auto& sapling:result.understory.saplings)Need(std::hypot(sapling.position.x-fern.position.x,
            sapling.position.z-fern.position.z)>=1.f,"relocated-fern-original-sapling-spacing",move.sourceId);
        for(unsigned id=0;id<420;++id)if(id!=move.sourceId){
            const auto& other=result.understory.ferns[id];const float distance=std::hypot(
                fern.position.x-other.position.x,fern.position.z-other.position.z);
            const float required=(std::max)(1.f,.65f*(radius+fernAssets[id%4].radius*other.scale.x));
            if(!(distance>=1.f&&distance>=.65f*(radius+fernAssets[id%4].radius*other.scale.x)))
                throw std::runtime_error("ENTRANCE_WORLD_FIRST_DIVERGENCE source_id="+std::to_string(move.sourceId)+
                    " gate=relocated-fern-original-neighbor-spacing other_source_id="+std::to_string(id)+
                    " actual_distance="+std::to_string(distance)+" required="+std::to_string(required));
        }
        const auto local=CanopyVolume::ConservativeLocalVolume(asset.radius,asset.minimumY,asset.height);
        if(!CanopyVolume::ClearsCanopyWood(fern,local,solids)){
            for(std::size_t boxIndex=0;boxIndex<solids.size();++boxIndex)
                if(!CanopyVolume::ClearsCanopyWood(fern,local,std::span<const BoxCollider3D>(&solids[boxIndex],1)))
                    throw std::runtime_error("ENTRANCE_WORLD_FIRST_DIVERGENCE source_id="+std::to_string(move.sourceId)+
                        " gate=relocated-fern-full-volume-world world_box_index="+std::to_string(boxIndex));
            Need(false,"relocated-fern-full-volume-world-invalid",move.sourceId);
        }
    }
    result.fingerprint=14695981039346656037ull;
    for(const auto& box:solids)for(auto word:BoxWords(box))Detail::Mix(result.fingerprint,word);
    for(unsigned id=0;id<420;++id){for(auto word:PoseWords(result.understory.ferns[id]))
        Detail::Mix(result.fingerprint,word);Detail::Mix(result.fingerprint,id%4);}
    return result;
}
inline void ValidateEntranceFernWood(const EntranceWorldAuthor& author,
    const std::array<FernColonyAsset,4>& assets,std::span<const BoxCollider3D> wood){
    EntranceWorldDetail::Need(author.applied&&author.understory.ferns.size()==420&&
        CanopyVolume::ValidWoodParts(wood),"complete-authored-world-wood");
    for(unsigned id=0;id<420;++id){const auto& asset=assets[id%4];
        const auto local=CanopyVolume::ConservativeLocalVolume(asset.radius,asset.minimumY,asset.height);
        if(!CanopyVolume::ClearsCanopyWood(author.understory.ferns[id],local,wood)){
            for(std::size_t index=0;index<wood.size();++index)
                if(!CanopyVolume::ClearsCanopyWood(author.understory.ferns[id],local,
                    std::span<const BoxCollider3D>(&wood[index],1)))
                    throw std::runtime_error("ENTRANCE_WORLD_FIRST_DIVERGENCE source_id="+std::to_string(id)+
                        " gate=all-retained-fern-full-volume-all-eight-wood wood_box_index="+std::to_string(index));
            EntranceWorldDetail::Need(false,"all-retained-fern-full-volume-all-eight-wood-invalid",id);
        }
    }
}
}
