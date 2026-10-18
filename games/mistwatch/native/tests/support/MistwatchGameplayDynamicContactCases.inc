#include <MistwatchPlayerLocomotion.h>
#include "MistwatchDynamicContactStateHashes.h"
#include <MistwatchEncounter.h>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace Aevrynd;using namespace Mistwatch;
const std::span<const BoxCollider3D> EmptyBoxes{};
const std::span<const ConvexAabbSweep3D* const> EmptyHulls{};
unsigned checks{},approaches{},atomicCases{},retryEdges{},otherCases{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
std::array<std::uint64_t,12> Whole(const CharacterController3D& c,const PlayerLocomotion& p,const Evasion& e,const Explorer& pose){
    const auto s=c.Stats();return {c.StateHash(),p.StateHash(),p.Motor().StateHash(),EvasionHash(e),PoseHash(pose),
        s.steps,s.collisions,s.jumps,s.landings,s.terrainContacts,c.DirectLastTick(),c.MemoryStats().retainedCapacityBytes};
}
struct Fixture {
    HeightField3D terrain{{{-30,0,-30},128,128,1}};
    CharacterController3D actor;Evasion evasion;Explorer explorer;PlayerLocomotion owner;
    explicit Fixture(Vec3 feet={},float yaw=0,bool crouched=false,CharacterControllerSettings3D settings={}):actor(settings){
        terrain.Fill(0);actor.Reset(feet);
        Need(actor.TrySetHeight(crouched?1.4f:1.8f,EmptyBoxes),"fixture actual height");
        Need(actor.Step({},EmptyBoxes,EmptyHulls,terrain,0)&&actor.Grounded(),"fixture actual grounded controller");
        explorer.yaw=yaw;explorer.requestedYaw=yaw;explorer.Update({},0,crouched);
        Need(explorer.PlaceFeet(actor.Position(),true,crouched,terrain,EmptyBoxes,EmptyHulls),"fixture actual initial pose");
    }
    PlayerLocomotionResult Old(PlayerLocomotionInput input={},float dt=LocomotionMotor::FixedStep,std::span<const BoxCollider3D> boxes={}){
        return owner.Advance(actor,evasion,explorer,input,dt,boxes,EmptyHulls,terrain,[]{return true;});
    }
    void Air(Vec3 feet,Vec3 velocity={}){
        Need(actor.TryRestoreMotionState({feet,velocity,actor.Settings().height,0,false},EmptyBoxes,EmptyHulls,terrain),"restore actual air controller");
        LocomotionMotorState motor;motor.velocity={velocity.x,0,velocity.z};motor.phase=LocomotionPhase::JumpOrFall;
        if(std::hypot(velocity.x,velocity.z)>0){motor.heading=std::atan2(velocity.x,velocity.z);motor.headingReady=true;}
        Need(owner.TryRestoreMotionState({motor,0,false},actor,evasion),"restore matching actual air motor");
        explorer.ResetLocomotion();explorer.Update({},0,false);
        Need(explorer.PlaceFeet(feet,false,false,terrain,EmptyBoxes,EmptyHulls),"initialize actual air pose");
    }
#if CANDIDATE
    PlayerLocomotionDynamicResult Step(PlayerLocomotionInput input,std::span<const DynamicCylinder3D> bodies,
        float dt=LocomotionMotor::FixedStep,std::span<const BoxCollider3D> boxes={}){
        return owner.Advance(actor,evasion,explorer,input,dt,boxes,EmptyHulls,terrain,[]{return true;},bodies);
    }
#endif
};
unsigned Parity(){
    std::ofstream out("parity.tsv",std::ios::binary);Need(bool(out),"open parity");unsigned samples{};
    for(unsigned recipe=0;recipe<12;++recipe){
        const float angle=std::remainder(float(recipe%8)*.7853981634f,6.2831853f);
        Fixture f({-3,0,-3},angle);
        for(unsigned tick=0;tick<120;++tick){
            PlayerLocomotionInput input;input.movement={std::sin(angle),0,std::cos(angle)};
            if(tick>=70)input.movement={};input.speedMultiplier=recipe%2?.45f:1.f;
            input.crouched=recipe%3==1;input.jumpPressed=recipe%3==2&&tick==18;
            input.evadePressed=recipe%3==0&&tick==25;input.dodgeDirection=input.movement;
            PlayerLocomotionResult result;
#if CANDIDATE
            const auto dynamic=f.Step(input,{});Need(!dynamic.contact,"empty span must not claim dynamic query");result=dynamic.motion;
#else
            result=f.Old(input);
#endif
            Need(result.accepted,"existing empty-path actual gameplay pose");
            out<<recipe<<'\t'<<tick<<'\t'<<result.accepted<<'\t'<<result.dashing<<'\t'<<result.lowCeiling<<'\t'<<result.steps;
            for(auto value:Whole(f.actor,f.owner,f.evasion,f.explorer))out<<'\t'<<value;
            out<<'\t'<<f.owner.PendingEvadeTicks()<<'\t'<<std::bit_cast<std::uint64_t>(f.owner.Remainder())<<'\n';++samples;
        }
    }
    Need(bool(out),"write parity");return samples;
}
#if CANDIDATE
using S=CharacterDynamicStepStatus3D;
const std::array<DynamicCylinder3D,1> BoarBody{{{{0,.5f,0},1.3f,.8f}}};
void Clear(const Fixture& f,std::span<const DynamicCylinder3D> bodies){
    for(const auto& body:bodies){BoxCylinderContact3D q;
        Need(QueryBoxCylinderContact3D(f.actor.Center(),f.actor.Extents(),{},body,q)&&q.kind==BoxCylinderContactKind3D::Clear,"actual gameplay final overlap");}
}
void Approaches(){
    for(unsigned kind=0;kind<3;++kind)for(unsigned index=0;index<8;++index){
        const float angle=std::remainder(float(index)*.7853981634f,6.2831853f);const Vec3 direction{std::sin(angle),0,std::cos(angle)};
        CharacterControllerSettings3D settings;if(kind==2)settings.gravity=0;
        Fixture f(direction*-3.5f,angle,kind==1,settings);
        if(kind==2)f.Air(direction*-3.5f+Vec3{0,.5f,0});
        const auto initialLandings=f.actor.Stats().landings;PlayerLocomotionInput input;input.movement=direction;input.crouched=kind==1;
        for(unsigned tick=0;tick<400;++tick){
            const auto result=f.Step(input,BoarBody);
            if(!result.motion.accepted)std::cerr<<"APPROACH kind="<<kind<<" direction="<<index<<" tick="<<tick
                <<" contact="<<(result.contact?int(*result.contact):-1)<<" poseRejected="<<result.motion.poseRejected<<" poseLine="<<result.motion.poseFailureLine<<'\n';
            Need(result.motion.accepted&&result.contact==S::Applied,"eight-direction gameplay approach rejected");
            Clear(f,BoarBody);
            if(kind==2)Need(!f.actor.Grounded()&&f.actor.Position().y==.5f&&f.actor.Stats().landings==initialLandings,"NPC granted airborne support");
        }
        const float distance=std::hypot(f.actor.Position().x,f.actor.Position().z);
        Need(distance>=1.74f&&distance<2.2f,"safe stop must enter unchanged 2.2m charge circle");
        Encounter encounter;encounter.feet={};Need(encounter.Tick(f.actor.Position(),LocomotionMotor::FixedStep,false)==0&&
            encounter.phase==EnemyPhase::Windup,"unchanged real Encounter charge threshold inaccessible");
        if(kind==2)Need(!f.actor.TryLaunchFromGround(.5f),"NPC side cannot authorize launch");
        std::cout<<"APPROACH_PASS kind="<<kind<<" direction="<<index<<" distance="<<distance<<'\n';++approaches;
    }
}
template<class Action> void Atomic(Fixture& f,Action&& action,S status,const char* reason){
    CharacterDirectDisplacementPreviewToken3D token;
    Need(f.actor.PreviewDirectDisplacementTick({f.actor.DirectLastTick()+1,{0,0,0},0},EmptyBoxes,token),"actual pending preview setup");
    const auto before=Whole(f.actor,f.owner,f.evasion,f.explorer);const auto* vertices=f.explorer.model.bindMesh.vertices.data();
    const auto* joints=f.explorer.model.skeleton.joints.data();const auto result=action();
    if(result.motion.accepted||result.contact!=status)std::cerr<<reason<<" actual="<<(result.contact?int(*result.contact):-1)
        <<" accepted="<<result.motion.accepted<<" pose="<<result.motion.poseRejected<<'\n';
    Need(!result.motion.accepted&&result.contact==status,reason);
    Need(Whole(f.actor,f.owner,f.evasion,f.explorer)==before&&f.actor.CanCommitDirectDisplacementPreview(token)&&
        f.explorer.model.bindMesh.vertices.data()==vertices&&f.explorer.model.skeleton.joints.data()==joints,
        "controller motor evasion pose queue or actual preview not atomically restored");++atomicCases;
}
void Edges(){
    for(bool queued:{false,true})for(bool jump:{false,true}){
        Fixture f;PlayerLocomotionInput edge;edge.jumpPressed=jump;edge.evadePressed=!jump;edge.dodgeDirection={1,0,0};
        if(queued)Need(f.Step(edge,{},0).motion.accepted,"queue original input before rejection");
        const auto beforeJumps=f.actor.Stats().jumps;
        Atomic(f,[&]{return f.Step(queued?PlayerLocomotionInput{}:edge,BoarBody);},S::UnresolvedStartedOverlap,"new or queued edge must reject explicit overlap");
        const auto retry=f.Step(queued?PlayerLocomotionInput{}:edge,{});
        Need(retry.motion.accepted&&f.actor.Stats().jumps==beforeJumps+1&&retry.motion.dashing==!jump,"same original Q or Space retries once");
        const auto acceptedJumps=f.actor.Stats().jumps;
        for(unsigned tick=0;tick<120;++tick)Need(f.Step({},{}).motion.accepted,"post retry actual locomotion");
        Need(f.actor.Stats().jumps==acceptedJumps&&f.evasion.remaining==0,"rejected edge must not duplicate on later ticks");++retryEdges;
    }
}
void Boundaries(){
    {
        Fixture f({-3,0,0},0,true);const std::array<BoxCollider3D,1> roof{{{{-3,1.6f,0},{1,.1f,1}}}};
        const auto result=f.Step({},BoarBody,LocomotionMotor::FixedStep,roof);
        Need(result.motion.accepted&&result.motion.lowCeiling&&f.actor.Settings().height==1.4f&&f.actor.Grounded(),"actual low roof crouch-to-stand must stay crouched");++otherCases;
    }
    {
        Fixture f({},0,true);const std::array<DynamicCylinder3D,1> overhead{{{{0,2,0},1.3f,.5f}}};
        Atomic(f,[&]{return f.Step({},overhead);},S::UnresolvedStartedOverlap,"standing into NPC must roll back height and pose");
        Need(f.actor.Settings().height==1.4f,"standing rollback must retain crouch height");
        Need(f.Step({},{}).motion.accepted&&f.actor.Settings().height==1.8f,"same stand input can retry without NPC");++otherCases;
    }
    {
        Fixture f({-1.8f,0,0},1.5707963268f);PlayerLocomotionInput q;q.evadePressed=true;q.dodgeDirection={1,0,0};
        const auto before=f.actor.Stats().jumps;const auto result=f.Step(q,BoarBody);
        Need(result.motion.accepted&&result.motion.dashing&&!f.actor.Grounded()&&f.actor.Stats().jumps==before+1,
            "real Q launch and side contact must not become NPC support");Clear(f,BoarBody);++otherCases;
    }
    {
        Fixture f;f.Air({0,1.31f,0},{0,-1,0});
        Atomic(f,[&]{return f.Step({},BoarBody,2*LocomotionMotor::FixedStep);},S::UnresolvedVerticalEntry,
            "later substep vertical must roll back entire outer frame");++otherCases;
    }
    {
        CharacterControllerSettings3D settings;settings.radius=.5f;settings.moveSpeed=12;settings.gravity=0;settings.maximumFallSpeed=16;
        Fixture f({},1.5707963268f,false,settings);f.Air({-1.03125f,1.03125f,0},{12,-12,0});
        const std::array<DynamicCylinder3D,1> body{{{{0,.5f,0},.5f,.5f}}};PlayerLocomotionInput movement;movement.movement={1,0,0};
        Atomic(f,[&]{return f.Step(movement,body);},S::UnresolvedSimultaneousEntry,"simultaneous must propagate concrete nonfatal status");++otherCases;
    }
    {
        Fixture f({-3,0,0},1.5707963268f);PlayerLocomotionInput q;q.evadePressed=true;q.dodgeDirection={1,0,0};
        CharacterDirectDisplacementPreviewToken3D token;Need(f.actor.PreviewDirectDisplacementTick({1,{},0},EmptyBoxes,token),"callback preview");
        const auto before=Whole(f.actor,f.owner,f.evasion,f.explorer);bool called{};
        const auto result=f.owner.Advance(f.actor,f.evasion,f.explorer,q,LocomotionMotor::FixedStep,EmptyBoxes,EmptyHulls,f.terrain,
            [&]{called=true;return false;},BoarBody);
        Need(called&&!result.motion.accepted&&result.contact==S::Applied&&Whole(f.actor,f.owner,f.evasion,f.explorer)==before&&
            f.actor.CanCommitDirectDisplacementPreview(token),"Applied contact diagnostic must not imply committed outer transaction");
        const auto retry=f.Step(q,BoarBody);Need(retry.motion.accepted&&retry.motion.dashing,"callback rollback original Q retry");++otherCases;
    }
}
#endif
}
int main(){try{
    const auto parity=Parity();
#if CANDIDATE
    Approaches();Edges();Boundaries();
    std::cout<<"GAMEPLAY_DYNAMIC_CONTACT_CANDIDATE_CPU_PASS parity="<<parity<<" approaches="<<approaches
        <<" atomic="<<atomicCases<<" retry_edges="<<retryEdges<<" other="<<otherCases
        <<" checks="<<checks<<" main=0 moving_body=0 pair=0 GPU=0\n";
#else
    std::cout<<"ORIGINAL_F_GAMEPLAY_EMPTY_PARITY snapshots="<<parity<<'\n';
#endif
    return 0;
}catch(const std::exception& e){std::cerr<<"GAMEPLAY_FIRST_DIVERGENCE "<<e.what()<<" checks="<<checks<<'\n';return 1;}}
