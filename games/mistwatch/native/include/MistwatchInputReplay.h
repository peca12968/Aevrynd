#pragma once
#include "MistwatchSession.h"
#include "MistwatchEncounter.h"
#include "MistwatchEvasion.h"
#include "MistwatchSaveGame.h"
#include "MistwatchLocomotionMotor.h"
#include <Aevrynd/Input.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string_view>

namespace Mistwatch {
struct DashJumpReplayEvidence {
    std::uint64_t jumpsBefore{},landingsBefore{},jumpsAfter{},landingsAtJump{},landingsAfter{};
    unsigned qTick{},spaceTick{},fullJumpTick{},completeTick{};
    float remainingAtSpace{},peakVelocityY{},peakHeight{},startHeight{},travel{};
    bool qObserved{},fullJumpObserved{};
};
// 與玩家可見訊號共用的有限搜尋門檻；未找到鎖定不能無限轉動。
inline constexpr bool CompletionScanMayTurn(unsigned turns,unsigned signal){return turns<16&&signal<100;}
// 新陡坡路線借用真世界唯讀觀測；沒有世界資料的舊路線仍要求全程接地。
struct WestCliffReplayWorld {
    const Aevrynd::CharacterController3D& controller;
    const Aevrynd::HeightField3D& terrain;
    std::span<const Aevrynd::BoxCollider3D> boxes;
    std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls;
    bool motionAccepted{};
    bool Validate(const SaveGame* saved,const Session& session,bool grounded,float height,Aevrynd::Vec3 velocity)const{
        if(!motionAccepted||!saved||!saved->motion||!saved->Valid()||saved->session.Serialize()!=session.Serialize()||
            controller.Settings().maximumTerrainStandingGrade!=1||controller.Settings().maximumTerrainUphillGrade!=1)return false;
        const auto& c=saved->motion->controller;const auto actual=controller.CaptureMotionState();
        const auto same=[](Aevrynd::Vec3 a,Aevrynd::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
        if(!same(c.feet,actual.feet)||!same(c.velocity,actual.velocity)||!same(c.velocity,velocity)||
            c.height!=actual.height||c.height!=height||c.yaw!=actual.yaw||c.grounded!=actual.grounded||c.grounded!=grounded||
            saved->motion->locomotion.dodgeFlight||saved->motion->locomotion.motor.runFlight||
            saved->motion->locomotion.motor.wasDashing||c.velocity.y>0)return false;
        // 此路段從不發送跳躍／奔跑；下降或貼陡面滑落必須通過完整身體及真地形核驗。
        // 在副本還原同一快照，不 Step、不吸地、不修改正式控制器。
        Aevrynd::CharacterController3D candidate(controller.Settings());
        if(!candidate.TryRestoreMotionState(c,boxes,hulls,terrain))return false;
        PlayerLocomotion owner;
        return owner.TryRestoreMotionState(saved->motion->locomotion,candidate,saved->evasion);
    }
};
// 只產生正式處理器的按鍵；不持有可修改的世界、角色、鏡頭或任務狀態。
// 獨立 Input 預設有焦點；此測試不宣稱驗證作業系統焦點或實體鍵盤。
class InputReplay {
public:
    // 唯讀路線資訊供配置驗證；讀取真正回放陣列，不能另造一份看似一致的路點。
    static constexpr Aevrynd::Vec3 BridgeSupplyApproach(unsigned index) {
        if(index>=3)throw std::invalid_argument("Unknown replay supply");
        const auto& point=bridgePoints_[5+index*3];return {point.x,0,point.z};
    }
    static constexpr Aevrynd::Vec3 BridgeSupplyReturn(unsigned index) {
        if(index>=3)throw std::invalid_argument("Unknown replay supply");
        const auto& point=bridgePoints_[6+index*3];return {point.x,0,point.z};
    }
    bool EnableDashJumpRoute(){
        if(groveRoute_||ticks_||resumeSaveRoute_||dashJumpRoute_||bridgeRoute_||combatRoute_||deathRetryRoute_||checkpointRoute_||powerRoute_||completionRoute_||westCliffRoute_)return false;
        dashJumpRoute_=checkpointRoute_=true;return true;
    }
    bool DashJumpRoute()const{return dashJumpRoute_;}
    bool EnableSprintFlowRoute(){if(!EnableDashJumpRoute())return false;sprintFlowRoute_=true;return true;}
    bool SprintFlowRoute()const{return sprintFlowRoute_;}
    const std::array<DashJumpReplayEvidence,3>& DashJumpEvidence()const{return dashJumpEvidence_;}
    bool EnablePivotCheckpointRoute(){
        if(groveRoute_||dashJumpRoute_||resumeSaveRoute_||ticks_||bridgeRoute_||combatRoute_||deathRetryRoute_||checkpointRoute_||powerRoute_||completionRoute_||westCliffRoute_)return false;
        pivotCheckpointRoute_=checkpointRoute_=true;return true;
    }
    bool PivotCheckpointRoute()const{return pivotCheckpointRoute_;}
    bool EnableBridgeRoute(){if(groveRoute_||resumeSaveRoute_||dashJumpRoute_||ticks_!=0||westCliffRoute_)return false;bridgeRoute_=true;return true;}
    bool BridgeRoute()const{return bridgeRoute_;}
    bool EnableCombatRoute(){if(groveRoute_||resumeSaveRoute_||dashJumpRoute_||ticks_!=0||deathRetryRoute_||westCliffRoute_)return false;combatRoute_=bridgeRoute_=true;return true;}
    bool CombatRoute()const{return combatRoute_;}
    bool EnablePowerRoute(){if(groveRoute_||resumeSaveRoute_||dashJumpRoute_||ticks_!=0||deathRetryRoute_||checkpointRoute_||westCliffRoute_)return false;powerRoute_=combatRoute_=bridgeRoute_=true;return true;}
    bool PowerRoute()const{return powerRoute_;}
    bool EnableCompletionRoute(){if(groveRoute_||resumeSaveRoute_||dashJumpRoute_||ticks_!=0||deathRetryRoute_||checkpointRoute_||westCliffRoute_)return false;completionRoute_=powerRoute_=combatRoute_=bridgeRoute_=true;return true;}
    bool CompletionRoute()const{return completionRoute_;}
    bool EnableDeathRetryRoute(){if(groveRoute_||resumeSaveRoute_||dashJumpRoute_||ticks_!=0||combatRoute_)return false;deathRetryRoute_=bridgeRoute_=true;return true;}
    bool DeathRetryRoute()const{return deathRetryRoute_;}
    bool EnableCheckpointRoute(){if(groveRoute_||resumeSaveRoute_||dashJumpRoute_||ticks_!=0||deathRetryRoute_||powerRoute_||completionRoute_||westCliffRoute_)return false;checkpointRoute_=combatRoute_=bridgeRoute_=true;return true;}
    bool CheckpointRoute()const{return checkpointRoute_;}
    bool EnableMotionCheckpointRoute(){if(groveRoute_||resumeSaveRoute_||dashJumpRoute_||ticks_||bridgeRoute_||combatRoute_||deathRetryRoute_||checkpointRoute_)return false;checkpointRoute_=motionOnly_=true;return true;}
    bool MotionCheckpointRoute()const{return motionOnly_;}
    bool EnableResumeSaveRoute(bool accept){
        if(groveRoute_||ticks_||resumeSaveRoute_||bridgeRoute_||combatRoute_||checkpointRoute_||deathRetryRoute_||powerRoute_||completionRoute_||westCliffRoute_||dashJumpRoute_)return false;
        resumeSaveRoute_=true;resumeAccept_=accept;return true;
    }
    bool ResumeSaveRoute()const{return resumeSaveRoute_;}
    bool ResumeAccept()const{return resumeAccept_;}
    bool ResumeExactObserved()const{return resumeExact_;}
    bool ResumeAdvanceObserved()const{return resumeAdvanced_;}
    bool ResumeRejectedUnchanged()const{return resumeRejected_;}
    bool EnableWestCliffRoute(float targetX,bool standingPolicy=false){
        if(groveRoute_||dashJumpRoute_||resumeSaveRoute_||ticks_||bridgeRoute_||combatRoute_||deathRetryRoute_||checkpointRoute_||powerRoute_||completionRoute_||
            westCliffRoute_||(targetX!=-47.f&&targetX!=-44.f))return false;
        westCliffTargetX_=targetX;westCliffStandingPolicy_=standingPolicy;westCliffRoute_=combatRoute_=bridgeRoute_=true;return true;
    }
    bool WestCliffRoute()const{return westCliffRoute_;}
    float WestCliffTargetX()const{return westCliffTargetX_;}
    bool EnableGroveRoute(){
        if(ticks_||groveRoute_||resumeSaveRoute_||dashJumpRoute_||bridgeRoute_||combatRoute_||deathRetryRoute_||checkpointRoute_||powerRoute_||completionRoute_||westCliffRoute_)return false;
        groveRoute_=combatRoute_=bridgeRoute_=true;return true;
    }
    bool GroveRoute()const{return groveRoute_;}
    static constexpr unsigned GrovePointCount(){return unsigned(std::size(grovePoints_));}
    static constexpr Aevrynd::Vec3 GrovePoint(unsigned i){
        if(i>=std::size(grovePoints_))throw std::invalid_argument("Unknown grove route point");
        return {grovePoints_[i].x,0,grovePoints_[i].z};
    }
    unsigned SavedCheckpoint()const{return savedCheckpoint_;}
    unsigned LoadedCheckpoint()const{return loadedCheckpoint_;}
    unsigned DamageEvents()const{return damageEvents_;}
    unsigned Retries()const{return retries_;}
    unsigned Dodges()const{return dodges_;}
    unsigned Counters()const{return counters_;}
    Aevrynd::Input& Controls(){return controls_;}
    bool Finished()const{return phase_==Step::Complete||phase_==Step::Failed;}
    bool Passed()const{return phase_==Step::Complete;}
    unsigned Ticks()const{return ticks_;}
    std::string_view Failure()const{return failure_;}
    std::string_view Phase()const {
        if(resumeSaveRoute_&&!Finished())return resumePhases_[resumeAction_];
        switch(phase_){
        case Step::Initial:return "initial";
        case Step::StartRelease:return "start-release";
        case Step::WaitStart:return "wait-start";
        case Step::WalkRoad:return "walk-road";
        case Step::GroveWalk:
            if(grovePoint_>=std::size(grovePoints_))return "grove-invalid";
            std::snprintf(grovePhase_.data(),grovePhase_.size(),"grove-%u-%s-%u",grovePoint_,grovePoints_[grovePoint_].name,groveAction_);
            return grovePhase_.data();
        case Step::ApproachShelter:return "approach-shelter";
        case Step::Crouch:return "crouch-outside";
        case Step::EnterShelter:return "enter-shelter";
        case Step::InteractRelease:return "interaction-release";
        case Step::Interact:return "collect-tools";
        case Step::VerifyTools:return "verify-tools";
        case Step::LowRoofRelease:return "low-roof-release";
        case Step::VerifyLowRoof:return "verify-low-roof";
        case Step::ExitShelter:return "exit-shelter";
        case Step::StandRelease:return "stand-release";
        case Step::VerifyStanding:return "verify-standing";
        case Step::BridgeRoute:return bridgePoints_[bridgePoint_].phases[bridgeAction_];
        case Step::Combat:return combatPhases_[counters_][combatAction_];
        case Step::Checkpoint:
            if(pivotCheckpointRoute_){std::snprintf(powerPhase_.data(),powerPhase_.size(),"pivot-save-%u-%s",pivotRound_,
                checkpointAction_==10?(pivotRound_?"release":"resume"):checkpointAction_<std::size(pivotPhases_)?pivotPhases_[checkpointAction_]:"invalid");return powerPhase_.data();}
            if(motionOnly_){std::snprintf(powerPhase_.data(),powerPhase_.size(),"motion-%s-%u",motionRound_?"dodge":"jump",checkpointAction_);return powerPhase_.data();}
            return checkpointPhases_[checkpointAction_];
        case Step::DashJump:
            if(sprintFlowRoute_)return sprintFlowPhases_[sprintFlowAction_];
            std::snprintf(dashJumpPhase_.data(),dashJumpPhase_.size(),"handoff-%u-%s",dashJumpRound_,dashJumpPhases_[dashJumpAction_]);
            return dashJumpPhase_.data();
        case Step::WestCliff:
            if(westCliffStandingPolicy_&&westCliffAction_==4)return "west-cliff-approach-channel-60";
            if(westCliffStandingPolicy_&&westCliffAction_==5)return "west-cliff-approach-60";
            return westCliffPhases_[westCliffAction_];
        case Step::Power:
            if(powerPoints_[powerPoint_].action==2&&powerAction_){
                std::snprintf(powerPhase_.data(),powerPhase_.size(),"power-dial-%u-turn-%u-%s",dialWheel_+1,dialTurn_+1,
                    powerAction_==1?"release":powerAction_==2?"press":"verify");
                return powerPhase_.data();
            }
            return powerPoints_[powerPoint_].phases[powerAction_];
        case Step::Completion:
            if(!completionAction_)return completionPoints_[completionPoint_].name;
            std::snprintf(completionPhase_.data(),completionPhase_.size(),"%s-turn-%u-%s",
                completionPoints_[completionPoint_].name,signalTurns_,completionActions_[completionAction_]);
            return completionPhase_.data();
        case Step::DeathRetry:
            if(deathAction_==3){
                return deathWaitSeconds_<.5f?"death-release":deathWaitSeconds_<1.f?"death-collapse-middle":"death-collapse-settled";
            }
            if(deathAction_==1||deathAction_==2){
                std::snprintf(deathPhase_.data(),deathPhase_.size(),"death-hit-%u-%s",
                    damageEvents_+(deathAction_==1?1u:0u),deathAction_==1?"wait":"observed");
                return deathPhase_.data();
            }
            return deathPhases_[deathAction_];
        case Step::Complete:return "complete";
        case Step::Failed:return "failed";
        }
        return "invalid";
    }
    void Tick(const Session& session,float cameraYaw,float controllerHeight,bool started,bool paused,float dt,
        const Encounter* enemy=nullptr,const Evasion* evasion=nullptr,bool grounded=true,
        const SaveGame* disk=nullptr,std::string_view feedback={},Aevrynd::Vec3 observedVelocity={},const SaveGame* motionSnapshot=nullptr,bool feetSettled=true,
        const WestCliffReplayWorld* westWorld=nullptr,const Aevrynd::CharacterController3D* handoffActor=nullptr,
        bool handoffMotionAccepted=false) {
        // 只清掉上一更新的邊緣，不先放開持續鍵；完整最終集合一次套用，避免假 Pressed。
        controls_.ConsumePresses();
        Keys desired{};
        if(Finished()){Apply(desired);return;}
        if(ticks_>=7200){Fail("total-tick-limit");Apply(desired);return;}
        ++ticks_;
        if(!session.Valid()||(!session.Alive()&&!(deathRetryRoute_&&phase_==Step::DeathRetry))||!std::isfinite(cameraYaw)||!std::isfinite(controllerHeight)||
           controllerHeight<=0||!Session::Finite(observedVelocity)||!std::isfinite(dt)||dt<=0||dt>.1f){Fail("invalid-live-state");Apply(desired);return;}
        observedSpeed_=std::hypot(observedVelocity.x,observedVelocity.z);
        observedVelocity_=observedVelocity;
        if(paused){Fail("production-handler-paused");Apply(desired);return;}
        if(resumeSaveRoute_){TickResumeSave(started,motionSnapshot,disk,feedback,desired);Apply(desired);return;}
        if(combatRoute_&&session.health!=100){Fail("combat-player-damaged");Apply(desired);return;}
        if(deathRetryRoute_&&phase_!=Step::DeathRetry&&session.health!=100){Fail("death-route-early-damage");Apply(desired);return;}
        if(ticks_-progressTick_>=600){Fail("phase-stalled-600-ticks");Apply(desired);return;}
        if(phase_!=Step::Initial&&phase_!=Step::StartRelease&&phase_!=Step::WaitStart&&!started){
            Fail("production-handler-not-started");Apply(desired);return;
        }
        switch(phase_){
        case Step::Initial:
            if(session.collected||session.repairMaterials||session.CurrentStage()!=Stage::Supplies||
               Distance(session.feet,0.f,0.f)>.18f||std::abs(session.feet.y)>.3f){Fail("fresh-origin-required");break;}
            if(started)Advance(dashJumpRoute_?Step::DashJump:(motionOnly_||pivotCheckpointRoute_)?Step::Checkpoint:Step::WalkRoad);
            else {desired[VK_RETURN]=true;Advance(Step::StartRelease);}
            break;
        case Step::StartRelease:
            // 開始鍵確實放開一個正式更新，不能以持續 Enter 掩蓋按鍵邊緣問題。
            Advance(Step::WaitStart);break;
        case Step::WaitStart:
            if(started)Advance(dashJumpRoute_?Step::DashJump:(motionOnly_||pivotCheckpointRoute_)?Step::Checkpoint:Step::WalkRoad);break;
        case Step::WalkRoad:
            Walk(session,cameraYaw,0.f,18.f,.18f,Step::ApproachShelter,desired);break;
        case Step::GroveWalk:
            TickGrove(session,cameraYaw,controllerHeight,grounded,dt,desired,motionSnapshot,feetSettled,handoffActor,handoffMotionAccepted);break;
        case Step::ApproachShelter:
            // 棚頂東緣 -5.5，角色半徑 .45，停止容差 .18；中心 -4.8 才保證整個站姿離開棚頂。
            Walk(session,cameraYaw,-4.8f,18.f,.18f,Step::Crouch,desired);break;
        case Step::Crouch:
            desired[VK_CONTROL]=true;
            if(controllerHeight<1.5f)Advance(Step::EnterShelter);
            break;
        case Step::EnterShelter:
            desired[VK_CONTROL]=true;
            // 桌面碰撞令中心最西只能到 -7.25；目標 -7.24 與 .025 容差可接受真實接觸點。
            // 距工具 .76m 加容差最多 .785m，仍嚴格位於正式 .8m 互動範圍，不放寬碰撞。
            Walk(session,cameraYaw,-7.24f,18.f,.025f,Step::InteractRelease,desired);break;
        case Step::InteractRelease:
            desired[VK_CONTROL]=true;Advance(Step::Interact);break;
        case Step::Interact:
            desired[VK_CONTROL]=true;desired['E']=true;Advance(Step::VerifyTools);break;
        case Step::VerifyTools:
            // 上個更新的唯一 E 已經交給正式處理器；本更新放開，不直接呼叫 Collect。
            desired[VK_CONTROL]=true;
            if((session.repairMaterials&1u)!=0)Advance(Step::LowRoofRelease);
            else Fail("production-E-did-not-collect-tools");
            break;
        case Step::LowRoofRelease:
            // 棚內放開 Ctrl 一個正式更新，讓同一 TrySetHeight 接受站立要求並由低頂拒絕。
            Advance(Step::VerifyLowRoof);break;
        case Step::VerifyLowRoof:
            if((session.repairMaterials&1u)==0||Distance(session.feet,-7.24f,18.f)>.025f){
                Fail("low-roof-observation-moved");break;
            }
            if(std::abs(controllerHeight-1.4f)>=.001f){Fail("low-roof-incorrectly-stood");break;}
            // 只讀核對上一正式更新保留蹲姿，恢復 Ctrl 後才繼續走出棚屋。
            desired[VK_CONTROL]=true;Advance(Step::ExitShelter);break;
        case Step::ExitShelter:
            desired[VK_CONTROL]=true;
            if((session.repairMaterials&1u)==0){Fail("collected-tools-lost");break;}
            Walk(session,cameraYaw,-4.8f,18.f,.18f,Step::StandRelease,desired);break;
        case Step::StandRelease:
            // 先讓正式更新看到 Ctrl 放開，下一更新才核對角色是否成功站起。
            Advance(Step::VerifyStanding);break;
        case Step::VerifyStanding:
            if((session.repairMaterials&1u)==0||Distance(session.feet,-4.8f,18.f)>.18f){Fail("exit-state-changed");break;}
            if(std::abs(controllerHeight-1.8f)<.001f)Advance(bridgeRoute_?Step::BridgeRoute:Step::Complete);
            break;
        case Step::BridgeRoute:
            TickBridge(session,cameraYaw,controllerHeight,desired);break;
        case Step::Combat:
            TickCombat(session,cameraYaw,controllerHeight,enemy,evasion,grounded,desired);break;
        case Step::Checkpoint:
            if(pivotCheckpointRoute_)TickPivotCheckpoint(session,cameraYaw,controllerHeight,enemy,evasion,grounded,
                disk,feedback,desired,motionSnapshot,feetSettled);
            else TickCheckpoint(session,cameraYaw,controllerHeight,enemy,evasion,grounded,dt,disk,feedback,desired,motionSnapshot);break;
        case Step::DashJump:
            TickDashJump(session,cameraYaw,controllerHeight,evasion,grounded,dt,desired,motionSnapshot,feetSettled,handoffActor,handoffMotionAccepted);break;
        case Step::WestCliff:
            TickWestCliff(session,cameraYaw,controllerHeight,enemy,evasion,grounded,dt,desired,feetSettled,motionSnapshot,westWorld);break;
        case Step::Power:
            TickPower(session,cameraYaw,controllerHeight,desired);break;
        case Step::Completion:
            TickCompletion(session,cameraYaw,controllerHeight,desired);break;
        case Step::DeathRetry:
            TickDeathRetry(session,cameraYaw,controllerHeight,enemy,evasion,grounded,dt,desired);break;
        case Step::Complete:case Step::Failed:break;
        }
        if(Finished())desired={};
        Apply(desired);
    }
private:
    enum class Step {Initial,StartRelease,WaitStart,WalkRoad,GroveWalk,ApproachShelter,Crouch,EnterShelter,
        InteractRelease,Interact,VerifyTools,LowRoofRelease,VerifyLowRoof,ExitShelter,StandRelease,VerifyStanding,BridgeRoute,Combat,Power,Completion,DeathRetry,Checkpoint,WestCliff,DashJump,Complete,Failed};
    using Keys=std::array<bool,256>;
    bool groveRoute_{};
    unsigned grovePoint_{},groveAction_{},groveActionTicks_{};
    mutable std::array<char,96> grovePhase_{};
    struct GrovePointData {float x,z;const char* name;};
    inline static constexpr GrovePointData grovePoints_[]{
        {0,46,"road"},{0,55.5f,"tree-corridor-east"},{-10,55.5f,"forest-approach"},
        {-13.5f,46,"grove-east"},{-14.5f,50,"tree-close"},{-13.5f,54,"grove-northeast"},
        {-18,55,"grove-north"},{-22.5f,54.5f,"grove-northwest"},{-22.5f,45.5f,"grove-west"},
        {-18,43.5f,"grove-south"},{-13.5f,46,"grove-east-return"},{-10,55.5f,"forest-return"},
        {0,55.5f,"tree-corridor-return"},{0,46,"road-return"}
    };
    bool resumeSaveRoute_{},resumeAccept_{},resumeExact_{},resumeAdvanced_{},resumeRejected_{};
    unsigned resumeAction_{};
    SaveGame resumeInitial_,resumeSaved_;
    inline static constexpr const char* resumePhases_[]{"resume-settle","resume-F9","resume-verify","resume-advance"};
    // 選單初始控制器尚未模擬，快照未必可存；仍逐欄比較真實觀測，不用空Serialize冒充相等。
    static bool SameObservedSnapshot(const SaveGame& a,const SaveGame& b){
        const auto base=[](const SaveGame& s){return SaveGame{s.session,s.encounter,s.evasion}.Serialize();};
        const auto av=base(a);if(av.empty()||av!=base(b)||a.worldIdentity!=b.worldIdentity||bool(a.motion)!=bool(b.motion))return false;
        if(!a.motion)return true;
        const auto same=[](Aevrynd::Vec3 p,Aevrynd::Vec3 q){return p.x==q.x&&p.y==q.y&&p.z==q.z;};
        const auto& p=*a.motion;const auto& q=*b.motion;const auto& c=p.controller;const auto& d=q.controller;
        const auto& m=p.locomotion.motor;const auto& n=q.locomotion.motor;
        return same(c.feet,d.feet)&&same(c.velocity,d.velocity)&&c.height==d.height&&c.yaw==d.yaw&&c.grounded==d.grounded&&
            same(m.velocity,n.velocity)&&m.heading==n.heading&&m.headingReady==n.headingReady&&m.phase==n.phase&&m.supportTicks==n.supportTicks&&
            m.tick==n.tick&&m.runFlight==n.runFlight&&m.wasDashing==n.wasDashing&&p.locomotion.remainder==q.locomotion.remainder&&
            p.locomotion.dodgeFlight==q.locomotion.dodgeFlight&&p.explorerYaw==q.explorerYaw&&p.explorerRequestedYaw==q.explorerRequestedYaw&&
            p.explorerPhase==q.explorerPhase&&p.explorerDistance==q.explorerDistance&&p.explorerSeconds==q.explorerSeconds;
    }
    void TickResumeSave(bool started,const SaveGame* live,const SaveGame* disk,std::string_view feedback,Keys& keys){
        if(!live){Fail("resume-live-observation-missing");return;}
        switch(resumeAction_){
        case 0:
            if(started||!SameObservedSnapshot(*live,*live)){Fail("resume-menu-initial-invalid");return;}
            resumeInitial_=*live;
            if(resumeAccept_){if(!disk||!disk->Valid()||!disk->motion||disk->evasion.remaining<=0){Fail("resume-active-save-fixture-required");return;}resumeSaved_=*disk;}
            ++resumeAction_;return;
        case 1:
            if(started||!SameObservedSnapshot(resumeInitial_,*live)){Fail("resume-menu-changed-before-F9");return;}
            keys[VK_F9]=true;++resumeAction_;return;
        case 2:
            if(resumeAccept_){
                if(!started||feedback!="Loaded"||!live->Valid()||live->Serialize()!=resumeSaved_.Serialize()){
                    Fail("resume-fresh-process-full-snapshot-mismatch");return;}
                resumeExact_=true;++loadedCheckpoint_;++resumeAction_;return;
            }
            if(started||feedback!="Save unavailable"||!SameObservedSnapshot(resumeInitial_,*live)){
                Fail("resume-rejected-save-mutated-menu-state");return;}
            resumeRejected_=true;Advance(Step::Complete);return;
        case 3:
            if(!started||!live->Valid()||!disk||disk->Serialize()!=resumeSaved_.Serialize()||!CheckpointStableFields(resumeSaved_,*live)||
                live->motion->locomotion.motor.tick<=resumeSaved_.motion->locomotion.motor.tick||
                live->evasion.remaining>=resumeSaved_.evasion.remaining||live->encounter.phaseTime>=resumeSaved_.encounter.phaseTime||
                Aevrynd::Dot(live->session.feet-resumeSaved_.session.feet,resumeSaved_.evasion.direction)<=0){
                Fail("resume-restored-world-did-not-advance");return;}
            resumeAdvanced_=true;Advance(Step::Complete);return;
        default:Fail("resume-action-invalid");return;
        }
    }
    struct BridgePoint {float x,z;unsigned action;std::array<std::string_view,4> phases;};
    // 每段先回主路再沿路前進，避免直接切越林木；互動仍由正式 E 處理器執行。
    // 先取南端木材再沿主路北上，避免收完北側物資後折返南端；收集與修橋條件完全不變。
    inline static constexpr BridgePoint bridgePoints_[]{
        {0,18,0,{"bridge-road-18"}}, {0,-8,0,{"wood-1-road"}},
        {6,-8,4,{"wood-1-approach","wood-1-release","wood-1-press","wood-1-verify"}},
        {0,-8,0,{"wood-1-return"}}, {0,SupplyApproach(0).z,0,{OriginalEntryLayoutActive?"bridge-road-6":"pilot-supply-spur-junction"}},
        {SupplyApproach(0).x,SupplyApproach(0).z,1,{"supply-0-approach","supply-0-release","supply-0-press","supply-0-verify"}},
        {0,SupplyApproach(0).z,0,{"supply-0-return"}}, {0,Session::supplies[1].z,0,{"bridge-road-14"}},
        {SupplyApproach(1).x,SupplyApproach(1).z,2,{"supply-1-approach","supply-1-release","supply-1-press","supply-1-verify"}},
        {0,Session::supplies[1].z,0,{"supply-1-return"}}, {0,Session::supplies[2].z,0,{"bridge-road-22"}},
        {SupplyApproach(2).x,SupplyApproach(2).z,3,{"supply-2-approach","supply-2-release","supply-2-press","supply-2-verify"}},
        {0,Session::supplies[2].z,0,{"supply-2-return"}}, {0,26,0,{"wood-2-road"}},
        {-6,26,5,{"wood-2-approach","wood-2-release","wood-2-press","wood-2-verify"}},
        {0,26,0,{"wood-2-return"}},
        {0,28.2f,6,{"repair-bridge-approach","repair-bridge-release","repair-bridge-press","repair-bridge-verify"}},
        // 中央河谷 z32..40；z41 已到北岸，仍低於野豬追擊區起點 z42。
        {0,41,0,{"cross-bridge"}}
    };
    Aevrynd::Input controls_;
    bool bridgeRoute_{},expectedBridge_{};
    bool combatRoute_{},dashObserved_{};
    bool powerRoute_{},expectedClue_{},expectedFuse_{},expectedPower_{};
    bool completionRoute_{};
    bool checkpointRoute_{},motionOnly_{};
    bool dashJumpRoute_{};
    bool sprintFlowRoute_{};
    unsigned sprintFlowAction_{},sprintFlowTicks_{};
    inline static constexpr const char* sprintFlowPhases_[]{"flow-settle","flow-walk","flow-run","flow-active-reverse","flow-brake","flow-rest-reverse","flow-rest-brake","flow-crouch-walk","flow-stand-rest"};
    unsigned dashJumpRound_{},dashJumpAction_{};
    std::uint64_t dashJumpLastMotorTick_{};
    bool dashJumpHasMotorTick_{};
    Aevrynd::Vec3 dashJumpStart_{};
    std::array<DashJumpReplayEvidence,3> dashJumpEvidence_{};
    mutable std::array<char,64> dashJumpPhase_{};
    inline static constexpr const char* dashJumpPhases_[]{"settle","dash","input-release","await-outcome","landing","rest"};
    bool pivotCheckpointRoute_{},pivotObserved_{};
    unsigned pivotRound_{};
    std::uint64_t pivotPhaseTick_{},pivotObservedTick_{},pivotBeforeSaveTick_{};
    Aevrynd::Vec3 pivotDirection_{},pivotStart_{},pivotRest_{};
    float pivotYaw_{},pivotPhase_{},pivotHeading_{};
    SaveGame pivotInitial_;
    inline static constexpr const char* pivotPhases_[]{"missing-release","missing-press","missing-verify","walk","stop","turn",
        "save-verify","live-change","load-release","load-verify","resume","between-round-stop"};
    bool westCliffRoute_{},westCliffStandingPolicy_{};
    float westCliffTargetX_{};
    unsigned westCliffAction_{};
    double westCliffElapsed_{},westCliffRestSeconds_{};
    float westCliffIssuedSeconds_{};
    Aevrynd::Vec3 westCliffStart_{};
    bool westCliffIssued_{};
    inline static constexpr std::string_view westCliffPhases_[]{"west-cliff-road-62","west-cliff-relay-approach-62",
        "west-cliff-north-bypass-66","west-cliff-west-66","west-cliff-approach-channel-58","west-cliff-approach-58",
        "west-cliff-downhill","west-cliff-settle","west-cliff-reverse","west-cliff-exit","west-cliff-exit-settle"};
    unsigned motionRound_{};
    unsigned checkpointAction_{},savedCheckpoint_{},loadedCheckpoint_{};
    float checkpointWait_{};
    SaveGame checkpointMissingBefore_,checkpointSaved_,checkpointPrevious_;
    Aevrynd::Vec3 checkpointDirection_{};
    inline static constexpr std::string_view checkpointPhases_[]{"checkpoint-missing-release","checkpoint-missing-press",
        "checkpoint-missing-verify","checkpoint-save-release","checkpoint-save-press","checkpoint-save-verify",
        "checkpoint-live-advance","checkpoint-load-release","checkpoint-load-press","checkpoint-load-verify","checkpoint-resumed-dash"};
    bool deathRetryRoute_{};
    unsigned deathAction_{},damageEvents_{},retries_{};
    float deathWaitSeconds_{};
    EnemyPhase deathPriorPhase_{EnemyPhase::Pursuit};
    Aevrynd::Vec3 deathStrike_{};
    mutable std::array<char,64> deathPhase_{};
    inline static constexpr std::string_view deathPhases_[]{"death-approach","","","death-release",
        "death-retry-press","death-retry-verify","death-ground-verify","death-recross"};
    unsigned completionPoint_{},completionAction_{},signalTurns_{};
    unsigned expectedPatchA_{},expectedExtraction_{},expectedFieldTasks_{},expectedLinked_{},expectedSectors_{};
    mutable std::array<char,112> completionPhase_{};
    struct CompletionPoint {float x,z,tolerance;unsigned action;const char* name;};
    inline static constexpr CompletionPoint completionPoints_[]{
        {0,65,.18f,0,"completion-road-65"},{0,62,.18f,0,"completion-road-62"},
        {-20.6f,62,.08f,1,"completion-west-station"},{0,62,.18f,0,"completion-west-return"},
        {0,86,.18f,0,"completion-road-86"},{20.6f,86,.08f,2,"completion-east-station"},
        {0,86,.18f,0,"completion-east-return"},{0,94,.18f,0,"completion-road-94"},
        {-18.6f,94,.08f,3,"completion-northwest-station"},{0,94,.18f,0,"completion-northwest-return"},
        {0,68.2f,.18f,4,"completion-beacon"}
    };
    inline static constexpr const char* completionActions_[]{"approach","prerequisite-release","prerequisite-press",
        "prerequisite-verify","confirm-release","confirm-press","confirm-verify","signal-observe",
        "signal-release","signal-press","signal-verify","link-release","link-press","link-verify"};
    unsigned powerPoint_{},powerAction_{},dialWheel_{},dialTurn_{},expectedDials_{},expectedCircuit_{};
    mutable std::array<char,96> powerPhase_{};
    struct PowerPoint {float x,z,tolerance;unsigned action;std::array<std::string_view,4> phases;};
    inline static constexpr PowerPoint powerPoints_[]{
        {0,48,.18f,0,{"power-road-48"}}, {0,79,.18f,0,{"power-road-79"}},
        {7.75f,79,.08f,1,{"power-log-approach","power-log-release","power-log-press","power-log-verify"}},
        {0,79,.18f,0,{"power-hut-exit"}}, {0,59,.18f,0,{"power-road-59"}},
        {-6.6f,59,.08f,2,{"power-cabinet-approach"}},
        {-6.6f,59,.08f,3,{"power-fuse-approach","power-fuse-release","power-fuse-press","power-fuse-verify"}},
        {0,59,.18f,0,{"power-workshop-exit"}}, {0,65,.18f,0,{"power-road-65"}},
        {5.4f,65,.18f,4,{"power-switch-1-approach","power-switch-1-release","power-switch-1-press","power-switch-1-verify"}},
        {5.4f,65,.18f,5,{"power-switch-3-approach","power-switch-3-release","power-switch-3-press","power-switch-3-verify"}},
        {5.4f,65,.18f,6,{"power-generator-approach","power-generator-release","power-generator-press","power-generator-verify"}}
    };
    unsigned combatAction_{},dodges_{},counters_{};
    Aevrynd::Vec3 lockedStrike_{};
    inline static constexpr std::string_view combatPhases_[3][8]{
        {"combat-1-approach","combat-1-windup","combat-1-dodge","combat-1-outside-circle","combat-1-recovery-approach","combat-1-counter-release","combat-1-counter-press","combat-1-counter-verify"},
        {"combat-2-approach","combat-2-windup","combat-2-dodge","combat-2-outside-circle","combat-2-recovery-approach","combat-2-counter-release","combat-2-counter-press","combat-2-counter-verify"},
        {"combat-3-approach","combat-3-windup","combat-3-dodge","combat-3-outside-circle","combat-3-recovery-approach","combat-3-counter-release","combat-3-counter-press","combat-3-counter-verify"}
    };
    unsigned bridgePoint_{},bridgeAction_{},expectedCollected_{},expectedMaterials_{1};
    Step phase_{Step::Initial};
    std::string_view failure_{};
    unsigned ticks_{},progressTick_{};
    float bestDistance_{std::numeric_limits<float>::infinity()};
    float observedSpeed_{};
    Aevrynd::Vec3 observedVelocity_{};
    bool braking_{};
    static float Distance(Aevrynd::Vec3 feet,float x,float z){return std::hypot(feet.x-x,feet.z-z);}
    void Apply(const Keys& keys){for(unsigned key=0;key<256;++key)controls_.SetKey(key,keys[key]);}
    void Advance(Step next){phase_=next;progressTick_=ticks_;bestDistance_=std::numeric_limits<float>::infinity();braking_=false;}
    void Fail(std::string_view reason){failure_=reason;phase_=Step::Failed;}
    void TickGrove(const Session& session,float yaw,float height,bool grounded,float dt,Keys& keys,
        const SaveGame* live,bool feetSettled,const Aevrynd::CharacterController3D* actor,bool motionAccepted){
        if(!groveRoute_||grovePoint_>=std::size(grovePoints_)||groveAction_>3||!actor||!motionAccepted||!live||!live->Valid()||
            !live->motion||live->session.Serialize()!=session.Serialize()||session.health!=100||session.collected!=7||
            session.repairMaterials!=7||!session.bridgeRepaired||!session.guardianDefeated||std::abs(dt-1.f/60.f)>1.e-8f){Fail("grove-live-world-unavailable");return;}
        const auto& c=live->motion->controller;const auto actual=actor->CaptureMotionState();
        const auto same=[](Aevrynd::Vec3 a,Aevrynd::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
        if(!same(c.feet,actual.feet)||!same(c.feet,session.feet)||!same(c.velocity,actual.velocity)||
            !same(c.velocity,observedVelocity_)||c.height!=height||c.grounded!=grounded||c.yaw!=actual.yaw||
            (std::abs(height-1.8f)>.001f&&std::abs(height-1.4f)>.001f)){Fail("grove-motion-identity-mismatch");return;}
        const auto point=grovePoints_[grovePoint_];
        if(groveAction_==0){
            if(Distance(session.feet,point.x,point.z)<=.18f&&observedSpeed_<.001f){
                if(grounded&&feetSettled){groveAction_=1;groveActionTicks_=0;BridgeProgress();}
                return;
            }
            Walk(session,yaw,point.x,point.z,.18f,Step::GroveWalk,keys);return;
        }
        // 在真樹旁使用正常Ctrl與方向鍵環視；不修改鏡頭、角色或碰撞資料。
        if(grovePoint_==4&&groveAction_<3){
            if(groveAction_==1){keys[VK_CONTROL]=true;keys[VK_RIGHT]=true;
                if(groveActionTicks_&&std::abs(height-1.4f)>.001f){Fail("grove-crouch-not-applied");return;}}
            else {keys[VK_LEFT]=true;if(groveActionTicks_&&std::abs(height-1.8f)>.001f){Fail("grove-stand-not-restored");return;}}
            if(++groveActionTicks_>=120){++groveAction_;groveActionTicks_=0;BridgeProgress();}
            return;
        }
        if(!grounded||!feetSettled||observedSpeed_>=.001f||std::abs(height-1.8f)>.001f)return;
        ++grovePoint_;groveAction_=groveActionTicks_=0;
        Advance(grovePoint_==std::size(grovePoints_)?Step::Complete:Step::GroveWalk);
    }
    void DashJumpAdvance(unsigned action){dashJumpAction_=action;BridgeProgress();}
    // 真按鍵操作的有界觀測路線；停止條件仍讀正式速度、接地及足部，不能直接煞停或轉身。
    void TickSprintFlow(float yaw,float height,bool grounded,bool feetSettled,Keys& keys){
        if(sprintFlowAction_>=std::size(sprintFlowPhases_)){Fail("sprint-flow-action-invalid");return;}
        const bool settled=grounded&&feetSettled&&Aevrynd::Length(observedVelocity_)<=.001f;
        const auto next=[&](){++sprintFlowAction_;sprintFlowTicks_=0;BridgeProgress();};
        ++sprintFlowTicks_;
        if(sprintFlowTicks_>180){Fail("sprint-flow-stage-timeout");return;}
        switch(sprintFlowAction_){
        case 0:if(settled)next();return;
        case 1:Steer(0,1,yaw,keys);if(sprintFlowTicks_==45)next();return;
        case 2:Steer(0,1,yaw,keys);keys[VK_SHIFT]=true;if(sprintFlowTicks_==90)next();return;
        case 3:Steer(0,-1,yaw,keys);if(sprintFlowTicks_==60)next();return;
        case 4:if(settled)next();return;
        case 5:Steer(0,1,yaw,keys);if(sprintFlowTicks_==60)next();return;
        case 6:if(settled)next();return;
        case 7:Steer(0,-1,yaw,keys);keys[VK_CONTROL]=true;if(sprintFlowTicks_==45)next();return;
        case 8:if(settled&&height==1.8f)Advance(Step::Complete);return;
        default:Fail("sprint-flow-action-invalid");return;
        }
    }
    // 只產生按鍵並讀回正式世界；不改角色、閃避、快照或冷卻來湊出連招。
    void TickDashJump(const Session& session,float yaw,float height,const Evasion* evasion,bool grounded,float dt,Keys& keys,
        const SaveGame* live,bool feetSettled,const Aevrynd::CharacterController3D* actor,bool motionAccepted){
        if(ticks_>900||dashJumpRound_>=3||dashJumpAction_>=6){Fail("handoff-finite-route-limit");return;}
        if(!actor||!motionAccepted||!evasion||!live||!live->Valid()||!live->motion||
           live->session.Serialize()!=session.Serialize()||session.health!=100||session.collected||session.repairMaterials||
           session.bridgeRepaired||session.guardianDefeated||(!sprintFlowRoute_&&height!=1.8f)||std::abs(dt-1.f/60.f)>1.e-8f){
            Fail("handoff-live-world-unavailable");return;
        }
        const auto& c=live->motion->controller;const auto actual=actor->CaptureMotionState();
        const auto same=[](Aevrynd::Vec3 a,Aevrynd::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
        const auto motorTick=live->motion->locomotion.motor.tick;
        if(!same(c.feet,actual.feet)||!same(c.feet,session.feet)||!same(c.velocity,actual.velocity)||
           !same(c.velocity,observedVelocity_)||c.height!=height||c.yaw!=actual.yaw||c.grounded!=grounded||
           !same(live->evasion.direction,evasion->direction)||live->evasion.remaining!=evasion->remaining||
           live->evasion.cooldown!=evasion->cooldown||
           grounded!=actor->Grounded()||actor->Settings().jumpGraceSeconds!=.1f||
           (dashJumpHasMotorTick_&&motorTick!=dashJumpLastMotorTick_+2)){
            Fail("handoff-motion-identity-mismatch");return;
        }
        dashJumpHasMotorTick_=true;dashJumpLastMotorTick_=motorTick;
        if(sprintFlowRoute_){TickSprintFlow(yaw,height,grounded,feetSettled,keys);return;}
        auto& proof=dashJumpEvidence_[dashJumpRound_];const auto stats=actor->Stats();
        proof.peakVelocityY=(std::max)(proof.peakVelocityY,c.velocity.y);
        proof.peakHeight=(std::max)(proof.peakHeight,c.feet.y);
        if(proof.qTick&&ticks_-proof.qTick>180){Fail("handoff-case-timeout");return;}
        if(dashJumpAction_>0&&stats.jumps>proof.jumpsBefore+(dashJumpRound_==1?2u:1u)){
            Fail("handoff-unrequested-extra-jump");return;
        }
        switch(dashJumpAction_){
        case 0:
            if(!grounded||!feetSettled||Aevrynd::Length(c.velocity)>.001f||evasion->remaining!=0||evasion->cooldown!=0)return;
            proof={};proof.jumpsBefore=stats.jumps;proof.landingsBefore=stats.landings;
            proof.startHeight=proof.peakHeight=c.feet.y;dashJumpStart_=c.feet;
            Steer(0,1,yaw,keys);keys['Q']=true;proof.qTick=ticks_;
            if(dashJumpRound_==2){keys[VK_SPACE]=true;proof.spaceTick=ticks_;}
            DashJumpAdvance(1);return;
        case 1:
            if(stats.jumps!=proof.jumpsBefore+1||grounded||evasion->remaining<=0||evasion->cooldown<=0){
                Fail("handoff-real-dash-launch-missing");return;
            }
            if(!proof.qObserved){proof.qObserved=true;++dodges_;}
            if(dashJumpRound_==2){DashJumpAdvance(2);return;}
            if(dashJumpRound_==0||evasion->remaining<=.075f){
                if((dashJumpRound_==0&&evasion->remaining<=.1f)||(dashJumpRound_==1&&evasion->remaining>.1f)){
                    Fail("handoff-space-outside-declared-window");return;
                }
                keys[VK_SPACE]=true;proof.spaceTick=ticks_;proof.remainingAtSpace=evasion->remaining;
                DashJumpAdvance(2);
            }
            return;
        case 2:DashJumpAdvance(3);return;
        case 3:
            if(dashJumpRound_==1&&stats.jumps==proof.jumpsBefore+2){
                // 著地與起跳可能位於同一外幀的兩個固定步，以真landings計數作證。
                if(grounded||evasion->remaining!=0||c.velocity.y<=6||stats.landings<=proof.landingsBefore){
                    Fail("handoff-full-jump-without-real-landing");return;
                }
                proof.fullJumpObserved=true;proof.fullJumpTick=ticks_;proof.landingsAtJump=stats.landings;
                DashJumpAdvance(4);return;
            }
            if(dashJumpRound_!=1&&grounded&&evasion->remaining==0)DashJumpAdvance(4);
            return;
        case 4:
            if(grounded&&feetSettled&&evasion->remaining==0)DashJumpAdvance(5);
            return;
        case 5:
            if(!grounded||!feetSettled||Aevrynd::Length(c.velocity)>.001f||evasion->remaining!=0||evasion->cooldown!=0)return;
            proof.jumpsAfter=stats.jumps;proof.landingsAfter=stats.landings;
            proof.travel=Aevrynd::Length(c.feet-dashJumpStart_);proof.completeTick=ticks_;
            if(!proof.qObserved||!proof.spaceTick||stats.jumps!=proof.jumpsBefore+(dashJumpRound_==1?2u:1u)||
               proof.fullJumpObserved!=(dashJumpRound_==1)||proof.travel<=1||proof.travel>=4||
               (dashJumpRound_==1&&(proof.peakVelocityY<=6||proof.peakHeight-proof.startHeight<=.8f))){
                Fail("handoff-case-evidence-incomplete");return;
            }
            if(++dashJumpRound_==3){Advance(Step::Complete);return;}
            DashJumpAdvance(0);return;
        default:Fail("handoff-action-invalid");return;
        }
    }
    void BridgeProgress(){progressTick_=ticks_;bestDistance_=std::numeric_limits<float>::infinity();}
    void NextBridgePoint(){++bridgePoint_;bridgeAction_=0;BridgeProgress();}
    void TickBridge(const Session& session,float yaw,float height,Keys& keys){
        const auto& point=bridgePoints_[bridgePoint_];
        unsigned collected=expectedCollected_,materials=expectedMaterials_;bool repaired=expectedBridge_;
        if(bridgeAction_==3){
            if(point.action>=1&&point.action<=3)collected|=1u<<(point.action-1);
            else if(point.action==4)materials|=2u;
            else if(point.action==5)materials|=4u;
            else if(point.action==6)repaired=true;
        }
        // 每步都核對完整預期位元，不能把別處取得的物品或預先修橋視為這次 E 成果。
        if(session.collected!=collected||session.repairMaterials!=materials||session.bridgeRepaired!=repaired){
            Fail("bridge-route-state-mismatch");return;
        }
        if(std::abs(height-1.8f)>=.001f){Fail("bridge-route-not-standing");return;}
        if(bridgeAction_!=0){
            if(Distance(session.feet,point.x,point.z)>.18f){Fail("bridge-interaction-moved");return;}
            if(bridgeAction_==1){bridgeAction_=2;BridgeProgress();return;}
            if(bridgeAction_==2){keys['E']=true;bridgeAction_=3;BridgeProgress();return;}
            expectedCollected_=collected;expectedMaterials_=materials;expectedBridge_=repaired;
            NextBridgePoint();return;
        }
        if(Distance(session.feet,point.x,point.z)<=.18f&&observedSpeed_<.001f){
            if(bridgePoint_+1==std::size(bridgePoints_)){
                if(!session.bridgeRepaired||session.collected!=7||session.repairMaterials!=7||
                   session.feet.z<=40.f||std::abs(session.feet.y-GroundElevation(session.feet.x,session.feet.z))>.12f){
                    Fail("bridge-crossing-incomplete");return;
                }
                Advance(deathRetryRoute_?Step::DeathRetry:combatRoute_?Step::Combat:Step::Complete);return;
            }
            if(point.action){bridgeAction_=1;BridgeProgress();}
            else NextBridgePoint();
            return;
        }
        Walk(session,yaw,point.x,point.z,.18f,Step::BridgeRoute,keys);
    }
    void DeathAdvance(unsigned action){deathAction_=action;BridgeProgress();}
    static bool SamePosition(Aevrynd::Vec3 a,Aevrynd::Vec3 b){
        return std::abs(a.x-b.x)<=.001f&&std::abs(a.y-b.y)<=.001f&&std::abs(a.z-b.z)<=.001f;
    }
    void TickDeathRetry(const Session& session,float yaw,float height,const Encounter* enemy,const Evasion* evasion,bool grounded,float dt,Keys& keys){
        // 死亡路線只觀測正式敵人傷害；不呼叫 Damage、Retry，也不寫入任何玩家或敵人狀態。
        if(!enemy||!evasion||!enemy->Valid()||!Session::Finite(evasion->direction)||
           evasion->remaining!=0||evasion->cooldown!=0||evasion->direction.x!=0||evasion->direction.y!=0||evasion->direction.z!=0){
            Fail("death-observation-unavailable");return;
        }
        if(session.collected!=7||session.repairMaterials!=7||!session.bridgeRepaired||session.guardianDefeated||
           session.CurrentStage()!=Stage::Encounter||session.fuseClueRead||session.fuseDials||session.fuseCollected||
           session.circuit||session.powerRestored||session.relayLinked||session.relaySectors||session.relayPatchA||
           session.relayPatchB||session.relayFieldTasks||session.extractionRoute||session.beaconLit||
           std::abs(height-1.8f)>=.001f||enemy->health!=3||enemy->countered||enemy->playerCooldown!=0){
            Fail("death-route-state-mismatch");return;
        }
        const unsigned expectedHealth=deathAction_>=5?100u:100u-20u*damageEvents_;
        if(deathAction_==1&&session.health!=expectedHealth){
            // 五次必須逐次看見蓄力變恢復與恰好 20 傷；回血、額外傷害或跳過蓄力都拒絕。
            if(damageEvents_>=5||session.health+20u!=expectedHealth||deathPriorPhase_!=EnemyPhase::Windup||
               enemy->phase!=EnemyPhase::Recovery||!SamePosition(enemy->strikeTarget,deathStrike_)||
               Distance(session.feet,deathStrike_.x,deathStrike_.z)>Encounter::StrikeRadius||
               std::abs(session.feet.y-deathStrike_.y)>=2.f){
                Fail("death-damage-sequence-invalid");return;
            }
            ++damageEvents_;DeathAdvance(2);return;
        }
        if(session.health!=expectedHealth){Fail("death-health-unexpected");return;}
        if(deathAction_>=5){
            // R 更新直接重建敵人與短衝；不是只驗證血量變成 100。
            if(damageEvents_!=5||enemy->phase!=EnemyPhase::Pursuit||enemy->phaseTime!=0||
               !SamePosition(enemy->feet,Session::arena)||!SamePosition(enemy->strikeTarget,Session::arena)){
                Fail("death-retry-enemy-not-reset");return;
            }
            if(deathAction_<=6&&!SamePosition(session.feet,Session::bridge)){
                Fail("death-retry-position-invalid");return;
            }
        }
        // 過橋時腳下是正式橋面，不是河床地形；只在平地階段與北岸終點驗地形高度。
        if(deathAction_!=5&&deathAction_!=7&&(!grounded||std::abs(session.feet.y-GroundElevation(session.feet.x,session.feet.z))>.12f)){
            Fail("death-route-not-grounded");return;
        }
        switch(deathAction_){
        case 0:
            if(Distance(session.feet,0,44)<=.18f){
                deathPriorPhase_=enemy->phase;deathStrike_=enemy->strikeTarget;DeathAdvance(1);return;
            }
            Walk(session,yaw,0,44,.18f,Step::DeathRetry,keys);return;
        case 1:
            if(Distance(session.feet,0,44)>.18f){Fail("death-wait-position-changed");return;}
            deathPriorPhase_=enemy->phase;deathStrike_=enemy->strikeTarget;return;
        case 2:
            // 傷害觀測階段保留獨立渲染與全放鍵更新，死亡畫面不能被同更新的 R 蓋掉。
            deathPriorPhase_=enemy->phase;deathStrike_=enemy->strikeTarget;
            DeathAdvance(damageEvents_==5?3u:1u);return;
        case 3:
            // 只累積正式更新時間，擷取畫面等待不推進；留下開始、中段與穩定姿態後才按重試。
            deathWaitSeconds_+=dt;
            if(deathWaitSeconds_>=1.5f)DeathAdvance(4);
            return;
        case 4:keys['R']=true;DeathAdvance(5);return;
        case 5:
            if(retries_!=0){Fail("death-retry-duplicated");return;}
            ++retries_;DeathAdvance(6);return;
        case 6:DeathAdvance(7);return;
        case 7:
            if(retries_!=1){Fail("death-retry-not-confirmed");return;}
            if(Distance(session.feet,0,41)<=.18f&&
               (!grounded||std::abs(session.feet.y-GroundElevation(session.feet.x,session.feet.z))>.12f)){
                Fail("death-recross-not-grounded");return;
            }
            Walk(session,yaw,0,41,.18f,Step::Complete,keys);return;
        default:Fail("death-action-invalid");return;
        }
    }
    void CombatAdvance(unsigned action){combatAction_=action;BridgeProgress();}
    void TickCombat(const Session& session,float yaw,float height,const Encounter* enemy,const Evasion* evasion,bool grounded,Keys& keys){
        if(!enemy||!evasion||!enemy->Valid()||!Session::Finite(evasion->direction)||
           !std::isfinite(evasion->remaining)||!std::isfinite(evasion->cooldown)||
           evasion->remaining<0||evasion->remaining>Evasion::Duration||evasion->cooldown<0||evasion->cooldown>Evasion::Cooldown){
            Fail("combat-observation-unavailable");return;
        }
        if(!session.bridgeRepaired||session.collected!=7||session.repairMaterials!=7||std::abs(height-1.8f)>=.001f){
            Fail("combat-route-state-mismatch");return;
        }
        const unsigned expectedHealth=3-counters_-(combatAction_==7?1u:0u);
        if(enemy->health!=expectedHealth){Fail("combat-counter-health-mismatch");return;}
        const float distance=Distance(session.feet,enemy->feet.x,enemy->feet.z);
        if(combatAction_==7){
            if(!enemy->countered||dodges_!=counters_+1){Fail("combat-counter-not-confirmed");return;}
            ++counters_;
            if(checkpointRoute_&&counters_==1){
                if(enemy->phase!=EnemyPhase::Recovery||session.guardianDefeated||session.CurrentStage()!=Stage::Encounter){
                    Fail("checkpoint-first-counter-invalid");return;
                }
                Advance(Step::Checkpoint);return;
            }
            if(counters_==3){
                if(enemy->phase!=EnemyPhase::Defeated||!session.guardianDefeated||session.CurrentStage()!=Stage::Fuse){
                    Fail("combat-victory-not-confirmed");return;
                }
                Advance(groveRoute_?Step::GroveWalk:westCliffRoute_?Step::WestCliff:powerRoute_?Step::Power:Step::Complete);return;
            }
            if(enemy->phase!=EnemyPhase::Recovery){Fail("combat-counter-outside-recovery");return;}
            CombatAdvance(1);return;
        }
        if(session.guardianDefeated||session.CurrentStage()!=Stage::Encounter){Fail("combat-stage-bypassed");return;}
        if(combatAction_<=1){
            if(enemy->phase==EnemyPhase::Windup){
                if(!grounded||evasion->remaining>0||evasion->cooldown>0||distance<.001f){Fail("combat-dodge-not-ready");return;}
                lockedStrike_=enemy->strikeTarget;dashObserved_=false;
                // 斜向前閃避，避開鎖定點並減少落地後的折返距離；只送正式方向鍵與 Q。
                const float side=counters_%2? -1.f:1.f;
                constexpr float diagonal=.70710678118f;
                const float dx=(-(session.feet.z-enemy->feet.z)*side-(session.feet.x-enemy->feet.x))*diagonal/distance;
                const float dz=((session.feet.x-enemy->feet.x)*side-(session.feet.z-enemy->feet.z))*diagonal/distance;
                Steer(dx,dz,yaw,keys);keys['Q']=true;CombatAdvance(2);return;
            }
            if(combatAction_==0){
                if(Distance(session.feet,0,44)<=.18f){CombatAdvance(1);return;}
                Walk(session,yaw,0,44,.18f,Step::Combat,keys);return;
            }
            // 等待上一輪反擊後的恢復結束，不在同一恢復期連按 F。
            if(enemy->phase==EnemyPhase::Pursuit&&distance>2.15f){
                const float scale=1.95f/distance;
                Walk(session,yaw,enemy->feet.x+(session.feet.x-enemy->feet.x)*scale,
                    enemy->feet.z+(session.feet.z-enemy->feet.z)*scale,.08f,Step::Combat,keys);
            }
            return;
        }
        if(combatAction_<=3){
            if(enemy->strikeTarget.x!=lockedStrike_.x||enemy->strikeTarget.y!=lockedStrike_.y||enemy->strikeTarget.z!=lockedStrike_.z){
                Fail("combat-strike-target-changed");return;
            }
            dashObserved_|=evasion->remaining>0;
            const bool outside=Distance(session.feet,lockedStrike_.x,lockedStrike_.z)>Encounter::StrikeRadius;
            if(enemy->phase==EnemyPhase::Recovery){
                if(!dashObserved_||!outside){Fail("combat-dodge-not-proven");return;}
                ++dodges_;CombatAdvance(4);return;
            }
            if(enemy->phase!=EnemyPhase::Windup){Fail("combat-windup-skipped");return;}
            if(dashObserved_&&outside&&combatAction_==2)CombatAdvance(3);
            // 閃避落地後可在預警圈外提前轉身靠近；不等恢復期才從靜止開始加速。
            // 整段目標線都須保持預警淨空；實際位移仍只送 WASD，由碰撞與傷害判定驗收。
            if(combatAction_==3&&grounded&&evasion->remaining==0&&distance>2.2f){
                const float scale=2.05f/distance;
                const float x=enemy->feet.x+(session.feet.x-enemy->feet.x)*scale;
                const float z=enemy->feet.z+(session.feet.z-enemy->feet.z)*scale;
                const float dx=x-session.feet.x,dz=z-session.feet.z;
                const float squared=dx*dx+dz*dz;
                const float t=squared>0?std::clamp(((lockedStrike_.x-session.feet.x)*dx+
                    (lockedStrike_.z-session.feet.z)*dz)/squared,0.f,1.f):0.f;
                if(std::hypot(session.feet.x+t*dx-lockedStrike_.x,
                    session.feet.z+t*dz-lockedStrike_.z)>Encounter::StrikeRadius+.1f)
                    Walk(session,yaw,x,z,.05f,Step::Combat,keys);
            }
            return;
        }
        if(enemy->phase!=EnemyPhase::Recovery||enemy->countered){Fail("combat-counter-window-missed");return;}
        if(evasion->remaining>0||enemy->playerCooldown>0)return;
        if(combatAction_==4){
            if(distance>=1.9f&&distance<=2.2f){CombatAdvance(5);return;}
            if(distance<.001f){Fail("combat-enemy-overlap");return;}
            const float scale=2.05f/distance;
            Walk(session,yaw,enemy->feet.x+(session.feet.x-enemy->feet.x)*scale,
                enemy->feet.z+(session.feet.z-enemy->feet.z)*scale,.05f,Step::Combat,keys);return;
        }
        if(distance>2.5f||std::abs(session.feet.y-enemy->feet.y)>=2.f){Fail("combat-counter-out-of-range");return;}
        if(combatAction_==5){
            // 存檔路線需要再次正式閃避；等觀測中的冷卻結束，不改敵人或閃避計時。
            if(checkpointRoute_&&counters_==0&&evasion->cooldown>0)return;
            CombatAdvance(6);return;
        }
        // F 必須有獨立釋放更新，下一更新只讀驗證真實敵人血量，不直接呼叫戰鬥 API。
        keys['F']=true;CombatAdvance(7);
    }
    void CheckpointAdvance(unsigned action){checkpointAction_=action;checkpointWait_=0;BridgeProgress();}
    static bool CheckpointStableFields(const SaveGame& baseline,const SaveGame& current){
        // 正式更新只容許位置與時間前進；任務、敵人位置、鎖定點、短衝方向不得被換掉。
        auto expected=baseline;expected.session.feet=current.session.feet;
        expected.encounter.phaseTime=current.encounter.phaseTime;expected.encounter.playerCooldown=current.encounter.playerCooldown;
        expected.evasion.remaining=current.evasion.remaining;expected.evasion.cooldown=current.evasion.cooldown;
        // 物理快照可隨正式模擬前進；載入當步另以完整序列化逐欄精確比對。
        if(bool(expected.motion)!=bool(current.motion))return false;
        expected.motion=current.motion;
        return expected.Serialize()==current.Serialize();
    }
    void TickCheckpoint(const Session& session,float yaw,float height,const Encounter* enemy,const Evasion* evasion,bool grounded,
        float dt,const SaveGame* disk,std::string_view feedback,Keys& keys,const SaveGame* motionSnapshot){
        if(motionOnly_){TickMotionCheckpoint(motionSnapshot,disk,feedback,dt,keys);return;}
        if(!enemy||!evasion){Fail("checkpoint-observation-missing");return;}
        const SaveGame live=motionSnapshot?*motionSnapshot:SaveGame{session,*enemy,*evasion};
        // 此處只投影比較任務／敵人／短衝觀測；完整world/motion仍由live.Valid與正式載入檢查。
        const SaveGame envelope{live.session,live.encounter,live.evasion};
        if(envelope.Serialize()!=SaveGame{session,*enemy,*evasion}.Serialize()||
           (live.motion&&live.motion->controller.height!=height)){
            Fail("checkpoint-motion-observation-mismatch");return;
        }
        const bool postureValid=live.motion?
            (live.motion->controller.grounded==grounded&&
             (checkpointAction_>4||grounded)&&
             ((checkpointAction_!=5&&checkpointAction_!=9&&checkpointAction_!=10)||!grounded)):grounded;
        if(!live.Valid()||!postureValid||std::abs(height-1.8f)>=.001f||session.health!=100||session.collected!=7||
           session.repairMaterials!=7||!session.bridgeRepaired||session.guardianDefeated||session.CurrentStage()!=Stage::Encounter||
           counters_!=1||dodges_!=1||enemy->health!=2||enemy->phase!=EnemyPhase::Recovery||!enemy->countered||
           session.fuseClueRead||session.fuseCollected||session.fuseDials||session.circuit||session.powerRestored||
           session.relayLinked||session.relaySectors||session.relayPatchA||session.relayPatchB||session.relayFieldTasks||session.extractionRoute){
            Fail("checkpoint-live-state-invalid");return;
        }
        if(checkpointAction_<5&&disk){Fail("checkpoint-save-already-exists");return;}
        if(checkpointAction_>=6&&(!disk||!disk->Valid()||disk->Serialize()!=checkpointSaved_.Serialize())){
            Fail("checkpoint-disk-changed");return;
        }
        if(checkpointAction_>=6&&checkpointAction_!=9&&
           !CheckpointStableFields(checkpointSaved_,live)){Fail("checkpoint-unexpected-live-mutation");return;}
        switch(checkpointAction_){
        case 0:CheckpointAdvance(1);return;
        case 1:
            // 只保存唯讀觀測作對照；磁碟檔案的缺失由呼叫端實際讀取結果證明。
            checkpointMissingBefore_=live;keys[VK_F9]=true;CheckpointAdvance(2);return;
        case 2:
            if(feedback!="Save unavailable"||live.Serialize()!=checkpointMissingBefore_.Serialize()){
                Fail("checkpoint-missing-load-mutated-state");return;
            }
            CheckpointAdvance(3);return;
        case 3:CheckpointAdvance(4);return;
        case 4:
            if(evasion->remaining!=0||evasion->cooldown!=0||enemy->phaseTime<.55f){Fail("checkpoint-save-window-unavailable");return;}
            // 無 WASD 的 Q 是正式鏡頭後方短衝；F5 同更新末尾保存真正進展後的狀態。
            checkpointDirection_={std::sin(yaw),0,-std::cos(yaw)};
            keys['Q']=true;keys[VK_F5]=true;CheckpointAdvance(5);return;
        case 5:
            if(!disk||!disk->Valid()||disk->Serialize()!=live.Serialize()||
               evasion->remaining<=0||evasion->cooldown<=0||
               Aevrynd::Length(evasion->direction-checkpointDirection_)>.00001f){
                Fail("checkpoint-save-readback-invalid");return;
            }
            checkpointSaved_=*disk;checkpointPrevious_=live;++savedCheckpoint_;CheckpointAdvance(6);return;
        case 6:
            if(enemy->phaseTime>=checkpointPrevious_.encounter.phaseTime||evasion->cooldown>=checkpointPrevious_.evasion.cooldown||
               evasion->remaining>checkpointPrevious_.evasion.remaining){Fail("checkpoint-live-clock-not-advanced");return;}
            checkpointWait_+=dt;checkpointPrevious_=live;
            if(checkpointWait_>=.35f){
                const auto delta=session.feet-checkpointSaved_.session.feet;
                if(evasion->remaining!=0||Aevrynd::Dot(delta,checkpointSaved_.evasion.direction)<=.1f||
                   live.Serialize()==checkpointSaved_.Serialize()){Fail("checkpoint-live-did-not-diverge");return;}
                CheckpointAdvance(7);
            }
            return;
        case 7:CheckpointAdvance(8);return;
        case 8:keys[VK_F9]=true;CheckpointAdvance(9);return;
        case 9:
            if(feedback!="Loaded"||live.Serialize()!=checkpointSaved_.Serialize()){
                Fail("checkpoint-load-restore-mismatch");return;
            }
            ++loadedCheckpoint_;checkpointPrevious_=live;CheckpointAdvance(10);return;
        case 10:{
            const auto delta=session.feet-checkpointSaved_.session.feet;
            if(evasion->remaining<=0||evasion->remaining>=checkpointPrevious_.evasion.remaining||
               evasion->cooldown>=checkpointPrevious_.evasion.cooldown||enemy->phaseTime>=checkpointPrevious_.encounter.phaseTime||
               Aevrynd::Dot(delta,checkpointSaved_.evasion.direction)<=0){Fail("checkpoint-loaded-dash-not-resumed");return;}
            checkpointWait_+=dt;checkpointPrevious_=live;
            if(checkpointWait_>=.1f){
                if(savedCheckpoint_!=1||loadedCheckpoint_!=1){Fail("checkpoint-counts-invalid");return;}
                Advance(Step::Complete);
            }
            return;
        }
        default:Fail("checkpoint-action-invalid");return;
        }
    }
    void PivotAdvance(unsigned action,const SaveGame& live){
        checkpointAction_=action;pivotPhaseTick_=live.motion->locomotion.motor.tick;BridgeProgress();
    }
    static bool PivotSamePoint(Aevrynd::Vec3 a,Aevrynd::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
    // 只送真按鍵；所有計時來自前一正式處理器已提交的 motor.tick，擷取等待不計時。
    void TickPivotCheckpoint(const Session& session,float yaw,float height,const Encounter* enemy,const Evasion* evasion,bool grounded,
        const SaveGame* disk,std::string_view feedback,Keys& keys,const SaveGame* live,bool feetSettled){
        using namespace Aevrynd;
        if(!live||!live->motion||!live->Valid()||!live->Serialize().starts_with("MISTWATCH_SAVE 3\n")||!enemy||!evasion||
           pivotRound_>1||checkpointAction_>=std::size(pivotPhases_)||bridgeRoute_||combatRoute_||motionOnly_||
           session.health!=100||session.CurrentStage()!=Stage::Supplies||session.collected||session.repairMaterials||session.bridgeRepaired||
           std::abs(height-1.8f)>=.001f||evasion->remaining!=0||evasion->cooldown!=0){Fail("pivot-save-live-invalid");return;}
        const SaveGame envelope{live->session,live->encounter,live->evasion};
        const auto& p=*live->motion;const auto& c=p.controller;const auto& m=p.locomotion.motor;
        if(envelope.Serialize()!=SaveGame{session,*enemy,*evasion}.Serialize()||c.grounded!=grounded||c.height!=height||
           !PivotSamePoint(c.velocity,observedVelocity_)||p.locomotion.dodgeFlight||m.runFlight||m.wasDashing){Fail("pivot-save-observation-mismatch");return;}
        if(pivotObserved_){
            auto expected=pivotInitial_;expected.session.feet=live->session.feet;expected.motion=live->motion;
            if(expected.Serialize()!=live->Serialize()){Fail("pivot-save-nonmotion-state-changed");return;}
            if(checkpointAction_!=2&&checkpointAction_!=9){
                if(m.tick<=pivotObservedTick_||m.tick-pivotObservedTick_>12){Fail("pivot-save-normal-clock-not-advanced");return;}
                if(c.feet.x==checkpointPrevious_.session.feet.x&&c.feet.z==checkpointPrevious_.session.feet.z&&
                   p.explorerPhase!=checkpointPrevious_.motion->explorerPhase){Fail("pivot-save-zero-root-phase-advanced");return;}
            }
        }
        pivotObserved_=true;pivotObservedTick_=m.tick;checkpointPrevious_=*live;
        if((!savedCheckpoint_&&checkpointAction_!=6&&disk)||(savedCheckpoint_&&checkpointAction_!=6&&
            (!disk||!disk->Valid()||disk->Serialize()!=checkpointSaved_.Serialize()))){Fail("pivot-save-disk-unexpected");return;}
        const auto steer=[&](float sign){Steer(pivotDirection_.x*sign,pivotDirection_.z*sign,yaw,keys);};
        const auto elapsed=m.tick>=pivotPhaseTick_?m.tick-pivotPhaseTick_:std::uint64_t{};
        const auto atRest=[&]{return grounded&&Length(c.velocity)==0&&Length(m.velocity)==0;};
        const auto pivotValid=[&]{return atRest()&&PivotSamePoint(c.feet,pivotRest_)&&p.explorerDistance==0&&p.explorerSeconds>0&&
            p.explorerPhase==pivotPhase_&&std::abs(std::remainder(p.explorerYaw-pivotYaw_,6.2831853f))>.0001f&&m.heading!=pivotHeading_;};
        switch(checkpointAction_){
        case 0:pivotInitial_=*live;PivotAdvance(1,*live);return;
        case 1:checkpointMissingBefore_=*live;keys[VK_F9]=true;PivotAdvance(2,*live);return;
        case 2:
            if(feedback!="Save unavailable"||live->Serialize()!=checkpointMissingBefore_.Serialize()){Fail("pivot-save-missing-load-mutated");return;}
            pivotDirection_={-std::sin(yaw),0,std::cos(yaw)};pivotStart_=c.feet;steer(1);PivotAdvance(3,*live);return;
        case 3:
            if(elapsed>=120&&Dot(c.feet-pivotStart_,pivotDirection_)>.5f&&observedSpeed_>1){PivotAdvance(4,*live);return;}
            if(elapsed>=360){Fail("pivot-save-walk-timeout");return;}steer(1);return;
        case 4:
            if(elapsed>=120&&atRest()&&feetSettled){pivotRest_=c.feet;pivotYaw_=p.explorerYaw;pivotPhase_=p.explorerPhase;pivotHeading_=m.heading;
                steer(-1);PivotAdvance(5,*live);return;}
            if(elapsed>=360)Fail("pivot-save-stop-timeout");return;
        case 5:
            if(!atRest()||!PivotSamePoint(c.feet,pivotRest_)||p.explorerPhase!=pivotPhase_){Fail("pivot-save-turn-left-stationary-state");return;}
            if(elapsed>=(pivotRound_?60u:20u)&&pivotValid()&&!feetSettled){
                pivotBeforeSaveTick_=m.tick;steer(-1);keys[VK_F5]=true;PivotAdvance(6,*live);return;}
            if(elapsed>=88){Fail("pivot-save-active-turn-not-reached");return;}steer(-1);return;
        case 6:
            if(feedback!="Saved"||!disk||!disk->motion||!disk->Valid()||disk->Serialize()!=live->Serialize()||
               m.tick<=pivotBeforeSaveTick_||!pivotValid()||(savedCheckpoint_&&disk->Serialize()==checkpointSaved_.Serialize())){
                Fail("pivot-save-post-handler-disk-mismatch");return;}
            checkpointSaved_=*disk;++savedCheckpoint_;steer(-1);PivotAdvance(7,*live);return;
        case 7:
            if(elapsed>=120){
                if(m.tick<=checkpointSaved_.motion->locomotion.motor.tick||Distance(c.feet,checkpointSaved_.session.feet.x,checkpointSaved_.session.feet.z)<=.1f||
                   live->Serialize()==checkpointSaved_.Serialize()){Fail("pivot-save-live-did-not-change");return;}
                PivotAdvance(8,*live);return;
            }
            steer(-1);return;
        case 8:keys[VK_F9]=true;PivotAdvance(9,*live);return;
        case 9:
            if(feedback!="Loaded"||live->Serialize()!=checkpointSaved_.Serialize()){Fail("pivot-save-loaded-v3-mismatch");return;}
            ++loadedCheckpoint_;if(!pivotRound_)steer(-1);PivotAdvance(10,*live);return;
        case 10:
            if(pivotRound_){
                const auto& saved=*checkpointSaved_.motion;
                if(!atRest()||!PivotSamePoint(c.feet,saved.controller.feet)||p.explorerYaw!=saved.explorerYaw||
                   p.explorerPhase!=saved.explorerPhase||m.heading!=saved.locomotion.motor.heading){Fail("pivot-save-release-replayed-motion");return;}
                if(elapsed>=120&&feetSettled){
                    if(savedCheckpoint_!=2||loadedCheckpoint_!=2){Fail("pivot-save-counts-invalid");return;}Advance(Step::Complete);
                }
                return;
            }
            if(elapsed>=240){
                if(Dot(c.feet-checkpointSaved_.session.feet,pivotDirection_*-1.f)<=.25f||observedSpeed_<=.1f){Fail("pivot-save-resumed-S-no-progress");return;}
                PivotAdvance(11,*live);return;
            }
            steer(-1);return;
        case 11:
            if(elapsed>=120&&atRest()&&feetSettled){
                if(pivotRound_||savedCheckpoint_!=1||loadedCheckpoint_!=1){Fail("pivot-save-round-counts-invalid");return;}
                ++pivotRound_;pivotDirection_={-std::sin(yaw),0,std::cos(yaw)};pivotStart_=c.feet;steer(1);PivotAdvance(3,*live);return;
            }
            if(elapsed>=360)Fail("pivot-save-between-round-stop-timeout");return;
        default:Fail("pivot-save-action-invalid");return;
        }
    }
    // 獨立驗證正式 F5/F9 的空中續接；不取代包含戰鬥時序的完整 checkpoint 路線。
    void TickMotionCheckpoint(const SaveGame* live,const SaveGame* disk,std::string_view feedback,float dt,Keys& keys){
        if(!live||!live->Valid()||!live->motion||live->session.health!=100||live->session.collected||live->session.bridgeRepaired){
            Fail("motion-save-live-invalid");return;
        }
        const auto& c=live->motion->controller;
        switch(checkpointAction_){
        case 0:
            if(!c.grounded||Aevrynd::Length(c.velocity)>.001f||live->evasion.cooldown>0)return;
            keys[motionRound_?'Q':VK_SPACE]=true;CheckpointAdvance(1);return;
        case 1:
            if(c.grounded||c.velocity.y<=0||(motionRound_&&live->evasion.remaining<=0)){
                Fail("motion-save-launch-missing");return;
            }
            keys[VK_F5]=true;CheckpointAdvance(2);return;
        case 2:
            if(feedback!="Saved"||!disk||!disk->motion||c.grounded||disk->Serialize()!=live->Serialize()){
                Fail("motion-save-disk-mismatch");return;
            }
            checkpointSaved_=*disk;++savedCheckpoint_;CheckpointAdvance(3);return;
        case 3:
            if(!disk||disk->Serialize()!=checkpointSaved_.Serialize()){Fail("motion-save-disk-mutated");return;}
            checkpointWait_+=dt;
            if(checkpointWait_>=.12f){
                if(live->Serialize()==checkpointSaved_.Serialize()){Fail("motion-save-no-progress");return;}
                CheckpointAdvance(4);
            }
            return;
        case 4:keys[VK_F9]=true;CheckpointAdvance(5);return;
        case 5:
            if(feedback!="Loaded"||c.grounded||live->Serialize()!=checkpointSaved_.Serialize()){
                Fail("motion-save-restore-mismatch");return;
            }
            ++loadedCheckpoint_;checkpointPrevious_=*live;CheckpointAdvance(6);return;
        case 6:
            if(!disk||disk->Serialize()!=checkpointSaved_.Serialize()){Fail("motion-save-disk-mutated");return;}
            if(!c.grounded&&c.velocity.y>=checkpointPrevious_.motion->controller.velocity.y){
                Fail("motion-save-gravity-not-resumed");return;
            }
            checkpointPrevious_=*live;
            if(c.grounded){
                if(motionRound_==0){++motionRound_;CheckpointAdvance(0);}
                else if(savedCheckpoint_==2&&loadedCheckpoint_==2)Advance(Step::Complete);
                else Fail("motion-save-counts-invalid");
            }
            return;
        default:Fail("motion-save-action-invalid");return;
        }
    }
    void WestCliffAdvance(const Session& session){
        ++westCliffAction_;westCliffElapsed_=0;westCliffRestSeconds_=0;westCliffIssuedSeconds_=0;westCliffIssued_=false;
        westCliffStart_=session.feet;BridgeProgress();
    }
    void TickWestCliff(const Session& session,float yaw,float height,const Encounter* enemy,
        const Evasion* evasion,bool grounded,float dt,Keys& keys,bool feetSettled,const SaveGame* motionSnapshot,const WestCliffReplayWorld* world){
        if(!westCliffRoute_||!enemy||!evasion||!enemy->Valid()||enemy->health||enemy->phase!=EnemyPhase::Defeated||
            !session.guardianDefeated||session.CurrentStage()!=Stage::Fuse||session.health!=100||session.collected!=7||
            session.repairMaterials!=7||!session.bridgeRepaired||session.fuseClueRead||session.fuseCollected||
            session.fuseDials||session.circuit||session.powerRestored||session.relayLinked||session.relaySectors||
            session.relayFieldTasks||session.relayPatchA||session.relayPatchB||session.extractionRoute||session.beaconLit||
            dodges_!=3||counters_!=3||(!world&&!grounded)||std::abs(height-1.8f)>=.001f||evasion->remaining!=0){
            Fail("west-cliff-live-state-invalid");return;
        }
        if((westCliffStandingPolicy_&&!world)||(world&&!world->Validate(motionSnapshot,session,grounded,height,observedVelocity_))){
            Fail("west-cliff-world-motion-invalid");return;
        }
        if(westCliffAction_<6){
            // 先沿已驗證的西站接近路段，到站體東側後北繞；後續森林段仍必須真實走通。
            // -44 西側三角面過陡；沿實測東側緩坡通道南行，再向西下坡接近原測點。
            // 站立限制版先在坡頂60m停妥，再穿過58m陡面向下；不要求在不可站立的陡面煞停。
            const float middleX=westCliffStandingPolicy_?westCliffTargetX_:(westCliffTargetX_==-44.f?-43.75f:-47.f);
            const float approachZ=westCliffStandingPolicy_?60.f:58.f;
            const std::array<Aevrynd::Vec2,6> points{{{0,62},{-18,62},{-18,66},{middleX,66},{middleX,approachZ},{westCliffTargetX_,approachZ}}};
            const float x=points[westCliffAction_].x,z=points[westCliffAction_].y;
            const float tolerance=westCliffAction_>=4?.08f:.18f;
            if(Distance(session.feet,x,z)<=tolerance&&observedSpeed_<.001f){
                if(grounded&&feetSettled)WestCliffAdvance(session);
                // 到點但未停穩不能進入 Walk 的同階段 Advance 刷新停滯時限。
                return;
            }
            // 原 Walk 只有距離確實改善才刷新停滯時限；長路段不能以持續送鍵冒充進展。
            Walk(session,yaw,x,z,tolerance,Step::WestCliff,keys);return;
        }
        if(westCliffAction_>10){Fail("west-cliff-action-invalid");return;}
        const unsigned localAction=westCliffAction_-6;
        // 觀測的是上一正式更新實際收到的按鍵時間，不能把本輪即將送出的 dt 當成已完成。
        if(westCliffIssued_)westCliffElapsed_+=double(westCliffIssuedSeconds_);
        if(localAction==4){
            // 必須觀測已送出的放鍵更新：身體停下且兩腳收步後連續半秒，不拿完成前一幀冒充靜止姿勢。
            if(westCliffIssued_&&grounded&&observedVelocity_.y==0&&observedSpeed_<.001f&&feetSettled)
                westCliffRestSeconds_+=double(westCliffIssuedSeconds_);
            else westCliffRestSeconds_=0;
            if(westCliffRestSeconds_>=.5&&westCliffElapsed_<=2.){Advance(Step::Complete);return;}
            if(westCliffElapsed_>=2.){Fail("west-cliff-exit-settle-timeout");return;}
            westCliffIssued_=true;westCliffIssuedSeconds_=dt;return;
        }
        const double duration=localAction==1?1./6.:1.;
        if(westCliffElapsed_>=duration){
            if((localAction==0||localAction==3)&&westCliffStart_.z-session.feet.z<=.5f){
                Fail(localAction==0?"west-cliff-downhill-no-progress":"west-cliff-exit-no-progress");return;
            }
            WestCliffAdvance(session);return;
        }
        if(localAction!=1)Steer(0,localAction==2?1.f:-1.f,yaw,keys);
        westCliffIssued_=true;westCliffIssuedSeconds_=dt;
    }
    static void Steer(float dx,float dz,float yaw,Keys& keys){
        const float localX=dx*std::cos(yaw)+dz*std::sin(yaw);
        const float localZ=-dx*std::sin(yaw)+dz*std::cos(yaw);
        constexpr float axisThreshold=.3826834324f;
        keys['D']=localX>axisThreshold;keys['A']=localX< -axisThreshold;
        keys['W']=localZ>axisThreshold;keys['S']=localZ< -axisThreshold;
    }
    void NextPowerPoint(){++powerPoint_;powerAction_=0;BridgeProgress();}
    void SelectNextDial(){
        // 撥輪次數取自已讀線索使用的正式密碼；零次的輪盤不製造多餘輸入。
        while(dialWheel_<Session::fuseCode.size()&&dialTurn_>=Session::fuseCode[dialWheel_]){++dialWheel_;dialTurn_=0;}
    }
    void TickPower(const Session& session,float yaw,float height,Keys& keys){
        const auto& point=powerPoints_[powerPoint_];
        bool clue=expectedClue_,fuse=expectedFuse_,power=expectedPower_;
        unsigned dials=expectedDials_,circuit=expectedCircuit_;
        if(powerAction_==3){
            if(point.action==1)clue=true;
            else if(point.action==2){
                const unsigned shift=dialWheel_*2;
                dials=(dials&~(3u<<shift))|((((dials>>shift)+1u)&3u)<<shift);
            }else if(point.action==3)fuse=true;
            else if(point.action==4)circuit^=3u;
            else if(point.action==5)circuit^=4u;
            else if(point.action==6)power=true;
        }
        const auto stage=power?Stage::Relays:fuse?Stage::Generator:Stage::Fuse;
        if(session.health!=100||!session.guardianDefeated||!session.bridgeRepaired||session.collected!=7||session.repairMaterials!=7||
           counters_!=3||dodges_!=3||std::abs(height-1.8f)>=.001f||session.fuseClueRead!=clue||session.fuseCollected!=fuse||
           session.fuseDials!=dials||session.circuit!=circuit||session.powerRestored!=power||session.CurrentStage()!=stage||
           session.relayLinked||session.relaySectors||session.relayPatchA||session.relayPatchB||session.relayFieldTasks||session.extractionRoute){
            Fail("power-route-state-mismatch");return;
        }
        if(powerAction_){
            if(Distance(session.feet,point.x,point.z)>point.tolerance){Fail("power-interaction-moved");return;}
            if(powerAction_==1){powerAction_=2;BridgeProgress();return;}
            if(powerAction_==2){
                keys[point.action==2?'1'+dialWheel_:point.action==4?'1':point.action==5?'3':'E']=true;
                powerAction_=3;BridgeProgress();return;
            }
            expectedClue_=clue;expectedFuse_=fuse;expectedPower_=power;expectedDials_=dials;expectedCircuit_=circuit;
            if(point.action==2){
                ++dialTurn_;SelectNextDial();
                if(dialWheel_<Session::fuseCode.size()){powerAction_=1;BridgeProgress();return;}
            }
            if(point.action==6){Advance(completionRoute_?Step::Completion:Step::Complete);return;}
            NextPowerPoint();return;
        }
        // 到點仍須等正式速度停穩；否則慣性會在互動放鍵階段越過原容差。
        if(Distance(session.feet,point.x,point.z)<=point.tolerance&&observedSpeed_<.001f){
            if(point.action){
                if(point.action==2){
                    if(!expectedClue_){Fail("power-clue-required");return;}
                    SelectNextDial();
                    if(dialWheel_==Session::fuseCode.size()){NextPowerPoint();return;}
                }
                powerAction_=1;BridgeProgress();
            }else NextPowerPoint();
            return;
        }
        Walk(session,yaw,point.x,point.z,point.tolerance,Step::Power,keys);
    }
    void CompletionAdvance(unsigned action){completionAction_=action;BridgeProgress();}
    void NextCompletionPoint(){++completionPoint_;completionAction_=0;signalTurns_=0;BridgeProgress();}
    void TickCompletion(const Session& session,float yaw,float height,Keys& keys){
        const auto& point=completionPoints_[completionPoint_];
        const unsigned station=point.action>=1&&point.action<=3?point.action-1:0;
        unsigned patch=expectedPatchA_,route=expectedExtraction_,field=expectedFieldTasks_;
        unsigned linked=expectedLinked_,sectors=expectedSectors_;
        const bool beacon=point.action==4&&completionAction_==13;
        if(point.action>=1&&point.action<=3){
            if(completionAction_==3){if(station==0)patch=1;else if(station==2)route=1;}
            if(completionAction_==6){if(station==0)field|=FieldRelayTask::WestCircuitBit;else if(station==2)field|=FieldRelayTask::NorthWestRouteBit;}
            if(completionAction_==10){
                const unsigned shift=station*4;
                sectors=(sectors&~(15u<<shift))|(((((sectors>>shift)&15u)+1u)&15u)<<shift);
            }
            if(completionAction_==13)linked|=1u<<station;
        }
        const auto stage=beacon?Stage::Complete:linked==7&&field==FieldRelayTask::AllTasks?Stage::Beacon:Stage::Relays;
        if(session.health!=100||!session.bridgeRepaired||!session.guardianDefeated||session.collected!=7||session.repairMaterials!=7||
           counters_!=3||dodges_!=3||!session.fuseClueRead||!session.fuseCollected||session.fuseDials!=Session::solvedFuseDials||
           !session.powerRestored||session.circuit!=7||std::abs(height-1.8f)>=.001f||session.beaconLit!=beacon||
           session.CurrentStage()!=stage||session.relayPatchA!=patch||session.relayPatchB||session.extractionRoute!=route||
           session.relayFieldTasks!=field||session.relayLinked!=linked||session.relaySectors!=sectors){
            Fail("completion-route-state-mismatch");return;
        }
        if(!completionAction_){
            // 抵達站點仍需實際煞停；不可繞過 Walk 的零速門提早開始互動。
            if(Distance(session.feet,point.x,point.z)>point.tolerance||observedSpeed_>=.001f){
                Walk(session,yaw,point.x,point.z,point.tolerance,Step::Completion,keys);return;
            }
            if(!point.action){NextCompletionPoint();return;}
            if(point.action==4){CompletionAdvance(11);return;}
            if(!session.NearRelay(station)){Fail("completion-station-not-grounded-near");return;}
            CompletionAdvance(station==1?7:1);return;
        }
        // 站在盤體朝主路一側；高度只驗正式控制器觀測，不把站點高度寫入人物。
        if(Distance(session.feet,point.x,point.z)>point.tolerance||
           std::abs(session.feet.y-GroundElevation(session.feet.x,session.feet.z))>.12f||
           (point.action==4?!session.Near(Session::beacon):!session.NearRelay(station))){
            Fail("completion-interaction-position-invalid");return;
        }
        switch(completionAction_){
        case 1:CompletionAdvance(2);return;
        case 2:keys[station==0?'1':'3']=true;CompletionAdvance(3);return;
        case 3:
            expectedPatchA_=patch;expectedExtraction_=route;CompletionAdvance(4);return;
        case 4:CompletionAdvance(5);return;
        case 5:keys['E']=true;CompletionAdvance(6);return;
        case 6:expectedFieldTasks_=field;CompletionAdvance(7);return;
        case 7:{
            // 此值與 HUD 訊號同源；不取得目標扇區，也不以預算旋轉次數選答案。
            const unsigned signal=session.RelaySignal(station);
            if(signal==100){CompletionAdvance(11);return;}
            if(!CompletionScanMayTurn(signalTurns_,signal)){Fail("completion-signal-scan-exhausted");return;}
            CompletionAdvance(8);return;
        }
        case 8:CompletionAdvance(9);return;
        case 9:keys['2']=true;++signalTurns_;CompletionAdvance(10);return;
        case 10:expectedSectors_=sectors;CompletionAdvance(7);return;
        case 11:CompletionAdvance(12);return;
        case 12:
            if(point.action!=4&&session.RelaySignal(station)!=100){Fail("completion-signal-lock-lost");return;}
            keys['E']=true;CompletionAdvance(13);return;
        case 13:
            if(point.action==4){Advance(Step::Complete);return;}
            expectedLinked_=linked;NextCompletionPoint();return;
        default:Fail("completion-action-invalid");return;
        }
    }
    void Walk(const Session& session,float yaw,float x,float z,float tolerance,Step next,Keys& keys) {
        const float distance=Distance(session.feet,x,z);
        // 只觀測真控制器速度並提早鬆鍵；不得直接改位置、速度或放寬原停靠容差。
        if(braking_&&observedSpeed_>=.001f)return;
        braking_=false;
        if(distance<=tolerance&&observedSpeed_<.001f){Advance(next);return;}
        if(distance+.02f<bestDistance_){bestDistance_=distance;progressTick_=ticks_;}
        const float stoppingDistance=observedSpeed_*observedSpeed_/(2*LocomotionMotor::Deceleration);
        if(observedSpeed_>=.001f&&distance<=stoppingDistance+tolerance*.5f){braking_=true;return;}
        if(distance<=.000001f)return;
        const float dx=(x-session.feet.x)/distance,dz=(z-session.feet.z)/distance;
        // 用目前速度及同源有限轉速估計轉向行程，不把整個一公尺接近區當成必須煞停。
        // 固定大區域會在低速剛開始轉身就再次放鍵，阻止 motor 繼續旋轉。
        // 此為每幀重估的按鍵引導，不是碰撞／可達性證明；正式控制器與姿態仍逐步驗收。
        const float closingSpeed=observedVelocity_.x*dx+observedVelocity_.z*dz;
        if(observedSpeed_>=.001f&&closingSpeed<observedSpeed_*.70710678f){
            const float angle=std::acos(std::clamp(closingSpeed/observedSpeed_,-1.f,1.f));
            // 已朝外或純切線時仍先停住；只在實際開始靠近目標後允許有限轉弧。
            const float turnReserve=closingSpeed<=0?1.f:
                observedSpeed_*angle/LocomotionMotor::TurnRate+tolerance*.5f;
            if(distance<=stoppingDistance+turnReserve){braking_=true;return;}
        }
        // 正式移動是 R(yaw) * 相機相對輸入；此處只做逆旋轉，輸出仍是離散 WASD。
        // 八方向鍵盤量化，不注入類比位移；保留正式角色速度、碰撞、相機與 IK。
        Steer(dx,dz,yaw,keys);
        // 完整路線用真實煞停距離加一公尺接近區決定放開 Shift，不在每個路點前固定六公尺慢行。
        // 棚屋蹲行、戰鬥及存檔情境不啟用跑步，也不延長既有更新步數上限。
        const bool traversal=phase_==Step::WalkRoad||phase_==Step::BridgeRoute||
            phase_==Step::Power||phase_==Step::Completion;
        keys[VK_SHIFT]=completionRoute_&&traversal&&distance>stoppingDistance+1.f;
    }
};
}
