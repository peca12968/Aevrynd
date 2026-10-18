#include "MistwatchObjectiveGuide.h"
#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include "MistwatchRangerHut.h"
#include "MistwatchWorldProfile.h"
#include <type_traits>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace Mistwatch;
using namespace Aevrynd;
namespace {
unsigned checks{},cases{};
void Check(bool condition,const char* reason) {
    ++checks;if(!condition)throw std::runtime_error(reason);
}
template<class Fn> void Case(const char* name,Fn body) {
    try {body();++cases;std::cout<<"PASS "<<name<<'\n';}
    catch(const std::exception& error) {std::cerr<<"FIRST_DIVERGENCE case="<<name<<" check="<<checks<<" reason="<<error.what()<<'\n';throw;}
}
// 只準備原正例缺的局部字段；副本真調至 green 驗未確認拒絕，不寫 production gate。
void ConfirmLocalRelayFixture(Session& s,unsigned index) {
    Check(index<3&&s.Valid(),"local relay fixture input invalid");
    if(index==1)return;
    const auto feetBefore=s.feet;
    const auto maskBefore=s.relayLinked,anglesBefore=s.relaySectors;
    s.feet=Session::relaySites[index];
    auto unconfirmed=s;
    for(unsigned turn=0;turn<16&&unconfirmed.RelaySector(index)!=Session::RelayTargetSector(index);++turn)
        Check(unconfirmed.TuneRelay(index,1),"local relay fixture probe tuning failed");
    Check(unconfirmed.RelaySector(index)==Session::RelayTargetSector(index),"local relay fixture probe did not reach green");
    const auto beforeLocal=unconfirmed.Serialize();
    Check(!beforeLocal.empty()&&!unconfirmed.ConnectRelay(index)&&unconfirmed.Serialize()==beforeLocal,
        "unconfirmed local relay connected or changed state");
    if(index==0)Check(s.CycleRelayPatch(0)&&s.ConfirmRelayCircuit(),"west local fixture confirmation failed");
    else Check(s.CycleExtractionRoute()&&s.ConfirmExtractionRoute(),"northwest local fixture confirmation failed");
    Check(s.RelayFieldTaskReady(index)&&s.relayLinked==maskBefore&&s.relaySectors==anglesBefore,
        "local confirmation changed original link or tuning fields");
    s.feet=feetBefore;
}
Session State(Stage stage) {
    Session s;
    if(stage>=Stage::RepairMaterials)s.collected=7;
    if(stage>=Stage::Bridge)s.repairMaterials=7;
    if(stage>=Stage::Encounter)s.bridgeRepaired=true;
    if(stage>=Stage::Fuse)s.guardianDefeated=true;
    if(stage>=Stage::Generator){s.fuseClueRead=true;s.fuseDials=Session::solvedFuseDials;s.fuseCollected=true;}
    if(stage>=Stage::Relays){s.circuit=7;s.powerRestored=true;}
    if(stage>=Stage::Beacon) {
        ConfirmLocalRelayFixture(s,0);ConfirmLocalRelayFixture(s,2);
        s.relayLinked=7;for(unsigned i=0;i<3;++i)s.relaySectors|=Session::RelayTargetSector(i)<<(i*4);
    }
    if(stage==Stage::Complete)s.beaconLit=true;
    Check(s.Valid(),"test fixture must be a valid Session");return s;
}
Camera3D Camera(float yaw=0) {
    Camera3D c;c.target={0,1,0};c.position={6*std::sin(yaw),3,-6*std::cos(yaw)};return c;
}
bool Same(Vec3 a,Vec3 b) {return a.x==b.x&&a.y==b.y&&a.z==b.z;}
void NoSecret(const ObjectiveGuide& g) {
    const auto text=g.objective+g.navigation+g.instruction+g.context;
    Check(text.find("2 - 0 - 3")==std::string::npos,"unread ranger code leaked");
    Check(text.find("sector")==std::string::npos,"relay target sector leaked");
}
}
int main() {
    try {
        HeightField3D terrain({{-64,0,-16},129,145,1});terrain.Fill(0);
        Encounter enemy;Evasion evasion;const std::vector<BoxCollider3D> empty;
        const auto build=[&](const Session& s,bool crouched=false,bool grounded=true,const Camera3D& c=Camera()) {
            return BuildObjectiveGuide(s,enemy,evasion,c,crouched,grounded,empty,terrain);
        };
        Case("all-nine-stages-and-terminal-semantics",[&] {
            for(unsigned i=0;i<9;++i){const auto s=State(static_cast<Stage>(i));const auto g=build(s);
                Check(g.valid,"valid stage lost");Check(g.hasTarget==(i<8),"target must exist only before completion");
                Check(g.navigation.size()<130&&g.instruction.size()<130&&g.context.size()<130,"HUD line exceeds 130 ASCII characters");}
            auto dead=State(Stage::Fuse);dead.health=0;const auto d=build(dead);
            Check(!d.hasTarget&&!d.useReady&&d.instruction.find("R: retry")!=std::string::npos,"death must advertise retry, not use");
            const auto done=build(State(Stage::Complete));Check(done.instruction.find("R: new")!=std::string::npos,"completion must advertise new expedition");
        });
        Case("supplies-nearest-completed-and-stable-tie",[&] {
            Session s;s.feet=Session::supplies[1];auto g=build(s);Check(g.index==1&&g.useReady,"nearest crate use missing");
            s.collected=2;g=build(s);Check(g.index!=1&&!g.useReady,"collected crate retained");
            s={};s.feet={-.5f,0,10};g=build(s);Check(g.index==0,"equal-distance tie must keep author index");
            for(unsigned mask=0;mask<7;++mask){s={};s.collected=mask;g=build(s);Check(!(mask&(1u<<g.index)),"target is already collected");NoSecret(g);}
        });
        Case("materials-nearest-and-truthful-use-grid",[&] {
            for(unsigned site=0;site<3;++site)for(bool crouched:{false,true})for(float x:{0.f,.8f,.81f,2.5f,2.51f})for(float y:{0.f,.3f,.31f,2.f,2.01f}) {
                auto s=State(Stage::RepairMaterials);s.repairMaterials=7^(1u<<site);s.feet=Session::repairSites[site]+Vec3{x,y,0};
                auto expected=s;const bool ready=expected.CollectRepairMaterial(site,crouched);const auto g=build(s,crouched);
                Check(g.index==site&&Same(g.target,Session::repairSites[site]),"material target drift");Check(g.useReady==ready,"material hint differs from Session gate");
                Check((g.instruction.find("E:")!=std::string::npos)==ready,"far or blocked material advertised E");}
        });
        Case("bridge-intent-is-attempt-only-and-read-only",[&] {
            auto s=State(Stage::Bridge);s.feet=Session::bridge+Vec3{0,0,3};auto g=build(s);
            Check(!g.useReady&&g.bridgeIntent==BridgeRepairIntent::None,"far bridge must not advertise an attempt");
            s.feet=Session::bridge;const auto snapshot=s.Serialize();g=build(s);
            Check(!g.useReady&&g.bridgeIntent==BridgeRepairIntent::AttemptRequiresAuthoritativeCheck,"near bridge intent must never become Ready");
            Check(g.instruction.find("attempt repair")!=std::string::npos&&g.instruction.find("checked on use")!=std::string::npos,"attempt qualification missing");
            Check(s.Serialize()==snapshot,"bridge intent modified Session");
            auto obstructed=Camera();obstructed.position={0,0,36};g=build(s,false,true,obstructed);
            Check(!g.useReady&&g.bridgeIntent==BridgeRepairIntent::AttemptRequiresAuthoritativeCheck,"blocked camera must not grant readiness");
        });
        Case("single-site-E-boundaries-match-real-Session",[&] {
            for(const auto stage:{Stage::Supplies,Stage::Bridge,Stage::Generator,Stage::Beacon})for(float x:{0.f,2.5f,2.501f})for(float y:{0.f,2.f,2.001f}) {
                auto s=State(stage);Vec3 target{};
                if(stage==Stage::Supplies){s.collected=6;target=Session::supplies[0];}
                else if(stage==Stage::Bridge)target=Session::bridge;
                else if(stage==Stage::Generator){s.circuit=7;target=Session::generator;}
                else target=Session::beacon;
                s.feet=target+Vec3{x,y,0};auto expected=s;bool ready{};
                if(stage==Stage::Supplies)ready=expected.Collect(0);
                else if(stage==Stage::Bridge)ready=expected.RepairBridge();
                else if(stage==Stage::Generator)ready=expected.RepairGenerator();
                else ready=expected.LightBeacon();
                const auto before=s.Serialize();const auto g=build(s);
                if(stage==Stage::Bridge) {
                    Check(!g.useReady,"bridge must never grant prevalidated readiness");
                    Check((g.bridgeIntent==BridgeRepairIntent::AttemptRequiresAuthoritativeCheck)==ready,"bridge intent differs from original Session eligibility");
                } else Check(g.useReady==ready,"single-site E hint differs from original Session boundary");
                Check(s.Serialize()==before,"guide must not modify quest state while checking E");
            }
        });
        Case("actual-camera-bearings-and-vertical-target",[&] {
            auto s=State(Stage::Bridge);s.feet={0,0,20};
            Check(build(s,false,true,Camera()).bearing==ObjectiveBearing::Ahead,"yaw zero forward mismatch");
            Check(build(s,false,true,Camera(3.14159265f)).bearing==ObjectiveBearing::Behind,"actual reversed camera ignored");
            Check(build(s,false,true,Camera(1.57079633f)).bearing==ObjectiveBearing::Right,"right bearing sign mismatch");
            Check(build(s,false,true,Camera(-1.57079633f)).bearing==ObjectiveBearing::Left,"left bearing sign mismatch");
            for(unsigned i=0;i<32;++i){const float yaw=float(i)*6.2831853f/32.f;const auto g=build(s,false,true,Camera(yaw));
                Check(g.valid&&std::abs(g.distance-10)<1e-6,"camera altered horizontal distance");
                const double expected=std::atan2(std::sin(double(yaw)),std::cos(double(yaw)));
                Check(std::abs(std::remainder(g.relativeRadians-expected,6.283185307179586))<1e-5,"camera-relative bearing mismatch");}
            s.feet=Session::bridge+Vec3{0,3,0};const auto g=build(s);Check(g.distance==0&&!g.useReady,"XZ arrival must not bypass vertical gate");
        });
        Case("fuse-clue-switch-no-leak-and-interior-boundaries",[&] {
            auto s=State(Stage::Fuse);s.feet=Session::fuse;s.fuseDials=Session::solvedFuseDials;auto g=build(s);
            Check(Same(g.target,Session::rangerLog)&&!g.useReady,"unread clue must target ranger log");NoSecret(g);
            s.feet=Session::rangerLog;g=build(s);Check(g.useReady&&!g.tuneReady,"log readiness missing");NoSecret(g);
            s.fuseClueRead=true;s.fuseDials=0;s.feet=Session::fuse+Vec3{.96f,0,0};g=build(s);
            Check(Same(g.target,Session::fuse)&&!g.useReady&&!g.tuneReady,"cabinet outside interior advertised action");
            s.feet=Session::fuse;g=build(s);Check(!g.useReady&&g.tuneReady&&g.context.find("2 - 0 - 3")!=std::string::npos,"read clue or dial feedback missing");
            s.fuseDials=Session::solvedFuseDials;g=build(s);Check(g.useReady,"solved cabinet does not advertise E");
            for(float x:{0.f,.95f,.951f})for(float y:{0.f,.35f,.351f}){s.feet=Session::fuse+Vec3{x,y,0};auto expected=s;
                Check(build(s).useReady==expected.CollectFuse(),"cabinet hint differs from Session gate");}
        });
        Case("generator-far-near-and-not-solved-for-player",[&] {
            auto s=State(Stage::Generator);auto g=build(s);Check(Same(g.target,Session::generator)&&!g.useReady&&!g.tuneReady,"far generator hint invalid");
            Check(g.context.find("Supplies")==std::string::npos,"generator stale supplies text");
            s.feet=Session::generator;g=build(s);Check(g.tuneReady&&!g.useReady,"unlit circuit must allow panel, not repair");
            Check(g.instruction.find("1 then 3")==std::string::npos,"generator solution sequence leaked");
            s.circuit=7;g=build(s);Check(g.useReady,"lit generator E missing");
        });
        Case("relay-completed-nearest-ground-and-green-lock",[&] {
            for(unsigned mask=0;mask<7;++mask){auto s=State(Stage::Relays);
                if(mask&1u)ConfirmLocalRelayFixture(s,0);
                if(mask&4u)ConfirmLocalRelayFixture(s,2);
                s.relayLinked=mask;
                for(unsigned i=0;i<3;++i)if(mask&(1u<<i))s.relaySectors|=Session::RelayTargetSector(i)<<(i*4);
                auto g=build(s);Check(!(mask&(1u<<g.index)),"completed relay retained");NoSecret(g);
                const unsigned selected=g.index;s.feet=Session::relaySites[selected];g=build(s);Check(g.tuneReady,"grounded near relay cannot tune");
                s.relaySectors=(s.relaySectors&~(15u<<(selected*4)))|(Session::RelayTargetSector(selected)<<(selected*4));
                ConfirmLocalRelayFixture(s,selected);
                g=build(s);Check(g.useReady,"green relay link missing");NoSecret(g);
                s.feet.y+=.121f;g=build(s);Check(!g.useReady&&!g.tuneReady,"airborne relay action advertised");
                Check(g.context.find("SIGNAL")==std::string::npos,"far/airborne relay reveals near signal");}
        });
        Case("beacon-from-every-last-relay-and-at-target",[&] {
            for(unsigned i=0;i<3;++i){auto s=State(Stage::Beacon);s.feet=Session::relaySites[i];const auto g=build(s);
                Check(Same(g.target,Session::beacon)&&!g.useReady,"beacon target/readiness wrong");
                Check(g.instruction.find("north")==std::string::npos,"beacon still hardcodes north");
                const double dz=double(Session::beacon.z)-s.feet.z;Check((std::cos(g.relativeRadians)>0)==(dz>0),"beacon forward/back sign contradicts actual coordinate");}
            auto s=State(Stage::Beacon);s.feet=Session::beacon;const auto g=build(s);Check(g.useReady&&g.bearing==ObjectiveBearing::AtTarget,"beacon near E missing");
        });
        Case("encounter-recovery-used-cooldown-range-dash-and-q",[&] {
            auto s=State(Stage::Encounter);s.feet=Session::arena+Vec3{0,0,2};enemy={};enemy.phase=EnemyPhase::Recovery;enemy.phaseTime=1;
            auto g=build(s);Check(g.counterReady&&g.instruction.find("F:")!=std::string::npos,"legal counter omitted");
            Check(g.evadeReady,"grounded Q omitted");auto next=enemy;const auto health=enemy.health;next.Tick(s.feet,1.f/60,true);Check(next.health==health-1,"advertised F not usable by Encounter");
            enemy.health=2;enemy.countered=true;g=build(s);Check(!g.counterReady&&g.instruction.find("F:")==std::string::npos,"used Recovery still advertises F");
            enemy.countered=false;enemy.playerCooldown=.1f;g=build(s);Check(!g.counterReady,"cooldown ignored");enemy.playerCooldown=0;
            s.feet.z+=1;g=build(s);Check(!g.counterReady,"out-of-range F advertised");s.feet=Session::arena+Vec3{0,2,1};g=build(s);Check(!g.counterReady,"strict vertical F boundary ignored");
            s.feet=Session::arena+Vec3{0,0,2};evasion.direction={0,0,1};evasion.remaining=.1f;evasion.cooldown=.9f;
            g=build(s);Check(!g.counterReady&&!g.evadeReady,"dashing must hide F and Q");evasion={};
            Check(!build(s,true).evadeReady&&!build(s,false,false).evadeReady,"crouch/air Q advertised");
            enemy={};enemy.phase=EnemyPhase::Windup;enemy.phaseTime=.5f;g=build(s);Check(!g.counterReady&&g.instruction.find("marked strike circle")!=std::string::npos,"windup instruction wrong");enemy={};
        });
        Case("encounter-wall-terrain-and-invalid-world-los",[&] {
            auto s=State(Stage::Encounter);s.feet=Session::arena+Vec3{0,0,2};enemy.phase=EnemyPhase::Recovery;enemy.phaseTime=1;
            const std::array wall{BoxCollider3D{{0,.65f,49},{.5f,1,.1f}}};
            auto g=BuildObjectiveGuide(s,enemy,evasion,Camera(),false,true,wall,terrain);Check(!g.counterReady,"wall-blocked F advertised");
            auto bad=wall;bad[0].extents.x=-1;g=BuildObjectiveGuide(s,enemy,evasion,Camera(),false,true,bad,terrain);Check(!g.counterReady,"invalid collider advertised F");
            auto ridge=terrain;Check(ridge.SetHeight(64,65,3),"terrain fixture edit failed");g=BuildObjectiveGuide(s,enemy,evasion,Camera(),false,true,empty,ridge);Check(!g.counterReady,"terrain-blocked F advertised");enemy={};
        });
        Case("load-retry-immediate-target-and-no-mutation",[&] {
            auto s=State(Stage::Fuse);s.fuseClueRead=true;s.feet=Session::fuse;const auto text=s.Serialize();Session loaded;
            Check(loaded.Restore(text),"real Session Restore failed");const auto before=loaded.Serialize();auto g=build(loaded);
            Check(Same(g.target,Session::fuse)&&loaded.Serialize()==before,"load target stale or mutated");
            loaded.Damage(100);Check(build(loaded).instruction.find("R: retry")!=std::string::npos,"death target stale");loaded.Retry();g=build(loaded);
            Check(Same(loaded.feet,Session::bridge)&&Same(g.target,Session::fuse)&&!g.useReady,"Retry must retain quest and update distance");
        });
        Case("invalid-input-fail-closed",[&] {
            Session s;auto c=Camera();s.feet.x=(std::numeric_limits<float>::quiet_NaN)();Check(!build(s).valid,"NaN Session accepted");
            s={};c.position=c.target;auto g=build(s,false,true,c);Check(!g.valid&&!g.useReady&&!g.counterReady,"zero horizontal camera accepted");
            c=Camera();c.target.z=(std::numeric_limits<float>::infinity)();Check(!build(s,false,true,c).valid,"nonfinite camera accepted");
            s=State(Stage::Encounter);enemy.phase=EnemyPhase::Recovery;enemy.phaseTime=1;enemy.playerCooldown=-1;Check(!build(s).valid,"invalid encounter accepted");enemy={};
        });
        Case("typed-intent-cannot-be-used-as-a-bool",[&] {
            static_assert(!std::is_convertible_v<BridgeRepairIntent,bool>);
            auto s=State(Stage::Bridge);s.feet=Session::bridge;
            const auto intent=EvaluateBridgeRepairIntent(s);
            Check(intent==BridgeRepairIntent::AttemptRequiresAuthoritativeCheck,"typed intent missing");
            Check(!build(s).useReady,"typed intent must not be converted into Ready");
        });
        Case("F9-R-retry-repair-atomic-stage-refresh",[&] {
            auto s=State(Stage::Bridge);s.feet=Session::bridge;
            Check(build(s).bridgeIntent==BridgeRepairIntent::AttemptRequiresAuthoritativeCheck,"initial attempt missing");
            Session loaded;Check(loaded.Restore(Session{}.Serialize()),"F9 unit fixture Restore failed");
            s=loaded;Check(build(s).bridgeIntent==BridgeRepairIntent::None,"F9 retained old bridge intent");
            s=State(Stage::Bridge);s.feet=Session::bridge;s.Damage(100);
            Check(build(s).bridgeIntent==BridgeRepairIntent::None,"death retained bridge intent");
            s.Retry();Check(build(s).bridgeIntent==BridgeRepairIntent::None,"retry at origin retained near intent");
            s.feet=Session::bridge;Check(build(s).bridgeIntent==BridgeRepairIntent::AttemptRequiresAuthoritativeCheck,"return after retry cannot attempt");
            Check(s.RepairBridge(),"real Session repair failed");
            Check(build(s).bridgeIntent==BridgeRepairIntent::None&&!build(s).useReady,"successful repair retained old bridge intent");
            s=Session{};Check(build(s).bridgeIntent==BridgeRepairIntent::None,"R new expedition retained intent");
        });
        Case("profile-world-terrain-changes-do-not-reuse-proof",[&] {
            auto s=State(Stage::Bridge);s.feet=Session::bridge;
            for(const auto profile:{WorldProfile::LegacyCore,WorldProfile::LegacyMiddle,WorldProfile::ForestCanopy,WorldProfile::ForestWood}) {
                (void)profile;
                for(float height:{0.f,50.f,(std::numeric_limits<float>::quiet_NaN)()}) {
                    auto changedTerrain=terrain;changedTerrain.Fill(height);
                    const std::array changedWorld{BoxCollider3D{{0,3,-6},{1,1,1}}};
                    const auto g=BuildObjectiveGuide(s,enemy,evasion,Camera(),false,true,changedWorld,changedTerrain);
                    Check(!g.useReady&&g.bridgeIntent==BridgeRepairIntent::AttemptRequiresAuthoritativeCheck,"changed world must not reuse prevalidated Ready");
                }
            }
        });
        Case("rendered-camera-not-input-or-candidate-camera",[&] {
            auto s=State(Stage::Bridge);s.feet={0,0,20};
            const auto production=build(s,false,true,Camera());
            const auto diagnostic=build(s,false,true,Camera(3.14159265f));
            Check(production.bearing==ObjectiveBearing::Ahead&&diagnostic.bearing==ObjectiveBearing::Behind,"actual diagnostic view ignored");
            Check(!production.useReady&&!diagnostic.useReady,"camera selection must not grant repair proof");
        });
        Case("bridge-attempt-read-only-and-authoritative-event",[&] {
            auto s=State(Stage::Bridge);s.feet=Session::bridge;unsigned eventCases{};
            const auto beforeFrames=s.Serialize();
            for(unsigned frame=0;frame<2000;++frame) {
                const auto g=build(s);Check(g.bridgeIntent==BridgeRepairIntent::AttemptRequiresAuthoritativeCheck&&!g.useReady,"repeated frames changed intent into proof");
            }
            // 這是狀態不變測試，不冒充未注入作者函式的呼叫次數量測。
            Check(s.Serialize()==beforeFrames,"repeated guide frames modified Session");
            const auto actualTerrain=CreateTerrain();
            for(const auto selector:{FirLayoutSelector::Legacy,FirLayoutSelector::ForestWood})for(bool blocked:{false,true}) {
                // 只在模擬 E 事件建立一次真作者 base world；不冒充完整 runtime canopy/native 測試。
                auto live=s;auto candidate=live;Check(candidate.RepairBridge(),"Session denied event");
                ++eventCases;const auto nextWorld=WorldSolids(true,selector);
                const std::array roof{RangerHutCameraRoofSolid()};
                auto c=Camera();c.position=blocked?Vec3{0,-.1f,36}:Vec3{0,3,28};
                const bool proof=ThirdPersonCameraPathClear3D(c.position,c.position,nextWorld,actualTerrain,roof,.16f);
                Check(proof!=blocked,"real engine camera predicate mismatched test event");
                const auto before=live.Serialize();if(proof)live=candidate;
                if(proof)Check(live.CurrentStage()==Stage::Encounter&&EvaluateBridgeRepairIntent(live)==BridgeRepairIntent::None,"commit did not atomically leave Bridge");
                else Check(live.Serialize()==before&&EvaluateBridgeRepairIntent(live)==BridgeRepairIntent::AttemptRequiresAuthoritativeCheck,"rejection changed Session or granted Ready");
            }
            Check(eventCases==4,"the four explicit E test cases did not all run");
        });
        std::cout<<"status=passed cases="<<cases<<" checks="<<checks<<" mode=isolated_cpu_attempt_adapter native_hud=unverified human_minutes=unverified\n";
        return 0;
    } catch(const std::exception&) {std::cerr<<"status=failed cases="<<cases<<" checks="<<checks<<'\n';return 1;}
}
