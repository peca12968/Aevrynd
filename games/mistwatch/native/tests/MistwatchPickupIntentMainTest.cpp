// 私有 CPU fixture 呼叫完整真 Main；不執行 OnStart、OnRender，也不寫玩家檔案。
#define MISTWATCH_SAVE_INPUT_AUDIT 1
#define wWinMain MistwatchProductionEntryNeverCalled
#ifndef MISTWATCH_PICKUP_MAIN_SOURCE
#define MISTWATCH_PICKUP_MAIN_SOURCE "../src/main.cpp"
#endif
#include MISTWATCH_PICKUP_MAIN_SOURCE
#undef wWinMain
#include <array>
#include <bit>
#include <iostream>

namespace PickupRegression {
using namespace Aevrynd;using namespace Mistwatch;
unsigned checks{},frames{},groups{},cases{};
constexpr float Dt=1.f/60.f;
constexpr unsigned FrameCap=800;
constexpr std::span<const ConvexAabbSweep3D* const> NoHulls{};
void Need(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
struct IntentState {
    bool armed{};PickupKind kind{};unsigned index{};double remaining{};
    explicit IntentState(const PickupIntent& intent):armed(intent.Armed()),kind(intent.Kind()),index(intent.Index()),remaining(intent.Remaining()){}
    bool Same(const PickupIntent& intent)const {
        return armed==intent.Armed()&&kind==intent.Kind()&&index==intent.Index()&&
            std::bit_cast<std::uint64_t>(remaining)==std::bit_cast<std::uint64_t>(intent.Remaining());
    }
};
}

class MistwatchSaveInputProbe {
    Aevrynd::Engine& engine_;Aevrynd::SkinnedModel3D raw_;
    void Headless(){using namespace PickupRegression;const auto stats=engine_.GetRendererStats();
        Need(!engine_.Window()&&stats.submittedFrames==0&&!stats.adapterLuidValid&&
             !engine_.Audio().GetStats().deviceAvailable,"REGRESSION_STARTED_NATIVE_RUNTIME");}
    void Keys(std::initializer_list<unsigned> keys){
        auto& input=engine_.GetInput();input.ConsumePresses();
        std::array<bool,256> held{};for(unsigned key:keys)held.at(key)=true;
        for(unsigned key=0;key<held.size();++key)input.SetKey(key,held[key]);
    }
    void Step(MistwatchGame& game,std::initializer_list<unsigned> keys={},bool requireCommit=true){using namespace PickupRegression;
        Need(frames<FrameCap,"REGRESSION_FRAME_CAP");Keys(keys);game.OnUpdate(engine_,Dt);++frames;Headless();
        if(requireCommit)Need(game.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"REGRESSION_WORLD_STEP_REJECTED");
    }
    std::unique_ptr<MistwatchGame> Seed(Aevrynd::Vec3 feet={4,0,3.3f},unsigned supplies=0,unsigned materials=0){using namespace PickupRegression;
        engine_.GetInput().SetFocused(true);Keys({});auto result=std::make_unique<MistwatchGame>();auto& game=*result;
        game.terrain.Configure({{-30,0,-30},128,128,1});game.terrain.Fill(0);
        game.session.feet=feet;game.session.collected=supplies;game.session.repairMaterials=materials;
        game.boar.model=raw_;ExplorerNapeHairBinding::Apply(game.explorer);BackpackFit1250::Apply(game.explorer,{+[](void*){},nullptr});
        game.started=true;game.paused=false;game.waterTime=2.5f;game.save.clear();game.audioSettings.clear();game.orbit.yaw=0;
        game.controller.Reset(feet);
        Need(game.controller.TryRestoreMotionState({feet,{},1.8f,0,true},game.solids,NoHulls,game.terrain),"REGRESSION_GROUNDED_FIXTURE");
        game.explorer.Update({},0,false);
        Need(game.explorer.PlaceFeet(feet,true,false,game.terrain,game.solids,NoHulls),"REGRESSION_IK_FIXTURE");
        game.ResetBoarPose();Need(game.UpdatePlayerCamera(Dt),"REGRESSION_CAMERA_FIXTURE");game.lastRenderedCamera=game.playerCameraMotion.Camera();
        for(unsigned i=0;i<8;++i)Step(game);
        Need(game.session.Valid()&&game.CaptureSaveGame().Valid(),"REGRESSION_SAVE_STATE_FIXTURE");return result;
    }
    static double Distance(const MistwatchGame& game,Aevrynd::Vec3 target){
        return std::hypot(double(game.session.feet.x)-target.x,double(game.session.feet.z)-target.z);
    }
    static void Empty(const MistwatchGame& game,const char* reason){using namespace PickupRegression;
        Need(!game.pickupIntent.Armed()&&game.pickupIntent.Kind()==Mistwatch::PickupKind::None&&
             game.pickupIntent.Index()==Mistwatch::PickupIntent::NoIndex&&game.pickupIntent.Remaining()==0,reason);
    }
    void ArmSupply(MistwatchGame& game){using namespace PickupRegression;
        Step(game,{'E'});Need(game.pickupIntent.Armed()&&game.pickupIntent.Kind()==PickupKind::Supply&&
            game.pickupIntent.Index()==0&&game.pickupIntent.Remaining()==PickupIntent::Duration&&
            game.session.collected==0&&game.playableInputs.Count()==0,"REGRESSION_SUPPLY_NOT_ARMED");
    }
    void WalkIntoRange(MistwatchGame& game,Aevrynd::Vec3 target,bool heldE){using namespace PickupRegression;
        unsigned steps{};
        while(Distance(game,target)>2.5&&steps<40){
            if(heldE)Step(game,{'W','E'});else Step(game,{'W'});
            ++steps;
        }
        Need(steps>0&&steps<=40&&Distance(game,target)<=2.5,"REGRESSION_ENTRY_NOT_REACHED");
    }
    void HeadlessEarly(){using namespace PickupRegression;const auto s=engine_.GetRendererStats();
        Need(!engine_.Window()&&s.submittedFrames==0&&!s.adapterLuidValid&&!engine_.Audio().GetStats().deviceAvailable,
            "PICKUP_HEADLESS_RUNTIME_CHANGED");}
    void StepEarly(MistwatchGame& g,std::initializer_list<unsigned> keys={},bool requireCommit=true){
        using namespace PickupRegression;Need(frames<FrameCap,"REGRESSION_FRAME_CAP");Keys(keys);g.OnUpdate(engine_,Dt);++frames;HeadlessEarly();
        if(requireCommit)Need(g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"PICKUP_REAL_WORLD_STEP_REJECTED");
    }
    std::unique_ptr<MistwatchGame> SeedEarly(Aevrynd::Vec3 feet={4,0,3.45f}){using namespace PickupRegression;
        engine_.GetInput().SetFocused(true);Keys({});auto p=std::make_unique<MistwatchGame>();auto& g=*p;
        g.terrain.Configure({{-30,0,-30},128,128,1});g.terrain.Fill(0);g.session.feet=feet;
        g.boar.model=raw_;ExplorerNapeHairBinding::Apply(g.explorer);BackpackFit1250::Apply(g.explorer,{+[](void*){},nullptr});
        g.started=true;g.paused=false;g.waterTime=2.5f;g.save.clear();g.orbit.yaw=0;
        g.controller.Reset(feet);
        Need(g.controller.TryRestoreMotionState({feet,{},1.8f,0,true},g.solids,NoHulls,g.terrain),"PICKUP_GROUNDED_FIXTURE");
        g.explorer.Update({},0,false);Need(g.explorer.PlaceFeet(feet,true,false,g.terrain,g.solids,NoHulls),"PICKUP_TRUE_IK_FIXTURE");
        g.ResetBoarPose();Need(g.UpdatePlayerCamera(Dt),"PICKUP_TRUE_CAMERA_FIXTURE");g.lastRenderedCamera=g.playerCameraMotion.Camera();
        for(unsigned n=0;n<8;++n)StepEarly(g);
        Need(g.session.Valid()&&g.CaptureSaveGame().Valid(),"PICKUP_VALID_FIXTURE");return p;
    }
    static double Distance(const MistwatchGame& g){return std::hypot(double(g.session.feet.x)-4,double(g.session.feet.z)-6);}
    void EarlyOnce(){using namespace PickupRegression;auto p=SeedEarly();auto& g=*p;
        Need(Distance(g)>2.5&&Distance(g)<=2.85,"PICKUP_INITIAL_APPROACH_RING");
        StepEarly(g,{'W','E'});Need(Distance(g)>2.5&&g.session.collected==0&&g.playableInputs.Count()==0,"PICKUP_E_WAS_NOT_EARLY_CONSUMED_EDGE");
        std::cout<<"EARLY_E_ARM distance="<<std::setprecision(12)<<Distance(g)<<" mask="<<g.session.collected<<'\n';
        unsigned heldSteps{};
        while(Distance(g)>2.5&&heldSteps<7){StepEarly(g,{'W','E'});++heldSteps;
            std::cout<<"EARLY_E_HELD step="<<heldSteps<<" distance="<<Distance(g)<<" mask="<<g.session.collected<<'\n';}
        Need(Distance(g)<=2.5&&heldSteps>0&&heldSteps*double(Dt)<=.12,"PICKUP_DID_NOT_ENTER_DURING_SHORT_GRACE");
        // 首個新增功能斷言的舊版執行失敗由獨立原測例保存；本合併檔另含新 API 回歸。
        Need(g.session.collected==1,"EARLY_E_PICKUP_INTENT_NOT_RETAINED");
        for(unsigned n=0;n<6;++n)StepEarly(g,{'W','E'});
        const auto expected=PickupFeedback::Build(0,1,0,0);
        Need(expected&&g.session.collected==1&&g.session.repairMaterials==0&&g.message==*expected,"PICKUP_NOT_ONCE_OR_FEEDBACK_CHANGED");
        ++groups;++cases;std::cout<<"PICKUP_REGRESSION_EARLY_ONCE_PASS\n";
    }
    void ExpiryNoEdgeBehind(){using namespace PickupRegression;
        {
            auto p=Seed();auto& game=*p;ArmSupply(game);
            for(unsigned i=0;i<8;++i)Step(game,{'E'});
            Empty(game,"EXPIRED_INTENT_RETAINED");Need(game.session.collected==0,"EXPIRY_REMOTE_PICKUP");
            WalkIntoRange(game,Session::supplies[0],true);
            Need(game.session.collected==0&&engine_.GetInput().Down('E')&&!engine_.GetInput().Pressed('E'),"EXPIRED_HELD_E_RETRIGGERED");
            Empty(game,"EXPIRED_HELD_E_REARMED");++cases;
        }
        {
            auto p=Seed();auto& game=*p;WalkIntoRange(game,Session::supplies[0],false);
            Need(game.session.collected==0,"NO_E_AUTOMATIC_PICKUP");Empty(game,"NO_E_INTENT_CREATED");++cases;
        }
        {
            auto p=Seed();auto& game=*p;
            // 此處是明示的記憶體鏡頭 fixture，不是假造 Guide 或實機轉向證據。
            game.lastRenderedCamera.target=game.session.feet;
            game.lastRenderedCamera.position=game.session.feet+Aevrynd::Vec3{0,4,8};
            const auto guide=BuildObjectiveGuide(game.session,game.encounter,game.evasion,game.lastRenderedCamera,
                false,true,game.solids,game.terrain);
            Need(guide.valid&&guide.bearing==ObjectiveBearing::Behind,"BACKWARD_GUIDE_FIXTURE");
            Step(game,{'E'});Empty(game,"BACKWARD_PRESS_ARMED");Need(game.session.collected==0,"BACKWARD_REMOTE_PICKUP");++cases;
        }
        ++groups;std::cout<<"PICKUP_REGRESSION_EXPIRY_NO_EDGE_BEHIND_PASS\n";
    }
    void OriginalAndExcluded(){using namespace PickupRegression;
        {
            auto p=Seed({4,0,3.6f});auto& game=*p;Step(game,{'E'});
            Need(game.session.collected==1&&game.session.repairMaterials==0,"FRESH_IN_RANGE_E_CHANGED");
            Empty(game,"IN_RANGE_E_LEFT_INTENT");++cases;
        }
        for(unsigned index=1;index<=2;++index){
            const auto target=Session::repairSites[index];auto p=Seed(target-Aevrynd::Vec3{0,0,2.55f},7,7u^(1u<<index));auto& game=*p;
            Step(game,{'W','E'});Need(Distance(game,target)>2.5&&game.pickupIntent.Armed()&&
                game.pickupIntent.Kind()==PickupKind::Timber&&game.pickupIntent.Index()==index,"TIMBER_E_NOT_EARLY_ARMED");
            const unsigned originalMask=game.session.repairMaterials;WalkIntoRange(game,target,true);
            Need(game.session.repairMaterials==7&&game.session.collected==7&&!game.session.bridgeRepaired,"TIMBER_NOT_SINGLE_ORIGINAL_COLLECT");
            const auto message=PickupFeedback::Build(7,7,originalMask,7);
            Need(message&&game.message==*message,"TIMBER_PICKUP_FEEDBACK_CHANGED");Empty(game,"TIMBER_SUCCESS_RETAINED");++cases;
        }
        {
            auto p=Seed(Session::repairSites[0]-Aevrynd::Vec3{0,0,2.55f},7,6);auto& game=*p;
            Step(game,{VK_CONTROL,'E'});Empty(game,"SHELTER_TOOL_EARLY_ARMED");
            Need(game.session.repairMaterials==6,"SHELTER_TOOL_REMOTE_COLLECTED");++cases;
        }
        {
            auto p=Seed(Session::repairSites[0]-Aevrynd::Vec3{0,0,.6f},7,6);auto& game=*p;
            Step(game,{VK_CONTROL,'E'});Need(game.controller.Settings().height<1.5f&&game.session.repairMaterials==7&&
                !game.session.bridgeRepaired,"ORIGINAL_CROUCHED_TOOL_E_CHANGED");Empty(game,"TOOL_SUCCESS_ARMED");++cases;
        }
        {
            auto p=Seed(Session::bridge-Aevrynd::Vec3{0,0,2.55f},7,7);auto& game=*p;
            Step(game,{'W','E'});Need(Distance(game,Session::bridge)>2.5,"BRIDGE_E_NOT_EARLY");Empty(game,"BRIDGE_EARLY_ARMED");
            WalkIntoRange(game,Session::bridge,true);Need(!game.session.bridgeRepaired,"BUFFER_AUTO_REPAIRED_BRIDGE");Empty(game,"BRIDGE_HELD_REARMED");++cases;
        }
        ++groups;std::cout<<"PICKUP_REGRESSION_ORIGINAL_PICKUPS_EXCLUSIONS_PASS\n";
    }
    void PauseFocusJournal(){using namespace PickupRegression;
        for(unsigned mode=0;mode<3;++mode){auto p=Seed();auto& game=*p;ArmSupply(game);const auto before=game.session.Serialize();
            if(mode==0){Step(game,{VK_ESCAPE,'E'},false);Need(game.paused,"ESC_DID_NOT_PAUSE");}
            if(mode==1){engine_.GetInput().SetFocused(false);Step(game,{},false);Need(game.paused&&!engine_.GetInput().Focused(),"FOCUS_DID_NOT_PAUSE");}
            if(mode==2){Step(game,{'J','E'});Need(game.fieldJournalOpen,"J_DID_NOT_OPEN");}
            Empty(game,"MENU_FOCUS_OR_J_NOT_CLEARED");Need(game.session.Serialize()==before,"CANCEL_CHANGED_SESSION");++cases;
        }
        ++groups;std::cout<<"PICKUP_REGRESSION_PAUSE_FOCUS_J_PASS\n";
    }
    void SaveLoadRetryKeys(){using namespace PickupRegression;
        // 空 save 是刻意的負控：驗按鍵取消，不冒稱已執行真磁碟存讀檔。
        for(unsigned key:{unsigned(VK_F5),unsigned(VK_F9),unsigned('R')}){auto p=Seed();auto& game=*p;ArmSupply(game);
            Need(game.save.empty()&&game.audioSettings.empty(),"REGRESSION_FILE_PATH_NOT_ISOLATED");
            Step(game,{key,'E'});Empty(game,"SAVE_LOAD_RETRY_KEY_RETAINED_INTENT");
            Need(game.session.collected==0&&game.session.health==100,"SAVE_LOAD_RETRY_KEY_CHANGED_PICKUP");
            WalkIntoRange(game,Session::supplies[0],true);Need(game.session.collected==0,"CANCELLED_INTENT_GHOST_PICKUP");
            Empty(game,"CANCELLED_INTENT_REARMED");++cases;
        }
        ++groups;std::cout<<"PICKUP_REGRESSION_SAVE_EMPTY_F5_F9_ALIVE_R_PASS\n";
    }
    void RejectedWorldPreserves(){using namespace PickupRegression;auto p=Seed();auto& game=*p;ArmSupply(game);
        const IntentState intent(game.pickupIntent);const auto session=game.session.Serialize(),saved=game.CaptureSaveGame().Serialize();
        const auto controller=game.controller.StateHash(),locomotion=game.playerLocomotion.StateHash(),serial=game.playablePairState.lastSerial;
        const auto water=std::bit_cast<std::uint32_t>(game.waterTime);const auto solids=game.solids;
        // 使用既有正式鏡頭測試的真包圍幾何；必須實際拒絕，不以假 callback 取代。
        game.solids.push_back({game.session.feet+Aevrynd::Vec3{0,2,0},{20,20,20}});
        for(unsigned i=0;i<3;++i){Step(game,{'E'},false);
            Need(game.playableOutcome!=MistwatchGame::PlayableOutcome::WorldCommitted&&!game.playableCameraOnlyCommitted,"PICKUP_REJECT_GEOMETRY_DID_NOT_REJECT");
            Need(intent.Same(game.pickupIntent),"REJECTED_WORLD_CHANGED_INTENT_OR_TIMER");
            Need(game.session.Serialize()==session&&game.CaptureSaveGame().Serialize()==saved&&
                game.controller.StateHash()==controller&&game.playerLocomotion.StateHash()==locomotion&&
                game.playablePairState.lastSerial==serial&&std::bit_cast<std::uint32_t>(game.waterTime)==water,"REJECTED_WORLD_LEAKED_STATE");
        }
        game.solids=solids;Step(game,{'E'});
        Need(game.pickupIntent.Armed()&&game.pickupIntent.Kind()==intent.kind&&game.pickupIntent.Index()==intent.index&&
            game.pickupIntent.Remaining()<intent.remaining&&game.session.collected==0,"RESTORED_WORLD_DID_NOT_ADVANCE_INTENT_ONCE");
        ++cases;
        {
            // 舊 FIFO 邊沿仍照原規則保留；解除阻擋後不能冒充當下的新 E 取得額外緩衝。
            auto queued=Seed();auto& old=*queued;const auto originalSolids=old.solids;
            old.solids.push_back({old.session.feet+Aevrynd::Vec3{0,2,0},{20,20,20}});
            Step(old,{'E'},false);
            Need(old.playableOutcome!=MistwatchGame::PlayableOutcome::WorldCommitted&&old.playableInputs.Count()==1&&
                old.playableInputs.Front().Pressed(FrameEdge::Interact),"QUEUED_E_REJECTION_NOT_REACHED");
            Empty(old,"REJECTED_QUEUED_E_ARMED");old.solids=originalSolids;
            Step(old,{'E'});
            Need(!engine_.GetInput().Pressed('E')&&engine_.GetInput().Down('E')&&old.playableInputs.Count()==0&&
                Distance(old,Session::supplies[0])>2.5&&old.session.collected==0,"QUEUED_E_RELEASE_COMMIT_NOT_OUTSIDE_CONSUMED");
            Empty(old,"DELAYED_FIFO_E_CREATED_NEW_INTENT");
            WalkIntoRange(old,Session::supplies[0],true);
            Need(old.session.collected==0,"DELAYED_FIFO_E_GRANTED_EXTRA_PICKUP");Empty(old,"DELAYED_FIFO_E_REARMED_ON_HOLD");++cases;
        }
        ++groups;std::cout<<"PICKUP_REGRESSION_TRUE_GEOMETRY_REJECTION_PASS\n";
    }
    void TargetStageChanges(){using namespace PickupRegression;
        for(unsigned mode=0;mode<2;++mode){auto p=Seed();auto& game=*p;ArmSupply(game);
            // 明示外部合法任務快照變更；不是靠傳送或假造 Guide 模擬實際行走。
            game.session.collected=mode==0?1u:7u;
            Need(game.session.Valid()&&game.CaptureSaveGame().Valid(),"TARGET_STAGE_FIXTURE_INVALID");
            const auto expected=game.session.Serialize();Step(game,{'E'});
            Empty(game,"CHANGED_TARGET_STAGE_RETAINED_INTENT");Need(game.session.Serialize()==expected,"CHANGED_TARGET_STAGE_COLLECTED_OTHER_ITEM");++cases;
        }
        ++groups;std::cout<<"PICKUP_REGRESSION_TARGET_STAGE_CHANGE_PASS\n";
    }
public:
    MistwatchSaveInputProbe(Aevrynd::Engine& engine,Aevrynd::SkinnedModel3D raw):engine_(engine),raw_(std::move(raw)){}
    void Run(){using namespace PickupRegression;Headless();EarlyOnce();ExpiryNoEdgeBehind();OriginalAndExcluded();PauseFocusJournal();SaveLoadRetryKeys();RejectedWorldPreserves();TargetStageChanges();
        Need(groups==7&&cases==20&&frames<=FrameCap,"PICKUP_REGRESSION_COVERAGE_CHANGED");
        std::cout<<"PICKUP_INTENT_REGRESSION_MAIN_CPU_PASS groups="<<groups<<" cases="<<cases<<" checks="<<checks<<" frames="<<frames
                 <<" OnStart=0 OnRender=0 save_io=0 GPU=false\n";
    }
};

int main(int argc,char** argv){try{PickupRegression::Need(argc==2,"RAW_BOAR_ARGUMENT_REQUIRED");
    const auto loaded=Aevrynd::LoadSkinnedModelAssetFile(argv[1]);PickupRegression::Need(bool(loaded),"RAW_BOAR_REQUIRED");
    Aevrynd::Engine engine;MistwatchSaveInputProbe probe(engine,loaded.model);probe.Run();return 0;
}catch(const std::exception& error){std::cerr<<"PICKUP_INTENT_REGRESSION_FIRST_DIVERGENCE="<<error.what()<<'\n';return 1;}}
