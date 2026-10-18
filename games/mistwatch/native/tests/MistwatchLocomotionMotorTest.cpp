#include "MistwatchLocomotionMotor.h"
#include "MistwatchEvasion.h"
#include <iostream>
#include <stdexcept>
using namespace Aevrynd;using namespace Mistwatch;
unsigned checks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
int main()try{
    HeightField3D terrain({{-20,0,-20},128,128,1});terrain.Fill(0);
    // 完全反向的左右路徑等長；相機矩陣的數個浮點尾差不能使轉向側跳變。
    for(float heading:{0.f,1.57079632679f,-1.57079632679f,3.14159265359f,-3.14159265359f})
    for(float residual:{-1.e-7f,0.f,1.e-7f}){
        CharacterController3D opposite;opposite.Reset({0,0,0});
        Need(opposite.Step({}, {},{},terrain,LocomotionMotor::FixedStep),"opposite fixture settle");
        LocomotionMotor turning;auto state=turning.CaptureMotionState();state.headingReady=true;state.heading=heading;
        Need(turning.TryRestoreMotionState(state,opposite),"opposite fixture heading");
        for(unsigned tick=0;tick<6;++tick){
            const float noise=tick%2?-residual:residual;
            LocomotionInput input;input.movement={-std::sin(heading)+noise*std::cos(heading),0,-std::cos(heading)-noise*std::sin(heading)};
            const float prior=turning.CaptureMotionState().heading;
            Need(turning.Step(opposite,input,{},{},terrain),"opposite finite step");
            const float delta=std::remainder(turning.CaptureMotionState().heading-prior,6.2831853f);
            Need(delta>0&&delta<=LocomotionMotor::TurnRate*LocomotionMotor::FixedStep+.000001f,
                "Opposite direction roundoff must keep one finite turn side");
        }
    }
    // 離開浮點同分區後，仍依真正較短的一側轉，不能把所有反向都強制同側。
    for(float heading:{0.f,1.57079632679f,-1.57079632679f,3.14159265359f})for(float offset:{-.00001f,.00001f}){
        CharacterController3D actor;actor.Reset({0,0,0});Need(actor.Step({}, {},{},terrain,LocomotionMotor::FixedStep),"off-tie settle");
        LocomotionMotor motor;auto state=motor.CaptureMotionState();state.headingReady=true;state.heading=heading;
        Need(motor.TryRestoreMotionState(state,actor),"off-tie heading");
        const double target=double(heading)+3.14159265358979323846+double(offset);
        LocomotionInput input;input.movement={float(std::sin(target)),0,float(std::cos(target))};
        Need(motor.Step(actor,input,{},{},terrain),"off-tie step");
        const float delta=std::remainder(motor.CaptureMotionState().heading-heading,6.2831853f);
        Need(delta*offset<0&&std::abs(delta)<=LocomotionMotor::TurnRate*LocomotionMotor::FixedStep+.000001f,"off-tie shortest side remains authoritative");
    }
    for(const auto direction:{Vec3{0,0,1},Vec3{0,0,-1},Vec3{1,0,0},Vec3{-1,0,0}}){
        CharacterController3D first;first.Reset({0,0,0});Need(first.Step({}, {},{},terrain,LocomotionMotor::FixedStep),"first direction settle");
        LocomotionMotor fresh;LocomotionInput input;input.movement=direction;
        for(unsigned tick=0;tick<8;++tick){const auto before=first.Position();Need(fresh.Step(first,input,{}, {},terrain),"first direction move");
            const auto d=first.Position()-before;
            Need(d.x*direction.x+d.z*direction.z>0,"first movement follows input, no world north bias");
            Need(std::abs(d.x*direction.z-d.z*direction.x)<.000001f,"first movement has no unintended lateral arc");}
    }
    CharacterController3D actor;actor.Reset({0,0,0});Need(actor.Step({}, {},{},terrain,LocomotionMotor::FixedStep),"settle");
    LocomotionMotor motor;LocomotionInput run{{0,0,1},1,false,false,false,true};
    unsigned flights=0,landings=0;float previousSpeed=0;
    for(unsigned i=0;i<360;++i){
        const bool grounded=actor.Grounded();Need(motor.Step(actor,run,{}, {},terrain),"accelerating run");
        const float speed=Length(motor.Velocity());Need(speed<=6.00001f&&speed-previousSpeed<=.10001f,"finite input acceleration");previousSpeed=speed;
        if(grounded&&!actor.Grounded())++flights;if(!grounded&&actor.Grounded())++landings;
        Need(actor.Position().y>=0,"never below ground");
    }
    Need(flights>5&&landings>5,"real repeated run support and flight");
    const auto launchCount=actor.Stats().jumps;
    for(unsigned i=0;i<180;++i){Need(motor.Step(actor,{}, {},{},terrain),"decelerating stop");Need(actor.Stats().jumps==launchCount,"no auto takeoff after stop request");}
    Need(motor.Phase()==LocomotionPhase::Standing&&Length(motor.Velocity())==0&&actor.Grounded(),"stationary after stop");
    for(unsigned i=0;i<120;++i)Need(motor.Step(actor,run,{}, {},terrain),"run before turn");
    auto reverse=run;reverse.movement={0,0,-1};float old=actor.Velocity().z;
    for(unsigned i=0;i<140;++i){Need(motor.Step(actor,reverse,{}, {},terrain),"finite reverse");Need(std::abs(actor.Velocity().z-old)<=.15002f,"no instantaneous reverse");old=actor.Velocity().z;}
    Need(actor.Velocity().z<-5.9f,"reverse eventually moves backward");
    // 將起跳邊沿放到自動離地同幀，玩家完整跳躍必須優先。
    for(unsigned i=0;i<1000&&!(actor.Grounded()&&motor.SupportTicks()==10);++i)Need(motor.Step(actor,reverse,{}, {},terrain),"seek takeoff boundary");
    Need(actor.Grounded()&&motor.SupportTicks()==10,"takeoff boundary reached");
    auto jump=reverse;jump.jump=true;Need(motor.Step(actor,jump,{}, {},terrain),"player jump at run boundary");
    Need(actor.Velocity().y>6&&motor.Phase()==LocomotionPhase::JumpOrFall,"player jump overrides run hop");
    const auto fullJump=actor.Stats().jumps;Need(motor.Step(actor,jump,{}, {},terrain)&&actor.Stats().jumps==fullJump,"no airborne repeated jump");
    const auto actorHash=actor.StateHash(),motorHash=motor.StateHash();
    auto invalid=run;invalid.movement.x=std::numeric_limits<float>::quiet_NaN();
    Need(!motor.Step(actor,invalid,{}, {},terrain)&&actor.StateHash()==actorHash&&motor.StateHash()==motorHash,"invalid input atomic");
    std::array<BoxCollider3D,1> badBox{{{{0,0,0},{-1,1,1}}}};
    Need(!motor.Step(actor,run,badBox,{},terrain)&&actor.StateHash()==actorHash&&motor.StateHash()==motorHash,"invalid collision atomic");
    motor.Reset();Need(motor.Tick()==0&&motor.SupportTicks()==0&&Length(motor.Velocity())==0,"load retry reset has no old momentum");
    CharacterController3D wallActor;wallActor.Reset({0,0,0});Need(wallActor.Step({}, {},{},terrain,LocomotionMotor::FixedStep),"wall settle");
    LocomotionMotor wallMotor;std::array<BoxCollider3D,1> wall{{{{0,2,2},{2,2,.1f}}}};
    for(unsigned i=0;i<240;++i)Need(wallMotor.Step(wallActor,run,wall,{},terrain),"wall motor tick");
    Need(wallActor.Position().z<=1.451f&&std::abs(wallMotor.Velocity().z)<.001f,"wall blocks position and motor velocity");
    // 使用原閃避時計的末幀 fraction，不以人工完整 tick 代替 .28s 的實際距離。
    // 三個退出案例各自重建平地角色；normal motor 不可保留 dash 的末幀速度。
    for(unsigned exitMode=0;exitMode<3;++exitMode){
        CharacterController3D dashActor;dashActor.Reset({0,0,0});
        Need(dashActor.Step({}, {},{},terrain,LocomotionMotor::FixedStep)&&dashActor.Grounded(),"dash fixture grounded");
        LocomotionMotor dashMotor;Evasion evasion;
        const float initialZ=dashActor.Position().z;
        bool sawDash=false,exited=false;unsigned dashTicks=0,postTicks=0;
        float exitZ=0,lastFraction=0;
        const auto initialJumps=dashActor.Stats().jumps;
        for(unsigned tick=0;tick<180&&postTicks<60;++tick){
            const auto motion=evasion.Tick(LocomotionMotor::FixedStep,tick==0,dashActor.Grounded(),false,{0,0,1});
            const bool firstExit=sawDash&&!motion.dashing&&!exited;
            if(firstExit){
                exitZ=dashActor.Position().z;exited=true;
                Need(std::abs(exitZ-initialZ-3.36f)<.0002f,"actual Evasion dash distance remains 3.36m");
                Need(lastFraction>0&&lastFraction<1,"actual last dash tick is fractional");
            }
            LocomotionInput input;
            input.movement=motion.dashing?motion.movement:(exitMode?Vec3{0,0,1}:Vec3{});
            input.speedMultiplier=motion.dashing?Evasion::SpeedMultiplier:1.f;
            input.dashing=motion.dashing;
            input.jump=firstExit&&exitMode==2;
            const float beforeZ=dashActor.Position().z;
            Need(dashMotor.Step(dashActor,input,{}, {},terrain),"dash transition step");
            if(motion.dashing){
                sawDash=true;++dashTicks;lastFraction=Length(motion.movement);
                const float expectedSpeed=6.f*Evasion::SpeedMultiplier*lastFraction;
                Need(std::abs(dashMotor.Velocity().z-expectedSpeed)<.00002f,"dash keeps original instantaneous fractional speed");
                Need(std::abs(dashActor.Position().z-beforeZ-expectedSpeed*LocomotionMotor::FixedStep)<.000003f,"dash tick distance follows actual fraction");
                Need(dashActor.Stats().jumps==initialJumps,"dash does not auto hop");
            }else if(exited){
                ++postTicks;
                const float expectedSpeed=exitMode?6.f:0.f;
                Need(std::abs(dashMotor.Velocity().z-expectedSpeed)<.00002f&&std::abs(dashActor.Velocity().z-expectedSpeed)<.00002f,"dash exit restores normal desired speed immediately");
                if(exitMode==0)Need(dashActor.Position().z==exitZ,"dash to standing has no extra sliding distance");
                if(firstExit&&exitMode==2)Need(!dashActor.Grounded()&&dashActor.Velocity().y>6&&dashMotor.Phase()==LocomotionPhase::JumpOrFall,"dash exit accepts full player jump");
                Need(dashActor.Stats().jumps==initialJumps+(exitMode==2?1u:0u),"exactly one requested exit jump");
            }
        }
        Need(sawDash&&exited&&dashTicks==34&&postTicks==60,"bounded actual dash and post-dash coverage");
        const float expectedTotal=3.36f+(exitMode?3.f:0.f);
        Need(std::abs(dashActor.Position().z-initialZ-expectedTotal)<.0005f,"dash plus half second post-motion preserves distance");
        std::cout<<"LOCOMOTION_DASH_EXIT mode="<<exitMode<<" dash_ticks="<<dashTicks<<" last_fraction="<<lastFraction<<" total_distance="<<dashActor.Position().z-initialZ<<'\n';
    }
    // 即使請求跑步飛行且速度達門檻，蹲姿仍不得自動起跳；完整跳躍輸入也受蹲姿規則抑制。
    CharacterController3D crouchActor;crouchActor.Reset({0,0,0});
    Need(crouchActor.Step({}, {},{},terrain,LocomotionMotor::FixedStep)&&crouchActor.Grounded(),"crouch fixture grounded");
    LocomotionMotor crouchMotor;auto crouchRun=run;crouchRun.crouched=true;
    const auto crouchJumps=crouchActor.Stats().jumps;
    for(unsigned i=0;i<240;++i){
        crouchRun.jump=i==120;
        Need(crouchMotor.Step(crouchActor,crouchRun,{}, {},terrain),"crouched run input");
        Need(crouchActor.Grounded()&&crouchActor.Stats().jumps==crouchJumps&&crouchMotor.SupportTicks()==0,"crouch never auto launches");
        Need(crouchMotor.Phase()!=LocomotionPhase::RunFlight&&crouchMotor.Phase()!=LocomotionPhase::RunSupport,"crouch does not enter run flight scheduling");
    }
    Need(Length(crouchMotor.Velocity())>=5.9f,"crouch suppression exercised above auto hop speed threshold");
    // 在短跑騰空中只按一次 Space，等待真著地才消耗；不得變成空中二段跳。
    CharacterController3D bufferedActor;bufferedActor.Reset({0,0,0});
    Need(bufferedActor.Step({}, {},{},terrain,LocomotionMotor::FixedStep)&&bufferedActor.Grounded(),"buffer fixture grounded");
    LocomotionMotor bufferedMotor;
    for(unsigned i=0;i<600&&bufferedMotor.Phase()!=LocomotionPhase::RunFlight;++i)
        Need(bufferedMotor.Step(bufferedActor,run,{}, {},terrain),"seek real running flight");
    Need(bufferedMotor.Phase()==LocomotionPhase::RunFlight&&!bufferedActor.Grounded()&&bufferedActor.Velocity().y>0,"buffer begins during rising run hop");
    const auto hopsBeforeBuffer=bufferedActor.Stats().jumps;
    const float velocityBeforeBuffer=bufferedActor.Velocity().y;
    auto bufferedJump=run;bufferedJump.jump=true;
    Need(bufferedMotor.Step(bufferedActor,bufferedJump,{}, {},terrain),"single airborne jump edge queued");
    Need(!bufferedActor.Grounded()&&bufferedActor.Stats().jumps==hopsBeforeBuffer&&bufferedActor.Velocity().y<velocityBeforeBuffer,"queued jump does not launch in air");
    bool bufferedFired=false,sawRealLanding=false;
    for(unsigned i=0;i<20&&!bufferedFired;++i){
        const bool wasGrounded=bufferedActor.Grounded();
        Need(bufferedMotor.Step(bufferedActor,run,{}, {},terrain),"release edge and await landing");
        if(bufferedActor.Grounded())sawRealLanding=true;
        if(bufferedActor.Stats().jumps>hopsBeforeBuffer){
            Need(wasGrounded&&sawRealLanding&&bufferedActor.Stats().jumps==hopsBeforeBuffer+1,"buffer consumed once only after actual landing");
            Need(!bufferedActor.Grounded()&&bufferedActor.Velocity().y>6&&bufferedMotor.Phase()==LocomotionPhase::JumpOrFall,"buffer produces full player jump rather than another short hop");
            bufferedFired=true;
        }else Need(bufferedActor.Stats().jumps==hopsBeforeBuffer,"no extra launch before support");
    }
    Need(bufferedFired,"one airborne edge survives short running flight");
    const auto fullBufferedJumpCount=bufferedActor.Stats().jumps;
    Need(bufferedMotor.Step(bufferedActor,bufferedJump,{}, {},terrain),"new edge during full jump queued only");
    for(unsigned i=0;i<40;++i){
        Need(bufferedMotor.Step(bufferedActor,run,{}, {},terrain),"full jump stays airborne past buffer lifetime");
        Need(!bufferedActor.Grounded()&&bufferedActor.Stats().jumps==fullBufferedJumpCount,"no midair double jump when buffer expires");
    }
    std::cout<<"LOCOMOTION_MOTOR_PASS checks="<<checks<<" flights="<<flights<<" landings="<<landings<<" pose_camera_main_integrated=false\n";return 0;
}catch(const std::exception& e){std::cerr<<"LOCOMOTION_MOTOR_FAIL "<<e.what()<<'\n';return 1;}
