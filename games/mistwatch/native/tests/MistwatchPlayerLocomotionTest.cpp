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
void EvadeBufferGreenControls(){
    unsigned bounded{},named{},starts{},rejections{},freshControls{};
    std::array<std::array<std::array<std::uint64_t,4>,21>,2> references{};
    const auto snapshot=[](const Fixture& f){return std::array<std::uint64_t,4>{f.actor.StateHash(),f.owner.StateHash(),PoseHash(f.explorer),EvasionHash(f.evasion)};};
    const auto fresh=[&](Fixture& f){PlayerLocomotionInput q;q.evadePressed=true;q.dodgeDirection={0,0,1};
        Need(f.actor.Grounded()&&f.evasion.remaining==0&&f.evasion.cooldown==0,"fresh Q positive-control prerequisites");
        const auto result=f.Step(q);Need(result.accepted&&result.dashing&&f.owner.PendingEvadeTicks()==0,"fresh Q remains responsive after negative control");++freshControls;};
    // 每個外幀頻率都遍歷冷卻尾端及超期；真控制器、骨架與閃避採相同120Hz固定步。
    for(unsigned hz:{120u,60u,30u})for(unsigned queued=0;queued<2;++queued)for(unsigned delay=0;delay<=20;++delay){
        Fixture f;f.evasion.direction={0,0,1};f.evasion.cooldown=float(delay)*LocomotionMotor::FixedStep;
        float expectedCooldown=f.evasion.cooldown;unsigned ready{};
        for(unsigned tick=1;tick<=32;++tick){expectedCooldown=(std::max)(0.f,expectedCooldown-LocomotionMotor::FixedStep);
            if(!ready&&expectedCooldown<=0)ready=tick;}
        Need(ready>0,"bounded cooldown really reaches ready state");const bool within=ready<=16;
        PlayerLocomotionInput q;q.evadePressed=true;q.dodgeDirection={0,0,1};
        if(queued){const auto zero=f.Step(q,0);Need(zero.accepted&&zero.steps==0&&f.owner.PendingEvadeTicks()==16,
            "zero-substep Q retains all sixteen valid ticks");}
        unsigned ticks{},actualStarts{};
        for(unsigned frame=0;frame<2*hz;++frame){const auto before=f.evasion.cooldown;
            const auto result=f.Step(!queued&&frame==0?q:PlayerLocomotionInput{},1.f/float(hz));
            Need(result.accepted&&result.steps==120/hz,"bounded queued Q actual fixed-step body and pose");
            const unsigned previous=ticks;ticks+=result.steps;
            if(f.evasion.cooldown>before){++actualStarts;++starts;
                Need(within&&ready>previous&&ready<=ticks,"Q starts only when original cooldown and original window permit");}
            const unsigned pending=!actualStarts&&ticks<16?16-ticks:0;
            Need(f.owner.PendingEvadeTicks()==pending,"unaccepted Q expires by valid fixed ticks, never outer frames");
        }
        Need(ticks==240&&actualStarts==(within?1u:0u),"single Q neither disappears inside window nor repeats after cooldown");
        const std::array state{f.actor.StateHash(),f.owner.Motor().StateHash(),PoseHash(f.explorer),EvasionHash(f.evasion)};
        if(hz==120)references[queued][delay]=state;
        else Need(state==references[queued][delay],"Q buffer frequency-invariant physics, pose, motor and cooldown");
        fresh(f);++bounded;
    }
    // 暫停、重設及蹲下是明確取消，不得在冷卻結束後補做已取消的閃避。
    for(unsigned cancel=0;cancel<3;++cancel){Fixture f;f.evasion.direction={0,0,1};f.evasion.cooldown=6.f*LocomotionMotor::FixedStep;
        PlayerLocomotionInput q;q.evadePressed=true;q.dodgeDirection={0,0,1};
        Need(f.Step(q).accepted&&f.owner.PendingEvadeTicks()==15,"prepare cancellation with real queued Q");
        if(cancel==0)f.owner.Suspend();else if(cancel==1)f.owner.Reset();else{
            PlayerLocomotionInput crouch;crouch.crouched=true;Need(f.Step(crouch).accepted,"crouch cancellation actual pose");}
        Need(f.owner.PendingEvadeTicks()==0,"explicit cancellation clears pending Q");
        for(unsigned tick=0;tick<180;++tick){const auto result=f.Step();Need(result.accepted&&!result.dashing,"cancelled Q cannot restart later");}
        fresh(f);++named;
    }
    // 拒絕鏡頭的整幀交易，必須連本來即將接單的Q與冷卻一起還原，重試只接一次。
    for(bool throwing:{false,true}){Fixture f;f.evasion.direction={0,0,1};f.evasion.cooldown=2.f*LocomotionMotor::FixedStep;
        PlayerLocomotionInput q;q.evadePressed=true;q.dodgeDirection={0,0,1};
        Need(f.Step(q).accepted&&f.owner.PendingEvadeTicks()==15,"prepare Q at final cooldown step");
        const auto before=snapshot(f);bool called{};
        const auto result=f.owner.Advance(f.actor,f.evasion,f.explorer,{},LocomotionMotor::FixedStep,{}, {},f.terrain,[&]()->bool{
            called=true;if(throwing)throw std::runtime_error("rejected-camera-probe");return false;});
        Need(called&&!result.accepted&&snapshot(f)==before,"camera-rejected new dash restores input, time, physics and pose");++rejections;
        const auto retry=f.Step();Need(retry.accepted&&retry.dashing&&f.owner.PendingEvadeTicks()==0,"same queued Q succeeds on accepted retry");
        for(unsigned tick=0;tick<240;++tick){const auto cooldown=f.evasion.cooldown;Need(f.Step().accepted,"post-rollback real dash drains naturally");
            Need(f.evasion.cooldown<=cooldown,"rollback must not create a second Q edge");}
        fresh(f);++named;
    }
    // 壞幾何及無效外幀不消耗有效等待期限；不能以拒絕計數把16tick窗口拖長或縮短。
    for(bool invalidTime:{false,true}){Fixture f;f.evasion.direction={0,0,1};f.evasion.cooldown=2.f*LocomotionMotor::FixedStep;
        PlayerLocomotionInput q;q.evadePressed=true;q.dodgeDirection={0,0,1};
        Need(f.Step(q).accepted&&f.owner.PendingEvadeTicks()==15,"prepare Q before invalid candidate");
        const auto before=snapshot(f);bool called{};BoxCollider3D bad{{0,0,0},{1,std::numeric_limits<float>::quiet_NaN(),1}};
        const auto boxes=invalidTime?std::span<const BoxCollider3D>{}:std::span<const BoxCollider3D>(&bad,1);
        for(unsigned attempt=0;attempt<17;++attempt){const auto result=f.owner.Advance(f.actor,f.evasion,f.explorer,{},
                invalidTime?.1001f:LocomotionMotor::FixedStep,boxes,{},f.terrain,[&]{called=true;return true;});
            Need(!result.accepted&&!called&&snapshot(f)==before,"invalid candidates preserve pending Q without consuming valid ticks");++rejections;}
        const auto retry=f.Step();Need(retry.accepted&&retry.dashing&&f.owner.PendingEvadeTicks()==0,"valid retry accepts original Q, not a replacement edge");++named;
    }
    {
        Fixture f;PlayerLocomotionInput empty;empty.evadePressed=true;
        Need(f.Step(empty).accepted&&f.owner.PendingEvadeTicks()==15,"zero-direction Q not mistaken for accepted dash");
        for(unsigned tick=0;tick<180;++tick){const auto result=f.Step();Need(result.accepted&&!result.dashing,"zero-direction Q never becomes a fabricated dash");}
        Need(f.owner.PendingEvadeTicks()==0,"zero-direction Q expires");fresh(f);++named;
    }
    {
        Fixture f;Need(f.actor.TryLaunchFromGround(5.f),"prepare long real airborne period");PlayerLocomotionInput q;q.evadePressed=true;q.dodgeDirection={0,0,1};
        for(unsigned tick=0;tick<180;++tick){const auto result=f.Step(tick==0?q:PlayerLocomotionInput{});
            Need(result.accepted&&!result.dashing,"Q cannot start in air or resurrect after airborne expiry");
            if(tick==15)Need(!f.actor.Grounded()&&f.owner.PendingEvadeTicks()==0,"long-air Q expires after original sixteen valid ticks");}
        fresh(f);++named;
    }
    {
        Fixture original;PlayerLocomotionInput q;q.evadePressed=true;q.dodgeDirection={0,0,1};
        Need(original.Step(q).dashing,"prepare original accepted dash");auto buffered=original;
        for(unsigned tick=0;tick<240;++tick){const auto baseline=original.Step(),candidate=buffered.Step(tick==0?q:PlayerLocomotionInput{});
            Need(baseline.accepted&&candidate.accepted&&original.actor.StateHash()==buffered.actor.StateHash()&&
                EvasionHash(original.evasion)==EvasionHash(buffered.evasion)&&PoseHash(original.explorer)==PoseHash(buffered.explorer),
                "Q during active dash does not extend dash, bypass cooldown or repeat later");}
        Need(buffered.owner.PendingEvadeTicks()==0,"Q pressed during active dash expires without repeat");fresh(buffered);++named;
    }
    Need(bounded==126&&named==10&&rejections==36,"bounded Q control accounting");
    std::cout<<"EVADE_COOLDOWN_BUFFER_CPU_PASS bounded_cases="<<bounded<<" named_cases="<<named<<" queued_dash_starts="<<starts
        <<" rejected_candidates="<<rejections<<" fresh_edge_controls="<<freshControls<<" window_ticks=16 cooldown_seconds="<<Evasion::Cooldown
        <<" dash_seconds="<<Evasion::Duration<<" native_play=0 GPU=0\n";
}
int main()try{
    {
        // 冷卻尾端的單次Q應保留原16固定步期限；接地但尚未接單不能提早吞掉輸入。
        Fixture f;f.evasion.direction={0,0,1};f.evasion.cooldown=6.f*LocomotionMotor::FixedStep;
        float expectedCooldown=f.evasion.cooldown;unsigned expectedStart{},actualStart{},firstPending{};
        PlayerLocomotionInput q;q.evadePressed=true;q.dodgeDirection={0,0,1};
        for(unsigned tick=1;tick<=16;++tick){
            // 獨立累積既有float冷卻規則，避免以實數乘法假設邊界剛好整除。
            expectedCooldown=(std::max)(0.f,expectedCooldown-LocomotionMotor::FixedStep);
            if(!expectedStart&&expectedCooldown<=0)expectedStart=tick;
            const auto result=f.Step(tick==1?q:PlayerLocomotionInput{});
            Need(result.accepted,"cooldown-tail Q actual controller and pose accepted");
            if(tick==1){firstPending=f.owner.PendingEvadeTicks();Need(!result.dashing,"cooldown-tail Q does not bypass cooldown");}
            if(result.dashing&&!actualStart)actualStart=tick;
        }
        std::cout<<"EVADE_BUFFER_COOLDOWN first_pending="<<firstPending<<" expected_start_tick="<<expectedStart
            <<" actual_start_tick="<<actualStart<<" window_ticks=16 single_Q=1\n"<<std::flush;
        Need(expectedStart>1&&expectedStart<=16,"cooldown-tail Q independent ready tick is inside original window");
        Need(firstPending==15&&actualStart==expectedStart&&f.owner.PendingEvadeTicks()==0,
            "cooldown-tail single Q must remain queued until a real dash starts within sixteen ticks");
    }
    EvadeBufferGreenControls();
    {
        Fixture f;f.explorer.nextFoot=1;
        Need(f.explorer.footSwings[0].Reset({-.12f,.1f,-.045f})&&
            f.explorer.footSwings[1].Reset({.12f,.1f,0}),"prepare repeated swing selection");
        Need(f.explorer.SelectSwingFoot({}, {0,0,1},true,false)==0,"lag override can keep original stance beyond one swing");
        Need(f.explorer.SelectSwingFoot({}, {0,0,-1},true,false)==1,"reverse selects newly trailing foot");
        Need(f.explorer.SelectSwingFoot({}, {0,0,1},true,true)==1,"turn preserves alternating priority");
        Need(f.explorer.SelectSwingFoot({}, {0,0,1},false,false)==1,"standing preserves priority");
    }
    for(float turn:{0.f,1.57079637f,-1.57079637f,3.14159265f}){
        Fixture f;float peak=0;unsigned flights=0;std::array<float,3> phasePeak{};std::array<unsigned,3> phaseFlights{};
        for(unsigned tick=0;tick<780;++tick){
            PlayerLocomotionInput input;input.running=tick<480||tick>=540;input.speedMultiplier=input.running?1.5f:1.f;
            input.movement=tick<240?Vec3{0,0,1}:tick<480?Vec3{std::sin(turn),0,std::cos(turn)}:
                tick<540?Vec3{}:Vec3{0,0,1};
            const bool grounded=f.actor.Grounded();const auto r=f.Step(input);
            if(!r.accepted)std::cerr<<"RUN_TURN_RESTART angle="<<turn<<" tick="<<tick<<" line="<<r.poseFailureLine<<'\n';
            Need(r.accepted,"running turn stop restart preserves body and pose constraints");
            peak=std::max(peak,Length(f.owner.Motor().Velocity()));flights+=grounded&&!f.actor.Grounded();
            if(tick<480||tick>=540){const unsigned stage=tick<240?0u:tick<480?1u:2u;
                phasePeak[stage]=std::max(phasePeak[stage],Length(f.owner.Motor().Velocity()));phaseFlights[stage]+=grounded&&!f.actor.Grounded();}
            if(tick==539)Need(Length(f.owner.Motor().Velocity())==0,"released movement really stops before restart");
        }
        Need(peak>6.1f&&flights>0,"support lookahead does not replace running with permanent slow walking");
        for(unsigned stage=0;stage<3;++stage){std::cout<<"RUN_PHASE angle="<<turn<<" phase="<<stage<<" peak="<<phasePeak[stage]<<" flights="<<phaseFlights[stage]<<'\n';
            Need(phasePeak[stage]>6.1f&&phaseFlights[stage]>0,"each initial turn and restart phase must really run");}
    }
    {
        // 真正停下但擺腳尚未結束時，輸入正規化不能改變制動計算中的實際速度。
        unsigned covered=0;
        for(unsigned initialTicks:{1u,8u,16u,30u}){
            Fixture f;PlayerLocomotionInput input;input.movement={0,0,1};
            for(unsigned i=0;i<initialTicks;++i)Need(f.Step(input).accepted,"restart initial movement");
            unsigned stopped=0;
            while(Length(f.owner.Motor().Velocity())>0&&stopped++<120)Need(f.Step().accepted,"restart actual braking");
            Need(Length(f.owner.Motor().Velocity())==0,"restart fixture really stopped");
            if(!f.explorer.footSwings[0].Active()&&!f.explorer.footSwings[1].Active())continue;
            ++covered;auto scaled=f;
            for(unsigned i=0;i<24;++i){
                auto twice=input;twice.movement={0,0,2};
                Need(f.Step(input).accepted&&scaled.Step(twice).accepted,"restart active swing accepted");
                Need(f.actor.StateHash()==scaled.actor.StateHash()&&f.owner.StateHash()==scaled.owner.StateHash()&&
                    PoseHash(f.explorer)==PoseHash(scaled.explorer),"normalized restart input has identical root and pose");
            }
        }
        Need(covered>=2,"at least two actual stopped active-swing restarts covered");
    }
    for(bool crouch:{false,true}){
        Fixture f;PlayerLocomotionInput input;input.movement={0,0,1};input.crouched=crouch;
        for(unsigned tick=0;tick<480;++tick){
            input.movement=tick<180?Vec3{0,0,1}:tick<360?Vec3{0,0,-1}:Vec3{tick%40<20?.2f:-.2f,0,-.2f};
            const auto next=f.explorer.nextFoot;
            auto projected=f.explorer.footSwings;
            std::array<FootSwingSample3D,2> endSamples{};
            for(unsigned side=0;side<2;++side)Need(projected[side].Advance(LocomotionMotor::FixedStep,endSamples[side]),"independent foot endpoint prediction");
            const auto r=f.Step(input);
            if(!r.accepted){
                std::cerr<<"GROUND_TURN crouch="<<crouch<<" tick="<<tick<<" line="<<r.poseFailureLine
                    <<" root="<<f.actor.Position().x<<','<<f.actor.Position().z<<" yaw="<<f.explorer.yaw<<'\n';
                for(unsigned s=0;s<2;++s){const auto& p=f.explorer.footSwings[s];
                    std::cerr<<"foot="<<s<<" target="<<f.explorer.footTargets[s].x<<','<<f.explorer.footTargets[s].y<<','<<f.explorer.footTargets[s].z
                        <<" landing="<<p.Landing().x<<','<<p.Landing().z<<" active="<<p.Active()<<" time="<<p.Elapsed()<<'/'<<p.Duration()<<" next="<<next<<'\n';}
            }
            Need(r.accepted,"ground turn full body and pose accepted");
            unsigned started=0;
            for(unsigned side=0;side<2;++side){
                const auto& path=f.explorer.footSwings[side];
                if(path.Active()&&path.Elapsed()==0){
                    ++started;
                    Need(!projected[0].Active()&&!projected[1].Active()&&endSamples[0].planted&&endSamples[1].planted,
                        "new foot starts only after both independent end-of-step samples really landed");
                    Need(Length(f.explorer.footTargets[side]-endSamples[side].positionWorld)<.00001f,
                        "new swing begins at old endpoint and does not consume prior frame time");
                }else{
                    Need(path.Elapsed()==projected[side].Elapsed()&&Length(f.explorer.footTargets[side]-endSamples[side].positionWorld)<.00001f,
                        "ongoing foot advances exactly once without loop-order dependence");
                }
            }
            Need(started<=1,"at most one new swing per common snapshot");
            const auto left=TransformPoint(f.explorer.pose.globalMatrices[11],{}),right=TransformPoint(f.explorer.pose.globalMatrices[14],{});
            Need(right.x-left.x>=.10f,"left/right feet do not cross during grounded turns");
            Need(!f.actor.Grounded()||f.explorer.planted[0]||f.explorer.planted[1],"ground gait retains a support foot");
        }
    }
    {
        Fixture f;PlayerLocomotionInput walk;walk.movement={0,0,1};
        const auto jumps=f.actor.Stats().jumps;
        for(unsigned tick=0;tick<240;++tick){
            auto r=f.Step(walk);
            if(!r.accepted||!f.actor.Grounded())std::cerr<<"WALK_WITHOUT_SHIFT tick="<<tick<<" accepted="<<r.accepted<<" grounded="<<f.actor.Grounded()<<'\n';
            Need(r.accepted&&f.actor.Grounded(),"ordinary W must not auto launch without running intent");
        }
        Need(f.actor.Stats().jumps==jumps,"walking has no run flight impulses");
    }
    {
        Fixture f;PlayerLocomotionInput run;run.movement={0,0,1};run.speedMultiplier=1.5f;run.running=true;
        unsigned flights=0,landings=0;
        for(unsigned tick=0;tick<360;++tick){
            const bool ground=f.actor.Grounded();auto r=f.Step(run);
            if(!r.accepted)std::cerr<<"SHIFT_RUN tick="<<tick<<" poseRejected="<<r.poseRejected<<'\n';
            Need(r.accepted,"Shift running must solve real body and pose");
            flights+=ground&&!f.actor.Grounded();landings+=!ground&&f.actor.Grounded();
        }
        Need(flights>5&&landings>5,"explicit running has actual flight and landings");
    }
    // 同一秒輸入分成 120/60/30 Hz 外幀；120 Hz 的真控制器與真角色每步都驗收。
    std::uint64_t referenceActor=0,referenceMotor=0,referencePose=0;
    for(unsigned hz:{120u,60u,30u}){
        Fixture f;PlayerLocomotionInput walk;walk.movement={0,0,1};walk.speedMultiplier=.45f;
        unsigned substeps=0;
        for(unsigned frame=0;frame<hz;++frame){auto r=f.Step(walk,1.f/hz);Need(r.accepted,"frequency walking actual pose");substeps+=r.steps;}
        Need(substeps==120&&f.owner.Motor().Tick()==120,"exact fixed tick count at all outer frequencies");
        if(hz==120){referenceActor=f.actor.StateHash();referenceMotor=f.owner.Motor().StateHash();referencePose=PoseHash(f.explorer);}
        else Need(f.actor.StateHash()==referenceActor&&f.owner.Motor().StateHash()==referenceMotor&&PoseHash(f.explorer)==referencePose,"frequency invariant physics and pose");
    }
    // 單一跳躍邊沿不可被 30Hz 外幀重複送四次，長跳中也不應追加一次跳躍。
    for(unsigned hz:{120u,60u,30u}){
        Fixture f;const auto jumps=f.actor.Stats().jumps;
        for(unsigned frame=0;frame<hz;++frame){PlayerLocomotionInput input;input.jumpPressed=frame==0;
            Need(f.Step(input,1.f/hz).accepted,"frequency one jump actual pose");}
        Need(f.actor.Stats().jumps==jumps+1,"exactly one jump edge");
    }
    {
        Fixture f;const auto model=f.explorer.model.bindMesh.vertices.data();
        const auto skeleton=f.explorer.model.skeleton.joints.data();
        PlayerLocomotionInput walk;walk.movement={0,0,1};walk.speedMultiplier=.45f;
        for(unsigned i=0;i<18;++i)Need(f.Step(walk).accepted,"setup active swing");
        const auto actor=f.actor.StateHash(),owner=f.owner.StateHash(),pose=PoseHash(f.explorer),evasion=EvasionHash(f.evasion);
        bool called=false;auto r=f.owner.Advance(f.actor,f.evasion,f.explorer,walk,1.f/30,{}, {},f.terrain,[&]{called=true;return false;});
        Need(called&&!r.accepted&&!r.poseRejected,"post pose camera rejection");
        Need(actor==f.actor.StateHash()&&owner==f.owner.StateHash()&&pose==PoseHash(f.explorer)&&evasion==EvasionHash(f.evasion),"all locomotion fields rollback");
        Need(model==f.explorer.model.bindMesh.vertices.data()&&skeleton==f.explorer.model.skeleton.joints.data(),"same model resource identity");
        called=false;r=f.owner.Advance(f.actor,f.evasion,f.explorer,walk,1.f/60,{}, {},f.terrain,[&]()->bool{called=true;throw std::runtime_error("camera");});
        Need(called&&!r.accepted&&actor==f.actor.StateHash()&&owner==f.owner.StateHash()&&pose==PoseHash(f.explorer),"throwing callback rollback");
        auto bad=walk;bad.movement.x=std::numeric_limits<float>::quiet_NaN();
        Need(!f.Step(bad).accepted&&actor==f.actor.StateHash()&&owner==f.owner.StateHash()&&pose==PoseHash(f.explorer),"invalid atomic");
    }
    {
        Fixture f;PlayerLocomotionInput jump;jump.jumpPressed=true;const auto jumps=f.actor.Stats().jumps;
        auto r=f.Step(jump,LocomotionMotor::FixedStep*.25f);Need(r.accepted&&r.steps==0,"zero substep stores edge");
        r=f.Step({},LocomotionMotor::FixedStep*.75f);
        Need(r.accepted&&r.steps==1&&f.actor.Stats().jumps==jumps+1,"queued jump consumed next tick");
        f.owner.Reset();Need(f.owner.Motor().Tick()==0&&f.owner.Remainder()==0&&f.owner.PendingEvadeTicks()==0,"reset clears all owner clocks");
    }
    {
        Fixture f;Need(f.actor.TryLaunchFromGround(1.2f),"actual short flight launch");
        PlayerLocomotionInput q;q.evadePressed=true;q.dodgeDirection={0,0,1};
        auto first=f.Step(q);Need(first.accepted&&!first.dashing&&f.owner.PendingEvadeTicks()>0,"Q retained while truly airborne");
        bool dash=false,landed=false;
        for(unsigned i=0;i<16&&!dash;++i){const bool grounded=f.actor.Grounded();auto r=f.Step();
            Need(r.accepted,"buffered Q actual pose");if(r.dashing){Need(grounded,"Q only starts grounded");dash=true;}landed=landed||grounded||f.actor.Grounded();}
        Need(dash&&landed&&f.owner.PendingEvadeTicks()==0,"short flight Q executed once after landing");
    }
    {
        Fixture f;PlayerLocomotionInput q;q.evadePressed=true;q.jumpPressed=true;q.dodgeDirection={0,0,1};
        Need(f.Step(q,0).accepted,"zero time queues edges");f.owner.Suspend();
        const auto jumps=f.actor.Stats().jumps;auto r=f.Step();
        Need(r.accepted&&!r.dashing&&f.actor.Stats().jumps==jumps&&f.owner.PendingEvadeTicks()==0,"suspend cancels queued edges");
        PlayerLocomotionInput crouch;crouch.crouched=true;crouch.jumpPressed=true;crouch.evadePressed=true;crouch.dodgeDirection={0,0,1};
        r=f.Step(crouch);Need(r.accepted&&!r.dashing&&f.actor.Stats().jumps==jumps&&f.actor.Settings().height==1.4f,"real crouch suppresses jump and dash");
        // 單幀上限與小數餘量有限；拒絕超量時間不能偷偷推進時鐘。
        Need(f.Step({},.1f).accepted,"maximum frame accepted");const auto state=f.owner.StateHash();
        Need(!f.Step({},.1001f).accepted&&f.owner.StateHash()==state,"oversize frame atomic");
    }
    std::cout<<"PASS PlayerLocomotion checks="<<checks<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" checks="<<checks<<'\n';return 1;}
