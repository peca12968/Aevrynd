#pragma once
#include "MistwatchPlayerLocomotion.h"
#include "MistwatchPairContact.h"
#include "MistwatchBoar.h"
#include "MistwatchCamera.h"
#include "MistwatchTuningInputRepeat.h"
#include "MistwatchActionFeedback.h"
#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <string>
#include <type_traits>

namespace Mistwatch {
struct BoarWritableState {
    Aevrynd::AnimationPose3D pose;
    float yaw{},cycle{},displacement{},brace{};
    std::array<Aevrynd::Vec3,4> targets{};std::array<bool,4> planted{};
    Aevrynd::Vec3 feet{};bool initialized{};
    void Capture(const Boar& b){pose=b.pose;yaw=b.yaw;cycle=b.cycle;displacement=b.displacement;brace=b.brace;
        targets=b.footTargets;planted=b.planted;feet=b.lastFeet;initialized=b.initialized;}
    void Restore(Boar& b)noexcept{using std::swap;swap(pose,b.pose);b.yaw=yaw;b.cycle=cycle;b.displacement=displacement;b.brace=brace;
        b.footTargets=targets;b.planted=planted;b.lastFeet=feet;b.initialized=initialized;}
};
struct ExplorerWritableState {
    Aevrynd::AnimationPose3D pose,deathStart,deathEnd;
    float phase{},yaw{},requestedYaw{},distance{},seconds{},deathElapsed{},deathClearance{};
    std::array<Aevrynd::Vec3,2> targets{};std::array<bool,2> planted{};
    Aevrynd::Vec3 feet{},velocity{};bool initialized{},grounded{},deathActive{},deathGrounded{},deathLimited{};
    std::array<Aevrynd::FootSwingTrajectory3D,2> swings;
    std::array<float,2> facing{},start{},end{};std::array<float,4> deathGaps{};unsigned next{};
    void Capture(const Explorer& e){pose=e.pose;phase=e.phase;yaw=e.yaw;requestedYaw=e.requestedYaw;targets=e.footTargets;planted=e.planted;
        feet=e.lastFeet;initialized=e.feetInitialized;distance=e.locomotionDistance;seconds=e.locomotionSeconds;velocity=e.locomotionVelocity;
        swings=e.footSwings;facing=e.footFacing;start=e.swingFacingStart;end=e.swingFacingEnd;grounded=e.gaitGrounded;next=e.nextFoot;
        deathStart=e.deathStart;deathEnd=e.deathEnd;deathElapsed=e.deathElapsed;deathActive=e.deathActive;deathGrounded=e.deathGroundedBasis;
        deathLimited=e.deathLimited;deathClearance=e.deathMinimumClearance;deathGaps=e.deathSupportGaps;}
    void Restore(Explorer& e)noexcept{using std::swap;swap(pose,e.pose);e.phase=phase;e.yaw=yaw;e.requestedYaw=requestedYaw;e.footTargets=targets;e.planted=planted;
        e.lastFeet=feet;e.feetInitialized=initialized;e.locomotionDistance=distance;e.locomotionSeconds=seconds;e.locomotionVelocity=velocity;
        e.footSwings=swings;e.footFacing=facing;e.swingFacingStart=start;e.swingFacingEnd=end;e.gaitGrounded=grounded;e.nextFoot=next;
        swap(deathStart,e.deathStart);swap(deathEnd,e.deathEnd);e.deathElapsed=deathElapsed;e.deathActive=deathActive;e.deathGroundedBasis=deathGrounded;
        e.deathLimited=deathLimited;e.deathMinimumClearance=deathClearance;e.deathSupportGaps=deathGaps;}
};
struct PlayableFrameRefs {
    Aevrynd::CharacterController3D& controller;PlayerLocomotion& player;Evasion& evasion;Session& session;Encounter& enemy;Explorer& explorer;Boar& boar;
    PairContact::State& pair;OrbitCamera& orbit;Aevrynd::ThirdPersonCameraMotion3D& camera;
    Aevrynd::ThirdPersonFramingResult3D& framing;Aevrynd::ThirdPersonCameraMotionResult3D& cameraResult;Aevrynd::Camera3D& rendered;
    TuningInputRepeat& tuning;ActionFeedbackToast& feedback;std::string& message;float& water;
    unsigned& fixedTicks;unsigned& captureTicks;float& displacement;float& occlusion;bool& lowRoofObserved;
};
// 持久scratch重用pose/string/vector容量；不保存模型、材質、terrain或世界碰撞清單。
struct PlayableFrameScratch {
    Aevrynd::CharacterController3D controller;PlayerLocomotion player;Evasion evasion;Session session;Encounter enemy;
    ExplorerWritableState explorer;BoarWritableState boar;PairContact::State pair;OrbitCamera orbit;
    Aevrynd::ThirdPersonCameraMotion3D camera;Aevrynd::ThirdPersonFramingResult3D framing;
    Aevrynd::ThirdPersonCameraMotionResult3D cameraResult;Aevrynd::Camera3D rendered;
    TuningInputRepeat tuning;ActionFeedbackToast feedback;std::string message;float water{},displacement{},occlusion{};
    unsigned fixedTicks{},captureTicks{};bool lowRoofObserved{};
    bool Capture(PlayableFrameRefs r)noexcept{try{
        controller=r.controller;player=r.player;evasion=r.evasion;session=r.session;enemy=r.enemy;explorer.Capture(r.explorer);boar.Capture(r.boar);
        pair=r.pair;orbit=r.orbit;camera=r.camera;framing=r.framing;cameraResult=r.cameraResult;rendered=r.rendered;tuning=r.tuning;feedback=r.feedback;
        message=r.message;water=r.water;fixedTicks=r.fixedTicks;captureTicks=r.captureTicks;displacement=r.displacement;occlusion=r.occlusion;lowRoofObserved=r.lowRoofObserved;
        return true;
    }catch(...){return false;}}
    void Restore(PlayableFrameRefs r)noexcept{
        static_assert(std::is_nothrow_swappable_v<Aevrynd::CharacterController3D>);
        using std::swap;swap(controller,r.controller);r.player=player;r.evasion=evasion;r.session=session;r.enemy=enemy;
        explorer.Restore(r.explorer);boar.Restore(r.boar);r.pair=pair;r.orbit=orbit;r.camera=camera;r.framing=framing;r.cameraResult=cameraResult;r.rendered=rendered;
        r.tuning=tuning;r.feedback=feedback;r.message.swap(message);r.water=water;r.fixedTicks=fixedTicks;r.captureTicks=captureTicks;
        r.displacement=displacement;r.occlusion=occlusion;r.lowRoofObserved=lowRoofObserved;
    }
};
class PlayableFrameGuard {
public:
    PlayableFrameGuard(PlayableFrameRefs refs,PlayableFrameScratch& scratch)noexcept:refs_(refs),scratch_(scratch){}
    PlayableFrameGuard(const PlayableFrameGuard&)=delete;
    ~PlayableFrameGuard(){Rollback();}
    void Commit()noexcept{active_=false;}
    void Rollback()noexcept{if(active_){scratch_.Restore(refs_);active_=false;}}
private:
    PlayableFrameRefs refs_;PlayableFrameScratch& scratch_;bool active_{true};
};
}
