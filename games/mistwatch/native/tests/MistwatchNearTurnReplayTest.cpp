#include "MistwatchInputReplay.h"
#include "MistwatchPlayerLocomotion.h"
#include <iostream>
#include <stdexcept>
using namespace Aevrynd;
using namespace Mistwatch;
namespace {
unsigned checks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
}
int main()try{
    HeightField3D terrain({{-20,0,-20},128,128,1});terrain.Fill(0);
    unsigned cases{};
    // 只合成初始站姿及 driver 起始階段；其後每一步皆由正式碰撞、motor、支撐腳及 IK 前進。
    // 平地空障礙情境不代表完整世界、鏡頭或 GPU 驗收。
    for(float distance:{.19f,.32f,.6f})for(float heading:{1.57079632679f,-1.57079632679f,0.f,3.14159265359f})
    for(float cameraYaw:{0.f,1.57079632679f,-1.57079632679f,.78539816339f}){
        CharacterControllerSettings3D settings;settings.maximumTerrainUphillGrade=1;settings.maximumTerrainStandingGrade=1;
        CharacterController3D actor(settings);actor.Reset({-.0159f,0,18+distance});
        Need(actor.Step({}, {},{},terrain,LocomotionMotor::FixedStep)&&actor.Grounded(),"fixture settle");
        Explorer explorer;explorer.yaw=explorer.requestedYaw=heading;explorer.Update({},0,false);
        Need(explorer.PlaceFeet(actor.Position(),true,false,terrain,{},{}),"fixture feet");
        Evasion evasion;PlayerLocomotion owner;
        auto state=owner.CaptureMotionState();state.motor.heading=heading;state.motor.headingReady=true;
        Need(owner.TryRestoreMotionState(state,actor,evasion),"fixture heading");
        InputReplay replay;Session session;replay.Tick(session,0,1.8f,true,false,1.f/60.f);
        Need(replay.Phase()=="walk-road","fixture route phase");
        unsigned ticks=0,pulses=0;bool previousPressed=false;float length=0,maximumExcursion=0;
        for(;ticks<600&&replay.Phase()=="walk-road"&&!replay.Finished();++ticks){
            session.feet=actor.Position();const auto before=session.Serialize();
            replay.Tick(session,cameraYaw,1.8f,true,false,1.f/60.f,nullptr,nullptr,actor.Grounded(),nullptr,{},actor.Velocity());
            Need(session.Serialize()==before,"driver modified observation");
            if(replay.Phase()!="walk-road")break;
            auto& keys=replay.Controls();
            const float x=float(keys.Down('D'))-float(keys.Down('A')),z=float(keys.Down('W'))-float(keys.Down('S'));
            PlayerLocomotionInput input;
            input.movement={x*std::cos(cameraYaw)-z*std::sin(cameraYaw),0,x*std::sin(cameraYaw)+z*std::cos(cameraYaw)};
            if(Length(input.movement)>1)input.movement=input.movement*(1/Length(input.movement));
            Need(!keys.Down(VK_SHIFT)&&!keys.Down(VK_SPACE),"near arrival must not run or jump");
            const bool pressed=Length(input.movement)>0;
            if(pressed&&!previousPressed)++pulses;
            previousPressed=pressed;
            for(unsigned sub=0;sub<2;++sub){
                const auto prior=actor.Position(),velocity=actor.Velocity();const float yaw=owner.Motor().CaptureMotionState().heading;
                const auto result=owner.Advance(actor,evasion,explorer,input,LocomotionMotor::FixedStep,{},{},terrain,[]{return true;});
                Need(result.accepted&&result.steps==1&&!result.dashing,"actual owner rejected");
                Need(actor.Grounded()&&actor.Stats().jumps==0,"unexpected flight");
                Need(Length(actor.Velocity()-velocity)<=LocomotionMotor::Deceleration*LocomotionMotor::FixedStep+.00002f,"instantaneous velocity change");
                Need(std::abs(std::remainder(owner.Motor().CaptureMotionState().heading-yaw,6.2831853f))<=LocomotionMotor::TurnRate*LocomotionMotor::FixedStep+.000002f,"instantaneous heading change");
                length+=Length(actor.Position()-prior);
                maximumExcursion=(std::max)(maximumExcursion,std::hypot(actor.Position().x,actor.Position().z-18.f));
            }
        }
        Need(replay.Phase()=="approach-shelter"&&std::hypot(actor.Position().x,actor.Position().z-18.f)<=.18f&&Length(actor.Velocity())<.001f,"arrival requires original tolerance and real stop");
        std::cout<<"NEAR_METRIC case="<<cases+1<<" distance="<<distance<<" heading="<<heading<<" camera="<<cameraYaw<<" ticks="<<ticks<<" path="<<length<<" max_distance="<<maximumExcursion<<'\n';
        // 此平地近點情境的體驗預算：1.5秒內停妥，向外多偏移最多5公分。
        Need(ticks<=90&&length<=distance+.18f&&maximumExcursion<=distance+.05f,"turn must not orbit or make an excessive detour");
        if(distance>=.32f&&std::abs(heading)>1.f&&std::abs(heading)<2.f)
            Need(pulses<=2,"room for closing turn must not produce repeated key-release pulses");
        std::cout<<"NEAR_TURN case="<<++cases<<" ticks="<<ticks<<" pulses="<<pulses<<" path="<<length<<" max_distance="<<maximumExcursion<<'\n';
    }
    {
        // 目標在牆內時必須維持真碰撞並以原停滯上限結束，不能當成到達或刷新時鐘。
        CharacterController3D actor;actor.Reset({0,0,18.7f});
        std::array<BoxCollider3D,1> boxes{{{{0,2,18},{2,2,.1f}}}};
        Need(actor.Step({},boxes,{},terrain,LocomotionMotor::FixedStep)&&actor.Grounded(),"wall fixture settle");
        Explorer explorer;explorer.yaw=explorer.requestedYaw=3.14159265359f;explorer.Update({},0,false);
        Need(explorer.PlaceFeet(actor.Position(),true,false,terrain,boxes,{}),"wall fixture feet");
        Evasion evasion;PlayerLocomotion owner;InputReplay replay;Session session;
        replay.Tick(session,0,1.8f,true,false,1.f/60.f);
        for(unsigned tick=0;tick<720&&!replay.Finished();++tick){
            session.feet=actor.Position();
            replay.Tick(session,0,1.8f,true,false,1.f/60.f,nullptr,nullptr,actor.Grounded(),nullptr,{},actor.Velocity());
            Need(replay.Phase()=="walk-road"||replay.Finished(),"wall cannot advance route");
            PlayerLocomotionInput input;auto& keys=replay.Controls();
            input.movement={float(keys.Down('D'))-float(keys.Down('A')),0,float(keys.Down('W'))-float(keys.Down('S'))};
            const auto result=owner.Advance(actor,evasion,explorer,input,1.f/60.f,boxes,{},terrain,[]{return true;});
            Need(result.accepted&&actor.Position().z>=18.549f,"collision must remain authoritative");
        }
        Need(replay.Finished()&&!replay.Passed()&&replay.Failure()=="phase-stalled-600-ticks","inaccessible target must expire without passing");
    }
    std::cout<<"NEAR_TURN_OWNER_PASS cases="<<cases<<" checks="<<checks<<" native_world=0 wall_negative=1\n";return 0;
}catch(const std::exception& e){std::cerr<<"NEAR_TURN_OWNER_ERROR "<<e.what()<<'\n';return 1;}
