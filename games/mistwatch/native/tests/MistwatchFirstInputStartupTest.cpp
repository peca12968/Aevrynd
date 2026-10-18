#include "MistwatchPlayerLocomotion.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace Aevrynd;
using namespace Mistwatch;
namespace {
unsigned checks{},steps{},starts{},frequencies{},callbacks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
PlayerLocomotionInput Move(Vec3 direction){PlayerLocomotionInput input;input.movement=direction;return input;}
struct Fixture {
    HeightField3D terrain{{{-30,0,-30},128,128,1}};
    CharacterController3D actor{CharacterControllerSettings3D{.jumpGraceSeconds=.1f}};
    PlayerLocomotion owner;Evasion evasion;Explorer explorer;
    Fixture(){terrain.Fill(0);actor.Reset({});owner.Reset();
        Need(actor.Step({}, {},{},terrain,LocomotionMotor::FixedStep)&&actor.Grounded(),"REAL_INITIAL_GROUND");
        explorer.Update({},0,false);Need(explorer.PlaceFeet(actor.Position(),true,false,terrain,{}),"REAL_INITIAL_FEET");}
    PlayerLocomotionResult Step(PlayerLocomotionInput input={},float dt=LocomotionMotor::FixedStep){
        const auto result=owner.Advance(actor,evasion,explorer,input,dt,{}, {},terrain,[]{return true;});
        Need(result.accepted&&!result.poseRejected,"REAL_PLAYER_REJECTED");steps+=result.steps;return result;}
};
// 不比較padding；公開曲線另看五個未來樣本，不宣稱讀到了私有係數位元。
std::vector<std::uint64_t> Words(const Fixture& f){
    std::vector<std::uint64_t> out;
    const auto u=[&](std::uint64_t n){out.push_back(n);};
    const auto scalar=[&](float n){Need(std::isfinite(n),"NONFINITE_FLOAT");u(std::bit_cast<std::uint32_t>(n));};
    const auto wide=[&](double n){Need(std::isfinite(n),"NONFINITE_DOUBLE");u(std::bit_cast<std::uint64_t>(n));};
    const auto vec=[&](Vec3 p){scalar(p.x);scalar(p.y);scalar(p.z);};
    u(f.actor.StateHash());u(f.owner.StateHash());
    const auto c=f.actor.CaptureMotionState();vec(c.feet);vec(c.velocity);scalar(c.height);scalar(c.yaw);u(c.grounded);
    const auto stats=f.actor.Stats();
    for(auto n:{stats.steps,stats.collisions,stats.jumps,stats.landings,stats.terrainContacts,stats.directTicks,stats.directBlocked,stats.directOverlaps})u(n);
    const auto m=f.owner.Motor().CaptureMotionState();vec(m.velocity);scalar(m.heading);u(m.headingReady);u(static_cast<unsigned>(m.phase));
    u(m.supportTicks);u(m.tick);u(m.runFlight);u(m.wasDashing);
    const auto owner=f.owner.CaptureMotionState();wide(owner.remainder);u(owner.dodgeFlight);u(f.owner.PendingEvadeTicks());
    vec(f.evasion.direction);scalar(f.evasion.remaining);scalar(f.evasion.cooldown);
    const auto& e=f.explorer;scalar(e.phase);scalar(e.yaw);scalar(e.requestedYaw);vec(e.lastFeet);u(e.feetInitialized);
    scalar(e.locomotionDistance);scalar(e.locomotionSeconds);vec(e.locomotionVelocity);u(e.gaitGrounded);u(e.nextFoot);
    for(const auto& t:e.pose.localTransforms){vec(t.translation);vec(t.rotation);vec(t.scale);
        scalar(t.quaternion.x);scalar(t.quaternion.y);scalar(t.quaternion.z);scalar(t.quaternion.w);u(t.usesQuaternion);}
    for(const auto& matrix:e.pose.globalMatrices)for(const auto& row:matrix.values)for(float n:row)scalar(n);
    for(unsigned side=0;side<2;++side){vec(e.footTargets[side]);u(e.planted[side]);scalar(e.footFacing[side]);
        scalar(e.swingFacingStart[side]);scalar(e.swingFacingEnd[side]);const auto& curve=e.footSwings[side];
        u(curve.Ready());u(curve.Active());u(curve.Replanned());vec(curve.Start());vec(curve.Landing());
        wide(curve.Elapsed());wide(curve.Duration());wide(curve.LiftHeight());wide(curve.SpeedBound());
        for(unsigned sample=0;sample<5;++sample){auto copy=curve;FootSwingSample3D p;
            Need(copy.Advance((std::max)(0.,curve.Duration()-curve.Elapsed())*sample/4.,p),"CURVE_SAMPLE");
            vec(p.positionWorld);vec(p.velocityWorld);wide(p.unusedSeconds);u(p.planted);}
    }
    return out;
}
void Prelude(Fixture& f){
    for(unsigned i=0;i<120;++i)f.Step(Move({0,0,1}));
    for(unsigned i=0;i<120;++i)f.Step();
    Need(f.actor.Grounded()&&Length(f.owner.Motor().Velocity())==0&&f.owner.Motor().CaptureMotionState().headingReady,
        "NATURAL_STOP_WITH_HEADING");
}
void FirstDirections(){
    constexpr std::array<Vec3,4> directions{{{0,0,1},{1,0,0},{0,0,-1},{-1,0,0}}};
    for(unsigned side=0;side<directions.size();++side){
        Fixture first;Need(!first.owner.Motor().CaptureMotionState().headingReady,"FIRST_NOT_UNREADY");
        const auto begin=first.actor.Position();Need(first.Step(Move(directions[side])).steps==1,"FIRST_FIXED_STEP");
        const auto delta=first.actor.Position()-begin;const auto motor=first.owner.Motor().CaptureMotionState();
        const float projection=Dot(delta,directions[side]);
        std::cout<<"STARTUP_FIRST direction="<<side<<" projection="<<projection<<" heading="<<motor.heading<<'\n';
        Need(projection>0&&motor.headingReady,"FIRST_ACCEPTED_TICK_MUST_MOVE_WITH_INPUT");
        Need(std::abs(std::remainder(motor.heading-std::atan2(directions[side].x,directions[side].z),6.2831853f))<=1.e-6f,
            "FIRST_HEADING_MUST_USE_REAL_INPUT");
        ++starts;std::vector<std::uint64_t> reference;
        for(unsigned hz:{120u,60u,30u}){
            Fixture f;const auto entry=f.actor.Position();auto previous=entry;unsigned fixed{};
            for(unsigned frame=0;frame<hz;++frame){
                const auto result=f.Step(Move(directions[side]),1.f/float(hz));fixed+=result.steps;
                Need(result.steps==120/hz,"FREQUENCY_SUBSTEP_COUNT");
                const auto now=f.actor.Position();
                Need(Dot(now-previous,directions[side])>=0,"WRONG_WAY_FIRST_DIRECTION");previous=now;
            }
            Need(fixed==120&&f.owner.Motor().Tick()==120&&Dot(f.actor.Position()-entry,directions[side])>.1f,"REAL_STARTUP_PROGRESS");
            const auto words=Words(f);if(hz==120)reference=words;else Need(words==reference,"STARTUP_FREQUENCY_BITS_DIFFER");
            ++frequencies;
        }
    }
}
void UnreadyControls(){
    Fixture f;Need(!f.owner.Motor().CaptureMotionState().headingReady,"RESET_CREATED_HEADING");
    for(unsigned tick=0;tick<12;++tick){f.Step();Need(!f.owner.Motor().CaptureMotionState().headingReady,"IDLE_CREATED_HEADING");}
    const auto before=Words(f);
    Need(f.Step(Move({0,0,-1}),0).steps==0&&Words(f)==before,"ZERO_DT_CREATED_HEADING_OR_CHANGED_STATE");
    Prelude(f);f.owner.Reset();Need(!f.owner.Motor().CaptureMotionState().headingReady,"PUBLIC_RESET_RETAINED_HEADING");
    for(unsigned tick=0;tick<12;++tick){f.Step();Need(!f.owner.Motor().CaptureMotionState().headingReady,"RESET_IDLE_CREATED_HEADING");}
}
void StoppedReverse(){
    Fixture f;Prelude(f);const auto entry=f.actor.Position();auto previous=entry;unsigned zero{},turning{};
    for(unsigned tick=0;tick<120;++tick){
        const auto oldYaw=f.explorer.yaw;const auto phase=f.explorer.phase;f.Step(Move({0,0,-1}));
        const auto now=f.actor.Position();Need(now.z-previous.z<=1.e-6f&&now.z-entry.z<=1.e-6f,"STOPPED_REVERSE_WRONG_WAY");
        Need(std::abs(now.x-entry.x)<=.002f,"STOPPED_REVERSE_SIDEWAYS");
        if(now.x==previous.x&&now.z==previous.z){
            ++zero;Need(std::bit_cast<std::uint32_t>(phase)==std::bit_cast<std::uint32_t>(f.explorer.phase),"ZERO_ROOT_ADVANCED_PHASE");
            if(oldYaw!=f.explorer.yaw&&(f.explorer.footSwings[0].Active()||f.explorer.footSwings[1].Active()))++turning;
        }
        if(tick==0)Need(now.x==entry.x&&now.z==entry.z,"KNOWN_HEADING_REVERSE_DID_NOT_PIVOT");
        previous=now;
    }
    Need(zero>0&&turning>0&&entry.z-f.actor.Position().z>.25f,"STOPPED_REVERSE_MUST_TURN_AND_PROGRESS");
}
void RejectAndRetry(bool throwing){
    Fixture f;Prelude(f);bool found{};
    for(unsigned tick=0;tick<120&&!found;++tick){
        const auto before=f.actor.Position();const float yaw=f.explorer.yaw;f.Step(Move({0,0,-1}));
        found=f.actor.Position().x==before.x&&f.actor.Position().z==before.z&&yaw!=f.explorer.yaw&&
            (f.explorer.footSwings[0].Active()||f.explorer.footSwings[1].Active());
    }
    Need(found,"REAL_ACTIVE_PIVOT_NOT_REACHED");
    CharacterDirectDisplacementPreviewToken3D token;
    Need(f.actor.PreviewDirectDisplacementTick({f.actor.DirectLastTick()+1,{},0},{},f.terrain,token)&&
        f.actor.CanCommitDirectDisplacementPreview(token),"PENDING_PREVIEW_SETUP");
    const auto before=Words(f);const auto model=f.explorer.model.bindMesh.vertices.data();unsigned called{};
    const auto result=f.owner.Advance(f.actor,f.evasion,f.explorer,Move({0,0,-1}),LocomotionMotor::FixedStep,{}, {},f.terrain,[&]()->bool{
        ++called;if(throwing)throw std::runtime_error("camera-rejected");return false;});
    Need(called==1&&!result.accepted&&!result.poseRejected&&Words(f)==before,"CAMERA_REJECTION_NOT_ATOMIC");
    Need(f.actor.CanCommitDirectDisplacementPreview(token)&&model==f.explorer.model.bindMesh.vertices.data(),"PREVIEW_OR_MODEL_NOT_RESTORED");
    Need(f.Step(Move({0,0,-1})).steps==1&&!f.actor.CanCommitDirectDisplacementPreview(token),"SAME_INPUT_RETRY_NOT_COMMITTED");
    ++callbacks;
}
}
int main()try{
    FirstDirections();UnreadyControls();StoppedReverse();RejectAndRetry(false);RejectAndRetry(true);
    Need(starts==4&&frequencies==12&&callbacks==2,"STARTUP_CONTROL_INVENTORY");
    std::cout<<"FIRST_INPUT_STARTUP_PASS directions=4 frequencies=12 callbacks=2 checks="<<checks<<" fixed_steps="<<steps
        <<" real_Player_Explorer_CC=1 v3_load_covered=0 Main_F5_F9=0 GPU=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"STARTUP_FIRST_DIVERGENCE "<<error.what()<<" checks="<<checks<<'\n';return 2;}
