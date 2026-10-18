#pragma once
#include "MistwatchForestCoverRetention.h"
#include "MistwatchShrubCanopy.h"
#include "MistwatchShrubComposition.h"
#include "MistwatchWoodlandBridgeWorld.h"

namespace Mistwatch {
inline constexpr std::string_view RoadsideCoverProfile="roadside-ground-cover-v1";
struct RoadsideCoverResult {
    std::vector<ForestCoverPlant> plants;
    unsigned attempted{},rejectedRoute{},rejectedWorld{},rejectedTerrain{},rejectedVegetation{};
    unsigned left{},right{};
    std::uint64_t fingerprint{1469598103934665603ull};
};

// 低草不是碰撞地物：保留完整可見道路及橫向任務支線，不套用大樹／石頭的 9m 樹冠禁放帶。
// 原 ForestCoverClearance 和 PropClearsObjectives 完全保留；此規則只用於新增且有獨立身份的路肩低草。
inline bool RoadsideCoverRouteClearance(Aevrynd::Vec3 root,float radius) {
    if(!Session::Finite(root)||!std::isfinite(radius)||radius<=0||radius>4)return false;
    const float minX=root.x-radius,maxX=root.x+radius,minZ=root.z-radius,maxZ=root.z+radius;
    if(minX<1.8f&&maxX> -1.8f)return false;
    // 連續彎路的中心／半寬導數總界 .0665+.0465；完整風包絡的 z 區間也不能侵路。
    const float centre=.35f*std::sin(root.z*.19f),width=2.10f+.15f*std::sin(root.z*.31f);
    if(std::abs(root.x-centre)<width+1.113f*radius+.08f)return false;
    if(SettlementTrailDistance(root.x,root.z)<.7f+radius)return false;
    if(minZ<43.f&&maxZ>29.f)return false;
    if(RiverDistance(root.x,root.z)<RiverOuterWidth(root.x)+1.5f+2.f*radius)return false;
    for(unsigned i=0;i<5;++i){const float x=minX+(maxX-minX)*float(i)*.25f;
        const float z=RiverCenterZ(x),margin=RiverOuterWidth(x)+2.f;
        if(minZ<z+margin&&maxZ>z-margin)return false;}
    const auto clears=[&](Aevrynd::Vec3 p,float margin){
        return std::hypot((std::max)(0.f,std::abs(p.x-root.x)-radius),
            (std::max)(0.f,std::abs(p.z-root.z)-radius))>=margin;
    };
    for(const auto p:Session::supplies){
        if(!clears(p,3.f))return false;
        if(minZ<p.z+2.f&&maxZ>p.z-2.f&&minX<(std::max)(0.f,p.x)+2.f&&maxX>(std::min)(0.f,p.x)-2.f)return false;
    }
    for(const auto p:Session::repairSites){
        if(!clears(p,4.f))return false;
        if(minZ<p.z+1.5f&&maxZ>p.z-1.5f&&minX<(std::max)(0.f,p.x)+1.f&&maxX>(std::min)(0.f,p.x)-1.f)return false;
    }
    for(const auto p:Session::relaySites){
        if(!clears(p,4.f))return false;
        if(minZ<p.z+1.8f&&maxZ>p.z-1.8f&&minX<(std::max)(0.f,p.x)+1.f&&maxX>(std::min)(0.f,p.x)-1.f)return false;
    }
    return clears(Session::arena,7.f)&&clears(Session::fuse,5.f)&&clears(Session::generator,5.f)&&
        clears(Session::beacon,5.f)&&clears(Session::rangerLog,5.f);
}

inline void ValidateRoadsideCoverInputs(
    const ForestCoverResult& original,const ForestUnderstory& understory,const FernColonyResult& colonies,
    const ShrubCanopyResult& shrubs,const std::array<ForestCoverAsset,5>& grassAssets,
    const std::array<FernColonyAsset,4>& fernAssets,
    std::span<const Aevrynd::BoxCollider3D> before,std::span<const Aevrynd::BoxCollider3D> after,const ShrubCompositionContract& composition={},
    const WoodlandBridgeWorld::ValidatedWorld* groupedWorld=nullptr) {
    const auto need=[](bool ok,const char* why){if(!ok)throw std::invalid_argument(why);};
    if(groupedWorld)groupedWorld->RequireValid();
    if(groupedWorld)need(before.empty()&&after.empty(),"ROADSIDE_MIXED_WORLD_INPUT");
    need(original.reachedBudget&&original.plants.size()==3600&&understory.ferns.size()==420&&
        colonies.reachedBudget&&colonies.plants.size()==300&&shrubs.reachedBudget&&shrubs.plants.size()==composition.ExpectedCount(),
        "ROADSIDE_COMPLETE_EXISTING_VEGETATION_REQUIRED");
    if(!groupedWorld)need(before.size()==8853&&after.size()==8854&&CanopyVolume::ValidWoodParts(before)&&
        CanopyVolume::ValidWoodParts(after),"ROADSIDE_COMPLETE_TWO_STATE_WORLD_REQUIRED");
    for(const auto& a:grassAssets)need(std::isfinite(a.radius)&&a.radius>=GrassMinimumRadius-.0001f&&
        a.radius<=4&&std::isfinite(a.height)&&a.height>=GrassAssetHeight-.0001f&&
        CanopyVolume::Detail::ValidLocal(CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.maximumY)),
        "ROADSIDE_ACTUAL_GRASS_VOLUME_REQUIRED");
    for(const auto& a:fernAssets)need(std::isfinite(a.radius)&&a.radius>0&&a.radius<=4,"ROADSIDE_ACTUAL_FERN_VOLUME_REQUIRED");
    const auto validPose=[](const Aevrynd::Transform3D& p){return Session::Finite(p.position)&&
        Session::Finite(p.rotation)&&Session::Finite(p.scale)&&p.scale.x>0&&p.scale.x==p.scale.y&&p.scale.x==p.scale.z;};
    const auto validRadius=[](float r){return std::isfinite(r)&&r>0&&r<=4;};
    for(const auto& p:original.plants)need(validPose(p.transform)&&validRadius(p.radius)&&p.variant<5,
        "ROADSIDE_EXISTING_GRASS_INVALID");
    for(const auto& p:understory.ferns)need(validPose(p),"ROADSIDE_EXISTING_FERN_INVALID");
    for(const auto& p:colonies.plants)need(validPose(p.transform)&&validRadius(p.radius)&&p.variant<4,
        "ROADSIDE_EXISTING_COLONY_INVALID");
    for(const auto& p:shrubs.plants)need(validPose(p.pose)&&validRadius(p.radius)&&p.variant<4,
        "ROADSIDE_EXISTING_SHRUB_INVALID");
    composition.Validate(shrubs);
}

inline RoadsideCoverResult CreateRoadsideCover(const Aevrynd::HeightField3D& terrain,
    const ForestCoverResult& original,const ForestUnderstory& understory,const FernColonyResult& colonies,
    const ShrubCanopyResult& shrubs,const std::array<ForestCoverAsset,5>& grassAssets,
    const std::array<FernColonyAsset,4>& fernAssets,
    std::span<const Aevrynd::BoxCollider3D> before,std::span<const Aevrynd::BoxCollider3D> after,const ShrubCompositionContract& composition={},
    const WoodlandBridgeWorld::ValidatedWorld* groupedWorld=nullptr) {
    ValidateRoadsideCoverInputs(original,understory,colonies,shrubs,grassAssets,fernAssets,before,after,composition,groupedWorld);
    const auto need=[](bool ok,const char* why){if(!ok)throw std::invalid_argument(why);};
    RoadsideCoverResult result;ForestCoverSettings footprint;
    std::uint32_t seed=0x52534331u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    // 有限、世界固定的雙側路肩；不依相機重生、不放大資產、不修改色彩或 alpha。
    for(unsigned row=0;row<84;++row)for(unsigned column=0;column<8;++column)for(float side:{-1.f,1.f}){
        ++result.attempted;
        Aevrynd::Vec3 root{side*(2.4f+.55f*float(column))+(random()-.5f)*.18f,0,
            -6.f+.4f*float(row)+(random()-.5f)*.18f};
        root.y=terrain.SampleHeight(root.x,root.z);
        const unsigned variant=(std::min)(unsigned(random()*5.f),4u);
        const float scale=ForestCoverScale(root),radius=grassAssets[variant].radius*scale+.19f;
        if(!RoadsideCoverRouteClearance(root,radius)){++result.rejectedRoute;continue;}
        const CanopyVolume::PlantVolume volume{root,CanopyVolume::ConservativeLocalVolume(
            grassAssets[variant].radius+.19f/scale,grassAssets[variant].minimumY,
            grassAssets[variant].maximumY+.011f/scale),scale,{0,root.x+root.z,0}};
        const bool clearsBefore=groupedWorld?groupedWorld->Clears(volume,false):CanopyVolume::ClearsCanopyWood(volume,before);
        const bool clearsAfter=groupedWorld?groupedWorld->Clears(volume,true):CanopyVolume::ClearsCanopyWood(volume,after);
        if(!clearsBefore||!clearsAfter){
            ++result.rejectedWorld;continue;}
        if(!ForestCoverFootprint(terrain,root,radius,footprint)){++result.rejectedTerrain;continue;}
        bool overlap=false;
        const auto close=[&](Aevrynd::Vec3 p,float r){return std::hypot(root.x-p.x,root.z-p.z)<.65f*(radius+r);};
        for(const auto& p:original.plants)if(close(p.transform.position,p.radius+.19f)){overlap=true;break;}
        if(!overlap)for(unsigned i=0;i<understory.ferns.size();++i)
            if(close(understory.ferns[i].position,fernAssets[i%4].radius*understory.ferns[i].scale.x+.19f)){overlap=true;break;}
        if(!overlap)for(const auto& p:colonies.plants)if(close(p.transform.position,p.radius+.19f)){overlap=true;break;}
        if(!overlap)for(const auto& p:shrubs.plants)if(close(p.pose.position,p.radius+.19f)){overlap=true;break;}
        if(!overlap)for(const auto& p:result.plants)if(close(p.transform.position,p.radius)){overlap=true;break;}
        if(overlap){++result.rejectedVegetation;continue;}
        result.plants.push_back({{root,{0,root.x+root.z,0},{scale,scale,scale}},variant,row,radius});
        if(root.x<0)++result.left;else ++result.right;
        ForestCoverRetentionDetail::Hasher hash;hash.value=result.fingerprint;hash.Plant(result.plants.back());result.fingerprint=hash.value;
    }
    need(result.plants.size()>=128&&result.left>=64&&result.right>=64,"ROADSIDE_BOTH_SHOULDERS_SHORTFALL");
    return result;
}
}
