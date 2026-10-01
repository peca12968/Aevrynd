#include "MistwatchWorld.h"
#include "MistwatchExplorer.h"
#include <Aevrynd/CharacterController3D.h>
#include <iostream>
int main() {
    Mistwatch::Session game;
    for(unsigned index=0;index<3;++index){game.feet=Mistwatch::Session::supplies[index];game.Collect(index);}
    game.feet=Mistwatch::Session::bridge;
    if(game.RepairBridge()) {
        std::cerr<<"FIRST_DIVERGENCE: bridge repairs without exploring for tools or timber\n";
        return 1;
    }
    if(game.CurrentStage()!=Mistwatch::Stage::RepairMaterials || game.CollectRepairMaterial(3,true))return 2;
    for(unsigned site=1;site<3;++site) {
        game.feet=Mistwatch::Session::repairSites[site];
        if(!game.CollectRepairMaterial(site,false)||game.CollectRepairMaterial(site,false))return 3;
        Mistwatch::Session partial;
        if(!partial.Restore(game.Serialize())||partial.repairMaterials!=game.repairMaterials)return 4;
    }
    const auto tools=Mistwatch::Session::repairSites[0];
    game.feet=tools;
    if(game.CollectRepairMaterial(0,false))return 5;
    game.feet=tools+Aevrynd::Vec3{1,0,0};
    if(game.CollectRepairMaterial(0,true))return 6;
    game.feet=tools+Aevrynd::Vec3{0,.31f,0};
    if(game.CollectRepairMaterial(0,true))return 7;

    const auto terrain=Mistwatch::CreateTerrain();
    const auto solids=Mistwatch::WorldSolids(false);
    Aevrynd::CharacterController3D actor;
    actor.Reset(tools+Aevrynd::Vec3{5,0,0});
    for(unsigned tick=0;tick<120;++tick)actor.Step({{-1,0,0},false,1},solids,terrain,1.f/60.f);
    if(actor.Position().x<tools.x+2 || actor.Stats().collisions==0) {
        std::cerr<<"Standing actor bypassed low shelter ceiling\n";return 8;
    }
    if(!actor.TrySetHeight(1.4f,solids))return 9;
    Mistwatch::Explorer explorer;
    unsigned crouchTicks=0;
    const auto approach=tools+Aevrynd::Vec3{.75f,0,0};
    while(Aevrynd::Length(actor.Position()-approach)>.03f && crouchTicks<300) {
        const auto previous=actor.Position();
        actor.Step({{-1,0,0},false,.45f},solids,terrain,1.f/60.f);
        explorer.Update(actor.Position()-previous,1.f/60.f,true);
        if(!explorer.PlaceFeet(actor.Position(),actor.Grounded(),true,terrain,solids))return 10;
        ++crouchTicks;
    }
    if(crouchTicks>=300)return 11;
    if(actor.Position().x<tools.x+.749f)return 24;
    const auto before=actor.StateHash();
    if(actor.TrySetHeight(1.8f,solids)||actor.StateHash()!=before)return 12;
    Aevrynd::SkinningWorkspace3D workspace;
    Aevrynd::Mesh3D pose;
    if(!Aevrynd::SkinModelInto(explorer.model,explorer.pose,workspace,pose))return 13;
    if(pose.Bounds().center.y+pose.Bounds().extents.y+actor.Position().y>1.45f)return 14;
    game.feet=actor.Position();
    if(!game.CollectRepairMaterial(0,true)||game.CurrentStage()!=Mistwatch::Stage::Bridge)return 15;
    Mistwatch::Session loaded;
    if(!loaded.Restore(game.Serialize())||loaded.repairMaterials!=7||Aevrynd::Length(loaded.feet-game.feet)>1e-5f)return 16;
    Aevrynd::CharacterController3D restored;
    restored.Reset(loaded.feet);restored.SetHeight(1.4f);
    if(restored.TrySetHeight(1.8f,solids))return 17;
    for(unsigned tick=0;tick<120;++tick)actor.Step({{1,0,0},false,.45f},solids,terrain,1.f/60.f);
    if(!actor.TrySetHeight(1.8f,solids))return 18;
    game.feet=Mistwatch::Session::bridge;
    if(!game.RepairBridge()||game.CollectRepairMaterial(0,true)||!game.Valid())return 19;
    game.Damage(100);game.Retry();
    if(game.repairMaterials!=7||!game.bridgeRepaired||!game.Alive())return 20;
    const auto valid=game.Serialize();
    for(const auto invalid:{"MISTWATCH 4 7 100 1 0 0 0 0 30 0 0 0 0",
        "MISTWATCH 4 7 100 0 0 0 0 0 30 0 0 0 8",
        "MISTWATCH 4 7 100 0 0 0 0 0 30 0 0 0",
        "MISTWATCH 4 7 100 0 0 0 0 0 30 0 0 0 -1",
        "MISTWATCH 4 7 100 0 0 0 0 0 30 0 0 0 7 extra"})
        if(game.Restore(invalid)||game.Serialize()!=valid)return 21;
    for(const auto legacy:{"MISTWATCH 1 7 100 1 0 0 0 0 30",
        "MISTWATCH 2 7 100 1 0 0 0 0 30 0 0",
        "MISTWATCH 3 7 100 1 0 0 0 0 30 0 0 0"})
        if(!loaded.Restore(legacy)||loaded.repairMaterials!=7||loaded.CurrentStage()!=Mistwatch::Stage::Encounter)return 22;
    if(!loaded.Restore("MISTWATCH 3 7 100 0 0 0 0 0 30 0 0 0")||
        loaded.repairMaterials!=0||loaded.CurrentStage()!=Mistwatch::Stage::RepairMaterials)return 23;
    std::cout<<"Repair exploration, crouched shelter traversal, model clearance, save v4 and legacy migration PASS; crouch ticks="<<crouchTicks<<'\n';
}
