#include "MistwatchPlayerLocomotion.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Aevrynd;
using namespace Mistwatch;
namespace {
unsigned checks{},steps{};
void Need(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
struct Fixture {
    HeightField3D terrain{{{-30,0,-30},128,128,1}};
    CharacterController3D actor;
    PlayerLocomotion owner;
    Evasion evasion;
    Explorer explorer;
    Fixture(){terrain.Fill(0);actor.Reset({0,0,0});Need(actor.Step({}, {},{},terrain,0)&&actor.Grounded(),"initial true ground");
        explorer.Update({},0,false);Need(explorer.PlaceFeet({},true,false,terrain,{},{}),"initial pose");}
    void Step(PlayerLocomotionInput input={},float dt=LocomotionMotor::FixedStep){
        const auto r=owner.Advance(actor,evasion,explorer,input,dt,{}, {},terrain,[]{return true;});
        if(!r.accepted)std::cerr<<"POSE_REJECT="<<r.poseFailureLine<<'\n';
        Need(r.accepted,"actual controller/pose step");++steps;
    }
};
void Compare(const Fixture& a,const Fixture& b){
    Need(Same(a.actor.Position(),b.actor.Position())&&Same(a.actor.Velocity(),b.actor.Velocity())&&a.actor.Grounded()==b.actor.Grounded(),"uninterrupted/restored physical trajectory exactly equal");
    const auto ao=a.owner.CaptureMotionState(),bo=b.owner.CaptureMotionState();
    const auto am=ao.motor,bm=bo.motor;
    // StateHash 還包含已消耗的輸入方向；載入故意清除輸入邊沿，不能要求其與動作一同復活。
    Need(ao.remainder==bo.remainder&&ao.dodgeFlight==bo.dodgeFlight&&Same(am.velocity,bm.velocity)&&
        am.heading==bm.heading&&am.headingReady==bm.headingReady&&am.phase==bm.phase&&
        am.supportTicks==bm.supportTicks&&am.tick==bm.tick&&am.runFlight==bm.runFlight&&am.wasDashing==bm.wasDashing,
        "owner physical future equals uninterrupted owner");
    Need(Same(a.evasion.direction,b.evasion.direction)&&a.evasion.remaining==b.evasion.remaining&&a.evasion.cooldown==b.evasion.cooldown,"actual evasion clocks preserved");
    Need(a.explorer.yaw==b.explorer.yaw&&a.explorer.phase==b.explorer.phase,"unchanged pose copy advances identically");
    Need(a.explorer.pose.globalMatrices.size()==b.explorer.pose.globalMatrices.size(),"pose size");
    for(std::size_t i=0;i<a.explorer.pose.globalMatrices.size();++i)
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
            Need(a.explorer.pose.globalMatrices[i].values[row][col]==b.explorer.pose.globalMatrices[i].values[row][col],"future actual bone matrix equal");
}
void Roundtrip(bool dodge,unsigned saveAfter){
    Fixture uninterrupted;PlayerLocomotionInput start;
    if(dodge){start.evadePressed=true;start.dodgeDirection={1,0,0};}else start.jumpPressed=true;
    uninterrupted.Step(start);
    for(unsigned i=1;i<saveAfter;++i)uninterrupted.Step();
    Need(!uninterrupted.actor.Grounded(),"checkpoint selected during real flight");
    const auto motion=uninterrupted.actor.CaptureMotionState();
    const auto owner=uninterrupted.owner.CaptureMotionState();
    const auto initialJumps=uninterrupted.actor.Stats().jumps;
    Fixture restored;const auto restoredJumps=restored.actor.Stats().jumps;
    Need(restored.actor.TryRestoreMotionState(motion,{}, {},restored.terrain),"restore bounded real controller motion");
    restored.evasion=uninterrupted.evasion;
    Need(restored.owner.TryRestoreMotionState(owner,restored.actor,restored.evasion),"restore controller-coupled locomotion owner");
    // 本測試只證明物理與 owner 恢復；姿態直接保留同一快照，尚不是磁碟 F5/F9 驗收。
    restored.explorer=uninterrupted.explorer;
    Need(restored.actor.Stats().jumps==restoredJumps,"restore does not synthesize a jump event");
    Compare(uninterrupted,restored);
    for(unsigned i=0;i<150;++i){
        const float dt=i%3==0?.1f:i%3==1?1.f/60.f:LocomotionMotor::FixedStep;
        uninterrupted.Step({},dt);restored.Step({},dt);Compare(uninterrupted,restored);
    }
    Need(uninterrupted.actor.Grounded()&&restored.actor.Grounded(),"both paths actually land");
    Need(uninterrupted.actor.Stats().jumps==initialJumps&&restored.actor.Stats().jumps==restoredJumps,"no extra impulses after restore");
    std::cout<<"ROUNDTRIP dodge="<<dodge<<" after="<<saveAfter<<" saved_vy="<<motion.velocity.y<<" future_batches=150\n";
}
void InvalidOwner(){
    Fixture f;PlayerLocomotionInput q;q.evadePressed=true;q.dodgeDirection={1,0,0};f.Step(q);
    const auto good=f.owner.CaptureMotionState();const auto before=f.owner.StateHash();
    const auto reject=[&](PlayerLocomotionMotionState candidate,Evasion e){
        Need(!f.owner.TryRestoreMotionState(candidate,f.actor,e),"malformed motion rejected");
        Need(f.owner.StateHash()==before,"malformed motion leaves owner unchanged");};
    auto bad=good;bad.remainder=std::numeric_limits<double>::quiet_NaN();reject(bad,f.evasion);
    bad=good;bad.remainder=LocomotionMotor::FixedStep;reject(bad,f.evasion);
    bad=good;bad.motor.velocity.x+=1;reject(bad,f.evasion);
    bad=good;bad.motor.phase=LocomotionPhase::Standing;reject(bad,f.evasion);
    bad=good;bad.motor.phase=static_cast<LocomotionPhase>(99);reject(bad,f.evasion);
    bad=good;bad.dodgeFlight=false;reject(bad,f.evasion);
    bad=good;bad.motor.runFlight=true;reject(bad,f.evasion);
    bad=good;bad.motor.supportTicks=11;reject(bad,f.evasion);
    bad=good;bad.motor.heading=std::numeric_limits<float>::infinity();reject(bad,f.evasion);
    auto badEvasion=f.evasion;badEvasion.cooldown=0;reject(good,badEvasion);
    badEvasion=f.evasion;badEvasion.direction={.5f,0,0};reject(good,badEvasion);
    // 載入清除尚未執行的按鍵，不將儲存前的按鍵當成新輸入再次執行。
    f.Step(q);Need(f.owner.PendingEvadeTicks()>0,"actual buffered airborne Q exists");
    const auto queued=f.owner.CaptureMotionState();
    Need(f.owner.TryRestoreMotionState(queued,f.actor,f.evasion)&&f.owner.PendingEvadeTicks()==0,"load clears input edge but preserves motion");
}
}
int main()try{
    for(unsigned at:{1u,9u,18u,28u,35u})Roundtrip(true,at);
    for(unsigned at:{1u,20u,42u,65u})Roundtrip(false,at);
    InvalidOwner();
    std::cout<<"MOTION_CHECKPOINT_PASS checks="<<checks<<" steps="<<steps<<" disk_and_main_integrated=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
