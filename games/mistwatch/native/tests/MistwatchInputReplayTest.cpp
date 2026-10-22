#include "MistwatchInputReplay.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace Mistwatch;
namespace {
void Need(bool good,const char* why){if(!good)throw std::runtime_error(why);}
constexpr float Dt=1.f/60.f;
void Tick(InputReplay& replay,const Session& session,float height=1.8f,bool started=true,float yaw=0.f){
    const auto before=session.Serialize();replay.Tick(session,yaw,height,started,false,Dt);
    Need(session.Serialize()==before,"Driver changed read-only Session");
}
bool Released(InputReplay& replay){for(unsigned key=0;key<256;++key)if(replay.Controls().Down(key)||replay.Controls().Pressed(key))return false;return true;}
// 合成觀測值只驗證 driver 狀態機，絕不是正式碰撞、鏡頭、按鍵處理器或通關證據。
void ReachInteraction(InputReplay& replay,Session& s){
    Tick(replay,s,1.8f,false);
    Need(replay.Controls().Down(VK_RETURN)&&replay.Controls().Pressed(VK_RETURN),"Enter press missing");
    Tick(replay,s);Need(!replay.Controls().Down(VK_RETURN)&&!replay.Controls().Pressed(VK_RETURN),"Enter release missing");
    Tick(replay,s);Tick(replay,s);
    Need(replay.Controls().Down('W')&&replay.Controls().Pressed('W'),"First walk press missing");
    Need(replay.Controls().Down(VK_SHIFT)==replay.CompletionRoute(),"Only complete route should run on the long road");
    if(replay.CompletionRoute()){
        auto nearReplay=replay;auto nearSession=s;nearSession.feet.z=17.f;
        Tick(nearReplay,nearSession);
        Need(nearReplay.Controls().Down('W')&&!nearReplay.Controls().Down(VK_SHIFT),"Run must release in final one metre approach");
        for(float distance:{2.f,3.f,4.f}){
            auto movingReplay=replay;auto movingSession=s;movingSession.feet.z=18.f-distance;
            movingReplay.Tick(movingSession,0,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{}, {0,0,9});
            if(distance==2)Need(Released(movingReplay),"Observed nine metre speed requires real braking before waypoint");
            else Need(movingReplay.Controls().Down('W')&&movingReplay.Controls().Down(VK_SHIFT)==(distance==4),
                "Shift release tracks observed stopping distance rather than a fixed six metre zone");
        }
    }
    Tick(replay,s);Need(replay.Controls().Down('W')&&!replay.Controls().Pressed('W'),"Held movement manufactured an edge");
    s.feet={0,0,18};Tick(replay,s);Need(!replay.Controls().Down('W'),"Waypoint movement not released");
    Tick(replay,s);Need(replay.Controls().Down('A'),"Approach direction incorrect");
    Need(!replay.Controls().Down(VK_SHIFT),"Shelter approach must not sprint");
    s.feet={-4.8f,0,18};Tick(replay,s);Tick(replay,s);
    Need(replay.Controls().Down(VK_CONTROL)&&replay.Controls().Pressed(VK_CONTROL),"Outside crouch missing");
    Tick(replay,s,1.4f);Need(replay.Controls().Down(VK_CONTROL)&&!replay.Controls().Pressed(VK_CONTROL),"Crouch hold edge incorrect");
    Tick(replay,s,1.4f);Need(replay.Controls().Down('A'),"Crouched entry direction incorrect");
    // 合成實際桌邊碰撞位置與微小橫向漂移，不能再要求到達桌子內的不可達點。
    s.feet={-7.25f,0,18.003f};Tick(replay,s,1.4f);
    Need(replay.Phase()=="interaction-release"&&!replay.Controls().Down('E'),"Interaction release phase missing");
    Tick(replay,s,1.4f);Need(!replay.Controls().Down('E'),"E not released for a real update");
    Tick(replay,s,1.4f);Need(replay.Controls().Down('E')&&replay.Controls().Pressed('E')&&replay.Phase()=="verify-tools","Single E edge missing");
}
void SuccessAndRequiredEvidence(){
    InputReplay replay;Session s;ReachInteraction(replay,s);
    auto rejected=replay;Tick(rejected,s,1.4f);
    Need(rejected.Finished()&&!rejected.Passed()&&rejected.Failure()=="production-E-did-not-collect-tools"&&Released(rejected),"No tool bit must fail and release controls");
    s.repairMaterials=1;Tick(replay,s,1.4f);
    Need(!replay.Controls().Down('E')&&!replay.Controls().Pressed('E')&&replay.Controls().Down(VK_CONTROL),"E must release while crouching");
    Need(replay.Phase()=="low-roof-release","Low roof verification was skipped");
    Tick(replay,s,1.4f);
    Need(Released(replay)&&replay.Phase()=="verify-low-roof","Low roof must receive a real Ctrl-release update");
    auto stoodInside=replay;Tick(stoodInside,s,1.8f);
    Need(stoodInside.Finished()&&!stoodInside.Passed()&&stoodInside.Failure()=="low-roof-incorrectly-stood"&&Released(stoodInside),"Standing under low roof must fail");
    auto movedInside=replay;auto movedSession=s;movedSession.feet.x=-4.8f;Tick(movedInside,movedSession,1.4f);
    Need(movedInside.Finished()&&!movedInside.Passed(),"Low roof observation cannot be moved outside");
    auto lostInside=replay;auto lostSession=s;lostSession.repairMaterials=0;Tick(lostInside,lostSession,1.4f);
    Need(lostInside.Finished()&&!lostInside.Passed(),"Low roof observation requires collected tools");
    Tick(replay,s,1.4f);
    Need(replay.Phase()=="exit-shelter"&&replay.Controls().Down(VK_CONTROL)&&replay.Controls().Pressed(VK_CONTROL),"Verified low roof must restore crouch with a real edge");
    Tick(replay,s,1.4f);Need(replay.Controls().Down('D'),"Exit direction incorrect");
    s.feet={-4.8f,0,18};Tick(replay,s,1.4f);Need(replay.Controls().Down(VK_CONTROL),"Exited segment released crouch early");
    Tick(replay,s,1.4f);Need(!replay.Controls().Down(VK_CONTROL)&&replay.Phase()=="verify-standing","Standing release update missing");
    auto inside=replay;auto insideSession=s;insideSession.feet.x=-5.8f;Tick(inside,insideSession);
    Need(inside.Finished()&&!inside.Passed(),"Standing inside shelter cannot pass");
    auto missing=replay;auto missingSession=s;missingSession.repairMaterials=0;Tick(missing,missingSession);
    Need(missing.Finished()&&!missing.Passed(),"Missing final tool bit cannot pass");
    Tick(replay,s,1.4f);Need(!replay.Finished(),"Crouched final observation cannot pass");
    Tick(replay,s);Need(replay.Passed()&&replay.Finished()&&replay.Failure().empty()&&Released(replay),"Outside standing with tool bit must pass");
    const auto count=replay.Ticks();Tick(replay,s);Need(replay.Ticks()==count&&Released(replay),"Completed replay remains inert");
}
void DeadlineAndInputSafety(){
    Session s;InputReplay stalled;Tick(stalled,s);
    for(unsigned i=0;i<605&&!stalled.Finished();++i)Tick(stalled,s);
    Need(stalled.Finished()&&!stalled.Passed()&&stalled.Failure()=="phase-stalled-600-ticks"&&stalled.Ticks()<=602&&Released(stalled),"No-progress deadline failed");
    InputReplay bounded;Tick(bounded,s);
    for(unsigned i=0;i<7210&&!bounded.Finished();++i){s.feet.z=-1000.f+float(i)*.025f;Tick(bounded,s);}
    Need(bounded.Finished()&&!bounded.Passed()&&bounded.Ticks()==7200&&bounded.Failure()=="total-tick-limit"&&Released(bounded),"Overall deadline failed");
    for(unsigned bad=0;bad<4;++bad){InputReplay replay;Session clean;const float nan=std::numeric_limits<float>::quiet_NaN();
        if(bad==0)clean.feet.x=nan;
        replay.Tick(clean,bad==1?nan:0.f,bad==2?nan:1.8f,false,false,bad==3?nan:Dt);
        Need(replay.Finished()&&!replay.Passed()&&Released(replay),"Nonfinite observation accepted");}
    InputReplay paused;Session clean;paused.Tick(clean,0,1.8f,true,true,Dt);
    Need(paused.Finished()&&!paused.Passed()&&Released(paused),"Paused handler must not be bypassed");
    InputReplay wrongOrigin;clean.feet.x=1;Tick(wrongOrigin,clean);
    Need(wrongOrigin.Finished()&&!wrongOrigin.Passed(),"Unexpected origin was silently accepted");
    InputReplay rotated;clean={};Tick(rotated,clean);Tick(rotated,clean,1.8f,true,1.57079632679f);
    Need(rotated.Controls().Down('D')&&!rotated.Controls().Down('W'),"World waypoint inverse yaw incorrect");
}
void BridgeRouteControls(InputReplay* combat=nullptr,Session* observation=nullptr,bool power=false,bool completion=false,bool death=false,bool checkpoint=false){
    InputReplay replay;Session s;
    Need(!replay.BridgeRoute()&&(checkpoint?replay.EnableCheckpointRoute():death?replay.EnableDeathRetryRoute():combat?(completion?replay.EnableCompletionRoute():power?replay.EnablePowerRoute():replay.EnableCombatRoute()):replay.EnableBridgeRoute())&&replay.BridgeRoute(),"Bridge route opt-in failed");
    Need(replay.CombatRoute()==(combat!=nullptr&&!death),"Combat route opt-in changed default mode");
    Need(replay.PowerRoute()==power,"Power route opt-in changed default mode");
    Need(replay.CompletionRoute()==completion,"Completion route opt-in changed default mode");
    Need(replay.DeathRetryRoute()==death,"Death route opt-in changed default mode");
    Need(replay.CheckpointRoute()==checkpoint,"Checkpoint route opt-in changed default mode");
    ReachInteraction(replay,s);
    Need(!replay.EnableBridgeRoute(),"Route changed after replay started");
    s.repairMaterials=1;Tick(replay,s,1.4f);Tick(replay,s,1.4f);Tick(replay,s,1.4f);
    s.feet={-4.8f,0,18};Tick(replay,s,1.4f);Tick(replay,s,1.4f);Tick(replay,s);
    Need(replay.Phase()=="bridge-road-18"&&!replay.Finished(),"Bridge route ended after shelter");
    Tick(replay,s);Need(replay.Controls().Down('D')&&!replay.Controls().Down('W'),"Return to road direction incorrect");
    auto dead=replay;auto deadSession=s;deadSession.health=0;Tick(dead,deadSession);
    Need(dead.Finished()&&!dead.Passed()&&Released(dead),"Dead route observation cannot pass");
    // 以下只供狀態機單測的合成觀測；不執行收集、修橋或控制器方法。
    struct Point {float x,z;unsigned action;const char* phase;};
    constexpr Point points[]{
        {0,18,0,"bridge-road-18"},{0,-8,0,"wood-1-road"},{6,-8,4,"wood-1-approach"},
        {0,-8,0,"wood-1-return"},{0,6,0,"bridge-road-6"},{2,6,1,"supply-0-approach"},
        {0,6,0,"supply-0-return"},{0,14,0,"bridge-road-14"},{-3,14,2,"supply-1-approach"},
        {0,14,0,"supply-1-return"},{0,22,0,"bridge-road-22"},{6,22,3,"supply-2-approach"},
        {0,22,0,"supply-2-return"},{0,26,0,"wood-2-road"},{-6,26,5,"wood-2-approach"},
        {0,26,0,"wood-2-return"},{0,28.2f,6,"repair-bridge-approach"}
    };
    for(const auto& point:points){
        Need(replay.Phase()==point.phase,"Bridge waypoint ordering changed");
        s.feet={point.x,0,point.z};Tick(replay,s);
        Need(!replay.Controls().Down('E'),"Waypoint manufactured interaction");
        if(!point.action)continue;
        Tick(replay,s);Need(Released(replay),"Interaction release did not reach a real update");
        Tick(replay,s);
        Need(replay.Controls().Down('E')&&replay.Controls().Pressed('E'),"Interaction E edge missing");
        auto absent=replay;Tick(absent,s);
        Need(absent.Finished()&&!absent.Passed()&&absent.Failure()=="bridge-route-state-mismatch"&&Released(absent),"Missing formal interaction result was accepted");
        if(point.action<=3)s.collected|=1u<<(point.action-1);
        else if(point.action==4)s.repairMaterials|=2u;
        else if(point.action==5)s.repairMaterials|=4u;
        else s.bridgeRepaired=true;
        Tick(replay,s);Need(Released(replay),"Interaction E was not released after verification");
    }
    Need(replay.Phase()=="cross-bridge"&&!replay.Finished(),"Repair alone cannot pass bridge route");
    auto unbuilt=replay;auto unbuiltSession=s;unbuiltSession.bridgeRepaired=false;Tick(unbuilt,unbuiltSession);
    Need(unbuilt.Finished()&&!unbuilt.Passed(),"Unrepaired bridge cannot pass");
    s.feet={0,0,39};Tick(replay,s);
    Need(!replay.Finished()&&replay.Controls().Down('W'),"Not crossing the river cannot pass");
    auto below=replay;auto belowSession=s;belowSession.feet={0,-4,41};Tick(below,belowSession);
    Need(below.Finished()&&!below.Passed()&&below.Failure()=="bridge-crossing-incomplete","Underwater north-bank observation cannot pass");
    s.feet={0,0,41};Tick(replay,s);
    if(combat){
        Need(!replay.Finished()&&replay.Phase()==(death?"death-approach":"combat-1-approach")&&observation,"Selected route must continue beyond bridge");
        *combat=replay;*observation=s;return;
    }
    Need(replay.Finished()&&replay.Passed()&&Released(replay),"Verified bridge and north-bank arrival did not pass");
    InputReplay defaultRoute;Session fresh;Tick(defaultRoute,fresh);
    Need(!defaultRoute.EnableBridgeRoute()&&!defaultRoute.BridgeRoute(),"Default route switched mid-run");
}
void CombatRouteControls(InputReplay* power=nullptr,Session* observation=nullptr,bool completion=false,SaveGame* checkpoint=nullptr){
    InputReplay replay;Session s;BridgeRouteControls(&replay,&s,power!=nullptr&&!checkpoint,completion,false,checkpoint!=nullptr);
    Need(!replay.EnableCombatRoute()&&replay.CombatRoute(),"Combat route switched after starting");
    Encounter enemy;Evasion evasion;
    const auto tick=[&](InputReplay& r,const Session& state,const Encounter* observed,const Evasion* dash,bool grounded=true){
        const auto prior=state.Serialize();const unsigned hp=observed?observed->health:0;
        const float remaining=dash?dash->remaining:0;
        r.Tick(state,0,1.8f,true,false,Dt,observed,dash,grounded);
        Need(state.Serialize()==prior&&(!observed||observed->health==hp)&&(!dash||dash->remaining==remaining),"Combat driver mutated observed gameplay state");
    };
    auto missing=replay;tick(missing,s,nullptr,&evasion);
    Need(missing.Finished()&&!missing.Passed(),"Missing enemy observation accepted");
    auto damaged=replay;auto damagedSession=s;damagedSession.health=80;tick(damaged,damagedSession,&enemy,&evasion);
    Need(damaged.Finished()&&!damaged.Passed()&&damaged.Failure()=="combat-player-damaged","Tanking damage accepted");
    tick(replay,s,&enemy,&evasion);Need(replay.Controls().Down('W'),"Combat entry direction incorrect");
    s.feet={0,0,44};tick(replay,s,&enemy,&evasion);
    // 合成三輪敵人觀測只驗 driver，不是正式戰鬥或閃避通關證據。
    for(unsigned round=0;round<3;++round){
        enemy.feet={0,0,46};enemy.phase=EnemyPhase::Windup;enemy.phaseTime=Encounter::WindupDuration;
        enemy.countered=false;enemy.playerCooldown=0;enemy.strikeTarget=s.feet;evasion={};
        auto airborne=replay;tick(airborne,s,&enemy,&evasion,false);
        Need(airborne.Finished()&&!airborne.Passed(),"Airborne Q assumed successful");
        if(round==0){
            auto rotated=replay;
            rotated.Tick(s,.45f,1.8f,true,false,Dt,&enemy,&evasion,true);
            Need(rotated.Controls().Pressed('Q')&&rotated.Controls().Down('D')&&
                !rotated.Controls().Down('W')&&!rotated.Controls().Down('A')&&!rotated.Controls().Down('S'),
                "Normalized diagonal dodge camera quantization incorrect");
        }
        tick(replay,s,&enemy,&evasion);
        Need(replay.Controls().Down('Q')&&replay.Controls().Pressed('Q')&&!replay.Controls().Down('F'),"Dodge Q must be a true pulse before counter");
        Need(replay.Controls().Down(round%2?'A':'D'),"Alternating side dodge direction incorrect");
        Need(replay.Controls().Down('W')&&!replay.Controls().Down('S'),"Dodge must approach diagonally rather than retreat from counter range");
        auto noDash=replay;auto recovery=enemy;recovery.phase=EnemyPhase::Recovery;recovery.phaseTime=Encounter::RecoveryDuration;
        auto escaped=s;escaped.feet.x+=2;
        tick(noDash,escaped,&recovery,&evasion);
        Need(noDash.Finished()&&!noDash.Passed()&&noDash.Failure()=="combat-dodge-not-proven","Position alone without actual dash accepted");
        evasion.direction={round%2?-1.f:1.f,0,0};evasion.remaining=.2f;evasion.cooldown=1.1f;
        s.feet.x+=round%2?-2.f:2.f;tick(replay,s,&enemy,&evasion);
        Need(!replay.Controls().Down('Q')&&!replay.Controls().Pressed('Q'),"Q did not release during real dash");
        {
            auto approach=replay;auto stoppedDash=evasion;stoppedDash.remaining=0;
            auto outsideSession=s;outsideSession.feet={round%2?-3.36f:3.36f,0,44};
            tick(approach,outsideSession,&enemy,&stoppedDash);
            Need((approach.Controls().Down('W')||approach.Controls().Down('A')||approach.Controls().Down('D'))&&
                !approach.Controls().Down('F')&&!approach.Controls().Down('Q')&&approach.Counters()==round,
                "Safe early approach must use movement only without claiming a counter");
            auto unsafe=replay;auto unsafeSession=s;unsafeSession.feet={0,0,42};
            tick(unsafe,unsafeSession,&enemy,&stoppedDash);
            Need(Released(unsafe),"Early approach must not cross the locked strike circle");
        }
        auto inside=replay;auto insideSession=s;insideSession.feet=enemy.strikeTarget;
        tick(inside,insideSession,&recovery,&evasion);
        Need(inside.Finished()&&!inside.Passed(),"Dash still inside strike circle accepted");
        enemy=recovery;evasion.remaining=0;tick(replay,s,&enemy,&evasion);
        Need(replay.Dodges()==round+1&&replay.Counters()==round,"Dodge counted without completed windup or counter counted early");
        s.feet={0,0,44};tick(replay,s,&enemy,&evasion);
        Need(!replay.Controls().Down('F'),"Recovery approach manufactured attack");
        if(checkpoint){
            const auto phase=replay.Phase();
            for(unsigned wait=0;wait<3;++wait){
                tick(replay,s,&enemy,&evasion);
                Need(!replay.Finished()&&Released(replay)&&replay.Phase()==phase&&replay.Counters()==0,
                    "Checkpoint counter must wait for observed dodge cooldown without claiming progress");
            }
            evasion.cooldown=0;
        }
        tick(replay,s,&enemy,&evasion);Need(Released(replay),"Counter release update missing");
        tick(replay,s,&enemy,&evasion);
        Need(replay.Controls().Down('F')&&replay.Controls().Pressed('F'),"Counter F edge missing");
        auto ineffective=replay;tick(ineffective,s,&enemy,&evasion);
        Need(ineffective.Finished()&&!ineffective.Passed(),"F without actual health reduction accepted");
        --enemy.health;enemy.countered=true;
        if(checkpoint){enemy.phaseTime=.8166667f;enemy.playerCooldown=.6333333f;evasion.cooldown=0;}
        if(!enemy.health){
            enemy.phase=EnemyPhase::Defeated;
            auto noVictory=replay;tick(noVictory,s,&enemy,&evasion);
            Need(noVictory.Finished()&&!noVictory.Passed(),"Enemy zero health without formal victory accepted");
            s.guardianDefeated=true;
        }
        tick(replay,s,&enemy,&evasion);
        Need(replay.Counters()==round+1&&Released(replay),"Counter confirmation or F release missing");
        if(checkpoint){
            Need(power&&observation&&round==0&&replay.Phase()=="checkpoint-missing-release"&&!replay.Finished(),"Checkpoint did not branch after the first counter");
            *power=replay;*observation=s;*checkpoint={s,enemy,evasion};return;
        }
    }
    Need(replay.Dodges()==3&&replay.Counters()==3&&s.health==100&&s.CurrentStage()==Stage::Fuse,"Three proven no-damage counters not observed");
    if(power){
        Need(!replay.Finished()&&replay.Phase()=="power-road-48"&&observation,"Power route must continue after actual combat victory");
        *power=replay;*observation=s;return;
    }
    Need(replay.Passed(),"Default combat route no longer finishes after victory");
}
void PowerRouteControls(InputReplay* completion=nullptr,Session* observation=nullptr){
    InputReplay replay;Session s;CombatRouteControls(&replay,&s,completion!=nullptr);
    Need(replay.PowerRoute()&&replay.CombatRoute()&&replay.BridgeRoute()&&!replay.EnablePowerRoute(),"Power mode must include earlier routes and forbid mid-run switches");
    auto damaged=replay;auto damagedSession=s;damagedSession.health=99;Tick(damaged,damagedSession);
    Need(damaged.Finished()&&!damaged.Passed(),"Power route accepted health loss");
    auto early=replay;auto earlySession=s;
    earlySession.fuseClueRead=true;earlySession.fuseDials=Session::solvedFuseDials;
    earlySession.fuseCollected=true;earlySession.circuit=7;earlySession.powerRestored=true;Tick(early,earlySession);
    Need(early.Finished()&&!early.Passed(),"Early power bypass accepted");
    struct Point {float x,z;unsigned action;const char* phase;};
    constexpr Point points[]{
        {0,48,0,"power-road-48"},{0,79,0,"power-road-79"},{7.75f,79,1,"power-log-approach"},
        {0,79,0,"power-hut-exit"},{0,59,0,"power-road-59"},{-6.6f,59,2,"power-cabinet-approach"},
        {-6.6f,59,3,"power-fuse-approach"},{0,59,0,"power-workshop-exit"},{0,65,0,"power-road-65"},
        {5.4f,65,4,"power-switch-1-approach"},{5.4f,65,5,"power-switch-3-approach"},{5.4f,65,6,"power-generator-approach"}
    };
    // 僅合成觀測控制資料；不呼叫 Session 的讀線索、撥輪、取物或供電 API。
    const auto interact=[&](unsigned action,unsigned wheel){
        Tick(replay,s);Need(Released(replay),"Power action needs a true release update");
        Tick(replay,s);
        const unsigned key=action==2?'1'+wheel:action==4?'1':action==5?'3':'E';
        Need(replay.Controls().Down(key)&&replay.Controls().Pressed(key),"Power action press edge missing");
        auto absent=replay;Tick(absent,s);
        Need(absent.Finished()&&!absent.Passed(),"Missing E/key result accepted");
        if(action==1)s.fuseClueRead=true;
        else if(action==2){
            const unsigned shift=wheel*2;
            s.fuseDials=(s.fuseDials&~(3u<<shift))|((((s.fuseDials>>shift)+1u)&3u)<<shift);
            auto wrong=replay;auto wrongSession=s;wrongSession.fuseDials^=1u;Tick(wrong,wrongSession);
            Need(wrong.Finished()&&!wrong.Passed(),"Wrong dial observation accepted");
        }else if(action==3)s.fuseCollected=true;
        else if(action==4||action==5){
            s.circuit^=action==4?3u:4u;
            auto wrong=replay;auto wrongSession=s;wrongSession.circuit^=1u;Tick(wrong,wrongSession);
            Need(wrong.Finished()&&!wrong.Passed(),"Wrong circuit observation accepted");
        }else s.powerRestored=true;
        Tick(replay,s);Need(Released(replay),"Power action failed to release after result verification");
    };
    for(const auto& point:points){
        Need(replay.Phase()==point.phase,"Power waypoint ordering changed");
        if(point.action==2){
            // 原生失敗在距離櫃子約 0.1m 持續以 0.6m/s 繞圈；不能靠增加到達容差掩蓋。
            for(const Aevrynd::Vec3 velocity:std::array<Aevrynd::Vec3,3>{{{.6f,0,0},{-.6f,0,0},{0,0,-.6f}}}){
                auto orbit=replay;auto nearby=s;nearby.feet={point.x,0,point.z-.11f};
                orbit.Tick(nearby,0,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{},velocity);
                Need(Released(orbit)&&orbit.Phase()==point.phase,"Near tangential or retreating motion must brake without advancing");
                orbit.Tick(nearby,0,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{}, {0,0,.1f});
                Need(Released(orbit)&&orbit.Phase()==point.phase,"Brake must finish even if residual velocity points toward target");
                Tick(orbit,nearby);
                Need(orbit.Controls().Down('W')&&!orbit.Controls().Down(VK_SHIFT)&&orbit.Phase()==point.phase,
                    "Zero-speed outside original tolerance must steer again, not pass or remain stuck braking");
            }
            auto closing=replay;auto nearby=s;nearby.feet={point.x,0,point.z-.11f};
            closing.Tick(nearby,0,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{}, {0,0,.6f});
            Need(closing.Controls().Down('W')&&closing.Phase()==point.phase,"Aligned low-speed approach must not brake unnecessarily");
        }
        s.feet={point.x,0,point.z};
        // 合成在點但仍有速度的觀測；不得跳過真煞停或偷改觀測來開始互動。
        for(float speed:{.001f,.3f,1.8f})for(const Aevrynd::Vec3 axis:std::array<Aevrynd::Vec3,3>{{{1,0,0},{-1,0,0},{0,0,1}}}){
            auto moving=replay;const auto saved=s.Serialize();
            for(unsigned frame=0;frame<4;++frame){
                moving.Tick(s,0,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{},axis*speed);
                Need(moving.Phase()==point.phase&&Released(moving)&&s.Serialize()==saved,
                    "Power must stop before advancing waypoint or interacting");
            }
            Tick(moving,s);
            Need(moving.Phase()!=point.phase&&!moving.Finished(),"Power resumes only once actually stopped");
        }
        Tick(replay,s);Need(Released(replay),"Power waypoint manufactured a key");
        if(!point.action)continue;
        if(point.action==2){
            auto noClue=replay;auto noClueSession=s;noClueSession.fuseClueRead=false;Tick(noClue,noClueSession);
            Need(noClue.Finished()&&!noClue.Passed(),"Dial entry without read clue accepted");
            for(unsigned wheel=0;wheel<Session::fuseCode.size();++wheel){
                for(unsigned turn=0;turn<Session::fuseCode[wheel];++turn){
                    const auto label="power-dial-"+std::to_string(wheel+1)+"-turn-"+std::to_string(turn+1)+"-release";
                    Need(replay.Phase()==label,"Each dial press must have a unique evidence phase");
                    interact(2,wheel);
                }
            }
            Need(s.fuseDials==Session::solvedFuseDials,"Dial sequence did not follow clue code");
        }else interact(point.action,0);
    }
    Need(s.powerRestored&&s.CurrentStage()==Stage::Relays&&s.health==100,"Verified power route did not reach relays");
    if(completion){
        Need(!replay.Finished()&&replay.Phase()=="completion-road-65"&&observation,"Completion mode ended at generator");
        *completion=replay;*observation=s;return;
    }
    Need(replay.Passed(),"Default power mode did not finish");
    InputReplay defaultMode;Session fresh;Tick(defaultMode,fresh);
    Need(!defaultMode.EnablePowerRoute()&&!defaultMode.PowerRoute(),"Default route switched to power after beginning");
}
void CompletionRouteControls(){
    InputReplay replay;Session s;PowerRouteControls(&replay,&s);
    Need(replay.CompletionRoute()&&!replay.EnableCompletionRoute(),"Completion mode switched after starting");
    struct Point {float x,z;unsigned action;const char* phase;};
    constexpr Point points[]{
        {0,65,0,"completion-road-65"},{0,62,0,"completion-road-62"},{-20.6f,62,1,"completion-west-station"},
        {0,62,0,"completion-west-return"},{0,86,0,"completion-road-86"},{20.6f,86,2,"completion-east-station"},
        {0,86,0,"completion-east-return"},{0,94,0,"completion-road-94"},{-18.6f,94,3,"completion-northwest-station"},
        {0,94,0,"completion-northwest-return"},{0,68.2f,4,"completion-beacon"}
    };
    // 只合成正式世界應回報的觀測，不呼叫接線、調向、連站或點亮信標 API。
    const auto press=[&](unsigned key,auto observation){
        Tick(replay,s);Need(Released(replay),"Completion action release update missing");
        Tick(replay,s);Need(replay.Controls().Down(key)&&replay.Controls().Pressed(key),"Completion action edge missing");
        auto absent=replay;Tick(absent,s);
        Need(absent.Finished()&&!absent.Passed(),"Missing completion action result accepted");
        observation();Tick(replay,s);Need(Released(replay),"Completion action not released after confirmation");
    };
    for(const auto& point:points){
        Need(replay.Phase()==point.phase,"Completion waypoint ordering changed");
        s.feet={point.x,GroundElevation(point.x,point.z),point.z};
        // 進入半徑不代表已煞停；互動前只能放鍵等候，不改合成觀測。
        for(float speed:{.001f,.3f,1.8f}){
            auto moving=replay;const auto saved=s.Serialize();
            for(unsigned frame=0;frame<4;++frame){
                moving.Tick(s,0,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{}, {speed,0,0});
                Need(moving.Phase()==point.phase&&Released(moving)&&s.Serialize()==saved,"Completion must stop before advancing waypoint or interacting");
            }
            Tick(moving,s);Need(moving.Phase()!=point.phase&&!moving.Finished(),"Completion resumes once actually stopped");
        }
        if(point.action>=1&&point.action<=3){
            auto floating=replay;auto floatingSession=s;floatingSession.feet.y+=.2f;Tick(floating,floatingSession);
            Need(floating.Finished()&&!floating.Passed(),"Station accepted non-grounded observation");
        }
        Tick(replay,s);Need(Released(replay),"Completion waypoint manufactured input");
        if(!point.action)continue;
        auto distant=replay;auto distantSession=s;distantSession.feet.x=0;Tick(distant,distantSession);
        if(point.action!=4)Need(distant.Finished()&&!distant.Passed(),"Station interaction accepted far observation");
        if(point.action==4){
            auto missingField=replay;auto missingFieldSession=s;missingFieldSession.relayFieldTasks=0;Tick(missingField,missingFieldSession);
            Need(missingField.Finished()&&!missingField.Passed(),"Beacon accepted incomplete field tasks");
            press('E',[&]{s.beaconLit=true;});continue;
        }
        const unsigned station=point.action-1;
        auto early=replay;auto earlySession=s;earlySession.relayLinked|=1u<<station;Tick(early,earlySession);
        Need(early.Finished()&&!early.Passed(),"Early relay link accepted");
        if(station==0){
            press('1',[&]{s.relayPatchA=1;});
            press('E',[&]{s.relayFieldTasks|=FieldRelayTask::WestCircuitBit;});
        }else if(station==2){
            press('3',[&]{s.extractionRoute=1;});
            press('E',[&]{s.relayFieldTasks|=FieldRelayTask::NorthWestRouteBit;});
        }
        unsigned turns{};
        while(s.RelaySignal(station)!=100){
            Need(turns++<16,"Visible signal did not lock in a full rotation");
            Tick(replay,s);
            press('2',[&]{
                const unsigned shift=station*4;
                const unsigned next=(((s.relaySectors>>shift)&15u)+1u)&15u;
                auto wrong=replay;auto wrongSession=s;
                wrongSession.relaySectors=(s.relaySectors&~(15u<<shift))|(((next+1u)&15u)<<shift);
                Tick(wrong,wrongSession);Need(wrong.Finished()&&!wrong.Passed(),"Wrong sector increment accepted");
                s.relaySectors=(s.relaySectors&~(15u<<shift))|(next<<shift);
            });
        }
        Tick(replay,s);
        press('E',[&]{s.relayLinked|=1u<<station;});
    }
    Need(replay.Passed()&&s.beaconLit&&s.CurrentStage()==Stage::Complete&&s.relayLinked==7&&
        s.relayFieldTasks==FieldRelayTask::AllTasks&&s.health==100,"Verified three relays and beacon did not finish");
    // 正確扇區遞增必然在十六步內遇到 100；另對相同有限搜尋門檻注入不鎖定訊號作負控。
    for(unsigned turns=0;turns<16;++turns)Need(CompletionScanMayTurn(turns,50),"Signal scan stopped before bounded search completed");
    Need(!CompletionScanMayTurn(16,50)&&!CompletionScanMayTurn(17,50)&&!CompletionScanMayTurn(0,100),"Nonlocking signal allowed an unbounded cycle");
    InputReplay original;Session fresh;Tick(original,fresh);
    Need(!original.EnableCompletionRoute()&&!original.CompletionRoute(),"Default route changed to completion mid-run");
}
void CheckpointRouteControls(){
    InputReplay mode;
    Need(mode.EnableCheckpointRoute()&&mode.CheckpointRoute()&&mode.CombatRoute()&&mode.BridgeRoute()&&
        !mode.PowerRoute()&&!mode.CompletionRoute()&&!mode.DeathRetryRoute(),"Checkpoint mode dependencies incorrect");
    Need(!mode.EnablePowerRoute()&&!mode.EnableCompletionRoute()&&!mode.EnableDeathRetryRoute(),"Checkpoint mixed incompatible routes");
    InputReplay power,completion,death;
    Need(power.EnablePowerRoute()&&!power.EnableCheckpointRoute()&&completion.EnableCompletionRoute()&&!completion.EnableCheckpointRoute()&&
        death.EnableDeathRetryRoute()&&!death.EnableCheckpointRoute(),"Checkpoint replaced an incompatible route");
    InputReplay replay;Session s;SaveGame live;CombatRouteControls(&replay,&s,false,&live);
    Need(live.Valid()&&!replay.EnableCheckpointRoute()&&replay.Counters()==1&&replay.Dodges()==1,"Checkpoint branch observation invalid");
    {
        auto v3=live;SavedPlayerMotion motion;motion.controller.feet=live.session.feet;
        motion.controller.grounded=true;v3.motion=motion;
        Need(v3.Valid(),"v3 grounded observation fixture");
        auto copy=replay;
        copy.Tick(live.session,0,1.8f,true,false,Dt,&live.encounter,&live.evasion,true,nullptr,{}, {},&v3);
        Need(!copy.Finished(),"v3 matching observation rejected before checkpoint");
        auto mismatch=v3;mismatch.encounter.phaseTime-=.01f;copy=replay;
        copy.Tick(live.session,0,1.8f,true,false,Dt,&live.encounter,&live.evasion,true,nullptr,{}, {},&mismatch);
        Need(copy.Finished()&&copy.Failure()=="checkpoint-motion-observation-mismatch","v3 mismatched envelope accepted");
        mismatch=v3;mismatch.motion->controller.grounded=false;copy=replay;
        copy.Tick(live.session,0,1.8f,true,false,Dt,&live.encounter,&live.evasion,true,nullptr,{}, {},&mismatch);
        Need(copy.Finished()&&!copy.Passed(),"v3 contradictory ground accepted");
    }
    // 本段僅合成觀測與磁碟讀取結果測試驅動器；不寫檔、不呼叫正式保存、讀檔或遊戲狀態修改 API。
    const auto tick=[](InputReplay& r,const SaveGame& state,const SaveGame* disk=nullptr,std::string_view feedback={},float dt=Dt){
        const auto before=state.Serialize();const auto diskBefore=disk?disk->Serialize():std::string{};
        r.Tick(state.session,0,1.8f,true,false,dt,&state.encounter,&state.evasion,true,disk,feedback);
        Need(before==state.Serialize()&&(!disk||diskBefore==disk->Serialize()),"Checkpoint driver modified readonly live or disk observation");
        Need(!r.Controls().Down('F')&&!r.Controls().Down('R')&&!r.Controls().Down('E')&&
            !r.Controls().Down('W')&&!r.Controls().Down('A')&&!r.Controls().Down('S')&&!r.Controls().Down('D'),"Checkpoint used extra movement or gameplay input");
    };
    const auto rejected=[&](InputReplay candidate,const SaveGame& state,const SaveGame* disk,std::string_view feedback){
        tick(candidate,state,disk,feedback);Need(candidate.Finished()&&!candidate.Passed()&&Released(candidate),"Invalid checkpoint observation accepted");
    };
    const auto advance=[](SaveGame& state,float dt){
        state.encounter.phaseTime-=dt;state.encounter.playerCooldown=std::max(0.f,state.encounter.playerCooldown-dt);
        if(state.evasion.remaining>0)state.session.feet=state.session.feet+state.evasion.direction*(8.f*std::min(dt,state.evasion.remaining));
        state.evasion.remaining=std::max(0.f,state.evasion.remaining-dt);state.evasion.cooldown=std::max(0.f,state.evasion.cooldown-dt);
    };
    rejected(replay,live,&live,{});
    tick(replay,live);Need(Released(replay)&&replay.Phase()=="checkpoint-missing-press","Missing-load release update absent");
    tick(replay,live);Need(replay.Controls().Pressed(VK_F9)&&replay.Phase()=="checkpoint-missing-verify","Missing-load F9 edge absent");
    auto mutated=live;mutated.encounter.phaseTime-=Dt;rejected(replay,mutated,nullptr,"Save unavailable");
    rejected(replay,live,nullptr,"Loaded");
    tick(replay,live,nullptr,"Save unavailable");Need(Released(replay),"Missing-load verification failed to release F9");
    tick(replay,live);Need(Released(replay)&&replay.Phase()=="checkpoint-save-press","Save release update absent");
    auto cooling=live;cooling.evasion.cooldown=.1f;rejected(replay,cooling,nullptr,{});
    tick(replay,live);
    Need(replay.Controls().Pressed('Q')&&replay.Controls().Pressed(VK_F5)&&!replay.Controls().Down(VK_F9)&&
        replay.Phase()=="checkpoint-save-verify","Same-update Q plus F5 edge absent");
    live.evasion.direction={0,0,-1};live.evasion.remaining=Evasion::Duration-Dt;live.evasion.cooldown=Evasion::Cooldown;
    live.session.feet.z-=8.f*Dt;live.encounter.phaseTime-=Dt;live.encounter.playerCooldown-=Dt;
    const SaveGame disk=live;Need(disk.Valid(),"Synthetic saved dash invalid");
    rejected(replay,live,nullptr,{});
    auto incomplete=disk;incomplete.evasion.remaining=Evasion::Duration+1;rejected(replay,live,&incomplete,{});
    auto wrongDisk=disk;wrongDisk.encounter.phaseTime-=Dt;rejected(replay,live,&wrongDisk,{});
    auto wrongDirection=disk;wrongDirection.evasion.direction={1,0,0};rejected(replay,wrongDirection,&wrongDirection,{});
    tick(replay,live,&disk,"Saved");Need(replay.SavedCheckpoint()==1&&Released(replay),"Saved readback was not verified or keys stayed held");
    rejected(replay,live,&disk,{});
    auto lost=live;lost.session.repairMaterials=3;rejected(replay,lost,&disk,{});
    auto wrongLive=live;advance(wrongLive,Dt);wrongLive.evasion.direction={1,0,0};rejected(replay,wrongLive,&disk,{});
    auto bounded=replay;auto slow=live;
    for(unsigned i=0;i<605&&!bounded.Finished();++i){advance(slow,.00001f);tick(bounded,slow,&disk,{},.00001f);}
    Need(bounded.Finished()&&!bounded.Passed()&&bounded.Failure()=="phase-stalled-600-ticks","Checkpoint slow waiting exceeded finite deadline");
    unsigned divergenceTicks{};
    while(replay.Phase()=="checkpoint-live-advance"&&divergenceTicks<30){
        advance(live,Dt);tick(replay,live,&disk);++divergenceTicks;Need(!replay.Finished()&&Released(replay),"Live advance failed or emitted keys");
    }
    Need(divergenceTicks>=21&&divergenceTicks<=22&&live.evasion.remaining==0&&replay.Phase()=="checkpoint-load-release", "Checkpoint did not wait for real divergence");
    rejected(replay,live,&wrongDisk,{});
    advance(live,Dt);tick(replay,live,&disk);Need(Released(replay)&&replay.Phase()=="checkpoint-load-press","Load release update absent");
    advance(live,Dt);tick(replay,live,&disk);
    Need(replay.Controls().Pressed(VK_F9)&&!replay.Controls().Down('Q')&&!replay.Controls().Down(VK_F5),"Restore did not use a separate F9 edge");
    rejected(replay,live,&disk,"Loaded");
    rejected(replay,disk,&disk,"Save unavailable");
    auto wrongRestore=disk;wrongRestore.evasion.direction={1,0,0};rejected(replay,wrongRestore,&disk,"Loaded");
    wrongRestore=disk;wrongRestore.encounter.playerCooldown-=Dt;rejected(replay,wrongRestore,&disk,"Loaded");
    live=disk;tick(replay,live,&disk,"Loaded");
    Need(replay.LoadedCheckpoint()==1&&!replay.Finished()&&Released(replay),"Exact restore alone incorrectly passed or failed to release input");
    rejected(replay,live,&disk,{});
    auto stationary=live;advance(stationary,Dt);stationary.session.feet=live.session.feet;rejected(replay,stationary,&disk,{});
    auto back=live;advance(back,Dt);back.session.feet.z=live.session.feet.z+.1f;rejected(replay,back,&disk,{});
    unsigned continuationTicks{};
    while(!replay.Finished()&&continuationTicks<10){advance(live,Dt);tick(replay,live,&disk);++continuationTicks;Need(Released(replay),"Restored continuation manufactured another key");}
    Need(replay.Passed()&&continuationTicks>=6&&continuationTicks<=7&&replay.SavedCheckpoint()==1&&replay.LoadedCheckpoint()==1&&
        live.session.health==100&&live.encounter.health==2&&live.evasion.remaining>0&&disk.Serialize()!=live.Serialize(),"Restored natural dash continuation did not pass");
}
void DeathRetryRouteControls(){
    InputReplay mode;
    Need(mode.EnableDeathRetryRoute()&&mode.BridgeRoute()&&!mode.CombatRoute()&&!mode.PowerRoute()&&!mode.CompletionRoute(),"Death route must be independent of victory routes");
    Need(!mode.EnableCombatRoute()&&!mode.EnablePowerRoute()&&!mode.EnableCompletionRoute(),"Victory mode was mixed with expected death");
    InputReplay victory;Need(victory.EnableCompletionRoute()&&!victory.EnableDeathRetryRoute(),"Death mode replaced an enabled victory route");
    InputReplay replay;Session s;BridgeRouteControls(&replay,&s,false,false,true);
    Need(!replay.EnableDeathRetryRoute()&&replay.DamageEvents()==0&&replay.Retries()==0,"Death mode changed after starting");
    Encounter enemy;Evasion evasion;
    const auto same=[](Aevrynd::Vec3 a,Aevrynd::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
    const auto tick=[&](InputReplay& r,const Session& state,const Encounter& observed,const Evasion& dash,bool grounded=true){
        const auto prior=state.Serialize();const auto oldEnemy=observed;const auto oldDash=dash;
        r.Tick(state,0,1.8f,true,false,Dt,&observed,&dash,grounded);
        Need(state.Serialize()==prior&&same(observed.feet,oldEnemy.feet)&&same(observed.strikeTarget,oldEnemy.strikeTarget)&&
            observed.health==oldEnemy.health&&observed.phase==oldEnemy.phase&&observed.phaseTime==oldEnemy.phaseTime&&
            observed.playerCooldown==oldEnemy.playerCooldown&&observed.countered==oldEnemy.countered&&
            same(dash.direction,oldDash.direction)&&dash.remaining==oldDash.remaining&&dash.cooldown==oldDash.cooldown,
            "Death driver mutated read-only observations");
        Need(!r.Controls().Down('F')&&!r.Controls().Down('Q')&&!r.Controls().Down(VK_F5)&&!r.Controls().Down(VK_F9),"Death route used combat or save bypass input");
    };
    auto earlyDamage=replay;auto hurt=s;hurt.health=80;tick(earlyDamage,hurt,enemy,evasion);
    Need(earlyDamage.Finished()&&!earlyDamage.Passed(),"Damage before waiting at combat point accepted");
    tick(replay,s,enemy,evasion);Need(replay.Controls().Down('W'),"Death approach did not use movement");
    s.feet={0,0,44};tick(replay,s,enemy,evasion);
    Need(replay.Phase()=="death-hit-1-wait"&&Released(replay),"Death approach failed to release movement");
    auto skippedWindup=replay;auto skippedState=s;skippedState.health=80;
    auto skippedEnemy=enemy;skippedEnemy.phase=EnemyPhase::Recovery;skippedEnemy.phaseTime=Encounter::RecoveryDuration;skippedEnemy.strikeTarget=s.feet;
    tick(skippedWindup,skippedState,skippedEnemy,evasion);
    Need(skippedWindup.Finished()&&!skippedWindup.Passed(),"Damage without prior observed windup accepted");
    // 合成五次蓄力與傷害只驗證驅動的正負控，不呼叫敵人攻擊或玩家 Damage，更不是原生死亡證據。
    for(unsigned hit=1;hit<=5;++hit){
        enemy.feet={0,0,46};enemy.phase=EnemyPhase::Windup;enemy.phaseTime=Encounter::WindupDuration;enemy.strikeTarget=s.feet;
        tick(replay,s,enemy,evasion);Need(Released(replay),"Waiting for damage must not emit any key");
        // 合成等待累積超過 600 更新，仍只有真實傷害觀測可讓該等待段取得新進展。
        for(unsigned waiting=0;waiting<130;++waiting){tick(replay,s,enemy,evasion);Need(!replay.Finished()&&Released(replay),"Finite damage waiting failed or emitted input");}
        auto wrongAmount=replay;auto wrongHealth=s;wrongHealth.health-=10;
        auto recovery=enemy;recovery.phase=EnemyPhase::Recovery;recovery.phaseTime=Encounter::RecoveryDuration;
        tick(wrongAmount,wrongHealth,recovery,evasion);
        Need(wrongAmount.Finished()&&!wrongAmount.Passed(),"Unexpected damage amount accepted");
        if(hit<5){auto prematureDeath=replay;auto zero=s;zero.health=0;tick(prematureDeath,zero,recovery,evasion);
            Need(prematureDeath.Finished()&&!prematureDeath.Passed(),"Death before five separate strikes accepted");}
        if(hit>1){auto healed=replay;auto healedState=s;healedState.health=100;tick(healed,healedState,enemy,evasion);
            Need(healed.Finished()&&!healed.Passed(),"Early healing accepted");}
        auto noRecovery=replay;auto damagedState=s;damagedState.health-=20;tick(noRecovery,damagedState,enemy,evasion);
        Need(noRecovery.Finished()&&!noRecovery.Passed(),"Health loss without Windup to Recovery accepted");
        s=damagedState;enemy=recovery;tick(replay,s,enemy,evasion);
        Need(replay.DamageEvents()==hit&&replay.Phase()=="death-hit-"+std::to_string(hit)+"-observed"&&Released(replay),"Damage observation requires a unique all-released phase");
        tick(replay,s,enemy,evasion);
        Need(Released(replay)&&!replay.Finished(),"Damage observation did not preserve a real released update");
    }
    Need(s.health==0&&replay.Phase()=="death-release"&&replay.Retries()==0,"Fifth actual damage did not reach death release");
    auto premature=replay;auto prematureState=s;prematureState.health=100;tick(premature,prematureState,enemy,evasion);
    Need(premature.Finished()&&!premature.Passed(),"Healing before R accepted");
    unsigned deathWaitTicks{};bool sawMiddle=false,sawSettled=false;
    while(replay.Phase()!="death-retry-press"&&deathWaitTicks<100){
        tick(replay,s,enemy,evasion);++deathWaitTicks;
        sawMiddle|=replay.Phase()=="death-collapse-middle";
        sawSettled|=replay.Phase()=="death-collapse-settled";
        Need(!replay.Finished()&&Released(replay),"Collapse wait emitted keys or failed");
    }
    Need(deathWaitTicks>=89&&deathWaitTicks<=91&&sawMiddle&&sawSettled&&replay.Phase()=="death-retry-press", "Death animation did not receive 1.5 seconds of real updates");
    tick(replay,s,enemy,evasion);
    Need(replay.Phase()=="death-retry-verify"&&replay.Controls().Down('R')&&replay.Controls().Pressed('R'),"Retry must emit exactly one R edge");
    auto noEffect=replay;tick(noEffect,s,enemy,evasion);
    Need(noEffect.Finished()&&!noEffect.Passed()&&Released(noEffect),"Missing R result accepted");
    // 以下合成正式 R 的預期觀測；不直接使用 Session::Retry 或正式控制器方法。
    s.health=100;s.feet=Session::bridge;Encounter reset;Evasion resetDash;
    auto lost=replay;auto lostState=s;lostState.bridgeRepaired=false;tick(lost,lostState,reset,resetDash,false);
    Need(lost.Finished()&&!lost.Passed(),"Retry lost bridge task without failing");
    for(unsigned lostBit=0;lostBit<2;++lostBit){
        auto missing=replay;auto missingState=s;
        if(lostBit==0)missingState.collected=3;else missingState.repairMaterials=3;
        tick(missing,missingState,reset,resetDash,false);Need(missing.Finished()&&!missing.Passed(),"Retry lost supplies or tools without failing");
    }
    auto staleEnemy=replay;tick(staleEnemy,s,enemy,resetDash,false);
    Need(staleEnemy.Finished()&&!staleEnemy.Passed(),"Retry accepted old enemy state");
    auto staleTarget=replay;auto staleTargetEnemy=reset;staleTargetEnemy.strikeTarget={0,0,44};tick(staleTarget,s,staleTargetEnemy,resetDash,false);
    Need(staleTarget.Finished()&&!staleTarget.Passed(),"Retry accepted old strike target");
    auto staleDash=replay;auto dash=resetDash;dash.direction={1,0,0};tick(staleDash,s,reset,dash,false);
    Need(staleDash.Finished()&&!staleDash.Passed(),"Retry accepted retained evasion direction");
    auto moved=replay;auto movedState=s;movedState.feet.z=41;tick(moved,movedState,reset,resetDash,false);
    Need(moved.Finished()&&!moved.Passed(),"Retry accepted wrong spawn position");
    tick(replay,s,reset,resetDash,false);
    Need(replay.Retries()==1&&replay.Phase()=="death-ground-verify"&&Released(replay),"R reset must allow transient ungrounded state and release R");
    auto floating=replay;tick(floating,s,reset,resetDash,false);
    Need(floating.Finished()&&!floating.Passed(),"Next normal update failed to require grounding");
    tick(replay,s,reset,resetDash);
    Need(replay.Phase()=="death-recross"&&!replay.Finished()&&Released(replay),"Retry alone incorrectly passed");
    auto stalled=replay;
    for(unsigned i=0;i<605&&!stalled.Finished();++i)tick(stalled,s,reset,resetDash);
    Need(stalled.Finished()&&!stalled.Passed()&&stalled.Failure()=="phase-stalled-600-ticks","Immobile retry route did not expire");
    tick(replay,s,reset,resetDash);Need(replay.Controls().Down('W')&&replay.Controls().Pressed('W'),"Retry did not resume real movement");
    s.feet.z=36;tick(replay,s,reset,resetDash);
    Need(!replay.Finished()&&replay.Controls().Down('W')&&!replay.Controls().Pressed('W'),"Bridge deck was confused with riverbed or held movement edge duplicated");
    s.feet.z=41;auto submerged=replay;auto submergedState=s;submergedState.feet.y=-4;tick(submerged,submergedState,reset,resetDash);
    Need(submerged.Finished()&&!submerged.Passed(),"Submerged north-bank retry passed");
    tick(replay,s,reset,resetDash);
    Need(replay.Passed()&&replay.DamageEvents()==5&&replay.Retries()==1&&Released(replay),"Five damages plus formal retry and recross did not finish");
}
}
int main()try {
    // 合成觀測驗證按鍵政策邊界；真控制器近點路線另行驗證，不將此當物理證據。
    for(float yaw:{0.f,1.57079632679f,-1.57079632679f})for(float side:{-1.f,1.f}){
        InputReplay base;Session s;base.Tick(s,0,1.8f,true,false,Dt);
        s.feet={0,0,18.32f};
        for(float closing:{-.000001f,0.f,.000001f}){
            auto boundary=base;
            boundary.Tick(s,yaw,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{}, {side*.15f,0,-closing});
            Need(Released(boundary)==(closing<=0)&&boundary.Phase()=="walk-road",
                "Non-closing boundary must brake; actual closing may continue within turn reserve");
        }
        for(float speed:{.15f,.3f,.6f}){
            auto replay=base;
            replay.Tick(s,yaw,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{}, {side*speed*.98f,0,-speed*.2f});
            Need(!Released(replay)&&!replay.Controls().Down(VK_SHIFT)&&replay.Phase()=="walk-road",
                "Low-speed turn with enough distance must retain direction input");
            const float x=float(replay.Controls().Down('D'))-float(replay.Controls().Down('A'));
            const float z=float(replay.Controls().Down('W'))-float(replay.Controls().Down('S'));
            Need(std::abs(x*std::cos(yaw)-z*std::sin(yaw))<.00001f&&x*std::sin(yaw)+z*std::cos(yaw)<-.99f,
                "Near turn must still steer toward target under camera rotation");
        }
        auto retreat=base;
        retreat.Tick(s,yaw,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{}, {0,0,.6f});
        Need(Released(retreat)&&retreat.Phase()=="walk-road","Larger reversal needs real braking within turn reserve");
        retreat.Tick(s,yaw,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{}, {0,0,-.1f});
        Need(Released(retreat),"Turn brake remains latched until fully stopped");
        retreat.Tick(s,yaw,1.8f,true,false,Dt);
        Need(!Released(retreat)&&retreat.Phase()=="walk-road","Stopped outside tolerance must resume steering");
    }
    {
        InputReplay replay;Session s;
        replay.Tick(s,0,1.8f,true,false,Dt);
        s.feet={0,0,17};
        replay.Tick(s,0,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{}, {0,0,6});
        Need(replay.Phase()=="walk-road"&&Released(replay),"Momentum requires early release without advancing waypoint");
        s.feet.z=17.96f;
        replay.Tick(s,0,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{}, {0,0,1});
        Need(replay.Phase()=="walk-road"&&Released(replay),"Within tolerance but still moving must not advance");
        replay.Tick(s,0,1.8f,true,false,Dt);
        Need(replay.Phase()=="approach-shelter"&&Released(replay),"Stationary within original tolerance may advance");
        InputReplay bad;Session initial;
        bad.Tick(initial,0,1.8f,true,false,Dt,nullptr,nullptr,true,nullptr,{}, {std::numeric_limits<float>::quiet_NaN(),0,0});
        Need(bad.Finished()&&!bad.Passed()&&Released(bad),"Nonfinite controller velocity must fail");
    }
    {
        InputReplay grove;Session s;const auto before=s.Serialize();
        Need(grove.EnableGroveRoute()&&grove.GroveRoute()&&!grove.EnableGroveRoute(),"Grove route opt-in exclusivity");
        Need(!grove.EnableBridgeRoute()&&!grove.EnableCombatRoute()&&!grove.EnableCheckpointRoute()&&
            !grove.EnableCompletionRoute()&&!grove.EnablePowerRoute()&&!grove.EnableDeathRetryRoute()&&
            !grove.EnableResumeSaveRoute(true)&&!grove.EnableWestCliffRoute(-44)&&!grove.EnableDashJumpRoute()&&
            !grove.EnableMotionCheckpointRoute()&&!grove.EnablePivotCheckpointRoute(),"Grove route mixed incompatible controls");
        Need(InputReplay::GrovePointCount()==14&&InputReplay::GrovePoint(4).x==-14.5f&&
            InputReplay::GrovePoint(4).z==50,"Actual near-tree route waypoint contract");
        Need(InputReplay::GrovePoint(9).x==-18&&InputReplay::GrovePoint(9).z==43.5f,
            "Grove south exit must avoid the actual blocked tree envelope");
        bool rejected=false;try{(void)InputReplay::GrovePoint(InputReplay::GrovePointCount());}catch(const std::invalid_argument&){rejected=true;}
        Need(rejected,"Grove route accepted unknown waypoint");
        Tick(grove,s);Need(grove.Phase()=="walk-road"&&grove.BridgeRoute()&&grove.CombatRoute(),"Grove route bypassed normal bridge/combat prerequisites");
        Tick(grove,s);Need(!grove.Finished()&&grove.Phase()=="walk-road"&&grove.Controls().Down('W')&&
            before==s.Serialize(),"Grove route skipped normal production-control prerequisite flow");
        InputReplay old;Need(old.EnableCheckpointRoute()&&!old.EnableGroveRoute(),"Grove route replaced existing checkpoint route");
    }
    SuccessAndRequiredEvidence();DeadlineAndInputSafety();BridgeRouteControls();CombatRouteControls();PowerRouteControls();CompletionRouteControls();DeathRetryRouteControls();CheckpointRouteControls();
    std::cout<<"INPUT_REPLAY_DRIVER_UNIT_PASS synthetic_observations_only=1 native_play_evidence=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"INPUT_REPLAY_DRIVER_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
