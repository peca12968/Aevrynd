#include "MistwatchPlayerLocomotion.h"
#include <iostream>
#include <stdexcept>
using namespace Aevrynd;using namespace Mistwatch;
unsigned checks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
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
        // 目前及未來採樣一起核對，避免只比較腳點而漏掉剩餘曲線的改變。
        for(unsigned j=0;j<5;++j){auto copy=s;FootSwingSample3D sample;
            const bool ok=copy.Advance((std::max)(0.,s.Duration()-s.Elapsed())*j/4.,sample);add(ok);
            if(ok){vec(sample.positionWorld);vec(sample.velocityWorld);add(sample.planted);}}
    }
    return h;
}
std::uint64_t EvasionHash(const Evasion& e){
    std::uint64_t h=0;for(float f:{e.direction.x,e.direction.y,e.direction.z,e.remaining,e.cooldown})h=(h^std::bit_cast<std::uint32_t>(f))*1099511628211ull;return h;
}
struct Fixture {
    HeightField3D terrain{{{-30,0,-30},128,128,1}};
    CharacterController3D actor;Evasion evasion;Explorer explorer;PlayerLocomotion owner;
    Fixture(){terrain.Fill(0);actor.Reset({0,0,0});
        Need(actor.Step({}, {},{},terrain,LocomotionMotor::FixedStep)&&actor.Grounded(),"settle actual controller");
        explorer.Update({},0,false);Need(explorer.PlaceFeet(actor.Position(),true,false,terrain,{},{}),"initialize actual pose");}
    PlayerLocomotionResult Step(PlayerLocomotionInput input={},float dt=LocomotionMotor::FixedStep){
        return owner.Advance(actor,evasion,explorer,input,dt,{}, {},terrain,[]{return true;});}
};
int main()try{
 unsigned cases=0;
 for(float degrees:{-15.01f,-15.f,-14.99f,14.99f,15.f,15.01f,-90.01f,-89.99f,89.99f,90.01f})
 for(unsigned phase:{0u,7u})for(bool running:{false,true}){
  Fixture f;PlayerLocomotionInput move;move.movement={0,0,1};move.running=running;move.speedMultiplier=running?1.5f:1.f;
  for(unsigned t=0;t<180+phase;++t)Need(f.Step(move).accepted,"initial movement");
  const float angle=degrees*.017453292519943295f;move.movement={std::sin(angle),0,std::cos(angle)};
  float peak=0;const auto start=f.actor.Position();
  for(unsigned t=0;t<360;++t){const auto r=f.Step(move);
   if(!r.accepted)std::cerr<<"degrees="<<degrees<<" phase="<<phase<<" running="<<running<<" tick="<<t<<" line="<<r.poseFailureLine<<'\n';
   Need(r.accepted,"boundary turn movement");
   peak=std::max(peak,Length(f.owner.Motor().Velocity()));
   for(const auto& swing:f.explorer.footSwings)Need(swing.SpeedBound()<=60,"foot speed unchanged");
  }
  Need(Length(f.actor.Position()-start)>3,"turn does not stall");
  if(running)Need(peak>6.1f,"boundary turn recovers run speed");
  for(unsigned t=0;t<360;++t)Need(f.Step().accepted,"boundary deceleration and settle");
  Need(Length(f.owner.Motor().Velocity())==0&&!f.explorer.footSwings[0].Active()&&!f.explorer.footSwings[1].Active(),"boundary settles");
  ++cases;
 }
 for(float sign:{-1.f,1.f}){
  Fixture f;
  for(unsigned s=0;s<2;++s){const Vec3 p{s?.12f:-.12f,.1f,-.6f};
   Need(f.explorer.footSwings[s].Reset(p),"negative support fixture");f.explorer.footTargets[s]=p;
  }
  f.explorer.Update({sign*.008f,0,0},LocomotionMotor::FixedStep,false);
  const auto before=PoseHash(f.explorer);unsigned line=0;
  Need(!f.explorer.PlaceFeet({},true,false,f.terrain,{},{},&line),"infeasible support rejects");
  // 不依賴實作行號；完整拒絕及逐欄位狀態回滾是持續維護的行為契約。
  Need(PoseHash(f.explorer)==before,"infeasible support full pose rollback");
  std::cout<<"SUPPORT_REJECT sign="<<sign<<" line="<<line<<'\n';
 }
 FootSwingTrajectory3D speed;Need(speed.Reset({0,.1f,0}),"speed fixture");
 Need(!speed.Begin({2,.1f,0},.05,.14,60),"infeasible speed rejects");
 Need(!speed.Active()&&speed.Duration()==0&&speed.Landing().x==0,"speed rejection unchanged");
 std::cout<<"TURN_BOUNDARY_PASS cases="<<cases<<" checks="<<checks<<" support_rollback=2 speed_reject=1\n";return 0;
}catch(const std::exception& e){std::cerr<<"TURN_BOUNDARY_FAIL "<<e.what()<<'\n';return 1;}
