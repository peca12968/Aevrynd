#pragma once
#include "MistwatchForestCover.h"
#include "MistwatchGroupedCanopyVolume.h"
#include "MistwatchFern.h"
#include "MistwatchFernColonyWoodBounds.h"
#include <span>
#include <stdexcept>

namespace Mistwatch {
struct FernColonyAsset {
    float radius{},height{},minimumY{};
    unsigned vertices{},triangles{};
};
inline FernColonyAsset FernColonyAssetFromMesh(const Aevrynd::Mesh3D& normalized) {
    if(normalized.vertices.empty()||normalized.indices.empty()||normalized.indices.size()%3)
        throw std::invalid_argument("Invalid fern topology");
    FernColonyAsset result;result.vertices=unsigned(normalized.vertices.size());
    result.triangles=unsigned(normalized.indices.size()/3);
    for(const auto& vertex:normalized.vertices) {
        if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z)||
           !std::isfinite(vertex.u)||!std::isfinite(vertex.v))throw std::invalid_argument("Nonfinite fern vertex");
        result.radius=(std::max)(result.radius,std::hypot(vertex.x,vertex.z));
        result.height=(std::max)(result.height,vertex.y);
        result.minimumY=(std::min)(result.minimumY,vertex.y);
    }
    for(const auto index:normalized.indices)if(index>=normalized.vertices.size())
        throw std::invalid_argument("Invalid fern index");
    return result;
}
struct FernColonyPlant {
    Aevrynd::Transform3D transform;
    unsigned variant{},colony{},grassPatch{};
    float radius{};
};
struct FernColony {
    Aevrynd::Vec3 centre;
    float radius{};
    unsigned grassPatch{},requested{},accepted{};
};
struct FernColonySettings {
    std::uint32_t seed{0x43464e32u};
    std::size_t targetPlants{300};
    unsigned maximumColonies{256},attemptsPerColony{240};
    float rootSeparationRatio{.65f};
};
struct FernColonyResult {
    std::vector<FernColonyPlant> plants;
    std::vector<FernColony> colonies;
    unsigned rejectedClearance{},rejectedTerrain{},rejectedExistingFern{},rejectedGrass{},rejectedSpacing{};
    bool reachedBudget{};
};
inline bool FernColonyClearsExisting(Aevrynd::Vec3 root,float radius,
    std::span<const Aevrynd::Transform3D> existing,const std::array<FernColonyAsset,4>& assets,float ratio=.65f) {
    if(!Session::Finite(root)||!std::isfinite(radius)||radius<=0||radius>4||
       !std::isfinite(ratio)||ratio<.65f||ratio>2.f||existing.size()>10000)return false;
    for(std::size_t i=0;i<existing.size();++i)
        if(std::hypot(root.x-existing[i].position.x,root.z-existing[i].position.z)<
           ratio*(radius+assets[i%4].radius*existing[i].scale.x))return false;
    return true;
}
inline FernColonyResult CreateFernColonies(const Aevrynd::HeightField3D& terrain,
    const ForestCoverResult& grass,std::span<const Aevrynd::Transform3D> existing,
    std::span<const Aevrynd::BoxCollider3D> solids,const std::array<FernColonyAsset,4>& assets,
    FernColonySettings settings={},std::span<const Aevrynd::BoxCollider3D> canopyWood={},bool fullSolidVolumes=false,
    const GroupedCanopyVolume::ValidatedWood* groupedWood=nullptr) {
    // 群組入口只接受已驗完整世界，不允許混用或以零目標跳過失效身分。
    if(groupedWood&&(!fullSolidVolumes||!canopyWood.empty()||groupedWood->Identity().groupCount==0||
        groupedWood->Identity().groupCount>GroupedCanopyVolume::MaximumGroups||
        groupedWood->Groups().size()!=groupedWood->Identity().groupCount||groupedWood->Identity().totalParts==0||
        groupedWood->Identity().totalParts>GroupedCanopyVolume::MaximumTotalParts))
        throw std::invalid_argument("Invalid grouped fern canopy wood");
    const auto cachedCanopy=FernColonyDetail::ImmutableCanopyWoodBounds::Create(canopyWood);
    // 有限工作量與輸入範圍是作者 API 契約，不是視覺品質或引擎通用安全門檻。
    if(grass.patches.size()>1000||grass.plants.size()>10000||existing.size()>10000||solids.size()>10000||
       settings.targetPlants>1000||!settings.maximumColonies||settings.maximumColonies>1000||
       !settings.attemptsPerColony||settings.attemptsPerColony>10000||
       !std::isfinite(settings.rootSeparationRatio)||settings.rootSeparationRatio<.65f||settings.rootSeparationRatio>2.f)
        throw std::invalid_argument("Invalid fern colony settings");
    for(const auto& asset:assets)if(!std::isfinite(asset.radius)||asset.radius<=0||asset.radius>4||
        !std::isfinite(asset.height)||asset.height<=0||asset.height>4||!std::isfinite(asset.minimumY)||asset.minimumY< -4||asset.minimumY>asset.height||
        !asset.vertices||!asset.triangles)throw std::invalid_argument("Invalid measured fern asset");
    if(groupedWood||!canopyWood.empty())for(const auto& asset:assets)
        if(!CanopyVolume::Detail::ValidLocal(CanopyVolume::ConservativeLocalVolume(asset.radius,asset.minimumY,asset.height)))
            throw std::invalid_argument("Invalid measured fern full volume");
    for(std::size_t i=0;i<existing.size();++i) {
        const auto& fern=existing[i];
        if(!Session::Finite(fern.position)||!Session::Finite(fern.rotation)||!Session::Finite(fern.scale)||
           fern.scale.x<=0||fern.scale.x>4||fern.scale.x!=fern.scale.y||fern.scale.x!=fern.scale.z||
           !terrain.Contains(fern.position.x,fern.position.z)||std::abs(fern.position.y)>10000||
           fern.rotation.x!=0||fern.rotation.z!=0)throw std::invalid_argument("Unsupported existing fern transform");
    }
    for(const auto& solid:solids)if(!Session::Finite(solid.center)||!Session::Finite(solid.extents)||
        solid.extents.x<0||solid.extents.y<0||solid.extents.z<0)throw std::invalid_argument("Invalid world collider");
    for(const auto& patch:grass.patches)if(!Session::Finite(patch.centre)||!std::isfinite(patch.radius)||
        patch.radius<=0||patch.radius>4||!terrain.Contains(patch.centre.x,patch.centre.z)||std::abs(patch.centre.y)>10000)
        throw std::invalid_argument("Invalid grass patch anchor");
    for(const auto& blade:grass.plants)if(!Session::Finite(blade.transform.position)||!Session::Finite(blade.transform.rotation)||
        !Session::Finite(blade.transform.scale)||blade.transform.scale.x<=0||blade.transform.scale.x!=blade.transform.scale.y||
        blade.transform.scale.x!=blade.transform.scale.z||!std::isfinite(blade.radius)||blade.radius<=0||blade.radius>4||
        !terrain.Contains(blade.transform.position.x,blade.transform.position.z)||std::abs(blade.transform.position.y)>10000||
        blade.variant>=5||blade.patch>=grass.patches.size())throw std::invalid_argument("Invalid measured grass footprint");
    FernColonyResult result;result.plants.reserve(settings.targetPlants);
    std::uint32_t seed=settings.seed;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    const ForestCoverSettings footprintSettings{};
    // 新增混合群落作者契約：根距至少 .65 倍雙方真水平半徑之和，允許裝飾葉緣相交。
    // 不宣稱葉片不穿插或人體碰撞；固體與任務仍避讓完整半徑，真地形門檻不變。
    // 不讀相機、玩家位置或可見結果；前綴配置讓不同預算可直接比較。
    for(unsigned patchIndex=0;patchIndex<grass.patches.size();++patchIndex) {
        if(result.plants.size()>=settings.targetPlants||result.colonies.size()>=settings.maximumColonies)break;
        const auto& patch=grass.patches[patchIndex];
        const float direction=random()*6.2831853f,offset=patch.radius*(.9f+.4f*random());
        Aevrynd::Vec3 centre=patch.centre+Aevrynd::Vec3{std::cos(direction)*offset,0,std::sin(direction)*offset};
        centre.y=terrain.SampleHeight(centre.x,centre.z);
        FernColony colony{centre,1.1f+.5f*random(),patchIndex,3u+unsigned(random()*4.f),0};
        const auto colonyIndex=unsigned(result.colonies.size());
        for(unsigned attempt=0;attempt<settings.attemptsPerColony&&colony.accepted<colony.requested&&
            result.plants.size()<settings.targetPlants;++attempt) {
            const float distance=colony.radius*std::sqrt(random()),angle=random()*6.2831853f;
            Aevrynd::Vec3 root=centre+Aevrynd::Vec3{distance*std::cos(angle),0,distance*std::sin(angle)};
            if(root.x< -32.f||root.x>32.f||root.z< -10.f||root.z>88.f){++result.rejectedClearance;continue;}
            root.y=terrain.SampleHeight(root.x,root.z);
            const unsigned variant=(std::min)(unsigned(random()*4.f),3u);
            const float scale=.6f+.18f*random(),radius=assets[variant].radius*scale;
            const float yaw=random()*6.2831853f;
            if(!ForestCoverClearance(root,radius,solids)){++result.rejectedClearance;continue;}
            // 開始完整實體體積檢查
if(fullSolidVolumes&&!solids.empty()&&!CanopyVolume::ClearsCanopyWood(
                CanopyVolume::PlantVolume{root,CanopyVolume::ConservativeLocalVolume(assets[variant].radius,
                    assets[variant].minimumY,assets[variant].height),scale,{0,yaw,0}},solids)){
                ++result.rejectedClearance;continue;
            }
// 結束完整實體體積檢查
if((groupedWood||!canopyWood.empty())&&!(groupedWood?groupedWood->Clears(
                CanopyVolume::PlantVolume{root,CanopyVolume::ConservativeLocalVolume(assets[variant].radius,
                    assets[variant].minimumY,assets[variant].height),scale,{0,yaw,0}}):cachedCanopy.Clears(
                CanopyVolume::PlantVolume{root,CanopyVolume::ConservativeLocalVolume(assets[variant].radius,
                    assets[variant].minimumY,assets[variant].height),scale,{0,yaw,0}}))){
                ++result.rejectedClearance;continue;
            }
            if(!ForestCoverFootprint(terrain,root,radius,footprintSettings)){++result.rejectedTerrain;continue;}
            if(!FernColonyClearsExisting(root,radius,existing,assets,settings.rootSeparationRatio)){++result.rejectedExistingFern;continue;}
            bool overlaps=false;
            for(const auto& blade:grass.plants)if(std::hypot(root.x-blade.transform.position.x,root.z-blade.transform.position.z)<
                settings.rootSeparationRatio*(radius+blade.radius)){overlaps=true;break;}
            if(overlaps){++result.rejectedGrass;continue;}
            for(const auto& fern:result.plants)if(std::hypot(root.x-fern.transform.position.x,root.z-fern.transform.position.z)<
                settings.rootSeparationRatio*(radius+fern.radius)){overlaps=true;break;}
            if(overlaps){++result.rejectedSpacing;continue;}
            result.plants.push_back({{root,{0,yaw,0},{scale,scale,scale}},variant,colonyIndex,patchIndex,radius});
            ++colony.accepted;
        }
        if(colony.accepted)result.colonies.push_back(colony);
    }
    result.reachedBudget=result.plants.size()==settings.targetPlants;
    return result;
}
}
