#include "MistwatchSession.h"
#include "MistwatchCamera.h"
#include <iostream>
int main() {
    using namespace Mistwatch;
    Session game;
    if(game.Collect(0) || game.RepairBridge() || game.LightBeacon()) return 1;
    for(unsigned i=0;i<3;++i) {
        game.feet=Session::supplies[i];
        if(!game.Collect(i) || game.Collect(i)) return 2;
    }
    game.feet=Session::bridge;
    if(game.RepairBridge())return 25;
    for(unsigned i=0;i<3;++i){game.feet=Session::repairSites[i];if(!game.CollectRepairMaterial(i,i==0))return 26;}
    game.feet=Session::bridge;
    if(!game.RepairBridge() || game.RepairBridge()) return 3;
    game.Damage(1000);
    game.feet=Session::arena;
    if(game.FinishEncounter()) return 4;
    game.Retry();
    if(!game.Alive() || !game.bridgeRepaired || game.feet.z!=Session::bridge.z) return 5;
    game.feet=Session::arena;
    if(!game.FinishEncounter()) return 6;
    Session loaded;
    if(!loaded.Restore(game.Serialize()) || loaded.CurrentStage()!=Stage::Fuse) return 7;
    const auto before=loaded.Serialize();
    for(auto invalid:{"MISTWATCH 2 7 100 1 1 0 0 0 0", "MISTWATCH 1 0 100 1 1 1 0 0 0",
                     "MISTWATCH 1 7 101 1 1 0 0 0 0", "MISTWATCH 1 7 100 1 1 0 0 0 0 trailing"})
        if(loaded.Restore(invalid) || loaded.Serialize()!=before) return 8;
    loaded.feet=Session::beacon;
    if(loaded.LightBeacon() || loaded.RepairGenerator() || loaded.CollectFuse()) return 15;
    loaded.feet=Session::fuse;
    if(loaded.CollectFuse())return 27;
    loaded.feet=Session::rangerLog;
    if(!loaded.ReadFuseClue())return 28;
    loaded.feet=Session::fuse;
    for(unsigned wheel=0;wheel<3;++wheel)for(unsigned turn=0;turn<Session::fuseCode[wheel];++turn)
        if(!loaded.CycleFuseDial(wheel))return 29;
    if(!loaded.CollectFuse()||loaded.CollectFuse()) return 16;
    Session powerSave;
    if(!powerSave.Restore(loaded.Serialize())||powerSave.CurrentStage()!=Stage::Generator) return 17;
    loaded.feet=Session::generator;
    if(loaded.RepairGenerator()||loaded.ToggleCircuit(3)) return 21;
    if(!loaded.ToggleCircuit(0)||loaded.circuit!=3||loaded.RepairGenerator()) return 22;
    Session panel;
    if(!panel.Restore(loaded.Serialize())||panel.circuit!=3) return 23;
    if(!loaded.ToggleCircuit(2)||loaded.circuit!=7) return 24;
    if(!loaded.RepairGenerator()||loaded.RepairGenerator()) return 18;
    loaded.feet=Session::beacon;
    if(!loaded.LightBeacon() || loaded.CurrentStage()!=Stage::Complete || loaded.LightBeacon()) return 9;
    std::istringstream oversized(std::string(100000,'x'));
    const auto completed=loaded.Serialize();
    if(loaded.RestoreStream(oversized)||loaded.Serialize()!=completed||oversized.tellg()!=513) return 10;
    std::istringstream valid(game.Serialize());
    if(!loaded.RestoreStream(valid)||loaded.CurrentStage()!=Stage::Fuse) return 11;
    Session legacy;
    if(!legacy.Restore("MISTWATCH 1 7 100 1 1 1 0 0 70")||legacy.CurrentStage()!=Stage::Complete) return 19;
    if(legacy.Restore("MISTWATCH 2 7 100 1 1 1 0 0 70 0 0")) return 20;
    game.Damage(100);
    std::istringstream aliveSave(before);
    if(!game.RestoreStream(aliveSave)||!game.Alive()) return 12;
    OrbitCamera camera;
    camera.Update({100,10000},100,0,1.f/60);
    if(camera.yaw>=0 || camera.pitch!=1.1f || camera.distance!=2.5f) return 13;
    camera.Update({0,-10000},-100,0,1.f/60);
    if(camera.pitch!=-.15f || camera.distance!=10.f) return 14;
    std::cout<<"Mistwatch mission progression, retry and save validation PASS\n";
    std::cout<<"Bounded save loading and mouse orbit limits PASS\n";
}
