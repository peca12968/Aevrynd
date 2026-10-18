#include "MistwatchPlayerLocomotion.h"
#include "MistwatchWestCliffRelief.h"
#include <iostream>
#include <stdexcept>
using namespace Aevrynd;
using namespace Mistwatch;
namespace {
unsigned checks{},steps{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
void Route(const HeightField3D& terrain,float x){
    CharacterControllerSettings3D settings;settings.maximumTerrainUphillGrade=1;
    CharacterController3D body(settings);PlayerLocomotion owner;Explorer e;Evasion evasion;
    body.Reset({x,terrain.SampleHeight(x,58),58});
    Need(body.Step({}, {},{},terrain,LocomotionMotor::FixedStep)&&body.Grounded(),"initial terrain contact");
    Need(e.PlaceFeet(body.Position(),true,false,terrain,{}),"initial grounded pose");
    const auto begin=body.Position();
    for(unsigned stage=0;stage<4;++stage){
        // 真正下坡、停步、反向碰坡及再離開；不能只讓角色不動就宣稱通過。
        PlayerLocomotionInput input;input.movement={0,0,stage==0||stage==3?-1.f:stage==1?0.f:1.f};
        const auto start=body.Position();
        for(unsigned tick=0;tick<(stage==1?20u:120u);++tick){
            const auto last=e.footTargets;
            const std::array priorActive{e.footSwings[0].Active(),e.footSwings[1].Active()};
            const auto r=owner.Advance(body,evasion,e,input,LocomotionMotor::FixedStep,{}, {},terrain,[]{return true;});
            ++steps;
            if(!r.accepted)std::cerr<<"x="<<x<<" stage="<<stage<<" tick="<<tick<<" line="<<r.poseFailureLine<<'\n';
            Need(r.accepted&&r.steps==1&&!r.poseRejected,"actual owner must advance");
            const auto world=CreateTransformMatrix(body.Position(),{0,e.yaw,0},{1,1,1});
            const float blend=std::clamp(std::hypot(e.locomotionVelocity.x,e.locomotionVelocity.z)/2.f,0.f,1.f);
            const float movingLower=.09f*blend*blend*(3-2*blend);
            Need(e.pose.localTransforms[0].translation.y>=.9f-movingLower-.35f-.000001f,"per-frame pelvis limit unchanged");
            for(unsigned side=0;side<2;++side){
                const unsigned upper=9+side*3;
                const auto hip=TransformPoint(world,TransformPoint(e.pose.globalMatrices[upper],{}));
                const auto knee=TransformPoint(world,TransformPoint(e.pose.globalMatrices[upper+1],{}));
                const auto ankle=TransformPoint(world,TransformPoint(e.pose.globalMatrices[upper+2],{}));
                Need(std::abs(Length(knee-hip)-.4f)<.0001f&&std::abs(Length(ankle-knee)-.38f)<.0001f,"no stretched bones");
                Need(Length(ankle-e.footTargets[side])<.005f,"ankle reaches real target");
                Need(e.footTargets[side].y>=terrain.SampleHeight(e.footTargets[side].x,e.footTargets[side].z)+.1f-.0005f,"target clearance unchanged");
                Need(Length(e.footTargets[side]-last[side])<60*LocomotionMotor::FixedStep+.0001f,"no foot teleport beyond speed limit");
                Need(e.footSwings[side].SpeedBound()<=60,"swing speed bound unchanged");
                if(!priorActive[side]&&e.footSwings[side].Active()){
                    const auto standingHip=TransformPoint(world,{side?.12f:-.12f,.88f,0});
                    const auto target=e.footSwings[side].Landing();
                    const auto delta=target-standingHip;
                    const float verticalSquared=.778f*.778f-(delta.x*delta.x+delta.z*delta.z);
                    Need(verticalSquared>0&&standingHip.y-target.y-std::sqrt(verticalSquared)<=.35f+.00001f,
                        "new landing reserves standing reach without predicted root motion");
                }
            }
        }
        if(stage==0||stage==3)Need(Length(body.Position()-start)>.5f,"downhill route and escape must move");
    }
    Need(body.Position().z<begin.z-1,"route progresses through world");
}
}
int main()try{
    const auto canonical=CreateTerrain();const auto derived=WestCliffRelief::Build(canonical);
    Route(canonical,-47);Route(derived.terrain,-44);
    std::cout<<"SLOPE_FOOT_REACH_PASS routes=2 steps="<<steps<<" checks="<<checks<<" downhill_escape=2\n";return 0;
}catch(const std::exception& e){std::cerr<<"SLOPE_FOOT_REACH_FAIL "<<e.what()<<'\n';return 1;}
