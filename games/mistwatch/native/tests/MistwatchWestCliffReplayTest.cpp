#include "MistwatchInputReplay.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Mistwatch;
namespace {
unsigned checks{};
constexpr float Dt=1.f/60.f;
void Need(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
bool Released(InputReplay& r){for(unsigned key=0;key<256;++key)if(r.Controls().Down(key))return false;return true;}
// 僅驗證唯讀回放狀態機；合成的座標、敵人與任務觀測不構成正式遊戲通行或碰撞證據。
struct Fixture {
    InputReplay replay;Session session;Encounter enemy;Evasion evasion;
    explicit Fixture(float x,bool standing=false){Need(replay.EnableWestCliffRoute(x,standing),"enable cliff route");}
    void Tick(float height=1.8f,bool started=true,float yaw=0,float dt=Dt,Aevrynd::Vec3 velocity={},bool feetSettled=true,
        bool grounded=true,const SaveGame* saved=nullptr,const WestCliffReplayWorld* world=nullptr){
        const auto before=session.Serialize();const auto oldEnemy=enemy.health;const auto oldDash=evasion.remaining;
        replay.Tick(session,yaw,height,started,false,dt,&enemy,&evasion,grounded,nullptr,{},velocity,saved,feetSettled,world);
        Need(before==session.Serialize()&&oldEnemy==enemy.health&&oldDash==evasion.remaining,"driver mutated observed state");
    }
    void ReachCliff(){
        Tick(1.8f,false);Need(replay.Controls().Pressed(VK_RETURN),"formal start required");Tick();Tick();
        session.feet={0,0,18};Tick();session.feet={-4.8f,0,18};Tick();Tick(1.4f);
        session.feet={-7.24f,0,18};Tick(1.4f);Tick(1.4f);Tick(1.4f);
        Need(replay.Controls().Pressed('E'),"formal tool interaction required");
        session.repairMaterials=1;Tick(1.4f);Tick(1.4f);Tick(1.4f);
        session.feet={-4.8f,0,18};Tick(1.4f);Tick(1.4f);Tick();
        Need(replay.Phase()=="bridge-road-18","shelter route retained");
        struct Point{float x,z;unsigned action;};
        constexpr Point points[]{{0,18,0},{0,-8,0},{6,-8,4},{0,-8,0},{0,6,0},{2,6,1},
            {0,6,0},{0,14,0},{-3,14,2},{0,14,0},{0,22,0},{6,22,3},{0,22,0},{0,26,0},
            {-6,26,5},{0,26,0},{0,28.2f,6}};
        for(const auto p:points){
            session.feet={p.x,0,p.z};Tick();
            if(!p.action)continue;
            Tick();Tick();Need(replay.Controls().Pressed('E'),"bridge interaction edge required");
            if(p.action<=3)session.collected|=1u<<(p.action-1);
            else if(p.action==4)session.repairMaterials|=2;
            else if(p.action==5)session.repairMaterials|=4;
            else session.bridgeRepaired=true;
            Tick();
        }
        session.feet={0,0,41};Tick();
        Need(replay.Phase()=="combat-1-approach","cliff route must pass combat");
        session.feet={0,0,44};
        for(unsigned round=0;round<3;++round){
            enemy.feet={0,0,46};enemy.phase=EnemyPhase::Windup;enemy.phaseTime=Encounter::WindupDuration;
            enemy.countered=false;enemy.playerCooldown=0;enemy.strikeTarget=session.feet;evasion={};
            Tick();Need(replay.Controls().Pressed('Q'),"formal dodge edge required");
            evasion.direction={round%2?-1.f:1.f,0,0};evasion.remaining=.2f;evasion.cooldown=1.1f;
            session.feet.x+=round%2?-2.f:2.f;Tick();
            enemy.phase=EnemyPhase::Recovery;enemy.phaseTime=Encounter::RecoveryDuration;evasion.remaining=0;Tick();
            session.feet={0,0,44};Tick();Tick();Tick();
            Need(replay.Controls().Pressed('F'),"formal counter edge required");
            --enemy.health;enemy.countered=true;
            if(!enemy.health){enemy.phase=EnemyPhase::Defeated;session.guardianDefeated=true;}
            Tick();
        }
        Need(replay.Phase()=="west-cliff-road-62"&&replay.Counters()==3&&replay.Dodges()==3&&!replay.Finished(),
            "cliff route must follow verified third counter");
    }
    void ReachDownhill(){
        ReachCliff();session.feet={0,0,62};Tick();
        Need(replay.Phase()=="west-cliff-relay-approach-62","known west relay approach phase");
        session.feet={-18,0,62};Tick();
        Need(replay.Phase()=="west-cliff-north-bypass-66","north bypass before relay body");
        session.feet={-18,0,66};Tick();
        Need(replay.Phase()=="west-cliff-west-66","western traverse after north bypass");
        const float middleX=replay.WestCliffTargetX()==-44.f?-43.75f:-47.f;
        session.feet={middleX,0,66};Tick();
        Need(replay.Phase()=="west-cliff-approach-channel-58","slope channel approach phase");
        session.feet={middleX,0,58};Tick();
        Need(replay.Phase()=="west-cliff-approach-58","final original slope approach phase");
        if(replay.WestCliffTargetX()==-44.f){
            Tick();Need(replay.Controls().Down('A')&&!replay.Controls().Down('S'),"channel approaches original point downhill west");
        }
        session.feet={replay.WestCliffTargetX(),0,58};Tick();
        Need(replay.Phase()=="west-cliff-downhill"&&Released(replay),"arrived at rest before downhill");
    }
};
void Selection(){
    for(float x:{-47.f,-44.f}){
        InputReplay r;Need(r.EnableWestCliffRoute(x)&&r.WestCliffRoute()&&r.WestCliffTargetX()==x&&r.BridgeRoute()&&r.CombatRoute(),"cliff configuration");
        Need(!r.EnableWestCliffRoute(x)&&!r.EnableBridgeRoute()&&!r.EnableCombatRoute()&&!r.EnablePowerRoute()&&
            !r.EnableCompletionRoute()&&!r.EnableDeathRetryRoute()&&!r.EnableCheckpointRoute()&&!r.EnableMotionCheckpointRoute(),"other route cannot overwrite cliff");
        Session s;r.Tick(s,0,1.8f,true,false,Dt);Need(!r.EnableWestCliffRoute(x),"cannot change active route");
    }
    for(float x:{0.f,-46.f,-47.0001f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        InputReplay r;Need(!r.EnableWestCliffRoute(x)&&!r.WestCliffRoute()&&!r.BridgeRoute()&&!r.CombatRoute(),"invalid target must leave route unchanged");
    }
    using Enable=bool(InputReplay::*)();
    for(Enable enable:{&InputReplay::EnableBridgeRoute,&InputReplay::EnableCombatRoute,&InputReplay::EnablePowerRoute,
        &InputReplay::EnableCompletionRoute,&InputReplay::EnableDeathRetryRoute,&InputReplay::EnableCheckpointRoute,&InputReplay::EnableMotionCheckpointRoute}){
        InputReplay r;Need((r.*enable)()&&!r.EnableWestCliffRoute(-47)&&!r.WestCliffRoute(),"cliff cannot overwrite selected route");
    }
}
void Routes(){
    for(float x:{-47.f,-44.f}){
        Fixture f(x);f.ReachDownhill();
        auto noProgress=f;for(unsigned tick=0;tick<100&&!noProgress.replay.Finished();++tick)noProgress.Tick();
        Need(noProgress.replay.Failure()=="west-cliff-downhill-no-progress"&&Released(noProgress.replay),"stationary downhill cannot pass");
        auto rotated=f;rotated.Tick(1.8f,true,1.57079632679f);
        Need(rotated.replay.Controls().Down('A')&&!rotated.replay.Controls().Down('S'),"world downhill uses camera inverse steering");
        const std::array<std::string_view,4> phases{"west-cliff-downhill","west-cliff-settle","west-cliff-reverse","west-cliff-exit"};
        for(unsigned stage=0;stage<phases.size();++stage){
            Need(f.replay.Phase()==phases[stage],"bounded slope phase order");
            auto invalid=f;invalid.session.guardianDefeated=false;invalid.Tick();
            Need(invalid.replay.Finished()&&!invalid.replay.Passed(),"lost prerequisite cannot pass");
            if(stage==3){
                auto stuck=f;for(unsigned tick=0;tick<100&&!stuck.replay.Finished();++tick)stuck.Tick();
                Need(stuck.replay.Failure()=="west-cliff-exit-no-progress","exit progress must be checked before settle origin changes");
            }
            unsigned ticks=0;
            while(f.replay.Phase()==phases[stage]&&ticks++<100){
                // 模擬上一按鍵的唯讀回報；上坡故意保持靜止，測試後續離開不能被省略。
                if(f.replay.Controls().Down('S'))f.session.feet.z-=.02f;
                f.Tick();
                if(f.replay.Phase()==phases[stage]){
                    Need(!f.replay.Controls().Down(VK_SHIFT)&&!f.replay.Controls().Down('Q')&&!f.replay.Controls().Down(VK_SPACE),"slope segment is normal walking only");
                    if(stage==1)Need(Released(f.replay),"settle releases keys");
                }
            }
            Need(ticks<100&&ticks>=(stage==1?10u:60u),"bounded duration must not complete early");
        }
        Need(!f.replay.Finished()&&f.replay.Phase()=="west-cliff-exit-settle"&&Released(f.replay),"exit must await real settled observations");
        auto elapsed=f;elapsed.Tick(1.8f,true,0,.1f);
        for(unsigned tick=0;tick<4;++tick){elapsed.Tick(1.8f,true,0,.1f);Need(!elapsed.replay.Finished(),"unobserved issued dt must not count toward half second");}
        elapsed.Tick(1.8f,true,0,.1f);Need(elapsed.replay.Passed(),"settle counts previous observed dt rather than fixed tick count");
        auto moving=f;
        for(unsigned tick=0;tick<125&&!moving.replay.Finished();++tick){
            moving.session.feet.z-=Dt;moving.Tick(1.8f,true,0,Dt,{0,0,-1});
            Need(Released(moving.replay),"settle never reissues movement keys");
        }
        Need(moving.replay.Failure()=="west-cliff-exit-settle-timeout","moving body must fail bounded settle deadline");
        auto swinging=f;
        for(unsigned tick=0;tick<125&&!swinging.replay.Finished();++tick)swinging.Tick(1.8f,true,0,Dt,{},false);
        Need(swinging.replay.Failure()=="west-cliff-exit-settle-timeout","zero body speed cannot hide active foot swings");
        auto boundary=f;
        for(unsigned tick=0;tick<125&&!boundary.replay.Finished();++tick)boundary.Tick(1.8f,true,0,Dt,{.001f,0,0});
        Need(boundary.replay.Failure()=="west-cliff-exit-settle-timeout","settle speed boundary is strictly less than threshold");
        // 先回報減速但仍擺腳，再回報真正停妥；不是抵達後立刻假造靜止的原生驗收。
        for(unsigned tick=0;tick<20;++tick){
            const float speed=1.f-float(tick)/20.f;
            f.session.feet.z-=speed*Dt;f.Tick(1.8f,true,0,Dt,{0,0,-speed},false);
            Need(!f.replay.Finished()&&Released(f.replay),"deceleration must not complete settle");
        }
        for(unsigned tick=0;tick<20;++tick){
            f.Tick();Need(!f.replay.Finished(),"less than half second settled cannot pass");
        }
        f.Tick(1.8f,true,0,Dt,{},false);
        Need(!f.replay.Finished(),"late foot swing resets continuous settled duration");
        for(unsigned tick=0;tick<29;++tick){f.Tick();Need(!f.replay.Finished(),"settled interval must restart after foot swing");}
        f.Tick();
        Need(f.replay.Passed()&&Released(f.replay),"verified slope cycle completes only after observed half second rest");
    }
}
void Bounds(){
    Fixture stalled(-47);stalled.ReachCliff();
    for(unsigned i=0;i<605&&!stalled.replay.Finished();++i)stalled.Tick();
    Need(stalled.replay.Failure()=="phase-stalled-600-ticks"&&Released(stalled.replay),"long approach cannot refresh on key issue");
    Fixture progress(-47);progress.ReachCliff();progress.session.feet={0,0,-1000};
    for(unsigned i=0;i<7210&&!progress.replay.Finished();++i){progress.session.feet.z+=.025f;progress.Tick();}
    Need(progress.replay.Ticks()==7200&&progress.replay.Failure()=="total-tick-limit"&&Released(progress.replay),"actual approach progress does not bypass total limit");
}
void WorldMotion(){
    using namespace Aevrynd;
    Fixture f(-44);f.ReachDownhill();
    HeightField3D terrain({{-60,0,40},41,41,1});terrain.RecalculateRange();
    CharacterControllerSettings3D settings;settings.maximumTerrainStandingGrade=settings.maximumTerrainUphillGrade=1;
    CharacterController3D actor(settings);actor.Reset({-44,.1f,58});
    LocomotionMotor motor;
    Need(motor.Step(actor,{}, {},{},terrain)&&!actor.Grounded()&&actor.Velocity().y<0,"real falling controller sample");
    f.session.feet=actor.Position();
    SaveGame saved;saved.session=f.session;saved.encounter=f.enemy;saved.evasion=f.evasion;saved.motion.emplace();
    saved.motion->controller=actor.CaptureMotionState();saved.motion->locomotion.motor=motor.CaptureMotionState();
    Need(saved.Valid(),"fall fixture structurally valid");
    WestCliffReplayWorld world{actor,terrain,{},{},true};
    const auto hash=actor.StateHash();const auto steps=actor.Stats().steps;
    Need(world.Validate(&saved,f.session,false,1.8f,actor.Velocity()),"actual fall verified without fake grounded");
    Need(actor.StateHash()==hash&&actor.Stats().steps==steps,"readonly world check never simulates actor");
    Fixture waiting(-44);waiting.ReachCliff();
    HeightField3D arrivalTerrain({{-60,0,40},81,41,1});arrivalTerrain.RecalculateRange();
    CharacterController3D arrival(settings);arrival.Reset({0,.1f,62});LocomotionMotor arrivalMotor;
    Need(arrivalMotor.Step(arrival,{}, {},{},arrivalTerrain),"arrival fall observation");
    waiting.session.feet=arrival.Position();auto arrivalSaved=saved;arrivalSaved.session=waiting.session;
    arrivalSaved.motion->controller=arrival.CaptureMotionState();arrivalSaved.motion->locomotion.motor=arrivalMotor.CaptureMotionState();
    // 使用含抵達點的真地形；停滯負控不以場外拒絕冒充逾時。
    const WestCliffReplayWorld supportedArrival{arrival,arrivalTerrain,{},{},true};
    Need(supportedArrival.Validate(&arrivalSaved,waiting.session,false,1.8f,arrival.Velocity()),"airborne arrival is physically valid");
    for(unsigned tick=0;tick<605&&!waiting.replay.Finished();++tick)
        waiting.Tick(1.8f,true,0,Dt,arrival.Velocity(),true,false,&arrivalSaved,&supportedArrival);
    Need(waiting.replay.Failure()=="phase-stalled-600-ticks"&&Released(waiting.replay),"airborne zero-horizontal arrival cannot refresh stall clock");
    auto legacy=f;legacy.Tick(1.8f,true,0,Dt,actor.Velocity(),false,false);
    Need(legacy.replay.Failure()=="west-cliff-live-state-invalid","legacy route retains grounded gate");
    auto physical=f;physical.Tick(1.8f,true,0,Dt,actor.Velocity(),false,false,&saved,&world);
    Need(!physical.replay.Finished()&&!physical.replay.Controls().Down(VK_SPACE)&&!physical.replay.Controls().Down('Q'),"verified falling walking route continues without jump");
    auto rejected=world;rejected.motionAccepted=false;
    auto failure=f;failure.Tick(1.8f,true,0,Dt,actor.Velocity(),false,false,&saved,&rejected);
    Need(failure.replay.Failure()=="west-cliff-world-motion-invalid"&&Released(failure.replay),"missing committed motion releases keys and fails");
    Need(!world.Validate(nullptr,f.session,false,1.8f,actor.Velocity()),"missing snapshot rejected");
    auto wrong=saved;wrong.motion->controller.feet.y+=.1f;wrong.session.feet.y+=.1f;
    Need(wrong.Valid()&&!world.Validate(&wrong,wrong.session,false,1.8f,actor.Velocity()),"stale foreign position rejected");
    Need(!world.Validate(&saved,f.session,true,1.8f,actor.Velocity()),"contradictory grounded rejected");
    Need(!world.Validate(&saved,f.session,false,1.4f,actor.Velocity()),"contradictory height rejected");
    wrong=saved;wrong.motion->locomotion.motor.wasDashing=true;
    Need(!world.Validate(&wrong,f.session,false,1.8f,actor.Velocity()),"hidden dash phase rejected");
    std::array<BoxCollider3D,1> boxes{{{actor.Center(),actor.Extents()}}};
    const WestCliffReplayWorld obstructed{actor,terrain,boxes,{},true};
    Need(!obstructed.Validate(&saved,f.session,false,1.8f,actor.Velocity()),"actual full-body overlap rejected");
    for(unsigned z=0;z<41;++z)for(unsigned x=0;x<41;++x)terrain.SetHeight(x,z,1);
    terrain.RecalculateRange();
    Need(!world.Validate(&saved,f.session,false,1.8f,actor.Velocity()),"below real terrain rejected");
    Need(actor.StateHash()==hash&&actor.Stats().steps==steps,"negative validation leaves actor untouched");
}
void StandingStaging(){
    using namespace Aevrynd;
    HeightField3D terrain({{-60,0,40},81,41,1});terrain.RecalculateRange();
    CharacterControllerSettings3D settings;settings.maximumTerrainStandingGrade=settings.maximumTerrainUphillGrade=1;
    for(float x:{-44.f,-47.f}){
        Fixture f(x,true);f.ReachCliff();
        auto missing=f;missing.Tick();
        Need(missing.replay.Failure()=="west-cliff-world-motion-invalid","standing route requires real world observation");
        const std::array<Vec2,6> points{{{0,62},{-18,62},{-18,66},{x,66},{x,60},{x,60}}};
        const std::array<std::string_view,6> after{"west-cliff-relay-approach-62","west-cliff-north-bypass-66",
            "west-cliff-west-66","west-cliff-approach-channel-60","west-cliff-approach-60","west-cliff-downhill"};
        for(unsigned i=0;i<points.size();++i){
            const auto point=points[i];CharacterController3D actor(settings);actor.Reset({point.x,0,point.y});
            LocomotionMotor motor;Need(motor.Step(actor,{}, {},{},terrain)&&actor.Grounded(),"standing staging support");
            f.session.feet=actor.Position();SaveGame saved;saved.session=f.session;saved.encounter=f.enemy;saved.evasion=f.evasion;
            saved.motion.emplace();saved.motion->controller=actor.CaptureMotionState();saved.motion->locomotion.motor=motor.CaptureMotionState();
            WestCliffReplayWorld world{actor,terrain,{},{},true};
            Need(saved.Valid()&&world.Validate(&saved,f.session,true,1.8f,{}),"valid staging sample");
            if(i==0){
                auto unready=f;
                for(unsigned t=0;t<605&&!unready.replay.Finished();++t)unready.Tick(1.8f,true,0,Dt,{},false,true,&saved,&world);
                Need(unready.replay.Failure()=="phase-stalled-600-ticks","grounded unready feet cannot refresh arrival timeout");
            }
            f.Tick(1.8f,true,0,Dt,{},true,true,&saved,&world);
            Need(f.replay.Phase()==after[i],"standing route stages on ledge before steep crossing");
        }
    }
}
}
int main()try{Selection();Routes();Bounds();WorldMotion();StandingStaging();std::cout<<"WEST_CLIFF_REPLAY_DRIVER_PASS checks="<<checks<<" native_play=0\n";return 0;}
catch(const std::exception& e){std::cerr<<"WEST_CLIFF_REPLAY_DRIVER_FAIL "<<e.what()<<'\n';return 1;}
