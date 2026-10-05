#include "MistwatchEncounterMotion.h"
#include "MistwatchBoar.h"
#include "MistwatchWorld.h"
#include "MistwatchLoadPreparation.h"
#include <iostream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <cstring>

namespace {
using namespace Mistwatch;
using namespace Aevrynd;
unsigned checks{},terrainTicks{},collisionTicks{},poseTicks{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Same(const Encounter& a,const Encounter& b){return Same(a.feet,b.feet)&&Same(a.strikeTarget,b.strikeTarget)&&
    a.health==b.health&&a.phase==b.phase&&a.phaseTime==b.phaseTime&&a.playerCooldown==b.playerCooldown&&a.countered==b.countered;}
HeightField3D Flat(){HeightField3D t({{-20,0,-20},81,161,.5f});t.Fill(0);return t;}
SaveGame Battle(){SaveGame s;s.session.collected=7;s.session.repairMaterials=7;s.session.bridgeRepaired=true;s.session.feet=Session::arena;
    Require(s.Valid(),"battle snapshot fixture invalid");return s;}
void TerrainPursuit(){
    const auto terrain=CreateTerrain();Encounter enemy,original;
    Boar boar;SkinningWorkspace3D workspace;Mesh3D posed;
    float maximumRootError{},oldRootError{},maximumFootError{},maximumRadius{},maximumTop{},minimumBottom{},bodyTerrainMinimum=1000;unsigned windups{};
    for(unsigned tick=0;tick<1500;++tick){
        // 與保留的原版重現完全相同地形、目標公式和 tick 數；不提高 .2m 舊門檻。
        const float x=tick<240?0.f:(std::min)(18.f,enemy.feet.x+3.f);
        const Vec3 player{x,terrain.SampleHeight(x,55),55};
        original.Tick(player,1.f/60.f,false);
        oldRootError=(std::max)(oldRootError,std::abs(original.feet.y-terrain.SampleHeight(original.feet.x,original.feet.z)));
        const auto prior=enemy.feet;const auto phase=enemy.phase;
        const auto update=TickEncounterWorld(enemy,player,1.f/60.f,false,{},terrain);
        Require(update.status==EncounterMotionStatus::Applied,"clear terrain movement invalid or blocked");
        const auto error=std::abs(enemy.feet.y-terrain.SampleHeight(enemy.feet.x,enemy.feet.z));
        maximumRootError=(std::max)(maximumRootError,error);Require(error<=.0001f,"terrain root support differs");
        Require(Length(enemy.feet-prior)<.25f,"terrain pursuit made an unbounded step");
        windups+=phase!=EnemyPhase::Windup&&enemy.phase==EnemyPhase::Windup;
        Require(boar.Update(enemy.feet-prior,1.f/60.f,enemy.phase,(std::max)(0.f,enemy.phaseTime),
            (enemy.phase==EnemyPhase::Windup?enemy.strikeTarget:player)-enemy.feet)&&boar.PlaceFeet(enemy.feet,terrain),"terrain pose rejected");
        const auto world=CreateTransformMatrix(enemy.feet,{0,boar.yaw,0},{1,1,1});
        for(unsigned leg=0;leg<4;++leg){
            const auto ankle=TransformPoint(world*boar.pose.globalMatrices[5+leg*3],{});
            maximumFootError=(std::max)(maximumFootError,Length(ankle-boar.footTargets[leg]));
        }
        Require(SkinModelInto(boar.model,boar.pose,workspace,posed),"actual terrain skinning failed");
        for(std::size_t vertex=0;vertex<posed.vertices.size();++vertex){
            const auto& v=posed.vertices[vertex];
            Require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"posed mesh nonfinite");
            maximumRadius=(std::max)(maximumRadius,std::hypot(v.x,v.z));maximumTop=(std::max)(maximumTop,v.y);
            minimumBottom=(std::min)(minimumBottom,v.y);
            const auto joint=boar.model.influences[vertex].joints[0];
            if(joint<3||joint==15){const auto p=TransformPoint(world,{v.x,v.y,v.z});
                bodyTerrainMinimum=(std::min)(bodyTerrainMinimum,p.y-terrain.SampleHeight(p.x,p.z));}
            Require(std::abs(v.x)<=EncounterBodyRadius&&std::abs(v.z)<=EncounterBodyRadius&&v.y>=EncounterBodyBottom&&
                v.y<=EncounterBodyBottom+EncounterBodyHeight,"posed animal outside conservative body");
        }
        ++terrainTicks;++poseTicks;
    }
    Require(oldRootError>.2f,"original root divergence no longer reproduced");
    Require(enemy.feet.x>15&&enemy.feet.y>1.5f,"corrected pursuit did not reach original uphill region");
    std::cout<<std::setprecision(9)<<"TERRAIN ticks="<<terrainTicks<<" old_root_error="<<oldRootError<<" new_root_error="<<maximumRootError
        <<" foot_error="<<maximumFootError<<" mesh_radius="<<maximumRadius<<" mesh_top="<<maximumTop<<" mesh_bottom="<<minimumBottom
        <<" body_terrain_min="<<bodyTerrainMinimum<<" windups="<<windups<<'\n';
    Require(maximumFootError<=.005f,"actual slope foot IK error exceeds existing tolerance");
    Require(windups>0,"corrected pursuit never reaches combat windup");
    Require(bodyTerrainMinimum>=-.02f,"actual torso/head penetrates terrain");
    // 同一真坡地向下行走，不靠強制重設到低地。
    for(unsigned i=0;i<360;++i){const auto prior=enemy.feet;const auto result=ResolveEncounterMove(prior,{-.025f,0,0},{},terrain);
        Require(result.status==EncounterMotionStatus::Applied&&EncounterSupported(result.feet,terrain),"downhill terrain support failed");enemy.feet=result.feet;
        Require(boar.Update(enemy.feet-prior,1.f/60.f,EnemyPhase::Pursuit,0,{-1,0,0})&&boar.PlaceFeet(enemy.feet,terrain),"downhill pose rejected");
        const auto world=CreateTransformMatrix(enemy.feet,{0,boar.yaw,0},{1,1,1});
        for(unsigned leg=0;leg<4;++leg)Require(Length(TransformPoint(world*boar.pose.globalMatrices[5+leg*3],{})-boar.footTargets[leg])<=.005f,"downhill foot endpoint out of tolerance");
        Require(SkinModelInto(boar.model,boar.pose,workspace,posed),"downhill skinning failed");
        for(const auto& v:posed.vertices)Require(std::abs(v.x)<=EncounterBodyRadius&&std::abs(v.z)<=EncounterBodyRadius&&
            v.y>=EncounterBodyBottom&&v.y<=EncounterBodyBottom+EncounterBodyHeight,"downhill full mesh outside collision envelope");
        ++poseTicks;
    }
}
void Collision(){
    const auto terrain=Flat();const Vec3 start{0,0,48};
    // 薄牆、樹幹及倒木；實際控制器逐步解算，不能跨越或在終点重疊。
    const std::array obstacles{BoxCollider3D{{3,.65f,48},{.025f,.65f,5}},
        BoxCollider3D{{3,.65f,48},{.18f,.65f,.18f}},BoxCollider3D{{3,.15f,48},{.5f,.15f,2}}};
    for(const auto& obstacle:obstacles){
        const std::array solid{obstacle};Vec3 feet=start;unsigned blocked{};
        for(unsigned i=0;i<120;++i){const auto result=ResolveEncounterMove(feet,{.1f,0,0},solid,terrain);
            Require(result.status!=EncounterMotionStatus::Invalid,"legal obstacle movement invalid");feet=result.feet;
            blocked+=result.status==EncounterMotionStatus::Blocked;
            Require(!EncounterBodyOverlaps(feet,solid)&&EncounterSupported(feet,terrain),"body crossed thin obstacle");++collisionTicks;}
        Require(blocked>0&&feet.x<=obstacle.center.x-obstacle.extents.x-EncounterBodyRadius+.0001f,"obstacle failed to stop animal nose");
    }
    const std::array wall{obstacles[0]};Vec3 feet=start;
    for(unsigned i=0;i<60;++i){const auto result=ResolveEncounterMove(feet,{.1f,0,.05f},wall,terrain);
        Require(result.status!=EncounterMotionStatus::Invalid&&!EncounterBodyOverlaps(result.feet,wall),"diagonal wall slide penetrated");feet=result.feet;}
    Require(feet.z>50.5f&&feet.x<1.68f,"wall slide lost unblocked axis");
    auto slope=Flat();
    for(unsigned z=0;z<slope.Settings().depth;++z)for(unsigned x=0;x<slope.Settings().width;++x){const auto p=slope.SamplePosition(x,z);
        Require(slope.SetHeight(x,z,.2f*p.x+.1f*(p.z-48)),"slope collision fixture failed");}
    feet=start;unsigned slopeHits{};
    const std::array slopeWall{BoxCollider3D{{3,1,48},{.025f,2,10}}};
    for(unsigned i=0;i<60;++i){const auto r=ResolveEncounterMove(feet,{.1f,0,.05f},slopeWall,slope);
        Require(r.status!=EncounterMotionStatus::Invalid&&!EncounterBodyOverlaps(r.feet,slopeWall)&&EncounterSupported(r.feet,slope),"slope slide height correction penetrated");
        feet=r.feet;slopeHits+=r.collisions;}
    Require(feet.z>50.5f&&feet.x<1.68f&&slopeHits>0,"slope slide did not reach and follow wall");
    const std::array ceiling{BoxCollider3D{{0,1.41f,48},{10,.1f,10}}};feet=start;
    for(unsigned i=0;i<6;++i){const auto r=ResolveEncounterMove(feet,{.1f,0,0},ceiling,slope);
        Require(r.status!=EncounterMotionStatus::Invalid&&EncounterSupported(r.feet,slope)&&!EncounterBodyOverlaps(r.feet,ceiling),"blocked vertical correction did not preserve safe support");feet=r.feet;}
    Require(feet.x<.051f,"animal body crossed slope ceiling");
    Encounter blocked;blocked.playerCooldown=.5f;
    const std::array touching{BoxCollider3D{{EncounterBodyRadius+.05f,.65f,48},{.05f,.65f,5}}};
    const auto update=TickEncounterWorld(blocked,{4,0,48},.1f,false,touching,terrain);
    Require(update.status==EncounterMotionStatus::Blocked&&Same(blocked.feet,start)&&std::abs(blocked.playerCooldown-.4f)<.00001f,"blocked pursuit failed timer semantics");
    for(const auto dt:{0.f,-.1f,.11f,(std::numeric_limits<float>::quiet_NaN)()}){
        const auto before=blocked;Require(TickEncounterWorld(blocked,{4,0,48},dt,true,{},terrain).status==EncounterMotionStatus::Invalid&&Same(before,blocked),"invalid dt changed whole encounter");}
    const auto before=blocked;
    const std::array overlap{BoxCollider3D{{0,.65f,48},{.1f,.65f,.1f}}};
    Require(TickEncounterWorld(blocked,{4,0,48},.1f,false,overlap,terrain).status==EncounterMotionStatus::Invalid&&Same(before,blocked),"start overlap advanced state");
    Require(ResolveEncounterMove({18.69f,0,48},{.1f,0,0},{},terrain).status==EncounterMotionStatus::Blocked,"terrain boundary did not block whole body");
    Require(ResolveEncounterMove({20,0,48},{},{},terrain).status==EncounterMotionStatus::Invalid,"edge spawn with missing foot terrain accepted");
    Require(ResolveEncounterMove(start,{1,0,0},{},terrain).status==EncounterMotionStatus::Invalid,"unbounded request accepted");
    auto corrupt=terrain;corrupt.Fill((std::numeric_limits<float>::quiet_NaN)());
    Require(TickEncounterWorld(blocked,{4,0,48},.1f,false,{},corrupt).status==EncounterMotionStatus::Invalid&&Same(before,blocked),"nonfinite terrain advanced encounter");
    const std::array badBox{BoxCollider3D{{4,.65f,48},{(std::numeric_limits<float>::quiet_NaN)(),.65f,1}}};
    Require(TickEncounterWorld(blocked,{4,0,48},.1f,false,badBox,terrain).status==EncounterMotionStatus::Invalid&&Same(before,blocked),"nonfinite collider advanced encounter");
    for(unsigned kind=0;kind<5;++kind){auto invalid=before;
        if(kind==0){invalid.phase=EnemyPhase::Windup;invalid.phaseTime=(std::numeric_limits<float>::quiet_NaN)();}
        if(kind==1)invalid.health=4;
        if(kind==2)invalid.phase=static_cast<EnemyPhase>(99);
        if(kind==3)invalid.strikeTarget.x=(std::numeric_limits<float>::infinity)();
        if(kind==4)invalid.countered=true;
        const auto prior=invalid;Require(TickEncounterWorld(invalid,{4,0,48},.1f,true,{},terrain).status==EncounterMotionStatus::Invalid,
            "invalid combat state accepted");
        // NaN 不使用 == 比較；拒絕後逐位元保存整筆 Encounter。
        Require(std::memcmp(&prior,&invalid,sizeof(Encounter))==0,"invalid combat state partially advanced");
    }
}
void FlatParity(){
    const auto terrain=Flat();Encounter old,current;
    for(unsigned tick=0;tick<600;++tick){
        const Vec3 player=tick<90?Vec3{0,0,43}:current.feet;
        const bool attack=current.phase==EnemyPhase::Recovery&&!current.countered&&current.playerCooldown<=0;
        const auto damage=old.Tick(player,.02f,attack);const auto result=TickEncounterWorld(current,player,.02f,attack,{},terrain);
        Require(result.status==EncounterMotionStatus::Applied&&damage==result.damage&&Same(old,current),"flat combat timing/damage changed");
    }
    Require(current.health==0&&current.phase==EnemyPhase::Defeated,"flat parity missed three counters");
}
void SavedSlope(){
    const auto terrain=CreateTerrain();auto save=Battle();
    save.encounter.feet={18,terrain.SampleHeight(18,55),55};
    CharacterController3D controller;Explorer explorer;Boar boar;OrbitCamera orbit;
    const LoadPreparationInputs inputs{controller,explorer,boar,orbit};
    for(const auto phase:{EnemyPhase::Pursuit,EnemyPhase::Windup,EnemyPhase::Recovery,EnemyPhase::Defeated}){
        save.encounter.phase=phase;save.encounter.phaseTime=phase==EnemyPhase::Windup?.4f:phase==EnemyPhase::Recovery?.7f:0;
        save.encounter.countered=phase==EnemyPhase::Defeated;save.encounter.health=phase==EnemyPhase::Defeated?0:3;
        save.session.guardianDefeated=phase==EnemyPhase::Defeated;
        const auto serialized=save.Serialize();SaveGame copy;
        Require(serialized.starts_with("MISTWATCH_SAVE 2")&&copy.Restore(serialized)&&Same(copy.encounter,save.encounter),"slope snapshot roundtrip differs");
        const auto result=PrepareSavedGame(copy,inputs,terrain);
        if(!result)std::cerr<<"slope load error="<<unsigned(result.error)<<" phase="<<unsigned(phase)<<'\n';
        Require(bool(result)&&Same(result.candidate->snapshot.encounter,save.encounter),"slope world precheck rejected reachable snapshot");
        for(const float height:{-100.f,100.f}){auto bad=copy;bad.encounter.feet.y=height;Require(bad.Valid(),"finite v2 malformed world fixture structurally invalid");
            const auto rejected=PrepareSavedGame(bad,inputs,terrain);Require(!rejected&&rejected.error==LoadPreparationError::BoarUnsupported,"unsupported enemy loaded");}
    }
    save=Battle();const auto legacy=save.Serialize();auto v1=legacy;v1[15]='1';SaveGame migrated;
    Require(migrated.Restore(v1)&&Same(migrated.encounter,save.encounter)&&bool(PrepareSavedGame(migrated,inputs,terrain)),"normal released v1 save rejected");
    const std::array extra{BoxCollider3D{{0,.65f,48},{.1f,.65f,.1f}}};
    save.session.feet=Session::bridge;
    const auto rejected=PrepareSavedGame(save,inputs,terrain,{},extra);
    Require(!rejected&&rejected.error==LoadPreparationError::BoarBodyOverlap,"enemy inside solid loaded");
    auto buried=save;buried.encounter.feet={18,0,55};
    auto buriedLegacy=buried.Serialize();buriedLegacy[15]='1';
    Require(migrated.Restore(buriedLegacy),"legacy buried fixture did not retain original structural contract");
    const auto unsafe=PrepareSavedGame(migrated,inputs,terrain);
    Require(!unsafe&&unsafe.error==LoadPreparationError::BoarUnsupported&&Same(migrated.encounter.feet,buried.encounter.feet),"unsafe legacy checkpoint silently relocated");
    auto peak=terrain;Require(peak.SetHeight(102,136,3),"peak fixture write failed");
    Require(peak.SampleHeight(0,48)==0,"peak fixture accidentally changes root height");
    const auto pointed=PrepareSavedGame(save,inputs,peak);
    Require(!pointed&&pointed.error==LoadPreparationError::BoarTerrainAboveBodyTop,"finite neighboring peak through animal accepted");
}
}
int main(){try{TerrainPursuit();Collision();FlatParity();SavedSlope();
    std::cout<<"ENCOUNTER_MOTION_PASS checks="<<checks<<" collision_ticks="<<collisionTicks<<" pose_ticks="<<poseTicks<<" native_input=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<" checks="<<checks<<" terrain_ticks="<<terrainTicks<<'\n';return 1;}}
