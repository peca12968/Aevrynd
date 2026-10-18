// 原11案原文由IO準備逐byte複製；兩個EXE只以helper的include位置區別。
#define main BridgeRepairOriginalMain
#include "MistwatchBridgeRepairPreparationTest.cpp"
#undef main

namespace BridgeClockTest {
using namespace BridgeRepairTest;
constexpr float H=1.f/120.f;
unsigned observations{},violations{},supportRejected{};

unsigned SeekMovingSwing(Live& live,const HeightField3D& terrain,bool crouched){
    PlayerLocomotionInput input;input.movement={0,0,1};input.crouched=crouched;
    for(unsigned tick=0;tick<64;++tick){
        const auto result=live.player.Advance(live.controller,live.evasion,live.explorer,input,H,
            live.solids,{},terrain,[]{return true;});
        Need(result.accepted&&result.steps==1&&!result.poseRejected,"moving setup true Player/IK step rejected");
        live.session.feet=live.controller.Position();
        Need(live.controller.Grounded(),"moving setup lost true static support");
        for(unsigned side=0;side<2;++side){
            const auto& path=live.explorer.footSwings[side];
            if(path.Active()&&path.Elapsed()>0&&path.Duration()-path.Elapsed()>3*double(H)){
                Need(live.explorer.locomotionSeconds==H&&live.explorer.locomotionDistance>.0001f&&
                    std::hypot(live.explorer.locomotionVelocity.x,live.explorer.locomotionVelocity.z)>0,
                    "active-swing witness lacks actual moving source metadata");
                std::cout<<"MOVING_WITNESS tick="<<tick<<" side="<<side<<" seconds_bits="
                    <<std::bit_cast<std::uint32_t>(live.explorer.locomotionSeconds)
                    <<" elapsed_bits="<<std::bit_cast<std::uint64_t>(path.Elapsed())
                    <<" duration_bits="<<std::bit_cast<std::uint64_t>(path.Duration())<<'\n';
                return side;
            }
        }
    }
    throw std::runtime_error("no true active moving swing within the fixed 64-tick setup");
}

void Observe(Live& live,const HeightField3D& terrain,std::span<const BoxCollider3D> world,
    const Camera3D& camera,unsigned side,const char* name,std::span<const BoxCollider3D> cameraOnly={}){
    Need(!LoadPreparationDetail::BodyOverlaps(live.controller.Center(),live.controller.Extents(),world),
        "clock setup candidate body gate not clear");
    Need(ThirdPersonCameraPathClear3D(camera.position,camera.position,world,terrain,cameraOnly,.16f),
        "clock setup candidate camera gate not clear");
    CharacterDirectDisplacementPreviewToken3D token;Preview(live,terrain,token);
    for(unsigned repeat=0;repeat<2;++repeat){
        const auto before=live.State();Words beforePose;beforePose.Pose(live.explorer.pose);
        const float beforeSeconds=live.explorer.locomotionSeconds;
        const auto beforeCurve=live.explorer.footSwings[side];auto advancedReference=beforeCurve;
        FootSwingSample3D referenceSample;
        Need(advancedReference.Advance(H,referenceSample),"fixed one-h independent curve reference failed");
        const auto result=PrepareBridgeRepairPlayer(live.controller,live.explorer,world,{},terrain,camera,cameraOnly);
        Need(bool(result)&&result.reason==BridgeRepairPlayerReason::Accepted,
            "spatially safe true moving helper unexpectedly rejected");
        Words afterPose;afterPose.Pose(live.explorer.pose);
        Words actualCurve,expectedCurve;actualCurve.SameObjectBytes(live.explorer.footSwings[side]);
        expectedCurve.SameObjectBytes(advancedReference);
        const auto after=live.State();
        const bool curveUnchanged=std::bit_cast<std::uint64_t>(beforeCurve.Elapsed())==
            std::bit_cast<std::uint64_t>(live.explorer.footSwings[side].Elapsed());
        const bool poseUnchanged=beforePose.data==afterPose.data;
        const bool allUnchanged=before==after;
        const bool secondsUnchanged=std::bit_cast<std::uint32_t>(beforeSeconds)==
            std::bit_cast<std::uint32_t>(live.explorer.locomotionSeconds);
        Need(live.controller.CanCommitDirectDisplacementPreview(token),"accepted helper invalidated real pending preview");
        const bool violation=!curveUnchanged||!poseUnchanged||!allUnchanged||!secondsUnchanged;
        ++observations;violations+=unsigned(violation);
        std::cout<<"CLOCK_OBSERVATION name="<<name<<" repeat="<<repeat<<" side="<<side
            <<" accepted=1 elapsed_before_bits="<<std::bit_cast<std::uint64_t>(beforeCurve.Elapsed())
            <<" elapsed_after_bits="<<std::bit_cast<std::uint64_t>(live.explorer.footSwings[side].Elapsed())
            <<" expected_h_advanced_bits="<<std::bit_cast<std::uint64_t>(advancedReference.Elapsed())
            <<" h_bits="<<std::bit_cast<std::uint32_t>(H)
            <<" extra_h_curve_reference_equal="<<(actualCurve.data==expectedCurve.data)
            <<" clock_unchanged="<<curveUnchanged<<" pose_words_equal="<<poseUnchanged
            <<" complete_state_words_equal="<<allUnchanged<<" seconds_metadata_equal="<<secondsUnchanged
            <<" pending_preview=1 violation="<<violation<<'\n';
    }
    Need(live.controller.CancelDirectDisplacementPreview(token),"cannot cancel unchanged input preview");
}

void SameWorld(bool crouched){
    HeightField3D terrain({{-16,0,-16},33,33,1});terrain.Fill(0);
    Live live;Flat(live,0,terrain);const unsigned side=SeekMovingSwing(live,terrain,crouched);
    const Camera3D camera{{10,10,-10},{0,1,0}};
    Observe(live,terrain,live.solids,camera,side,crouched?"same-world-crouched":"same-world-standing");
}

void RealRepairedWorld(const SkinnedModel3D& rawBoar){
    const auto terrain=CreateTerrain();Live live;PrepareCanonical(live,31,0,terrain,rawBoar);
    const unsigned side=SeekMovingSwing(live,terrain,false);
    const auto world=WorldSolids(true,Fir,Fallen);
    Need(live.solids.size()==223&&world.size()==224&&!live.session.bridgeRepaired,
        "actual canonical old/new repair worlds or uncommitted session changed");
    // 沿用原驗收的真鏡頭；同時先檢查相同roof-only遮擋資料未被省略。
    const std::array cameraOnly{RangerHutCameraRoofSolid()};const auto camera=live.camera.Camera();
    Need(ThirdPersonCameraPathClear3D(camera.position,camera.position,world,terrain,cameraOnly,.16f),
        "actual repaired-world original roof camera gate not clear");
    Observe(live,terrain,world,camera,side,"actual-canonical-repaired-world",cameraOnly);
}

void RemovedSupport(){
    HeightField3D terrain({{-16,0,-16},33,33,1});terrain.Fill(0);
    Live live;live.solids={BoxCollider3D{{0,.25f,0},{4,.25f,4}}};Bind(live.explorer);
    live.controller.Reset({0,.5f,0});live.controller.Step({},live.solids,terrain,H);
    Need(live.controller.Grounded()&&live.controller.Position().y==.5f,"raised source platform not true static support");
    live.explorer.Update({},H,false);
    Need(live.explorer.PlaceFeet(live.controller.Position(),true,false,terrain,live.solids),
        "raised source platform real Explorer/IK rejected");
    live.session.feet=live.controller.Position();SeekMovingSwing(live,terrain,false);
    const Camera3D camera{{10,10,-10},{0,1,0}};
    Need(!LoadPreparationDetail::BodyOverlaps(live.controller.Center(),live.controller.Extents(),{}),
        "removed-support candidate body gate not clear");
    Need(ThirdPersonCameraPathClear3D(camera.position,camera.position,{},terrain,{},.16f),
        "removed-support candidate camera gate not clear");
    CheckRejected(live,terrain,{}, {},camera,{},BridgeRepairPlayerReason::PoseRejected,"removed-real-static-support");
    ++supportRejected;
    std::cout<<"SUPPORT_REVALIDATION removed_height_bits="<<std::bit_cast<std::uint32_t>(.5f)
        <<" true_body_clear=1 true_camera_clear=1 actual_IK_rejected=1 complete_state_unchanged=1 pending_preview=1\n";
}
}

int BridgeClockActiveMain(int argc,char** argv)try{
    using namespace BridgeClockTest;
    // 原檔內的11案、counterconditions及marker原樣先執行，不因新clock性質失敗跳過。
    const int original=BridgeRepairOriginalMain(argc,argv);if(original!=0)return original;
    std::cout<<std::noboolalpha;
    const auto raw=LoadSkinnedModelAssetFile(argv[1]);Need(bool(raw)&&raw.model.Valid(),"same raw boar input missing");
    SameWorld(false);SameWorld(true);RealRepairedWorld(raw.model);RemovedSupport();
    Need(observations==6&&supportRejected==1,"new finite clock/support inventory incomplete");
    std::cout<<"BRIDGE_PREFLIGHT_CLOCK_COMPLETE original_cases=11 observations="<<observations
        <<" property_violations="<<violations<<" changed_world_support_rejected="<<supportRejected
        <<" exact_state_not_only_clock=1 actual_main=0 full_route7200=0 GPU=0\n";
    return violations?1:0;
}catch(const std::exception& error){std::cerr<<"FIRST_UNEXPECTED_DIVERGENCE: "<<error.what()<<'\n';return 2;}

#include "MistwatchBridgeRepairClockControls.h"
