#include "MistwatchPlayerLocomotion.h"
#include "MistwatchWestCliffRelief.h"
#include <array>
#include <iostream>
#include <iomanip>
#include <stdexcept>
using namespace Aevrynd;
using namespace Mistwatch;
namespace {
unsigned checks{},ticks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
HeightField3D Terrain(float grade=0){
    HeightField3D t({{-16,0,-16},65,65,.5f});
    for(unsigned z=0;z<65;++z)for(unsigned x=0;x<65;++x)t.SetHeight(x,z,grade*(-16.f+.5f*float(x)));
    t.RecalculateRange();return t;
}
CharacterControllerSettings3D Settings(bool policy){
    CharacterControllerSettings3D s;s.radius=.1f;s.maximumTerrainStandingGrade=policy?.1f:0.f;return s;
}
std::uint64_t PoseHash(const Explorer& e){
    std::uint64_t h=1469598103934665603ull;
    const auto add=[&](std::uint64_t n){h=(h^n)*1099511628211ull;};
    const auto scalar=[&](float f){add(std::bit_cast<std::uint32_t>(f));};
    const auto vector=[&](Vec3 v){scalar(v.x);scalar(v.y);scalar(v.z);};
    scalar(e.phase);scalar(e.yaw);scalar(e.requestedYaw);vector(e.lastFeet);add(e.feetInitialized);
    scalar(e.locomotionDistance);scalar(e.locomotionSeconds);vector(e.locomotionVelocity);add(e.gaitGrounded);add(e.nextFoot);
    for(const auto& t:e.pose.localTransforms){vector(t.translation);vector(t.rotation);vector(t.scale);
        scalar(t.quaternion.x);scalar(t.quaternion.y);scalar(t.quaternion.z);scalar(t.quaternion.w);add(t.usesQuaternion);}
    for(const auto& m:e.pose.globalMatrices)for(const auto& row:m.values)for(float f:row)scalar(f);
    for(unsigned side=0;side<2;++side){
        vector(e.footTargets[side]);add(e.planted[side]);scalar(e.footFacing[side]);scalar(e.swingFacingStart[side]);scalar(e.swingFacingEnd[side]);
        const auto& p=e.footSwings[side];add(p.Ready());add(p.Active());add(p.Replanned());
        for(double v:{p.Duration(),p.Elapsed(),p.LiftHeight(),p.SpeedBound()})add(std::bit_cast<std::uint64_t>(v));
        for(unsigned i=0;i<5;++i){auto copy=p;FootSwingSample3D sample;const bool ok=copy.Advance(std::max(0.,p.Duration()-p.Elapsed())*i/4.,sample);
            add(ok);if(ok){vector(sample.positionWorld);vector(sample.velocityWorld);add(sample.planted);}}
    }
    return h;
}
std::uint64_t EvasionHash(const Evasion& e){
    std::uint64_t h=0;for(float v:{e.direction.x,e.direction.y,e.direction.z,e.remaining,e.cooldown})h=(h^std::bit_cast<std::uint32_t>(v))*1099511628211ull;return h;
}
void SameMotion(const CharacterController3D& a,const CharacterController3D& b){
    const auto x=a.CaptureMotionState(),y=b.CaptureMotionState();
    const bool same=x.feet.x==y.feet.x&&x.feet.y==y.feet.y&&x.feet.z==y.feet.z&&
        x.velocity.x==y.velocity.x&&x.velocity.y==y.velocity.y&&x.velocity.z==y.velocity.z&&
        x.grounded==y.grounded&&x.height==y.height&&x.yaw==y.yaw;
    if(!same)std::cerr<<std::setprecision(10)<<"MOTION_DIFFERENCE a_feet="<<x.feet.x<<','<<x.feet.y<<','<<x.feet.z
        <<" b_feet="<<y.feet.x<<','<<y.feet.y<<','<<y.feet.z<<" a_velocity="<<x.velocity.x<<','<<x.velocity.y<<','<<x.velocity.z
        <<" b_velocity="<<y.velocity.x<<','<<y.velocity.y<<','<<y.velocity.z<<" grounded="<<x.grounded<<','<<y.grounded<<'\n';
    Need(same,"motion path changed or integrated twice");
}
struct Fixture {
    HeightField3D terrain;std::vector<BoxCollider3D> boxes;
    CharacterController3D actor;PlayerLocomotion owner;Explorer explorer;Evasion evasion;
    Fixture(bool bridge=false,bool policy=true):terrain(Terrain(bridge?.2f:0)),actor(Settings(policy)){
        if(bridge)boxes.push_back({{0,.9f,0},{4,.1f,8}});
        actor.Reset({0,bridge?1.f:0,0});
        Need(actor.Step({},boxes,{},terrain,LocomotionMotor::FixedStep)&&actor.Grounded(),"actual initial support");
        explorer.Update({},0,false);Need(explorer.PlaceFeet(actor.Position(),true,false,terrain,boxes,{}),"initial pose");
    }
    PlayerLocomotionResult Step(PlayerLocomotionInput input={},std::span<const ConvexAabbSweep3D* const> hulls={}){
        ++ticks;return owner.Advance(actor,evasion,explorer,input,LocomotionMotor::FixedStep,boxes,hulls,terrain,[]{return true;});
    }
};
PlayerLocomotionInput Dodge(){PlayerLocomotionInput i;i.evadePressed=true;i.dodgeDirection={0,0,1};return i;}
void ValidDash(){
    for(bool bridge:{false,true})for(bool policy:{false,true}){
        Fixture live(bridge,policy);
        auto reference=live.actor;
        const auto jumps=live.actor.Stats().jumps;const auto before=live.actor.Position();
        const auto steps=live.actor.Stats().steps,motorTicks=live.owner.Motor().Tick();
        const float launch=.5f*live.actor.Settings().gravity*(Evasion::Duration+2*LocomotionMotor::FixedStep);
        // 同政策的獨立控制器只接一次脈衝及一次 Step；不以不同子步數的新舊解算互比位元。
        Need(policy?reference.TryLaunchFromGround(launch,live.boxes,{},live.terrain):reference.TryLaunchFromGround(launch),"reference launch once");
        Need(reference.Step({{0,0,1},false,Evasion::SpeedMultiplier},live.boxes,{},live.terrain,LocomotionMotor::FixedStep),"reference integration once");
        const auto first=live.Step(Dodge());
        Need(first.accepted&&first.steps==1&&live.actor.Stats().steps==steps+1&&live.owner.Motor().Tick()==motorTicks+1,"supported dodge advances exactly one owner and physics tick");
        Need(live.evasion.remaining>0&&!live.actor.Grounded()&&live.actor.Velocity().y>0&&live.actor.Stats().jumps==jumps+1,
            "supported dodge commits exactly one launch");
        SameMotion(live.actor,reference);
        // 此固定 fixture 的舊分軸／新凸形解算分別為 6／8 子步；解析半隱式重力不能多積分一輪。
        const double n=policy?8.:6.,h=LocomotionMotor::FixedStep;
        const double height=launch*h-live.actor.Settings().gravity*h*h*(n+1)/(2*n);
        Need(std::abs(live.actor.Velocity().y-(launch-live.actor.Settings().gravity*LocomotionMotor::FixedStep))<.00001f&&
            std::abs(live.actor.Position().z-before.z-.1f)<.00001f&&
            std::abs(double(live.actor.Position().y-before.y)-height)<.00001,"first dodge tick integrates gravity and distance once");
        // 還原正式空中閃避快照；後續不重播起跳或補一個新的脈衝。
        auto resumed=live;resumed.owner.Reset();
        Need(resumed.owner.TryRestoreMotionState(live.owner.CaptureMotionState(),resumed.actor,resumed.evasion),"airborne dash restore");
        for(unsigned tick=0;tick<24;++tick){
            Need(reference.Step({{0,0,1},false,Evasion::SpeedMultiplier},live.boxes,{},live.terrain,LocomotionMotor::FixedStep),"reference ongoing integration once");
            Need(live.Step().accepted&&resumed.Step().accepted,"supported ongoing airborne dash");
            SameMotion(live.actor,reference);SameMotion(live.actor,resumed.actor);
            Need(live.actor.Stats().jumps==jumps+1&&resumed.actor.Stats().jumps==jumps+1,"airborne dash does not relaunch");
            Need(live.actor.Stats().steps==steps+tick+2&&live.owner.Motor().Tick()==motorTicks+tick+2,"ongoing dash has no hidden integration");
            Need(EvasionHash(live.evasion)==EvasionHash(resumed.evasion),"restored dash preserves duration and cooldown");
            Need(PoseHash(live.explorer)==PoseHash(resumed.explorer),"restored dash preserves full pose");
        }
    }
}
void SteepStaleSupport(){
    Fixture f;f.terrain=Terrain(.2f);auto baseline=f;
    Need(f.actor.Grounded(),"fixture keeps previous flat grounded state before real reclassification");
    const auto before=f.actor.Position();
    for(unsigned tick=0;tick<20;++tick){
        const auto r=f.Step(tick==0?Dodge():PlayerLocomotionInput{}),b=baseline.Step();
        if(!r.accepted)std::cerr<<"STEEP_OWNER_DIAGNOSTIC tick="<<tick<<" pose_line="<<r.poseFailureLine<<'\n';
        Need(r.accepted&&b.accepted,"valid steep surface must advance rather than rollback waiting");
        Need(!r.dashing&&!f.actor.Grounded()&&f.actor.Stats().jumps==0&&f.evasion.remaining==0&&f.evasion.cooldown==0,
            "stale grounded must not start evasion or cooldown");
        SameMotion(f.actor,baseline.actor);
    }
    Need(f.actor.Position().x<before.x&&f.actor.Position().y<before.y&&f.owner.PendingEvadeTicks()==0,
        "declined steep dodge still progresses physically and expires queued edge");
}
void Atomic(){
    Fixture f;const auto a=f.actor.StateHash(),o=f.owner.StateHash(),p=PoseHash(f.explorer),e=EvasionHash(f.evasion);
    const std::array<const ConvexAabbSweep3D*,1> bad{nullptr};
    Need(!f.Step(Dodge(),bad).accepted,"invalid launch geometry rejects owner step");
    Need(a==f.actor.StateHash()&&o==f.owner.StateHash()&&p==PoseHash(f.explorer)&&e==EvasionHash(f.evasion),
        "invalid geometry preserves controller owner evasion and complete pose state");
    Need(f.Step(Dodge()).accepted&&f.actor.Stats().jumps==1,"valid retry after atomic rejection works");
    const auto count=f.actor.Stats().jumps;
    Need(f.Step(Dodge()).accepted&&f.actor.Stats().jumps==count,"repeated airborne request does not double launch");
    Fixture cooling;cooling.evasion.direction={0,0,1};cooling.evasion.cooldown=.5f;
    Need(cooling.Step(Dodge()).accepted&&cooling.actor.Grounded()&&cooling.actor.Stats().jumps==0&&cooling.evasion.remaining==0,
        "cooldown does not commit an unused launch candidate");
}
void WestTurnSupport(){
    const auto derived=WestCliffRelief::Build(CreateTerrain());
    const auto& terrain=derived.terrain;
    for(const auto point:{Vec2{-44.0856f,55.4638f},Vec2{-44,58},Vec2{-47,58}}){
    CharacterControllerSettings3D settings;settings.maximumTerrainStandingGrade=1;settings.maximumTerrainUphillGrade=1;
    CharacterController3D actor(settings);PlayerLocomotion owner;Explorer explorer;Evasion evasion;
    actor.Reset({point.x,terrain.SampleHeight(point.x,point.y),point.y});
    explorer.Update({},0,false);
    Need(actor.Step({}, {},{},terrain,LocomotionMotor::FixedStep)&&explorer.PlaceFeet(actor.Position(),actor.Grounded(),false,terrain,{},{}),"west turn actual setup");
    const auto start=actor.Position();
    for(unsigned stage=0;stage<4;++stage){
        PlayerLocomotionInput input;input.movement={0,0,stage==0||stage==3?-1.f:stage==1?0.f:1.f};
        for(unsigned tick=0;tick<120;++tick){
            const auto priorTargets=explorer.footTargets;const auto priorPlanted=explorer.planted;
            const bool priorGrounded=actor.Grounded();
            ++ticks;const auto result=owner.Advance(actor,evasion,explorer,input,LocomotionMotor::FixedStep,{}, {},terrain,[]{return true;});
            if(!result.accepted)std::cerr<<"WEST_TURN_FAILURE x="<<point.x<<" z="<<point.y<<" stage="<<stage<<" tick="<<tick<<" line="<<result.poseFailureLine<<'\n';
            Need(result.accepted,"west turn chooses viable three-dimensional support without rejecting or freezing");
            Need(actor.Stats().jumps==0,"slope recovery never invents a jump impulse");
            Need(!(explorer.footSwings[0].Active()&&explorer.footSwings[1].Active()),"only one foot swings at a time");
            const auto world=CreateTransformMatrix(actor.Position(),{0,explorer.yaw,0},{1,1,1});
            const float blend=std::clamp(Length(explorer.locomotionVelocity)/2.f,0.f,1.f);
            if(actor.Grounded())Need(explorer.pose.localTransforms[0].translation.y>=.9f-.09f*blend*blend*(3-2*blend)-.35f-.000001f,
                "pelvis lowering gate unchanged during real braking");
            for(unsigned side=0;side<2;++side){
                Need(explorer.footSwings[side].SpeedBound()<=60,"west turn keeps original foot speed gate");
                const unsigned upper=9+3*side;
                const auto hip=TransformPoint(world,TransformPoint(explorer.pose.globalMatrices[upper],{}));
                const auto knee=TransformPoint(world,TransformPoint(explorer.pose.globalMatrices[upper+1],{}));
                const auto ankle=TransformPoint(world,TransformPoint(explorer.pose.globalMatrices[upper+2],{}));
                Need(std::abs(Length(knee-hip)-.4f)<.0001f&&std::abs(Length(ankle-knee)-.38f)<.0001f,"actual leg bones do not stretch");
                Need(Length(ankle-explorer.footTargets[side])<.005f,"actual ankle reaches current target");
                if(priorGrounded&&actor.Grounded()){
                    Need(Length(explorer.footTargets[side]-priorTargets[side])<=60*LocomotionMotor::FixedStep+.0001f,"continuous grounded foot speed remains bounded");
                    if(priorPlanted[side]&&explorer.planted[side])Need(Length(explorer.footTargets[side]-priorTargets[side])==0,
                        "consecutive planted contact remains world locked");
                }
            }
        }
    }
    Need(Length(actor.Position()-start)>7.f,"west turn makes actual progress");
    }
}
void EndpointTransfer(){
    constexpr double h=LocomotionMotor::FixedStep;
    for(unsigned side=0;side<2;++side)for(double remaining:{h*.5,h,h*1.5})for(bool sameFoot:{false,true})for(bool zeroTime:{false,true}){
        Fixture f;auto& e=f.explorer;
        const auto landing=e.footTargets[side];
        Need(e.footSwings[side].Begin(landing,.05,.04,60),"endpoint fixture starts real C2 swing");
        FootSwingSample3D before;
        Need(e.footSwings[side].Advance(.05-remaining,before),"endpoint fixture advances before handoff");
        e.footTargets[side]=before.positionWorld;e.planted[side]=false;e.nextFoot=sameFoot?side:1-side;
        auto projected=e.footSwings[side];FootSwingSample3D end;
        Need(projected.Advance(zeroTime?0:h,end),"independent endpoint prediction");
        e.Update({0,0,.001f},zeroTime?0:LocomotionMotor::FixedStep,false);
        Need(e.PlaceFeet(f.actor.Position(),true,false,f.terrain,f.boxes),"endpoint handoff placement");
        if(zeroTime||projected.Active()){
            Need(e.footSwings[side].Elapsed()==projected.Elapsed()&&!e.footSwings[1-side].Active(),"unfinished or zero-time swing never releases support");
            Need(Length(e.footTargets[side]-end.positionWorld)==0,"ongoing swing advances exactly once");
        }else{
            const unsigned next=sameFoot?side:1-side;
            Need(e.footSwings[next].Active()&&e.footSwings[next].Elapsed()==0,"completed endpoint permits exactly zero-time new swing");
            Need(Length(e.footTargets[side]-landing)==0,"old swing lands without endpoint teleport");
            Need(e.planted[1-next],"handoff retains opposite planted support");
        }
    }
    for(unsigned side=0;side<2;++side){
        Fixture f;auto& e=f.explorer;const auto landing=e.footTargets[side];
        Need(e.footSwings[side].Begin(landing,.05,.04,60),"removed support fixture swing");
        FootSwingSample3D before;Need(e.footSwings[side].Advance(.05-h*.5,before),"removed support nearly landed");
        e.footTargets[side]=before.positionWorld;e.planted[side]=false;e.nextFoot=side;
        for(unsigned z=0;z<65;++z)for(unsigned x=0;x<65;++x)f.terrain.SetHeight(x,z,-1);
        f.terrain.RecalculateRange();
        const float x=side?.12f:-.12f;
        // 另一腳及新落點仍有支撐，只有舊端點的支撐被移除。
        f.boxes={{{-x,-.1f,0},{.05f,.1f,.08f}},{{x,-.1f,.4f},{.05f,.1f,.08f}}};
        e.Update({0,0,.001f},LocomotionMotor::FixedStep,false);
        const auto pose=PoseHash(e);
        Need(!e.PlaceFeet(f.actor.Position(),true,false,f.terrain,f.boxes),"same-foot restart cannot skip missing old landing support");
        Need(PoseHash(e)==pose,"endpoint rejection preserves entire pose and trajectory state");
    }
}
}
int main()try{
    ValidDash();SteepStaleSupport();Atomic();EndpointTransfer();WestTurnSupport();
    std::cout<<"STEEP_SLIDE_OWNER_PASS ticks="<<ticks<<" checks="<<checks<<" native_play=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"STEEP_SLIDE_OWNER_FAIL "<<e.what()<<'\n';return 1;}
