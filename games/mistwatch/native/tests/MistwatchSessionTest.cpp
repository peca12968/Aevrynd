#include "MistwatchSession.h"
#include "MistwatchCamera.h"
#include "MistwatchCameraInput.h"
#include <iostream>
bool EventTimeOrbitInputContract(){
    unsigned cases{};
    const auto closeEnough=[](float a,float b){return std::abs(a-b)<.000001f;};
    for(const float dt:{1.f/30.f,1.f/60.f,1.f/120.f}){
        for(const float postPress:{0.f,10.f})for(const bool release:{false,true}){
            Aevrynd::Input input;Mistwatch::OrbitCamera orbit;
            input.SetMouse({100,100});input.ConsumePresses();
            input.SetMouse({200,100});input.SetMouseButton(true);
            if(postPress>0)input.SetMouse({200+postPress,95});
            if(release){input.SetMouseButton(false);input.SetMouse({300+postPress,100});}
            Mistwatch::UpdateOrbitFromInput(orbit,input,0,dt);
            if(!closeEnough(orbit.yaw,-postPress*.005f)||!closeEnough(orbit.pitch,postPress>0?.28f:.3f)||
                !closeEnough(input.MouseDelta().x,100+postPress+(release?100.f:0.f))){
                std::cerr<<"EVENT_TIME_ORBIT_FIRST_DIVERGENCE post_press="<<postPress<<" release="<<release
                    <<" dt="<<dt<<" actual_yaw="<<orbit.yaw<<'\n';return false;
            }
            const float savedYaw=orbit.yaw,savedPitch=orbit.pitch;
            input.ConsumePresses();Mistwatch::UpdateOrbitFromInput(orbit,input,0,dt);
            if(!closeEnough(orbit.yaw,savedYaw)||!closeEnough(orbit.pitch,savedPitch))return false;
            ++cases;
        }
        Aevrynd::Input input;Mistwatch::OrbitCamera orbit;input.AddMouseWheel(2);
        Mistwatch::UpdateOrbitFromInput(orbit,input,1,dt);
        if(!closeEnough(orbit.yaw,dt*1.5f)||!closeEnough(orbit.distance,4.6f))return false;
        input.ConsumePresses();input.SetMouse({0,0});input.SetMouseButton(true);input.SetMouse({10,10});
        input.SetFocused(false);input.SetFocused(true);input.SetMouse({600,400});
        const float savedYaw=orbit.yaw,savedPitch=orbit.pitch;Mistwatch::UpdateOrbitFromInput(orbit,input,0,dt);
        if(!closeEnough(orbit.yaw,savedYaw)||!closeEnough(orbit.pitch,savedPitch))return false;
        ++cases;
        // 不先補移動訊息：按下／放開的真座標本身必須界定拖曳，不可依賴上一個anchor。
        Aevrynd::Input buttonInput;Mistwatch::OrbitCamera buttonOrbit;
        buttonInput.SetMouse({100,100});buttonInput.ConsumePresses();
        buttonInput.SetMouseButtonAt({200,100},true);buttonInput.SetMouse({210,100});
        buttonInput.SetMouseButtonAt({220,100},false);buttonInput.SetMouse({300,100});
        Mistwatch::UpdateOrbitFromInput(buttonOrbit,buttonInput,0,dt);
        if(!closeEnough(buttonOrbit.yaw,-.1f)||!closeEnough(buttonOrbit.pitch,.3f)||
            !closeEnough(buttonInput.MouseDelta().x,200.f)||!closeEnough(buttonInput.LeftMouseDragDelta().x,20.f))return false;
        ++cases;
    }
    std::cout<<"EVENT_TIME_ORBIT_PRODUCTION_INPUT_CPU_PASS cases="<<cases<<" raw_delta_preserved=1 release_tail_preserved=1 native_window=0\n";
    return cases==18;
}
int main() {
    if(!EventTimeOrbitInputContract())return 31;
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
    if(loaded.LightBeacon()||loaded.CurrentStage()!=Stage::Relays)return 30;
    for(unsigned index=0;index<3;++index) {
        loaded.feet=Session::relaySites[index];
        for(unsigned turn=0;turn<Session::RelayTargetSector(index);++turn)if(!loaded.TuneRelay(index,1))return 31;
        // 原連站正例先驗新任務未確認必拒絕，再走真確認API，不移除原檢查。
        if(index==0){
            if(loaded.ConnectRelay(index))return 33;
            if(!loaded.CycleRelayPatch(0)||!loaded.ConfirmRelayCircuit())return 34;
        }
        if(index==2){
            if(loaded.ConnectRelay(index))return 35;
            if(!loaded.CycleExtractionRoute()||!loaded.ConfirmExtractionRoute())return 36;
        }
        if(!loaded.ConnectRelay(index))return 32;
    }
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
