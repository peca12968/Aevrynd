#include "MistwatchEvasion.h"
#include "MistwatchEncounter.h"
#include "MistwatchWorld.h"
#include <Aevrynd/CharacterController3D.h>
#include <iostream>
#include <limits>
int main() {
    using namespace Mistwatch;
    using namespace Aevrynd;
    Evasion dodge;
    const auto terrain=CreateTerrain();
    CharacterController3D player;
    const std::vector<BoxCollider3D> empty;
    player.Reset(Session::arena);player.Step({},empty,terrain,1.f/60.f);
    Encounter enemy;
    enemy.Tick(player.Position(),1.f/60.f,false);
    unsigned damage=0;
    for(unsigned tick=0;tick<60;++tick) {
        const auto motion=dodge.Tick(1.f/60.f,tick==0,player.Grounded(),false,tick==0?Vec3{1,0,0}:Vec3{-1,0,0});
        player.Step({motion.dashing?motion.movement:Vec3{},false,motion.dashing?Evasion::SpeedMultiplier:1.f},empty,terrain,1.f/60.f);
        damage+=enemy.Tick(player.Position(),1.f/60.f,false);
    }
    const float distance=player.Position().x;
    if(damage || std::abs(distance-Evasion::Duration*Evasion::SpeedMultiplier*player.Settings().moveSpeed)>.002f)return 1;
    if(dodge.Tick(.02f,true,true,false,{-1,0,0}).dashing)return 2;
    for(int tick=0;tick<20;++tick)dodge.Tick(.02f,false,true,false,{});
    if(!dodge.Tick(.02f,true,true,false,{-1,0,0}).dashing)return 3;
    dodge={};
    if(dodge.Tick(.02f,true,false,false,{1,0,0}).dashing ||
       dodge.Tick(.02f,true,true,true,{1,0,0}).dashing ||
       dodge.Tick(.02f,true,true,false,{}).dashing)return 4;
    const auto snapshot=dodge;
    dodge.Tick(std::numeric_limits<float>::quiet_NaN(),true,true,false,{1,0,0});
    dodge.Tick(.02f,true,true,false,{std::numeric_limits<float>::infinity(),0,0});
    if(dodge.remaining!=snapshot.remaining || dodge.cooldown!=snapshot.cooldown)return 5;
    // 同一閃避撞上牆時停在碰撞邊界；不能因閃避狀態而免傷。
    const std::vector<BoxCollider3D> wall{{Session::arena+Vec3{1,.9f,0},{.1f,.9f,3}}};
    player.Reset(Session::arena);player.Step({},wall,terrain,1.f/60.f);
    enemy={};enemy.Tick(player.Position(),1.f/60.f,false);damage=0;dodge={};
    for(unsigned tick=0;tick<60;++tick) {
        const auto motion=dodge.Tick(1.f/60.f,tick==0,player.Grounded(),false,{1,0,0});
        player.Step({motion.dashing?motion.movement:Vec3{},false,motion.dashing?Evasion::SpeedMultiplier:1.f},wall,terrain,1.f/60.f);
        damage+=enemy.Tick(player.Position(),1.f/60.f,false);
    }
    if(player.Position().x>.46f || player.Stats().collisions==0 || damage!=20)return 6;
    // 完整反擊回圈使用真實控制器走位，不傳送玩家；三次閃避後接近反擊。
    enemy={};dodge={};damage=0;
    player.Reset(Session::arena+Vec3{0,0,-1.5f});player.Step({},empty,terrain,1.f/60.f);
    unsigned combatTicks=0,evades=0;
    for(;combatTicks<1200 && enemy.health;++combatTicks) {
        Vec3 movement{};
        bool request=false;
        if(enemy.phase==EnemyPhase::Windup) {movement={1,0,0};request=dodge.cooldown<=0;}
        else {
            const auto delta=enemy.feet-player.Position();
            if(Length(delta)>1.5f)movement=Normalize(delta);
        }
        const auto motion=dodge.Tick(1.f/60.f,request,player.Grounded(),false,movement);
        if(request && motion.dashing)++evades;
        player.Step({motion.movement,false,motion.dashing?Evasion::SpeedMultiplier:1.f},empty,terrain,1.f/60.f);
        damage+=enemy.Tick(player.Position(),1.f/60.f,!motion.dashing);
    }
    if(enemy.health || damage || evades!=3)return 7;
    std::cout<<"Collision-backed evasion PASS; open distance="<<distance<<" blocked damage=20; zero-damage encounter ticks="<<combatTicks<<" evades="<<evades<<'\n';
}
