// 獨立CPU翻譯單元，僅沿既有audit友元呼叫真正Main；舊版也能編譯此同一份測試。
#define MISTWATCH_SAVE_INPUT_AUDIT 1
#define wWinMain MistwatchProductionEntryNeverCalled
#ifndef MISTWATCH_COUNTER_BOUNDARY_MAIN_SOURCE
#define MISTWATCH_COUNTER_BOUNDARY_MAIN_SOURCE "../src/main.cpp"
#endif
#include MISTWATCH_COUNTER_BOUNDARY_MAIN_SOURCE
#undef wWinMain
#include <iostream>
#include <limits>

namespace CounterBoundary {
using namespace Aevrynd;using namespace Mistwatch;
unsigned checks{},frames{},groups{};
constexpr float Dt=1.f/60.f;
constexpr unsigned AttackMask=1u<<unsigned(FrameEdge::Attack);
constexpr std::span<const ConvexAabbSweep3D* const> NoHulls{};
void Need(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
}
class MistwatchSaveInputProbe {
    Aevrynd::Engine& engine_;Aevrynd::SkinnedModel3D raw_;
    void Headless(){using namespace CounterBoundary;
        const auto stats=engine_.GetRendererStats();Need(engine_.Window()==nullptr&&stats.submittedFrames==0&&
            !stats.adapterLuidValid&&!engine_.Audio().GetStats().deviceAvailable,"HEADLESS_RUNTIME_CHANGED");}
    void Input(std::initializer_list<unsigned> held){
        auto& in=engine_.GetInput();in.ConsumePresses();std::array<bool,256> desired{};
        for(unsigned key:held)desired.at(key)=true;
        for(unsigned key=0;key<desired.size();++key)in.SetKey(key,desired[key]);
    }
    void Step(MistwatchGame& g,std::initializer_list<unsigned> held){using namespace CounterBoundary;
        Input(held);g.OnUpdate(engine_,Dt);++frames;Headless();}
    void Committed(const MistwatchGame& g){using namespace CounterBoundary;
        Need(g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"REAL_WORLD_NOT_COMMITTED");
        Need(g.playableLastPairStatus&&*g.playableLastPairStatus==Mistwatch::PairContact::Status::Applied,
            "REAL_NPC_NOT_APPLIED");}
    std::unique_ptr<MistwatchGame> Seed(){using namespace CounterBoundary;
        engine_.GetInput().SetFocused(true);Input({});
        auto p=std::make_unique<MistwatchGame>();auto& g=*p;
        g.terrain.Configure({{-30,0,-30},128,128,1});g.terrain.Fill(0);
        g.session.collected=7;g.session.repairMaterials=7;g.session.bridgeRepaired=true;g.session.feet={0,0,-2};
        g.encounter.feet={};g.encounter.phase=EnemyPhase::Recovery;g.encounter.phaseTime=1;
        Need(g.session.Valid()&&g.encounter.Valid()&&g.session.CurrentStage()==Stage::Encounter,"FIXTURE_INVALID");
        ExplorerNapeHairBinding::Apply(g.explorer);BackpackFit1250::Apply(g.explorer,{+[](void*){},nullptr});
        g.boar.model=raw_;g.started=true;g.paused=false;g.save.clear();g.waterTime=2.5f;
        g.controller.Reset(g.session.feet);
        Need(g.controller.TryRestoreMotionState({g.session.feet,{},1.8f,0,true},g.solids,NoHulls,g.terrain),"REAL_GROUND_RESTORE");
        g.explorer.Update({},0,false);
        Need(g.explorer.PlaceFeet(g.session.feet,true,false,g.terrain,g.solids,NoHulls),"REAL_EXPLORER_IK");
        g.ResetBoarPose();Need(g.UpdatePlayerCamera(Dt),"REAL_CAMERA_SEED");g.lastRenderedCamera=g.playerCameraMotion.Camera();
        g.fieldJournalOpen=false;g.actionFeedback.Clear();g.tuningInputRepeat.Reset();
        for(unsigned tick=0;tick<8;++tick){Step(g,{});Committed(g);}
        Need(g.session.health==100&&g.playableInputs.Count()==0&&g.controller.Grounded(),"SETTLED_FIXTURE_CHANGED");return p;
    }
    void Windup(MistwatchGame& g,bool early=false,bool willHurt=false){using namespace CounterBoundary;
        Encounter e;e.feet={};const Vec3 first=willHurt?g.session.feet:Vec3{0,0,2};
        Need(e.Tick(first,Dt,false)==0&&e.phase==EnemyPhase::Windup,"TRUE_WINDUP_START");
        unsigned ticks{};const float stop=early?2.f*Dt:Dt;
        while(e.phaseTime>stop&&ticks++<80)Need(e.Tick(g.session.feet,Dt,false)==0,"WINDUP_PREMATURE_STRIKE");
        Need(e.Valid()&&e.phase==EnemyPhase::Windup&&e.phaseTime>0&&
            (early?(e.phaseTime>Dt&&e.phaseTime<=2.f*Dt):e.phaseTime<=Dt),"TRUE_TICK_BOUNDARY_NOT_REACHED");
        g.encounter=e;g.ResetBoarPose();
        const auto guide=BuildObjectiveGuide(g.session,g.encounter,g.evasion,g.lastRenderedCamera,false,true,g.solids,g.terrain);
        Need(guide.valid&&!guide.counterReady,"WINDUP_ADVERTISED_READY");
    }
    void Boundary(MistwatchGame& g,std::initializer_list<unsigned> held={'F'}){using namespace CounterBoundary;
        Windup(g);const auto serial=g.playablePairState.lastSerial;
        Step(g,held);Committed(g);
        Need(g.encounter.phase==EnemyPhase::Recovery&&g.encounter.health==3&&!g.encounter.countered&&
            g.session.health==100&&g.inputReplayLastDamage==0&&g.playablePairState.lastSerial==serial+1,"BOUNDARY_WORLD_CHANGED");
        const auto guide=BuildObjectiveGuide(g.session,g.encounter,g.evasion,g.lastRenderedCamera,false,true,g.solids,g.terrain);
        Need(guide.valid&&guide.counterReady,"BOUNDARY_NOT_ACTUALLY_READY");
        // 舊Main+舊Latch能執行到這裡，再因真實Count=0失敗，不靠新欄位造成編譯失敗。
        Need(g.playableInputs.Count()==1,"COUNTER_BOUNDARY_FRESH_EDGE_LOST");
        Need(g.playableInputs.Front().mask==AttackMask,"BOUNDARY_REPLAYED_OTHER_EDGES");
    }
    void MainBoundary(){using namespace CounterBoundary;auto p=Seed();auto& g=*p;Boundary(g);
        Need(engine_.GetInput().Down('F'),"INITIAL_F_NOT_HELD");
        Step(g,{'F'});Committed(g);
        Need(!engine_.GetInput().Pressed('F')&&g.encounter.health==2&&g.encounter.countered&&
            g.playableInputs.Count()==0&&g.session.health==100,"DEFERRED_EDGE_NOT_ONCE_NEXT_WORLD");
        for(unsigned tick=0;tick<20;++tick){Step(g,{'F'});Committed(g);Need(g.encounter.health==2&&g.playableInputs.Count()==0,"HELD_F_AUTO_REPEATED");}
        Step(g,{});Step(g,{'F'});Committed(g);Need(g.encounter.health==2,"SECOND_ATTACK_SAME_OPENING");++groups;
    }
    void EarlyHurtDash(){using namespace CounterBoundary;
        {auto p=Seed();auto& g=*p;Windup(g,true);Step(g,{'F'});Committed(g);
            Need(g.encounter.phase==EnemyPhase::Windup&&g.playableInputs.Count()==0,"EARLY_PRESS_BUFFERED");
            Step(g,{'F'});Committed(g);Need(g.encounter.phase==EnemyPhase::Recovery&&g.encounter.health==3&&g.playableInputs.Count()==0,"EARLY_HELD_PRESS_FIRED");
            Step(g,{});Step(g,{'F'});Committed(g);Need(g.encounter.health==2&&g.encounter.countered,"FRESH_RECOVERY_REPRESS_BLOCKED");}
        {auto p=Seed();auto& g=*p;Windup(g,false,true);Step(g,{'F'});Committed(g);
            Need(g.session.health==80&&g.inputReplayLastDamage==20&&g.encounter.health==3&&g.playableInputs.Count()==0,"BOUNDARY_GRANTED_INVULNERABILITY");
            Step(g,{'F'});Committed(g);Need(g.encounter.health==3,"HURT_EDGE_RETRIED");}
        {auto p=Seed();auto& g=*p;Windup(g);Step(g,{'Q','F'});Committed(g);
            Need(g.inputReplayLastDashing&&g.encounter.health==3&&g.playableInputs.Count()==0,"DASH_F_BUFFERED");}++groups;
    }
    void PauseFocus(){using namespace CounterBoundary;
        for(bool focus:{false,true}){auto p=Seed();auto& g=*p;Boundary(g);
            if(focus){engine_.GetInput().SetFocused(false);Step(g,{});}else Step(g,{'F',VK_ESCAPE});
            Need(g.paused&&g.playableInputs.Count()==0&&g.encounter.health==3,"PAUSE_FOCUS_RETAINED_ATTACK");
            if(focus)engine_.GetInput().SetFocused(true);
            Step(g,{});Step(g,{'F',VK_ESCAPE});Need(!g.paused&&g.encounter.health==3&&g.playableInputs.Count()==0,"RESUME_ATTACK_REPLAYED");
            Step(g,{'F'});Committed(g);Need(g.encounter.health==3&&g.playableInputs.Count()==0,"POST_RESUME_HELD_ATTACK_REPLAYED");
            Step(g,{});Step(g,{'F'});Committed(g);Need(g.encounter.health==2,"POST_RESUME_FRESH_ATTACK_BLOCKED");}++groups;
    }
    void OneAttemptRangeLos(){using namespace CounterBoundary;
        for(bool wall:{false,true}){auto p=Seed();auto& g=*p;Boundary(g);
            if(wall)g.solids.push_back({{0,.7f,-1.43f},{.35f,.65f,.03f}});
            else {const Vec3 feet{0,0,-3};g.session.feet=feet;
                Need(g.controller.TryRestoreMotionState({feet,{},1.8f,0,true},g.solids,NoHulls,g.terrain),"RANGE_FIXTURE_RESTORE");
                g.playerLocomotion.Reset();g.explorer.feetInitialized=false;g.explorer.Update({},0,false);
                Need(g.explorer.PlaceFeet(feet,true,false,g.terrain,g.solids,NoHulls)&&g.UpdatePlayerCamera(Dt),"RANGE_FIXTURE_IK_CAMERA");}
            const auto guide=BuildObjectiveGuide(g.session,g.encounter,g.evasion,g.lastRenderedCamera,false,true,g.solids,g.terrain);
            Need(guide.valid&&!guide.counterReady,"BLOCKED_NEXT_STEP_ADVERTISED_READY");
            Step(g,{'F'});Committed(g);Need(g.encounter.health==3&&g.playableInputs.Count()==0,"FAILED_DEFERRED_ATTEMPT_RETAINED");
            if(wall)g.solids.clear();
            for(unsigned tick=0;tick<3;++tick){Step(g,{'F'});Committed(g);Need(g.encounter.health==3&&g.playableInputs.Count()==0,"FAILED_ATTACK_AUTO_DELAYED");}
        }++groups;
    }
    void MixedAndFifo(){using namespace CounterBoundary;auto p=Seed();auto& g=*p;Boundary(g,{'E','F'});
        const auto collected=g.session.collected,materials=g.session.repairMaterials;
        Step(g,{'E','F','1'});Committed(g);
        Need(g.encounter.health==2&&g.playableInputs.Count()==1&&
            g.playableInputs.Front().mask==(1u<<unsigned(FrameEdge::Tune1)),"DEFERRED_EDGE_BROKE_FIFO");
        Need(g.session.collected==collected&&g.session.repairMaterials==materials,"MIXED_INTERACTION_REPLAYED");
        Step(g,{'E','F','1'});Committed(g);Need(g.playableInputs.Count()==0&&g.encounter.health==2,"FIFO_NOT_DRAINED_ONCE");++groups;
    }
    void RollbackRetry(){using namespace CounterBoundary;auto p=Seed();auto& g=*p;Boundary(g);
        const auto saved=g.CaptureSaveGame().Serialize();const auto water=g.waterTime;const auto serial=g.playablePairState.lastSerial;
        const auto bind=g.boar.model.skeleton.joints[4].bindTransform;
        // 故障注入只破壞私有fixture的足部骨長資料，逼真Boar.PlaceFeet拒絕，不替換pair callback。
        g.boar.model.skeleton.joints[4].bindTransform.translation.x=std::numeric_limits<float>::quiet_NaN();
        Step(g,{'F'});
        Need(g.playableOutcome!=MistwatchGame::PlayableOutcome::WorldCommitted&&g.playableLastPairStatus&&
            *g.playableLastPairStatus==Mistwatch::PairContact::Status::PoseRejected,"REAL_POSE_REJECTION_NOT_REACHED");
        Need(g.CaptureSaveGame().Serialize()==saved&&g.waterTime==water&&g.playablePairState.lastSerial==serial&&
            g.encounter.health==3&&g.playableInputs.Count()==1&&g.playableInputs.Front().mask==AttackMask,"REJECTED_WORLD_CONSUMED_DEFERRED_EDGE");
        g.boar.model.skeleton.joints[4].bindTransform=bind;
        Step(g,{'F'});Committed(g);Need(g.encounter.health==2&&g.encounter.countered&&g.playableInputs.Count()==0&&
            g.playablePairState.lastSerial==serial+1,"RETRY_NOT_SINGLE_SUCCESSFUL_WORLD_STEP");
        for(unsigned tick=0;tick<3;++tick){Step(g,{'F'});Committed(g);Need(g.encounter.health==2&&g.playableInputs.Count()==0,"RETRY_ATTACK_REPEATED");}++groups;
    }
public:
    MistwatchSaveInputProbe(Aevrynd::Engine& engine,Aevrynd::SkinnedModel3D raw):engine_(engine),raw_(std::move(raw)){}
    void Run(){using namespace CounterBoundary;Headless();MainBoundary();EarlyHurtDash();PauseFocus();OneAttemptRangeLos();MixedAndFifo();RollbackRetry();
        Need(groups==6,"EXACT_SIX_GROUPS");std::cout<<"COUNTER_BOUNDARY_TRUE_MAIN_CPU_PASS groups="<<groups<<" checks="<<checks<<" frames="<<frames
            <<" true_OnUpdate=1 real_Input=1 window=0 renderer_started=0 GPU=false\n";}
};
int main(int argc,char** argv){try{
    CounterBoundary::Need(argc==2,"USAGE_RAW_BOAR_ASSET");const auto loaded=Aevrynd::LoadSkinnedModelAssetFile(argv[1]);
    CounterBoundary::Need(bool(loaded),"RAW_BOAR_ASSET_NOT_LOADED");
    Aevrynd::Engine engine;MistwatchSaveInputProbe probe(engine,loaded.model);probe.Run();return 0;
}catch(const std::exception& error){std::cerr<<"COUNTER_BOUNDARY_FIRST_DIVERGENCE="<<error.what()<<'\n';return 1;}}
