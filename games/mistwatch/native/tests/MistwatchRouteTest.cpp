#include "MistwatchWorld.h"
#include "MistwatchEncounter.h"
#include <Aevrynd/CharacterController3D.h>
#include <iostream>

int main() {
    using namespace Mistwatch;
    Session game;
    Encounter enemy;
    Aevrynd::CharacterController3D actor;
    const auto terrain=CreateTerrain();
    auto solids=WorldSolids(false);
    unsigned ticks{};
    actor.Reset({});
    const auto tick=[&](Aevrynd::Vec3 move,bool attack=false) {
        actor.Step({move,false,1},solids,terrain,1.f/60.f);
        game.feet=actor.Position();
        ++ticks;
        if(game.feet.y< -5) game.Damage(100);
        if(game.CurrentStage()==Stage::Encounter) {
            game.Damage(enemy.Tick(game.feet,1.f/60.f,attack));
            if(!enemy.health) game.FinishEncounter(enemy.feet);
        }
    };
    const auto walk=[&](Aevrynd::Vec3 destination) {
        for(unsigned step=0;step<1800;++step) {
            auto delta=destination-game.feet;delta.y=0;
            if(Aevrynd::Length(delta)<.15f) return game.Alive();
            tick(Aevrynd::Normalize(delta));
            if(!game.Alive()) return false;
        }
        std::cerr<<"Route stalled at "<<game.feet.x<<","<<game.feet.y<<","<<game.feet.z<<'\n';
        return false;
    };
    for(unsigned index=0;index<Session::supplies.size();++index) {
        const auto supply=Session::supplies[index];
        // 沿主路前進，再從橫向支線接近補給，全部經過真實碰撞。
        if(!walk({0,0,game.feet.z})||!walk({0,0,supply.z})||!walk(supply)||!game.Collect(index)) return 1;
    }
    if(!walk({0,0,game.feet.z})||!walk(Session::bridge)||!game.RepairBridge()) return 2;
    solids=WorldSolids(true);
    const auto checkpoint=game.Serialize();
    Session restored;
    if(!restored.Restore(checkpoint)||restored.CurrentStage()!=Stage::Encounter) return 3;
    if(!walk({0,0,45.5f})) return 4;
    for(unsigned step=0;step<600 && game.CurrentStage()==Stage::Encounter;++step) tick({},true);
    if(!game.Alive()||!game.guardianDefeated) return 5;
    if(!walk({0,0,Session::fuse.z})||!walk(Session::fuse)||!game.CollectFuse()) return 7;
    if(!walk({0,0,Session::fuse.z})||!walk({0,0,Session::generator.z})||!walk(Session::generator+Aevrynd::Vec3{-1.6f,0,0})) return 8;
    if(game.RepairGenerator()||!game.ToggleCircuit(0)||!game.ToggleCircuit(2)||!game.RepairGenerator()) return 10;
    if(!walk({0,0,Session::generator.z})) return 9;
    if(!walk(Session::beacon)||!game.LightBeacon()||game.CurrentStage()!=Stage::Complete) return 6;
    std::cout<<"Complete collision-backed route PASS; seconds="<<float(ticks)/60.f<<" health="<<game.health<<'\n';
}
