#pragma once
#include "MistwatchNettleAsset.h"
#include "MistwatchForestCover.h"
#include "MistwatchWoodlandBridgeWorld.h"
#include <bit>

namespace Mistwatch {
inline constexpr std::string_view NettleMixProfile="nettle-mixed-v1";
inline constexpr float NettleWindHorizontal=.19f,NettleWindUpward=.011f;
struct NettleMixResult {
    std::vector<unsigned> renderVariants;
    std::array<unsigned,6> variants{};
    unsigned accepted{},rejected{},worldChecks{};
    std::uint64_t fingerprint{14695981039346656037ull};
};
inline CanopyVolume::PlantLocalVolume NettleWindVolume(const NettleAssetBounds& asset,float scale) {
    if(!std::isfinite(scale)||scale<=0)throw std::invalid_argument("Invalid nettle world scale");
    // Shader 的風位移發生在世界座標，不隨 instance scale 縮小。
    return CanopyVolume::ConservativeLocalVolume(asset.radius+NettleWindHorizontal/scale,
        asset.minimumY,asset.maximumY+NettleWindUpward/scale);
}
inline NettleMixResult CreateNettleMix(const ForestCoverResult& source,
    const Aevrynd::HeightField3D& terrain,const std::array<NettleAssetBounds,6>& assets,
    std::span<const Aevrynd::BoxCollider3D> vegetation,
    std::span<const Aevrynd::BoxCollider3D> completeBefore,
    std::span<const Aevrynd::BoxCollider3D> completeAfter,
    const WoodlandBridgeWorld::ValidatedWorld* groupedWorld=nullptr) {
    if(groupedWorld)groupedWorld->RequireValid();
    if(!source.reachedBudget||source.plants.size()!=3600)
        throw std::invalid_argument("Nettle requires complete authored two-bridge world");
    if(groupedWorld){
        if(!completeBefore.empty()||!completeAfter.empty())
            throw std::invalid_argument("Nettle mixed grouped and flat world");
        const auto body=groupedWorld->Bodies(false);
        if(vegetation.size()!=body.size())throw std::invalid_argument("Nettle requires fixed woodland body order");
        const auto same=[](const Aevrynd::BoxCollider3D& a,const Aevrynd::BoxCollider3D& b){
            const std::array<std::uint32_t,6> left{{std::bit_cast<std::uint32_t>(a.center.x),std::bit_cast<std::uint32_t>(a.center.y),
                std::bit_cast<std::uint32_t>(a.center.z),std::bit_cast<std::uint32_t>(a.extents.x),
                std::bit_cast<std::uint32_t>(a.extents.y),std::bit_cast<std::uint32_t>(a.extents.z)}};
            const std::array<std::uint32_t,6> right{{std::bit_cast<std::uint32_t>(b.center.x),std::bit_cast<std::uint32_t>(b.center.y),
                std::bit_cast<std::uint32_t>(b.center.z),std::bit_cast<std::uint32_t>(b.extents.x),
                std::bit_cast<std::uint32_t>(b.extents.y),std::bit_cast<std::uint32_t>(b.extents.z)}};
            return left==right;
        };
        if(!std::equal(vegetation.begin(),vegetation.end(),body.begin(),body.end(),same))
            throw std::invalid_argument("Nettle requires fixed woodland body order");
    }else if(vegetation.size()!=236||completeBefore.size()!=8853||completeAfter.size()!=8854||
        !CanopyVolume::ValidWoodParts(vegetation)||!CanopyVolume::ValidWoodParts(completeBefore)||
        !CanopyVolume::ValidWoodParts(completeAfter))throw std::invalid_argument("Nettle requires complete authored two-bridge world");
    for(const auto& asset:assets)if(!CanopyVolume::Detail::ValidLocal(
        {asset.radius,asset.minimumY,asset.maximumY}))throw std::invalid_argument("Invalid complete nettle source bounds");
    NettleMixResult result;result.renderVariants.assign(source.plants.size(),NettleVariantCount);
    const auto word=[&](std::uint32_t v){for(unsigned b=0;b<4;++b){result.fingerprint^=(v>>(b*8))&255u;result.fingerprint*=1099511628211ull;}};
    constexpr std::array<unsigned,3> eligibleVariants{0,1,4};
    for(std::size_t index=0;index<source.plants.size();++index) {
        const auto& plant=source.plants[index];const auto& pose=plant.transform;
        if(!Session::Finite(pose.position)||!Session::Finite(pose.scale)||!Session::Finite(pose.rotation)||
            pose.scale.x<=0||pose.scale.x!=pose.scale.y||pose.scale.x!=pose.scale.z||pose.rotation.x!=0||
            pose.rotation.z!=0||plant.variant>=5||!std::isfinite(plant.radius)||plant.radius<=0||
            pose.position.y!=terrain.SampleHeight(pose.position.x,pose.position.z))
            throw std::invalid_argument("Nettle canonical root/pose invalid");
        // 固定來源序號每三株混一株，不依鏡頭搜尋、不改根位置，不改原人口或變體。
        if(index%3==0) {
            const unsigned variant=eligibleVariants[(index/3)%eligibleVariants.size()];
            const auto local=NettleWindVolume(assets[variant],pose.scale.x);
            const float radius=local.radius*pose.scale.x;
            // 包絡不大於原認證半徑，故原全對 root .65 間距證明仍適用。
            const bool footprint=radius<=plant.radius&&ForestCoverFootprint(terrain,pose.position,radius,{})&&
                ForestCoverClearance(pose.position,radius,vegetation);
            const bool before=groupedWorld?groupedWorld->Clears(pose,local,false):CanopyVolume::ClearsCanopyWood(pose,local,completeBefore);
            const bool after=groupedWorld?groupedWorld->Clears(pose,local,true):CanopyVolume::ClearsCanopyWood(pose,local,completeAfter);result.worldChecks+=2;
            if(footprint&&before&&after){result.renderVariants[index]=variant;++result.variants[variant];++result.accepted;}
            else ++result.rejected;
        }
        word(unsigned(index));word(result.renderVariants[index]);
        for(float v:{pose.position.x,pose.position.y,pose.position.z,pose.rotation.y,pose.scale.x})word(std::bit_cast<std::uint32_t>(v));
    }
    if(result.accepted+result.rejected!=1200||result.worldChecks!=2400)
        throw std::runtime_error("Nettle finite source selection incomplete");
    return result;
}
}
