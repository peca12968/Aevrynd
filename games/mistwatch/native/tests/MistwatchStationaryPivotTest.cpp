#include "MistwatchPlayerLocomotion.h"
#include <array>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <vector>

using namespace Aevrynd;
using namespace Mistwatch;
namespace {
unsigned checks{};
constexpr float Tau=6.2831853f;
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Fixture {
    HeightField3D terrain{{{-30,0,-30},128,128,1}};
    CharacterController3D actor{CharacterControllerSettings3D{.jumpGraceSeconds=.1f}};
    Evasion evasion;Explorer explorer;PlayerLocomotion owner;
    Fixture(){terrain.Fill(0);actor.Reset({});
        Need(actor.Step({}, {},{},terrain,LocomotionMotor::FixedStep)&&actor.Grounded(),"REAL_GROUND_SETUP");
        explorer.Update({},0,false);Need(explorer.PlaceFeet(actor.Position(),true,false,terrain,{},{}),"REAL_POSE_SETUP");}
    PlayerLocomotionResult Step(PlayerLocomotionInput input={},float dt=LocomotionMotor::FixedStep,
        std::span<const BoxCollider3D> boxes={}){
        const auto result=owner.Advance(actor,evasion,explorer,input,dt,boxes,{},terrain,[]{return true;});
        if(!result.accepted)std::cerr<<"PIVOT_PLAYER_REJECTED tick="<<owner.Motor().Tick()<<" poseRejected="<<result.poseRejected
            <<" failureLine="<<result.poseFailureLine<<" substep="<<result.failedSubstep<<" failedPosition="
            <<result.failedPosition.x<<','<<result.failedPosition.y<<','<<result.failedPosition.z<<'\n';
        Need(result.accepted,"TRUE_PLAYER_ADVANCE_REJECTED");return result;
    }
};
PlayerLocomotionInput Sprint(float z){PlayerLocomotionInput input;input.movement={0,0,z};input.speedMultiplier=1.5f;input.running=true;return input;}
void Prelude(Fixture& f){
    for(unsigned tick=0;tick<120;++tick)Need(f.Step(Sprint(1)).steps==1,"APPROACH_FIXED_TICK");
    for(unsigned tick=0;tick<120;++tick)Need(f.Step().steps==1,"STOP_FIXED_TICK");
    Need(f.actor.Grounded()&&Length(f.owner.Motor().Velocity())==0,"NATURALLY_STOPPED_BEFORE_REVERSE");
}
// 原 PlayerTest 的完整姿態雜湊，含每腳五個未來曲線樣本，只用於拒絕交易的還原核對。
std::uint64_t PoseHash(const Explorer& e){
    std::uint64_t h=1469598103934665603ull;
    const auto add=[&](std::uint64_t n){h^=n;h*=1099511628211ull;};
    const auto scalar=[&](float f){add(std::bit_cast<std::uint32_t>(f));};
    const auto vec=[&](Vec3 v){scalar(v.x);scalar(v.y);scalar(v.z);};
    scalar(e.phase);scalar(e.yaw);scalar(e.requestedYaw);vec(e.lastFeet);add(e.feetInitialized);scalar(e.locomotionDistance);
    scalar(e.locomotionSeconds);vec(e.locomotionVelocity);add(e.gaitGrounded);add(e.nextFoot);
    for(const auto& t:e.pose.localTransforms){vec(t.translation);vec(t.rotation);vec(t.scale);
        scalar(t.quaternion.x);scalar(t.quaternion.y);scalar(t.quaternion.z);scalar(t.quaternion.w);add(t.usesQuaternion);}
    for(const auto& m:e.pose.globalMatrices)for(const auto& row:m.values)for(float f:row)scalar(f);
    for(unsigned i=0;i<2;++i){vec(e.footTargets[i]);add(e.planted[i]);scalar(e.footFacing[i]);scalar(e.swingFacingStart[i]);scalar(e.swingFacingEnd[i]);
        const auto& s=e.footSwings[i];add(s.Ready());add(s.Active());add(s.Replanned());
        for(double d:{s.Elapsed(),s.Duration(),s.LiftHeight(),s.SpeedBound()})add(std::bit_cast<std::uint64_t>(d));
        for(unsigned j=0;j<5;++j){auto copy=s;FootSwingSample3D sample;
            const bool ok=copy.Advance((std::max)(0.,s.Duration()-s.Elapsed())*j/4.,sample);add(ok);
            if(ok){vec(sample.positionWorld);vec(sample.velocityWorld);add(sample.planted);}}
    }
    return h;
}
std::uint64_t EvasionHash(const Evasion& e){
    std::uint64_t h=0;for(float f:{e.direction.x,e.direction.y,e.direction.z,e.remaining,e.cooldown})h=(h^std::bit_cast<std::uint32_t>(f))*1099511628211ull;return h;
}
// 逐欄位擷取公開物理、owner、步態及全部十五關節；不比較 padding，也不拿姿態雜湊代替。
std::vector<std::uint64_t> Words(const Fixture& f){
    std::vector<std::uint64_t> out;
    const auto u=[&](std::uint64_t value){out.push_back(value);};
    const auto scalar=[&](float value){Need(std::isfinite(value),"FINAL_NONFINITE_FLOAT");u(std::bit_cast<std::uint32_t>(value));};
    const auto wide=[&](double value){Need(std::isfinite(value),"FINAL_NONFINITE_DOUBLE");u(std::bit_cast<std::uint64_t>(value));};
    const auto vec=[&](Vec3 value){scalar(value.x);scalar(value.y);scalar(value.z);};
    const auto c=f.actor.CaptureMotionState();vec(c.feet);vec(c.velocity);scalar(c.height);scalar(c.yaw);u(c.grounded);
    const auto stats=f.actor.Stats();for(auto value:{stats.steps,stats.collisions,stats.jumps,stats.landings,stats.terrainContacts,stats.directTicks,stats.directBlocked,stats.directOverlaps})u(value);
    const auto m=f.owner.Motor().CaptureMotionState();vec(m.velocity);scalar(m.heading);u(m.headingReady);u(static_cast<unsigned>(m.phase));u(m.supportTicks);u(m.tick);u(m.runFlight);u(m.wasDashing);
    const auto owner=f.owner.CaptureMotionState();wide(owner.remainder);u(owner.dodgeFlight);u(f.owner.PendingEvadeTicks());
    vec(f.evasion.direction);scalar(f.evasion.remaining);scalar(f.evasion.cooldown);
    const auto& e=f.explorer;scalar(e.phase);scalar(e.yaw);scalar(e.requestedYaw);vec(e.lastFeet);u(e.feetInitialized);
    scalar(e.locomotionDistance);scalar(e.locomotionSeconds);vec(e.locomotionVelocity);u(e.gaitGrounded);u(e.nextFoot);
    for(unsigned side=0;side<2;++side){vec(e.footTargets[side]);u(e.planted[side]);scalar(e.footFacing[side]);scalar(e.swingFacingStart[side]);scalar(e.swingFacingEnd[side]);
        const auto& s=e.footSwings[side];u(s.Ready());u(s.Active());u(s.Replanned());wide(s.Elapsed());wide(s.Duration());wide(s.LiftHeight());wide(s.SpeedBound());}
    Need(e.model.skeleton.joints.size()==15&&e.pose.localTransforms.size()==15&&e.pose.globalMatrices.size()==15,"REAL_FIFTEEN_JOINT_FINAL_POSE");
    for(std::size_t joint=0;joint<15;++joint){const auto& t=e.pose.localTransforms[joint];vec(t.translation);vec(t.rotation);vec(t.scale);
        scalar(t.quaternion.x);scalar(t.quaternion.y);scalar(t.quaternion.z);scalar(t.quaternion.w);u(t.usesQuaternion);
        for(const auto& row:e.pose.globalMatrices[joint].values)for(float value:row)scalar(value);}
    return out;
}
struct RestartMetrics {
    Vec3 entry{};float startYaw{},wrongWay{},lateral{},yawTravel{},maxYawStep{};unsigned activeTicks{},zeroTicks{},turnAtRestTicks{};
    void Observe(const Fixture& f,Vec3 before,float priorYaw,float priorPhase,unsigned tick){
        const auto after=f.actor.Position();const float dx=after.x-before.x,dz=after.z-before.z;
        const float yawStep=std::abs(std::remainder(f.explorer.yaw-priorYaw,Tau));
        wrongWay=(std::max)(wrongWay,(std::max)(0.f,after.z-entry.z));lateral=(std::max)(lateral,std::abs(after.x-entry.x));
        yawTravel+=yawStep;maxYawStep=(std::max)(maxYawStep,yawStep);
        const bool active=f.explorer.footSwings[0].Active()||f.explorer.footSwings[1].Active();activeTicks+=active;
        if(dx==0&&dz==0){++zeroTicks;Need(std::bit_cast<std::uint32_t>(f.explorer.phase)==std::bit_cast<std::uint32_t>(priorPhase),"ZERO_ROOT_ADVANCED_GAIT_PHASE");
            if(yawStep>0&&active)++turnAtRestTicks;}
        std::cout<<"PIVOT_FRAME tick="<<tick<<" dx="<<dx<<" dz="<<dz<<" x="<<after.x<<" z="<<after.z
            <<" speed="<<Length(f.owner.Motor().Velocity())<<" yaw="<<f.explorer.yaw<<" yaw_step="<<yawStep
            <<" phase_before="<<priorPhase<<" phase_after="<<f.explorer.phase<<" active="<<active<<'\n';
        Need(dz<=1.e-6f&&wrongWay<=1.e-6f,"WRONG_WAY_ROOT_MOTION");
        Need(lateral<=.002f,"SIDEWAYS_ROOT_MOTION");
        Need(std::isfinite(f.explorer.yaw)&&yawStep<=9.42477796f*LocomotionMotor::FixedStep+1.e-5f,"BODY_YAW_EXCEEDED_EXISTING_RATE");
    }
    void Finish(const Fixture& f)const{
        const float finalTurn=std::abs(std::remainder(f.explorer.yaw-startYaw,Tau));
        const float travel=entry.z-f.actor.Position().z,speed=Length(f.owner.Motor().Velocity());
        Need(finalTurn>2&&yawTravel>2&&activeTicks>0&&zeroTicks>0&&turnAtRestTicks>0,"NO_REAL_STATIONARY_BODY_AND_FOOT_TURN");
        Need(travel>.25f&&speed>1.f,"RESTART_MUST_PROGRESS_NOT_FREEZE");
        std::cout<<"STATIONARY_PIVOT_RESTART_PASS ticks=120 wrong_way="<<wrongWay<<" lateral="<<lateral<<" final_turn="<<finalTurn
            <<" max_yaw_step="<<maxYawStep<<" active_ticks="<<activeTicks<<" zero_ticks="<<zeroTicks<<" turn_at_rest_ticks="<<turnAtRestTicks
            <<" requested_travel="<<travel<<" speed="<<speed<<'\n';
    }
};
std::vector<std::uint64_t> FourPhases(unsigned hz){
    Fixture f;RestartMetrics metrics;
    for(unsigned phase=0;phase<4;++phase){const auto input=phase==0?Sprint(1):phase==2?Sprint(-1):PlayerLocomotionInput{};
        if(phase==2){Need(f.actor.Grounded()&&Length(f.owner.Motor().Velocity())==0,"NATURAL_ZERO_AT_REVERSE_ENTRY");metrics.entry=f.actor.Position();metrics.startYaw=f.explorer.yaw;}
        for(unsigned frame=1;frame<=hz;++frame){const auto before=f.actor.Position();const auto yaw=f.explorer.yaw,gait=f.explorer.phase;
            Need(f.Step(input,1.f/float(hz)).steps==120/hz,"EXACT_FREQUENCY_FIXED_TICKS");
            if(phase==2&&hz==120)metrics.Observe(f,before,yaw,gait,frame);}
        if(phase==2&&hz==120)metrics.Finish(f);
    }
    Need(f.owner.Motor().Tick()==480,"EXACT_FOUR_SECONDS");auto words=Words(f);
    std::cout<<"PIVOT_FINAL_WORDS hz="<<hz<<" fixed_ticks=480 fields="<<words.size()<<" bits=";
    for(std::size_t i=0;i<words.size();++i)std::cout<<(i?",":"")<<words[i];std::cout<<'\n';return words;
}
void Rollback(bool throwing){
    Fixture f;Prelude(f);bool found=false;unsigned sought{};
    // 第一個真零位移／有限轉身／活動腳曲線事件；不改寫控制器、朝向或曲線時間。
    for(sought=1;sought<=120;++sought){const auto before=f.actor.Position();const float yaw=f.explorer.yaw;
        f.Step(Sprint(-1));const auto after=f.actor.Position();
        if(after.x==before.x&&after.z==before.z&&std::remainder(f.explorer.yaw-yaw,Tau)!=0&&
            (f.explorer.footSwings[0].Active()||f.explorer.footSwings[1].Active())){found=true;break;}}
    Need(found&&f.actor.Grounded(),"TRUE_PIVOT_ROLLBACK_ENTRY_NOT_REACHED");
    CharacterDirectDisplacementPreviewToken3D token;
    Need(f.actor.PreviewDirectDisplacementTick({f.actor.DirectLastTick()+1,{},0},{},f.terrain,token)&&
        f.actor.CanCommitDirectDisplacementPreview(token),"REAL_PENDING_PREVIEW_SETUP");
    const std::array before{f.actor.StateHash(),f.owner.StateHash(),EvasionHash(f.evasion),PoseHash(f.explorer)};
    const auto fields=Words(f);const auto model=f.explorer.model.bindMesh.vertices.data();
    const auto skeleton=f.explorer.model.skeleton.joints.data();unsigned callbacks{};
    const auto result=f.owner.Advance(f.actor,f.evasion,f.explorer,Sprint(-1),LocomotionMotor::FixedStep,{}, {},f.terrain,[&]()->bool{
        ++callbacks;if(throwing)throw std::runtime_error("pivot-camera-probe");return false;});
    Need(callbacks==1&&!result.accepted&&!result.poseRejected,"TRUE_POST_POSE_CALLBACK_MUST_REJECT");
    Need(before==std::array{f.actor.StateHash(),f.owner.StateHash(),EvasionHash(f.evasion),PoseHash(f.explorer)}&&fields==Words(f),"PIVOT_TRANSACTION_DID_NOT_RESTORE_ALL_STATE");
    Need(f.actor.CanCommitDirectDisplacementPreview(token)&&model==f.explorer.model.bindMesh.vertices.data()&&
        skeleton==f.explorer.model.skeleton.joints.data(),"PIVOT_PREVIEW_OR_MODEL_IDENTITY_LOST");
    Need(f.Step(Sprint(-1)).steps==1&&!f.actor.CanCommitDirectDisplacementPreview(token),"SAME_INPUT_RETRY_MUST_COMMIT");
    std::cout<<"PIVOT_ROLLBACK_PASS throwing="<<throwing<<" seek_ticks="<<sought<<" callbacks="<<callbacks<<" preview_restored=1 retry=1\n";
}
void JumpControls(){
    Fixture jump;Prelude(jump);auto input=Sprint(-1);input.jumpPressed=true;
    const auto jumps=jump.actor.Stats().jumps;const float y=jump.actor.Position().y;
    jump.Step(input);Need(jump.actor.Stats().jumps==jumps+1&&!jump.actor.Grounded()&&jump.actor.Velocity().y>6&&jump.actor.Position().y>y,"STATIONARY_S_SPACE_MUST_REALLY_JUMP");
    for(unsigned tick=0;tick<12;++tick){jump.Step();Need(jump.actor.Stats().jumps==jumps+1,"SINGLE_JUMP_EDGE_ONLY");}
    Fixture crouch;PlayerLocomotionInput low;low.crouched=true;low.jumpPressed=true;
    crouch.Step(low);Need(crouch.actor.Grounded()&&crouch.actor.Stats().jumps==0&&crouch.actor.Settings().height==1.4f,"CROUCH_MUST_SUPPRESS_JUMP");
    Fixture roof;PlayerLocomotionInput duck;duck.crouched=true;roof.Step(duck);
    const std::array<BoxCollider3D,1> ceiling{{{{0,1.7f,0},{2,.1f,2}}}};
    PlayerLocomotionInput stand;stand.jumpPressed=true;const auto denied=roof.Step(stand,LocomotionMotor::FixedStep,ceiling);
    Need(denied.lowCeiling&&roof.actor.Grounded()&&roof.actor.Settings().height==1.4f&&roof.actor.Stats().jumps==0&&
        roof.actor.Position().y+roof.actor.Settings().height<=ceiling[0].center.y-ceiling[0].extents.y,"LOW_CEILING_MUST_KEEP_SAFE_CROUCH");
    std::cout<<"PIVOT_JUMP_CONTROLS_PASS standing_S_space=1 single_jump=1 crouch_denied=1 low_ceiling_denied=1\n";
}
}
int main()try{
    std::cout.imbue(std::locale::classic());std::cerr.imbue(std::locale::classic());std::cout<<std::setprecision(17);std::cerr<<std::setprecision(17);
    const auto reference=FourPhases(120);
    for(unsigned hz:{60u,30u}){const auto current=FourPhases(hz);
        if(current!=reference){for(std::size_t i=0;i<(std::min)(reference.size(),current.size());++i)if(reference[i]!=current[i]){
            std::cerr<<"PIVOT_FREQUENCY_DIFFERENCE hz="<<hz<<" field="<<i<<" reference="<<reference[i]<<" actual="<<current[i]<<'\n';break;}}
        Need(current==reference,"FREQUENCY_ACTOR_OWNER_POSE_BITS_DIFFER");
        std::cout<<"PIVOT_FREQUENCY_PASS hz="<<hz<<" fields="<<current.size()<<" bit_equal=1 fixed_ticks=480\n";}
    Rollback(false);Rollback(true);JumpControls();
    std::cout<<"STATIONARY_PIVOT_PASS checks="<<checks<<" frequencies=3 callbacks=2 real_IK=1 preview=1 GPU=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"STATIONARY_PIVOT_FIRST_DIVERGENCE reason="<<e.what()<<" checks="<<checks<<'\n';return 2;}
