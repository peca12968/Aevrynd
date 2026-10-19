// 真Main CPU接線：此檔必須複製到沒有候選header的獨立目錄再編譯，避免污染RED。
#define MISTWATCH_SAVE_INPUT_AUDIT 1
#define wWinMain MistwatchProductionEntryNeverCalled
#ifndef MISTWATCH_RELAY_TRACKING_MAIN_SOURCE
#define MISTWATCH_RELAY_TRACKING_MAIN_SOURCE "../src/main.cpp"
#endif
#include MISTWATCH_RELAY_TRACKING_MAIN_SOURCE
#undef wWinMain
#include <iostream>
#include <limits>

namespace TrackingTest {
using namespace Aevrynd;using namespace Mistwatch;namespace Map=Mistwatch::SurveyMap;
unsigned checks{},frames{},groups{},nearPairs{};
constexpr float Dt=1.f/60.f;constexpr unsigned None=3;
constexpr std::span<const ConvexAabbSweep3D* const> NoHulls{};
void Need(bool ok,const char* why);
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
struct Hash {std::uint64_t value=14695981039346656037ull;
    void Word(std::uint64_t v){value=(value^v)*1099511628211ull;}
    void Float(float v){Word(std::bit_cast<std::uint32_t>(v));}
    void Text(std::string_view s){for(unsigned char c:s)Word(c);}
    template<class T>void Bytes(const T& t){static_assert(std::is_trivially_copyable_v<T>);
        for(auto b:std::as_bytes(std::span{&t,1}))Word(std::to_integer<unsigned char>(b));}
};
Encounter Defeated(){Encounter e;unsigned ticks{};
    while(e.health&&ticks++<1000){const auto p=e.phase==EnemyPhase::Windup?e.strikeTarget+Vec3{3,0,0}:e.feet+Vec3{0,0,-2};
        e.Tick(p,Dt,e.phase==EnemyPhase::Recovery&&!e.countered);}
    Need(e.Valid()&&e.health==0&&e.phase==EnemyPhase::Defeated,"TRUE_ENCOUNTER_DEFEATED_FIXTURE");return e;}
Session Powered(){Session s;
    for(unsigned i=0;i<3;++i){s.feet=Session::supplies[i];Need(s.Collect(i),"FIXTURE_SUPPLY");}
    for(unsigned i=0;i<3;++i){s.feet=Session::repairSites[i];Need(s.CollectRepairMaterial(i,i==0),"FIXTURE_MATERIAL");}
    s.feet=Session::bridge;Need(s.RepairBridge(),"FIXTURE_BRIDGE");const auto enemy=Defeated();
    s.feet=enemy.feet;Need(s.FinishEncounter(enemy.feet),"FIXTURE_FINISH_ENCOUNTER");
    s.feet=Session::rangerLog;Need(s.ReadFuseClue(),"FIXTURE_CLUE");s.feet=Session::fuse;
    for(unsigned i=0;i<3;++i)for(unsigned n=0;n<Session::fuseCode[i];++n)Need(s.CycleFuseDial(i),"FIXTURE_DIAL");
    Need(s.CollectFuse(),"FIXTURE_FUSE");s.feet=Session::generator;
    Need(s.ToggleCircuit(0)&&s.ToggleCircuit(2)&&s.RepairGenerator()&&s.Valid(),"FIXTURE_POWER");return s;}
Vec3 StationFeet(unsigned i){const auto site=Session::relaySites.at(i);const float x=site.x+(i==1?-1.5f:1.5f);
    return {x,GroundElevation(x,site.z),site.z};}
Camera3D Camera(Vec3 feet){Camera3D c;c.target=feet+Vec3{0,1,0};c.position=c.target+Vec3{0,2,-6};return c;}
template<class S>auto Guide(const S& s,const Encounter& e,const Evasion& action,const Camera3D& c,
    const std::vector<BoxCollider3D>& solids,const HeightField3D& terrain,unsigned preferred=None){
    if constexpr(requires{BuildObjectiveGuide(s,e,action,c,false,true,solids,terrain,preferred);})
        return BuildObjectiveGuide(s,e,action,c,false,true,solids,terrain,preferred);
    else return BuildObjectiveGuide(s,e,action,c,false,true,solids,terrain);
}
bool SameOldGuide(const ObjectiveGuide& a,const ObjectiveGuide& b){return a.valid==b.valid&&a.hasTarget==b.hasTarget&&
    a.useReady==b.useReady&&a.counterReady==b.counterReady&&a.evadeReady==b.evadeReady&&a.tuneReady==b.tuneReady&&
    a.earlyToolsReady==b.earlyToolsReady&&a.bridgeIntent==b.bridgeIntent&&a.index==b.index&&Same(a.target,b.target)&&
    a.distance==b.distance&&a.relativeRadians==b.relativeRadians&&a.bearing==b.bearing&&a.objective==b.objective&&
    a.navigation==b.navigation&&a.instruction==b.instruction&&a.context==b.context;}
void Complete(Session& s,unsigned index){s.feet=StationFeet(index);Need(s.NearRelay(index),"FIXTURE_NEAR_STATION");
    if(index==0){Need(FieldInput::Lever(s,0)==FieldInput::Result::PatchChanged,"FIXTURE_PATCH");
        Need(FieldInput::Interact(s)==FieldInput::Result::CircuitConfirmed,"FIXTURE_CIRCUIT");}
    if(index==2){Need(FieldInput::Lever(s,2)==FieldInput::Result::RouteChanged,"FIXTURE_ROUTE");
        Need(FieldInput::Interact(s)==FieldInput::Result::RouteConfirmed,"FIXTURE_ROUTE_CONFIRM");}
    for(unsigned n=0;n<16&&s.RelaySignal(index)!=100;++n)
        Need(FieldInput::Lever(s,1)==FieldInput::Result::AntennaChanged,"FIXTURE_AIM");
    Need(FieldInput::Interact(s)==FieldInput::Result::RelayConnected,"FIXTURE_LINK");}
std::string LayoutText(const Map::Layout& layout){std::string s;for(unsigned i=0;i<layout.count;++i)s+=layout.lines[i].text+'\n';return s;}
template<class D>void MapTracking(const D& d,unsigned tracked,unsigned preview){
    if constexpr(requires{d.trackedRelay;d.previewRelay;d.relayControls;})
        Need(d.relayControls&&d.trackedRelay==tracked&&d.previewRelay==preview,"MAP_TRACKING_STATE");
    else Need(false,"MAP_TRACKING_API_MISSING");
}
}
class MistwatchSaveInputProbe {
    Aevrynd::Engine& engine_;Aevrynd::SkinnedModel3D raw_;std::filesystem::path output_;
    void Headless(){using namespace TrackingTest;const auto s=engine_.GetRendererStats();Need(!engine_.Window()&&s.submittedFrames==0&&
        !s.adapterLuidValid&&!engine_.Audio().GetStats().deviceAvailable,"HEADLESS_RUNTIME_CHANGED");}
    void Input(std::initializer_list<unsigned> keys){auto& in=engine_.GetInput();in.ConsumePresses();std::array<bool,256> held{};
        for(unsigned k:keys)held.at(k)=true;for(unsigned k=0;k<held.size();++k)in.SetKey(k,held[k]);}
    void Step(MistwatchGame& g,std::initializer_list<unsigned> keys){Input(keys);g.OnUpdate(engine_,TrackingTest::Dt);++TrackingTest::frames;Headless();}
    static std::uint64_t World(const MistwatchGame& g){TrackingTest::Hash h;h.Text(g.CaptureSaveGame().Serialize());
        h.Float(g.waterTime);h.Word(g.controller.StateHash());h.Word(g.playerLocomotion.StateHash());h.Word(g.playablePairState.lastSerial);
        for(const auto* p:{&g.explorer.pose,&g.boar.pose})for(const auto& m:p->globalMatrices)for(const auto& r:m.values)for(float f:r)h.Float(f);
        return h.value;}
    static std::uint64_t CameraHash(const MistwatchGame& g){TrackingTest::Hash h;h.Bytes(g.playerCameraMotion);h.Bytes(g.orbit);
        h.Bytes(g.lastCameraMotion);h.Bytes(g.lastRenderedCamera);h.Word(g.cameraFixedTicks);return h.value;}
    template<class G>static unsigned Tracked(const G& g){if constexpr(requires{g.trackedRelay;})return g.trackedRelay;else return 3;}
    template<class G>static unsigned Preview(const G& g){if constexpr(requires{g.previewRelay;})return g.previewRelay;else return 3;}
    template<class G>static auto View(const G& g){return TrackingTest::Guide(g.session,g.encounter,g.evasion,g.lastRenderedCamera,g.solids,g.terrain,Tracked(g));}
    template<class G>static auto MapView(const G& g){const auto guide=View(g);
        if constexpr(requires{TrackingTest::Map::Build(g.session,guide,g.lastRenderedCamera,g.terrain,Tracked(g),Preview(g),true);})
            return TrackingTest::Map::Build(g.session,guide,g.lastRenderedCamera,g.terrain,Tracked(g),Preview(g),true);
        else return TrackingTest::Map::Build(g.session,guide,g.lastRenderedCamera,g.terrain);}
    std::unique_ptr<MistwatchGame> Seed(Aevrynd::Vec3 feet={0,0,65},const Mistwatch::Session* prepared=nullptr){using namespace TrackingTest;
        engine_.GetInput().SetFocused(true);Input({});auto p=std::make_unique<MistwatchGame>();auto& g=*p;
        g.session=prepared?*prepared:Powered();g.encounter=Defeated();g.session.feet=feet;g.session.feet.y=GroundElevation(feet.x,feet.z);
        g.terrain=CreateTerrain();g.solids=g.RuntimeWorldSolids(true);g.boar.model=raw_;
        ExplorerNapeHairBinding::Apply(g.explorer);BackpackFit1250::Apply(g.explorer,{+[](void*){},nullptr});
        g.started=true;g.paused=false;g.waterTime=2.5f;g.save.clear();g.fieldJournalOpen=false;
        g.controller.Reset(g.session.feet);Need(g.controller.TryRestoreMotionState({g.session.feet,{},1.8f,0,true},g.solids,NoHulls,g.terrain),"TRACKING_GROUNDED_RESTORE");
        g.explorer.Update({},0,false);Need(g.explorer.PlaceFeet(g.session.feet,true,false,g.terrain,g.solids,NoHulls),"TRACKING_REAL_IK");
        g.ResetBoarPose();Need(g.UpdatePlayerCamera(Dt),"TRACKING_REAL_CAMERA");g.lastRenderedCamera=g.playerCameraMotion.Camera();
        for(unsigned n=0;n<8;++n){Step(g,{});Need(g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"TRACKING_SETTLE_REJECTED");}
        Need(g.CaptureSaveGame().Valid(),"TRACKING_SNAPSHOT_INVALID");return p;}
    void PickNorthWest(MistwatchGame& g){using namespace TrackingTest;Step(g,{VK_ESCAPE});Need(g.paused,"ESC_NOT_PAUSED");
        const auto world=World(g),camera=CameraHash(g);Step(g,{VK_LEFT});
        Need(World(g)==world&&CameraHash(g)==camera,"MAP_SELECTION_CHANGED_WORLD");
        Step(g,{VK_LEFT,VK_RETURN});
        Need(g.paused&&World(g)==world&&CameraHash(g)==camera,"ENTER_RESUMED_OR_CHANGED_WORLD");
        // 第一項新增行為：同Test對原Main/原headers可編譯，舊版仍追蹤nearest西站，這裡才真RED。
        Need(Same(View(g).target,Session::relaySites[2]),"RELAY_TRACKING_SELECTION_NOT_CONFIRMED");
        Need(Tracked(g)==2&&Preview(g)==None,"CONFIRMATION_STATE_WRONG");}
    void SelectionAndResume(){using namespace TrackingTest;auto p=Seed();auto& g=*p;PickNorthWest(g);
        const auto d=MapView(g);MapTracking(d,2,None);DrawList draw;const auto layout=Map::MeasureLayout(draw,d);
        Need(LayoutText(layout).find("追蹤")!=std::string::npos,"TRACKED_MAP_TEXT_MISSING");
        Need(Map::Draw(draw,d).tiles==750,"TRACKING_CHANGED_TILE_BUDGET");
        const auto world=World(g),camera=CameraHash(g);Step(g,{VK_LEFT,VK_ESCAPE});Need(!g.paused&&World(g)==world&&CameraHash(g)==camera,"RESUME_BARRIER_CHANGED_WORLD");
        const float yaw=g.orbit.yaw;Step(g,{VK_LEFT});Step(g,{VK_LEFT});
        Need(g.orbit.yaw==yaw&&Same(View(g).target,Session::relaySites[2]),"MAP_HELD_ARROW_LEAKED_TO_CAMERA");
        const auto hud=FieldPresentation::Build(g.session,View(g),g.encounter,true,false,false);
        Need(hud.navigation.find("西北中繼站")!=std::string::npos,"ACTIVE_HUD_NOT_TRACKED");
        Step(g,{});Step(g,{VK_LEFT});Need(g.orbit.yaw!=yaw,"RELEASED_FRESH_ARROW_BLOCKED");
        const auto feet=g.session.feet;for(unsigned n=0;n<8;++n)Step(g,{'W'});Need(Length(g.session.feet-feet)>.01f,"TRACKING_BLOCKED_HELD_W");++groups;}
    void CancelAndHeld(){using namespace TrackingTest;auto p=Seed();auto& g=*p;PickNorthWest(g);Step(g,{});
        const auto frozen=World(g),camera=CameraHash(g);
        Step(g,{VK_LEFT,VK_RIGHT,VK_RETURN});Need(Tracked(g)==2&&Preview(g)==None,"SIMULTANEOUS_ARROWS_SELECTED");
        Step(g,{});Step(g,{VK_RIGHT});const auto preview=Preview(g);Need(preview!=None,"PREVIEW_NOT_CREATED");
        for(unsigned n=0;n<4;++n)Step(g,{VK_RIGHT});Need(Preview(g)==preview,"HELD_ARROW_RETROGGLED");
        Step(g,{'J',VK_RETURN});Need(g.fieldJournalOpen&&Tracked(g)==2&&Preview(g)==None,"J_CONFIRMED_PREVIEW");
        Step(g,{});Step(g,{'J',VK_RIGHT,VK_RETURN});Need(!g.fieldJournalOpen&&Tracked(g)==2&&Preview(g)==None,"J_CLOSE_SELECTED");
        Step(g,{});Step(g,{VK_RIGHT});engine_.GetInput().SetFocused(false);Step(g,{});
        Need(Tracked(g)==2&&Preview(g)==None&&g.paused,"FOCUS_CHANGED_TRACKING");engine_.GetInput().SetFocused(true);
        Step(g,{VK_RETURN});Need(Tracked(g)==2&&Preview(g)==None,"FOCUS_REGAIN_CONFIRMED");
        Need(World(g)==frozen&&CameraHash(g)==camera,"PAUSED_CONTROLS_CHANGED_WORLD");
        auto ordinary=Seed();auto& h=*ordinary;Step(h,{VK_LEFT});const float yaw=h.orbit.yaw;
        Step(h,{VK_LEFT,VK_ESCAPE});Step(h,{VK_LEFT});Step(h,{VK_LEFT,VK_ESCAPE});Step(h,{VK_LEFT});
        Need(!h.paused&&h.orbit.yaw!=yaw,"PREEXISTING_HELD_CAMERA_ARROW_BLOCKED");
        // 沒有由地圖消耗的fresh箭頭：手冊頁、同批關手冊、同批恢復均保留原鏡頭語意。
        for(unsigned mode=0;mode<3;++mode){
            auto q=Seed();auto& u=*q;Step(u,{VK_ESCAPE});Step(u,{});const auto world=World(u),cameraBefore=CameraHash(u);
            if(mode<2){Step(u,{'J'});Step(u,{});Need(u.fieldJournalOpen,"ARROW_NEGATIVE_MANUAL_NOT_OPEN");}
            if(mode==0)Step(u,{VK_LEFT});
            if(mode==1)Step(u,{'J',VK_LEFT});
            Need(Tracked(u)==None&&Preview(u)==None,"NON_MAP_ARROW_SELECTED_STATION");
            Step(u,{VK_LEFT,VK_ESCAPE});
            Need(!u.paused&&World(u)==world&&CameraHash(u)==cameraBefore&&Tracked(u)==None&&Preview(u)==None,
                "NON_MAP_ARROW_RESUME_CHANGED_STATE");
            const float resumeYaw=u.orbit.yaw;Step(u,{VK_LEFT});
            Need(u.orbit.yaw!=resumeYaw,"UNCONSUMED_PAUSED_ARROW_BLOCKED");
        }++groups;}
    void PureGuideAndFallback(){using namespace TrackingTest;const auto terrain=CreateTerrain();const auto enemy=Defeated();const Evasion action;
        for(unsigned mask=0;mask<8;++mask){auto s=Powered();for(unsigned i=0;i<3;++i)if(mask&(1u<<i))Complete(s,i);
            s.feet={0,0,65};const auto camera=Camera(s.feet);const std::vector<BoxCollider3D> empty;
            const auto ordinary=BuildObjectiveGuide(s,enemy,action,camera,false,true,empty,terrain);
            for(unsigned invalid:{None,4u,(std::numeric_limits<unsigned>::max)()})
                Need(SameOldGuide(ordinary,Guide(s,enemy,action,camera,empty,terrain,invalid)),"DEFAULT_OR_INVALID_TRACKING_CHANGED_GUIDE");
            for(unsigned i=0;i<3;++i){const auto q=Guide(s,enemy,action,camera,empty,terrain,i);
                if(mask&(1u<<i))Need(SameOldGuide(q,ordinary),"COMPLETED_TARGET_NOT_FALLBACK");
                else Need(Same(q.target,Session::relaySites[i])&&q.index==ordinary.index&&q.useReady==ordinary.useReady&&
                    q.tuneReady==ordinary.tuneReady&&q.context==ordinary.context&&q.instruction==ordinary.instruction,"NAVIGATION_CHANGED_INTERACTION_FIELDS");}
        }
        auto s=Powered();s.feet={-21,GroundElevation(-21,77.9f),77.9f};const auto camera=Camera(s.feet);const std::vector<BoxCollider3D> empty;
        Need(Guide(s,enemy,action,camera,empty,terrain).index==0,"WEST_SPLIT_FIXTURE");
        Need(Same(Guide(s,enemy,action,camera,empty,terrain,2).target,Session::relaySites[2]),"PIN_BELOW_SPLIT");
        s.feet={-21,GroundElevation(-21,78.1f),78.1f};Need(Guide(s,enemy,action,camera,empty,terrain).index==2,"NORTHWEST_SPLIT_FIXTURE");
        Need(Same(Guide(s,enemy,action,camera,empty,terrain,0).target,Session::relaySites[0]),"PIN_ABOVE_SPLIT");
        Session fresh;Need(SameOldGuide(BuildObjectiveGuide(fresh,enemy,action,camera,false,true,empty,terrain),
            Guide(fresh,enemy,action,camera,empty,terrain,2)),"OTHER_STAGE_TRACKED");++groups;}
    void NearPairs(){using namespace TrackingTest;
        constexpr std::array names{"西側山脊中繼站","東側瞭望中繼站","西北中繼站"};
        for(unsigned localIndex=0;localIndex<3;++localIndex)for(unsigned preferred=0;preferred<3;++preferred)if(localIndex!=preferred){
            auto p=Seed(StationFeet(localIndex));auto& g=*p;
            // 選擇經真暫停按鍵，不直接寫新增production欄位。
            Step(g,{VK_ESCAPE});Step(g,{VK_RIGHT});Step(g,{});
            if(Preview(g)!=preferred){Step(g,{VK_RIGHT});Step(g,{});}
            Step(g,{VK_RETURN});Need(Tracked(g)==preferred,"NEAR_PAIR_SELECTION");
            Step(g,{VK_ESCAPE});Step(g,{});
            const auto baseline=BuildObjectiveGuide(g.session,g.encounter,g.evasion,g.lastRenderedCamera,false,true,g.solids,g.terrain);
            const auto guide=View(g);Need(guide.index==localIndex&&guide.index==baseline.index&&guide.useReady==baseline.useReady&&
                guide.tuneReady==baseline.tuneReady&&guide.instruction==baseline.instruction&&guide.context==baseline.context&&
                guide.counterReady==baseline.counterReady&&guide.evadeReady==baseline.evadeReady&&
                guide.earlyToolsReady==baseline.earlyToolsReady&&guide.bridgeIntent==baseline.bridgeIntent,
                "TRACKING_CHANGED_NEAR_GUIDE");
            const auto hud=FieldPresentation::Build(g.session,guide,g.encounter,g.started,g.paused,g.fieldJournalOpen);
            Need(hud.navigation.find(names[preferred])!=std::string::npos&&hud.context.find(names[localIndex])!=std::string::npos,"NEAR_HUD_NAV_ACTION_MISMATCH");
            Need(hud.instruction.find("sector")==std::string::npos&&hud.context.find("sector")==std::string::npos,"HUD_TARGET_SECTOR_LEAK");
            DrawList measured;const auto layout=FieldPresentation::MeasureLayout(measured,hud,g.session,{},g.actionFeedback,g.started,g.paused);
            Need(layout.bottom.Bottom()<=720&&layout.bottom.top>=576,"NEAR_HUD_LAYOUT_CHANGED");
            if(localIndex==0){Step(g,{'1'});Step(g,{});Step(g,{'E'});Need((g.session.relayFieldTasks&FieldRelayTask::WestCircuitBit)!=0&&g.session.relayLinked==0,"TRUE_E_WRONG_WEST_OPERATION");}
            if(localIndex==2){Step(g,{'3'});Step(g,{});Step(g,{'E'});Need((g.session.relayFieldTasks&FieldRelayTask::NorthWestRouteBit)!=0&&g.session.relayLinked==0,"TRUE_E_WRONG_ROUTE_OPERATION");}
            for(unsigned n=0;n<16&&g.session.RelaySignal(localIndex)!=100;++n){Step(g,{});Step(g,{'2'});}
            Need(g.session.RelaySignal(localIndex)==100,"TRUE_NEAR_AIM");Step(g,{});Step(g,{'E'});
            Need(g.session.relayLinked==(1u<<localIndex)&&!(g.session.relayLinked&(1u<<preferred))&&Tracked(g)==preferred,"TRUE_E_LINKED_NAVIGATION_NOT_NEAR");
            ++nearPairs;
        }Need(nearPairs==6,"EXACT_SIX_NEAR_PAIRS");
        // 真E完成目前追蹤站後，Main應立即清選擇並回到另一個未完成站。
        auto p=Seed(StationFeet(2));auto& g=*p;Step(g,{VK_ESCAPE});
        for(unsigned n=0;n<3;++n){Step(g,{VK_RIGHT});Step(g,{});}
        Step(g,{VK_RETURN});Need(Tracked(g)==2,"SELF_STATION_TRACKING");Step(g,{VK_ESCAPE});Step(g,{});
        Step(g,{'3'});Step(g,{});Step(g,{'E'});
        for(unsigned n=0;n<16&&g.session.RelaySignal(2)!=100;++n){Step(g,{});Step(g,{'2'});}
        Step(g,{});Step(g,{'E'});Need((g.session.relayLinked&4u)&&Tracked(g)==None&&Preview(g)==None&&
            !Same(View(g).target,Session::relaySites[2]),"TRUE_COMPLETED_STATION_NOT_FALLBACK");++groups;}
    void TuningConfirmationPriority(){using namespace TrackingTest;
        const auto aim=[&](){
            auto p=Seed(StationFeet(1));auto& g=*p;
            // 至少真按一次；最後一次調向仍保持 Down，但下一 tick 沒有新 Pressed。
            unsigned presses{};
            do{Step(g,{});Step(g,{'2'});++presses;}
            while(g.session.RelaySignal(1)!=100&&presses<16);
            Need(g.session.RelaySignal(1)==100&&g.tuningInputRepeat.ArmedMask()==2,"CONFIRM_GREEN_HELD_FIXTURE");
            return p;
        };
        const auto approachRepeat=[&](MistwatchGame& g){
            unsigned ticks{};
            while(g.tuningInputRepeat.Remaining(1)>Dt&&ticks++<30)Step(g,{'2'});
            Need(ticks<30&&g.tuningInputRepeat.Remaining(1)>0&&g.tuningInputRepeat.Remaining(1)<=Dt,
                "CONFIRM_NEXT_TICK_CROSSES_REPEAT");
            Need(g.session.RelaySignal(1)==100,"CONFIRM_LOCK_LOST_BEFORE_EDGE");
        };
        // 第一輪 .35 秒與後續 .12 秒連發都要覆蓋；釋鍵對照不能取代仍按住的案例。
        for(unsigned mode=0;mode<3;++mode){
            auto p=aim();auto& g=*p;
            if(mode==2){
                unsigned ticks{};const auto sector=g.session.RelaySector(1);
                do{Step(g,{'2'});++ticks;}while(g.session.RelaySector(1)==sector&&ticks<30);
                while(g.session.RelaySignal(1)!=100&&ticks<160){Step(g,{'2'});++ticks;}
                Need(ticks>1&&ticks<160&&g.session.RelaySignal(1)==100,"CONFIRM_PERIODIC_LOCK_FIXTURE");
            }
            approachRepeat(g);const auto sector=g.session.RelaySector(1);const auto serial=g.playablePairState.lastSerial;
            if(mode==0)Step(g,{'E'});else Step(g,{'2','E'});
            std::cout<<"TUNING_CONFIRM mode="<<mode<<" sector_before="<<sector<<" sector_after="<<g.session.RelaySector(1)
                <<" signal_after="<<g.session.RelaySignal(1)<<" linked="<<g.session.relayLinked<<'\n';
            Need(g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted&&g.playablePairState.lastSerial==serial+1,
                "CONFIRM_TRUE_MAIN_NOT_COMMITTED");
            Need(g.session.relayLinked==2&&g.session.RelaySector(1)==sector,"HELD_REPEAT_STOLE_EXPLICIT_CONFIRMATION");
            Need(g.playableInputs.Count()==0,"CONFIRM_FIFO_NOT_CONSUMED");
            for(unsigned n=0;n<12;++n)Step(g,{'2','E'});
            Need(g.session.relayLinked==2&&g.session.RelaySector(1)==sector&&g.tuningInputRepeat.ArmedMask()==0,
                "CONFIRM_HELD_KEYS_REEDITED_LINKED_STATION");
        }
        // 沒有 E 的正常連發不能被抑制。
        {auto p=aim();auto& g=*p;approachRepeat(g);const auto sector=g.session.RelaySector(1);
            Step(g,{'2'});Need(g.session.RelaySector(1)!=sector&&g.session.RelaySignal(1)<100&&g.session.relayLinked==0,
                "CONFIRM_CHANGE_DISABLED_ORDINARY_REPEAT");}
        // 同批次的新調向仍優先於 E：不能忽略玩家剛按的數字鍵而替他連線。
        for(unsigned key:{unsigned('1'),unsigned('2')}){
            auto p=aim();auto& g=*p;Step(g,{});const auto sector=g.session.RelaySector(1);
            Step(g,{key,'E'});Need(g.session.RelaySector(1)!=sector&&g.session.RelaySignal(1)<100&&g.session.relayLinked==0,
                "CONFIRM_CHANGE_IGNORED_FRESH_TUNING_EDGE");
        }
        // 按住 E 不等於新的確認邊沿；後續調向仍照常連發。
        {auto p=aim();auto& g=*p;Step(g,{});Step(g,{'2','E'});
            Need(g.session.relayLinked==0,"CONFIRM_HELD_E_NEGATIVE_FIXTURE");
            const auto sector=g.session.RelaySector(1);unsigned ticks{};
            while(g.tuningInputRepeat.Remaining(1)>Dt&&ticks++<30)Step(g,{'2','E'});
            Step(g,{'2','E'});Need(ticks<30&&g.session.RelaySector(1)!=sector&&g.session.relayLinked==0,
                "CONFIRM_HELD_E_SUPPRESSED_REPEAT_OR_RECONFIRMED");}
        // 保險絲與發電機走真 OnUpdate；fixture 只用 Session 的合法進程操作準備。
        const auto ready=[&](bool generator){
            Session s;
            for(unsigned i=0;i<3;++i){s.feet=Session::supplies[i];Need(s.Collect(i),"CONFIRM_STAGE_SUPPLY");}
            for(unsigned i=0;i<3;++i){s.feet=Session::repairSites[i];Need(s.CollectRepairMaterial(i,i==0),"CONFIRM_STAGE_MATERIAL");}
            s.feet=Session::bridge;Need(s.RepairBridge(),"CONFIRM_STAGE_BRIDGE");const auto enemy=Defeated();
            s.feet=enemy.feet;Need(s.FinishEncounter(enemy.feet),"CONFIRM_STAGE_ENCOUNTER");
            s.feet=Session::rangerLog;Need(s.ReadFuseClue(),"CONFIRM_STAGE_CLUE");s.feet=Session::fuse;
            for(unsigned i=0;i<3;++i)for(unsigned n=0;n<Session::fuseCode[i]-(i==0&&!generator?1u:0u);++n)
                Need(s.CycleFuseDial(i),"CONFIRM_STAGE_DIAL");
            if(generator){Need(s.CollectFuse(),"CONFIRM_STAGE_FUSE");s.feet=Session::generator;
                Need(s.ToggleCircuit(2),"CONFIRM_STAGE_CIRCUIT");s.feet.z-=1.5f;}
            Need(s.Valid(),"CONFIRM_LEGAL_STAGE_FIXTURE");return s;
        };
        for(bool generator:{false,true})for(unsigned mode=0;mode<4;++mode){
            const auto session=ready(generator);auto p=Seed(session.feet,&session);auto& g=*p;
            const auto value=[&](){return generator?g.session.circuit:g.session.fuseDials;};
            const auto completed=[&](){return generator?g.session.powerRestored:g.session.fuseCollected;};
            if(mode==2){Step(g,{'1','E'});Need(completed(),"CONFIRM_FRESH_TUNE_THEN_E_CANNOT_COMPLETE");}
            else{
                Step(g,{'1'});Need(!completed(),"CONFIRM_TUNING_AUTO_COMPLETED_OBJECTIVE");
                if(mode==1){Step(g,{});Step(g,{'1','E'});Need(!completed(),"CONFIRM_FRESH_WRONG_TUNE_WAS_IGNORED");}
                else{
                    if(mode==3){Step(g,{});Step(g,{'1'});}
                    unsigned ticks{};
                    while(g.tuningInputRepeat.Remaining(0)>Dt&&ticks++<30)Step(g,{'1'});
                    Need(ticks<30&&g.tuningInputRepeat.Remaining(0)>0&&g.tuningInputRepeat.Remaining(0)<=Dt,
                        "CONFIRM_STAGE_NEXT_REPEAT_BOUNDARY");
                    const auto before=value();const auto remaining=g.tuningInputRepeat.Remaining(0);Step(g,{'1','E'});
                    Need(value()==before,"CONFIRM_STAGE_AUTOREPEAT_CHANGED_EXPLICIT_CONFIRMATION");
                    Need(completed()==(mode==0),"CONFIRM_STAGE_WRONG_COMPLETION");
                    if(mode==3){
                        Need(g.tuningInputRepeat.Remaining(0)>0&&g.tuningInputRepeat.Remaining(0)<=.12L,
                            "CONFIRM_FAILED_E_RESTARTED_HOLD_DELAY");
                        Need(g.tuningInputRepeat.Remaining(0)==.12L-(static_cast<long double>(Dt)-remaining),
                            "CONFIRM_FAILED_E_LOST_EXACT_REPEAT_FRACTION");
                        unsigned after{};
                        while(value()==before&&after++<9)Step(g,{'1','E'});
                        Need(after>1&&after<=9&&value()!=before&&!completed(),"CONFIRM_FAILED_E_LOST_NORMAL_HELD_REPEAT");
                    }
                }
            }
            Need(g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted&&g.playableInputs.Count()==0&&g.CaptureSaveGame().Valid(),
                "CONFIRM_STAGE_NOT_VALID_COMMITTED_GAME");
            std::cout<<"TUNING_CONFIRM stage="<<(generator?"generator":"fuse")<<" mode="<<mode<<" completed="<<completed()<<'\n';
        }
        // 同批混合舊長按與新按鍵：只排除舊鍵，不能整批取消或整批放行。
        {auto session=ready(true);Need(session.ToggleCircuit(2)&&session.circuit==0,"CONFIRM_MIXED_CIRCUIT_FIXTURE");
            auto p=Seed(session.feet,&session);auto& g=*p;Step(g,{'1'});Need(g.session.circuit==3,"CONFIRM_MIXED_ARM");
            unsigned ticks{};while(g.tuningInputRepeat.Remaining(0)>Dt&&ticks++<30)Step(g,{'1'});
            Need(ticks<30&&g.session.circuit==3&&g.tuningInputRepeat.Remaining(0)<=Dt,"CONFIRM_MIXED_DEADLINE");
            Step(g,{'1','3','E'});
            Need(g.session.circuit==7&&g.session.powerRestored&&g.playableInputs.Count()==0&&
                g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"CONFIRM_MIXED_MASK_NOT_FRESH_ONLY");
            std::cout<<"TUNING_CONFIRM mixed_held1_fresh3_E=pass circuit=7 powered=1\n";}
        ++groups;
    }
    template<class G>void InvalidHud(G guide,const MistwatchGame& g){using namespace TrackingTest;
        if constexpr(requires{guide.relayTrackingActive;guide.navigationRelayIndex;}){
            const auto reject=[&](const Session& session,const G& malformed,const char* reason){
                const auto hud=FieldPresentation::Build(session,malformed,g.encounter,true,false,false);
                Need(!hud.valid&&hud.objective=="任務資料待確認"&&hud.navigation.empty()&&hud.instruction.empty()&&
                    hud.context.empty()&&hud.controls.empty()&&hud.journal.empty(),reason);
            };
            for(unsigned invalid:{None,(std::numeric_limits<unsigned>::max)()}){
                auto bad=guide;bad.relayTrackingActive=true;bad.navigationRelayIndex=invalid;
                reject(g.session,bad,"INVALID_HUD_TRACKING_NOT_REJECTED");
            }
            auto completed=g.session;Complete(completed,2);auto bad=guide;
            bad.relayTrackingActive=true;bad.navigationRelayIndex=2;
            reject(completed,bad,"COMPLETED_HUD_TRACKING_NOT_REJECTED");
            Session fresh;reject(fresh,bad,"WRONG_STAGE_HUD_TRACKING_NOT_REJECTED");
            bad.hasTarget=false;reject(g.session,bad,"MISSING_TARGET_HUD_TRACKING_NOT_REJECTED");
            // 未開啟tracking時，新欄位不得改變舊手工Guide fixture的任何HUD欄位。
            auto ordinary=guide;ordinary.relayTrackingActive=false;
            const auto before=FieldPresentation::Build(g.session,ordinary,g.encounter,true,false,false);
            ordinary.navigationRelayIndex=(std::numeric_limits<unsigned>::max)();
            const auto after=FieldPresentation::Build(g.session,ordinary,g.encounter,true,false,false);
            Need(before.valid==after.valid&&before.compactJournal==after.compactJournal&&before.objective==after.objective&&
                before.navigation==after.navigation&&before.instruction==after.instruction&&before.context==after.context&&
                before.controls==after.controls&&before.journal==after.journal,"NON_TRACKING_FIXTURE_CHANGED");
        }
    }
    template<class D>void InvalidMap(D d){using namespace TrackingTest;
        if constexpr(requires{d.trackedRelay;}){
            d.trackedRelay=(std::numeric_limits<unsigned>::max)();DrawList draw;const auto size=draw.vertices.size();bool rejected{};
            try{(void)Map::Draw(draw,d);}catch(const std::runtime_error&){rejected=true;}
            Need(rejected&&draw.vertices.size()==size&&draw.worldVertices.empty(),"INVALID_TRACKED_MAP_SUBMITTED");
        }
    }
    void MapLayout(){using namespace TrackingTest;auto p=Seed();auto& g=*p;PickNorthWest(g);Step(g,{});Step(g,{VK_RIGHT});
        const auto d=MapView(g);MapTracking(d,2,0);DrawList draw;const auto layout=Map::MeasureLayout(draw,d);const auto text=LayoutText(layout);
        Need(text.find("追蹤")!=std::string::npos&&text.find("待確認")!=std::string::npos&&
            text.find("Enter")!=std::string::npos&&text.find("sector")==std::string::npos&&text.find("2 - 0 - 3")==std::string::npos,"MAP_TRACKING_TEXT_OR_LEAK");
        for(unsigned i=0;i<layout.count;++i){const auto& line=layout.lines[i];const auto size=draw.MeasureSystemText(line.text,line.pixels);
            Need(line.pixels>=18.2f&&line.position.x+size.x<=1260&&line.position.y>=176&&line.position.y+size.y<=552,"MAP_FONT_OR_CONTAINMENT");}
        const auto receipt=Map::Draw(draw,d);Need(receipt.drawn&&receipt.tiles==750&&draw.Stats2D().textGlyphs>0&&
            draw.worldVertices.empty()&&draw.worldIndices.empty()&&draw.meshInstances.empty()&&draw.skinnedMeshInstances.empty(),"MAP_GPU_OR_TILE_SCOPE");
        const auto plain=Map::Build(g.session,View(g),g.lastRenderedCamera,g.terrain);DrawList unselected;Map::Draw(unselected,plain);
        Need(draw.Stats2D().submittedTriangles>unselected.Stats2D().submittedTriangles,"TRACKING_OUTER_RINGS_NOT_SUBMITTED");InvalidMap(d);InvalidHud(View(g),g);++groups;}
    void LoadAndReset(){using namespace TrackingTest;auto p=Seed();auto& g=*p;
        g.save=output_/"valid.save";const auto snapshot=g.CaptureSaveGame();Need(snapshot.Valid(),"LOAD_SNAPSHOT_VALID");
        Need(WriteSaveFile(g.save,snapshot)==SaveFileResult::Saved,"WRITE_TEST_OWNED_VALID_SAVE");PickNorthWest(g);
        Step(g,{VK_LEFT,VK_ESCAPE});Step(g,{VK_LEFT});const float yaw=g.orbit.yaw;
        const auto validPath=g.save;g.save=output_/"invalid.save";
        {std::ofstream file(g.save,std::ios::binary);file<<"invalid\n";file.close();Need(bool(file),"WRITE_TEST_OWNED_INVALID_SAVE");}
        Step(g,{VK_LEFT,VK_F9,VK_ESCAPE});Need(g.message=="Save unavailable"&&Tracked(g)==2&&!g.paused&&g.orbit.yaw==yaw,"FAILED_LOAD_CHANGED_TRACKING_OR_PRIORITY");
        g.save=validPath;Step(g,{VK_LEFT});Step(g,{VK_LEFT,VK_F9,VK_ESCAPE});
        Need(g.message=="Loaded"&&Tracked(g)==None&&Preview(g)==None&&!g.paused,"TRUE_LOAD_DID_NOT_CLEAR_SELECTION");
        const float loadedYaw=g.orbit.yaw;Step(g,{VK_LEFT});Need(g.orbit.yaw==loadedYaw,"LOAD_RELEASE_BARRIER_LEAKED");
        Step(g,{});Step(g,{VK_LEFT});Need(g.orbit.yaw!=loadedYaw,"LOAD_NEW_ARROW_BLOCKED");
        Step(g,{});PickNorthWest(g);Step(g,{VK_ESCAPE});
        g.session.Damage(100);Step(g,{});Step(g,{'R'});
        Need(g.session.Alive()&&Tracked(g)==None&&Preview(g)==None&&Same(g.session.feet,Session::bridge),"TRUE_DEATH_RETRY_KEPT_TRACKING");++groups;}
public:
    MistwatchSaveInputProbe(Aevrynd::Engine& engine,Aevrynd::SkinnedModel3D model,std::filesystem::path output):engine_(engine),raw_(std::move(model)),output_(std::move(output)){}
    void Run(){using namespace TrackingTest;Headless();SelectionAndResume();CancelAndHeld();PureGuideAndFallback();NearPairs();TuningConfirmationPriority();MapLayout();LoadAndReset();
        Need(groups==7&&nearPairs==6,"TRACKING_EXACT_GROUPS");std::cout<<"RELAY_TRACKING_TRUE_MAIN_CPU_PASS groups="<<groups<<" near_pairs="<<nearPairs<<" checks="<<checks<<" frames="<<frames
            <<" true_OnUpdate=1 OnRender=0 window=0 GPU=false\n";}
};
int main(int argc,char** argv){try{using namespace TrackingTest;Need(argc==3,"RAW_BOAR_AND_FRESH_OUTPUT_REQUIRED");
    const std::filesystem::path output=argv[2];Need(output.is_absolute()&&!std::filesystem::exists(output)&&std::filesystem::create_directories(output),"FRESH_TEST_OUTPUT_REQUIRED");
    const auto loaded=Aevrynd::LoadSkinnedModelAssetFile(argv[1]);Need(bool(loaded),"RAW_BOAR_REQUIRED");
    Aevrynd::Engine engine;MistwatchSaveInputProbe probe(engine,loaded.model,output);probe.Run();return 0;
}catch(const std::exception& error){std::cerr<<"RELAY_TRACKING_FIRST_DIVERGENCE="<<error.what()<<'\n';return 1;}}
