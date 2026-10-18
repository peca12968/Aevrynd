#include "MistwatchPlayerLocomotion.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string_view>
#include <utility>

using namespace Aevrynd;
using namespace Mistwatch;
namespace {
unsigned checks{},completedCases{},rollbackChecks{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool Finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}

// 沿用正式 PlayerTest 的姿態核對，包含兩腳各五個未來曲線採樣；不讀 padding。
std::uint64_t PoseHash(const Explorer& e){
    std::uint64_t h=1469598103934665603ull;
    const auto add=[&](std::uint64_t n){h^=n;h*=1099511628211ull;};
    const auto scalar=[&](float v){add(std::bit_cast<std::uint32_t>(v));};
    const auto vec=[&](Vec3 v){scalar(v.x);scalar(v.y);scalar(v.z);};
    scalar(e.phase);scalar(e.yaw);scalar(e.requestedYaw);vec(e.lastFeet);add(e.feetInitialized);
    scalar(e.locomotionDistance);scalar(e.locomotionSeconds);vec(e.locomotionVelocity);add(e.gaitGrounded);add(e.nextFoot);
    for(const auto& t:e.pose.localTransforms){vec(t.translation);vec(t.rotation);vec(t.scale);
        scalar(t.quaternion.x);scalar(t.quaternion.y);scalar(t.quaternion.z);scalar(t.quaternion.w);add(t.usesQuaternion);}
    for(const auto& m:e.pose.globalMatrices)for(const auto& row:m.values)for(float v:row)scalar(v);
    for(unsigned side=0;side<2;++side){vec(e.footTargets[side]);add(e.planted[side]);scalar(e.footFacing[side]);
        scalar(e.swingFacingStart[side]);scalar(e.swingFacingEnd[side]);const auto& swing=e.footSwings[side];
        add(swing.Ready());add(swing.Active());add(swing.Replanned());
        for(double d:{swing.Elapsed(),swing.Duration(),swing.LiftHeight(),swing.SpeedBound()})add(std::bit_cast<std::uint64_t>(d));
        for(unsigned sample=0;sample<5;++sample){auto copy=swing;FootSwingSample3D value;
            const bool valid=copy.Advance((std::max)(0.,swing.Duration()-swing.Elapsed())*sample/4.,value);add(valid);
            if(valid){vec(value.positionWorld);vec(value.velocityWorld);add(value.planted);}}
    }
    return h;
}
std::uint64_t EvasionHash(const Evasion& e){
    std::uint64_t h=0;for(float v:{e.direction.x,e.direction.y,e.direction.z,e.remaining,e.cooldown})
        h=(h^std::bit_cast<std::uint32_t>(v))*1099511628211ull;return h;
}
struct Snapshot {
    std::uint64_t controller,owner,pose,evasion;
    bool operator==(const Snapshot&)const=default;
};
struct Fixture {
    HeightField3D terrain{{{-30,0,-30},128,128,1}};
    CharacterController3D actor{CharacterControllerSettings3D{.jumpGraceSeconds=.1f}};
    Evasion evasion;Explorer explorer;PlayerLocomotion owner;
    Fixture(unsigned hz,bool crouched){
        terrain.Fill(0);actor.Reset({});
        Need(actor.Step({}, {},{},terrain,LocomotionMotor::FixedStep)&&actor.Grounded(),"REAL_CONTROLLER_GROUND_SETUP");
        explorer.Update({},0,false);
        Need(explorer.PlaceFeet(actor.Position(),true,false,terrain,{},{}),"REAL_EXPLORER_POSE_SETUP");
        PlayerLocomotionInput idle;idle.crouched=crouched;
        for(unsigned frame=0;frame<hz/2;++frame)Accepted(idle,1.f/float(hz),120/hz);
        Need(owner.Motor().Tick()==60&&actor.Grounded()&&Length(owner.Motor().Velocity())==0,
            "NATURAL_HALF_SECOND_SETTLE");
        Need(evasion.remaining==0&&evasion.cooldown==0,"NATURAL_FRESH_EVASION");
    }
    Snapshot State()const{return {actor.StateHash(),owner.StateHash(),PoseHash(explorer),EvasionHash(evasion)};}
    template<class Accept>
    PlayerLocomotionResult Advance(PlayerLocomotionInput input,float dt,Accept&& accept){
        return owner.Advance(actor,evasion,explorer,input,dt,{}, {},terrain,std::forward<Accept>(accept));
    }
    PlayerLocomotionResult Accepted(PlayerLocomotionInput input,float dt,unsigned steps){
        const auto result=Advance(input,dt,[]{return true;});
        if(!result.accepted)std::cerr<<"HANDOFF_PLAYER_REJECTED motor_tick="<<owner.Motor().Tick()
            <<" pose_rejected="<<result.poseRejected<<" failure_line="<<result.poseFailureLine
            <<" failed_substep="<<result.failedSubstep<<" failed_x="<<result.failedPosition.x
            <<" failed_y="<<result.failedPosition.y<<" failed_z="<<result.failedPosition.z<<'\n';
        Need(result.accepted,"TRUE_PLAYER_ADVANCE_REJECTED");Need(result.steps==steps,"ACTUAL_FIXED_STEP_COUNT");
        Need(Finite(actor.Position())&&Finite(actor.Velocity())&&Finite(owner.Motor().Velocity()),"FINITE_ACTUAL_MOTION");
        return result;
    }
};
struct Pilot {int lastDash{-1},firstExit{-1},firstLanding{-1};};
struct Case {
    const char* name;int pulse{-1};bool crouched{},cooldown{},tail{},exit{},rollback{},throwing{};
};

void Rollback(Fixture& f,const Case& spec,PlayerLocomotionInput input,float dt,unsigned frame){
    CharacterDirectDisplacementPreviewToken3D token;
    Need(f.actor.PreviewDirectDisplacementTick({f.actor.DirectLastTick()+1,{},0},
        std::span<const BoxCollider3D>{},f.terrain,token),"REAL_PENDING_PREVIEW_SETUP");
    Need(f.actor.CanCommitDirectDisplacementPreview(token),"REAL_PENDING_PREVIEW_INITIAL_VALID");
    const auto before=f.State();bool called=false;
    const auto rejected=f.Advance(input,dt,[&]()->bool{
        called=true;if(spec.throwing)throw std::runtime_error("HANDOFF_EXPECTED_CALLBACK_THROW");return false;});
    const auto after=f.State();const bool previewValid=f.actor.CanCommitDirectDisplacementPreview(token);
    std::cout<<"HANDOFF_ROLLBACK case="<<spec.name<<" frame="<<frame<<" callback_called="<<called
        <<" accepted="<<rejected.accepted<<" state_equal="<<(before==after)<<" preview_valid="<<previewValid
        <<" controller_before="<<before.controller<<" controller_after="<<after.controller
        <<" owner_before="<<before.owner<<" owner_after="<<after.owner
        <<" pose_before="<<before.pose<<" pose_after="<<after.pose
        <<" evasion_before="<<before.evasion<<" evasion_after="<<after.evasion<<'\n';
    Need(called&&!rejected.accepted&&before==after&&previewValid,"CALLBACK_REJECTION_MUST_RESTORE_FULL_TRANSACTION");
    Need(f.actor.CancelDirectDisplacementPreview(token),"CANCEL_UNCOMMITTED_CONTROL_PREVIEW");++rollbackChecks;
    // 呼叫端重送同一輸入；不以另一個按鍵或人工姿態修補失敗。
}

Pilot Run(unsigned hz,const Case& spec,const Pilot* reference=nullptr){
    Fixture f(hz,spec.crouched);const float dt=1.f/float(hz);const unsigned stepsPerFrame=120/hz;
    const auto entryJumps=f.actor.Stats().jumps,entryLandings=f.actor.Stats().landings;
    const auto entryTick=f.owner.Motor().Tick();Pilot observed;unsigned pulses{},dashStarts{},dashFrames{};
    int firstExtraJump=-1;bool sawDash=false;float peakVy=f.actor.Velocity().y;
    Need(spec.pulse<int(hz*3/2),"PREDECLARED_PULSE_INSIDE_FINITE_ROUTE");
    for(unsigned frame=0;frame<hz*3/2;++frame){
        PlayerLocomotionInput input;input.movement={0,0,1};input.dodgeDirection={0,0,1};
        input.crouched=spec.crouched;input.evadePressed=frame==0;
        if(spec.cooldown&&reference&&int(frame)==reference->firstExit+1){
            Need(f.evasion.remaining==0&&f.evasion.cooldown>16*LocomotionMotor::FixedStep,
                "SECOND_Q_NATURALLY_INSIDE_COOLDOWN_OUTSIDE_BUFFER_WINDOW");input.evadePressed=true;
        }
        input.jumpPressed=int(frame)==spec.pulse;pulses+=input.jumpPressed;
        const auto beforeTick=f.owner.Motor().Tick();const float priorRemaining=f.evasion.remaining,priorCooldown=f.evasion.cooldown;
        const auto priorJumps=f.actor.Stats().jumps;const bool priorGround=f.actor.Grounded();
        if(input.jumpPressed&&spec.tail)Need(priorRemaining>0,"TAIL_PULSE_HAS_REAL_ACTIVE_DASH_AT_ENTRY");
        if(input.jumpPressed&&spec.rollback)Rollback(f,spec,input,dt,frame);
        const auto result=f.Accepted(input,dt,stepsPerFrame);
        if(result.dashing){sawDash=true;++dashFrames;observed.lastDash=int(frame);}
        else if(sawDash&&observed.firstExit<0)observed.firstExit=int(frame);
        if(f.actor.Stats().landings>entryLandings&&observed.firstLanding<0)observed.firstLanding=int(frame);
        if(f.actor.Stats().jumps>entryJumps+1&&firstExtraJump<0)firstExtraJump=int(frame);
        if(f.evasion.cooldown>priorCooldown)++dashStarts;
        peakVy=(std::max)(peakVy,f.actor.Velocity().y);
        const auto p=f.actor.Position(),v=f.actor.Velocity();
        std::cout<<"HANDOFF_FRAME case="<<spec.name<<" hz="<<hz<<" frame="<<frame
            <<" motor_before="<<beforeTick<<" motor_after="<<f.owner.Motor().Tick()
            <<" relative_fixed_before="<<beforeTick-entryTick<<" relative_fixed_after="<<f.owner.Motor().Tick()-entryTick
            <<" space="<<input.jumpPressed<<" q="<<input.evadePressed<<" crouched="<<input.crouched
            <<" accepted="<<result.accepted<<" steps="<<result.steps<<" dashing="<<result.dashing
            <<" grounded_before="<<priorGround<<" grounded="<<f.actor.Grounded()
            <<" x="<<p.x<<" y="<<p.y<<" z="<<p.z<<" vx="<<v.x<<" vy="<<v.y<<" vz="<<v.z
            <<" jumps_before="<<priorJumps<<" jumps="<<f.actor.Stats().jumps<<" landings="<<f.actor.Stats().landings
            <<" remaining_before="<<priorRemaining<<" remaining="<<f.evasion.remaining
            <<" cooldown_before="<<priorCooldown<<" cooldown="<<f.evasion.cooldown
            <<" remaining_bits="<<std::bit_cast<std::uint32_t>(f.evasion.remaining)
            <<" vy_bits="<<std::bit_cast<std::uint32_t>(v.y)<<'\n';
        if(input.jumpPressed&&spec.exit)Need(!result.dashing,"EXIT_CONTROL_ACTUALLY_NON_DASHING");
    }
    Need(f.owner.Motor().Tick()-entryTick==180,"FINITE_ROUTE_CONSUMES_180_REAL_FIXED_STEPS");
    Need(pulses==(spec.pulse>=0?1u:0u),"EXACTLY_ONE_REQUESTED_SPACE_EDGE");
    Need(f.evasion.remaining==0,"FINITE_ROUTE_DRAINS_DASH_NATURALLY");
    if(spec.crouched)Need(!sawDash&&dashStarts==0&&f.actor.Stats().jumps==entryJumps&&f.actor.Settings().height==1.4f,
        "EXISTING_CROUCH_SUPPRESSES_JUMP_AND_DASH");
    else Need(sawDash&&observed.lastDash>=0&&observed.firstExit>observed.lastDash&&dashStarts==1,
        "ONE_REAL_DASH_WITH_NATURAL_EXIT_AND_ORIGINAL_COOLDOWN");
    if(spec.pulse<0)Need(f.actor.Stats().jumps==entryJumps+(spec.crouched?0u:1u),"NO_SPACE_DOES_NOT_INVENT_FULL_JUMP");
    if(spec.pulse>=0&&!spec.crouched){
        const bool full=spec.tail||spec.exit||spec.rollback;
        Need(f.actor.Stats().jumps==entryJumps+(full?2u:1u),"DECLARED_LATE_HANDOFF_OR_EARLY_CANCEL_POLICY");
        if(full)Need(firstExtraJump>observed.firstLanding&&observed.firstLanding>=0,"FULL_JUMP_ONLY_AFTER_REAL_LANDING");
    }
    std::cout<<"HANDOFF_SUMMARY case="<<spec.name<<" hz="<<hz<<" frames="<<hz*3/2<<" fixed_steps=180"
        <<" pulse_frame="<<spec.pulse<<" space_edges="<<pulses<<" dash_starts="<<dashStarts<<" dash_frames="<<dashFrames
        <<" last_dash_frame="<<observed.lastDash<<" first_non_dash_frame="<<observed.firstExit
        <<" first_landing_frame="<<observed.firstLanding<<" first_extra_jump_frame="<<firstExtraJump
        <<" jump_delta="<<f.actor.Stats().jumps-entryJumps<<" landing_delta="<<f.actor.Stats().landings-entryLandings
        <<" peak_vy="<<peakVy<<" final_grounded="<<f.actor.Grounded()<<" final_vy="<<f.actor.Velocity().y
        <<" final_cooldown="<<f.evasion.cooldown<<" final_pose_hash="<<PoseHash(f.explorer)<<'\n';
    ++completedCases;return observed;
}
}
int ObserveAllHandoffCases()try{
    std::cout.imbue(std::locale::classic());std::cerr.imbue(std::locale::classic());
    std::cout<<std::setprecision(std::numeric_limits<double>::max_digits10);
    std::cerr<<std::setprecision(std::numeric_limits<double>::max_digits10);
    std::cout<<"HANDOFF_CONFIG grace_seconds=0.1 terrain=flat0 boxes=0 hulls=0 dynamic=0 settle_fixed_steps=60"
        <<" route_fixed_steps=180 movement=positive_z speed_multiplier=1 running=0 native=0 GPU=0"
        <<" late_jump_required=1 fixed_step="<<LocomotionMotor::FixedStep<<" dash_seconds="<<Evasion::Duration
        <<" cooldown_seconds="<<Evasion::Cooldown<<'\n';
    Pilot pilot120;
    for(unsigned hz:{120u,60u,30u}){
        const auto pilot=Run(hz,{"none"});Need(pilot.lastDash>=3&&pilot.firstExit>=0,"BOUNDED_NATURAL_DASH_PILOT");
        std::cout<<"HANDOFF_PILOT hz="<<hz<<" last_dash_frame="<<pilot.lastDash<<" first_non_dash_frame="<<pilot.firstExit
            <<" first_landing_frame="<<pilot.firstLanding<<" internal_phase_resolution="<<120/hz<<'\n';
        Run(hz,{"early",1});
        Run(hz,{"tail_minus2",pilot.lastDash-2,false,false,true});
        Run(hz,{"tail_minus1",pilot.lastDash-1,false,false,true});
        Run(hz,{"tail_last",pilot.lastDash,false,false,true});
        Run(hz,{"first_non_dash",pilot.firstExit,false,false,false,true});
        Run(hz,{"crouched",0,true});
        Run(hz,{"cooldown",-1,false,true},&pilot);
        if(hz==120)pilot120=pilot;
    }
    Run(120,{"callback_false",pilot120.lastDash,false,false,true,false,true,false});
    Run(120,{"callback_throw",pilot120.lastDash,false,false,true,false,true,true});
    Need(completedCases==26&&rollbackChecks==2,"FINITE_CASE_ACCOUNTING");
    std::cout<<"DASH_JUMP_HANDOFF_OBSERVATION_COMPLETE cases="<<completedCases<<" hz_contexts=3 callback_rollbacks="
        <<rollbackChecks<<" checks="<<checks<<" late_jump_required=1 policy_changed=1 native=0 GPU=0\n";return 0;
}catch(const std::exception& e){
    std::cerr<<"DASH_JUMP_HANDOFF_FIRST_DIVERGENCE reason="<<e.what()<<" checks="<<checks
        <<" completed_cases="<<completedCases<<" callback_rollbacks="<<rollbackChecks<<'\n';return 2;
}

namespace {
PlayerLocomotionInput Walking(){PlayerLocomotionInput input;input.movement={0,0,1};input.dodgeDirection={0,0,1};return input;}
void BeginTail(Fixture& f){
    auto input=Walking();input.evadePressed=true;
    Need(f.Accepted(input,LocomotionMotor::FixedStep,1).dashing,"NATURAL_LIFECYCLE_Q_START");
    unsigned ticks{};
    while(f.evasion.remaining>5*LocomotionMotor::FixedStep&&ticks++<40)
        f.Accepted(Walking(),LocomotionMotor::FixedStep,1);
    Need(ticks<40&&f.evasion.remaining>3*LocomotionMotor::FixedStep&&f.evasion.remaining<=.1f,
        "NATURAL_LATE_DASH_BEFORE_SPACE");
    input=Walking();input.jumpPressed=true;
    Need(f.Accepted(input,LocomotionMotor::FixedStep,1).dashing&&f.evasion.remaining>0&&f.actor.Stats().jumps==1,
        "SINGLE_SPACE_RETAINED_DURING_REAL_DASH_NOT_EXTRA_AIR_JUMP");
}
void Drain(Fixture& f,bool expectedExtra){
    bool sawLanding{},fired{};
    for(unsigned tick=0;tick<180;++tick){
        const bool before=f.actor.Grounded();const auto jumps=f.actor.Stats().jumps;
        f.Accepted(Walking(),LocomotionMotor::FixedStep,1);
        sawLanding|=f.actor.Grounded();
        if(f.actor.Stats().jumps>jumps){
            Need(expectedExtra&&before&&sawLanding&&f.actor.Stats().jumps==2&&!fired,
                "HANDOFF_EXACTLY_ONCE_ONLY_AFTER_TRUE_LANDING");fired=true;
        }
    }
    Need(f.actor.Grounded()&&f.actor.Stats().jumps==(expectedExtra?2u:1u)&&fired==expectedExtra,
        "LIFECYCLE_FINAL_JUMP_INVENTORY");
}
void QueuedLifecycle(){
    for(unsigned mode=0;mode<6;++mode){
        Fixture f(120,false);BeginTail(f);const auto queued=f.State();
        const auto zero=f.Advance({},0,[]{return true;});
        Need(zero.accepted&&zero.steps==0&&f.State()==queued,"ZERO_DT_PRESERVES_QUEUE_WITHOUT_AGING");
        if(mode==1)f.owner.Suspend();
        if(mode==2)f.owner.Reset();
        if(mode==3)Need(f.owner.TryRestoreMotionState(f.owner.CaptureMotionState(),f.actor,f.evasion),"OWN_VALID_RESTORE_CANCELS_INPUT");
        if(mode==4){auto crouch=Walking();crouch.crouched=true;f.Accepted(crouch,LocomotionMotor::FixedStep,1);}
        if(mode==5)f.terrain.Fill(-5);
        Drain(f,mode==0);
        std::cout<<"HANDOFF_LIFECYCLE mode="<<mode<<" jumps="<<f.actor.Stats().jumps<<" grounded="<<f.actor.Grounded()<<'\n';
    }
}
void Threshold(){
    for(unsigned mode=0;mode<3;++mode){
        Fixture f(120,false);auto q=Walking();q.evadePressed=true;f.Accepted(q,LocomotionMotor::FixedStep,1);
        while(f.evasion.remaining>.1f)f.Accepted(Walking(),LocomotionMotor::FixedStep,1);
        // 模擬經公開載入預檢接受的邊界值；不是宣稱自然積分恰好產生這三個位元。
        f.evasion.remaining=mode==0?std::nextafter(.1f,0.f):mode==1?.1f:std::nextafter(.1f,1.f);
        Need(f.owner.TryRestoreMotionState(f.owner.CaptureMotionState(),f.actor,f.evasion),"PUBLIC_RESTORE_ACCEPTS_VALID_THRESHOLD_NEIGHBOR");
        const auto bits=std::bit_cast<std::uint32_t>(f.evasion.remaining);
        auto jump=Walking();jump.jumpPressed=true;f.Accepted(jump,LocomotionMotor::FixedStep,1);
        Drain(f,mode<2);
        std::cout<<"HANDOFF_THRESHOLD mode="<<mode<<" entry_remaining_bits="<<bits<<" full_jump="<<(mode<2)<<'\n';
    }
}
void Simultaneous(){
    for(unsigned hz:{120u,60u,30u}){
        Fixture f(hz,false);
        for(unsigned frame=0;frame<hz*3/2;++frame){auto input=Walking();input.evadePressed=input.jumpPressed=frame==0;
            f.Accepted(input,1.f/float(hz),120/hz);}
        Need(f.actor.Grounded()&&f.actor.Stats().jumps==1,"SIMULTANEOUS_Q_SPACE_DOES_NOT_QUEUE_LATE_JUMP");
        std::cout<<"HANDOFF_SIMULTANEOUS hz="<<hz<<" jumps="<<f.actor.Stats().jumps<<'\n';
    }
}
}
int main()try{
    Need(ObserveAllHandoffCases()==0,"COMPLETE_DECLARED_HANDOFF_MATRIX");
    QueuedLifecycle();Threshold();Simultaneous();
    std::cout<<"HANDOFF_LIFECYCLE_CONTRACT_PASS lifecycle=6 threshold_neighbors=3 simultaneous_hz=3 checks="<<checks
        <<" native=0 GPU=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"HANDOFF_LIFECYCLE_FIRST_DIVERGENCE "<<error.what()<<'\n';return 2;}
