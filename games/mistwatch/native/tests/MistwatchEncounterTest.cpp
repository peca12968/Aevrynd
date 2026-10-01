#include "MistwatchEncounter.h"
#include <iostream>
#include <limits>
int main(){
    Mistwatch::Encounter protectedEnemy;
    protectedEnemy.Tick(Mistwatch::Session::arena,.02f,true);
    if(protectedEnemy.health!=3) {
        std::cerr<<"FIRST_DIVERGENCE: guardian takes damage before a recovery opening\n";
        return 6;
    }
    Mistwatch::Encounter enemy;
    auto player=Mistwatch::Session::arena;
    if(enemy.Tick(player,.02f,false)!=0 || enemy.phase!=Mistwatch::EnemyPhase::Windup) return 1;
    unsigned damage=0;
    player.x+=5;
    for(int i=0;i<50;++i) damage+=enemy.Tick(player,.02f,false);
    if(damage || enemy.phase!=Mistwatch::EnemyPhase::Recovery) return 2;
    enemy={};player=Mistwatch::Session::arena;
    for(int i=0;i<50;++i) damage+=enemy.Tick(player,.02f,false);
    if(damage!=20) return 3;
    enemy={};
    for(int strike=0;strike<3;++strike) {
        for(int tick=0;tick<200 && enemy.phase!=Mistwatch::EnemyPhase::Recovery;++tick)enemy.Tick(player,.02f,true);
        if(enemy.phase!=Mistwatch::EnemyPhase::Recovery)return 12;
        const auto previous=enemy.health;
        enemy.Tick(player,.02f,true);
        if(enemy.health!=previous-1) return 4;
        if(enemy.health) {
            for(int i=0;i<40;++i)enemy.Tick(player,.02f,true);
            if(enemy.health!=previous-1)return 7;
            for(int tick=0;tick<200 && enemy.phase==Mistwatch::EnemyPhase::Recovery;++tick)enemy.Tick(player,.02f,false);
            if(enemy.phase==Mistwatch::EnemyPhase::Recovery)return 13;
        }
    }
    if(enemy.health || enemy.phase!=Mistwatch::EnemyPhase::Defeated || enemy.Tick(player,.02f,true)) return 5;
    enemy={};player=Mistwatch::Session::arena;player.x+=2;
    enemy.Tick(player,.02f,false);
    const auto target=enemy.strikeTarget;
    player.x-=3;
    damage=0;
    for(int i=0;i<45;++i)damage+=enemy.Tick(player,.02f,false);
    if(damage || Aevrynd::Length(enemy.strikeTarget-target)>.001f)return 8;
    // 玩家仍在敵人兩公尺內，但離開固定預警圈後不受傷。
    if(std::hypot(player.x-enemy.feet.x,player.z-enemy.feet.z)>2.2f)return 9;
    enemy={};player=Mistwatch::Session::arena;enemy.Tick(player,.02f,false);
    player.y+=2.1f;damage=0;
    for(int i=0;i<45;++i)damage+=enemy.Tick(player,.02f,false);
    if(damage)return 10;
    const auto before=enemy.phaseTime;
    enemy.Tick(player,std::numeric_limits<float>::quiet_NaN(),true);
    if(enemy.phaseTime!=before)return 11;
    enemy={};player={0,0,43};unsigned windups=0;
    for(unsigned tick=0;tick<300;++tick) {
        enemy.Tick(player,1.f/60.f,false);
        if(enemy.phase==Mistwatch::EnemyPhase::Windup)++windups;
    }
    if(!windups) {
        std::cerr<<"FIRST_DIVERGENCE: pursuit stalls outside attack trigger at distance "
            <<std::hypot(player.x-enemy.feet.x,player.z-enemy.feet.z)<<'\n';return 14;
    }
    std::cout<<"Fixed strike target, recovery-only counter, one hit per opening and defeat PASS\n";
}
