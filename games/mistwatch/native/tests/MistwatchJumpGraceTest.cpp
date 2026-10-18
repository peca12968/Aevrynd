#include "MistwatchPlayerLocomotion.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Aevrynd;
using namespace Mistwatch;
namespace {
unsigned checks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Fixture {
    HeightField3D terrain{{{-16,0,-16},129,129,.5f}};
    std::array<BoxCollider3D,1> boxes{{{{0,.5f,0},{2,.5f,2}}}};
    CharacterController3D actor;
    LocomotionMotor motor;
    unsigned edgeTick{};
    static CharacterControllerSettings3D Settings(float grace,float grade){
        CharacterControllerSettings3D s;s.jumpGraceSeconds=grace;s.maximumTerrainStandingGrade=grade;return s;
    }
    explicit Fixture(float grace=.1f,float grade=0):actor(Settings(grace,grade)){
        terrain.Fill(0);actor.Reset({0,1,0});
        for(unsigned i=0;i<12;++i)Need(motor.Step(actor,{},boxes,{},terrain),"settle accepted");
        Need(actor.Grounded()&&actor.Position().y==1,"real platform support");
    }
    void Move(bool jump=false,bool crouch=false,bool dash=false){
        LocomotionInput input;input.movement={0,0,1};input.jump=jump;input.crouched=crouch;input.dashing=dash;
        Need(motor.Step(actor,input,boxes,{},terrain),"normal public motor step");
    }
    void WalkOff(){
        for(unsigned i=1;i<=200;++i){Move();if(!actor.Grounded()){edgeTick=i;break;}}
        Need(edgeTick>1&&edgeTick<=200&&actor.Position().y>0&&actor.Velocity().y<=0&&actor.Stats().jumps==0,"natural supported walkoff");
    }
};
}
int main()try{
    // 預設關閉不授權離地起跳；全段逐位元相容另由原始平台探針比較。
    Fixture legacy(0);legacy.WalkOff();Need(!legacy.actor.CanJumpFromRecentGround(),"default grace disabled");
    legacy.Move(true);Need(legacy.actor.Stats().jumps==0,"default airborne jump remains rejected");
    for(float grade:{0.f,1.f}){
        Fixture f(.1f,grade);f.WalkOff();Need(f.actor.CanJumpFromRecentGround(),"walkoff earns bounded grace");
        const auto p=f.actor.Position();f.Move(true);
        Need(f.actor.Stats().jumps==1&&!f.actor.Grounded()&&f.actor.Velocity().y>6&&f.actor.Position().y>p.y,"first airborne input becomes real upward jump");
        Need(!f.actor.CanJumpFromRecentGround(),"successful launch consumes grace");
        for(unsigned i=0;i<40;++i){f.Move(i==0);Need(f.actor.Stats().jumps==1&&!f.actor.Grounded()&&!f.actor.CanJumpFromRecentGround(),"no second airborne impulse");}
        std::cout<<"JUMP_GRACE_IMMEDIATE grade="<<grade<<" edge_tick="<<f.edgeTick<<" jumps="<<f.actor.Stats().jumps<<'\n';
    }
    Fixture last;last.WalkOff();
    for(unsigned i=0;i<10;++i)last.Move();
    Need(last.actor.CanJumpFromRecentGround(),"last available fixed step remains usable");last.Move(true);
    Need(last.actor.Stats().jumps==1,"late in-window input succeeds once");
    Fixture expired;expired.WalkOff();
    for(unsigned i=0;i<11;++i)expired.Move();
    Need(!expired.actor.CanJumpFromRecentGround()&&!expired.actor.Grounded(),"100ms expires without a float extra tick");expired.Move(true);
    Need(expired.actor.Stats().jumps==0,"expired credit cannot launch");
    for(unsigned i=0;i<8;++i){expired.Move();Need(expired.actor.Stats().jumps==0,"expiry is not renewed while falling");}
    Fixture zero;zero.WalkOff();const auto zeroHash=zero.actor.StateHash();
    Need(zero.actor.Step({{},true,1},zero.boxes,{},zero.terrain,0),"zero dt validates");
    Need(zero.actor.StateHash()==zeroHash&&zero.actor.CanJumpFromRecentGround(),"zero dt neither launches nor consumes credit");
    for(float dt:{-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        Need(!zero.actor.Step({{},true,1},zero.boxes,{},zero.terrain,dt),"invalid dt rejected");
        Need(zero.actor.StateHash()==zeroHash,"invalid dt leaves whole state unchanged");
    }
    LocomotionInput invalid;invalid.movement.x=std::numeric_limits<float>::quiet_NaN();
    const auto motorHash=zero.motor.StateHash();
    Need(!zero.motor.Step(zero.actor,invalid,zero.boxes,{},zero.terrain),"invalid input rejects transaction");
    Need(zero.actor.StateHash()==zeroHash&&zero.motor.StateHash()==motorHash,"rejected motor preserves credit and pending input");
    zero.Move(true);Need(zero.actor.Stats().jumps==1,"failed attempts did not consume credit");
    for(float invalidPeriod:{-.1f,.25001f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        CharacterControllerSettings3D s;s.jumpGraceSeconds=invalidPeriod;CharacterController3D actor(s);actor.Reset({0,1,0});
        const auto h=actor.StateHash();Need(!actor.Step({},zero.boxes,{},zero.terrain,LocomotionMotor::FixedStep)&&actor.StateHash()==h,"invalid grace rejects without mutation");
    }
    Fixture spawn;spawn.actor.Reset({0,3,0});spawn.motor.Reset();spawn.Move(true);
    Need(spawn.actor.Stats().jumps==0&&!spawn.actor.CanJumpFromRecentGround(),"initial air has no historical support");
    Fixture noImpulse;auto noImpulseSettings=noImpulse.actor.Settings();noImpulseSettings.jumpSpeed=0;
    noImpulse.actor=CharacterController3D(noImpulseSettings);noImpulse.actor.Reset({0,1,0});noImpulse.motor.Reset();
    for(unsigned i=0;i<12;++i)Need(noImpulse.motor.Step(noImpulse.actor,{},noImpulse.boxes,{},noImpulse.terrain),"zero impulse fixture settle");
    noImpulse.WalkOff();const auto fallingSpeed=noImpulse.actor.Velocity().y;noImpulse.Move(true);
    Need(!noImpulse.actor.CanJumpFromRecentGround()&&noImpulse.actor.Stats().jumps==0&&noImpulse.actor.Velocity().y<fallingSpeed,"zero jump speed cannot brake falling or invent a jump");
    Fixture normal;normal.Move(true);Need(normal.actor.Stats().jumps==1,"normal supported jump");normal.Move(true);
    Need(normal.actor.Stats().jumps==1&&!normal.actor.CanJumpFromRecentGround(),"ground jump clears credit");
    Fixture run;Need(run.actor.TryLaunchFromGround(1.2f,run.boxes,{},run.terrain),"real running launch");run.Move(true);
    Need(run.actor.Stats().jumps==1&&!run.actor.CanJumpFromRecentGround(),"run launch cannot be upgraded to midair full jump");
    for(unsigned mode=0;mode<2;++mode){Fixture f;f.WalkOff();f.Move(true,mode==0,mode==1);Need(f.actor.Stats().jumps==0,"crouch and dash suppress grace jump");}
    // 重設、傳送、讀檔與 direct 位移都必須撤銷舊世界的短暫支撐信用。
    Fixture mutation;mutation.WalkOff();
    {auto actor=mutation.actor;actor.Reset(actor.Position());Need(!actor.CanJumpFromRecentGround(),"reset clears grace");}
    {auto actor=mutation.actor;Need(actor.Teleport(actor.Position(),actor.Yaw(),actor.DirectLastTick()),"legal teleport");Need(!actor.CanJumpFromRecentGround(),"teleport clears grace");}
    {auto actor=mutation.actor;const auto state=actor.CaptureMotionState();Need(actor.TryRestoreMotionState(state,mutation.boxes,{},mutation.terrain),"legal airborne restore");Need(!actor.CanJumpFromRecentGround(),"restore does not replay credit");}
    {auto actor=mutation.actor;Need(actor.ApplyDirectDisplacementTick({1,{},0},mutation.boxes,mutation.terrain),"legal direct no motion");Need(!actor.CanJumpFromRecentGround(),"direct owner clears motor credit");}
    // 上方與動態頂面仍走原接觸解算；不能將接觸旗標冒充真接地。
    Fixture ceiling;ceiling.WalkOff();
    const std::array<BoxCollider3D,2> roof{{ceiling.boxes[0],{{0,3.2f,3},{4,.1f,4}}}};
    LocomotionInput forward;forward.movement={0,0,1};forward.jump=true;
    Need(ceiling.motor.Step(ceiling.actor,forward,roof,{},ceiling.terrain),"grace launch below clear roof");forward.jump=false;
    bool hitRoof=false;
    for(unsigned i=0;i<18;++i){const auto collisions=ceiling.actor.Stats().collisions;Need(ceiling.motor.Step(ceiling.actor,forward,roof,{},ceiling.terrain),"roof collision step");
        if(ceiling.actor.Stats().collisions>collisions&&ceiling.actor.Velocity().y==0&&!ceiling.actor.Grounded())hitRoof=true;
        Need(ceiling.actor.Stats().jumps==1&&!ceiling.actor.CanJumpFromRecentGround(),"ceiling does not renew credit");}
    Need(hitRoof,"real head collision blocks rising jump");
    CharacterControllerSettings3D s;s.jumpGraceSeconds=.1f;CharacterController3D atop(s);atop.Reset({0,1,0});
    const std::array<DynamicCylinder3D,1> dynamic{{{{0,.5f,0},2,.5f}}};CharacterDynamicContactState3D contact;
    Need(atop.Step({}, {},{},mutation.terrain,dynamic,LocomotionMotor::FixedStep,CharacterDynamicVerticalResponse3D::BlockOnly,&contact)==CharacterDynamicStepStatus3D::Applied,"dynamic top contact applied");
    Need(contact.blockedBelow&&!atop.Grounded()&&!atop.CanJumpFromRecentGround(),"dynamic top cannot grant jump credit");
    s.maximumTerrainStandingGrade=1;CharacterController3D steep(s);HeightField3D slope({{-8,0,-8},33,33,.5f});
    for(unsigned z=0;z<33;++z)for(unsigned x=0;x<33;++x)Need(slope.SetHeight(x,z,4*(-8.f+.5f*z)),"slope setup");
    steep.Reset({0,0,0});Need(steep.Step({}, {},{},slope,LocomotionMotor::FixedStep),"real steep contact");
    Need(!steep.Grounded()&&!steep.CanJumpFromRecentGround(),"steep slide never earns grace");
    // 既有落地前緩衝仍需在寬限早已到期之後、真落地的下一步才消耗。
    Fixture buffer;buffer.WalkOff();
    while(buffer.actor.Position().y>.15f){buffer.Move();Need(buffer.actor.Stats().jumps==0,"falling without auto jump");}
    Need(!buffer.actor.Grounded()&&!buffer.actor.CanJumpFromRecentGround(),"buffer begins after grace expiry");buffer.Move(true);
    bool landed=false,fired=false;
    for(unsigned i=0;i<16&&!fired;++i){const bool before=buffer.actor.Grounded();buffer.Move();landed=landed||buffer.actor.Grounded();
        if(buffer.actor.Stats().jumps){Need(before&&landed&&buffer.actor.Stats().jumps==1,"buffer waits for true landing");fired=true;}}
    Need(fired,"landing buffer still works with grace enabled");
    // 真 Player owner、骨架、腳部與外幀交易共同驗證，不只單獨 Motor。
    for(unsigned hz:{120u,60u,30u}){
        Fixture f;PlayerLocomotion owner;Evasion evasion;Explorer explorer;
        explorer.Update({},0,false);Need(explorer.PlaceFeet(f.actor.Position(),true,false,f.terrain,f.boxes,{}),"owner initial real pose");
        PlayerLocomotionInput input;input.movement={0,0,1};unsigned frames{};
        for(;frames<1000&&f.actor.Grounded();++frames){
            const auto result=owner.Advance(f.actor,evasion,explorer,input,1.f/float(hz),f.boxes,{},f.terrain,[]{return true;});
            Need(result.accepted&&result.steps==120/hz,"owner natural walkoff accepted with real pose");
        }
        Need(frames>1&&frames<1000&&!f.actor.Grounded()&&f.actor.CanJumpFromRecentGround(),"owner naturally reaches grace window");
        input.jumpPressed=true;const auto actorHash=f.actor.StateHash(),ownerHash=owner.StateHash();unsigned callbacks{};
        const auto rejected=owner.Advance(f.actor,evasion,explorer,input,1.f/float(hz),f.boxes,{},f.terrain,[&]{++callbacks;return false;});
        Need(!rejected.accepted&&callbacks==1&&f.actor.StateHash()==actorHash&&owner.StateHash()==ownerHash,"camera rejection restores grace and queued edge");
        const auto queued=owner.Advance(f.actor,evasion,explorer,input,0,f.boxes,{},f.terrain,[]{return true;});
        Need(queued.accepted&&queued.steps==0&&f.actor.StateHash()==actorHash,"zero substep retains input without aging grace");
        input.jumpPressed=false;const auto launched=owner.Advance(f.actor,evasion,explorer,input,1.f/float(hz),f.boxes,{},f.terrain,[]{return true;});
        Need(launched.accepted&&launched.steps==120/hz&&f.actor.Stats().jumps==1&&f.actor.Velocity().y>6&&!f.actor.Grounded(),"owner queued airborne jump drives real pose at every frame rate");
        std::cout<<"JUMP_GRACE_OWNER hz="<<hz<<" natural_edge_frames="<<frames<<" fixed_steps="<<launched.steps<<" rollback=1 zero_substep=1\n";
    }
    std::cout<<"JUMP_GRACE_PASS checks="<<checks<<" seconds=0.1 default_disabled=1 no_double_jump=1 GPU=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"JUMP_GRACE_FAIL "<<e.what()<<" checks="<<checks<<'\n';return 1;}
