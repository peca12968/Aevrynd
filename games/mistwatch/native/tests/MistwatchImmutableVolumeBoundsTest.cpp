#include "MistwatchImmutableVolumeBounds.h"
#include "MistwatchForestCover.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
using namespace Aevrynd;using namespace Mistwatch;using namespace Mistwatch::CanopyVolume;
unsigned checks{};
void Require(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
float& Axis(Vec3& v,unsigned a){return a==0?v.x:a==1?v.y:v.z;}
const PlantVolume plant{{0,0,0},{.5f,0,1},1,{}};
void Compare(const PlantVolume& p,std::span<const BoxCollider3D> boxes,float margin,bool expected){
    const bool original=ClearsCanopyWood(p,boxes,margin);
    ImmutableVolumeBoundsCache cache(boxes);
    Require(cache.SourceParts()==boxes.size()&&cache.StorageParts()==0&&cache.ComputedParts()==0,"CACHE_CONSTRUCTOR_MUST_NOT_VALIDATE_OR_ALLOCATE");
    Require(original==expected&&cache.Clears(p,margin)==original,"CACHE_ORIGINAL_AND_EXPECTED_BOOL");
    const auto computed=cache.ComputedParts(),storage=cache.StorageParts();
    Require(cache.Clears(p,margin)==original&&cache.ComputedParts()==computed&&cache.StorageParts()==storage,"CACHE_REPEAT_MUST_NOT_RECOMPUTE");
}
void HelperControls(){
    Compare(plant,{},0,true);
    const BoxCollider3D high{{0,4,0},{1,.5f,1}},overlap{{0,.5f,0},{.5f,.5f,.5f}};
    Compare(plant,std::span(&high,1),0,true);Compare(plant,std::span(&overlap,1),0,false);
    // 原三軸嚴格比較及雙精度向外ULP不變：接觸/內一個float ULP拒絕，外一個ULP放行。
    for(unsigned axis=0;axis<3;++axis)for(float sign:{-1.f,1.f})for(float margin:{0.f,.03125f}){
        auto box=overlap;
        const float touch=axis==1?(sign>0?1.5f:-.5f):sign;
        Axis(box.center,axis)=touch+sign*margin;
        Compare(plant,std::span(&box,1),margin,false);
        Axis(box.center,axis)=std::nextafter(touch+sign*margin,sign>0?
            std::numeric_limits<float>::infinity():-std::numeric_limits<float>::infinity());
        Compare(plant,std::span(&box,1),margin,true);
        Axis(box.center,axis)=std::nextafter(touch+sign*margin,sign>0?
            -std::numeric_limits<float>::infinity():std::numeric_limits<float>::infinity());
        Compare(plant,std::span(&box,1),margin,false);
    }
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    for(float value:{nan,inf,-inf}){
        for(unsigned axis=0;axis<3;++axis){
            auto p=plant;Axis(p.root,axis)=value;Compare(p,std::span(&overlap,1),0,false);
            p=plant;Axis(p.rotation,axis)=value;Compare(p,std::span(&overlap,1),0,false);
            auto box=high;Axis(box.center,axis)=value;Compare(plant,std::span(&box,1),0,false);
            box=high;Axis(box.extents,axis)=value;Compare(plant,std::span(&box,1),0,false);
        }
        auto p=plant;p.local.radius=value;Compare(p,{},0,false);
        p=plant;p.local.minimumY=value;Compare(p,{},0,false);
        p=plant;p.local.maximumY=value;Compare(p,{},0,false);
        p=plant;p.uniformScale=value;Compare(p,{},0,false);Compare(plant,{},value,false);
    }
    for(float value:{0.f,-1.f}){
        auto p=plant;p.local.radius=value;Compare(p,{},0,false);
        p=plant;p.uniformScale=value;Compare(p,{},0,false);
        for(unsigned axis=0;axis<3;++axis){auto box=high;Axis(box.extents,axis)=value;Compare(plant,std::span(&box,1),0,false);}
    }
    auto p=plant;p.rotation.x=.001f;Compare(p,{},0,false);
    p=plant;p.rotation.z=.001f;Compare(p,{},0,false);
    p=plant;p.local.maximumY=p.local.minimumY;Compare(p,{},0,false);
    p=plant;p.uniformScale=std::numeric_limits<float>::denorm_min();Compare(p,{},0,false);
    p=plant;p.root.x=std::numeric_limits<float>::max();Compare(p,{},0,false);
    p=plant;p.local.radius=2;p.uniformScale=std::numeric_limits<float>::max();Compare(p,{},0,false);
    Compare(plant,{},-1,false);Compare(plant,{},std::numeric_limits<float>::max(),false);
    auto box=high;box.center.x=std::numeric_limits<float>::max();Compare(plant,std::span(&box,1),0,false);
    std::vector<BoxCollider3D> tooMany(10001,high);
    ImmutableVolumeBoundsCache excessive(tooMany);
    p=plant;p.root.x=nan;
    Require(!excessive.Clears(p)&&!excessive.Clears(plant)&&excessive.StorageParts()==0&&excessive.ComputedParts()==0,"OVERSIZED_SOURCE_MUST_NOT_ALLOCATE");

    // 原helper的prefix短路保留，壞尾端不是入口預驗的solids，不能提早求bounds。
    box=high;box.extents.x=0;
    const std::array<BoxCollider3D,2> prefix{{overlap,box}};
    ImmutableVolumeBoundsCache lazy(prefix);
    Require(!lazy.Clears(plant,0)&&lazy.ComputedParts()==1&&lazy.StorageParts()==2,"EARLY_OVERLAP_MUST_NOT_VISIT_BAD_TAIL");
    auto separatedPlant=plant;separatedPlant.root.x=20;
    Require(!ClearsCanopyWood(separatedPlant,prefix,0)&&!lazy.Clears(separatedPlant,0)&&lazy.ComputedParts()==2,"BAD_TAIL_REACHED_IN_ORIGINAL_ORDER");
    Require(!lazy.Clears(separatedPlant,0)&&lazy.ComputedParts()==2,"INVALID_PART_MUST_BE_MEMOIZED");
    const std::array<BoxCollider3D,3> ordered{{overlap,{{10,.5f,0},{.5f,.5f,.5f}},{{20,.5f,0},{.5f,.5f,.5f}}}};
    ImmutableVolumeBoundsCache order(ordered);
    Require(!order.Clears(plant,0)&&order.ComputedParts()==1,"PREFIX1_ORDER");
    separatedPlant.root.x=10;Require(!order.Clears(separatedPlant,0)&&order.ComputedParts()==2,"PREFIX2_ORDER");
    separatedPlant.root.x=20;Require(!order.Clears(separatedPlant,0)&&order.ComputedParts()==3,"PREFIX3_ORDER");
    separatedPlant.root.x=30;Require(order.Clears(separatedPlant,0)&&order.ComputedParts()==3,"CACHED_ALL_SEPARATED");
    auto badPlant=plant;badPlant.local.radius=0;
    ImmutableVolumeBoundsCache unused(ordered);
    Require(!unused.Clears(badPlant)&&unused.StorageParts()==0&&unused.ComputedParts()==0,"PLANT_INVALID_BEFORE_CACHE_STORAGE");
    // 新來源只能新建owner，不修改已借用來源後沿用舊cache。
    const BoxCollider3D different{{40,4,0},{1,.5f,1}};
    ImmutableVolumeBoundsCache fresh(std::span(&different,1));
    Require(fresh.Clears(plant,0)&&fresh.ComputedParts()==1&&order.ComputedParts()==3,"FRESH_SOURCE_MUST_NOT_REUSE_OLD_OWNER");
}
template<class F>void Throws(F&& f,std::string_view expected){
    bool hit=false;try{f();}catch(const std::invalid_argument& e){hit=e.what()==expected;}
    Require(hit,"ENTRY_EXCEPTION_REASON_AND_ORDER");
}
void EntryControls(){
    const auto terrain=CreateTerrain();ForestUnderstory understory;
    std::array<ForestCoverAsset,5> assets;assets.fill({GrassMinimumRadius,GrassAssetHeight,0,GrassAssetHeight});
    ForestCoverSettings zero;zero.targetPlants=0;
    const BoxCollider3D valid{{100,100,100},{1,1,1}},invalid{{100,100,100},{0,1,1}};
    auto p=zero;p.targetPlants=10001;
    Throws([&]{(void)CreateForestCover(terrain,{},understory,{},assets,p,std::span(&invalid,1),false);},"Invalid canopy wood parts for grass");
    Throws([&]{(void)CreateForestCover(terrain,{},understory,{},assets,p);},"Invalid forest cover settings");
    auto invalidAssets=assets;invalidAssets[0].radius=std::numeric_limits<float>::quiet_NaN();
    Throws([&]{(void)CreateForestCover(terrain,{},understory,{},invalidAssets,zero);},"Invalid normalized grass asset footprint");
    invalidAssets=assets;invalidAssets[0].minimumY=std::numeric_limits<float>::quiet_NaN();
    const auto noWood=CreateForestCover(terrain,{},understory,std::span(&invalid,1),invalidAssets,zero,{},true);
    Require(noWood.reachedBudget&&noWood.plants.empty()&&noWood.patches.empty(),"ZERO_NO_WOOD_MUST_NOT_VALIDATE_UNUSED_FULL_SOLID_OR_LOCAL_VOLUME");
    Throws([&]{(void)CreateForestCover(terrain,{},understory,{},invalidAssets,zero,std::span(&valid,1),false);},"Invalid normalized grass full volume");
    for(bool full:{false,true}){
        const auto empty=CreateForestCover(terrain,{},understory,std::span(&invalid,1),assets,zero,{},full);
        Require(empty.reachedBudget&&empty.plants.empty()&&empty.patches.empty()&&!empty.rejectedClearance&&!empty.rejectedTerrain&&!empty.rejectedSpacing,"ZERO_RESULT_ALL_COUNTERS_UNCHANGED");
    }
}
}
int main(){
    try{HelperControls();EntryControls();if(checks!=344)throw std::runtime_error("FORMAL_HELPER_ENTRY_CHECK_COUNT");std::cout<<"IMMUTABLE_VOLUME_BOUNDS_HELPER_ENTRY_CPU_PASS checks="<<checks<<" helper_entry=ONLY positive_target_full_flag=NOT_THIS_TEST full_world=NOT_THIS_TEST original_token_controls=UNCHANGED_SEPARATE\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<" checks="<<checks<<'\n';return 1;}
}
