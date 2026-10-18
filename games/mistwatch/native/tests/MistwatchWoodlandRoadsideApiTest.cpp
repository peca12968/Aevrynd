#include "fallen/MistwatchAuthoredWorldQualification.h"
#include "MistwatchEntranceUnderstory.h"
#include "MistwatchNettleMix.h"
#include "MistwatchWoodlandCanopy.h"
#include "MistwatchWoodlandRecipe.h"
#include <iostream>

namespace Q=::Mistwatch::AuthoredQualification;
namespace F=::Mistwatch::FallenWorldSeat;
namespace C=::Mistwatch::WoodlandCanopy;
namespace G=::Mistwatch::GroupedCanopyVolume;
namespace W=::Mistwatch::WoodlandBridgeWorld;
using namespace Aevrynd;
using namespace Mistwatch;

namespace {
unsigned negatives{};
void Need(bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);}
template<class Action>void Reject(Action&& action,std::string_view expected){
    bool rejected{};try{action();}catch(const std::invalid_argument& e){
        Need(e.what()==expected,"WRONG_NEGATIVE_GATE");rejected=true;}
    Need(rejected,"NEGATIVE_ACCEPTED");++negatives;
}
bool Same(const RoadsideCoverResult& a,const RoadsideCoverResult& b){
    if(a.attempted!=b.attempted||a.rejectedRoute!=b.rejectedRoute||a.rejectedWorld!=b.rejectedWorld||
        a.rejectedTerrain!=b.rejectedTerrain||a.rejectedVegetation!=b.rejectedVegetation||a.left!=b.left||
        a.right!=b.right||a.fingerprint!=b.fingerprint||a.plants.size()!=b.plants.size())return false;
    for(std::size_t i=0;i<a.plants.size();++i){const auto& x=a.plants[i];const auto& y=b.plants[i];
        if(!Q::Detail::Same(x.transform,y.transform)||x.variant!=y.variant||x.patch!=y.patch||
            !Q::Detail::Same(x.radius,y.radius))return false;}return true;
}
bool Same(const EntranceUnderstoryResult& a,const EntranceUnderstoryResult& b){
    if(a.clusterIds!=b.clusterIds||a.suppressedGrassIds!=b.suppressedGrassIds||
        a.suppressedBroadleafIds!=b.suppressedBroadleafIds||a.clusterCounts!=b.clusterCounts||
        a.attempted!=b.attempted||a.rejectedRoute!=b.rejectedRoute||a.rejectedWorld!=b.rejectedWorld||
        a.rejectedTerrain!=b.rejectedTerrain||a.rejectedVegetation!=b.rejectedVegetation||a.left!=b.left||
        a.right!=b.right||a.fingerprint!=b.fingerprint||a.suppressionFingerprint!=b.suppressionFingerprint||
        a.plants.size()!=b.plants.size())return false;
    for(std::size_t i=0;i<a.plants.size();++i){const auto& x=a.plants[i];const auto& y=b.plants[i];
        if(!Q::Detail::Same(x.pose,y.pose)||x.variant!=y.variant||!Q::Detail::Same(x.radius,y.radius))return false;}return true;
}
std::array<NettleAssetBounds,6> LoadNettleBounds(const std::filesystem::path& root){
    std::array<NettleAssetBounds,6> result;
    for(unsigned i=0;i<result.size();++i){auto loaded=LoadAevryndMeshFile(root/"mistwatch-nettle/meshes"/NettleMeshName(i));
        Need(bool(loaded),"NETTLE_ASSET_MISSING");NormalizeNettleAsset(loaded.mesh,i);result[i]=MeasureNettleAssetBounds(loaded.mesh);}
    return result;
}
}

int wmain(int argc,wchar_t** argv)try{
    Need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","USAGE_RUNTIME_ASSETS");
    const auto root=std::filesystem::path(argv[2]);Need(root.is_absolute(),"ABSOLUTE_ASSETS_REQUIRED");
    F::Budget budget;const auto terrain=CreateTerrain();const auto assets=Q::LoadAssets(root);
    const auto source=Q::CreateSourceContext(terrain,assets,budget);std::vector<F::Placement> placements;
    for(const auto& p:FallenLogPlacements()){F::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;
        q.pose=p.pose;q.collider=p.collider;placements.push_back(q);}
    const auto context=Q::CreateContext(terrain,assets,source,placements,budget);
    Q::ValidateContext(context,terrain,assets,budget);const auto contextSeal=Q::ContextFingerprint(context);
    const auto legacyBefore=F::Boxes(context.worlds[0]),legacyAfter=F::Boxes(context.worlds[1]);
    const auto nettleBounds=LoadNettleBounds(root);std::array<NettleAssetBounds,2> broadleafBounds{{nettleBounds[2],nettleBounds[3]}};

    // 舊入口仍以原 8853/8854 flat world 產生逐欄相同結果；明確 nullptr 不改任何 RNG 或接受順序。
    const auto oldRoad=CreateRoadsideCover(terrain,context.retention.cover,context.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,legacyBefore,legacyAfter);
    const auto oldRoadExplicit=CreateRoadsideCover(terrain,context.retention.cover,context.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,legacyBefore,legacyAfter,{},nullptr);
    Need(Same(oldRoad,oldRoadExplicit),"LEGACY_ROADSIDE_CHANGED");
    const auto oldBroadleaf=CreateRoadsideBroadleaf(terrain,context.retention.cover,context.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,broadleafBounds,legacyBefore,legacyAfter);
    const auto oldBroadleafExplicit=CreateRoadsideBroadleaf(terrain,context.retention.cover,context.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,broadleafBounds,legacyBefore,legacyAfter,{},nullptr);
    Need(Same(oldBroadleaf,oldBroadleafExplicit),"LEGACY_BROADLEAF_CHANGED");
    const auto oldUnderstory=CreateEntranceUnderstory(terrain,context.retention.cover,context.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,oldBroadleaf,legacyBefore,legacyAfter);
    const auto oldUnderstoryExplicit=CreateEntranceUnderstory(terrain,context.retention.cover,context.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,oldBroadleaf,legacyBefore,legacyAfter,nullptr,{},nullptr);
    Need(Same(oldUnderstory,oldUnderstoryExplicit),"LEGACY_UNDERSTORY_CHANGED");
    const auto legacyBodies=context.author.VegetationSolids(false);
    const auto oldNettle=CreateNettleMix(context.retention.cover,terrain,nettleBounds,legacyBodies,legacyBefore,legacyAfter);
    const auto oldNettleExplicit=CreateNettleMix(context.retention.cover,terrain,nettleBounds,legacyBodies,legacyBefore,legacyAfter,nullptr);
    Need(oldNettle.renderVariants==oldNettleExplicit.renderVariants&&oldNettle.variants==oldNettleExplicit.variants&&
        oldNettle.accepted==oldNettleExplicit.accepted&&oldNettle.rejected==oldNettleExplicit.rejected&&
        oldNettle.worldChecks==oldNettleExplicit.worldChecks&&oldNettle.fingerprint==oldNettleExplicit.fingerprint,
        "LEGACY_NETTLE_CHANGED");

    // 新 world 只能由固定真來源、固定 recipe 與十二株真 proxy 建立，不以測試內生成值自我背書。
    const auto woodland=C::ApplyRecipe(terrain,context.author,context.canopy,assets.canopy,assets.derivedStem,
        assets.fernAssets,C::FixedRecipe());
    std::vector<G::Group> groups;groups.reserve(woodland.proxies.size());
    for(const auto& proxy:woodland.proxies)groups.emplace_back(proxy.parts);
    const G::ValidatedWood groupedWood(groups,W::WoodIdentity);
    const auto bodiesBefore=woodland.author.VegetationSolids(false),bodiesAfter=woodland.author.VegetationSolids(true);
    const W::ValidatedWorld world(bodiesBefore,bodiesAfter,groupedWood);
    Need(world.Bodies(false).size()==229&&world.Bodies(true).size()==230&&world.Wood().Identity()==W::WoodIdentity,
        "FIXED_WOODLAND_WORLD_REQUIRED");

    const std::span<const BoxCollider3D> empty;
    const auto newRoad=CreateRoadsideCover(terrain,context.retention.cover,woodland.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,empty,empty,{},&world);
    const auto newBroadleaf=CreateRoadsideBroadleaf(terrain,context.retention.cover,woodland.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,broadleafBounds,empty,empty,{},&world);
    const auto newUnderstory=CreateEntranceUnderstory(terrain,context.retention.cover,woodland.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,newBroadleaf,empty,empty,nullptr,{},&world);
    const auto newNettle=CreateNettleMix(context.retention.cover,terrain,nettleBounds,world.Bodies(false),empty,empty,&world);
    Need(newRoad.plants.size()>=128&&newBroadleaf.plants.size()==96&&newUnderstory.plants.size()==24&&
        newNettle.accepted+newNettle.rejected==1200&&newNettle.worldChecks==2400,"WOODLAND_CALLERS_INCOMPLETE");

    // 每個 public caller 都拒絕 grouped/flat 混用；Nettle 另要求 vegetation 逐 float bit 等於固定 before bodies。
    Reject([&]{(void)CreateRoadsideCover(terrain,context.retention.cover,woodland.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,legacyBefore,empty,{},&world);},"ROADSIDE_MIXED_WORLD_INPUT");
    Reject([&]{(void)CreateRoadsideBroadleaf(terrain,context.retention.cover,woodland.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,broadleafBounds,empty,legacyAfter,{},&world);},"ROADSIDE_MIXED_WORLD_INPUT");
    Reject([&]{(void)CreateEntranceUnderstory(terrain,context.retention.cover,woodland.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,newBroadleaf,legacyBefore,empty,nullptr,{},&world);},"ROADSIDE_MIXED_WORLD_INPUT");
    Reject([&]{(void)CreateNettleMix(context.retention.cover,terrain,nettleBounds,world.Bodies(false),legacyBefore,empty,&world);},
        "Nettle mixed grouped and flat world");
    auto changedBodies=std::vector<BoxCollider3D>(world.Bodies(false).begin(),world.Bodies(false).end());
    changedBodies.back().center.x=std::nextafter(changedBodies.back().center.x,INFINITY);
    Reject([&]{(void)CreateNettleMix(context.retention.cover,terrain,nettleBounds,changedBodies,empty,empty,&world);},
        "Nettle requires fixed woodland body order");

    // moved-from world 必須在任何 vegetation／asset 驗證前拒絕；不能退化成空木件成功。
    auto stale=world;auto live=std::move(stale);(void)live;
    Reject([&]{(void)CreateRoadsideCover(terrain,context.retention.cover,woodland.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,empty,empty,{},&stale);},"WOODLAND_BRIDGE_WORLD_INVALID_STATE");
    Reject([&]{(void)CreateRoadsideBroadleaf(terrain,context.retention.cover,woodland.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,broadleafBounds,empty,empty,{},&stale);},"WOODLAND_BRIDGE_WORLD_INVALID_STATE");
    Reject([&]{(void)CreateEntranceUnderstory(terrain,context.retention.cover,woodland.author.understory,context.colonies,
        context.shrubs,assets.grassAssets,assets.fernAssets,newBroadleaf,empty,empty,nullptr,{},&stale);},"WOODLAND_BRIDGE_WORLD_INVALID_STATE");
    Reject([&]{(void)CreateNettleMix(context.retention.cover,terrain,nettleBounds,empty,empty,empty,&stale);},
        "WOODLAND_BRIDGE_WORLD_INVALID_STATE");

    // 找一個只被末組攔下的真 15cm proxy 部位，證明完整查詢沒有漏掉 group 11。
    bool lastGroupObserved{};const auto allGroups=world.Wood().Groups();
    for(const auto& box:allGroups.back()){
        const CanopyVolume::PlantVolume probe{box.center,{.001f,-.001f,.001f},1,{}};
        if(!CanopyVolume::ClearsCanopyWood(probe,world.Bodies(false)))continue;
        bool earlierClear=true;for(std::size_t i=0;i+1<allGroups.size();++i)
            earlierClear&=CanopyVolume::ClearsCanopyWood(probe,allGroups[i]);
        if(earlierClear){Need(!world.Clears(probe,false),"FINAL_GROUP_NOT_QUERIED");lastGroupObserved=true;break;}
    }
    Need(lastGroupObserved,"NO_EXCLUSIVE_FINAL_GROUP_PROBE");
    Need(Q::ContextFingerprint(context)==contextSeal,"ORIGINAL_CONTEXT_MUTATED");
    std::cout<<"WOODLAND_ROADSIDE_API_CPU_PASS old_flat_unchanged=1 grouped_callers=4 mixed_rejected=4 moved_rejected=4"
        <<" body_bit_negative=1 last_group_observed=1 negatives="<<negatives
        <<" full_world_qualified=false native_passed=false\n";return 0;
}catch(const std::exception& e){std::cerr<<"WOODLAND_ROADSIDE_API_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
