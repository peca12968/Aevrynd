#include "MistwatchEncounterMotion.h"
#include "MistwatchWorld.h"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
using namespace Aevrynd;using namespace Mistwatch;
unsigned checks{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
HeightField3D Flat(){HeightField3D t({{-20,0,-20},81,161,.5f});t.Fill(0);return t;}
Encounter Recover(Vec3 feet=Session::arena){Encounter e;e.feet=feet;e.phase=EnemyPhase::Recovery;e.phaseTime=.5f;return e;}
Encounter Strike(Vec3 target){Encounter e;e.phase=EnemyPhase::Windup;e.phaseTime=.01f;e.strikeTarget=target;return e;}
void Cover(){
    const auto terrain=Flat();const std::array wall{BoxCollider3D{{1.55f,1,48},{.1f,1,5}}};const Vec3 player{2.15f,0,48};
    Encounter enemy;Need(!EncounterBodyOverlaps(enemy.feet,wall),"enemy overlaps cover fixture");
    const auto windup=TickEncounterWorld(enemy,player,.02f,false,wall,terrain);
    Need(windup.status==EncounterMotionStatus::Applied&&enemy.phase==EnemyPhase::Pursuit,"windup starts through wall");
    enemy=Recover();const auto hp=enemy.health;
    TickEncounterWorld(enemy,player,.02f,true,wall,terrain);
    Need(enemy.health==hp&&!enemy.countered&&enemy.playerCooldown==0,"blocked counter consumes HP/cooldown");
    TickEncounterWorld(enemy,player,.02f,true,{},terrain);
    Need(enemy.health==2&&enemy.countered&&enemy.playerCooldown==.65f,"blocked counter swallows next clear attack");
    for(const auto targets:{std::pair{player,player},std::pair{Vec3{.9f,0,48},player},std::pair{player,Vec3{.9f,0,48}}}){
        enemy=Strike(targets.first);const auto result=TickEncounterWorld(enemy,targets.second,.02f,false,wall,terrain);
        Need(result.damage==0&&enemy.phase==EnemyPhase::Recovery&&enemy.phaseTime==Encounter::RecoveryDuration,"blocked strike skipped recovery or damaged through cover");
    }
    // 清空遮擋後仍保留原傷害數值。
    enemy=Strike(player);Need(TickEncounterWorld(enemy,player,.02f,false,{},terrain).damage==20,"clear strike balance changed");
    const std::array behind{BoxCollider3D{{3,1,48},{.05f,1,5}}};enemy=Recover();
    TickEncounterWorld(enemy,player,.02f,true,behind,terrain);Need(enemy.health==2&&enemy.countered,"wall beyond finite segment blocks counter");
    const std::array low{BoxCollider3D{{1.55f,.1f,48},{.1f,.1f,5}}};enemy=Recover();
    TickEncounterWorld(enemy,player,.02f,true,low,terrain);Need(enemy.health==2,"low floor prop falsely blocks torso line");
    enemy=Recover();TickEncounterWorld(enemy,{2.15f,1.5f,48},.02f,true,wall,terrain);Need(enemy.health==3,"jump enables counter through tall cover");
    enemy=Recover();TickEncounterWorld(enemy,{2.15f,2.f,48},.02f,true,{},terrain);Need(enemy.health==3,"vertical reach restriction changed");
}
void ChangingCover(){
    const auto terrain=Flat();const Vec3 player{2.15f,0,48};
    const std::array wall{BoxCollider3D{{1.55f,1,48},{.1f,1,5}}};
    for(const bool addCover:{false,true}){
        Encounter enemy;
        Need(TickEncounterWorld(enemy,player,.02f,false,{},terrain).status==EncounterMotionStatus::Applied&&enemy.phase==EnemyPhase::Windup,
            "clear pursuit does not begin windup");
        unsigned damage{};
        for(unsigned i=0;i<43;++i){
            const auto cover=(i<20||addCover)?std::span<const BoxCollider3D>(wall):std::span<const BoxCollider3D>();
            const auto result=TickEncounterWorld(enemy,player,.02f,false,cover,terrain);
            Need(result.status==EncounterMotionStatus::Applied,"changing cover corrupts encounter motion");damage+=result.damage;
        }
        Need(enemy.phase==EnemyPhase::Recovery&&damage==(addCover?0u:20u),"impact uses cached cover instead of current world");
    }
    auto enemy=Recover();const auto before=enemy;
    auto corrupted=terrain;corrupted.SetHeight(43,136,std::numeric_limits<float>::quiet_NaN());
    const auto result=TickEncounterWorld(enemy,player,.02f,true,{},corrupted);
    Need(result.status==EncounterMotionStatus::Invalid&&result.damage==0&&enemy.health==before.health&&
        enemy.phase==before.phase&&enemy.phaseTime==before.phaseTime&&enemy.playerCooldown==before.playerCooldown&&
        enemy.countered==before.countered&&enemy.feet.x==before.feet.x&&enemy.feet.y==before.feet.y&&enemy.feet.z==before.feet.z,
        "invalid contact terrain partially mutates combat state");
    enemy=Strike({2.15f,0,48});const auto lockedBefore=enemy;
    corrupted=terrain;corrupted.SetHeight(44,136,std::numeric_limits<float>::quiet_NaN());
    const auto locked=TickEncounterWorld(enemy,{.9f,0,48},.02f,false,{},corrupted);
    Need(locked.status==EncounterMotionStatus::Invalid&&locked.damage==0&&enemy.health==lockedBefore.health&&
        enemy.phase==lockedBefore.phase&&enemy.phaseTime==lockedBefore.phaseTime&&enemy.playerCooldown==lockedBefore.playerCooldown&&
        enemy.countered==lockedBefore.countered&&enemy.feet.x==lockedBefore.feet.x&&enemy.feet.y==lockedBefore.feet.y&&
        enemy.feet.z==lockedBefore.feet.z&&enemy.strikeTarget.x==lockedBefore.strikeTarget.x&&
        enemy.strikeTarget.y==lockedBefore.strikeTarget.y&&enemy.strikeTarget.z==lockedBefore.strikeTarget.z,
        "invalid locked strike line commits impact transaction");
}
void ActualTree(){
    const auto terrain=CreateTerrain();Vec3 tree{};bool found{};
    for(const auto p:TreePositions())if(p.x>5&&p.x<5.3f&&p.z>48.7f&&p.z<49){tree=p;found=true;break;}
    Need(found,"fixed world tree fixture missing");
    const std::array trunk{BoxCollider3D{tree+Vec3{0,2,0},{.25f,2,.25f}}};
    auto enemy=Recover({tree.x-1.62f,terrain.SampleHeight(tree.x-1.62f,tree.z),tree.z});
    const Vec3 player{tree.x+.75f,terrain.SampleHeight(tree.x+.75f,tree.z),tree.z};
    Need(!EncounterBodyOverlaps(enemy.feet,trunk),"actual tree enemy fixture overlaps");
    auto old=enemy;old.Tick(player,.02f,true);
    const auto result=TickEncounterWorld(enemy,player,.02f,true,trunk,terrain);
    Need(old.health==2&&enemy.health==3&&result.status==EncounterMotionStatus::Applied,"actual fixed tree cover counter not corrected");
}
void TerrainCover(){
    auto terrain=Flat();terrain.SetHeight(43,136,2); // x=1.5,z=48；根及玩家仍在原平面。
    auto enemy=Recover();const Vec3 player{2.15f,terrain.SampleHeight(2.15f,48),48};
    Need(terrain.SampleHeight(enemy.feet.x,enemy.feet.z)==0&&player.y==0,"terrain cover fixture changes actors");
    const auto result=TickEncounterWorld(enemy,player,.02f,true,{},terrain);
    Need(result.status==EncounterMotionStatus::Applied&&enemy.health==3,"narrow terrain ridge permits counter");
}
}
int main(){try{Cover();ChangingCover();ActualTree();TerrainCover();std::cout<<"ENCOUNTER_SIGHT_PASS checks="<<checks<<" native_input=0\n";return 0;}
catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE "<<error.what()<<" checks="<<checks<<'\n';return 1;}}
