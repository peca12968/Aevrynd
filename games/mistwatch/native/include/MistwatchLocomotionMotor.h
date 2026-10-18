#pragma once
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/ConvexAabbSweep3D.h>
#include <bit>
#include <optional>

namespace Mistwatch {
enum class LocomotionPhase {Standing,GroundMotion,RunSupport,RunFlight,JumpOrFall};
struct LocomotionMotorState {
    Aevrynd::Vec3 velocity{};
    float heading{};
    bool headingReady{};
    LocomotionPhase phase{LocomotionPhase::Standing};
    unsigned supportTicks{};
    std::uint64_t tick{};
    bool runFlight{},wasDashing{};
};
struct LocomotionInput {
    Aevrynd::Vec3 movement{};
    float speedMultiplier{1};
    bool jump{},crouched{},dashing{},enableRunFlight{};
    float poseHeading{};
    bool constrainToPose{};
    float maximumSpeed{std::numeric_limits<float>::max()};
};
// contact 是未提交候選的控制器診斷；accepted 才代表完整馬達交易已提交。
struct LocomotionDynamicStepResult {
    bool accepted{};
    std::optional<Aevrynd::CharacterDynamicStepStatus3D> contact;
};
// 遊戲層固定120Hz輸入與接觸事件所有者；不自行改寫位置，也不驅動第二套重力。
// 呼叫端負責按鍵邊沿、暫停及把此物件與姿態/鏡頭一起作整幀交易。
class LocomotionMotor {
public:
    static constexpr float FixedStep=1.f/120.f,Acceleration=12.f,Deceleration=18.f,CrouchDeceleration=30.f;
    static constexpr float TurnRate=4.71238898f;
    void Reset()noexcept{velocity_={};heading_=0;headingReady_=false;phase_=LocomotionPhase::Standing;supportTicks_=0;tick_=0;runFlight_=false;wasDashing_=false;pendingJumpTicks_=0;}
    void ClearPendingJump()noexcept{pendingJumpTicks_=0;}
    Aevrynd::Vec3 Velocity()const noexcept{return velocity_;}
    LocomotionPhase Phase()const noexcept{return phase_;}
    unsigned SupportTicks()const noexcept{return supportTicks_;}
    std::uint64_t Tick()const noexcept{return tick_;}
    // 實際移動狀態與待處理按鍵分離；讀檔不重播尚未消耗的跳躍邊沿。
    LocomotionMotorState CaptureMotionState()const noexcept{
        return {velocity_,heading_,headingReady_,phase_,supportTicks_,tick_,runFlight_,wasDashing_};
    }
    bool TryRestoreMotionState(const LocomotionMotorState& s,const Aevrynd::CharacterController3D& controller)noexcept{
        const auto finite=[](Aevrynd::Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
        const auto& settings=controller.Settings();
        if(!std::isfinite(settings.maximumTerrainStandingGrade)||settings.maximumTerrainStandingGrade<0||settings.maximumTerrainStandingGrade>16)return false;
        // 滑坡速度來自控制器重力，不是輸入倍率；啟用時沿用控制器的有限合成速度界。
        const float slideLimit=settings.maximumTerrainStandingGrade>0?
            float(std::hypot(double(settings.maximumFallSpeed),double(settings.moveSpeed)*4)):0;
        if(settings.maximumTerrainStandingGrade>0&&(!std::isfinite(settings.maximumFallSpeed)||settings.maximumFallSpeed<=0||!std::isfinite(slideLimit)))return false;
        const double horizontalLimit=settings.maximumTerrainStandingGrade>0?
            double(std::nextafter(slideLimit,std::numeric_limits<float>::infinity())):double(settings.moveSpeed*4.00001f);
        if(!finite(s.velocity)||s.velocity.y!=0||!finite(controller.Velocity())||
           s.velocity.x!=controller.Velocity().x||s.velocity.z!=controller.Velocity().z||
           !std::isfinite(s.heading)||std::abs(s.heading)>3.14159275f||
           (!s.headingReady&&(s.heading!=0||Aevrynd::Length(s.velocity)>0))||
           !std::isfinite(controller.Settings().moveSpeed)||controller.Settings().moveSpeed<=0||
           std::hypot(double(s.velocity.x),double(s.velocity.z))>horizontalLimit||
           s.supportTicks>10||s.tick==std::numeric_limits<std::uint64_t>::max())return false;
        const bool ground=controller.Grounded();const float speed=Aevrynd::Length(s.velocity);
        if(!std::isfinite(speed))return false;
        switch(s.phase){
        case LocomotionPhase::Standing:if(!ground||speed>.001f||s.runFlight||s.supportTicks)return false;break;
        case LocomotionPhase::GroundMotion:if(!ground||speed<=.001f||s.runFlight||s.supportTicks)return false;break;
        case LocomotionPhase::RunSupport:if(!ground||speed<5.4f||s.runFlight)return false;break;
        case LocomotionPhase::RunFlight:if(ground||!s.runFlight||s.supportTicks)return false;break;
        case LocomotionPhase::JumpOrFall:if(ground||s.runFlight||s.supportTicks)return false;break;
        default:return false;
        }
        velocity_=s.velocity;heading_=s.heading;headingReady_=s.headingReady;phase_=s.phase;
        supportTicks_=s.supportTicks;tick_=s.tick;runFlight_=s.runFlight;wasDashing_=s.wasDashing;
        pendingJumpTicks_=0;return true;
    }
    std::uint64_t StateHash()const noexcept{
        std::uint64_t h=1469598103934665603ull;
        const auto add=[&](std::uint64_t v){h^=v;h*=1099511628211ull;};
        add(std::bit_cast<std::uint32_t>(velocity_.x));add(std::bit_cast<std::uint32_t>(velocity_.z));
        add(std::bit_cast<std::uint32_t>(heading_));
        add(headingReady_);
        add(static_cast<unsigned>(phase_));add(supportTicks_);add(tick_);add(runFlight_);add(wasDashing_);add(pendingJumpTicks_);return h;
    }
    bool Step(Aevrynd::CharacterController3D& controller,LocomotionInput input,
        std::span<const Aevrynd::BoxCollider3D> boxes,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,const Aevrynd::HeightField3D& terrain){
        return StepInternal(controller,input,boxes,hulls,terrain,{},nullptr);
    }
    // 圓柱只借用本次呼叫，不加入地面、跨階、起跳支撐或姿態 IK。
    LocomotionDynamicStepResult Step(Aevrynd::CharacterController3D& controller,LocomotionInput input,
        std::span<const Aevrynd::BoxCollider3D> boxes,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,const Aevrynd::HeightField3D& terrain,
        std::span<const Aevrynd::DynamicCylinder3D> dynamic){
        if(dynamic.empty())return {Step(controller,input,boxes,hulls,terrain),std::nullopt};
        LocomotionDynamicStepResult result;
        result.accepted=StepInternal(controller,input,boxes,hulls,terrain,dynamic,&result.contact);return result;
    }
    // 垂直接觸政策必須由呼叫端明確選擇；舊overload仍沿原Reject路徑。
    LocomotionDynamicStepResult Step(Aevrynd::CharacterController3D& controller,LocomotionInput input,
        std::span<const Aevrynd::BoxCollider3D> boxes,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,const Aevrynd::HeightField3D& terrain,
        std::span<const Aevrynd::DynamicCylinder3D> dynamic,Aevrynd::CharacterDynamicVerticalResponse3D vertical,
        Aevrynd::CharacterDynamicContactState3D* appliedContact=nullptr){
        using namespace Aevrynd;LocomotionDynamicStepResult result;
        if(vertical!=CharacterDynamicVerticalResponse3D::Reject&&vertical!=CharacterDynamicVerticalResponse3D::BlockOnly){
            result.contact=CharacterDynamicStepStatus3D::InvalidInput;return result;
        }
        if(dynamic.empty()){
            result.accepted=Step(controller,input,boxes,hulls,terrain);
            if(result.accepted&&appliedContact)*appliedContact={};return result;
        }
        CharacterDynamicContactState3D contacts;
        result.accepted=StepInternal(controller,input,boxes,hulls,terrain,dynamic,&result.contact,vertical,&contacts);
        if(result.accepted&&appliedContact)*appliedContact=contacts;return result;
    }
private:
    bool StepInternal(Aevrynd::CharacterController3D& controller,LocomotionInput input,
        std::span<const Aevrynd::BoxCollider3D> boxes,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,const Aevrynd::HeightField3D& terrain,
        std::span<const Aevrynd::DynamicCylinder3D> dynamic,
        std::optional<Aevrynd::CharacterDynamicStepStatus3D>* contact,
        std::optional<Aevrynd::CharacterDynamicVerticalResponse3D> vertical={},
        Aevrynd::CharacterDynamicContactState3D* appliedContact=nullptr){
        using namespace Aevrynd;
        const auto finite=[](Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
        if(!finite(input.movement)||!std::isfinite(input.poseHeading)||!std::isfinite(input.maximumSpeed)||input.maximumSpeed<0||!std::isfinite(input.speedMultiplier)||input.speedMultiplier<=0||input.speedMultiplier>4||
           !std::isfinite(controller.Settings().moveSpeed)||controller.Settings().moveSpeed<=0||
           !finite(controller.Position())||!finite(controller.Velocity())||
           !std::isfinite(controller.Settings().radius)||controller.Settings().radius<=0||
           !std::isfinite(controller.Settings().height)||controller.Settings().height<=0||
           !std::isfinite(controller.Settings().gravity)||controller.Settings().gravity<0||
           !std::isfinite(controller.Settings().maximumFallSpeed)||controller.Settings().maximumFallSpeed<=0||
           tick_==std::numeric_limits<std::uint64_t>::max())return false;
        for(const auto& box:boxes)if(!finite(box.center)||!finite(box.extents)||box.extents.x<0||box.extents.y<0||box.extents.z<0)return false;
        try{
            auto candidate=controller;auto next=*this;
            Vec3 direction{input.movement.x,0,input.movement.z};const float length=Length(direction);
            if(!std::isfinite(length))return false;
            if(length>1)direction=direction*(1/length);
            const float base=controller.Settings().moveSpeed;
            Vec3 desired=direction*(base*input.speedMultiplier);
            const float requestedSpeed=Length(desired);
            if(!input.dashing&&!next.wasDashing_&&requestedSpeed>.0001f){
                // 急轉先降低根移動速度並轉過方向，再恢復步速；不能穿過零速後瞬間反向拉走雙腳。
                constexpr float tau=6.2831853f;
                const float target=std::atan2(desired.x,desired.z);
                // 重試／讀檔後首次移動不能預設世界北方，否則會先朝按鍵的反方向移動。
                const bool hadHeading=next.headingReady_;
                if(!next.headingReady_){next.heading_=target;next.headingReady_=true;}
                float turn=std::remainder(target-next.heading_,tau);
                // 正反半圈在數值精度內等長時固定向正側轉，避免相機矩陣尾差令左右選擇翻轉。
                // 僅處理半圈的浮點同分，不擴大一般轉角，也不跳過每步有限轉速。
                constexpr float pi=tau*.5f,tieTolerance=4*std::numeric_limits<float>::epsilon();
                if(std::abs(std::abs(turn)-pi)<=tieTolerance)turn=pi;
                next.heading_=std::remainder(next.heading_+std::clamp(turn,-TurnRate*FixedStep,TurnRate*FixedStep),tau);
                const float remaining=std::abs(std::remainder(target-next.heading_,tau));
                const bool poseTurning=input.constrainToPose&&std::abs(std::remainder(target-input.poseHeading,tau))>.26179939f;
                // 真站定後先經姿態與足部支撐轉身，不能沿舊朝向反著新按鍵起步。
                const bool pivotFromRest=hadHeading&&poseTurning&&!input.crouched&&!input.jump&&next.pendingJumpTicks_==0&&
                    candidate.Grounded()&&Length(next.velocity_)<=.0001f;
                const float speed=pivotFromRest?0.f:remaining>.78539816f||poseTurning?(std::min)(requestedSpeed,.6f):requestedSpeed;
                desired={std::sin(next.heading_)*speed,0,std::cos(next.heading_)*speed};
            }else if(requestedSpeed>.0001f){next.heading_=std::atan2(desired.x,desired.z);next.headingReady_=true;}
            if(!input.dashing&&!next.wasDashing_){const float speed=Length(desired);
                if(speed>input.maximumSpeed)desired=desired*(input.maximumSpeed/speed);}
            const Vec3 difference=desired-next.velocity_;
            const float distance=Length(difference);
            if(!finite(desired)||!finite(difference)||!std::isfinite(distance)||!std::isfinite(Length(desired)))return false;
            // 接地蹲行較快收速；空中及一般行走沿用原有慣性，不瞬移或硬夾速度。
            const float rate=Length(desired)<Length(next.velocity_)?
                (input.crouched&&candidate.Grounded()?CrouchDeceleration:Deceleration):Acceleration;
            // 閃避及退出當幀沿用舊即時速度；末幀fraction不得變成額外滑行慣性。
            next.velocity_=input.dashing||next.wasDashing_?desired:distance<=rate*FixedStep?desired:next.velocity_+difference*(rate*FixedStep/distance);
            next.wasDashing_=input.dashing;
            // 短騰空時按下的單幀跳躍保留16tick；只在真接地後消耗，不做空中二段跳。
            if(input.crouched||input.dashing)next.pendingJumpTicks_=0;
            else if(input.jump)next.pendingJumpTicks_=16;
            const bool playerJump=(candidate.Grounded()||candidate.CanJumpFromRecentGround())&&next.pendingJumpTicks_>0;
            const bool runRequested=input.enableRunFlight&&!input.crouched&&!input.dashing&&!playerJump&&
                Length(desired)>=5.4f&&Length(next.velocity_)>=5.4f;
            const bool wasGrounded=candidate.Grounded();
            if(playerJump&&wasGrounded){next.runFlight_=false;next.supportTicks_=0;}
            else if(runRequested&&wasGrounded&&next.supportTicks_>=10){
                // 舊 grounded 不能授權從真陡坡起跳；交由控制器重驗本輪世界支撐。
                const bool slopePolicy=candidate.Settings().maximumTerrainStandingGrade>0;
                const bool launched=vertical&&!dynamic.empty()?candidate.TryLaunchFromGround(1.2f,boxes,hulls,terrain,dynamic):
                    slopePolicy?candidate.TryLaunchFromGround(1.2f,boxes,hulls,terrain):candidate.TryLaunchFromGround(1.2f);
                if(!launched&&!slopePolicy)return false;
                if(launched){next.runFlight_=true;next.supportTicks_=0;}
            }
            const float multiplier=(std::max)(1.f,Length(next.velocity_)/base);
            const auto priorJumps=candidate.Stats().jumps;
            CharacterDynamicContactState3D contacts;
            if(dynamic.empty()){
                if(!candidate.Step({next.velocity_*(1/(base*multiplier)),playerJump,multiplier},boxes,hulls,terrain,FixedStep))return false;
            }else{
                const auto status=vertical?candidate.Step({next.velocity_*(1/(base*multiplier)),playerJump,multiplier},
                    boxes,hulls,terrain,dynamic,FixedStep,*vertical,&contacts):
                    candidate.Step({next.velocity_*(1/(base*multiplier)),playerJump,multiplier},boxes,hulls,terrain,dynamic,FixedStep);
                if(contact)*contact=status;
                if(status!=CharacterDynamicStepStatus3D::Applied)return false;
            }
            if(!finite(candidate.Position())||!finite(candidate.Velocity()))return false;
            // 舊Grounded仍須由控制器重驗支撐；只有正式起跳成功才消耗，合法未起跳的tick照常到期。
            if(playerJump&&candidate.Stats().jumps!=priorJumps)next.pendingJumpTicks_=0;
            else if(next.pendingJumpTicks_>0)--next.pendingJumpTicks_;
            // 撞牆後保留碰撞解算的實際速度，不能讓motor繼續累積穿牆意圖。
            next.velocity_={candidate.Velocity().x,0,candidate.Velocity().z};
            // 首次無按鍵滑落也會產生實際水平速度；保存可立即還原的正規方向狀態。
            if(candidate.Settings().maximumTerrainStandingGrade>0&&!next.headingReady_&&Length(next.velocity_)>0){
                next.heading_=std::atan2(next.velocity_.x,next.velocity_.z);next.headingReady_=true;
            }
            if(candidate.Grounded()){
                const bool landed=!wasGrounded||next.runFlight_;
                next.runFlight_=false;
                if(runRequested&&Length(next.velocity_)>=5.4f){next.supportTicks_=landed?0:(std::min)(10u,next.supportTicks_+1);next.phase_=LocomotionPhase::RunSupport;}
                else {next.supportTicks_=0;next.phase_=Length(next.velocity_)>.001f?LocomotionPhase::GroundMotion:LocomotionPhase::Standing;}
            }else{next.supportTicks_=0;next.phase_=next.runFlight_?LocomotionPhase::RunFlight:LocomotionPhase::JumpOrFall;}
            ++next.tick_;controller=std::move(candidate);*this=next;
            if(appliedContact)*appliedContact=contacts;return true;
        }catch(...){return false;}
    }
private:
    Aevrynd::Vec3 velocity_{};
    float heading_{};
    bool headingReady_{};
    LocomotionPhase phase_{LocomotionPhase::Standing};
    unsigned supportTicks_{};
    std::uint64_t tick_{};
    bool runFlight_{};
    bool wasDashing_{};
    unsigned pendingJumpTicks_{};
};
}
