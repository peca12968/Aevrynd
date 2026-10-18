#include "fallen/MistwatchAuthoredWorldQualification.h"
#include "MistwatchWoodlandRecipe.h"
#include "MistwatchWoodlandBridgeWorld.h"
#include "MistwatchShrubArcDensity.h"
#include "MistwatchRoadsideBroadleaf.h"
#include "MistwatchEntranceUnderstory.h"
#include "MistwatchNettleMix.h"
#include "MistwatchLoadPreparation.h"
#include "MistwatchForestHabitatFloor.h"
#include "MistwatchEntranceRoadDomain.h"
#include "MistwatchSettlementGround.h"
#include <iostream>

using namespace Aevrynd;
using namespace Mistwatch;
namespace Q=::Mistwatch::AuthoredQualification;
namespace W=::Mistwatch::FallenWorldSeat;
namespace C=::Mistwatch::WoodlandCanopy;
namespace G=::Mistwatch::GroupedCanopyVolume;
namespace B=::Mistwatch::WoodlandBridgeWorld;
namespace {
unsigned checks{},negatives{};
void Need(bool okay,const char* why){++checks;if(!okay)throw std::runtime_error(why);}
template<class Action>void Reject(Action&& action,const char* why){bool caught{};try{action();}catch(const std::invalid_argument&){caught=true;}
    Need(caught,why);++negatives;}
void SameBoxes(std::span<const BoxCollider3D> a,std::span<const BoxCollider3D> b,const char* why){
    Need(a.size()==b.size(),why);for(std::size_t i=0;i<a.size();++i)Need(B::Detail::Words(a[i])==B::Detail::Words(b[i]),why);
}
void CheckVolume(const Transform3D& pose,CanopyVolume::PlantLocalVolume local,const B::ValidatedWorld& world){
    for(bool repaired:{false,true}){
        Need(CanopyVolume::ClearsCanopyWood(pose,local,world.Bodies(repaired)),"INDEPENDENT_BODY_VOLUME");
        for(const auto& group:world.Wood().Groups())Need(CanopyVolume::ClearsCanopyWood(pose,local,group),"INDEPENDENT_ALL_TWELVE_VOLUME");
    }
}
std::vector<BoxCollider3D> Extras(const C::Result& r){std::vector<BoxCollider3D> out;
    for(const auto& p:r.author.middle.trees)out.push_back(p.trunk);
    for(const auto& p:r.proxies)out.insert(out.end(),p.parts.begin(),p.parts.end());return out;}
void SaveBoth(const C::Result& r,const B::ValidatedWorld& world,const HeightField3D& terrain){
    CharacterController3D controller;Explorer explorer;Boar boar;OrbitCamera orbit{-.65f,.3f,6};
    const LoadPreparationInputs live{controller,explorer,boar,orbit};const auto extras=Extras(r);
    Need(extras.size()==C::FixedMiddleCount+C::FixedWoodParts,"SAVE_COMPLETE_EXTRA_COUNT");
    for(bool repaired:{false,true}){SaveGame save;
        if(repaired){for(unsigned i=0;i<3;++i){save.session.feet=Session::supplies[i];Need(save.session.Collect(i),"SAVE_SUPPLY_RULE");}
            for(unsigned i=0;i<3;++i){save.session.feet=Session::repairSites[i];Need(save.session.CollectRepairMaterial(i,i==0),"SAVE_MATERIAL_RULE");}
            save.session.feet=Session::bridge;Need(save.session.RepairBridge(),"SAVE_BRIDGE_RULE");}
        save.session.feet={0,terrain.SampleHeight(0,0),0};Need(save.Valid(),"SAVE_VALID_SNAPSHOT");
        const auto serialized=save.Serialize();SaveGame parsed;Need(parsed.Restore(serialized),"SAVE_PARSE");
        const auto prepared=PrepareSavedGame(parsed,live,terrain,{},extras,FirLayoutSelector::ForestWood,FallenLogProfile::ScannedStraightV1,&r.author);
        if(!prepared)throw std::runtime_error("WOODLAND_SAVE_PREPARATION_ERROR="+std::to_string(unsigned(prepared.error)));
        SameBoxes(prepared.candidate->solids,world.Flatten(repaired),"SAVE_EXACT_BODY_MIDDLE_TWELVE_ORDER");
        Need(prepared.candidate->cameraResult.accepted&&prepared.candidate->cameraResult.subjectVisible,"SAVE_CAMERA_ACCEPTED");
    }
    // 真第十二組木件必須參與query；其第一格不是僅靠總數保留的空證據。
    const auto part=r.proxies.back().parts.front();const Vec3 p=part.center;
    const CanopyVolume::PlantVolume probe{p,{.01f,-.01f,.01f},1,{}};
    Need(!world.Clears(probe,false)&&!world.Clears(probe,true),"ACTUAL_LAST_GROUP_QUERY");
    Need(LoadPreparationDetail::BodyOverlaps(part.center,{.01f,.01f,.01f},extras),"SAVE_LAST_GROUP_BODY_OVERLAP");
}
std::uint64_t NativeFerns(std::span<const FernColonyPlant> plants){Q::Detail::Hash h;
    for(const auto& p:plants){h.Pose(p.transform);h.Word(p.variant);}return h.value;}
void Expected(std::string_view key,std::uint64_t value){std::cout<<"WOODLAND_NATIVE_EXPECTED "<<key<<'='<<value<<std::endl;}
}
int wmain(int argc,wchar_t** argv){try{
    Need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","ABSOLUTE_RUNTIME_USAGE");const auto root=std::filesystem::path(argv[2]);Need(root.is_absolute(),"ABSOLUTE_ASSETS");
    W::Budget budget;const auto assets=Q::LoadAssets(root);const auto terrain=CreateTerrain();
    const auto source=Q::CreateSourceContext(terrain,assets,budget);std::vector<W::Placement> placements;
    for(const auto& p:FallenLogPlacements()){W::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;q.pose=p.pose;q.collider=p.collider;placements.push_back(q);}
    const auto old=Q::CreateContext(terrain,assets,source,placements,budget);Q::ValidateContext(old,terrain,assets,budget);const auto oldIdentity=Q::ContextFingerprint(old);
    std::cout<<"WOODLAND_DOWNSTREAM_STAGE fixed-author"<<std::endl;
    const auto result=C::ApplyRecipe(terrain,old.author,old.canopy,assets.canopy,assets.derivedStem,assets.fernAssets,C::FixedRecipe());
    Need(result.complete&&result.outputIdentity==C::FixedOutputIdentity&&result.wood.size()==C::FixedWoodParts&&
        result.woodFingerprint==C::FixedWoodFingerprint,"EXACT_FIXED_AUTHOR");
    std::vector<G::Group> groups;for(const auto& p:result.proxies)groups.emplace_back(p.parts);
    const G::ValidatedWood wood(groups,B::WoodIdentity);const auto before=result.author.VegetationSolids(false),after=result.author.VegetationSolids(true);
    const B::ValidatedWorld world(before,after,wood);
    Need(B::BodyFingerprint(before,false)==B::BeforeBodyFingerprint&&B::BodyFingerprint(after,true)==B::AfterBodyFingerprint,"SOURCE_DERIVED_BODY_ORACLE");
    auto bad=before;bad.back().center.x=std::nextafter(bad.back().center.x,INFINITY);Reject([&]{(void)B::ValidatedWorld(bad,after,wood);},"BODY_ONE_ULP_REJECTED");
    bad=before;std::swap(bad.front(),bad.back());Reject([&]{(void)B::ValidatedWorld(bad,after,wood);},"BODY_ORDER_REJECTED");
    auto badAfter=after;std::swap(badAfter[33],badAfter.back());Reject([&]{(void)B::ValidatedWorld(before,badAfter,wood);},"BRIDGE_WRONG_INDEX_REJECTED");
    badAfter=after;badAfter.erase(badAfter.begin()+33);Reject([&]{(void)B::ValidatedWorld(before,badAfter,wood);},"BRIDGE_MISSING_REJECTED");
    auto movedFrom=world;auto moved=std::move(movedFrom);moved.RequireValid();Reject([&]{movedFrom.RequireValid();},"WORLD_MOVED_FROM_REJECTED");
    const auto oldSolids=W::Boxes(old.sourceMapped[0]);
    ForestCoverWorldView original{terrain,source.firs.roots,source.understory,oldSolids,old.sourceWood,4335,0};
    ForestCoverWorldView finalWorld{terrain,result.author.firs.roots,result.author.understory,world.Bodies(false),{},C::FixedWoodParts,0,&world.Wood()};
    original.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(original);finalWorld.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(finalWorld);
    std::cout<<"WOODLAND_DOWNSTREAM_STAGE grass-retention original_fp="<<original.expectedInputFingerprint<<" final_fp="<<finalWorld.expectedInputFingerprint<<std::endl;
    const auto retained=CreateForestCoverRetained(old.sourceCover,original,finalWorld,assets.grassAssets,assets.grassIdentities);
    Need(retained.cover.reachedBudget&&retained.cover.plants.size()==3600&&retained.provenance.size()==3600&&
        retained.retained+retained.filled==3600&&retained.retained+retained.rejected==3600,"FULL_3600_RETENTION_PROVENANCE");
    std::array<unsigned,400> patchCounts{};Need(retained.cover.patches.size()<=patchCounts.size(),"GRASS_PATCH_BUDGET");
    for(unsigned i=0;i<3600;++i){const auto& p=retained.cover.plants[i];Need(p.variant<5&&p.patch<retained.cover.patches.size(),"GRASS_SOURCE_VARIANT_PATCH");++patchCounts[p.patch];
        const auto& a=assets.grassAssets[p.variant];CheckVolume(p.transform,CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.maximumY),world);
        Need(p.transform.position.y==terrain.SampleHeight(p.transform.position.x,p.transform.position.z)&&ForestCoverFootprint(terrain,p.transform.position,p.radius,{}),"GRASS_COMPLETE_GROUND");
        const auto& origin=retained.provenance[i];if(origin.origin==ForestCoverPlantOrigin::Retained){Need(origin.sourcePlant<old.sourceCover.plants.size(),"GRASS_RETAINED_SOURCE_ID");
            const auto& oldPlant=old.sourceCover.plants[origin.sourcePlant];Need(Q::Detail::Same(p.transform,oldPlant.transform)&&Q::Detail::Same(p.radius,oldPlant.radius)&&p.variant==oldPlant.variant&&origin.sourcePatch==oldPlant.patch,"GRASS_RETAINED_ALL_POSE_BITS");}
        for(unsigned j=0;j<i;++j)Need(std::hypot(p.transform.position.x-retained.cover.plants[j].transform.position.x,p.transform.position.z-retained.cover.plants[j].transform.position.z)>=.65f*(p.radius+retained.cover.plants[j].radius),"GRASS_FULL_PAIR_SPACING");}
    for(unsigned i=0;i<retained.cover.patches.size();++i)Need(patchCounts[i]==retained.cover.patches[i].accepted&&patchCounts[i]>0,"GRASS_PATCH_PROVENANCE_COUNT");
    std::cout<<"WOODLAND_DOWNSTREAM_STAGE fern-colonies"<<std::endl;
    const auto colonies=CreateFernColonies(terrain,retained.cover,result.author.understory.ferns,world.Bodies(false),assets.fernAssets,{}, {},true,&world.Wood());
    Need(colonies.reachedBudget&&colonies.plants.size()==300,"FULL_300_FERN_COLONIES");
    for(const auto& p:colonies.plants){const auto& a=assets.fernAssets[p.variant];CheckVolume(p.transform,CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.height),world);
        Need(ForestCoverFootprint(terrain,p.transform.position,p.radius,{}),"COLONY_COMPLETE_GROUND");}
    std::cout<<"WOODLAND_DOWNSTREAM_STAGE preserved-prefix-shrub128"<<std::endl;
    const auto prefix=CreateShrubCanopy(assets.shrubLeaf,terrain,world.Bodies(false),result.author.understory,retained.cover,colonies,assets.fernAssets,64,{},true,nullptr,&world.Wood());
    const auto shrubs=CreateShrubArcDensity(assets.shrubLeaf,terrain,world.Bodies(false),result.author.understory,retained.cover,colonies,assets.fernAssets,128,{},true,nullptr,&world.Wood());
    const auto composition=ShrubCompositionContract::PreservedPrefix128(prefix,shrubs);Need(shrubs.reachedBudget&&shrubs.plants.size()==128,"NORMAL_FULL_128_SHRUBS");
    for(const auto& p:shrubs.plants){const auto& a=shrubs.variants[p.variant];CheckVolume(p.pose,CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.maximumY),world);Need(ForestCoverFootprint(terrain,p.pose.position,p.radius,{}),"SHRUB_COMPLETE_GROUND");}
    std::array<NettleAssetBounds,6> nettle{};for(unsigned i=0;i<6;++i){auto mesh=LoadAevryndMeshFile(root/"mistwatch-nettle/meshes"/NettleMeshName(i));Need(bool(mesh),"NETTLE_ACTUAL_SOURCE");NormalizeNettleAsset(mesh.mesh,i);nettle[i]=MeasureNettleAssetBounds(mesh.mesh);}
    const std::array<NettleAssetBounds,2> broadleafAssets{{nettle[2],nettle[3]}};
    std::cout<<"WOODLAND_DOWNSTREAM_STAGE both-bridge-roadside"<<std::endl;
    const auto shoulder=CreateRoadsideCover(terrain,retained.cover,result.author.understory,colonies,shrubs,assets.grassAssets,assets.fernAssets,{}, {},composition,&world);
    const auto broadleaf=CreateRoadsideBroadleaf(terrain,retained.cover,result.author.understory,colonies,shrubs,assets.grassAssets,assets.fernAssets,broadleafAssets,{}, {},composition,&world);
    Need(shoulder.left>=64&&shoulder.right>=64&&broadleaf.plants.size()==96&&broadleaf.left==48&&broadleaf.right==48,"FULL_BOTH_SHOULDERS");
    for(const auto& p:broadleaf.plants){const auto& a=broadleafAssets[p.variant];CheckVolume(p.transform,CanopyVolume::ConservativeLocalVolume(a.radius+.19f/p.transform.scale.x,a.minimumY,a.maximumY+.011f/p.transform.scale.x),world);}
    const auto under=CreateEntranceUnderstory(terrain,retained.cover,result.author.understory,colonies,shrubs,assets.grassAssets,assets.fernAssets,broadleaf,{}, {},nullptr,composition,&world);
    Need(under.plants.size()==24,"FULL_24_ENTRANCE_UNDERSTORY");for(unsigned count:under.clusterCounts)Need(count==4,"ALL_SIX_CLUSTERS");
    for(const auto& p:under.plants){const auto& a=shrubs.variants[p.variant];CheckVolume(p.pose,CanopyVolume::ConservativeLocalVolume(a.radius+.19f/p.pose.scale.x,a.minimumY,a.maximumY+.011f/p.pose.scale.x),world);}
    const auto mix=CreateNettleMix(retained.cover,terrain,nettle,world.Bodies(false),{}, {},&world);Need(mix.accepted>0&&mix.worldChecks==2400&&mix.accepted+mix.rejected==1200,"FINITE_NETTLE_SELECTION");
    std::cout<<"WOODLAND_DOWNSTREAM_STAGE save-two-states"<<std::endl;SaveBoth(result,world,terrain);
    std::vector<Vec3> canopyRoots;for(const auto& p:result.canopy.plants)canopyRoots.push_back(p.pose.position);
    auto ground=CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    auto landscape=CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    const auto habitat=ApplyForestHabitatFloor(ground,terrain,{result.author.firs.roots,canopyRoots});
    const auto habitatLandscape=ApplyForestHabitatFloor(landscape,terrain,{result.author.firs.roots,canopyRoots},true);
    Need(ground.Valid()&&landscape.Valid()&&habitat.changed>0,"FULL_TWELVE_HABITAT");
    auto road=CreateEntranceRoadDomain(ground);const auto settlement=SettlementGround::Create(ground,road);
    // 回放的Expected來自獨立CPU作者，不取原生frame或舊八株的幾何指紋。
    const auto gameExpected=[](std::string_view key,std::uint64_t value){
        std::cout<<"WOODLAND_GAME_EXPECTED "<<key<<'='<<value<<std::endl;};
    std::cout<<"WOODLAND_GAME_EXPECTED settlement_ground_profile="<<SettlementGround::Profile<<std::endl;
    gameExpected("settlement_ground_moved_triangles",settlement.movedSourceTriangles.size());
    gameExpected("settlement_ground_boundary_vertices",settlement.newBoundaryVertices.size());
    gameExpected("settlement_ground_changed_forest",settlement.changedForestVertices);
    gameExpected("settlement_ground_changed_road",settlement.changedRoadVertices);
    gameExpected("settlement_ground_original_forest_fingerprint",road.receipt.forestFingerprint);
    gameExpected("settlement_ground_original_road_fingerprint",road.receipt.roadFingerprint);
    gameExpected("entrance_road_source_fingerprint",settlement.partition.receipt.sourceFingerprint);
    gameExpected("entrance_road_forest_fingerprint",settlement.partition.receipt.forestFingerprint);
    gameExpected("entrance_road_road_fingerprint",settlement.partition.receipt.roadFingerprint);
    gameExpected("entrance_road_source_triangles",settlement.partition.receipt.sourceTriangles);
    gameExpected("entrance_road_forest_triangles",settlement.partition.receipt.forestTriangles);
    gameExpected("entrance_road_road_triangles",settlement.partition.receipt.roadTriangles);
    gameExpected("entrance_road_shared_vertices",settlement.partition.receipt.sharedVertices);
    Expected("author",result.author.fingerprint);Expected("canopy",result.canopy.fingerprint);
    Expected("fir",result.author.firs.fingerprint);Expected("middle",MiddleLayerTransformFingerprint(result.author.middle.trees));
    Expected("grass",Q::Detail::NativeCoverIdentity(retained.cover));Expected("retention",retained.fingerprint);
    Expected("fern",NativeFerns(colonies.plants));Expected("shrub",shrubs.fingerprint);
    Expected("broadleaf",broadleaf.fingerprint);Expected("understory",under.fingerprint);
    Expected("suppression",under.suppressionFingerprint);Expected("suppressed_grass",under.suppressedGrassIds.size());
    Expected("suppressed_broadleaf",under.suppressedBroadleafIds.size());
    Expected("habitat_terrain",habitat.fingerprint);Expected("habitat_landscape",habitatLandscape.fingerprint);
    Expected("road_source",settlement.partition.receipt.sourceFingerprint);
    Expected("road_forest",settlement.partition.receipt.forestFingerprint);Expected("road_road",settlement.partition.receipt.roadFingerprint);
    for(unsigned i=0;i<C::TotalCount;++i){C::Detail::Hash h;h.Mesh(result.canopy.plants[i].rootedTrunk);Expected("root_"+std::to_string(i)+"_mesh",h.value);}
    Need(Q::ContextFingerprint(old)==oldIdentity,"ORIGINAL_EIGHT_CONTEXT_UNCHANGED");
    std::cout<<"WOODLAND_DOWNSTREAM_CPU_PASS checks="<<checks<<" negatives="<<negatives<<" grass=3600 fern_colonies=300 shrubs=128 roadside_broadleaf=96 entrance_understory=24"
        <<" retention="<<retained.fingerprint<<" retained="<<retained.retained<<" filled="<<retained.filled<<" body_before="<<B::BeforeBodyCount<<" body_after="<<B::AfterBodyCount
        <<" complete_before="<<C::FixedCompleteBefore<<" complete_after="<<C::FixedCompleteAfter<<" both_bridge_save=true actual_last_group=true normal_integrated=false native_passed=false art_accepted=false"<<std::endl;
    return 0;
}catch(const std::exception& e){std::cerr<<"WOODLAND_DOWNSTREAM_FIRST_DIVERGENCE "<<e.what()<<std::endl;return 2;}}
