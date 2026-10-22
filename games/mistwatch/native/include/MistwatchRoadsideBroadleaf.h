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
    const WoodlandBridgeWorld::ValidatedWorld* groupedWorld=nullptr,RoadsideInputPhase phase=RoadsideInputPhase::FinalWorld) {
    ValidateRoadsideCoverInputs(original,understory,colonies,shrubs,grassAssets,fernAssets,before,after,composition,groupedWorld,phase);
    for(const auto& a:assets)if(!CanopyVolume::Detail::ValidLocal({a.radius,a.minimumY,a.maximumY})||
        a.radius>4||a.maximumY<=0||a.maximumY-a.minimumY<=0)
        throw std::invalid_argument("ROADSIDE_BROADLEAF_ACTUAL_VOLUME_REQUIRED");
    RoadsideCoverResult result;std::uint32_t seed=0x52424c31u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    // 世界固定的小群落，不沿路排成等距護欄。左右各有有限嘗試與相同品質門。
    for(float side:{-1.f,1.f}){
        std::array<float,16> plannedX{};
        if(phase==RoadsideInputPhase::PlanBeforeSoftLayers){
            float maximumRadius{};
            for(unsigned i=0;i<assets.size();++i){const auto& a=assets[i];const float maximumHeight=i==0?.35f:.16f;
                maximumRadius=(std::max)(maximumRadius,a.radius*maximumHeight/(a.maximumY-a.minimumY)+.19f);}
            CanopyVolume::ImmutableVolumeBoundsCache beforeBounds(before),afterBounds(after);
            // 新谷地以實際地形與完整碰撞找路肩到林緣的落點；不增加1024嘗試或減少48株。
            // 四個錨點取可用橫向區間的四分位，不在失效平地patch上反覆抽樣。
            for(unsigned row=0;row<4;++row){const float z=-3.7f+8.1f*float(row);std::vector<float> available;
                for(unsigned column=0;column<23;++column){const float x=3.f+.5f*float(column);
                    Aevrynd::Vec3 root{side*x,terrain.SampleHeight(side*x,z),z};
                    if(!RoadsideCoverRouteClearance(root,maximumRadius)||!ForestCoverFootprint(terrain,root,maximumRadius,{}))continue;
                    const CanopyVolume::PlantVolume volume{root,CanopyVolume::ConservativeLocalVolume(maximumRadius,0,.361f),1,{}};
                    if(!(groupedWorld?groupedWorld->Clears(volume,false):beforeBounds.Clears(volume))||
                        !(groupedWorld?groupedWorld->Clears(volume,true):afterBounds.Clears(volume)))continue;
                    bool occupied=false;
                    for(unsigned i=0;i<understory.ferns.size();++i){const auto& p=understory.ferns[i];
                        occupied|=std::hypot(root.x-p.position.x,root.z-p.position.z)<.65f*(maximumRadius+fernAssets[i%4].radius*p.scale.x+.19f);}
                    for(const auto& p:shrubs.plants)occupied|=std::hypot(root.x-p.pose.position.x,root.z-p.pose.position.z)<.65f*(maximumRadius+p.radius+.19f);
                    if(!occupied)available.push_back(x);
                }
                for(unsigned band=0;band<4;++band){const unsigned patch=row*4+band;
                    plannedX[patch]=available.empty()?3.15f+.72f*float(band):available[(std::min)(available.size()-1,((2*band+1)*available.size())/8)];}
            }
        }
        unsigned accepted{};
        for(unsigned attempt=0;attempt<1024&&accepted<RoadsideBroadleafPerSide;++attempt){
            ++result.attempted;
            const unsigned patch=attempt%16;
            const float centreX=phase==RoadsideInputPhase::PlanBeforeSoftLayers?plannedX[patch]:3.15f+.72f*float(patch%4);
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
        if(accepted!=RoadsideBroadleafPerSide)throw std::runtime_error("ROADSIDE_BROADLEAF_SIDE_SHORTFALL side="+std::to_string(int(side))+" accepted="+
            std::to_string(accepted)+" route="+std::to_string(result.rejectedRoute)+" world="+std::to_string(result.rejectedWorld)+
            " terrain="+std::to_string(result.rejectedTerrain)+" vegetation="+std::to_string(result.rejectedVegetation));
    }
    return result;
}
}
