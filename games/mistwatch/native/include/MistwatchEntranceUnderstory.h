#pragma once
#include "MistwatchRoadsideBroadleaf.h"

namespace Mistwatch {
inline constexpr std::string_view EntranceUnderstoryProfile="entrance-understory-v2";
inline constexpr std::array<Aevrynd::Vec3,6> EntranceUnderstoryCentres{{
    {-4.6f,0,-3.5f},{4.6f,0,0.f},{-4.6f,0,5.5f},{4.6f,0,14.f},{-8.85f,0,20.f},{6.f,0,26.5f}}};
// 群落沿既有道路延伸；入口三株真樹幹佔據原小區域，不能以刪木或縮半徑湊數。
// 最後右群受補給支線與河岸夾限，沿坡腳橫向展開，不跨入河道。
// 工具棚左群涵蓋內側路肩到外側林緣；兩個修復互動區仍以原四米完整排除。
inline constexpr std::array<Aevrynd::Vec3,6> EntranceUnderstoryHalfExtents{{
    {1.9f,0,4.5f},{1.9f,0,4.f},{1.9f,0,4.5f},{1.9f,0,4.f},{6.15f,0,4.f},{3.f,0,1.5f}}};
struct EntranceUnderstoryResult {
    std::vector<ShrubPlant> plants;
    std::vector<unsigned> clusterIds;
    std::vector<unsigned> suppressedGrassIds,suppressedBroadleafIds;
    std::array<unsigned,6> clusterCounts{};
    unsigned attempted{},rejectedRoute{},rejectedWorld{},rejectedTerrain{},rejectedVegetation{},left{},right{};
    std::uint64_t fingerprint{14695981039346656037ull};
    std::uint64_t suppressionFingerprint{14695981039346656037ull};
};
enum class EntranceUnderstoryStage : unsigned { Route,World,Terrain,Vegetation,Accepted };
struct EntranceUnderstoryAttempt {
    unsigned cluster{},ordinal{},variant{};
    Aevrynd::Vec3 root;
    float radius{},height{};
    EntranceUnderstoryStage stage{};
    unsigned vegetationMask{};
};
struct EntranceUnderstoryTrace {
    // 最多六群各2048筆；只旁記，不改候選亂數、尺寸或接受條件。
    std::vector<EntranceUnderstoryAttempt> attempts;
};
inline EntranceUnderstoryResult CreateEntranceUnderstory(const Aevrynd::HeightField3D& terrain,
    const ForestCoverResult& original,const ForestUnderstory& understory,const FernColonyResult& colonies,
    const ShrubCanopyResult& shrubs,const std::array<ForestCoverAsset,5>& grassAssets,
    const std::array<FernColonyAsset,4>& fernAssets,const RoadsideCoverResult& roadsideBroadleaf,
    std::span<const Aevrynd::BoxCollider3D> before,std::span<const Aevrynd::BoxCollider3D> after,
    EntranceUnderstoryTrace* trace=nullptr,const ShrubCompositionContract& composition={},
    const WoodlandBridgeWorld::ValidatedWorld* groupedWorld=nullptr) {
    if(groupedWorld)groupedWorld->RequireValid();
    if(trace&&!trace->attempts.empty())throw std::invalid_argument("ENTRANCE_UNDERSTORY_TRACE_NOT_EMPTY");
    ValidateRoadsideCoverInputs(original,understory,colonies,shrubs,grassAssets,fernAssets,before,after,composition,groupedWorld);
    if(roadsideBroadleaf.plants.size()!=96||roadsideBroadleaf.left!=48||roadsideBroadleaf.right!=48)
        throw std::invalid_argument("ENTRANCE_UNDERSTORY_COMPLETE_BROADLEAF_REQUIRED");
    for(const auto& p:roadsideBroadleaf.plants)if(!Session::Finite(p.transform.position)||
        !Session::Finite(p.transform.rotation)||!Session::Finite(p.transform.scale)||p.transform.scale.x<=0||
        p.transform.scale.x!=p.transform.scale.y||p.transform.scale.x!=p.transform.scale.z||
        !std::isfinite(p.radius)||p.radius<=0||p.radius>4||p.variant>=2)
        throw std::invalid_argument("ENTRANCE_UNDERSTORY_INVALID_BROADLEAF");
    for(const auto& a:shrubs.variants){
        CanopyVolume::PlantLocalVolume volume;float radius{};
        if(!a.stems.Valid()||!a.leaves.Valid()||!CanopyVolume::MeasureLocalVolume(
            std::span<const Aevrynd::Vertex3D>(a.stems.vertices),std::span<const Aevrynd::Vertex3D>(a.leaves.vertices),volume))
            throw std::invalid_argument("ENTRANCE_UNDERSTORY_ACTUAL_GEOMETRY_REQUIRED");
        for(const auto* mesh:{&a.stems,&a.leaves})for(const auto& v:mesh->vertices)
            radius=(std::max)(radius,std::hypot(v.x,v.z));
        if(radius!=a.radius||volume.minimumY!=a.minimumY||volume.maximumY!=a.maximumY||
            radius<=0||radius>.75f||a.maximumY<=a.minimumY||a.leafCount!=504||a.stemCount!=7)
            throw std::invalid_argument("ENTRANCE_UNDERSTORY_ACTUAL_GEOMETRY_REQUIRED");
    }
    EntranceUnderstoryResult result;std::uint32_t seed=0x45554e31u;std::string firstFailure;
    if(trace)trace->attempts.reserve(6*2048);
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    // 新層級配置保留原作者陣列；新灌木優先，實際衝突軟草必須由renderer依精確ID停繪。
    // 木體、蕨、舊／新灌木仍用原完整間距；不是保留穿插實例再降低距離門檻。
    // 六個世界群落各四株，不靠鏡頭，也不把不足的配額改成成功。
    for(unsigned cluster=0;cluster<EntranceUnderstoryCentres.size();++cluster){
        const auto centre=EntranceUnderstoryCentres[cluster];const auto half=EntranceUnderstoryHalfExtents[cluster];
        for(unsigned attempt=0;attempt<2048&&result.clusterCounts[cluster]<4;++attempt){
            ++result.attempted;const unsigned variant=(std::min)(3u,unsigned(random()*4.f));
            const auto& a=shrubs.variants[variant];const float height=.65f+.35f*random();
            const float scale=height/(a.maximumY-a.minimumY);
            Aevrynd::Vec3 root{centre.x+(random()-.5f)*2.f*half.x,0,centre.z+(random()-.5f)*2.f*half.z};
            root.y=terrain.SampleHeight(root.x,root.z);
            const float radius=a.radius*scale+.19f;
            const Aevrynd::Transform3D pose{{root.x,root.y-a.minimumY*scale,root.z},
                {0,random()*6.2831853f,0},{scale,scale,scale}};
            const auto record=[&](EntranceUnderstoryStage stage,unsigned mask=0){
                if(trace){if(trace->attempts.size()>=6*2048)throw std::runtime_error("ENTRANCE_UNDERSTORY_TRACE_LIMIT");
                    trace->attempts.push_back({cluster,attempt,variant,root,radius,height,stage,mask});}};
            if(!RoadsideCoverRouteClearance(root,radius)){++result.rejectedRoute;record(EntranceUnderstoryStage::Route);continue;}
            const CanopyVolume::PlantVolume volume{pose.position,
                CanopyVolume::ConservativeLocalVolume(a.radius+.19f/scale,a.minimumY,a.maximumY+.011f/scale),scale,pose.rotation};
            const bool clearsBefore=groupedWorld?groupedWorld->Clears(volume,false):CanopyVolume::ClearsCanopyWood(volume,before);
            const bool clearsAfter=groupedWorld?groupedWorld->Clears(volume,true):CanopyVolume::ClearsCanopyWood(volume,after);
            if(!clearsBefore||!clearsAfter){
                ++result.rejectedWorld;record(EntranceUnderstoryStage::World);continue;}
            if(!ForestCoverFootprint(terrain,root,radius,{})){++result.rejectedTerrain;record(EntranceUnderstoryStage::Terrain);continue;}
            bool overlap=false;unsigned mask{};
            const auto tooClose=[&](Aevrynd::Vec3 p,float minimum){return std::hypot(root.x-p.x,root.z-p.z)<minimum;};
            // 軟草沿用同一距離判斷記錄讓位；不把軟草衝突當成硬體安全通過。
            for(const auto& p:original.plants)if(tooClose(p.transform.position,(std::max)(.22f,.65f*(radius+p.radius+.19f)))){mask|=1;break;}
            for(const auto& p:roadsideBroadleaf.plants)
                if(tooClose(p.transform.position,(std::max)(.22f,.65f*(radius+p.radius)))){mask|=2;break;}
            if(trace||!overlap)for(unsigned i=0;i<understory.ferns.size();++i)
                if(tooClose(understory.ferns[i].position,radius+fernAssets[i%4].radius*understory.ferns[i].scale.x+.19f+.03f)){overlap=true;mask|=4;break;}
            if(trace||!overlap)for(const auto& p:colonies.plants)if(tooClose(p.transform.position,radius+p.radius+.19f+.03f)){overlap=true;mask|=8;break;}
            if(trace||!overlap)for(const auto& p:shrubs.plants)if(tooClose(p.pose.position,radius+p.radius+.19f+.03f)){overlap=true;mask|=16;break;}
            if(trace||!overlap)for(const auto& p:result.plants)if(tooClose(p.pose.position,radius+p.radius+.03f)){overlap=true;mask|=32;break;}
            if(overlap){++result.rejectedVegetation;record(EntranceUnderstoryStage::Vegetation,mask);continue;}
            record(EntranceUnderstoryStage::Accepted,mask);
            result.plants.push_back({pose,variant,radius});result.clusterIds.push_back(cluster);++result.clusterCounts[cluster];
            if(root.x<0)++result.left;else ++result.right;
            const auto mix=[&](std::uint32_t word){result.fingerprint^=word;result.fingerprint*=1099511628211ull;};
            for(const float v:{pose.position.x,pose.position.y,pose.position.z,pose.rotation.y,scale,radius})mix(std::bit_cast<std::uint32_t>(v));
            mix(variant);mix(cluster);
        }
        if(result.clusterCounts[cluster]!=4){const std::string failure="ENTRANCE_UNDERSTORY_CLUSTER_SHORTFALL_"+
            std::to_string(cluster)+"_actual_"+std::to_string(result.clusterCounts[cluster])+"_route_"+
            std::to_string(result.rejectedRoute)+"_world_"+std::to_string(result.rejectedWorld)+"_terrain_"+
            std::to_string(result.rejectedTerrain)+"_vegetation_"+std::to_string(result.rejectedVegetation);
            if(!trace)throw std::runtime_error(failure);
            if(firstFailure.empty())firstFailure=failure;
        }
    }
    // 診斷可走完後續群落，但仍以第一個不足拋錯；不回傳半成品為成功結果。
    if(!firstFailure.empty())throw std::runtime_error(firstFailure);
    // 依原source順序產生唯一ID；只有真正與新灌木衝突的軟草讓位，未衝突的禁止移除。
    const auto suppressed=[&](const ForestCoverPlant& soft,float wind){
        for(const auto& shrub:result.plants)if(std::hypot(shrub.pose.position.x-soft.transform.position.x,
            shrub.pose.position.z-soft.transform.position.z)<(std::max)(.22f,.65f*(shrub.radius+soft.radius+wind)))return true;
        return false;
    };
    for(unsigned i=0;i<original.plants.size();++i)if(suppressed(original.plants[i],.19f))result.suppressedGrassIds.push_back(i);
    for(unsigned i=0;i<roadsideBroadleaf.plants.size();++i)if(suppressed(roadsideBroadleaf.plants[i],0.f))result.suppressedBroadleafIds.push_back(i);
    if(result.suppressedGrassIds.size()>180||result.suppressedBroadleafIds.size()>48)
        throw std::runtime_error("ENTRANCE_UNDERSTORY_SUPPRESSION_BUDGET_grass_"+
            std::to_string(result.suppressedGrassIds.size())+"_broadleaf_"+std::to_string(result.suppressedBroadleafIds.size()));
    const auto mixSuppression=[&](std::uint32_t word){
        result.suppressionFingerprint^=word;result.suppressionFingerprint*=1099511628211ull;
        result.fingerprint^=word;result.fingerprint*=1099511628211ull;
    };
    mixSuppression(0x47524153u);mixSuppression(static_cast<std::uint32_t>(result.suppressedGrassIds.size()));
    for(const auto id:result.suppressedGrassIds)mixSuppression(id);
    mixSuppression(0x42524f41u);mixSuppression(static_cast<std::uint32_t>(result.suppressedBroadleafIds.size()));
    for(const auto id:result.suppressedBroadleafIds)mixSuppression(id);
    return result;
}
}
