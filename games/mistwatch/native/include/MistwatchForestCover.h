#pragma once
#include "MistwatchWorld.h"
#include "MistwatchScannedGrass.h"
#include "MistwatchCanopyVolumeClearance.h"
#include "MistwatchImmutableVolumeBounds.h"
#include "MistwatchIndexedVolumeBounds.h"
#include <array>
#include <span>
#include <limits>

namespace Mistwatch {
struct ForestCoverAsset {
    float radius{},height{};
    float minimumY{},maximumY{};
};
// 植被規劃使用真正的水平圓形間距，不把灌木的空角落方盒當成玩家碰撞。
enum class ForestCoverReservationKind {Shrub,WindMixedRoadside};
struct ForestCoverReservation {Aevrynd::Vec3 centre;float radius{};ForestCoverReservationKind kind{ForestCoverReservationKind::Shrub};};
inline void ValidateForestCoverReservations(std::span<const ForestCoverReservation> reservations){
    if(reservations.size()>512)throw std::invalid_argument("FOREST_COVER_RESERVATION_BUDGET");
    for(const auto& p:reservations)if(!Session::Finite(p.centre)||!std::isfinite(p.radius)||p.radius<=0||p.radius>8||
        (p.kind!=ForestCoverReservationKind::Shrub&&p.kind!=ForestCoverReservationKind::WindMixedRoadside))
        throw std::invalid_argument("FOREST_COVER_RESERVATION_INVALID");
}
inline bool ForestCoverReservationClearance(Aevrynd::Vec3 root,float radius,std::span<const ForestCoverReservation> reservations){
    if(!Session::Finite(root)||!std::isfinite(radius)||radius<=0)return false;
    for(const auto& p:reservations){
        if(!Session::Finite(p.centre)||!std::isfinite(p.radius)||p.radius<=0||p.radius>8||
            (p.kind!=ForestCoverReservationKind::Shrub&&p.kind!=ForestCoverReservationKind::WindMixedRoadside))return false;
        // 路肩沿用原CreateRoadsideBroadleaf的風包絡混合間距，灌木仍用完整半徑+.03。
        const float separation=p.kind==ForestCoverReservationKind::Shrub?radius+p.radius+.03f:.65f*(radius+p.radius+.19f);
        if(std::hypot(root.x-p.centre.x,root.z-p.centre.z)<separation)return false;
    }
    return true;
}
struct ForestCoverPlant {
    Aevrynd::Transform3D transform;
    unsigned variant{},patch{};
    float radius{};
};
struct ForestCoverPatch {
    Aevrynd::Vec3 centre;
    float radius{},aspect{},yaw{};
    unsigned requested{},accepted{};
};
struct ForestCoverSettings {
    // 下列坡度、高度差及資產尺寸是 Mistwatch 作者門檻，不是引擎通用安全保證。
    std::uint32_t seed{0x434f5645u};
    std::size_t targetPlants{3600};
    unsigned maximumPatches{400},attemptsPerPatch{240};
    unsigned independentAnchors{900},minimumPlantsPerPatch{20},maximumPlantsPerPatch{32};
    bool prioritizeRouteShoulders{true};
    float minimumNormalY{.88f},maximumFootprintHeightDelta{.24f};
    // 允許葉緣相交，但不讓草根密集堆疊；所有數值只影響裝飾草，不影響碰撞。
    float rootSeparationRatio{.65f};
};
struct ForestCoverResult {
    std::vector<ForestCoverPlant> plants;
    std::vector<ForestCoverPatch> patches;
    unsigned rejectedClearance{},rejectedTerrain{},rejectedSpacing{};
    bool reachedBudget{};
};
inline ForestCoverAsset ForestCoverAssetFromMesh(const Aevrynd::Mesh3D& normalized) {
    CanopyVolume::PlantLocalVolume volume;
    if(!normalized.Valid()||!CanopyVolume::MeasureLocalVolume(std::span<const Aevrynd::Vertex3D>(normalized.vertices),volume))
        throw std::invalid_argument("Invalid normalized grass full volume");
    ForestCoverAsset result;
    for(const auto& vertex:normalized.vertices) {
        result.radius=(std::max)(result.radius,std::hypot(vertex.x,vertex.z));
        result.height=(std::max)(result.height,vertex.y);
    }
    // 真最低／最高點另存；原radius/height計算與作者語意維持不變。
    result.minimumY=volume.minimumY;result.maximumY=volume.maximumY;
    return result;
}
inline float ForestCoverScale(Aevrynd::Vec3 root) {
    // 沿用真正草叢的尺度；配置密度不得用放大模型或更改遮罩替代。
    return .5f+.25f*std::sin(root.x*1.7f+root.z);
}
inline bool ForestCoverClearance(Aevrynd::Vec3 root,float radius,
    std::span<const Aevrynd::BoxCollider3D> solids,const EntryLayout& layout=ActiveEntryLayout) {
    if(!Session::Finite(root)||!std::isfinite(radius)||radius<=0)return false;
    // 原草的根部排除區加上完整模型半徑；另保留所有地物／任務通路排除。
    if(std::abs(root.x)<1.8f+radius||SettlementTrailDistance(root.x,root.z)<.7f+radius||
       RiverDistance(root.x,root.z)<RiverOuterWidth(root.x)+1.5f+2.f*radius)return false;
    if(!PropClearsObjectives({root,{radius,1.f,radius}},layout))return false;
    for(const auto p:layout.repairSites)
        if(std::hypot(root.x-p.x,root.z-p.z)<3.f+radius)return false;
    for(const auto p:{Session::fuse,Session::rangerLog})
        if(std::abs(root.x-p.x)<3.f+radius&&std::abs(root.z-p.z)<3.5f+radius)return false;
    for(const auto& box:solids)
        if(std::abs(root.x-box.center.x)<box.extents.x+radius&&
           std::abs(root.z-box.center.z)<box.extents.z+radius)return false;
    return true;
}
inline bool ForestCoverFootprint(const Aevrynd::HeightField3D& terrain,Aevrynd::Vec3 root,
    float radius,const ForestCoverSettings& settings) {
    if(!Session::Finite(root)||!std::isfinite(radius)||radius<=0)return false;
    if(!terrain.Contains(root.x-radius,root.z-radius)||!terrain.Contains(root.x+radius,root.z+radius))return false;
    const auto safe=[&](float x,float z) {
        const float height=terrain.SampleHeight(x,z);
        const auto normal=terrain.SampleNormal(x,z);
        return std::isfinite(height)&&std::isfinite(normal.y)&&normal.y>=settings.minimumNormalY&&
            std::abs(height-root.y)<=settings.maximumFootprintHeightDelta;
    };
    if(!safe(root.x,root.z))return false;
    // 檢查包住整個旋轉模型的方形；三角面高度的極值在格點或邊界交點。
    const auto& grid=terrain.Settings();
    const int x0=int(std::ceil((root.x-radius-grid.origin.x)/grid.cellSize));
    const int x1=int(std::floor((root.x+radius-grid.origin.x)/grid.cellSize));
    const int z0=int(std::ceil((root.z-radius-grid.origin.z)/grid.cellSize));
    const int z1=int(std::floor((root.z+radius-grid.origin.z)/grid.cellSize));
    const float minX=root.x-radius,maxX=root.x+radius,minZ=root.z-radius,maxZ=root.z+radius;
    for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x)
        if(!safe(grid.origin.x+float(x)*grid.cellSize,grid.origin.z+float(z)*grid.cellSize))return false;
    for(int x=x0;x<=x1;++x)
        if(!safe(grid.origin.x+float(x)*grid.cellSize,minZ)||!safe(grid.origin.x+float(x)*grid.cellSize,maxZ))return false;
    for(int z=z0;z<=z1;++z)
        if(!safe(minX,grid.origin.z+float(z)*grid.cellSize)||!safe(maxX,grid.origin.z+float(z)*grid.cellSize))return false;
    const float originSum=grid.origin.x+grid.origin.z;
    const int sum0=int(std::ceil((minX+minZ-originSum)/grid.cellSize));
    const int sum1=int(std::floor((maxX+maxZ-originSum)/grid.cellSize));
    for(int sum=sum0;sum<=sum1;++sum) {
        const float diagonal=originSum+float(sum)*grid.cellSize;
        for(float x:{minX,maxX})if(diagonal-x>=minZ&&diagonal-x<=maxZ&&!safe(x,diagonal-x))return false;
        for(float z:{minZ,maxZ})if(diagonal-z>=minX&&diagonal-z<=maxX&&!safe(diagonal-z,z))return false;
    }
    // 完整覆蓋相交格子的兩個真三角面坡度；不以平滑採样法線藏起陡面。
    const int cellX0=int(std::floor((minX-grid.origin.x)/grid.cellSize));
    const int cellX1=(std::min)(int(grid.width)-2,int(std::floor((maxX-grid.origin.x)/grid.cellSize)));
    const int cellZ0=int(std::floor((minZ-grid.origin.z)/grid.cellSize));
    const int cellZ1=(std::min)(int(grid.depth)-2,int(std::floor((maxZ-grid.origin.z)/grid.cellSize)));
    for(int z=cellZ0;z<=cellZ1;++z)for(int x=cellX0;x<=cellX1;++x) {
        const float a=terrain.Height(unsigned(x),unsigned(z)),b=terrain.Height(unsigned(x+1),unsigned(z));
        const float c=terrain.Height(unsigned(x),unsigned(z+1)),d=terrain.Height(unsigned(x+1),unsigned(z+1));
        const auto first=Aevrynd::Normalize({a-b,grid.cellSize,a-c});
        const auto second=Aevrynd::Normalize({c-d,grid.cellSize,b-d});
        if(!std::isfinite(first.y)||!std::isfinite(second.y)||first.y<settings.minimumNormalY||second.y<settings.minimumNormalY)return false;
    }
    for(unsigned step=0;step<=16;++step) {
        const float offset=-radius+2.f*radius*float(step)/16.f;
        if(!safe(root.x-radius,root.z+offset)||!safe(root.x+radius,root.z+offset)||
           !safe(root.x+offset,root.z-radius)||!safe(root.x+offset,root.z+radius))return false;
    }
    return true;
}
inline ForestCoverResult CreateForestCover(const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::Vec3> trees,const ForestUnderstory& understory,
    std::span<const Aevrynd::BoxCollider3D> solids,const std::array<ForestCoverAsset,5>& assets,
    ForestCoverSettings settings={},std::span<const Aevrynd::BoxCollider3D> canopyWood={},bool fullSolidVolumes=false,
    const EntryLayout& layout=ActiveEntryLayout,std::span<const ForestCoverReservation> reservations={}) {
    ValidateForestCoverReservations(reservations);
    if(!CanopyVolume::ValidWoodParts(canopyWood))throw std::invalid_argument("Invalid canopy wood parts for grass");
    if(settings.targetPlants>10000||!settings.maximumPatches||settings.maximumPatches>1000||
       !settings.attemptsPerPatch||settings.attemptsPerPatch>10000||
       settings.independentAnchors>2000||!settings.minimumPlantsPerPatch||
       settings.maximumPlantsPerPatch<settings.minimumPlantsPerPatch||settings.maximumPlantsPerPatch>128||
       !std::isfinite(settings.minimumNormalY)||settings.minimumNormalY<.88f||settings.minimumNormalY>1.f||
       !std::isfinite(settings.maximumFootprintHeightDelta)||settings.maximumFootprintHeightDelta<=0||
       settings.maximumFootprintHeightDelta>.24f||!std::isfinite(settings.rootSeparationRatio)||
       settings.rootSeparationRatio<.65f||settings.rootSeparationRatio>2.f)
        throw std::invalid_argument("Invalid forest cover settings");
    for(const auto& asset:assets)if(!std::isfinite(asset.radius)||asset.radius<GrassMinimumRadius-.0001f||
       asset.radius>4.f||!std::isfinite(asset.height)||asset.height<GrassAssetHeight-.0001f)
        throw std::invalid_argument("Invalid normalized grass asset footprint");
    if(!canopyWood.empty())for(const auto& asset:assets)
        if(!CanopyVolume::Detail::ValidLocal(CanopyVolume::ConservativeLocalVolume(asset.radius,asset.minimumY,asset.maximumY)))
            throw std::invalid_argument("Invalid normalized grass full volume");
    CanopyVolume::IndexedVolumeBounds solidVolumeBounds(solids),canopyVolumeBounds(canopyWood);
    ForestCoverResult result;
    result.plants.reserve(settings.targetPlants);
    std::uint32_t seed=settings.seed;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    std::vector<Aevrynd::Vec3> anchors;
    for(const auto& fern:understory.ferns)anchors.push_back(fern.position);
    for(const auto& tree:understory.saplings)anchors.push_back(tree.position);
    for(const auto tree:trees)for(float sign:{-1.f,1.f})anchors.push_back(tree+Aevrynd::Vec3{sign*2.2f,0,sign*.8f});
    // 起始林段在世界座標分層配置路肩，包含玩家後方；不以相機距離產生植株。
    // 每層加入固定種子的微擾，避免格狀排列；完整腳印仍須通過原有排除與坡度契約。
    for(float z=-8.f;z<=20.f;z+=4.f)for(float x:{6.f,9.f,12.f})for(float sign:{-1.f,1.f})
        anchors.push_back({sign*x+(random()-.5f)*1.4f,0,z+(random()-.5f)*1.4f});
    for(unsigned index=0;index<settings.independentAnchors;++index)
        anchors.push_back({-32.f+64.f*random(),0,-10.f+98.f*random()});
    for(std::size_t index=anchors.size();index>1;--index) {
        const auto other=(std::min)(std::size_t(random()*float(index)),index-1);
        std::swap(anchors[index-1],anchors[other]);
    }
    // 先配置可見的路肩與乾岸林下，再填遠處；同級保留固定種子的順序。
    // 這是世界座標的棲地權重，不依目前鏡頭改變或重生植株。
    if(settings.prioritizeRouteShoulders) {
        const auto priority=[](Aevrynd::Vec3 p) {
            if(p.z>=-10.f&&p.z<=20.f&&std::abs(p.x)>=4.5f&&std::abs(p.x)<=12.f)return 3;
            if(p.z>=0.f&&p.z<=26.f&&std::abs(p.x)>=4.5f&&std::abs(p.x)<=16.f)return 2;
            if(p.x<=-8.f&&p.x>=-24.f&&p.z>=20.f&&p.z<=52.f)return 1;
            return 0;
        };
        std::stable_sort(anchors.begin(),anchors.end(),[&](Aevrynd::Vec3 a,Aevrynd::Vec3 b){return priority(a)>priority(b);});
    }
    for(auto centre:anchors) {
        if(result.plants.size()>=settings.targetPlants||result.patches.size()>=settings.maximumPatches)break;
        centre.y=terrain.SampleHeight(centre.x,centre.z);
        if(!ForestCoverClearance(centre,.5f,solids,layout)||
            (!reservations.empty()&&!ForestCoverReservationClearance(centre,.5f,reservations)))continue;
        bool overlaps=false;
        for(const auto& patch:result.patches)if(std::hypot(centre.x-patch.centre.x,centre.z-patch.centre.z)<3.2f){overlaps=true;break;}
        if(overlaps)continue;
        ForestCoverPatch patch{centre,1.5f+1.3f*random(),.6f+.4f*random(),random()*6.2831853f,
            settings.minimumPlantsPerPatch+unsigned(random()*float(settings.maximumPlantsPerPatch-settings.minimumPlantsPerPatch+1)),0};
        const auto patchIndex=unsigned(result.patches.size());
        const float cosine=std::cos(patch.yaw),sine=std::sin(patch.yaw);
        for(unsigned attempt=0;attempt<settings.attemptsPerPatch&&patch.accepted<patch.requested&&
            result.plants.size()<settings.targetPlants;++attempt) {
            const float distance=patch.radius*std::sqrt(random()),angle=random()*6.2831853f;
            const float dx=distance*std::cos(angle),dz=distance*std::sin(angle)*patch.aspect;
            Aevrynd::Vec3 root{centre.x+dx*cosine-dz*sine,0,centre.z+dx*sine+dz*cosine};
            if(root.x< -32.f||root.x>32.f||root.z< -10.f||root.z>88.f)continue;
            root.y=terrain.SampleHeight(root.x,root.z);
            const float scale=ForestCoverScale(root);
            const unsigned variant=(std::min)(unsigned(random()*5.f),4u);
            const float radius=assets[variant].radius*scale;
            if(!ForestCoverClearance(root,radius,solids,layout)||
                (!reservations.empty()&&!ForestCoverReservationClearance(root,radius,reservations))){++result.rejectedClearance;continue;}
            // 開始完整實體體積檢查
if(fullSolidVolumes&&!solids.empty()&&!solidVolumeBounds.Clears(
                CanopyVolume::PlantVolume{root,CanopyVolume::ConservativeLocalVolume(assets[variant].radius,
                    assets[variant].minimumY,assets[variant].maximumY),scale,{0,root.x+root.z,0}})){
                ++result.rejectedClearance;continue;
            }
// 結束完整實體體積檢查
if(!canopyWood.empty()&&!canopyVolumeBounds.Clears(
                CanopyVolume::PlantVolume{root,CanopyVolume::ConservativeLocalVolume(assets[variant].radius,
                    assets[variant].minimumY,assets[variant].maximumY),scale,{0,root.x+root.z,0}})){
                ++result.rejectedClearance;continue;
            }
            if(!ForestCoverFootprint(terrain,root,radius,settings)){++result.rejectedTerrain;continue;}
            bool crowded=false;
            for(const auto& plant:result.plants)
                if(std::hypot(root.x-plant.transform.position.x,root.z-plant.transform.position.z)<
                   settings.rootSeparationRatio*(radius+plant.radius)){crowded=true;break;}
            if(crowded){++result.rejectedSpacing;continue;}
            result.plants.push_back({{root,{0,root.x+root.z,0},{scale,scale,scale}},variant,patchIndex,radius});
            ++patch.accepted;
        }
        if(patch.accepted)result.patches.push_back(patch);
    }
    result.reachedBudget=result.plants.size()==settings.targetPlants;
    return result;
}
}
