#pragma once
#include "MistwatchLocomotionMotor.h"
#include "MistwatchEvasion.h"
#include "MistwatchExplorer.h"
#include <optional>
#include <utility>

namespace Mistwatch {
struct PlayerLocomotionInput {
    Aevrynd::Vec3 movement{},dodgeDirection{};
    float speedMultiplier{1};
    bool jumpPressed{},evadePressed{},crouched{},running{};
};
struct PlayerLocomotionResult {
    bool accepted{},dashing{},lowCeiling{};
    unsigned steps{};
    bool poseRejected{};
    unsigned poseFailureLine{},failedSubstep{};
    Aevrynd::Vec3 failedPosition{};
    ExplorerFootPlacementDiagnostic footPlacementDiagnostic{};
};
// motion.accepted 才提交整個外幀；contact 僅為未提交候選診斷。
// 拒絕表示本次輸入尚未消耗，外層須保留並以相同輸入重試，不能當成 fatal 或假成功。
struct PlayerLocomotionDynamicResult {
    PlayerLocomotionResult motion;
    std::optional<Aevrynd::CharacterDynamicStepStatus3D> contact;
};
struct PlayerLocomotionMotionState {
    LocomotionMotorState motor;
    double remainder{};
    bool dodgeFlight{};
};
// 只備份 Update/PlaceFeet 可寫的姿態，不複製模型、材質、靴底網格或 GPU 身分。
struct PlayerLocomotionPoseState {
    Aevrynd::AnimationPose3D pose;
    float phase,yaw,requestedYaw;
    std::array<Aevrynd::Vec3,2> targets;
    std::array<bool,2> planted;
    Aevrynd::Vec3 feet;
    bool initialized;
    float distance,seconds;
    Aevrynd::Vec3 velocity;
    std::array<Aevrynd::FootSwingTrajectory3D,2> swings;
    std::array<float,2> facing,start,end;
    bool grounded;
    unsigned next;
    explicit PlayerLocomotionPoseState(const Explorer& e):pose(e.pose),phase(e.phase),yaw(e.yaw),requestedYaw(e.requestedYaw),
        targets(e.footTargets),planted(e.planted),feet(e.lastFeet),initialized(e.feetInitialized),
        distance(e.locomotionDistance),seconds(e.locomotionSeconds),velocity(e.locomotionVelocity),
        swings(e.footSwings),facing(e.footFacing),start(e.swingFacingStart),end(e.swingFacingEnd),
        grounded(e.gaitGrounded),next(e.nextFoot){}
    void Restore(Explorer& e)noexcept{
        e.pose=std::move(pose);e.phase=phase;e.yaw=yaw;e.requestedYaw=requestedYaw;e.footTargets=targets;e.planted=planted;
        e.lastFeet=feet;e.feetInitialized=initialized;e.locomotionDistance=distance;e.locomotionSeconds=seconds;
        e.locomotionVelocity=velocity;e.footSwings=swings;e.footFacing=facing;e.swingFacingStart=start;
        e.swingFacingEnd=end;e.gaitGrounded=grounded;e.nextFoot=next;
    }
};
class PlayerLocomotion {
public:
    void Reset()noexcept{motor_.Reset();remainder_=0;jump_=false;evadeTicks_=0;dodge_={};dodgeFlight_=false;}
    // 暫停不消耗世界時間，也不能在恢復時補跑暫停時間或延遲按鍵。
    void Suspend()noexcept{remainder_=0;jump_=false;evadeTicks_=0;dodge_={};motor_.ClearPendingJump();}
    const LocomotionMotor& Motor()const noexcept{return motor_;}
    double Remainder()const noexcept{return remainder_;}
    unsigned PendingEvadeTicks()const noexcept{return evadeTicks_;}
    PlayerLocomotionMotionState CaptureMotionState()const noexcept{return {motor_.CaptureMotionState(),remainder_,dodgeFlight_};}
    // 控制器、閃避與此快照須由載入預檢放在同一候選中；本方法不修改控制器或姿態。
    bool TryRestoreMotionState(const PlayerLocomotionMotionState& s,const Aevrynd::CharacterController3D& controller,
        const Evasion& evasion)noexcept{
        const float directionLength=std::hypot(evasion.direction.x,evasion.direction.z);
        if(!std::isfinite(s.remainder)||s.remainder<0||s.remainder>=double(LocomotionMotor::FixedStep)||
           !std::isfinite(evasion.direction.x)||!std::isfinite(evasion.direction.y)||!std::isfinite(evasion.direction.z)||
           evasion.direction.y!=0||!std::isfinite(directionLength)||
           (directionLength!=0&&std::abs(directionLength-1)>1.e-5f)||
           !std::isfinite(evasion.remaining)||evasion.remaining<0||evasion.remaining>Evasion::Duration||
           !std::isfinite(evasion.cooldown)||evasion.cooldown<0||evasion.cooldown>Evasion::Cooldown||
           ((evasion.remaining>0||evasion.cooldown>0)&&std::abs(directionLength-1)>1.e-5f)||
           (evasion.remaining>0&&(!s.dodgeFlight||evasion.cooldown+1.e-6f<evasion.remaining+Evasion::Cooldown-Evasion::Duration))||
           (s.dodgeFlight&&s.motor.runFlight))return false;
        auto motor=motor_;if(!motor.TryRestoreMotionState(s.motor,controller))return false;
        motor_=motor;remainder_=s.remainder;dodgeFlight_=s.dodgeFlight;
        jump_=false;evadeTicks_=0;dodge_={};return true;
    }
    std::uint64_t StateHash()const noexcept{
        auto h=motor_.StateHash();const auto add=[&](std::uint64_t v){h^=v;h*=1099511628211ull;};
        add(std::bit_cast<std::uint64_t>(remainder_));add(jump_);add(evadeTicks_);add(dodgeFlight_);
        add(std::bit_cast<std::uint32_t>(dodge_.x));add(std::bit_cast<std::uint32_t>(dodge_.y));add(std::bit_cast<std::uint32_t>(dodge_.z));return h;
    }
    // 回呼只負責外層鏡頭驗收；若回呼修改外部狀態，外層仍須備份並還原它們。
    // false 包含本次輸入佇列的完整回滾；不得把拒絕當成已消耗輸入的成功步。
    template<class Accept>
    PlayerLocomotionResult Advance(Aevrynd::CharacterController3D& controller,Evasion& evasion,Explorer& explorer,
        const PlayerLocomotionInput& input,float dt,std::span<const Aevrynd::BoxCollider3D> boxes,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,const Aevrynd::HeightField3D& terrain,Accept&& accept)noexcept{
        return AdvanceInternal(controller,evasion,explorer,input,dt,boxes,hulls,terrain,{},nullptr,std::forward<Accept>(accept));
    }
    template<class Accept>
    PlayerLocomotionDynamicResult Advance(Aevrynd::CharacterController3D& controller,Evasion& evasion,Explorer& explorer,
        const PlayerLocomotionInput& input,float dt,std::span<const Aevrynd::BoxCollider3D> boxes,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,const Aevrynd::HeightField3D& terrain,Accept&& accept,
        std::span<const Aevrynd::DynamicCylinder3D> dynamic)noexcept{
        if(dynamic.empty())return {Advance(controller,evasion,explorer,input,dt,boxes,hulls,terrain,std::forward<Accept>(accept)),std::nullopt};
        PlayerLocomotionDynamicResult result;
        result.motion=AdvanceInternal(controller,evasion,explorer,input,dt,boxes,hulls,terrain,dynamic,&result.contact,std::forward<Accept>(accept));
        return result;
    }
    // 新政策為末端明確opt-in；空span保持舊靜態整幀路徑，報告只在整幀accepted時寫入。
    template<class Accept>
    PlayerLocomotionDynamicResult Advance(Aevrynd::CharacterController3D& controller,Evasion& evasion,Explorer& explorer,
        const PlayerLocomotionInput& input,float dt,std::span<const Aevrynd::BoxCollider3D> boxes,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,const Aevrynd::HeightField3D& terrain,Accept&& accept,
        std::span<const Aevrynd::DynamicCylinder3D> dynamic,Aevrynd::CharacterDynamicVerticalResponse3D vertical,
        Aevrynd::CharacterDynamicContactState3D* appliedContact=nullptr)noexcept{
        using namespace Aevrynd;PlayerLocomotionDynamicResult result;
        if(vertical!=CharacterDynamicVerticalResponse3D::Reject&&vertical!=CharacterDynamicVerticalResponse3D::BlockOnly){
            result.contact=CharacterDynamicStepStatus3D::InvalidInput;return result;
        }
        if(dynamic.empty()){
            result.motion=Advance(controller,evasion,explorer,input,dt,boxes,hulls,terrain,std::forward<Accept>(accept));
            if(result.motion.accepted&&appliedContact)*appliedContact={};return result;
        }
        CharacterDynamicContactState3D contacts;
        result.motion=AdvanceInternal(controller,evasion,explorer,input,dt,boxes,hulls,terrain,dynamic,&result.contact,
            std::forward<Accept>(accept),vertical,&contacts);
        if(result.motion.accepted&&appliedContact)*appliedContact=contacts;return result;
    }
private:
    template<class Accept>
    PlayerLocomotionResult AdvanceInternal(Aevrynd::CharacterController3D& controller,Evasion& evasion,Explorer& explorer,
        const PlayerLocomotionInput& input,float dt,std::span<const Aevrynd::BoxCollider3D> boxes,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,const Aevrynd::HeightField3D& terrain,
        std::span<const Aevrynd::DynamicCylinder3D> dynamic,
        std::optional<Aevrynd::CharacterDynamicStepStatus3D>* contact,Accept&& accept,
        std::optional<Aevrynd::CharacterDynamicVerticalResponse3D> vertical={},
        Aevrynd::CharacterDynamicContactState3D* appliedContact=nullptr)noexcept{
        using namespace Aevrynd;
        PlayerLocomotionResult result;
        const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
        if(!std::isfinite(dt)||dt<0||dt>.1f||!finite(input.movement)||!finite(input.dodgeDirection)||
            !std::isfinite(input.speedMultiplier)||input.speedMultiplier<=0||input.speedMultiplier>4||
            !finite(evasion.direction)||!std::isfinite(evasion.remaining)||evasion.remaining<0||
            !std::isfinite(evasion.cooldown)||evasion.cooldown<0||explorer.model.skeleton.joints.size()!=15||
            !std::isfinite(explorer.yaw)||!std::isfinite(explorer.phase))return result;
        // 所有可能配置的備份必須在第一次寫入之前成功。
        try{
            auto priorController=controller;const auto priorEvasion=evasion;const auto priorOwner=*this;
            PlayerLocomotionPoseState priorPose(explorer);
            const auto restore=[&]()noexcept{controller=std::move(priorController);evasion=priorEvasion;*this=priorOwner;priorPose.Restore(explorer);};
            try{
                if(input.crouched){jump_=false;evadeTicks_=0;motor_.ClearPendingJump();}
                else {
                    jump_=jump_||input.jumpPressed;
                    if(input.evadePressed){evadeTicks_=16;dodge_=input.dodgeDirection;}
                }
                remainder_+=double(dt);
                CharacterDynamicContactState3D contacts;
                constexpr double h=double(LocomotionMotor::FixedStep),epsilon=1.e-8;
                for(unsigned sub=0;sub<12&&remainder_+epsilon>=h;++sub){
                    const auto previous=controller.Position();
                    const bool heightAccepted=vertical&&!dynamic.empty()?
                        controller.TrySetHeight(input.crouched?1.4f:1.8f,boxes,hulls,dynamic):
                        controller.TrySetHeight(input.crouched?1.4f:1.8f,boxes,hulls);
                    const bool crouched=controller.Settings().height<1.5f;
                    result.lowCeiling=result.lowCeiling||(!input.crouched&&!heightAccepted);
                    if(crouched){jump_=false;evadeTicks_=0;motor_.ClearPendingJump();}
                    // 閃避使用真實短騰空；若提早碰到較高支撐面，不把落地雙腳繼續拖行。
                    if(dodgeFlight_&&controller.Grounded()){evasion.remaining=0;dodgeFlight_=false;}
                    const float remainingBefore=evasion.remaining;
                    const bool wasDashing=remainingBefore>0;
                    bool attempt=evadeTicks_>0&&controller.Grounded()&&!crouched;
                    bool launchGrounded=controller.Grounded();
                    std::optional<CharacterController3D> verifiedLaunch;
                    const bool mayStart=attempt&&remainingBefore<=0&&evasion.cooldown<=LocomotionMotor::FixedStep&&
                        std::hypot(dodge_.x,dodge_.z)>.001f;
                    if(controller.Settings().maximumTerrainStandingGrade>0&&launchGrounded&&!dodgeFlight_&&(wasDashing||mayStart)){
                        const float flightTime=wasDashing?remainingBefore:Evasion::Duration;
                        const float launch=.5f*controller.Settings().gravity*(flightTime+2*LocomotionMotor::FixedStep);
                        // 只預檢脈衝，不先積分。只有閃避實際接單後才提交同一個已驗證候選。
                        if(!std::isfinite(launch)||launch<=0||!std::isfinite(controller.Settings().jumpSpeed)||launch>controller.Settings().jumpSpeed){restore();return {};}
                        verifiedLaunch.emplace(controller);
                        const bool launchAccepted=vertical&&!dynamic.empty()?
                            verifiedLaunch->TryLaunchFromGround(launch,boxes,hulls,terrain,dynamic):
                            verifiedLaunch->TryLaunchFromGround(launch,boxes,hulls,terrain);
                        if(!launchAccepted){
                            verifiedLaunch.reset();launchGrounded=false;attempt=false;
                            // 有效陡坡不接新閃避，也不延續過期接地的舊閃避；壞幾何仍須由正式 Step 拒絕。
                            if(wasDashing)evasion.remaining=0;
                        }
                    }
                    const auto motion=evasion.Tick(LocomotionMotor::FixedStep,attempt,launchGrounded,crouched,
                        attempt?dodge_:input.movement);
                    // 接地嘗試仍可能被冷卻拒絕；只有真正接單的新閃避才消耗邊沿，其餘合法步沿原期限到期。
                    if(attempt&&!wasDashing&&motion.dashing)evadeTicks_=0;
                    else if(evadeTicks_>0)--evadeTicks_;
                    if(motion.dashing&&!dodgeFlight_){
                        // 留兩個固定步作離散積分餘裕；限高、重力及碰撞仍由同一控制器驗證。
                        // 讀檔後已在空中的閃避只恢復追蹤；舊接地閃避僅以剩餘時間起跳，不重置冷卻或位移。
                        if(controller.Grounded()){
                            const float flightTime=wasDashing?remainingBefore:Evasion::Duration;
                            const float launch=.5f*controller.Settings().gravity*(flightTime+2*LocomotionMotor::FixedStep);
                            if(verifiedLaunch)controller=std::move(*verifiedLaunch);
                            else{
                                const bool launched=vertical&&!dynamic.empty()?controller.TryLaunchFromGround(launch,boxes,hulls,terrain,dynamic):
                                    controller.TryLaunchFromGround(launch);
                                if(!launched){restore();return {};}
                            }
                        }
                        dodgeFlight_=true;
                    }
                    const bool pressed=jump_;
                    // 閃避最後0.1秒的新跳躍邊沿保留至退出；起步同按與早段輸入仍取消。
                    // 只延續既有輸入佇列，實際起跳仍由馬達的接地、緩衝與碰撞規則驗證。
                    jump_=pressed&&wasDashing&&motion.dashing&&remainingBefore<=.1f;
                    LocomotionInput command{motion.dashing?motion.movement:input.movement,
                        motion.dashing?Evasion::SpeedMultiplier:crouched?.45f:input.speedMultiplier,
                        pressed&&!motion.dashing&&!crouched,crouched,motion.dashing,input.running};
                    command.poseHeading=explorer.yaw;command.constrainToPose=true;
                    if(controller.Grounded()&&!motion.dashing){
                        // 在擺脚完成前保留支撐腳的水平可達餘裕，轉好方向也不能立即拉走仍固定的腳。
                        auto direction=motor_.Velocity();const float actualSpeed=Length(direction);
                        float directionLength=actualSpeed;
                        if(directionLength<.0001f){direction=input.movement;direction.y=0;directionLength=Length(direction);}
                        if(directionLength>.0001f){direction=direction*(1/directionLength);
                            const auto constrainSupport=[&](Vec3 target,double horizon){
                                const auto delta=previous-target;
                                const float along=delta.x*direction.x+delta.z*direction.z;
                                const float across=delta.x*direction.z-delta.z*direction.x;
                                constexpr float supportRadius=.5f;
                                const float available=std::sqrt((std::max)(0.f,supportRadius*supportRadius-across*across))-along;
                                double limit=(std::max)(0.,double(available)/horizon);
                                if(limit<double(actualSpeed)){
                                    // 限制目標速度不等於瞬間制動。積分減速段及其後等速段，預留真正的煞車位移。
                                    constexpr double brake=LocomotionMotor::Deceleration;
                                    const double discriminant=brake*brake*horizon*horizon-2*brake*horizon*actualSpeed+2*brake*available;
                                    limit=discriminant>0?(std::max)(0.,double(actualSpeed)-brake*horizon+std::sqrt(discriminant)):0.;
                                }
                                command.maximumSpeed=(std::min)(command.maximumSpeed,float(limit));
                            };
                            for(unsigned swing=0;swing<2;++swing)if(explorer.footSwings[swing].Active()){
                                const double remaining=explorer.footSwings[swing].Duration()-explorer.footSwings[swing].Elapsed();
                                constrainSupport(explorer.footTargets[1-swing],remaining+2*LocomotionMotor::FixedStep);
                                // 先看下一步真正會選哪隻腳；拖後腳連續擺動時，原支撐腳不能提前解除約束。
                                const bool turning=std::abs(std::remainder(std::atan2(direction.x,direction.z)-explorer.yaw,6.2831853f))>.26179939f;
                                const unsigned nextSwing=explorer.SelectSwingFoot(previous,direction,true,turning);
                                // 減速會延長下一擺腳，用進入時可能的最低速度估計時長，不假設瞬間停住。
                                constexpr double travel=.3,acceleration=LocomotionMotor::Acceleration;
                                const double minimumSpeed=(std::max)(0.,double(actualSpeed)-LocomotionMotor::Deceleration*(remaining+2*LocomotionMotor::FixedStep));
                                const double nextDuration=std::clamp(2*travel/(minimumSpeed+
                                    std::sqrt(minimumSpeed*minimumSpeed+2*acceleration*travel)),.05,.22);
                                const double horizon=remaining+nextDuration+3*LocomotionMotor::FixedStep;
                                constrainSupport(explorer.footSwings[1-nextSwing].Landing(),horizon);
                                // 還在轉向時選腳分支可能改變，同時保留另一種支撐的餘裕。
                                if(turning)constrainSupport(explorer.footSwings[nextSwing].Landing(),horizon);
                            }
                        }
                    }
                    const float verticalBefore=controller.Velocity().y;
                    bool dynamicHeadBlocked=false; // 每個固定步重設，只接收本步成功馬達的頂接事件。
                    if(dynamic.empty()){
                        if(!motor_.Step(controller,command,boxes,hulls,terrain)){restore();return {};}
                    }else{
                        CharacterDynamicContactState3D subContacts;
                        const auto movement=vertical?motor_.Step(controller,command,boxes,hulls,terrain,dynamic,*vertical,&subContacts):
                            motor_.Step(controller,command,boxes,hulls,terrain,dynamic);
                        if(contact)*contact=movement.contact;
                        if(!movement.accepted){restore();return {};}
                        if(vertical)dynamicHeadBlocked=subContacts.blockedAbove;
                        if(vertical){contacts.blockedSide|=subContacts.blockedSide;contacts.blockedAbove|=subContacts.blockedAbove;
                            contacts.blockedBelow|=subContacts.blockedBelow;}
                    }
                    // 撞頂會截斷上升，不可仍按完整飛行時間推進；不改冷卻或免除傷害。
                    if(dodgeFlight_&&verticalBefore>0&&(controller.Velocity().y==0||dynamicHeadBlocked))evasion.remaining=0;
                    std::optional<float> stationaryFacing;
                    if(controller.Grounded()&&!crouched&&!motion.dashing&&
                       Length(motor_.Velocity())<=.0001f&&std::hypot(input.movement.x,input.movement.z)>.0001f)
                        stationaryFacing=motor_.CaptureMotionState().heading;
                    explorer.Update(controller.Position()-previous,LocomotionMotor::FixedStep,crouched,stationaryFacing);
                    unsigned failureLine=0;
                    ExplorerFootPlacementDiagnostic footPlacementDiagnostic;
                    if(!explorer.PlaceFeet(controller.Position(),controller.Grounded(),crouched,terrain,boxes,hulls,&failureLine,&footPlacementDiagnostic,
                        ExplorerFootPlacementMode::Advance,input.movement.x==0&&input.movement.z==0&&!motion.dashing)){
                        const auto failedPosition=controller.Position();
                        restore();result={};result.poseRejected=true;result.poseFailureLine=failureLine;
                        // 診斷來自回滾前的候選，不從 restore 後的人物重建。
                        result.footPlacementDiagnostic=footPlacementDiagnostic;
                        result.failedSubstep=sub;result.failedPosition=failedPosition;return result;
                    }
                    result.dashing=result.dashing||motion.dashing;++result.steps;
                    remainder_=(std::max)(0.,remainder_-h);
                }
                if(remainder_+epsilon>=h){restore();return {};}
                if(!std::forward<Accept>(accept)()){restore();return {};}
                result.accepted=true;if(appliedContact)*appliedContact=contacts;return result;
            }catch(...){restore();return {};}
        }catch(...){return {};}
    }
private:
    LocomotionMotor motor_;
    double remainder_{};
    bool jump_{};
    unsigned evadeTicks_{};
    Aevrynd::Vec3 dodge_{};
    bool dodgeFlight_{};
};
}
