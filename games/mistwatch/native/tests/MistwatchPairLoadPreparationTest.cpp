#include "MistwatchPairLoadPreparation.h"
#define main PreservedLoadPreparationSuiteMain
#include "MistwatchLoadPreparationTest.cpp"
#undef main
#include "MistwatchSaveFile.h"
#include "MistwatchExplorerNapeHairBinding.h"
#include "MistwatchExplorerBackpack.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <fstream>
#include <iterator>

namespace PairSaveTrial {
unsigned fullPrepared{},constructed{},rejected{},positive{},fileChecks{},futureSteps{};
void Need(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
std::string Bytes(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);
    Need(bool(file),"file read");return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};}
void EqualPose(const AnimationPose3D& a,const AnimationPose3D& b){
    Need(a.localTransforms.size()==b.localTransforms.size()&&a.globalMatrices.size()==b.globalMatrices.size(),"future pose size");
    const auto scalar=[](float x,float y){Need(std::bit_cast<std::uint32_t>(x)==std::bit_cast<std::uint32_t>(y),"future pose scalar");};
    const auto point=[&](Vec3 x,Vec3 y){scalar(x.x,y.x);scalar(x.y,y.y);scalar(x.z,y.z);};
    for(std::size_t i=0;i<a.localTransforms.size();++i){const auto& x=a.localTransforms[i];const auto& y=b.localTransforms[i];
        point(x.translation,y.translation);point(x.rotation,y.rotation);point(x.scale,y.scale);
        scalar(x.quaternion.x,y.quaternion.x);scalar(x.quaternion.y,y.quaternion.y);scalar(x.quaternion.z,y.quaternion.z);scalar(x.quaternion.w,y.quaternion.w);
        Need(x.usesQuaternion==y.usesQuaternion,"future quaternion flag");}
    for(std::size_t i=0;i<a.globalMatrices.size();++i)for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)scalar(a.globalMatrices[i][r][c],b.globalMatrices[i][r][c]);
}
LoadPreparationResult Prepare(const SaveGame& save,const Live& live,const HeightField3D& terrain){
    auto p=PrepareChecked(save,live,terrain);if(!p)std::cerr<<"prepare_error="<<unsigned(p.error)<<'\n';
    Need(bool(p),"selected full PrepareSavedGame failed");++fullPrepared;return p;
}
void Reason(const PreparedSavedGame& p,PairLoadPreflightReason expected,bool enabled=true){
    const auto before=p.snapshot.Serialize();const auto controller=p.controller.StateHash();
    const auto result=ValidatePreparedPairLoad(p,enabled);
    if(result.reason!=expected)std::cerr<<"expected_reason="<<unsigned(expected)<<" actual_reason="<<unsigned(result.reason)<<'\n';
    Need(result.reason==expected,"pair preflight reason");
    Need(p.snapshot.Serialize()==before&&p.controller.StateHash()==controller,"helper mutated prepared candidate");
    if(result)++positive;else ++rejected;
}
SavedPlayerMotion CaptureMotion(const PreparedSavedGame& p){return {p.controller.CaptureMotionState(),p.locomotion.CaptureMotionState(),
    p.explorer.yaw,p.explorer.requestedYaw,p.explorer.phase,p.explorer.locomotionDistance,p.explorer.locomotionSeconds};}
SaveGame Battle(Vec3 feet){SaveGame s;s.session=BridgeSession();s.session.feet=feet;Need(s.Valid(),"battle snapshot");return s;}
void PureQueryControls(const Live& live){
    const auto at=[&](Vec3 enemy,float radius=.45f){
        auto saved=Battle({0,0,48});saved.encounter.feet=enemy;Need(saved.Valid(),"constructed snapshot");
        PreparedSavedGame p(saved,live.Inputs());CharacterControllerSettings3D settings=live.controller.Settings();settings.radius=radius;
        p.controller=CharacterController3D(settings);p.controller.Reset(saved.session.feet);++constructed;return p;
    };
    auto sideTouch=at({1.75f,0,48});Reason(sideTouch,PairLoadPreflightReason::Accepted);
    auto corner=at({1.5f,0,49.5f});Reason(corner,PairLoadPreflightReason::Accepted);
    Need(LoadPreparationDetail::BodyOverlaps(corner.controller.Center(),corner.controller.Extents(),
        std::array<BoxCollider3D,1>{{{PairContact::BodyAt(corner.snapshot.encounter.feet).center,{1.3f,.8f,1.3f}}}}),"corner must be old AABB false-positive control");
    // .3+.5=.8、.8+.8=1.6 與 1.6+.9=2.5 的實際 f32 值可構成精確上下界接觸。
    auto topTouch=at({0,.3f,48});topTouch.snapshot.session.feet.y=1.6f;topTouch.controller.Reset(topTouch.snapshot.session.feet);
    Reason(topTouch,PairLoadPreflightReason::Accepted);
    auto above=at({0,-3,48});Reason(above,PairLoadPreflightReason::Accepted);
    auto overlap=at({0,0,48});Reason(overlap,PairLoadPreflightReason::LiveNpcOverlap);
    auto vertical=at({0,1,48});Reason(vertical,PairLoadPreflightReason::LiveNpcOverlap);
    auto mismatch=at({10,0,48});mismatch.controller.Reset({1,0,48});Reason(mismatch,PairLoadPreflightReason::PlayerSnapshotMismatch);
    auto volume=at({10,0,48},0);Reason(volume,PairLoadPreflightReason::InvalidPlayerVolume);
    auto nonfinite=at({10,0,48},(std::numeric_limits<float>::quiet_NaN)());Reason(nonfinite,PairLoadPreflightReason::InvalidPlayerVolume);
    auto domain=at({10,0,48},1.e-9f);Reason(domain,PairLoadPreflightReason::QueryUnresolved);
    auto invalid=at({10,0,48});invalid.snapshot.encounter.health=0;Reason(invalid,PairLoadPreflightReason::InvalidSnapshot);
    auto contradiction=at({10,0,48});contradiction.snapshot.session.guardianDefeated=true;Reason(contradiction,PairLoadPreflightReason::InvalidSnapshot);
    Reason(invalid,PairLoadPreflightReason::PairDisabled,false);
}
void FullControls(Live& live,const HeightField3D& terrain,const std::filesystem::path& directory){
    SaveGame legacy;auto old=Prepare(legacy,live,terrain);ValidateCandidate(*old.candidate,legacy,terrain);
    Reason(*old.candidate,PairLoadPreflightReason::Accepted);Reason(*old.candidate,PairLoadPreflightReason::PairDisabled,false);
    SaveGame roundtrip;Need(roundtrip.Restore(legacy.Serialize())&&roundtrip.Serialize()==legacy.Serialize(),"legacy v2 roundtrip");
    SaveGame migrated;Need(migrated.Restore(legacy.session.Serialize()),"session legacy migration");
    auto migratedPrepared=Prepare(migrated,live,terrain);Reason(*migratedPrepared.candidate,PairLoadPreflightReason::Accepted);
    auto v3=legacy;v3.motion=CaptureMotion(*old.candidate);Need(v3.Valid()&&v3.Serialize().starts_with("MISTWATCH_SAVE 3\n"),"v3 source");
    auto motion=Prepare(v3,live,terrain);Reason(*motion.candidate,PairLoadPreflightReason::Accepted);
    auto crouch=v3;crouch.motion->controller.height=1.4f;Need(crouch.Valid(),"crouch v3 source");
    auto crouched=Prepare(crouch,live,terrain);Reason(*crouched.candidate,PairLoadPreflightReason::Accepted);
    Need(crouched.candidate->controller.Settings().height==1.4f,"crouched restore changed height");
    auto signedZero=v3;signedZero.session.feet.x=0.f;signedZero.motion->controller.feet.x=-0.f;
    Need(signedZero.Valid()&&!PairContact::Same(signedZero.session.feet,signedZero.motion->controller.feet),"signed-zero numeric versus bit control");
    auto zeros=Prepare(signedZero,live,terrain);Reason(*zeros.candidate,PairLoadPreflightReason::Accepted);
    Need(std::signbit(zeros.candidate->controller.Position().x)&&!std::signbit(zeros.candidate->snapshot.session.feet.x),"signed zeros not preserved");
    auto air=v3;air.session.feet.y=3;air.motion->controller.feet.y=3;air.motion->controller.grounded=false;
    air.motion->controller.velocity.y=-1;air.motion->locomotion.motor.phase=LocomotionPhase::JumpOrFall;
    Need(air.Valid(),"air v3 source");auto airborne=Prepare(air,live,terrain);Reason(*airborne.candidate,PairLoadPreflightReason::Accepted);
    Need(!airborne.candidate->controller.Grounded()&&airborne.candidate->controller.Velocity().y==-1,"air motion not truly restored");
    auto unsafe=Battle(Session::arena);auto bad=Prepare(unsafe,live,terrain);
    Reason(*bad.candidate,PairLoadPreflightReason::LiveNpcOverlap);Reason(*bad.candidate,PairLoadPreflightReason::PairDisabled,false);
    auto airUnsafe=Battle({0,1,48});auto airBad=Prepare(airUnsafe,live,terrain);Reason(*airBad.candidate,PairLoadPreflightReason::LiveNpcOverlap);
    auto defeated=unsafe;defeated.session.guardianDefeated=true;defeated.encounter.health=0;
    defeated.encounter.phase=EnemyPhase::Defeated;defeated.encounter.countered=true;Need(defeated.Valid(),"defeated source");
    auto dead=Prepare(defeated,live,terrain);Reason(*dead.candidate,PairLoadPreflightReason::NoLiveNpc);
    Need(!PairContact::LiveBody(dead.candidate->snapshot.session,dead.candidate->snapshot.encounter),"defeated body remained");
    // 真實存檔 API：先保留安全檢查點；模擬 F5 原順序的新增拒絕不得呼叫 WriteSaveFile。
    const auto safePath=directory/"safe-save.txt",unsafePath=directory/"unsafe-legacy.txt";
    Need(WriteSaveFile(safePath,legacy)==SaveFileResult::Saved,"safe fixture write");const auto safeBytes=Bytes(safePath);unsigned writes{};
    if(ValidatePreparedPairLoad(*bad.candidate)){++writes;(void)WriteSaveFile(safePath,unsafe);}
    Need(writes==0&&Bytes(safePath)==safeBytes,"F5 pair rejection replaced old file");++fileChecks;
    Need(WriteSaveFile(unsafePath,unsafe)==SaveFileResult::Saved,"unsafe pre-pair legacy fixture write");
    const auto unsafeBytes=Bytes(unsafePath);SaveGame fromDisk;Need(ReadSaveFile(unsafePath,fromDisk),"actual unsafe legacy ReadSaveFile");
    const auto before=live.Fingerprint();const auto cameraBefore=live.motion;
    auto pending=Prepare(fromDisk,live,terrain);Need(!ValidatePreparedPairLoad(*pending.candidate),"F9 unsafe should reject before Swap");
    Need(Bytes(unsafePath)==unsafeBytes&&Bytes(safePath)==safeBytes&&live.Fingerprint()==before,"F9 pair rejection mutated file/live");
    SameFutureCamera(live,cameraBefore,terrain);++fileChecks;
    auto allowed=Prepare(v3,live,terrain);Need(bool(ValidatePreparedPairLoad(*allowed.candidate)),"safe v3 helper");
    PlayerLocomotion committedOwner;const auto ownerBefore=committedOwner.StateHash();
    auto targets=live.Targets();targets.locomotion=&committedOwner;
    SwapPreparedSavedGame(*allowed.candidate,targets);
    // 正式 CaptureSaveGame 從已提交物理／動作擁有者重建 v3，不把舊 fixture 的 optional motion 當 runtime 狀態。
    SaveGame captured{live.game.session,live.game.encounter,live.game.evasion};
    captured.motion=SavedPlayerMotion{live.controller.CaptureMotionState(),committedOwner.CaptureMotionState(),
        live.explorer.yaw,live.explorer.requestedYaw,live.explorer.phase,live.explorer.locomotionDistance,live.explorer.locomotionSeconds};
    Need(captured.Serialize()==v3.Serialize(),"success CaptureSaveGame-equivalent snapshot differs");
    SwapPreparedSavedGame(*allowed.candidate,targets);
    Need(live.Fingerprint()==before&&committedOwner.StateHash()==ownerBefore,"success immediate rollback changed old live/owner");
}
void PendingAndCamera(Live& live,const HeightField3D& terrain){
    PlayerLocomotion owner;PlayerLocomotionInput input;input.jumpPressed=true;
    const auto queued=owner.Advance(live.controller,live.game.evasion,live.explorer,input,LocomotionMotor::FixedStep*.25f,live.solids,{},terrain,[]{return true;});
    Need(queued.accepted&&queued.steps==0&&owner.Remainder()>0,"actual pending input queue not formed");
    const auto ownerBefore=owner;const auto ownerHash=owner.StateHash();const auto remainder=owner.Remainder();
    const auto liveBefore=live.Fingerprint();const auto cameraBefore=live.motion;
    CharacterDirectDisplacementPreviewToken3D token;CharacterDirectDisplacementResult3D preview;
    Need(live.controller.PreviewDirectDisplacementTick({1,{.001f,0,0},0},live.solids,terrain,token,&preview)&&
        live.controller.CanCommitDirectDisplacementPreview(token),"real pending preview control");
    auto restored=Prepare(Battle(Session::arena),live,terrain);Need(!ValidatePreparedPairLoad(*restored.candidate),"pending unsafe accepted");
    Need(live.controller.CanCommitDirectDisplacementPreview(token),"rejected pair preflight cancelled live preview");
    Need(live.Fingerprint()==liveBefore&&owner.StateHash()==ownerHash&&owner.Remainder()==remainder,"rejected pair preflight cleared live owner");
    SameFutureCamera(live,cameraBefore,terrain);Need(live.controller.CancelDirectDisplacementPreview(token),"pending preview remained uncancellable");
    auto a=live,b=live;auto ownerA=owner,ownerB=ownerBefore;input.jumpPressed=false;
    const auto x=ownerA.Advance(a.controller,a.game.evasion,a.explorer,input,LocomotionMotor::FixedStep*.75f,a.solids,{},terrain,[]{return true;});
    const auto y=ownerB.Advance(b.controller,b.game.evasion,b.explorer,input,LocomotionMotor::FixedStep*.75f,b.solids,{},terrain,[]{return true;});
    Need(x.accepted&&y.accepted&&x.steps==1&&y.steps==1&&a.controller.Velocity().y>0,"pending jump not actually consumed after rejection");
    Need(a.controller.StateHash()==b.controller.StateHash()&&ownerA.StateHash()==ownerB.StateHash(),"future actual controller/owner continuation differs");
    EqualPose(a.explorer.pose,b.explorer.pose);futureSteps=x.steps;
}
}
int main(int argc,char** argv)try{
    using namespace PairSaveTrial;
    Need(argc==3,"usage raw-boar output-directory");const std::filesystem::path directory=argv[2];
    Need(!std::filesystem::exists(directory),"fresh fixture directory required");std::filesystem::create_directories(directory);
    const auto terrain=CreateTerrain();Live live;const auto raw=LoadSkinnedModelAssetFile(argv[1]);
    Need(bool(raw)&&raw.model.skeleton.joints.size()==16&&raw.model.bindMesh.indices==live.boar.model.bindMesh.indices,"actual raw boar unavailable");
    live.boar.model=raw.model;ExplorerNapeHairBinding::Apply(live.explorer);BackpackFit1250::Apply(live.explorer,{+[](void*){},nullptr});
    auto initial=PairSaveTrial::Prepare(live.game,live,terrain);SwapPreparedSavedGame(*initial.candidate,live.Targets());
    PairSaveTrial::PureQueryControls(live);PairSaveTrial::FullControls(live,terrain,directory);PairSaveTrial::PendingAndCamera(live,terrain);
    std::cout<<"PAIR_SAVE_PREFLIGHT_CPU_PASS full_prepared="<<PairSaveTrial::fullPrepared<<" constructed_query_controls="<<PairSaveTrial::constructed
        <<" accepted="<<PairSaveTrial::positive<<" rejected="<<PairSaveTrial::rejected<<" real_file_preservation="<<PairSaveTrial::fileChecks
        <<" pending_preview_valid_after_reject=1 future_jump_steps="<<PairSaveTrial::futureSteps
        <<" future_camera_updates=6 raw_boar=1 fallback_boar=0 current_Explorer=1 whole_original_suite_executed=0 main_F5_F9_qualified=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE: "<<e.what()<<'\n';return 1;}
