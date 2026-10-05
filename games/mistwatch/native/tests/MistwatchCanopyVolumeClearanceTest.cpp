#include "MistwatchCanopyVolumeClearance.h"
#include "MistwatchShrubCanopy.h"
#include "MistwatchMiddleLayer.h"
#include <Aevrynd/MeshLoader.h>
#include <bit>
#include <iostream>
#include <filesystem>
#include <stdexcept>

namespace {
using namespace Aevrynd;using namespace Mistwatch;using namespace Mistwatch::CanopyVolume;
unsigned checks{};
void Require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
float& Axis(Vec3& value,unsigned axis){return axis==0?value.x:axis==1?value.y:value.z;}
bool SamePose(const Transform3D& a,const Transform3D& b){
    return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&
        a.rotation.x==b.rotation.x&&a.rotation.y==b.rotation.y&&a.rotation.z==b.rotation.z&&
        a.scale.x==b.scale.x&&a.scale.y==b.scale.y&&a.scale.z==b.scale.z;
}
template<class Function>void RejectWood(Function function){
    bool rejected=false;
    try{function();}catch(const std::invalid_argument& error){rejected=std::string_view(error.what()).find("canopy wood")!=std::string_view::npos;}
    Require(rejected,"Invalid wood must fail at generator entry even for target zero");
}
void HelperCases(){
    const PlantVolume grass{{0,0,0},{.5f,-.02f,.4f},1,{}};
    const BoxCollider3D overhead{{0,2,0},{2,.1f,2}},low{{0,1,0},{2,.1f,2}};
    Require(ClearsCanopyWood(grass),"Valid plant with empty wood remains compatible");
    Require(ClearsCanopyWood(grass,std::span(&overhead,1)),"Overhead wood must clear short grass");
    Require(ClearsCanopyWood(grass,std::span(&low,1)),"Branch above short grass must clear");
    auto shrub=grass;shrub.local.maximumY=1.1f;
    Require(!ClearsCanopyWood(shrub,std::span(&low,1)),"Low branch rejects full shrub height despite clear footpoint");
    auto negative=grass;negative.local.minimumY=-.08f;
    const BoxCollider3D below{{0,-.06f,0},{.25f,.02f,.25f}};
    Require(!ClearsCanopyWood(negative,std::span(&below,1),0),"Negative local minimum is part of full volume");
    const BoxCollider3D touch{{1,0,0},{.5f,.25f,.25f}},gap{{1.015625f,0,0},{.5f,.25f,.25f}};
    Require(!ClearsCanopyWood(grass,std::span(&touch,1),0),"Exact contact must reject");
    Require(ClearsCanopyWood(grass,std::span(&gap,1),0),"Gap without margin can clear");
    Require(!ClearsCanopyWood(grass,std::span(&gap,1),.03125f),"Near contact inside margin must reject");
    auto exact=grass;exact.local.maximumY=.5f;
    const BoxCollider3D top{{0,.75f,0},{.25f,.25f,.25f}};
    Require(!ClearsCanopyWood(exact,std::span(&top,1),0),"Vertical touch must reject");
    const BoxCollider3D corner{{.6875f,.5f,.6875f},{.0625f,.25f,.0625f}};
    auto yaw=grass;yaw.local={1,-.125f,1};
    for(float angle:{0.f,.785398163f,1.570796327f,10000.f}){yaw.rotation.y=angle;Require(!ClearsCanopyWood(yaw,std::span(&corner,1),0),"Radial footprint covers yaw rotation");}
    yaw.rotation={.001f,0,0};Require(!ClearsCanopyWood(yaw),"Pitch unsupported even with empty wood");
    yaw.rotation={0,0,.001f};Require(!ClearsCanopyWood(yaw),"Roll unsupported even with empty wood");
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    for(float value:{nan,inf,-inf}){
        for(unsigned axis=0;axis<3;++axis){auto bad=grass;Axis(bad.root,axis)=value;Require(!ClearsCanopyWood(bad),"Nonfinite root rejected");
            bad=grass;Axis(bad.rotation,axis)=value;Require(!ClearsCanopyWood(bad),"Nonfinite rotation rejected");}
        auto bad=grass;bad.local.radius=value;Require(!ClearsCanopyWood(bad),"Nonfinite radius rejected");
        bad=grass;bad.local.minimumY=value;Require(!ClearsCanopyWood(bad),"Nonfinite minimum rejected");
        bad=grass;bad.local.maximumY=value;Require(!ClearsCanopyWood(bad),"Nonfinite maximum rejected");
        bad=grass;bad.uniformScale=value;Require(!ClearsCanopyWood(bad),"Nonfinite scale rejected");
        Require(!ClearsCanopyWood(grass,{},value),"Nonfinite margin rejected");
    }
    for(float value:{0.f,-1.f}){auto bad=grass;bad.uniformScale=value;Require(!ClearsCanopyWood(bad),"Nonpositive scale rejected");
        bad=grass;bad.local.radius=value;Require(!ClearsCanopyWood(bad),"Nonpositive radius rejected");}
    auto bad=grass;bad.local.maximumY=bad.local.minimumY;Require(!ClearsCanopyWood(bad),"Degenerate plant rejected");
    bad=grass;bad.local.radius=2;bad.uniformScale=std::numeric_limits<float>::max();Require(!ClearsCanopyWood(bad),"Scaled float overflow rejected");
    bad=grass;bad.uniformScale=std::numeric_limits<float>::denorm_min();Require(!ClearsCanopyWood(bad),"Scaled float underflow rejected");
    bad=grass;bad.root.x=std::numeric_limits<float>::max();Require(!ClearsCanopyWood(bad),"World precision collapse rejected");
    Require(!ClearsCanopyWood(grass,{},-1),"Negative margin rejected");
    Require(!ClearsCanopyWood(grass,{},std::numeric_limits<float>::max()),"Margin overflow rejected");
    Transform3D pose{};pose.scale={1,2,1};Require(!ClearsCanopyWood(pose,grass.local),"Nonuniform pose rejected");
    Mesh3D measured;measured.vertices={{-.5f,-.0625f,0},{.5f,1,0},{0,.25f,.5f}};measured.indices={0,1,2};
    PlantLocalVolume volume;
    Require(MeasureLocalVolume(std::span<const Vertex3D>(measured.vertices),volume)&&volume.minimumY==-.0625f&&volume.maximumY==1,"Full mesh measurement retains negative minimum and actual maximum");
    Require(!MeasureLocalVolume(std::span<const Vertex3D>{},volume),"Empty mesh volume rejects");
    measured.vertices[0].x=nan;Require(!MeasureLocalVolume(std::span<const Vertex3D>(measured.vertices),volume),"NaN vertex cannot be hidden by max reduction");
    measured.vertices[0]={std::numeric_limits<float>::max(),0,std::numeric_limits<float>::max()};
    Require(!MeasureLocalVolume(std::span<const Vertex3D>(measured.vertices),volume),"Measured radial overflow rejects");
    std::cout<<"CANOPY_HELPER_CASES_PASS\n";
}
void EntryCases(){
    const auto terrain=CreateTerrain();const ForestUnderstory understory{};const ForestCoverResult grass{};const FernColonyResult colonies{};
    const std::array<ForestCoverAsset,5> grassAssets{};const std::array<FernColonyAsset,4> fernAssets{};const Mesh3D noLeaf{};
    ForestCoverSettings noGrass;noGrass.targetPlants=0;FernColonySettings noFern;noFern.targetPlants=0;
    const auto reject=[&](const BoxCollider3D& box){
        const std::array<BoxCollider3D,2> wood{{{{100,100,100},{1,1,1}},box}};
        Require(!ValidWoodParts(wood),"Whole wood list must prevalidate");
        RejectWood([&]{(void)CreateForestCover(terrain,{},understory,{},grassAssets,noGrass,wood);});
        RejectWood([&]{(void)CreateFernColonies(terrain,grass,{}, {},fernAssets,noFern,wood);});
        RejectWood([&]{(void)CreateShrubCanopy(noLeaf,terrain,{},understory,grass,colonies,fernAssets,0,wood);});
    };
    const BoxCollider3D valid{{20,20,20},{1,1,1}};
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    for(float value:{nan,inf,-inf})for(unsigned axis=0;axis<3;++axis){auto bad=valid;Axis(bad.center,axis)=value;reject(bad);
        bad=valid;Axis(bad.extents,axis)=value;reject(bad);}
    for(float value:{-1.f,0.f})for(unsigned axis=0;axis<3;++axis){auto bad=valid;Axis(bad.extents,axis)=value;reject(bad);}
    auto huge=valid;huge.center.x=std::numeric_limits<float>::max();huge.extents.x=huge.center.x;reject(huge);
    huge=valid;huge.center.x=std::numeric_limits<float>::max();reject(huge);
    std::cout<<"CANOPY_ENTRY_PREVALIDATION_PASS\n";
}
void CompareGrass(const ForestCoverResult& a,const ForestCoverResult& b){
    Require(a.plants.size()==b.plants.size()&&a.patches.size()==b.patches.size()&&a.reachedBudget==b.reachedBudget&&
        a.rejectedClearance==b.rejectedClearance&&a.rejectedTerrain==b.rejectedTerrain&&a.rejectedSpacing==b.rejectedSpacing,"Empty/high wood changed grass counters");
    for(std::size_t i=0;i<a.plants.size();++i)Require(SamePose(a.plants[i].transform,b.plants[i].transform)&&a.plants[i].variant==b.plants[i].variant&&
        a.plants[i].patch==b.plants[i].patch&&a.plants[i].radius==b.plants[i].radius,"Empty/high wood changed full grass identity");
    for(std::size_t i=0;i<a.patches.size();++i){const auto& p=a.patches[i];const auto& q=b.patches[i];
        Require(p.centre.x==q.centre.x&&p.centre.y==q.centre.y&&p.centre.z==q.centre.z&&p.radius==q.radius&&p.aspect==q.aspect&&p.yaw==q.yaw&&p.requested==q.requested&&p.accepted==q.accepted,"Empty/high wood changed patch RNG");}
}
void CompareFern(const FernColonyResult& a,const FernColonyResult& b){
    Require(a.plants.size()==b.plants.size()&&a.colonies.size()==b.colonies.size()&&a.reachedBudget==b.reachedBudget&&
        a.rejectedClearance==b.rejectedClearance&&a.rejectedTerrain==b.rejectedTerrain&&a.rejectedExistingFern==b.rejectedExistingFern&&
        a.rejectedGrass==b.rejectedGrass&&a.rejectedSpacing==b.rejectedSpacing,"Empty/high wood changed fern counters");
    for(std::size_t i=0;i<a.plants.size();++i)Require(SamePose(a.plants[i].transform,b.plants[i].transform)&&a.plants[i].variant==b.plants[i].variant&&
        a.plants[i].colony==b.plants[i].colony&&a.plants[i].grassPatch==b.plants[i].grassPatch&&a.plants[i].radius==b.plants[i].radius,"Empty/high wood changed full fern identity");
    for(std::size_t i=0;i<a.colonies.size();++i){const auto& p=a.colonies[i];const auto& q=b.colonies[i];
        Require(p.centre.x==q.centre.x&&p.centre.y==q.centre.y&&p.centre.z==q.centre.z&&p.radius==q.radius&&p.grassPatch==q.grassPatch&&p.requested==q.requested&&p.accepted==q.accepted,"Empty/high wood changed colony RNG");}
}
void CompareShrub(const ShrubCanopyResult& a,const ShrubCanopyResult& b){
    Require(a.fingerprint==b.fingerprint&&a.attempted==b.attempted&&a.reachedBudget==b.reachedBudget&&a.plants.size()==b.plants.size()&&
        a.rejectedClearance==b.rejectedClearance&&a.rejectedTerrain==b.rejectedTerrain&&a.rejectedVegetation==b.rejectedVegetation,"Empty/high wood changed shrub fingerprint/counters");
    for(std::size_t i=0;i<a.plants.size();++i)Require(SamePose(a.plants[i].pose,b.plants[i].pose)&&a.plants[i].radius==b.plants[i].radius&&a.plants[i].variant==b.plants[i].variant,"Empty/high wood changed shrub pose");
}
void GeneratorCases(){
    const auto assetRoot=std::filesystem::path(MISTWATCH_SOURCE_ROOT)/"engine/assets";
    const auto terrain=CreateTerrain();const auto trees=TreePositions();const auto props=ForestProps(terrain,trees);
    const auto understory=CreateUnderstory(terrain,trees,props);const auto broadleaf=CreateBroadleafTrees(terrain,trees,props,understory);
    auto solids=WorldSolids(false);const auto middle=CreateMiddleLayer(terrain,trees,props,understory,broadleaf,solids);
    Require(middle.reachedBudget,"Original middle generation failed");for(const auto& tree:middle.trees)solids.push_back(tree.trunk);
    std::array<ForestCoverAsset,5> grassAssets;
    for(unsigned i=0;i<5;++i){auto loaded=LoadAevryndMeshFile(assetRoot/"mistwatch-grass-medium-02/meshes"/GrassMeshName(i));Require(bool(loaded),"Real grass asset missing");
        NormalizeGrassAsset(loaded.mesh);grassAssets[i]=ForestCoverAssetFromMesh(loaded.mesh);
        float radius=0,height=0,minimum=std::numeric_limits<float>::infinity(),maximum=-minimum;
        for(const auto& v:loaded.mesh.vertices){radius=(std::max)(radius,std::hypot(v.x,v.z));height=(std::max)(height,v.y);minimum=(std::min)(minimum,v.y);maximum=(std::max)(maximum,v.y);}
        Require(grassAssets[i].radius==radius&&grassAssets[i].height==height&&grassAssets[i].minimumY==minimum&&grassAssets[i].maximumY==maximum,"Real grass metadata changed old radius/height or omitted actual min/max");
    }
    std::array<FernColonyAsset,4> fernAssets;
    for(unsigned i=0;i<4;++i){auto loaded=LoadAevryndMeshFile(assetRoot/"mistwatch-fern-02/meshes"/FernMeshName(i));Require(bool(loaded),"Real fern asset missing");NormalizeFernAsset(loaded.mesh);fernAssets[i]=FernColonyAssetFromMesh(loaded.mesh);}
    auto leaf=LoadAevryndMeshFile(assetRoot/"realistic-cc0-first-batch/cooked/meshes/tree-small-02"/BroadleafMeshName(1));Require(bool(leaf),"Real shrub source missing");
    const BoxCollider3D overhead{{0,100,40},{100,.5f,100}},blocked{{0,0,40},{100,100,100}};
    const auto grass=CreateForestCover(terrain,trees,understory,solids,grassAssets);
    Require(grass.reachedBudget&&grass.plants.size()==3600&&grass.patches.size()==154,"Original middle grass profile drift");
    CompareGrass(grass,CreateForestCover(terrain,trees,understory,solids,grassAssets,{},{}));
    CompareGrass(grass,CreateForestCover(terrain,trees,understory,solids,grassAssets,{},std::span(&overhead,1)));
    const auto blockedGrass=CreateForestCover(terrain,trees,understory,solids,grassAssets,{},std::span(&blocked,1));
    Require(blockedGrass.plants.empty()&&!blockedGrass.reachedBudget&&blockedGrass.rejectedClearance>0,"Grass generator ignored full-volume wood rejection");
    const auto& firstGrass=grass.plants.front();const auto& firstGrassAsset=grassAssets[firstGrass.variant];
    const BoxCollider3D grassBranch{firstGrass.transform.position+Vec3{0,(firstGrassAsset.minimumY+firstGrassAsset.maximumY)*firstGrass.transform.scale.x*.5f,0},{.005f,.005f,.005f}};
    Require(std::abs(grassBranch.center.y-firstGrass.transform.position.y)>grassBranch.extents.y,"Grass branch fixture must clear root footpoint");
    const auto filteredGrass=CreateForestCover(terrain,trees,understory,solids,grassAssets,{},std::span(&grassBranch,1));
    Require(std::none_of(filteredGrass.plants.begin(),filteredGrass.plants.end(),[&](const auto& p){return SamePose(p.transform,firstGrass.transform);}),"Grass generator must reject wood at body above root");
    const auto fern=CreateFernColonies(terrain,grass,understory.ferns,solids,fernAssets);
    Require(fern.reachedBudget&&fern.plants.size()==300&&fern.colonies.size()==86,"Original middle fern profile drift");
    CompareFern(fern,CreateFernColonies(terrain,grass,understory.ferns,solids,fernAssets,{},{}));
    CompareFern(fern,CreateFernColonies(terrain,grass,understory.ferns,solids,fernAssets,{},std::span(&overhead,1)));
    const auto blockedFern=CreateFernColonies(terrain,grass,understory.ferns,solids,fernAssets,{},std::span(&blocked,1));
    Require(blockedFern.plants.empty()&&!blockedFern.reachedBudget&&blockedFern.rejectedClearance>0,"Fern generator ignored full-volume wood rejection");
    const auto& firstFern=fern.plants.front();const auto& firstFernAsset=fernAssets[firstFern.variant];
    const BoxCollider3D fernBranch{firstFern.transform.position+Vec3{0,(firstFernAsset.minimumY+firstFernAsset.height)*firstFern.transform.scale.x*.5f,0},{.005f,.005f,.005f}};
    Require(std::abs(fernBranch.center.y-firstFern.transform.position.y)>fernBranch.extents.y,"Fern branch fixture must clear root footpoint");
    const auto filteredFern=CreateFernColonies(terrain,grass,understory.ferns,solids,fernAssets,{},std::span(&fernBranch,1));
    Require(std::none_of(filteredFern.plants.begin(),filteredFern.plants.end(),[&](const auto& p){return SamePose(p.transform,firstFern.transform);}),"Fern generator must reject wood at body above root");
    const auto shrub=CreateShrubCanopy(leaf.mesh,terrain,solids,understory,grass,fern,fernAssets);
    Require(shrub.reachedBudget&&shrub.fingerprint==18032575485754629455ull&&shrub.attempted==6284,"Original shrub fingerprint drift");
    CompareShrub(shrub,CreateShrubCanopy(leaf.mesh,terrain,solids,understory,grass,fern,fernAssets,64,{}));
    CompareShrub(shrub,CreateShrubCanopy(leaf.mesh,terrain,solids,understory,grass,fern,fernAssets,64,std::span(&overhead,1)));
    const auto blockedShrub=CreateShrubCanopy(leaf.mesh,terrain,solids,understory,grass,fern,fernAssets,64,std::span(&blocked,1));
    Require(blockedShrub.plants.empty()&&!blockedShrub.reachedBudget&&blockedShrub.rejectedClearance>0,"Shrub generator ignored full-volume wood rejection");
    const auto& firstShrub=shrub.plants.front();const auto& firstShrubAsset=shrub.variants[firstShrub.variant];
    const BoxCollider3D shrubBranch{firstShrub.pose.position+Vec3{0,(firstShrubAsset.minimumY+firstShrubAsset.maximumY)*firstShrub.pose.scale.x*.5f,0},{.005f,.005f,.005f}};
    Require(std::abs(shrubBranch.center.y-firstShrub.pose.position.y)>shrubBranch.extents.y,"Shrub branch fixture must clear root footpoint");
    const auto filteredShrub=CreateShrubCanopy(leaf.mesh,terrain,solids,understory,grass,fern,fernAssets,64,std::span(&shrubBranch,1));
    Require(std::none_of(filteredShrub.plants.begin(),filteredShrub.plants.end(),[&](const auto& p){return SamePose(p.pose,firstShrub.pose);}),"Shrub generator must reject wood at body above root");
    for(const auto& variant:shrub.variants){float minimum=std::numeric_limits<float>::infinity(),maximum=-minimum;
        for(const auto* mesh:{&variant.stems,&variant.leaves})for(const auto& v:mesh->vertices){minimum=(std::min)(minimum,v.y);maximum=(std::max)(maximum,v.y);}
        Require(variant.minimumY==minimum&&variant.maximumY==maximum&&minimum<0,"Shrub full stems/leaves min/max missing");}
    Require(solids.size()==245&&understory.ferns.size()==420,"Original solids or original fern population changed");
    std::cout<<"CANOPY_GENERATOR_EMPTY_HIGH_WOOD_COMPATIBILITY_PASS grass=3600 fern=300 shrub=64 solids=245\n";
}
}
int main(){try{HelperCases();EntryCases();GeneratorCases();std::cout<<"MISTWATCH_CANOPY_VOLUME_CLEARANCE_PASS checks="<<checks<<" static_mesh_volume_only=1 dynamic_wind_unverified=1\n";return 0;}
catch(const std::exception& error){std::cerr<<"CANOPY_VOLUME_FIRST_DIVERGENCE "<<error.what()<<" checks="<<checks<<'\n';return 1;}}
