#pragma once
#include "MistwatchRoadsideCover.h"
#include "MistwatchNettleAsset.h"

namespace Mistwatch {
inline constexpr std::string_view RoadsideBroadleafProfile="roadside-broadleaf-v1";
inline constexpr std::array<unsigned,2> RoadsideBroadleafVariants{2,3};
inline constexpr unsigned RoadsideBroadleafPerSide=48;

inline RoadsideCoverResult CreateRoadsideBroadleaf(const Aevrynd::HeightField3D& terrain,
    const ForestCoverResult& original,const ForestUnderstory& understory,const FernColonyResult& colonies,
    const ShrubCanopyResult& shrubs,const std::array<ForestCoverAsset,5>& grassAssets,
    const std::array<FernColonyAsset,4>& fernAssets,const std::array<NettleAssetBounds,2>& assets,
    std::span<const Aevrynd::BoxCollider3D> before,std::span<const Aevrynd::BoxCollider3D> after,const ShrubCompositionContract& composition={},
    const WoodlandBridgeWorld::ValidatedWorld* groupedWorld=nullptr) {
    ValidateRoadsideCoverInputs(original,understory,colonies,shrubs,grassAssets,fernAssets,before,after,composition,groupedWorld);
    for(const auto& a:assets)if(!CanopyVolume::Detail::ValidLocal({a.radius,a.minimumY,a.maximumY})||
        a.radius>4||a.maximumY<=0||a.maximumY-a.minimumY<=0)
        throw std::invalid_argument("ROADSIDE_BROADLEAF_ACTUAL_VOLUME_REQUIRED");
    RoadsideCoverResult result;std::uint32_t seed=0x52424c31u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    // 世界固定的小群落，不沿路排成等距護欄。左右各有有限嘗試與相同品質門。
    for(float side:{-1.f,1.f}){
        unsigned accepted{};
        for(unsigned attempt=0;attempt<1024&&accepted<RoadsideBroadleafPerSide;++attempt){
            ++result.attempted;
            const unsigned patch=attempt%16;
            const float centreX=3.15f+.72f*float(patch%4);
            const float centreZ=-3.7f+8.1f*float(patch/4);
            Aevrynd::Vec3 root{side*(centreX+(random()-.5f)*1.4f),0,centreZ+(random()-.5f)*5.4f};
            root.y=terrain.SampleHeight(root.x,root.z);
            const unsigned variant=random()<.5f?0u:1u;const auto& a=assets[variant];
            const float height=variant==0?.26f+.09f*random():.10f+.06f*random();
            const float scale=height/(a.maximumY-a.minimumY);
            const float radius=a.radius*scale+.19f;
            if(!RoadsideCoverRouteClearance(root,radius)){++result.rejectedRoute;continue;}
            // 真掃描資產的最低點接地；風在世界座標有固定上界，不因縮放而消失。
            const Aevrynd::Transform3D pose{{root.x,root.y-a.minimumY*scale,root.z},
                {0,random()*6.2831853f,0},{scale,scale,scale}};
            const CanopyVolume::PlantVolume volume{pose.position,
                CanopyVolume::ConservativeLocalVolume(a.radius+.19f/scale,a.minimumY,a.maximumY+.011f/scale),
                scale,pose.rotation};
            const bool clearsBefore=groupedWorld?groupedWorld->Clears(volume,false):CanopyVolume::ClearsCanopyWood(volume,before);
            const bool clearsAfter=groupedWorld?groupedWorld->Clears(volume,true):CanopyVolume::ClearsCanopyWood(volume,after);
            if(!clearsBefore||!clearsAfter){
                ++result.rejectedWorld;continue;}
            if(!ForestCoverFootprint(terrain,root,radius,{})){++result.rejectedTerrain;continue;}
            bool overlap=false;
            const auto close=[&](Aevrynd::Vec3 p,float r){return std::hypot(root.x-p.x,root.z-p.z)<.65f*(radius+r);};
            for(const auto& p:original.plants)if(close(p.transform.position,p.radius+.19f)){overlap=true;break;}
            if(!overlap)for(unsigned i=0;i<understory.ferns.size();++i)
                if(close(understory.ferns[i].position,fernAssets[i%4].radius*understory.ferns[i].scale.x+.19f)){overlap=true;break;}
            if(!overlap)for(const auto& p:colonies.plants)if(close(p.transform.position,p.radius+.19f)){overlap=true;break;}
            if(!overlap)for(const auto& p:shrubs.plants)if(close(p.pose.position,p.radius+.19f)){overlap=true;break;}
            if(!overlap)for(const auto& p:result.plants)if(close(p.transform.position,p.radius)){overlap=true;break;}
            if(overlap){++result.rejectedVegetation;continue;}
            result.plants.push_back({pose,variant,patch,radius});++accepted;
            if(side<0)++result.left;else ++result.right;
            ForestCoverRetentionDetail::Hasher hash;hash.value=result.fingerprint;hash.Plant(result.plants.back());result.fingerprint=hash.value;
        }
        if(accepted!=RoadsideBroadleafPerSide)throw std::runtime_error("ROADSIDE_BROADLEAF_SIDE_SHORTFALL");
    }
    return result;
}
}
