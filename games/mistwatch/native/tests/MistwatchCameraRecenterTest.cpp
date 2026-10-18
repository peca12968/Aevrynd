// 同一翻譯單元對正式／候選Main編譯；只用既有友元與舊公開API觀測真C輸入。
#define MISTWATCH_SAVE_INPUT_AUDIT 1
#define wWinMain MistwatchProductionEntryNeverCalled
#ifndef MISTWATCH_RECENTER_MAIN_SOURCE
#error MISTWATCH_RECENTER_MAIN_SOURCE must name the isolated main source
#endif
#include MISTWATCH_RECENTER_MAIN_SOURCE
#undef wWinMain
#include <iostream>

namespace RecenterTrial {
using namespace Aevrynd;using namespace Mistwatch;
unsigned checks{},frames{},groups{};
constexpr float Dt=1.f/60.f;
constexpr std::span<const ConvexAabbSweep3D* const> NoHulls{};
void Need(bool ok,const char* reason);
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
float Heading(float yaw){return std::remainder(yaw,6.2831853f);}
template<class T>bool SameObject(const T& a,const T& b){static_assert(std::is_trivially_copyable_v<T>);
    const auto x=std::as_bytes(std::span{&a,1}),y=std::as_bytes(std::span{&b,1});return std::equal(x.begin(),x.end(),y.begin());}
// 3603 實測只在 MotionResult 尾端 padding[66..67] 不同；具名欄位仍逐位元比較，不降低精度。
bool SameObject(const ThirdPersonCameraMotionResult3D& a,const ThirdPersonCameraMotionResult3D& b){
    return SameObject(a.camera,b.camera)&&a.status==b.status&&Same(a.displacement,b.displacement)&&
        a.accepted==b.accepted&&a.subjectVisible==b.subjectVisible;
}
bool SameObject(const ThirdPersonFramingResult3D& a,const ThirdPersonFramingResult3D& b){
    return SameObject(a.camera,b.camera)&&Same(a.yawAdjustment,b.yawAdjustment)&&a.candidatesTested==b.candidatesTested&&
        a.subjectVisible==b.subjectVisible;
}
template<class Camera>void ScalarCases(){
    if constexpr(requires(Camera c){c.Update(Vec2{},0.f,0.f,0.f,true,0.f);}){
        for(unsigned i=0;i<128;++i){
            Camera actual;actual.yaw=float(int(i%17)-8)*.31f;actual.pitch=float(i%7)*.15f;actual.distance=2.5f+float(i%13)*.5f;
            const Vec2 drag{float(int(i%9)-4)*7,float(int(i%5)-2)*11};const float wheel=float(int(i%7)-3)*.5f;
            const float keyboard=float(int(i%3)-1),dt=i%4==0?.25f:i%4==1?0.f:Dt;
            auto expected=actual;
            expected.yaw=std::remainder(expected.yaw-drag.x*.005f+keyboard*(std::min)(dt,.1f)*1.5f,6.2831853f);
            expected.pitch=std::clamp(expected.pitch+drag.y*.004f,-.15f,1.1f);
            expected.distance=std::clamp(expected.distance-wheel*.7f,2.5f,10.f);
            actual.Update(drag,wheel,keyboard,dt,false,(std::numeric_limits<float>::quiet_NaN)());
            Need(SameObject(actual,expected),"SCALAR_FALSE_BRANCH_NOT_BITEXACT");
            Camera active;const float heading=float(int(i)-64)*.21f,pitch=active.pitch,distance=active.distance;
            active.Update({},wheel,0,dt,true,heading);
            Need(Same(active.yaw,Heading(heading))&&Same(active.pitch,pitch)&&
                Same(active.distance,std::clamp(distance-wheel*.7f,2.5f,10.f)),"SCALAR_RECENTER_NOT_EXACT");
            const auto before=active;active.Update(drag,wheel,keyboard,dt,true,
                i%2?(std::numeric_limits<float>::infinity)():(std::numeric_limits<float>::quiet_NaN)());
            Need(SameObject(active,before),"SCALAR_INVALID_HEADING_WROTE_STATE");
            active=before;expected=before;
            expected.Update(drag,wheel,keyboard,dt);
            active.Update(drag,wheel,keyboard,dt,true,heading);
            if(drag.x!=0||drag.y!=0||keyboard!=0)Need(SameObject(active,expected),"SCALAR_MANUAL_PRIORITY_CHANGED");
        }
    }else Need(false,"SCALAR_RECENTER_OVERLOAD_MISSING");
    ++groups;
}
}
class MistwatchSaveInputProbe {
    Aevrynd::Engine& engine_;Aevrynd::SkinnedModel3D raw_;
    void Headless(){using namespace RecenterTrial;const auto s=engine_.GetRendererStats();
        Need(!engine_.Window()&&s.submittedFrames==0&&!s.adapterLuidValid&&!engine_.Audio().GetStats().deviceAvailable,
            "RECENTER_HEADLESS_RUNTIME_CHANGED");}
    void Input(std::initializer_list<unsigned> keys,Aevrynd::Vec2 drag={},float wheel=0){
        auto& in=engine_.GetInput();in.SetMouseButton(false);in.SetMouse({100,100});in.ConsumePresses();
        std::array<bool,256> held{};for(unsigned k:keys)held.at(k)=true;
        for(unsigned k=0;k<held.size();++k)in.SetKey(k,held[k]);
        in.SetMouseButton(true);in.SetMouse({100+drag.x,100+drag.y});in.SetMouseButton(false);in.AddMouseWheel(wheel);
    }
    void Step(MistwatchGame& g,std::initializer_list<unsigned> keys={},Aevrynd::Vec2 drag={},float wheel=0){
        Input(keys,drag,wheel);g.OnUpdate(engine_,RecenterTrial::Dt);++RecenterTrial::frames;Headless();}
    std::unique_ptr<MistwatchGame> Seed(){using namespace RecenterTrial;
        engine_.GetInput().SetFocused(true);Input({});auto p=std::make_unique<MistwatchGame>();auto& g=*p;
        g.terrain=CreateTerrain();g.session.feet={0,g.terrain.SampleHeight(0,0),0};g.solids=g.RuntimeWorldSolids(false);
        g.boar.model=raw_;ExplorerNapeHairBinding::Apply(g.explorer);BackpackFit1250::Apply(g.explorer,{+[](void*){},nullptr});
        g.started=true;g.paused=false;g.waterTime=2.5f;g.save.clear();
        g.controller.Reset(g.session.feet);
        Need(g.controller.TryRestoreMotionState({g.session.feet,{},1.8f,0,true},g.solids,NoHulls,g.terrain),"RECENTER_GROUNDED_FIXTURE");
        g.explorer.Update({},0,false);Need(g.explorer.PlaceFeet(g.session.feet,true,false,g.terrain,g.solids,NoHulls),"RECENTER_REAL_IK");
        g.ResetBoarPose();Need(g.UpdatePlayerCamera(Dt),"RECENTER_REAL_CAMERA");g.lastRenderedCamera=g.playerCameraMotion.Camera();
        for(unsigned n=0;n<8;++n){Step(g);Need(g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"RECENTER_FIXTURE_SETTLE");}
        Need(g.CaptureSaveGame().Valid(),"RECENTER_FIXTURE_SNAPSHOT");return p;
    }
    void Turn(MistwatchGame& g){using namespace RecenterTrial;
        for(unsigned n=0;n<32;++n)Step(g,{VK_RIGHT});Step(g);
        Need(std::abs(std::remainder(g.orbit.yaw-g.explorer.yaw,6.2831853f))>.3f,"RECENTER_TRUE_ARROW_SETUP");}
    void FreshAndHeld(){using namespace RecenterTrial;auto p=Seed();auto& g=*p;Turn(g);
        const float heading=g.explorer.yaw,pitch=g.orbit.pitch,distance=g.orbit.distance;Step(g,{'C'});
        // 第一個新增行為：原Main可正常編譯並執行到此處才失敗，不靠缺API或假舊實作。
        Need(Same(g.orbit.yaw,Heading(heading)),"CAMERA_RECENTER_C_EDGE_NOT_APPLIED");
        Need(Same(g.orbit.pitch,pitch)&&Same(g.orbit.distance,distance),"RECENTER_CHANGED_PITCH_DISTANCE");
        for(unsigned n=0;n<4;++n)Step(g,{'C',VK_RIGHT});const float turned=g.orbit.yaw;
        Step(g,{'C'});Need(Same(g.orbit.yaw,turned),"HELD_C_REISSUED_AFTER_MANUAL");
        Step(g);Step(g,{'C'});Need(Same(g.orbit.yaw,Heading(g.explorer.yaw)),"RELEASED_C_NOT_REARMED");++groups;
    }
    void ManualAndZoom(){using namespace RecenterTrial;auto p=Seed();auto& g=*p;Turn(g);
        const float yaw=g.orbit.yaw,pitch=g.orbit.pitch,distance=g.orbit.distance;
        Step(g,{'C',VK_LEFT,VK_RIGHT});Need(Same(g.orbit.yaw,yaw),"C_OVERRODE_ZERO_NET_MANUAL_ARROWS");
        Step(g);const float beforeDrag=g.orbit.yaw;Step(g,{'C'},{12,5});
        Need(Same(g.orbit.yaw,std::remainder(beforeDrag-12*.005f,6.2831853f)),"C_OVERRODE_MANUAL_DRAG");
        Need(Same(g.orbit.pitch,std::clamp(pitch+5*.004f,-.15f,1.1f)),"MANUAL_PITCH_CHANGED");
        Step(g);const float keptPitch=g.orbit.pitch,heading=g.explorer.yaw;Step(g,{'C'},{},1);
        Need(Same(g.orbit.yaw,Heading(heading))&&Same(g.orbit.pitch,keptPitch)&&
            Same(g.orbit.distance,std::clamp(distance-.7f,2.5f,10.f)),"C_WHEEL_NOT_COMBINED");++groups;
    }
    void PauseAndFocus(){using namespace RecenterTrial;auto p=Seed();auto& g=*p;Turn(g);
        Step(g,{VK_ESCAPE});Step(g,{'C'});const float yaw=g.orbit.yaw;const auto saved=g.CaptureSaveGame().Serialize();const float water=g.waterTime;
        Step(g,{'C',VK_ESCAPE});Need(!g.paused&&Same(g.orbit.yaw,yaw)&&g.CaptureSaveGame().Serialize()==saved&&Same(g.waterTime,water),
            "PAUSED_C_CROSSED_RESUME_BARRIER");Step(g,{'C'});Need(Same(g.orbit.yaw,yaw),"PAUSED_C_SURVIVED_CLEAR");
        engine_.GetInput().SetFocused(false);g.OnUpdate(engine_,Dt);++frames;Headless();Need(g.paused,"FOCUS_DID_NOT_PAUSE");
        engine_.GetInput().SetFocused(true);Step(g,{VK_ESCAPE});Step(g);
        Need(Same(g.orbit.yaw,yaw),"FOCUS_RECENTER_GHOST");Step(g,{'C'});
        Need(Same(g.orbit.yaw,Heading(g.explorer.yaw)),"FOCUS_NEW_C_NOT_WORKING");++groups;
    }
    void RejectedManualCancellation(){using namespace RecenterTrial;auto p=Seed();auto& g=*p;Turn(g);
        const auto solids=g.solids;const float yaw=g.orbit.yaw;
        // 真幾何包住角色與鏡頭，要求既有交易確實拒絕；沒有替換碰撞或鏡頭函式。
        g.solids.push_back({g.session.feet+Vec3{0,2,0},{20,20,20}});
        Step(g,{'C'});Need(!g.playableCameraOnlyCommitted&&g.playableOutcome!=MistwatchGame::PlayableOutcome::WorldCommitted&&
            Same(g.orbit.yaw,yaw),"RECENTER_REJECTION_FIXTURE_NOT_REJECTED");
        Step(g,{'C',VK_LEFT,VK_RIGHT});Need(Same(g.orbit.yaw,yaw),"REJECTED_MANUAL_CHANGED_CAMERA");
        g.solids=solids;Step(g,{'C'});Need(Same(g.orbit.yaw,yaw),"REJECTED_MANUAL_LEFT_DELAYED_C");
        Step(g);Step(g,{'C'});Need(Same(g.orbit.yaw,Heading(g.explorer.yaw)),"MANUAL_CANCEL_BLOCKED_NEW_C");
        Turn(g);const float pendingYaw=g.orbit.yaw;g.solids.push_back({g.session.feet+Vec3{0,2,0},{20,20,20}});
        Step(g,{'C'});Need(Same(g.orbit.yaw,pendingYaw)&&!g.playableCameraOnlyCommitted,"SECOND_REJECT_SETUP");
        g.solids=solids;Step(g);Need(Same(g.orbit.yaw,Heading(g.explorer.yaw)),"REJECTED_CAMERA_C_NOT_RETAINED");++groups;
    }
    void FifoAndAtomic(){using namespace RecenterTrial;auto p=Seed();auto& g=*p;Turn(g);Aevrynd::Input input;
        for(unsigned n=0;n<FrameInputLatch::Capacity;++n){input.SetKey('E',false);input.ConsumePresses();input.SetKey('E',true);
            Need(g.playableInputs.Observe(input,{0,0,1}),"FIFO_FILL_FAILED");}
        Need(g.playableInputs.Count()==16,"FIFO_CAPACITY_CHANGED");Step(g,{'C'});
        Need(Same(g.orbit.yaw,Heading(g.explorer.yaw))&&g.playableInputs.Count()==15,"C_USED_WORLD_FIFO_SLOT");
        FrameInputLatch latch;Aevrynd::Input device;
        device.AddMouseWheel(2);Need(latch.Observe(device,{0,0,1}),"ATOMIC_CAMERA_SETUP");device.ConsumePresses();
        device.AddMouseWheel((std::numeric_limits<float>::infinity)());device.SetKey('C',true);
        Need(!latch.Observe(device,{0,0,1})&&latch.Overflowed()&&latch.Wheel()==2&&latch.Count()==0,"INVALID_CAMERA_PARTIAL_COMMIT");
        latch.Clear();Need(latch.Count()==0&&!latch.Overflowed()&&latch.Wheel()==0,"LATCH_CLEAR_CHANGED");++groups;
    }
    std::unique_ptr<MistwatchGame> CompleteSeed(){using namespace RecenterTrial;auto p=Seed();auto& g=*p;const auto feet=g.session.feet;
        // 舊版合法存檔遷移只準備記憶體fixture，不代表真通關；所有物理／骨架仍用既有實作。
        Need(g.session.Restore("MISTWATCH 1 7 100 1 1 1 0 0 70"),"COMPLETE_VALID_LEGACY_FIXTURE");g.session.feet=feet;
        Encounter defeated;for(unsigned tick=0;tick<600&&defeated.health;++tick)defeated.Tick(defeated.feet,Dt,true);
        Need(defeated.Valid()&&defeated.health==0&&defeated.phase==EnemyPhase::Defeated,"COMPLETE_REAL_DEFEAT_SETUP");
        g.encounter=defeated;g.solids=g.RuntimeWorldSolids(true);g.controller.Reset(feet);
        Need(g.controller.TryRestoreMotionState({feet,{},1.8f,0,true},g.solids,NoHulls,g.terrain),"COMPLETE_REPAIRED_GROUNDED_FIXTURE");
        Need(g.explorer.PlaceFeet(feet,true,false,g.terrain,g.solids,NoHulls),"COMPLETE_REAL_IK");g.ResetBoarPose();
        g.playerCameraMotion.Reset();Need(g.UpdatePlayerCamera(Dt),"COMPLETE_REAL_CAMERA");g.lastRenderedCamera=g.playerCameraMotion.Camera();
        Need(g.session.CurrentStage()==Stage::Complete&&g.CaptureSaveGame().Valid()&&g.save.empty(),"COMPLETE_SNAPSHOT_INVALID");return p;
    }
    void CompleteCamera(){using namespace RecenterTrial;auto p=CompleteSeed();auto& g=*p;Turn(g);
        const auto saved=g.CaptureSaveGame().Serialize();const auto controller=g.controller.StateHash(),motor=g.playerLocomotion.StateHash();
        const float water=g.waterTime;const auto worldUnchanged=[&]{Need(g.CaptureSaveGame().Valid()&&g.CaptureSaveGame().Serialize()==saved&&
            g.controller.StateHash()==controller&&g.playerLocomotion.StateHash()==motor&&Same(g.waterTime,water)&&g.save.empty(),"COMPLETE_C_CHANGED_WORLD_OR_SAVE");};
        const float pitch=g.orbit.pitch,distance=g.orbit.distance;Step(g,{'C'});
        Need(Same(g.orbit.yaw,Heading(g.explorer.yaw))&&Same(g.orbit.pitch,pitch)&&Same(g.orbit.distance,distance),"COMPLETE_C_NOT_APPLIED");worldUnchanged();
        Step(g);Turn(g);const float yaw=g.orbit.yaw;const auto solids=g.solids;
        g.solids.push_back({g.session.feet+Vec3{0,2,0},{20,20,20}});Step(g,{'C'});
        Need(Same(g.orbit.yaw,yaw),"COMPLETE_REJECT_FIXTURE_NOT_REJECTED");Step(g,{'C',VK_LEFT,VK_RIGHT});
        Need(Same(g.orbit.yaw,yaw),"COMPLETE_REJECTED_MANUAL_MOVED_CAMERA");g.solids=solids;Step(g,{'C'});
        Need(Same(g.orbit.yaw,yaw),"COMPLETE_REJECTED_MANUAL_LEFT_GHOST_C");worldUnchanged();
        Step(g);
        // 無C控組：同一真安全鏡頭交易先求原結果，完整還原鏡頭owner後再走真完成OnUpdate。
        const auto beforeCamera=g.playerCameraMotion;const auto beforeOrbit=g.orbit;const auto beforeFraming=g.lastPlayerFraming;
        const auto beforeMotion=g.lastCameraMotion;const auto beforeFixed=g.cameraFixedTicks,beforeCapture=g.captureCameraTicks;
        const float beforeDisplacement=g.cameraMaximumTickDisplacement,beforeOcclusion=g.cameraOcclusionSeconds;
        const Vec2 drag{7,-3};const float wheel=.5f;
        Need(g.TryUpdateCameraOnly(drag,wheel,1,Dt),"COMPLETE_NO_C_CONTROL_REJECTED");
        const auto expectedCamera=g.playerCameraMotion;const auto expectedOrbit=g.orbit;const auto expectedFraming=g.lastPlayerFraming;
        const auto expectedMotion=g.lastCameraMotion;const auto expectedFixed=g.cameraFixedTicks,expectedCapture=g.captureCameraTicks;
        const float expectedDisplacement=g.cameraMaximumTickDisplacement,expectedOcclusion=g.cameraOcclusionSeconds;
        g.playerCameraMotion=beforeCamera;g.orbit=beforeOrbit;g.lastPlayerFraming=beforeFraming;g.lastCameraMotion=beforeMotion;
        g.cameraFixedTicks=beforeFixed;g.captureCameraTicks=beforeCapture;g.cameraMaximumTickDisplacement=beforeDisplacement;g.cameraOcclusionSeconds=beforeOcclusion;
        Step(g,{VK_RIGHT},drag,wheel);
        Need(SameObject(g.playerCameraMotion,expectedCamera)&&SameObject(g.orbit,expectedOrbit)&&SameObject(g.lastPlayerFraming,expectedFraming)&&
            SameObject(g.lastCameraMotion,expectedMotion)&&g.cameraFixedTicks==expectedFixed&&g.captureCameraTicks==expectedCapture&&
            Same(g.cameraMaximumTickDisplacement,expectedDisplacement)&&Same(g.cameraOcclusionSeconds,expectedOcclusion),"COMPLETE_NO_C_OLD_PATH_CHANGED");
        worldUnchanged();++groups;
    }
public:
    MistwatchSaveInputProbe(Aevrynd::Engine& e,Aevrynd::SkinnedModel3D raw):engine_(e),raw_(std::move(raw)){}
    void Run(){using namespace RecenterTrial;Headless();FreshAndHeld();ManualAndZoom();PauseAndFocus();RejectedManualCancellation();FifoAndAtomic();
        CompleteCamera();ScalarCases<OrbitCamera>();Need(groups==7,"RECENTER_GROUP_COUNT");std::cout<<"CAMERA_RECENTER_TRUE_MAIN_CPU_PASS groups="<<groups<<" checks="<<checks<<" frames="<<frames
            <<" true_OnUpdate=1 OnStart=0 OnRender=0 GPU=false\n";}
};
int main(int argc,char** argv){try{RecenterTrial::Need(argc==2,"RAW_BOAR_ARGUMENT_REQUIRED");
    const auto loaded=Aevrynd::LoadSkinnedModelAssetFile(argv[1]);RecenterTrial::Need(bool(loaded),"RAW_BOAR_REQUIRED");
    Aevrynd::Engine engine;MistwatchSaveInputProbe probe(engine,loaded.model);probe.Run();return 0;
}catch(const std::exception& error){std::cerr<<"CAMERA_RECENTER_FIRST_DIVERGENCE="<<error.what()<<'\n';return 1;}}
