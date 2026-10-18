#include "MistwatchLoadPreparation.h"
#include "MistwatchSaveFile.h"
#include <bit>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace Aevrynd;
using namespace Mistwatch;
namespace {
unsigned checks{},steps{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
struct Live {
    SaveGame game;
    CharacterController3D controller;
    PlayerLocomotion locomotion;
    Explorer explorer;
    Boar boar;
    OrbitCamera orbit{-.65f,.3f,6};
    std::vector<BoxCollider3D> solids=WorldSolids(false);
    ThirdPersonCameraMotion3D cameraMotion;
    ThirdPersonFramingResult3D framing;
    ThirdPersonCameraMotionResult3D cameraResult;
    Camera3D rendered;
    LoadPreparationInputs Inputs()const{return {controller,explorer,boar,orbit};}
    SavedGameCommitTargets Targets(){return {game.session,game.encounter,game.evasion,controller,explorer,boar,orbit,
        solids,cameraMotion,framing,cameraResult,rendered,&locomotion};}
    SaveGame Capture()const{
        SaveGame saved{game.session,game.encounter,game.evasion};
        saved.session.feet=controller.Position();
        saved.motion=SavedPlayerMotion{controller.CaptureMotionState(),locomotion.CaptureMotionState(),
            explorer.yaw,explorer.requestedYaw,explorer.phase,explorer.locomotionDistance,explorer.locomotionSeconds};
        return saved;
    }
    void Step(const HeightField3D& terrain,PlayerLocomotionInput input={},float dt=LocomotionMotor::FixedStep){
        const auto r=locomotion.Advance(controller,game.evasion,explorer,input,dt,solids,{},terrain,[]{return true;});
        if(!r.accepted)std::cerr<<"POSE_REJECT line="<<r.poseFailureLine<<'\n';
        Need(r.accepted,"real current-world controller and pose step");
        game.session.feet=controller.Position();++steps;
    }
    std::uint64_t Fingerprint()const{
        std::uint64_t hash=controller.StateHash();
        const auto add=[&](std::uint64_t value){hash^=value;hash*=1099511628211ull;};
        add(locomotion.StateHash());
        for(char c:Capture().Serialize())add(static_cast<unsigned char>(c));
        for(const auto& matrix:explorer.pose.globalMatrices)for(const auto& row:matrix.values)
            for(float value:row)add(std::bit_cast<std::uint32_t>(value));
        add(std::bit_cast<std::uint32_t>(orbit.yaw));add(std::bit_cast<std::uint32_t>(orbit.pitch));
        return hash;
    }
};
Live Start(const HeightField3D& terrain){
    Live live;const SaveGame legacy;
    auto prepared=PrepareSavedGame(legacy,live.Inputs(),terrain);
    Need(bool(prepared),"initial real world legacy setup");
    SwapPreparedSavedGame(*prepared.candidate,live.Targets());
    Need(live.controller.Grounded(),"initial physical terrain contact");
    return live;
}
void CompareMotion(const Live& a,const Live& b){
    const auto x=a.controller.CaptureMotionState(),y=b.controller.CaptureMotionState();
    Need(Same(x.feet,y.feet)&&Same(x.velocity,y.velocity)&&x.height==y.height&&x.yaw==y.yaw&&x.grounded==y.grounded,
        "disk restore physical continuation equals uninterrupted flight");
    const auto p=a.locomotion.CaptureMotionState(),q=b.locomotion.CaptureMotionState();
    Need(Same(p.motor.velocity,q.motor.velocity)&&p.motor.heading==q.motor.heading&&p.motor.headingReady==q.motor.headingReady&&
        p.motor.phase==q.motor.phase&&p.motor.supportTicks==q.motor.supportTicks&&p.motor.tick==q.motor.tick&&
        p.motor.runFlight==q.motor.runFlight&&p.motor.wasDashing==q.motor.wasDashing&&p.remainder==q.remainder&&p.dodgeFlight==q.dodgeFlight,
        "disk restore locomotion owner retained");
    Need(Same(a.game.evasion.direction,b.game.evasion.direction)&&a.game.evasion.remaining==b.game.evasion.remaining&&
        a.game.evasion.cooldown==b.game.evasion.cooldown,"disk restore evasion clock retained");
}
void CompareAirPose(const Explorer& a,const Explorer& b){
    Need(a.pose.localTransforms.size()==b.pose.localTransforms.size()&&a.pose.globalMatrices.size()==b.pose.globalMatrices.size(),
        "airborne restored pose size");
    for(std::size_t i=0;i<a.pose.localTransforms.size();++i){
        const auto& x=a.pose.localTransforms[i];const auto& y=b.pose.localTransforms[i];
        Need(Same(x.translation,y.translation)&&Same(x.rotation,y.rotation)&&Same(x.scale,y.scale)&&
            x.usesQuaternion==y.usesQuaternion&&x.quaternion.x==y.quaternion.x&&x.quaternion.y==y.quaternion.y&&
            x.quaternion.z==y.quaternion.z&&x.quaternion.w==y.quaternion.w,"immediate restored airborne local bone transform equals saved pose");
    }
    for(std::size_t i=0;i<a.pose.globalMatrices.size();++i)
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
            Need(a.pose.globalMatrices[i].values[row][col]==b.pose.globalMatrices[i].values[row][col],
                "immediate restored airborne global bone matrix equals saved pose");
}
void Roundtrip(const HeightField3D& terrain,const std::filesystem::path& directory,bool dodge,unsigned at){
    auto uninterrupted=Start(terrain);
    PlayerLocomotionInput input;input.jumpPressed=!dodge;input.evadePressed=dodge;input.dodgeDirection={1,0,0};
    uninterrupted.Step(terrain,input);
    for(unsigned i=1;i<at;++i)uninterrupted.Step(terrain);
    Need(!uninterrupted.controller.Grounded(),"real airborne checkpoint selected");
    const auto snapshot=uninterrupted.Capture();
    Need(snapshot.Valid(),"captured actual motion structurally valid");
    const auto filename=directory/(std::string(dodge?"dodge-":"jump-")+std::to_string(at)+".save");
    Need(WriteSaveFile(filename,snapshot)==SaveFileResult::Saved,"production atomic disk writer saves snapshot");
    SaveGame readback;
    Need(ReadSaveFile(filename,readback),"production disk reader restores motion snapshot");
    Need(readback.Serialize()==snapshot.Serialize(),"disk roundtrip retains all serialized data");
    auto restored=Start(terrain);
    const auto prior=restored.Fingerprint(),jumps=restored.controller.Stats().jumps;
    auto prepared=PrepareSavedGame(readback,restored.Inputs(),terrain);
    if(!prepared)std::cerr<<"LOAD_REJECT error="<<unsigned(prepared.error)<<" dodge="<<dodge<<" at="<<at<<'\n';
    Need(bool(prepared),"v3 complete load preparation accepts actual airborne motion");
    Need(restored.Fingerprint()==prior,"preparation does not mutate live player");
    Need(prepared.candidate->motionRestored&&!prepared.candidate->supportProbeUsed&&prepared.candidate->airbornePoseValidated,
        "v3 uses real restoration and shoe clearance instead of support integration");
    SwapPreparedSavedGame(*prepared.candidate,restored.Targets());
    CompareMotion(uninterrupted,restored);
    CompareAirPose(uninterrupted.explorer,restored.explorer);
    Need(restored.controller.Stats().jumps==jumps,"load does not synthesize jump event");
    Need(restored.explorer.yaw==snapshot.motion->explorerYaw&&restored.explorer.phase==snapshot.motion->explorerPhase&&
        restored.explorer.requestedYaw==snapshot.motion->explorerRequestedYaw,"loaded animation direction and phase retained");
    SwapPreparedSavedGame(*prepared.candidate,restored.Targets());
    Need(restored.Fingerprint()==prior,"entire player commit is reversibly atomic");
    SwapPreparedSavedGame(*prepared.candidate,restored.Targets());
    // 足部軌跡重建不宣稱接地姿態逐位元相等；空中物理必須一路相同到真正落地。
    unsigned flightSteps{};
    while(!uninterrupted.controller.Grounded()&&flightSteps<200){
        uninterrupted.Step(terrain);restored.Step(terrain);CompareMotion(uninterrupted,restored);++flightSteps;
    }
    Need(flightSteps>0&&uninterrupted.controller.Grounded()&&restored.controller.Grounded(),"both trajectories actually land");
    Need(restored.controller.Stats().jumps==jumps,"continued flight does not repeat jump");
    for(unsigned i=0;i<12;++i)restored.Step(terrain);
    Need(restored.controller.Grounded(),"restored player remains operable after landing");
    std::cout<<"MOTION_DISK_LOAD dodge="<<dodge<<" at="<<at<<" flight_steps="<<flightSteps<<" file="<<filename.string()<<'\n';
}
void Rejections(const HeightField3D& terrain){
    auto live=Start(terrain);PlayerLocomotionInput jump;jump.jumpPressed=true;
    live.Step(terrain,jump);for(unsigned i=0;i<18;++i)live.Step(terrain);
    const auto good=live.Capture();const auto prior=live.Fingerprint();
    const auto reject=[&](const SaveGame& bad,std::span<const BoxCollider3D> extra={}){
        const auto result=PrepareSavedGame(bad,live.Inputs(),terrain,{},extra);
        Need(!result,"unsafe motion load rejected");Need(live.Fingerprint()==prior,"failed load leaves prior player state unchanged");};
    auto bad=good;bad.motion->controller.velocity.y=live.controller.Settings().jumpSpeed+1;reject(bad);
    bad=good;bad.motion->controller.velocity.x+=1;reject(bad);
    bad=good;bad.motion->locomotion.remainder=std::numeric_limits<double>::quiet_NaN();reject(bad);
    bad=good;bad.motion->controller.feet.y=-1;bad.session.feet=bad.motion->controller.feet;reject(bad);
    const auto center=live.controller.Center();const std::array blocked{BoxCollider3D{center,{.2f,.2f,.2f}}};reject(good,blocked);
    // 有效舊檔仍使用原先清除速度與動作擁有者的遷移，不誤保留載入前的跳躍。
    SaveGame legacy;auto prepared=PrepareSavedGame(legacy,live.Inputs(),terrain);Need(bool(prepared),"legacy still loads");
    SwapPreparedSavedGame(*prepared.candidate,live.Targets());
    PlayerLocomotion empty;
    Need(Same(live.controller.Velocity(),{})&&live.locomotion.StateHash()==empty.StateHash(),"legacy owner reset remains explicit");
}
}
int main()try{
    const auto terrain=CreateTerrain();
    const auto id=std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory=std::filesystem::temp_directory_path()/("MistwatchMotionLoad-"+std::to_string(id));
    Need(std::filesystem::create_directory(directory),"create unique disk evidence directory");
    for(unsigned at:{1u,9u,18u,28u})Roundtrip(terrain,directory,true,at);
    for(unsigned at:{1u,20u,42u,65u})Roundtrip(terrain,directory,false,at);
    Rejections(terrain);
    std::cout<<"MOTION_LOAD_CPU_PASS checks="<<checks<<" steps="<<steps<<" disk_prepare_swap=1 native_F5_F9=0 exact_ground_gait=0 evidence="<<directory.string()<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
