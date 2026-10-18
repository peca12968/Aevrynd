// 僅CPU稽核翻譯單元使用既有友元；不替換Engine、物理、IK或相機實作。
#define MISTWATCH_SAVE_INPUT_AUDIT 1
#define wWinMain MistwatchProductionEntryNeverCalled
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
#include <sstream>
#include <type_traits>

namespace RecoveryTrial {
using namespace Aevrynd;
using namespace Mistwatch;
unsigned checks{},frames{};
void Require(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
bool Same(Vec3 a,Vec3 b){return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(b.x)&&
    std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(b.y)&&
    std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(b.z);}
struct Hash {
    std::uint64_t value{14695981039346656037ull};
    void Word(std::uint64_t x){value^=x;value*=1099511628211ull;}
    void Float(float x){Word(std::bit_cast<std::uint32_t>(x));}
    void Double(double x){Word(std::bit_cast<std::uint64_t>(x));}
    void Vec(Vec3 v){Float(v.x);Float(v.y);Float(v.z);}
    void Text(std::string_view s){Word(s.size());for(unsigned char c:s)Word(c);}
    void Pose(const AnimationPose3D& p){Word(p.localTransforms.size());Word(p.globalMatrices.size());
        for(const auto& t:p.localTransforms){Vec(t.translation);Vec(t.rotation);Vec(t.scale);Float(t.quaternion.x);
            Float(t.quaternion.y);Float(t.quaternion.z);Float(t.quaternion.w);Word(t.usesQuaternion);}
        for(const auto& m:p.globalMatrices)for(const auto& r:m.values)for(float f:r)Float(f);}
    template<class T> void Bytes(const T& v){static_assert(std::is_trivially_copyable_v<T>);
        for(auto b:std::as_bytes(std::span{&v,1}))Word(std::to_integer<unsigned char>(b));}
};
std::string Read(const std::filesystem::path& p){std::ifstream s(p,std::ios::binary);Require(bool(s),"READ_PRIVATE_SENTINEL");
    return {std::istreambuf_iterator<char>(s),std::istreambuf_iterator<char>()};}
void Write(const std::filesystem::path& p,std::string_view v){std::ofstream s(p,std::ios::binary|std::ios::trunc);
    s.write(v.data(),static_cast<std::streamsize>(v.size()));s.close();Require(bool(s),"WRITE_PRIVATE_SENTINEL");}
constexpr std::array<BoxCollider3D,4> Walls{{{{1.3f,.5f,0},{0,5,5}},{{-1.3f,.5f,0},{0,5,5}},
    {{0,.5f,1.3f},{5,5,0}},{{0,.5f,-1.3f},{5,5,0}}}};
constexpr std::span<const ConvexAabbSweep3D* const> NoHulls{};
}

// 名稱沿用正式main既有的測試友元，沒有新增production access hook。
class MistwatchSaveInputProbe {
    using Token=Aevrynd::CharacterDirectDisplacementPreviewToken3D;
    using PairStatus=Mistwatch::PairContact::Status;
    Aevrynd::Engine& engine_;
    Aevrynd::SkinnedModel3D raw_;
    std::filesystem::path out_;
    std::ofstream trace_;

    static unsigned Cause(const MistwatchGame& g){
        return unsigned(g.playableRecoveryCause);
    }
    static std::uint64_t PoseHash(const Mistwatch::Explorer& e){
        RecoveryTrial::Hash h;h.Pose(e.pose);h.Pose(e.deathStart);h.Pose(e.deathEnd);
        for(float f:{e.phase,e.yaw,e.requestedYaw,e.locomotionDistance,e.locomotionSeconds,e.deathElapsed,e.deathMinimumClearance})h.Float(f);
        h.Vec(e.lastFeet);h.Vec(e.locomotionVelocity);h.Word(e.feetInitialized);h.Word(e.gaitGrounded);h.Word(e.nextFoot);
        h.Word(e.deathActive);h.Word(e.deathGroundedBasis);h.Word(e.deathLimited);
        for(float x:e.deathSupportGaps)h.Float(x);
        for(unsigned side=0;side<2;++side){h.Vec(e.footTargets[side]);h.Word(e.planted[side]);
            h.Float(e.footFacing[side]);h.Float(e.swingFacingStart[side]);h.Float(e.swingFacingEnd[side]);
            const auto& swing=e.footSwings[side];h.Word(swing.Ready());h.Word(swing.Active());h.Word(swing.Replanned());
            for(double d:{swing.Elapsed(),swing.Duration(),swing.LiftHeight(),swing.SpeedBound()})h.Double(d);
            for(unsigned i=0;i<5;++i){auto copy=swing;Aevrynd::FootSwingSample3D s;
                const bool ok=copy.Advance((std::max)(0.,swing.Duration()-swing.Elapsed())*i/4.,s);h.Word(ok);
                if(ok){h.Vec(s.positionWorld);h.Vec(s.velocityWorld);h.Word(s.planted);}}}
        return h.value;
    }
    static std::uint64_t WorldHash(const MistwatchGame& g){
        RecoveryTrial::Hash h;h.Text(g.session.Serialize());h.Word(g.controller.StateHash());h.Word(g.playerLocomotion.StateHash());
        const auto s=g.controller.Stats();h.Word(s.steps);h.Word(s.collisions);h.Word(s.jumps);h.Word(s.landings);h.Word(s.terrainContacts);
        h.Word(s.directTicks);h.Word(s.directBlocked);h.Word(s.directOverlaps);h.Bytes(g.controller.Settings());
        h.Word(g.controller.DirectLastTick());h.Word(g.controller.MemoryStats().retainedCapacityBytes);
        for(float f:{g.evasion.direction.x,g.evasion.direction.y,g.evasion.direction.z,g.evasion.remaining,g.evasion.cooldown})h.Float(f);
        h.Vec(g.encounter.feet);h.Vec(g.encounter.strikeTarget);h.Word(g.encounter.health);h.Word(unsigned(g.encounter.phase));
        h.Float(g.encounter.phaseTime);h.Float(g.encounter.playerCooldown);h.Word(g.encounter.countered);h.Word(g.playablePairState.lastSerial);
        h.Word(PoseHash(g.explorer));h.Pose(g.boar.pose);h.Float(g.boar.yaw);h.Float(g.boar.cycle);h.Float(g.boar.displacement);
        h.Float(g.boar.brace);h.Vec(g.boar.lastFeet);h.Word(g.boar.initialized);
        for(unsigned i=0;i<4;++i){h.Vec(g.boar.footTargets[i]);h.Word(g.boar.planted[i]);}
        h.Float(g.waterTime);h.Word(g.fieldJournalOpen);h.Word(g.started);h.Word(g.paused);
        h.Word(g.tuningInputRepeat.ArmedMask());for(unsigned i=0;i<3;++i)h.Double(double(g.tuningInputRepeat.Remaining(i)));
        h.Word(unsigned(g.actionFeedback.Event()));h.Double(g.actionFeedback.RemainingSeconds());
        h.Word(g.solids.size());for(const auto& b:g.solids){h.Vec(b.center);h.Vec(b.extents);}
        return h.value;
    }
    static std::uint64_t CameraHash(const MistwatchGame& g){RecoveryTrial::Hash h;
        h.Bytes(g.playerCameraMotion);h.Bytes(g.orbit);h.Bytes(g.lastPlayerFraming);h.Bytes(g.lastCameraMotion);h.Bytes(g.lastRenderedCamera);
        h.Word(g.cameraFixedTicks);h.Word(g.captureCameraTicks);h.Float(g.cameraMaximumTickDisplacement);h.Float(g.cameraOcclusionSeconds);return h.value;}
    static std::uint64_t LatchHash(const MistwatchGame& g){RecoveryTrial::Hash h;h.Bytes(g.playableInputs);return h.value;}
    static std::uint64_t AudioHash(const MistwatchGame& g){RecoveryTrial::Hash h;h.Bytes(g.gameAudio.director);
        for(auto n:g.gameAudio.requests)h.Word(n);h.Text(g.gameAudio.options.Serialize());return h.value;}
    void NoStartup()const{using namespace RecoveryTrial;
        Require(engine_.Window()==nullptr,"HEADLESS_CREATED_WINDOW");
        const auto s=engine_.GetRendererStats();Require(s.submittedFrames==0&&!s.adapterLuidValid,"HEADLESS_RENDERER_STARTED");
        Require(!engine_.Audio().GetStats().deviceAvailable,"HEADLESS_AUDIO_DEVICE_STARTED");}
    std::unique_ptr<MistwatchGame> Seed(Aevrynd::Vec3 feet,bool enclosed){using namespace RecoveryTrial;using namespace Mistwatch;
        auto p=std::make_unique<MistwatchGame>();auto& g=*p;
        g.terrain.Configure({{-30,0,-30},128,128,1});g.terrain.Fill(0);
        if(enclosed)g.solids.assign(Walls.begin(),Walls.end());
        g.session.collected=7;g.session.repairMaterials=7;g.session.bridgeRepaired=true;g.session.health=75;g.session.feet=feet;
        g.encounter.feet={};g.encounter.phase=EnemyPhase::Recovery;g.encounter.phaseTime=1;
        Require(g.session.Valid()&&g.encounter.Valid(),"REAL_SEED_INVALID");
        ExplorerNapeHairBinding::Apply(g.explorer);BackpackFit1250::Apply(g.explorer,{+[](void*){},nullptr});
        g.boar.model=raw_;g.started=true;g.paused=false;g.waterTime=2.5f;
        g.controller.Reset(feet);Require(g.controller.TrySetHeight(1.8f,g.solids),"REAL_SEED_HEIGHT");
        if(feet.y>0){Require(g.controller.TryRestoreMotionState({feet,{0,-1,0},1.8f,0,false},g.solids,NoHulls,g.terrain),"REAL_AIR_CONTROLLER");
            LocomotionMotorState motor;motor.phase=LocomotionPhase::JumpOrFall;
            Require(g.playerLocomotion.TryRestoreMotionState({motor,0,false},g.controller,g.evasion),"REAL_AIR_MOTOR");}
        // 零時間更新只驗證，不建立接地；以真正支撐檢查還原靜止測試起點。
        else Require(g.controller.TryRestoreMotionState({feet,{},1.8f,0,true},g.solids,NoHulls,g.terrain)&&g.controller.Grounded(),"REAL_GROUNDED_SEED");
        g.explorer.Update({},0,false);Require(g.explorer.PlaceFeet(feet,g.controller.Grounded(),false,g.terrain,g.solids,NoHulls),"REAL_EXPLORER_IK_SEED");
        g.ResetBoarPose();Require(g.UpdatePlayerCamera(1.f/60.f),"REAL_CAMERA_SEED");g.lastRenderedCamera=g.playerCameraMotion.Camera();
        g.fieldJournalOpen=true;g.actionFeedback.Publish(ActionFeedbackEvent::Saved);
        g.tuningInputRepeat.Tick(0,1,1,1);
        g.save=out_/((enclosed?"enclosed":"open")+std::to_string(RecoveryTrial::frames)+".save");
        const auto snapshot=g.CaptureSaveGame();Require(snapshot.Valid(),"REAL_SAVE_SENTINEL_INVALID");Write(g.save,snapshot.Serialize());
        NoStartup();return p;
    }
    void Tick(MistwatchGame& g,std::initializer_list<unsigned> keys,float dt=1.f/60.f){
        auto& in=engine_.GetInput();for(unsigned k=0;k<256;++k)in.SetKey(k,false);in.ConsumePresses();
        for(unsigned key:keys)in.SetKey(key,true);
        // 呼叫完整實際OnUpdate；不直接執行複製的R片段或pair mapper。
        g.OnUpdate(engine_,dt);++RecoveryTrial::frames;
        trace_<<RecoveryTrial::frames<<'\t'<<1<<'\t'<<Cause(g)<<'\t'<<g.playableNeedsContactPolicy
            <<'\t'<<unsigned(g.playableOutcome)<<'\t'<<(g.playableLastPairStatus?int(*g.playableLastPairStatus):-1)
            <<'\t'<<g.controller.Position().x<<'\t'<<g.controller.Position().y<<'\t'<<g.controller.Position().z
            <<'\t'<<g.session.health<<'\t'<<g.encounter.phaseTime<<'\t'<<g.playablePairState.lastSerial
            <<'\t'<<g.playableInputs.Count()<<'\t'<<g.playableInputs.Front().mask<<'\t'<<g.waterTime
            <<'\t'<<g.cameraFixedTicks<<'\t'<<g.inputReplayLastDamage<<'\t'<<g.message<<'\n';trace_.flush();
        in.ConsumePresses();NoStartup();
    }
    void EnclosedRecovery(){using namespace RecoveryTrial;auto p=Seed({0,12,0},true);auto& g=*p;
        Require(!Mistwatch::EncounterBodyOverlaps(g.encounter.feet,g.solids)&&Mistwatch::PairContact::AxisEnclosed(g.encounter.feet,g.solids),"ORIGINAL_ENCLOSED_INPUTS_NOT_TRUE");
        Token pending;Require(g.controller.PreviewDirectDisplacementTick({1,{},0},{},pending),"REAL_PENDING_PREVIEW_SEED");
        const auto world=WorldHash(g),audio=AudioHash(g);const auto file=Read(g.save);const auto beforeCamera=g.cameraFixedTicks;
        auto oneCamera=g.playerCameraMotion;
        const auto oneResult=oneCamera.Update(Mistwatch::BuildPlayerCameraMotionRequest(g.session.feet,g.controller.Settings().height,
            g.orbit,oneCamera.Initialized(),g.solids,g.terrain,g.RuntimeCameraOnly()),
            Mistwatch::PlayerCameraSamples(g.session.feet,g.controller.Settings().height),g.solids,
            Mistwatch::CameraCeilingSolids(),g.terrain,1.f/60.f,g.RuntimeCameraOnly());
        RecoveryTrial::Hash expectedCamera;expectedCamera.Bytes(oneCamera);
        Tick(g,{'E','F',VK_F5});
        Require(g.playableLastPairStatus&&*g.playableLastPairStatus==PairStatus::RecoveryRequired,"DID_NOT_REACH_REAL_MAIN_PAIR_RECOVERY_REQUIRED");
        Require(WorldHash(g)==world&&g.controller.CanCommitDirectDisplacementPreview(pending),"ENCLOSED_MAIN_PARTIAL_WORLD_COMMIT");
        Require(g.playableInputs.Count()==1&&g.playableInputs.Front().Pressed(Mistwatch::FrameEdge::Interact)&&
            g.playableInputs.Front().Pressed(Mistwatch::FrameEdge::Save),"ENCLOSED_MAIN_LOST_PENDING_E_SAVE");
        Require(g.cameraFixedTicks==beforeCamera+unsigned(g.playableCameraOnlyCommitted),"ENCLOSED_CAMERA_NOT_ZERO_OR_ONCE");
        RecoveryTrial::Hash actualCamera;actualCamera.Bytes(g.playerCameraMotion);
        Require(g.playableCameraOnlyCommitted==oneResult.accepted&&actualCamera.value==expectedCamera.value,"ENCLOSED_CAMERA_OWNER_NOT_ONE_REAL_ADVANCE");
        Require(AudioHash(g)==audio&&Read(g.save)==file,"ENCLOSED_AUDIO_OR_FILE_CHANGED");
        const unsigned beforeCause=Cause(g);const auto cameraTicks=g.cameraFixedTicks;Tick(g,{'R'});
        std::cout<<"REAL_MAIN_RECOVERY_R before_cause="<<beforeCause<<" after_cause="<<Cause(g)
            <<" safe_retry="<<(g.message=="Safe retry - expedition progress retained")
            <<" final_y="<<g.controller.Position().y<<" final_z="<<g.controller.Position().z
            <<" queue="<<g.playableInputs.Count()<<" old_preview_valid="<<g.controller.CanCommitDirectDisplacementPreview(pending)<<'\n';
        Require(g.message=="Safe retry - expedition progress retained","MAIN_NPC_RECOVERY_R_UNAVAILABLE");
        Require(beforeCause==2&&Cause(g)==0&&!g.playableNeedsContactPolicy,"TYPED_RECOVERY_CAUSE_NOT_CLEARED");
        Require(Same(g.session.feet,Mistwatch::Session::bridge)&&Same(g.controller.Position(),g.session.feet)&&g.session.health==100&&
            g.session.collected==7&&g.session.repairMaterials==7&&g.session.bridgeRepaired,"SAFE_RETRY_PROGRESS_OR_FEET_CHANGED");
        Require(g.encounter.Valid()&&Same(g.encounter.feet,Mistwatch::Session::arena)&&g.encounter.phase==Mistwatch::EnemyPhase::Pursuit&&
            g.encounter.phaseTime==0&&g.playablePairState.lastSerial==0,"R_CONSUMED_ENCOUNTER_TIME_OR_DAMAGE");
        Require(g.playableInputs.Count()==0&&!g.playableInputs.Overflowed()&&!g.controller.CanCommitDirectDisplacementPreview(pending),"SAFE_R_DID_NOT_CANCEL_OLD_INPUT_EPOCH");
        Require(!g.fieldJournalOpen&&!g.actionFeedback.Visible()&&g.tuningInputRepeat.ArmedMask()==0,"SAFE_R_UI_RESET_NOT_COMMITTED");
        Require(g.lastCameraMotion.accepted&&g.cameraFixedTicks==cameraTicks+1,"SAFE_R_CAMERA_NOT_ONCE");
        Require(Read(g.save)==file&&g.inputReplayLastDamage==0,"SAFE_R_WROTE_SAVE_OR_DAMAGE");
        Require(std::bit_cast<std::uint32_t>(g.waterTime)==std::bit_cast<std::uint32_t>(std::fmod(2.5f+1.f/60.f,3600.f)),"SAFE_R_WATER_NOT_SINGLE_DT");
        Tick(g,{});Require(g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted&&Cause(g)==0,"POST_R_REAL_MAIN_NOT_CONTINUABLE");
    }
    void RejectedRetry(){using namespace RecoveryTrial;auto p=Seed({0,12,0},true);auto& g=*p;Tick(g,{'E',VK_F5});
        Require(Cause(g)==2,"REJECT_CONTROL_NO_RECOVERY_CAUSE");
        // 真靜態物件阻住原Retry站位，不替換Body/IK/camera callback。
        g.solids.push_back({{0,.9f,30},{1,1,1}});
        Token pending;Require(g.controller.PreviewDirectDisplacementTick({g.controller.DirectLastTick()+1,{},0},{},pending),"REJECT_PREVIEW_SEED");
        const auto world=WorldHash(g),camera=CameraHash(g),latch=LatchHash(g),audio=AudioHash(g);const auto file=Read(g.save);
        Tick(g,{'R'});
        Require(g.message=="Safe retry unavailable - ESC pauses; prior state retained"&&Cause(g)==2,"REAL_BLOCKED_RETRY_NOT_REJECTED");
        Require(WorldHash(g)==world&&CameraHash(g)==camera&&LatchHash(g)==latch&&AudioHash(g)==audio&&Read(g.save)==file&&
            g.controller.CanCommitDirectDisplacementPreview(pending),"REJECTED_R_NOT_FULLY_ATOMIC");
    }
    void InitialOverlap(){using namespace RecoveryTrial;auto p=Seed({},false);auto& g=*p;Tick(g,{});
        Require(Cause(g)==1&&g.message=="Initial body overlap - R: safe retry / ESC: pause","INITIAL_OVERLAP_CAUSE_NOT_DISTINCT");
        Tick(g,{'R'});Require(Cause(g)==0&&g.message=="Safe retry - expedition progress retained","INITIAL_OVERLAP_REAL_R_BROKEN");}
    void OrdinaryContact(){using namespace RecoveryTrial;auto p=Seed({0,0,-2},false);auto& g=*p;bool contact{};
        for(unsigned i=0;i<24;++i){Tick(g,{'W'});Require(Cause(g)==0,"ORDINARY_SOLVABLE_CONTACT_REQUESTS_R");
            Require(g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"ORDINARY_CONTACT_FROZE_REAL_MAIN");
            contact|=g.playableCommittedContact.blockedSide||g.playableCommittedContact.blockedAbove||g.playableCommittedContact.blockedBelow;}
        Require(contact,"ORDINARY_CONTROL_DID_NOT_REACH_REAL_DYNAMIC_CONTACT");}
    void GroundedFixtureInitialization(){
        // 此處獨立計數，不消耗原五案的模擬時間或改動原763項檢查。
        unsigned checks{};
        const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
        Aevrynd::HeightField3D flat;flat.Configure({{-30,0,-30},128,128,1});flat.Fill(0);
        Aevrynd::CharacterController3D body{Aevrynd::CharacterControllerSettings3D{.jumpGraceSeconds=.1f}};
        body.Reset({});const auto reset=body.StateHash();
        check(body.Step({}, {},RecoveryTrial::NoHulls,flat,0)&&!body.Grounded()&&body.StateHash()==reset,
            "ZERO_DT_MUST_NOT_CREATE_GROUND");
        check(body.TryRestoreMotionState({{},{},1.8f,0,true},{},RecoveryTrial::NoHulls,flat)&&body.Grounded()&&RecoveryTrial::Same(body.Position(),{}),
            "SUPPORTED_GROUNDED_FIXTURE_RESTORE");
        const auto supported=body.StateHash();
        check(!body.TryRestoreMotionState({{0,1,0},{},1.8f,0,true},{},RecoveryTrial::NoHulls,flat)&&body.StateHash()==supported,
            "UNSUPPORTED_GROUNDED_FIXTURE_MUST_REJECT_ATOMICALLY");
        check(engine_.Window()==nullptr&&engine_.GetRendererStats().submittedFrames==0&&!engine_.Audio().GetStats().deviceAvailable,
            "GROUND_FIXTURE_STARTED_RUNTIME");
        std::cout<<"GROUNDED_FIXTURE_INIT_PASS checks="<<checks<<" zero_dt_noop=1 supported_restore=1 unsupported_rejected=1\n";
    }
    void FocusFixtureBoundary(){
        // 獨立計數，不改原五個案例的763檢查與151個真OnUpdate追蹤。
        unsigned boundaryChecks{};
        const auto check=[&](bool ok,const char* why){++boundaryChecks;if(!ok)throw std::runtime_error(why);};
        auto p=std::make_unique<MistwatchGame>();auto& g=*p;
        g.terrain=Mistwatch::CreateTerrain();
        const auto feet=g.FocusProbeStartFeet();g.session.feet=feet;g.controller.Reset(feet);
        const auto body=Mistwatch::PairContact::LiveBody(g.session,g.encounter);
        check(bool(body),"FOCUS_FIXTURE_MISSING_LIVE_BODY");
        Aevrynd::BoxCylinderContact3D clear;
        check(Aevrynd::QueryBoxCylinderContact3D(g.controller.Center(),g.controller.Extents(),{},*body,clear),"FOCUS_FIXTURE_QUERY_FAILED");
        check(clear.kind==Aevrynd::BoxCylinderContactKind3D::Clear,"FOCUS_FIXTURE_NOT_CLEAR");
        auto oldFeet=g.encounter.feet+Aevrynd::Vec3{0,0,-1.5f};oldFeet.y=g.terrain.SampleHeight(oldFeet.x,oldFeet.z);
        g.controller.Reset(oldFeet);Aevrynd::BoxCylinderContact3D overlap;
        check(Aevrynd::QueryBoxCylinderContact3D(g.controller.Center(),g.controller.Extents(),{},*body,overlap),"OLD_FOCUS_FIXTURE_QUERY_FAILED");
        check(overlap.kind==Aevrynd::BoxCylinderContactKind3D::StartedOverlapping,"OLD_FOCUS_OVERLAP_GUARD_WEAKENED");
        check(engine_.Window()==nullptr&&engine_.GetRendererStats().submittedFrames==0&&!engine_.Audio().GetStats().deviceAvailable,
            "FOCUS_BOUNDARY_STARTED_RUNTIME");
        std::cout<<"FOCUS_FIXTURE_BODY_BOUNDARY_PASS checks="<<boundaryChecks<<" new_clear=1 old_overlap_rejected=1 renderer_started=0 window=0\n";
    }
    void OrdinaryFall(){using namespace RecoveryTrial;auto p=Seed({0,1.31f,0},false);auto& g=*p;
        const auto landings=g.controller.Stats().landings;bool touched{};
        for(unsigned i=0;i<120;++i){Tick(g,{});
            Require(Cause(g)==0&&g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"ORDINARY_FALL_HELD_OR_SENT_TO_R");
            touched|=g.playableCommittedContact.blockedBelow;
            if(g.playableCommittedContact.blockedBelow)Require(!g.controller.Grounded(),"NPC_CAP_GRANTED_GROUND");}
        Require(touched&&g.controller.Grounded()&&g.controller.Position().y==0&&g.controller.Stats().landings==landings+1,
            "ORDINARY_FALL_DID_NOT_LEAVE_NPC_AND_REACH_TRUE_TERRAIN");}
    void CompletionCameraInspection(){
        // 新檢查在原summary之後獨立計數；任務進度用公開Session操作準備，最後E由真Main提交。
        unsigned cameraChecks{},inspectionFrames{};
        const auto check=[&](bool ok,const char* why){++cameraChecks;if(!ok)throw std::runtime_error(why);};
        Mistwatch::Session ready;
        for(unsigned i=0;i<ready.supplies.size();++i){ready.feet=ready.supplies[i];check(ready.Collect(i),"COMPLETE_PREP_SUPPLY");}
        for(unsigned i=0;i<ready.repairSites.size();++i){ready.feet=ready.repairSites[i];check(ready.CollectRepairMaterial(i,true),"COMPLETE_PREP_REPAIR_ITEM");}
        ready.feet=ready.bridge;check(ready.RepairBridge(),"COMPLETE_PREP_BRIDGE");
        ready.feet=ready.arena;check(ready.FinishEncounter(),"COMPLETE_PREP_ENCOUNTER");
        ready.feet=ready.rangerLog;check(ready.ReadFuseClue(),"COMPLETE_PREP_CLUE");
        ready.feet=ready.fuse;
        for(unsigned i=0;i<3;++i)for(unsigned turn=0;turn<ready.fuseCode[i];++turn)
            check(ready.CycleFuseDial(i),"COMPLETE_PREP_DIAL");
        check(ready.CollectFuse(),"COMPLETE_PREP_FUSE");
        ready.feet=ready.generator;check(ready.ToggleCircuit(0)&&ready.ToggleCircuit(2)&&ready.RepairGenerator(),"COMPLETE_PREP_POWER");
        for(unsigned i=0;i<3;++i){
            ready.feet=ready.relaySites[i];
            if(i==0)check(ready.CycleRelayPatch(0)&&ready.ConfirmRelayCircuit(),"COMPLETE_PREP_WEST");
            if(i==2)check(ready.CycleExtractionRoute()&&ready.ConfirmExtractionRoute(),"COMPLETE_PREP_NORTHWEST");
            for(unsigned turn=0;turn<ready.RelayTargetSector(i);++turn)check(ready.TuneRelay(i,1),"COMPLETE_PREP_ANTENNA");
            check(ready.ConnectRelay(i),"COMPLETE_PREP_CONNECT");
        }
        const auto inspectFeet=ready.beacon+Aevrynd::Vec3{0,0,-2};
        ready.feet=inspectFeet;
        check(ready.Valid()&&ready.CurrentStage()==Mistwatch::Stage::Beacon&&!ready.beaconLit,"COMPLETE_PREP_NOT_BEACON");
        const auto isolatedSeed=[&](const char* name){
            const auto previousOutput=out_,directory=out_/name;
            check(!std::filesystem::exists(directory)&&std::filesystem::create_directories(directory),"COMPLETE_FRESH_CASE_OUTPUT");
            out_=directory;
            try{auto result=Seed(inspectFeet,false);out_=previousOutput;return result;}
            catch(...){out_=previousOutput;throw;}
        };
        auto p=isolatedSeed("completion-camera");auto& g=*p;
        // 準備階段以公開狀態機走完三次反擊；不是正式Main戰鬥或玩家操作驗證。
        Mistwatch::Encounter defeated;
        for(unsigned tick=0;tick<600&&defeated.health;++tick)defeated.Tick(defeated.feet,1.f/60.f,true);
        check(defeated.Valid()&&defeated.health==0&&defeated.countered&&
            defeated.phase==Mistwatch::EnemyPhase::Defeated,"COMPLETE_PUBLIC_DEFEAT_SETUP");
        g.session=ready;g.encounter=defeated;
        check(g.encounter.Valid(),"COMPLETE_PREP_DEFEATED_ENEMY");g.ResetBoarPose();g.actionFeedback.Clear();
        auto& input=engine_.GetInput();
        const auto prepare=[&](std::initializer_list<unsigned> keys,Aevrynd::Vec2 drag={},float wheel=0){
            for(unsigned key=0;key<256;++key)input.SetKey(key,false);
            input.SetMouseButton(false);input.SetMouse({100,100});input.ConsumePresses();
            input.SetMouseButton(true);input.SetMouse({100+drag.x,100+drag.y});input.SetMouseButton(false);
            input.AddMouseWheel(wheel);for(unsigned key:keys)input.SetKey(key,true);
        };
        const auto run=[&](float dt=1.f/60.f){
            g.OnUpdate(engine_,dt);++inspectionFrames;
            check(engine_.Window()==nullptr&&engine_.GetRendererStats().submittedFrames==0&&
                !engine_.Audio().GetStats().deviceAvailable,"COMPLETION_CAMERA_STARTED_RUNTIME");
        };
        const auto deviceHash=[&]{
            RecoveryTrial::Hash h;h.Word(input.Focused());h.Word(input.FocusLost());
            for(unsigned key=0;key<256;++key){h.Word(input.Down(key));h.Word(input.Pressed(key));}
            for(const auto v:{input.Mouse(),input.MouseDelta(),input.LeftMouseDragDelta()}){h.Float(v.x);h.Float(v.y);}
            h.Float(input.MouseWheel());h.Word(input.MouseDown());h.Word(input.MousePressed());h.Word(input.RightMousePressed());
            return h.value;
        };
        prepare({'E'});run();
        check(g.session.beaconLit&&g.session.Alive()&&g.session.CurrentStage()==Mistwatch::Stage::Complete&&
            g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"TRUE_MAIN_E_DID_NOT_COMPLETE");
        check(g.CaptureSaveGame().Valid()&&g.playerCameraMotion.Initialized(),"COMPLETE_SNAPSHOT_OR_CAMERA_INVALID");
        // 輸入佇列正例來自公開Input/Observe，鏡頭檢視不得提交或清除世界邊沿。
        prepare({VK_SPACE});check(g.playableInputs.Observe(input,{0,0,1})&&g.playableInputs.Count()==1,"COMPLETE_PENDING_EDGE_SETUP");
        Token pending;check(g.controller.PreviewDirectDisplacementTick({g.controller.DirectLastTick()+1,{},0},{},pending),"COMPLETE_PENDING_BODY_PREVIEW");
        const auto frozen=WorldHash(g),frozenLatch=LatchHash(g),frozenPose=PoseHash(g.explorer);
        const auto frozenSave=g.CaptureSaveGame().Serialize(),frozenDisk=RecoveryTrial::Read(g.save);
        const auto initialCamera=g.playerCameraMotion.Camera();
        const auto yaw=[](const Aevrynd::Camera3D& c){const auto v=c.position-c.target;return std::atan2(v.x,-v.z);};
        const auto safeCamera=[&](const Aevrynd::Camera3D& previous){
            const auto& c=g.playerCameraMotion.Camera();const float dt=1.f/60.f;
            check(g.lastCameraMotion.accepted&&g.lastCameraMotion.status!=Aevrynd::ThirdPersonCameraMotionStatus3D::ResetCut,
                "COMPLETE_CAMERA_NOT_NORMAL_ACCEPTED_UPDATE");
            check(Aevrynd::Length(c.position-previous.position)<=30.f*dt+.00002f&&
                std::abs(std::remainder(yaw(c)-yaw(previous),6.2831853f))<=2.4f*dt+.00001f,"COMPLETE_CAMERA_RATE");
            check(Aevrynd::ThirdPersonCameraPathClear3D(previous.position,c.position,g.solids,g.terrain,g.RuntimeCameraOnly(),.16f),
                "COMPLETE_CAMERA_PATH");
            for(const auto sample:Mistwatch::PlayerCameraSamples(g.session.feet,g.controller.Settings().height))
                check(Aevrynd::ProjectToViewport(sample,c,{1280,720}).visible,"COMPLETE_FLAT_FIXTURE_FRAMING");
        };
        for(unsigned frame=0;frame<24;++frame){
            const auto camera=g.playerCameraMotion.Camera();const auto fixed=g.cameraFixedTicks;
            prepare({'W','Q','E',VK_SPACE},{-2,0});run();
            check(g.cameraFixedTicks==fixed+1,"COMPLETE_ORBIT_NOT_ONE_CAMERA_UPDATE");safeCamera(camera);
            check(WorldHash(g)==frozen&&PoseHash(g.explorer)==frozenPose&&g.CaptureSaveGame().Serialize()==frozenSave,
                "COMPLETE_ORBIT_ADVANCED_WORLD");
            check(LatchHash(g)==frozenLatch&&g.controller.CanCommitDirectDisplacementPreview(pending)&&
                RecoveryTrial::Read(g.save)==frozenDisk,"COMPLETE_ORBIT_CONSUMED_INPUT_OR_SAVE");
        }
        check(std::abs(std::remainder(yaw(g.playerCameraMotion.Camera())-yaw(initialCamera),6.2831853f))>.05f,"COMPLETE_ORBIT_NO_VISIBLE_RESPONSE");
        const float rangeBefore=Aevrynd::Length(g.playerCameraMotion.Camera().position-g.playerCameraMotion.Camera().target);
        for(unsigned frame=0;frame<24;++frame){
            const auto camera=g.playerCameraMotion.Camera();prepare({}, {},.05f);run();
            check(WorldHash(g)==frozen&&LatchHash(g)==frozenLatch&&g.controller.CanCommitDirectDisplacementPreview(pending),
                "COMPLETE_ZOOM_ADVANCED_WORLD_OR_INPUT");safeCamera(camera);
        }
        check(Aevrynd::Length(g.playerCameraMotion.Camera().position-g.playerCameraMotion.Camera().target)<rangeBefore-.1f,
            "COMPLETE_ZOOM_NO_VISIBLE_RESPONSE");
        for(const float rejectedDt:{0.f,std::numeric_limits<float>::quiet_NaN()}){
            prepare({'W','Q',VK_SPACE},{-8,3},.5f);
            const auto camera=CameraHash(g),world=WorldHash(g),latch=LatchHash(g),device=deviceHash(),pose=PoseHash(g.explorer);
            run(rejectedDt);
            check(CameraHash(g)==camera&&WorldHash(g)==world&&LatchHash(g)==latch&&deviceHash()==device&&PoseHash(g.explorer)==pose&&
                g.controller.CanCommitDirectDisplacementPreview(pending),"COMPLETE_CAMERA_REJECT_NOT_FULLY_ATOMIC");
        }
        const auto beforePauseCamera=CameraHash(g);prepare({VK_ESCAPE},{-8,0},.5f);run();
        check(g.paused&&CameraHash(g)==beforePauseCamera,"COMPLETE_ESCAPE_DID_NOT_PAUSE_CAMERA");
        const auto pausedWorld=WorldHash(g),pausedCamera=CameraHash(g);const auto pausedDisk=RecoveryTrial::Read(g.save);
        prepare({VK_F5,'W','Q'},{-8,2},.5f);run();
        check(WorldHash(g)==pausedWorld&&CameraHash(g)==pausedCamera&&RecoveryTrial::Read(g.save)==pausedDisk,
            "COMPLETE_EXPLICIT_PAUSE_CHANGED");
        prepare({VK_ESCAPE});run();check(!g.paused&&g.session.beaconLit,"COMPLETE_ESCAPE_DID_NOT_RESUME");
        const auto beforeSave=g.CaptureSaveGame().Serialize();const auto beforeSaveBody=g.controller.StateHash(),beforeSaveOwner=g.playerLocomotion.StateHash();
        const auto beforeSavePose=PoseHash(g.explorer);const float beforeSaveWater=g.waterTime;
        prepare({VK_F5},{-2,0},.05f);run();
        check(g.CaptureSaveGame().Serialize()==beforeSave&&g.controller.StateHash()==beforeSaveBody&&
            g.playerLocomotion.StateHash()==beforeSaveOwner&&PoseHash(g.explorer)==beforeSavePose&&g.waterTime==beforeSaveWater,
            "COMPLETE_F5_ADVANCED_WORLD");
        check(g.message=="Saved"||g.message=="Save position unavailable"||g.message=="Save failed","COMPLETE_F5_WAS_SWALLOWED");
        if(g.message=="Saved")check(RecoveryTrial::Read(g.save)==beforeSave,"COMPLETE_F5_WRONG_SNAPSHOT");
        else check(RecoveryTrial::Read(g.save)==frozenDisk,"COMPLETE_REJECTED_F5_REPLACED_DISK");
        // 死亡沿原路徑，仍不開放鏡頭；不要求原死亡動畫或原水面時鐘凍結。
        auto dead=isolatedSeed("death-camera-control");dead->session.Damage(100);dead->actionFeedback.Clear();
        const auto deadCamera=CameraHash(*dead);prepare({}, {-8,2},.5f);
        dead->OnUpdate(engine_,1.f/60.f);++inspectionFrames;
        check(CameraHash(*dead)==deadCamera&&!dead->session.Alive(),"COMPLETION_FEATURE_BROADENED_TO_DEATH");
        prepare({'R',VK_F5});run();
        check(!g.session.beaconLit&&g.session.CurrentStage()==Mistwatch::Stage::Supplies&&g.message=="New expedition"&&
            !g.fieldJournalOpen&&g.playableInputs.Count()==0,"COMPLETE_R_F5_PRECEDENCE_CHANGED");
        std::cout<<"COMPLETION_CAMERA_INSPECTION_CPU_PASS checks="<<cameraChecks<<" frames="<<inspectionFrames
            <<" true_Main_E_complete=1 world_frozen=1 orbit_zoom=1 rejected_camera_atomic=1 pause_unchanged=1 dead_unchanged=1 R_F5_preserved=1 GPU=0\n";
    }
    static std::uint64_t ResumeWorldHash(const MistwatchGame& g){
        RecoveryTrial::Hash h;h.Text(g.session.Serialize());h.Word(g.controller.StateHash());h.Word(g.playerLocomotion.StateHash());
        const auto s=g.controller.Stats();h.Word(s.steps);h.Word(s.collisions);h.Word(s.jumps);h.Word(s.landings);h.Word(s.terrainContacts);
        h.Word(s.directTicks);h.Word(s.directBlocked);h.Word(s.directOverlaps);h.Bytes(g.controller.Settings());
        h.Word(g.controller.DirectLastTick());h.Word(g.controller.MemoryStats().retainedCapacityBytes);
        for(float f:{g.evasion.direction.x,g.evasion.direction.y,g.evasion.direction.z,g.evasion.remaining,g.evasion.cooldown})h.Float(f);
        h.Vec(g.encounter.feet);h.Vec(g.encounter.strikeTarget);h.Word(g.encounter.health);h.Word(unsigned(g.encounter.phase));
        h.Float(g.encounter.phaseTime);h.Float(g.encounter.playerCooldown);h.Word(g.encounter.countered);h.Word(g.playablePairState.lastSerial);
        h.Word(PoseHash(g.explorer));h.Pose(g.boar.pose);h.Float(g.boar.yaw);h.Float(g.boar.cycle);h.Float(g.boar.displacement);
        h.Float(g.boar.brace);h.Vec(g.boar.lastFeet);h.Word(g.boar.initialized);
        for(unsigned i=0;i<4;++i){h.Vec(g.boar.footTargets[i]);h.Word(g.boar.planted[i]);}
        h.Float(g.waterTime);h.Word(g.fieldJournalOpen);h.Word(g.started);
        h.Word(g.tuningInputRepeat.ArmedMask());for(unsigned i=0;i<3;++i)h.Double(double(g.tuningInputRepeat.Remaining(i)));
        h.Word(unsigned(g.actionFeedback.Event()));h.Double(g.actionFeedback.RemainingSeconds());
        h.Word(g.solids.size());for(const auto& b:g.solids){h.Vec(b.center);h.Vec(b.extents);}
        return h.value;
    }
    static std::uint64_t SurveyWorldHash(const MistwatchGame& g){
        // 沿原完整世界摘要，僅排除本輪合法改變的 journal/paused 展示旗標。
        RecoveryTrial::Hash h;h.Text(g.session.Serialize());h.Word(g.controller.StateHash());h.Word(g.playerLocomotion.StateHash());
        const auto s=g.controller.Stats();h.Word(s.steps);h.Word(s.collisions);h.Word(s.jumps);h.Word(s.landings);h.Word(s.terrainContacts);
        h.Word(s.directTicks);h.Word(s.directBlocked);h.Word(s.directOverlaps);h.Bytes(g.controller.Settings());
        h.Word(g.controller.DirectLastTick());h.Word(g.controller.MemoryStats().retainedCapacityBytes);
        for(float f:{g.evasion.direction.x,g.evasion.direction.y,g.evasion.direction.z,g.evasion.remaining,g.evasion.cooldown})h.Float(f);
        h.Vec(g.encounter.feet);h.Vec(g.encounter.strikeTarget);h.Word(g.encounter.health);h.Word(unsigned(g.encounter.phase));
        h.Float(g.encounter.phaseTime);h.Float(g.encounter.playerCooldown);h.Word(g.encounter.countered);h.Word(g.playablePairState.lastSerial);
        h.Word(PoseHash(g.explorer));h.Pose(g.boar.pose);h.Float(g.boar.yaw);h.Float(g.boar.cycle);h.Float(g.boar.displacement);
        h.Float(g.boar.brace);h.Vec(g.boar.lastFeet);h.Word(g.boar.initialized);
        for(unsigned i=0;i<4;++i){h.Vec(g.boar.footTargets[i]);h.Word(g.boar.planted[i]);}
        h.Float(g.waterTime);h.Word(g.started);
        h.Word(g.tuningInputRepeat.ArmedMask());for(unsigned i=0;i<3;++i)h.Double(double(g.tuningInputRepeat.Remaining(i)));
        h.Word(unsigned(g.actionFeedback.Event()));h.Double(g.actionFeedback.RemainingSeconds());
        h.Word(g.solids.size());for(const auto& b:g.solids){h.Vec(b.center);h.Vec(b.extents);}
        return h.value;
    }
    void SurveyMapControls(){
        // 真 OnUpdate/真 Input；不呼叫 OnRender，也不把 ShouldShow 當成 GPU 圖像證明。
        const auto oldChecks=RecoveryTrial::checks,oldFrames=RecoveryTrial::frames;
        unsigned controlChecks{},controlFrames{},controlCases{};const char* currentCase="setup";
        const auto check=[&](bool ok,const char* why){++controlChecks;if(!ok){
            std::cerr<<"SURVEY_MAP_CONTROLS_FIRST_DIVERGENCE case="<<currentCase<<" frame="<<controlFrames<<" reason="<<why<<'\n';
            throw std::runtime_error(why);}};
        auto& input=engine_.GetInput();
        const auto prepare=[&](std::initializer_list<unsigned> keys){
            for(unsigned k=0;k<256;++k)input.SetKey(k,false);
            input.SetMouseButton(false);input.SetRightMouseButton(false);input.ConsumePresses();
            for(unsigned k:keys)input.SetKey(k,true);
        };
        const auto read=[&](const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);
            check(bool(f),"SURVEY_READ_SAVE");return std::string{std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};};
        std::ofstream trace(out_/"survey-map-controls.tsv",std::ios::binary);
        check(bool(trace),"SURVEY_TRACE_CREATE");trace<<std::setprecision(std::numeric_limits<float>::max_digits10)
            <<"case\tframe\tstarted\tpaused\talive\tjournal\tmap_gate\tJ_down\tJ_pressed\tx\ty\tz\tworld\tcamera\tlatch\n";
        const auto mapGate=[&](const MistwatchGame& g){
            return Mistwatch::SurveyMap::ShouldShow(g.started,g.paused,g.session.Alive(),g.fieldJournalOpen);};
        const auto run=[&](MistwatchGame& g){
            const bool down=input.Down('J'),pressed=input.Pressed('J');
            g.OnUpdate(engine_,1.f/60.f);++controlFrames;
            trace<<currentCase<<'\t'<<controlFrames<<'\t'<<g.started<<'\t'<<g.paused<<'\t'<<g.session.Alive()
                <<'\t'<<g.fieldJournalOpen<<'\t'<<mapGate(g)<<'\t'<<down<<'\t'<<pressed
                <<'\t'<<g.controller.Position().x<<'\t'<<g.controller.Position().y<<'\t'<<g.controller.Position().z
                <<'\t'<<SurveyWorldHash(g)<<'\t'<<CameraHash(g)<<'\t'<<LatchHash(g)<<'\n';trace.flush();
            check(bool(trace),"SURVEY_TRACE_WRITE");
            check(engine_.Window()==nullptr&&engine_.GetRendererStats().submittedFrames==0&&
                  !engine_.Audio().GetStats().deviceAvailable,"SURVEY_STARTED_RUNTIME");
        };
        const auto seed=[&](const char* name){
            auto p=std::make_unique<MistwatchGame>();auto& g=*p;
            const Aevrynd::Vec3 feet{0,0,12};g.terrain.Configure({{-30,0,-30},128,128,1});g.terrain.Fill(0);
            g.session.collected=7;g.session.repairMaterials=7;g.session.bridgeRepaired=true;g.session.health=75;g.session.feet=feet;
            g.encounter.feet={};g.encounter.phase=Mistwatch::EnemyPhase::Recovery;g.encounter.phaseTime=1;
            check(g.session.Valid()&&g.encounter.Valid(),"SURVEY_VALID_FIXTURE");
            Mistwatch::ExplorerNapeHairBinding::Apply(g.explorer);Mistwatch::BackpackFit1250::Apply(g.explorer,{+[](void*){},nullptr});
            g.boar.model=raw_;g.started=true;g.waterTime=2.5f;g.controller.Reset(feet);
            check(g.controller.TryRestoreMotionState({feet,{},1.8f,0,true},g.solids,RecoveryTrial::NoHulls,g.terrain),"SURVEY_GROUNDED_FIXTURE");
            g.explorer.Update({},0,false);
            check(g.explorer.PlaceFeet(feet,true,false,g.terrain,g.solids,RecoveryTrial::NoHulls),"SURVEY_IK_FIXTURE");
            g.ResetBoarPose();check(g.UpdatePlayerCamera(1.f/60.f),"SURVEY_CAMERA_FIXTURE");g.lastRenderedCamera=g.playerCameraMotion.Camera();
            g.fieldJournalOpen=false;g.actionFeedback.Clear();g.tuningInputRepeat.Reset();
            const auto directory=out_/name;
            check(!std::filesystem::exists(directory)&&std::filesystem::create_directory(directory),"SURVEY_FRESH_CASE_OUTPUT");
            g.save=directory/"checkpoint.save";
            for(unsigned tick=0;tick<8;++tick){prepare({});run(g);
                check(g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"SURVEY_NATURAL_SETTLE_REJECTED");}
            const auto snapshot=g.CaptureSaveGame();check(snapshot.Valid(),"SURVEY_SAVE_SETUP");
            {std::ofstream file(g.save,std::ios::binary);const auto data=snapshot.Serialize();
                file.write(data.data(),static_cast<std::streamsize>(data.size()));file.close();check(bool(file),"SURVEY_WRITE_SAVE");}
            return p;
        };
        currentCase="paused-J-map-and-resume";++controlCases;
        auto p=seed("survey-paused-controls");auto& g=*p;
        const auto beforeSession=g.session.Serialize(),beforeDisk=read(g.save);
        const auto beforeBody=g.controller.StateHash(),beforePose=PoseHash(g.explorer),beforeCamera=CameraHash(g);
        const auto beforeEnemy=g.encounter.phaseTime;const auto beforeWater=g.waterTime;
        check(g.encounter.phase==Mistwatch::EnemyPhase::Recovery&&beforeEnemy>1.f/60.f,"SURVEY_RECOVERY_TIMER_FIXTURE");
        prepare({VK_ESCAPE,'W',VK_F5});run(g);
        check(g.started&&g.paused&&!g.fieldJournalOpen&&mapGate(g),"SURVEY_ESC_DID_NOT_SHOW_MAP_GATE");
        check(g.session.Serialize()==beforeSession&&g.controller.StateHash()==beforeBody&&PoseHash(g.explorer)==beforePose&&
              CameraHash(g)==beforeCamera&&g.encounter.phaseTime==beforeEnemy&&g.waterTime==beforeWater&&read(g.save)==beforeDisk,
              "SURVEY_PAUSE_ENTRY_ADVANCED_WORLD_OR_SAVE");
        const auto world=SurveyWorldHash(g),camera=CameraHash(g),latch=LatchHash(g);
        const auto save=g.CaptureSaveGame().Serialize(),disk=read(g.save);
        Token pending;check(g.controller.PreviewDirectDisplacementTick({g.controller.DirectLastTick()+1,{},0},{},pending),"SURVEY_PENDING_PREVIEW");
        const auto frozen=[&]{check(SurveyWorldHash(g)==world&&CameraHash(g)==camera&&LatchHash(g)==latch&&
            g.CaptureSaveGame().Serialize()==save&&read(g.save)==disk&&g.controller.CanCommitDirectDisplacementPreview(pending),
            "SURVEY_PAUSED_OR_RESUME_WORLD_CAMERA_SAVE_LATCH_CHANGED");};
        prepare({'J','W',VK_F5});input.SetMouse({100,100});input.SetMouseButton(true);input.SetMouse({108,103});
        input.SetMouseButton(false);input.AddMouseWheel(.5f);run(g);
        check(g.paused&&g.fieldJournalOpen&&!mapGate(g),"SURVEY_J_DID_NOT_HIDE_MAP_GATE");frozen();
        for(unsigned tick=0;tick<3;++tick){input.ConsumePresses();input.SetKey('J',true);
            check(input.Down('J')&&!input.Pressed('J'),"SURVEY_HELD_J_NOT_REAL_CONSUMED_INPUT");run(g);
            check(g.paused&&g.fieldJournalOpen&&!mapGate(g),"SURVEY_HELD_J_RETOGGLED");frozen();}
        input.SetKey('J',false);input.ConsumePresses();input.SetKey('J',true);
        check(input.Pressed('J'),"SURVEY_FRESH_J_EDGE_MISSING");run(g);
        check(g.paused&&!g.fieldJournalOpen&&mapGate(g),"SURVEY_SECOND_J_DID_NOT_RESTORE_MAP_GATE");frozen();
        input.ConsumePresses();input.SetKey('J',true);run(g);
        check(!g.fieldJournalOpen&&mapGate(g),"SURVEY_CLOSED_HELD_J_RETOGGLED");frozen();
        input.ConsumePresses();input.SetKey('J',false);input.SetKey(VK_ESCAPE,true);
        check(input.Down('W')&&input.Pressed(VK_ESCAPE),"SURVEY_RESUME_INPUT_NOT_HELD_W");run(g);
        check(!g.paused&&!g.fieldJournalOpen&&!mapGate(g),"SURVEY_ESC_DID_NOT_RESUME");frozen();
        const auto feet=g.controller.Position();const auto cameraTicks=g.cameraFixedTicks;input.SetKey(VK_ESCAPE,false);input.ConsumePresses();run(g);
        check(input.Down('W')&&g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted&&
              Aevrynd::Length(g.controller.Position()-feet)>.001f&&g.waterTime>beforeWater&&g.encounter.phaseTime<beforeEnemy,
              "SURVEY_HELD_W_NEXT_UPDATE_DID_NOT_MOVE");
        check(g.cameraFixedTicks==cameraTicks+1&&!g.controller.CanCommitDirectDisplacementPreview(pending)&&
              g.playableInputs.Count()==0&&read(g.save)==disk&&!mapGate(g),"SURVEY_NEXT_UPDATE_CAMERA_INPUT_SAVE_CHANGED");
        currentCase="active-J-does-not-pause";++controlCases;
        auto active=seed("survey-active-controls");const auto activeDisk=read(active->save);const auto activeWater=active->waterTime;
        prepare({'J'});run(*active);
        check(active->started&&!active->paused&&active->fieldJournalOpen&&!mapGate(*active)&&active->waterTime>activeWater&&
              active->playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"SURVEY_ACTIVE_J_CHANGED_EXISTING_SEMANTICS");
        input.ConsumePresses();input.SetKey('J',true);run(*active);
        check(active->fieldJournalOpen&&!active->paused&&!mapGate(*active)&&read(active->save)==activeDisk,"SURVEY_ACTIVE_HELD_J_RETOGGLED");
        currentCase="dead-J-is-existing-journal-only";++controlCases;
        auto dead=seed("survey-dead-controls");dead->session.Damage(100);const auto deadCamera=CameraHash(*dead);const auto deadSession=dead->session.Serialize();
        const auto deadDisk=read(dead->save);prepare({'J'});run(*dead);
        check(!dead->session.Alive()&&!dead->paused&&dead->fieldJournalOpen&&!mapGate(*dead)&&
              CameraHash(*dead)==deadCamera&&dead->session.Serialize()==deadSession,"SURVEY_DEAD_J_CHANGED_EXISTING_SEMANTICS");
        input.ConsumePresses();input.SetKey('J',true);run(*dead);
        check(dead->fieldJournalOpen&&!mapGate(*dead)&&read(dead->save)==deadDisk,"SURVEY_DEAD_HELD_J_RETOGGLED");
        currentCase="prestart-J-does-not-start";++controlCases;
        auto intro=seed("survey-prestart-controls");intro->started=false;const auto introCamera=CameraHash(*intro),introBody=intro->controller.StateHash();
        const auto introSession=intro->session.Serialize(),introDisk=read(intro->save);prepare({'J'});run(*intro);
        check(!intro->started&&!intro->paused&&intro->fieldJournalOpen&&!mapGate(*intro)&&CameraHash(*intro)==introCamera&&
              intro->controller.StateHash()==introBody&&intro->session.Serialize()==introSession,"SURVEY_PRESTART_J_CHANGED_EXISTING_SEMANTICS");
        input.ConsumePresses();input.SetKey('J',true);run(*intro);
        check(!intro->started&&intro->fieldJournalOpen&&!mapGate(*intro)&&read(intro->save)==introDisk,"SURVEY_PRESTART_HELD_J_RETOGGLED");
        prepare({});trace.close();check(bool(trace),"SURVEY_TRACE_CLOSE");
        check(RecoveryTrial::checks==oldChecks&&RecoveryTrial::frames==oldFrames,"SURVEY_CHANGED_ORIGINAL_COUNTERS");
        std::cout<<"SURVEY_MAP_CONTROLS_CPU_PASS cases="<<controlCases<<" checks="<<controlChecks<<" frames="<<controlFrames
            <<" true_OnUpdate=1 real_Input=1 held_J_no_repeat=1 paused_world_frozen=1 resume_boundary_frozen=1 held_W_next_update=1"
            <<" original_counters_unchanged=1 OnRender=0 GPU=0\n";
    }
    void ResumeActionBarrier(){
        // 恢復與暫停輸入的取樣邊界；獨立計數與真OnUpdate，不改原追蹤或既有案例。
        const auto oldChecks=RecoveryTrial::checks,oldFrames=RecoveryTrial::frames;
        unsigned barrierChecks{},barrierFrames{},cases{};
        const auto check=[&](bool ok,const char* why){++barrierChecks;if(!ok)throw std::runtime_error(why);};
        auto& input=engine_.GetInput();
        const auto prepare=[&](std::initializer_list<unsigned> keys){
            for(unsigned key=0;key<256;++key)input.SetKey(key,false);
            input.SetMouseButton(false);input.ConsumePresses();for(unsigned key:keys)input.SetKey(key,true);
        };
        const auto read=[&](const std::filesystem::path& path){
            std::ifstream file(path,std::ios::binary);check(bool(file),"RESUME_READ_PRIVATE_SAVE");
            return std::string{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        };
        const auto run=[&](MistwatchGame& g){
            g.OnUpdate(engine_,1.f/60.f);++barrierFrames;
            check(engine_.Window()==nullptr&&engine_.GetRendererStats().submittedFrames==0&&
                !engine_.Audio().GetStats().deviceAvailable,"RESUME_STARTED_RUNTIME");
        };
        for(const unsigned action:{unsigned(VK_SPACE),unsigned('Q'),unsigned('E'),unsigned('F'),unsigned('1'),unsigned(VK_F5),unsigned('R')}){
            ++cases;auto p=std::make_unique<MistwatchGame>();auto& g=*p;
            const Aevrynd::Vec3 feet=action=='E'?Mistwatch::Session::supplies[0]:
                action=='F'?Aevrynd::Vec3{0,0,-2}:action=='1'?Mistwatch::Session::fuse:Aevrynd::Vec3{0,0,12};
            g.terrain.Configure({{-30,0,-30},128,128,1});g.terrain.Fill(0);
            g.session.collected=7;g.session.repairMaterials=7;g.session.bridgeRepaired=true;g.session.health=75;g.session.feet=feet;
            g.encounter.feet={};g.encounter.phase=Mistwatch::EnemyPhase::Recovery;g.encounter.phaseTime=1;
            if(action=='E'){g.session={};g.session.feet=feet;g.encounter={};}
            if(action=='1'){
                Mistwatch::Session ready;
                for(unsigned i=0;i<3;++i){ready.feet=ready.supplies[i];check(ready.Collect(i),"RESUME_TUNE_SUPPLY");}
                for(unsigned i=0;i<3;++i){ready.feet=ready.repairSites[i];check(ready.CollectRepairMaterial(i,true),"RESUME_TUNE_MATERIAL");}
                ready.feet=ready.bridge;check(ready.RepairBridge(),"RESUME_TUNE_BRIDGE");
                ready.feet=ready.arena;check(ready.FinishEncounter(),"RESUME_TUNE_ENCOUNTER");
                ready.feet=ready.rangerLog;check(ready.ReadFuseClue(),"RESUME_TUNE_CLUE");ready.feet=feet;g.session=ready;
                Mistwatch::Encounter defeated;
                for(unsigned tick=0;tick<600&&defeated.health;++tick)defeated.Tick(defeated.feet,1.f/60.f,true);
                check(defeated.Valid()&&!defeated.health&&defeated.countered,"RESUME_TUNE_PUBLIC_DEFEAT");g.encounter=defeated;
            }
            check(g.session.Valid()&&g.encounter.Valid(),"RESUME_VALID_PUBLIC_FIXTURE");
            Mistwatch::ExplorerNapeHairBinding::Apply(g.explorer);Mistwatch::BackpackFit1250::Apply(g.explorer,{+[](void*){},nullptr});
            g.boar.model=raw_;g.started=true;g.waterTime=2.5f;g.controller.Reset(feet);
            check(g.controller.Settings().jumpGraceSeconds==.1f&&
                g.controller.TryRestoreMotionState({feet,{},1.8f,0,true},g.solids,RecoveryTrial::NoHulls,g.terrain),
                "RESUME_REAL_GROUNDED_FIXTURE");
            g.explorer.Update({},0,false);
            check(g.explorer.PlaceFeet(feet,true,false,g.terrain,g.solids,RecoveryTrial::NoHulls),"RESUME_REAL_IK_FIXTURE");
            g.ResetBoarPose();check(g.UpdatePlayerCamera(1.f/60.f),"RESUME_REAL_CAMERA_FIXTURE");g.lastRenderedCamera=g.playerCameraMotion.Camera();
            const auto directory=out_/("resume-"+std::to_string(action));
            check(!std::filesystem::exists(directory)&&std::filesystem::create_directory(directory),"RESUME_FRESH_OUTPUT");
            g.save=directory/"checkpoint.save";
            for(unsigned tick=0;tick<8;++tick){prepare({});run(g);
                check(g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"RESUME_NATURAL_SETTLE_REJECTED");}
            if(action=='R')g.session.Damage(100);
            const auto save=g.CaptureSaveGame();check(save.Valid(),"RESUME_TRUE_SAVE_SETUP");
            {std::ofstream file(g.save,std::ios::binary);const auto data=save.Serialize();file.write(data.data(),static_cast<std::streamsize>(data.size()));
                file.close();check(bool(file),"RESUME_WRITE_PRIVATE_SAVE");}
            g.actionFeedback.Publish(Mistwatch::ActionFeedbackEvent::Saved);
            prepare({VK_ESCAPE});run(g);check(g.paused,"RESUME_ENTER_PAUSE");
            Token pending;check(g.controller.PreviewDirectDisplacementTick({g.controller.DirectLastTick()+1,{},0},{},pending),"RESUME_PENDING_PREVIEW");
            const auto world=ResumeWorldHash(g),camera=CameraHash(g),pose=PoseHash(g.explorer);
            const auto disk=read(g.save);const auto jumps=g.controller.Stats().jumps;
            const auto collected=g.session.collected;
            const auto dials=g.session.fuseDials;
            const auto enemyHealth=g.encounter.health;
            // 先於Esc點按遊戲鍵；Pressed即使已放開仍保留，沒有偽造Input私有旗標。
            prepare({});input.SetKey(action,true);input.SetKey(action,false);
            if(action=='1'){input.SetKey('2',true);input.SetKey('2',false);input.SetKey('3',true);input.SetKey('3',false);}
            if(action==VK_SPACE)input.SetKey('W',true);
            input.SetKey(VK_ESCAPE,true);run(g);
            if(action==VK_SPACE){
                std::cout<<"RESUME_SPACE_WITNESS jumps_before="<<jumps<<" jumps_after="<<g.controller.Stats().jumps
                    <<" vy="<<g.controller.Velocity().y<<" accepted="<<g.inputReplayMotionAccepted
                    <<" paused="<<g.paused<<'\n';
                check(g.controller.Stats().jumps==jumps,"RESUME_SPACE_BEFORE_ESC_TRIGGERED_JUMP");
            }
            check(!g.paused,"RESUME_ESCAPE_DID_NOT_RESUME");
            check(ResumeWorldHash(g)==world&&CameraHash(g)==camera&&PoseHash(g.explorer)==pose,
                "RESUME_TRANSITION_ADVANCED_FROZEN_WORLD");
            check(g.playableInputs.Count()==0&&!g.playableInputs.Overflowed()&&g.playerLocomotion.PendingEvadeTicks()==0&&
                g.controller.CanCommitDirectDisplacementPreview(pending)&&read(g.save)==disk,"RESUME_TRANSITION_RETAINED_ACTION_OR_SAVE");
            if(action==VK_SPACE){
                check(input.Down('W'),"RESUME_CLEARED_HELD_MOVEMENT");const auto before=g.controller.Position();
                for(unsigned tick=0;tick<8;++tick){input.ConsumePresses();run(g);
                    check(g.playableOutcome==MistwatchGame::PlayableOutcome::WorldCommitted,"RESUME_HELD_W_NOT_ACCEPTED");}
                check(Aevrynd::Length(g.controller.Position()-before)>0,"RESUME_HELD_W_REQUIRES_REPRESS");
            }else for(unsigned tick=0;tick<2;++tick){prepare({});run(g);}
            check(g.controller.Stats().jumps==jumps&&g.evasion.remaining==0&&g.session.collected==collected&&
                g.session.fuseDials==dials&&g.encounter.health==enemyHealth&&read(g.save)==disk&&
                (action!='R'||!g.session.Alive()),"RESUME_DELAYED_ACTION_FIRED");
            prepare({action});run(g);
            if(action==VK_SPACE)check(g.controller.Stats().jumps==jumps+1,"RESUME_FRESH_SPACE_BLOCKED");
            else if(action=='Q')check(g.inputReplayLastDashing,"RESUME_FRESH_Q_BLOCKED");
            else if(action=='E')check(g.session.collected!=collected,"RESUME_FRESH_E_BLOCKED");
            else if(action=='F')check(g.encounter.health+1==enemyHealth,"RESUME_FRESH_F_BLOCKED");
            else if(action=='1')check(g.session.fuseDials!=dials,"RESUME_FRESH_TUNE_BLOCKED");
            else if(action==VK_F5)check(g.message=="Saved"&&read(g.save)!=disk,"RESUME_FRESH_F5_BLOCKED");
            else check(g.session.Alive()&&g.session.health==100&&RecoveryTrial::Same(g.session.feet,Mistwatch::Session::bridge),"RESUME_FRESH_R_BLOCKED");
        }
        check(RecoveryTrial::checks==oldChecks&&RecoveryTrial::frames==oldFrames,"RESUME_CHANGED_ORIGINAL_COUNTERS");
        std::cout<<"RESUME_ACTION_BARRIER_CPU_PASS checks="<<barrierChecks<<" cases="<<cases<<" frames="<<barrierFrames
            <<" true_OnUpdate=1 pause_space_order=1 resume_world_frozen=1 no_delayed_actions=1 held_move_next=1 fresh_actions_next=1 original_counters_unchanged=1 GPU=0\n";
    }
public:
    MistwatchSaveInputProbe(Aevrynd::Engine& engine,Aevrynd::SkinnedModel3D model,std::filesystem::path out):
        engine_(engine),raw_(std::move(model)),out_(std::move(out)),trace_(out_/"actual-main-frames.tsv",std::ios::binary){
        RecoveryTrial::Require(bool(trace_),"TRACE_CREATE");trace_<<std::setprecision(std::numeric_limits<float>::max_digits10)
            <<"frame\ttyped\tcause\tneeds_policy\toutcome\tpair_status\tx\ty\tz\thealth\tenemy_time\tpair_serial\tqueue\tfront\twater\tcamera_ticks\tdamage\tmessage\n";}
    void Run(){EnclosedRecovery();RejectedRetry();InitialOverlap();OrdinaryContact();OrdinaryFall();GroundedFixtureInitialization();FocusFixtureBoundary();
        std::cout<<"REAL_MAIN_RECOVERY_CPU_PASS checks="<<RecoveryTrial::checks<<" frames="<<RecoveryTrial::frames
            <<" actual_OnUpdate=1 real_Input=1 renderer_started=0 window=0 global_input=0\n";
        CompletionCameraInspection();ResumeActionBarrier();SurveyMapControls();}
};

int main(int argc,char** argv){try{
    RecoveryTrial::Require(argc==3,"USAGE_RAW_BOAR_NEW_OUTPUT");const std::filesystem::path output=argv[2];
    RecoveryTrial::Require(output.is_absolute()&&!std::filesystem::exists(output),"FRESH_OUTPUT_REQUIRED");
    RecoveryTrial::Require(std::filesystem::create_directories(output),"OUTPUT_CREATE");
    const auto loaded=Aevrynd::LoadSkinnedModelAssetFile(argv[1]);RecoveryTrial::Require(bool(loaded),"RAW_BOAR_NOT_LOADED");
    // 不呼叫Run/OnStart/OnRender；真Engine的Renderer/Audio物件維持未Initialize。
    Aevrynd::Engine engine;
    MistwatchSaveInputProbe probe(engine,loaded.model,output);probe.Run();return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE="<<e.what()<<'\n';return 1;}}
