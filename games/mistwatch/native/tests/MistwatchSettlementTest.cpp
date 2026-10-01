#include "MistwatchWorld.h"
#include "MistwatchCamera.h"
#include <iostream>
int main() {
    Mistwatch::Session game;
    if(!game.Restore("MISTWATCH 4 7 100 1 1 0 -7 0 59 0 0 0 7"))return 2;
    if(game.CollectFuse()) {
        std::cerr<<"FIRST_DIVERGENCE: workshop fuse can be collected without investigating settlement or opening cabinet\n";
        return 1;
    }
    using namespace Mistwatch;
    Session early;
    early.feet=Session::rangerLog;
    if(early.ReadFuseClue()||early.CycleFuseDial(0))return 3;
    game.fuseDials=Session::solvedFuseDials;
    if(game.CollectFuse())return 4;
    game.fuseDials=0;
    game.feet=Session::fuse+Aevrynd::Vec3{1,0,0};
    if(game.CycleFuseDial(0))return 5;
    game.feet=Session::fuse+Aevrynd::Vec3{0,.36f,0};
    if(game.CycleFuseDial(0)||game.CollectFuse())return 6;
    game.feet=Session::fuse;
    if(game.CycleFuseDial(3)||game.FuseDial(3))return 7;
    for(unsigned turn=0;turn<4;++turn)if(!game.CycleFuseDial(0))return 8;
    if(game.fuseDials||!game.CycleFuseDial(0))return 9;
    Session loaded;
    if(!loaded.Restore(game.Serialize())||loaded.fuseDials!=1||loaded.fuseClueRead)return 10;
    const auto terrain=CreateTerrain();
    const auto solids=WorldSolids(true);
    Aevrynd::CharacterController3D actor;
    actor.Reset(Session::rangerLog+Aevrynd::Vec3{-4,0,0});
    for(unsigned tick=0;tick<100;++tick)actor.Step({{1,0,0},false,1},solids,terrain,1.f/60.f);
    game.feet=actor.Position();
    if(game.feet.x>Session::rangerLog.x+.101f||!game.NearInterior(Session::rangerLog)||actor.Stats().collisions==0)return 11;
    if(!game.ReadFuseClue()||game.ReadFuseClue())return 12;
    if(!loaded.Restore(game.Serialize())||!loaded.fuseClueRead||loaded.fuseDials!=1)return 13;
    game.Damage(100);
    if(game.CycleFuseDial(0)||game.ReadFuseClue()||game.CollectFuse())return 14;
    game.Retry();
    if(!game.fuseClueRead||game.fuseDials!=1||game.CurrentStage()!=Stage::Fuse)return 15;
    actor.Reset(Session::fuse+Aevrynd::Vec3{4,0,0});
    for(unsigned tick=0;tick<100;++tick)actor.Step({{-1,0,0},false,1},solids,terrain,1.f/60.f);
    game.feet=actor.Position();
    if(game.feet.x<Session::fuse.x-.101f||!game.NearInterior(Session::fuse)||actor.Stats().collisions==0)return 16;
    Mistwatch::OrbitCamera orbit;orbit.yaw=1.25f;
    const auto camera=BuildPlayerCamera(game.feet,1.8f,orbit,solids,terrain);
    if(!Aevrynd::ProjectToViewport(game.feet+Aevrynd::Vec3{0,1,0},camera,{1280,720}).visible)return 17;
    game.fuseDials=0;
    const auto cabinetCheckpoint=game.Serialize();
    unsigned rejected{};
    for(unsigned combination=0;combination<64;++combination) {
        if(!game.Restore(cabinetCheckpoint))return 18;
        game.fuseDials=combination;
        const auto before=game.Serialize();
        if(combination==Session::solvedFuseDials) {
            if(!game.CollectFuse()||game.CollectFuse()||game.CurrentStage()!=Stage::Generator)return 19;
        } else {
            if(game.CollectFuse()||game.Serialize()!=before)return 20;
            ++rejected;
        }
    }
    if(!game.Restore(cabinetCheckpoint))return 21;
    for(unsigned wheel=0;wheel<3;++wheel)for(unsigned turn=0;turn<Session::fuseCode[wheel];++turn)
        if(!game.CycleFuseDial(wheel))return 22;
    if(!game.CollectFuse()||game.CycleFuseDial(0)||!game.Valid())return 23;
    const auto completed=game.Serialize();
    for(const auto invalid:{"MISTWATCH 5 7 100 1 1 0 -7 0 59 0 0 0 7 1 64",
        "MISTWATCH 5 7 100 1 1 0 -7 0 59 0 0 0 7 2 50",
        "MISTWATCH 5 7 100 1 1 0 -7 0 59 1 0 0 7 1 0",
        "MISTWATCH 5 7 100 1 1 0 -7 0 59 1 0 0 7 0 50",
        "MISTWATCH 5 7 100 1 0 0 -7 0 59 0 0 0 7 1 0",
        "MISTWATCH 5 7 100 1 1 0 -7 0 59 0 0 0 7 1",
        "MISTWATCH 5 7 100 1 1 0 -7 0 59 0 0 0 7 1 -1",
        "MISTWATCH 6 7 100 1 1 0 -7 0 59 0 0 0 7 1 50",
        "MISTWATCH 5 7 100 1 1 0 -7 0 59 0 0 0 7 1 50 extra"})
        if(game.Restore(invalid)||game.Serialize()!=completed)return 24;
    for(const auto legacy:{"MISTWATCH 1 7 100 1 1 1 0 0 70",
        "MISTWATCH 2 7 100 1 1 0 7 0 65 1 0",
        "MISTWATCH 3 7 100 1 1 0 7 0 65 1 0 3",
        "MISTWATCH 4 7 100 1 1 0 7 0 65 1 0 3 7"}) {
        if(!loaded.Restore(legacy)||!loaded.fuseClueRead||loaded.fuseDials!=Session::solvedFuseDials||!loaded.fuseCollected)return 25;
    }
    std::cout<<"Settlement clue/cabinet PASS; rejected combinations="<<rejected
        <<" physical furniture=2 save_v5=passed legacy_v1_to_v4=passed\n";
    return 0;
}
