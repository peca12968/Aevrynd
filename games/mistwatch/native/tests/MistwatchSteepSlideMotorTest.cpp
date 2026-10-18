#include "MistwatchLocomotionMotor.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace Aevrynd;
using namespace Mistwatch;
namespace {
unsigned checks{},ticks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
HeightField3D Terrain(float grade){
    HeightField3D terrain({{-256,0,-256},257,257,2});
    for(unsigned z=0;z<257;++z)for(unsigned x=0;x<257;++x)terrain.SetHeight(x,z,grade*(-256.f+2.f*float(x)));
    terrain.RecalculateRange();return terrain;
}
CharacterControllerSettings3D Settings(bool enabled=true){
    CharacterControllerSettings3D s;s.moveSpeed=2;s.maximumTerrainStandingGrade=enabled?1.f:0.f;return s;
}
void SameMotion(const CharacterController3D& a,const CharacterController3D& b){
    const auto x=a.CaptureMotionState(),y=b.CaptureMotionState();
    Need(x.feet.x==y.feet.x&&x.feet.y==y.feet.y&&x.feet.z==y.feet.z&&
        x.velocity.x==y.velocity.x&&x.velocity.y==y.velocity.y&&x.velocity.z==y.velocity.z&&
        x.height==y.height&&x.yaw==y.yaw&&x.grounded==y.grounded,"restored controller motion diverged");
}
void Restore(const CharacterController3D& actor,const LocomotionMotor& motor,const HeightField3D& terrain,
    CharacterController3D& restored,LocomotionMotor& replay){
    Need(restored.TryRestoreMotionState(actor.CaptureMotionState(),{},{},terrain),"controller rejected its own slide state");
    Need(replay.TryRestoreMotionState(motor.CaptureMotionState(),restored),"motor rejected its own slide state");
    SameMotion(actor,restored);Need(replay.StateHash()==motor.StateHash(),"restored motor hash differs");
}
void Sliding(){
    const auto terrain=Terrain(2);const auto settings=Settings();
    CharacterController3D actor(settings);actor.Reset({});LocomotionMotor motor;
    Need(motor.Step(actor,{},{},{},terrain),"first no-input slide tick");++ticks;
    Need(!actor.Grounded()&&actor.Position().x<0&&actor.Velocity().y<0&&motor.Velocity().x<0,
        "first no-input tick must actually slide downhill");
    Need(motor.Phase()==LocomotionPhase::JumpOrFall&&motor.CaptureMotionState().headingReady,
        "passive slide must have immediately restorable heading and phase");
    CharacterController3D restored(settings);LocomotionMotor replay;
    Restore(actor,motor,terrain,restored,replay);
    const auto savedHash=replay.StateHash();auto corrupt=motor.CaptureMotionState();corrupt.headingReady=false;corrupt.heading=0;
    Need(!replay.TryRestoreMotionState(corrupt,restored)&&replay.StateHash()==savedHash,"undefined heading with speed remains invalid");
    corrupt=motor.CaptureMotionState();corrupt.velocity.z+=1;
    Need(!replay.TryRestoreMotionState(corrupt,restored)&&replay.StateHash()==savedHash,"motor cannot bypass controller velocity validation");
    bool fast=false;
    for(unsigned tick=0;tick<360;++tick){
        Need(motor.Step(actor,{},{},{},terrain),"continued gravity slide");++ticks;
        Need(!actor.Grounded()&&motor.Phase()==LocomotionPhase::JumpOrFall&&actor.Stats().jumps==0,"slide is not grounded or a jump");
        const double horizontal=std::hypot(double(actor.Velocity().x),double(actor.Velocity().z));
        const float limit=float(std::hypot(double(settings.maximumFallSpeed),double(settings.moveSpeed)*4));
        Need(horizontal<=std::nextafter(limit,std::numeric_limits<float>::infinity()),"slide respects controller horizontal bound");
        if(horizontal>settings.moveSpeed*4.00001f)fast=true;
        Restore(actor,motor,terrain,restored,replay);
    }
    Need(fast,"fixture must exceed legacy input-speed limit using gravity only");
    // 每輪自還原後再走完全相同的正式控制器路徑，不以手造高速狀態冒充滑坡存讀檔。
    for(unsigned tick=0;tick<120;++tick){
        LocomotionInput input;input.movement={0,0,tick<60?1.f:-1.f};
        Need(motor.Step(actor,input,{},{},terrain)&&replay.Step(restored,input,{},{},terrain),"saved slide replay step");ticks+=2;
        SameMotion(actor,restored);Need(motor.StateHash()==replay.StateHash(),"saved slide replay not deterministic");
    }
}
void StaleSupport(){
    for(bool jumping:{false,true}){
        auto terrain=Terrain(0);auto settings=Settings();settings.moveSpeed=6;
        CharacterController3D actor(settings);actor.Reset({});LocomotionMotor motor;
        Need(motor.Step(actor,{},{},{},terrain)&&actor.Grounded(),"flat support setup");
        LocomotionInput run;run.movement={0,0,1};run.enableRunFlight=true;
        for(unsigned tick=0;tick<200&&motor.SupportTicks()<10;++tick){Need(motor.Step(actor,run,{},{},terrain),"reach pending auto launch");++ticks;}
        Need(actor.Grounded()&&motor.SupportTicks()==10,"stale support fixture reaches actual launch boundary");
        const auto actorHash=actor.StateHash(),motorHash=motor.StateHash();
        const std::array<const ConvexAabbSweep3D*,1> invalidHulls{nullptr};
        Need(!motor.Step(actor,run,{},invalidHulls,terrain)&&actor.StateHash()==actorHash&&motor.StateHash()==motorHash,
            "invalid launch geometry must reject entire motor transaction");
        // 改變支撐環境但不修改任何 actor/motor 狀態，明確重現上一幀 grounded 的過期觀測。
        terrain=Terrain(2);const auto jumps=actor.Stats().jumps;run.jump=jumping;
        Need(motor.Step(actor,run,{},{},terrain),"stale support must continue as a valid slide tick");++ticks;
        Need(!actor.Grounded()&&actor.Stats().jumps==jumps&&actor.Velocity().y<=0&&
            motor.Phase()==LocomotionPhase::JumpOrFall&&motor.SupportTicks()==0,"stale support must not authorize jump or run hop");
    }
}
void Legacy(){
    const auto terrain=Terrain(2);const auto settings=Settings(false);
    CharacterController3D actor(settings);actor.Reset({});LocomotionMotor motor;
    for(unsigned tick=0;tick<30;++tick){Need(motor.Step(actor,{},{},{},terrain),"legacy standing tick");++ticks;}
    Need(actor.Grounded()&&Length(actor.Position())==0&&Length(motor.Velocity())==0&&motor.Phase()==LocomotionPhase::Standing,
        "disabled policy retains legacy steep standing");
    LocomotionInput jump;jump.jump=true;
    Need(motor.Step(actor,jump,{},{},terrain)&&actor.Stats().jumps==1&&actor.Velocity().y>0,"disabled policy retains legacy steep jump");
    CharacterController3D restored(settings);LocomotionMotor replay;Restore(actor,motor,terrain,restored,replay);
}
unsigned bufferChecks{},bufferTicks{},bufferCases{},bufferRejections{},bufferQueuedJumps{},bufferFreshControls{};
void BufferNeed(bool value,const char* why){++bufferChecks;if(!value)throw std::runtime_error(why);}
void BufferStep(CharacterController3D& actor,LocomotionMotor& motor,const HeightField3D& terrain,
    LocomotionInput input={}){
    BufferNeed(motor.Step(actor,input,{},{},terrain),"buffer valid tick rejected");++bufferTicks;
}
void BeginDeniedEdge(CharacterController3D& actor,LocomotionMotor& motor,const HeightField3D& flat,
    const HeightField3D& steep,bool queued){
    actor.Reset({});BufferStep(actor,motor,flat);
    BufferNeed(actor.Grounded()&&actor.Stats().jumps==0,"buffer fixture must first establish real flat support");
    LocomotionInput press;press.jump=queued;BufferStep(actor,motor,steep,press);
    BufferNeed(!actor.Grounded()&&actor.Stats().jumps==0&&actor.Velocity().y<=0&&
        motor.Phase()==LocomotionPhase::JumpOrFall,"stale flat support cannot authorize a real steep jump");
}
void DelayOnSteep(CharacterController3D& actor,LocomotionMotor& motor,const HeightField3D& steep,unsigned delay){
    for(unsigned tick=0;tick<delay;++tick){
        BufferStep(actor,motor,steep);
        BufferNeed(!actor.Grounded()&&actor.Stats().jumps==0&&actor.Velocity().y<=0,
            "buffer must not launch while true support remains steep");
    }
}
void WatchAndFreshControl(CharacterController3D& actor,LocomotionMotor& motor,const HeightField3D& terrain,
    std::uint64_t expected){
    // 足夠涵蓋完整落地，未送任何新edge；不能在再次接地後把同一按鍵重播成第二跳。
    for(unsigned tick=0;tick<180;++tick){BufferStep(actor,motor,terrain);
        BufferNeed(actor.Stats().jumps==expected,"buffer replayed or invented a jump without another edge");}
    BufferNeed(actor.Grounded(),"buffer fixture did not recover stable support after observation");
    LocomotionInput fresh;fresh.jump=true;BufferStep(actor,motor,terrain,fresh);
    BufferNeed(actor.Stats().jumps==expected+1&&actor.Velocity().y>0&&!actor.Grounded(),
        "fresh supported edge positive control failed");++bufferFreshControls;
}
void FlatRecoveryTick(CharacterController3D& actor,LocomotionMotor& motor,const HeightField3D& flat){
    BufferStep(actor,motor,flat);
    BufferNeed(actor.Grounded()&&actor.Stats().jumps==0,
        "first flat recovery must establish support, not jump using an airborne prior state");
}
void ObserveRecoveredBuffer(CharacterController3D& actor,LocomotionMotor& motor,const HeightField3D& flat,bool expectedJump){
    BufferStep(actor,motor,flat);
    const std::uint64_t expected=expectedJump?1u:0u;
    BufferNeed(actor.Stats().jumps==expected,"exact supported-buffer window result differs");
    if(expectedJump)BufferNeed(!actor.Grounded()&&actor.Velocity().y>0,"accepted queued edge did not truly launch");
    else BufferNeed(actor.Grounded()&&actor.Velocity().y==0,"expired or cancelled buffer changed stable support");
    bufferQueuedJumps+=static_cast<unsigned>(expected);WatchAndFreshControl(actor,motor,flat,expected);
}
bool SameStats(const CharacterControllerStats3D& a,const CharacterControllerStats3D& b){
    return a.steps==b.steps&&a.collisions==b.collisions&&a.jumps==b.jumps&&a.landings==b.landings&&
        a.terrainContacts==b.terrainContacts&&a.directTicks==b.directTicks&&a.directBlocked==b.directBlocked&&
        a.directOverlaps==b.directOverlaps;
}
void RejectedGeometry(CharacterController3D& actor,LocomotionMotor& motor,const HeightField3D& terrain,bool badHull,
    bool freshEdge){
    const std::array<BoxCollider3D,1> badBoxes{{{{0,0,0},{-1,1,1}}}};
    const std::array<const ConvexAabbSweep3D*,1> badHulls{{nullptr}};
    const auto boxes=badHull?std::span<const BoxCollider3D>{}:std::span<const BoxCollider3D>{badBoxes};
    const auto hulls=badHull?std::span<const ConvexAabbSweep3D* const>{badHulls}:std::span<const ConvexAabbSweep3D* const>{};
    const auto actorHash=actor.StateHash(),motorHash=motor.StateHash(),motorTick=motor.Tick();const auto stats=actor.Stats();
    // 拒絕17次已超過整個16tick窗口；失敗不是validtick，不能偷減待處理edge或洩漏新的edge。
    for(unsigned attempt=0;attempt<17;++attempt){
        LocomotionInput input;input.jump=freshEdge;
        BufferNeed(!motor.Step(actor,input,boxes,hulls,terrain),"invalid geometry unexpectedly accepted");++bufferRejections;
        BufferNeed(actor.StateHash()==actorHash&&motor.StateHash()==motorHash&&motor.Tick()==motorTick&&
            SameStats(actor.Stats(),stats),"rejected geometry changed actor, motor, statistics or pending buffer transaction");
    }
}
void SupportedJumpBufferContract(){
    const auto flat=Terrain(0),steep=Terrain(2);std::string active="setup";unsigned bounded{},named{},withinWindow{};
    try{
        for(bool queued:{false,true})for(unsigned delay=0;delay<=20;++delay){
            active="window-queued-"+std::to_string(queued)+"-delay-"+std::to_string(delay);
            CharacterController3D actor{Settings()};LocomotionMotor motor;BeginDeniedEdge(actor,motor,flat,steep,queued);
            DelayOnSteep(actor,motor,steep,delay);FlatRecoveryTick(actor,motor,flat);
            // 測試自己的literal：press當tick16->15；delay後剩15-delay，恢復支撐再用1tick。
            // 因此下一tick僅在delay<=13仍可真跳；不讀motor內部常數當oracle，也不放寬窗口。
            const bool expected=queued&&delay<=13;ObserveRecoveredBuffer(actor,motor,flat,expected);
            withinWindow+=expected;++bounded;++bufferCases;
        }
        for(unsigned cancel=0;cancel<4;++cancel){
            active="cancel-"+std::to_string(cancel);CharacterController3D actor{Settings()};LocomotionMotor motor;
            BeginDeniedEdge(actor,motor,flat,steep,true);
            if(cancel<2){
                // 蹲下／閃避同tick即使再帶jump edge，也不能刷新或保留先前的待處理跳躍。
                LocomotionInput input;input.jump=true;input.crouched=cancel==0;input.dashing=cancel==1;
                BufferStep(actor,motor,steep,input);
                BufferNeed(!actor.Grounded()&&actor.Stats().jumps==0,"crouch or dash cancellation launched on steep support");
            }else{
                const auto actorHash=actor.StateHash();
                if(cancel==2)motor.ClearPendingJump();else motor.Reset();
                BufferNeed(actor.StateHash()==actorHash,"motor cancellation API modified controller state");
            }
            FlatRecoveryTick(actor,motor,flat);ObserveRecoveredBuffer(actor,motor,flat,false);++named;++bufferCases;
        }
        for(bool queued:{false,true}){
            active="persistent-steep-queued-"+std::to_string(queued);CharacterController3D actor{Settings()};LocomotionMotor motor;
            BeginDeniedEdge(actor,motor,flat,steep,queued);DelayOnSteep(actor,motor,steep,64);
            FlatRecoveryTick(actor,motor,flat);ObserveRecoveredBuffer(actor,motor,flat,false);++named;++bufferCases;
        }
        for(bool badHull:{false,true})for(bool ready:{false,true}){
            active="atomic-rejection-hull-"+std::to_string(badHull)+"-ready-"+std::to_string(ready);
            CharacterController3D actor{Settings()};LocomotionMotor motor;BeginDeniedEdge(actor,motor,flat,steep,true);
            // 精確邊界delay13：在steep待支撐或flat已支撐兩個階段，任何錯誤消耗都會丟掉唯一機會。
            DelayOnSteep(actor,motor,steep,13);if(ready)FlatRecoveryTick(actor,motor,flat);
            RejectedGeometry(actor,motor,ready?flat:steep,badHull,false);
            if(!ready)FlatRecoveryTick(actor,motor,flat);
            ObserveRecoveredBuffer(actor,motor,flat,true);++named;++bufferCases;
        }
        for(bool badHull:{false,true}){
            active="rejected-new-edge-hull-"+std::to_string(badHull);CharacterController3D actor{Settings()};actor.Reset({});LocomotionMotor motor;
            BufferStep(actor,motor,flat);BufferNeed(actor.Grounded(),"failed-edge fixture needs flat support");
            RejectedGeometry(actor,motor,flat,badHull,true);
            FlatRecoveryTick(actor,motor,flat);ObserveRecoveredBuffer(actor,motor,flat,false);++named;++bufferCases;
        }
        for(bool edge:{false,true}){
            active="legacy-policy-zero-edge-"+std::to_string(edge);CharacterController3D actor{Settings(false)};actor.Reset({});LocomotionMotor motor;
            BufferStep(actor,motor,steep);BufferNeed(actor.Grounded(),"legacy policy zero no longer stands on steep terrain");
            LocomotionInput input;input.jump=edge;BufferStep(actor,motor,steep,input);
            const std::uint64_t expected=edge?1u:0u;
            BufferNeed(actor.Stats().jumps==expected,"legacy policy zero edge no longer launches immediately");
            if(edge)BufferNeed(actor.Velocity().y>0&&!actor.Grounded(),"legacy policy zero launch not real");
            WatchAndFreshControl(actor,motor,steep,expected);++named;++bufferCases;
        }
        BufferNeed(bounded==42&&named==14&&bufferCases==56&&withinWindow==14&&bufferQueuedJumps==18&&
            bufferRejections==102&&bufferFreshControls==56,"supported-buffer exact case accounting differs");
        std::cout<<"SUPPORTED_JUMP_BUFFER_CPU_PASS bounded_cases="<<bounded<<" named_cases="<<named
            <<" total_cases="<<bufferCases<<" window_ticks=16 recovery_delay_max=13 within_window_recoveries="<<withinWindow
            <<" actual_queued_jumps="<<bufferQueuedJumps<<" atomic_rejections="<<bufferRejections
            <<" fresh_edge_controls="<<bufferFreshControls<<" valid_ticks="<<bufferTicks<<" checks="<<bufferChecks
            <<" native_play=0 GPU=0\n";
    }catch(const std::exception& error){throw std::runtime_error("supported buffer case="+active+" reason="+error.what());}
}
}
int main()try{
    Sliding();StaleSupport();Legacy();
    SupportedJumpBufferContract();
    std::cout<<"STEEP_SLIDE_MOTOR_PASS ticks="<<ticks<<" checks="<<checks<<" native_play=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"STEEP_SLIDE_MOTOR_FAIL "<<e.what()<<'\n';return 1;}
