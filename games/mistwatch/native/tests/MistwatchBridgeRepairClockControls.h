// 額外狀態都經真Player/控制器/Explorer進入；不寫入elapsed或虛構IK結果。
namespace BridgeClockExtra {
using namespace BridgeClockTest;
unsigned extraObservations{},extraViolations{},finishedNonneutral{};

Words Curves(const Explorer& explorer){
    Words out;for(const auto& curve:explorer.footSwings)out.SameObjectBytes(curve);
    out.F(explorer.phase);out.F(explorer.yaw);out.F(explorer.requestedYaw);
    out.F(explorer.locomotionDistance);out.F(explorer.locomotionSeconds);out.V(explorer.locomotionVelocity);
    for(float value:explorer.footFacing)out.F(value);
    for(float value:explorer.swingFacingStart)out.F(value);
    for(float value:explorer.swingFacingEnd)out.F(value);
    out.Add(explorer.nextFoot);out.Add(explorer.gaitGrounded);
    return out;
}

void SpatialObservation(Live& live,const HeightField3D& terrain,const char* name,bool firstPoseMayChange=false){
    const Camera3D camera{{10,10,-10},{0,1,0}};
    Need(!LoadPreparationDetail::BodyOverlaps(live.controller.Center(),live.controller.Extents(),live.solids),
        "extra spatial observation body gate not clear");
    Need(ThirdPersonCameraPathClear3D(camera.position,camera.position,live.solids,terrain,{},.16f),
        "extra spatial observation camera gate not clear");
    CharacterDirectDisplacementPreviewToken3D token;Preview(live,terrain,token);
    for(unsigned repeat=0;repeat<2;++repeat){
        const auto before=live.State();const auto gait=Curves(live.explorer);
        const auto physics=live.controller.StateHash(),owner=live.player.StateHash();
        Words pose;pose.Pose(live.explorer.pose);
        const auto spatialResult=PrepareBridgeRepairPlayer(live.controller,live.explorer,live.solids,{},terrain,camera);
        Need(bool(spatialResult)&&spatialResult.reason==BridgeRepairPlayerReason::Accepted,"extra true spatial pose unexpectedly rejected");
        Words afterPose;afterPose.Pose(live.explorer.pose);
        const bool sameGait=gait.data==Curves(live.explorer).data;
        const bool samePose=pose.data==afterPose.data;
        const bool sameAll=before==live.State();
        Need(physics==live.controller.StateHash()&&owner==live.player.StateHash(),"extra helper changed physics/input owner");
        Need(live.controller.CanCommitDirectDisplacementPreview(token),"extra helper changed real pending input preview");
        const bool requirePose=!firstPoseMayChange||repeat>0;
        const bool violation=!sameGait||(requirePose&&(!samePose||!sameAll));
        ++extraObservations;extraViolations+=unsigned(violation);
        std::cout<<"SPATIAL_OBSERVATION name="<<name<<" repeat="<<repeat
            <<" accepted=1 clock_and_gait_metadata_words_equal="<<sameGait<<" pose_words_equal="<<samePose
            <<" complete_state_words_equal="<<sameAll<<" first_pose_may_rebuild="<<firstPoseMayChange
            <<" pose_equality_required="<<requirePose<<" physics_input_unchanged=1 pending_preview=1 violation="<<violation<<'\n';
    }
    Need(live.controller.CancelDirectDisplacementPreview(token),"extra helper cannot cancel retained pending input preview");
}

void Turning(){
    HeightField3D terrain({{-16,0,-16},33,33,1});terrain.Fill(0);
    Live live;Flat(live,0,terrain);
    Need(!live.player.Motor().CaptureMotionState().headingReady,
        "fresh real Player motor heading already initialized before first rightward input");
    PlayerLocomotionInput input;input.movement={1,0,0};
    const auto turnResult=live.player.Advance(live.controller,live.evasion,live.explorer,input,H,
        live.solids,{},terrain,[]{return true;});
    Need(turnResult.accepted&&turnResult.steps==1&&!turnResult.poseRejected&&live.controller.Grounded(),
        "real turning Player/IK setup rejected");
    live.session.feet=live.controller.Position();
    const auto velocity=live.explorer.locomotionVelocity;
    Need(std::hypot(velocity.x,velocity.z)>.001f&&std::abs(std::remainder(std::atan2(velocity.x,velocity.z)-
        live.explorer.yaw,6.2831853f))>.26179939f,"fixed real turning witness did not enter turning condition");
    SpatialObservation(live,terrain,"true-player-turning");
}

void Airborne(){
    HeightField3D terrain({{-16,0,-16},33,33,1});terrain.Fill(0);
    Live live;Flat(live,0,terrain);PlayerLocomotionInput input;input.jumpPressed=true;
    const auto jumpResult=live.player.Advance(live.controller,live.evasion,live.explorer,input,H,
        live.solids,{},terrain,[]{return true;});
    Need(jumpResult.accepted&&jumpResult.steps==1&&!jumpResult.poseRejected&&!live.controller.Grounded()&&
        live.controller.Velocity().y>0&&live.controller.Stats().jumps==1,"real jumping Player/IK setup rejected");
    live.session.feet=live.controller.Position();
    SpatialObservation(live,terrain,"true-player-airborne");
}

void CompletedNonneutral(){
    HeightField3D terrain({{-16,0,-16},33,33,1});terrain.Fill(0);
    Live live;Flat(live,0,terrain);SeekMovingSwing(live,terrain,true);
    PlayerLocomotionInput release;release.crouched=true;
    bool witnessed=false;float offset{};unsigned completedTick{};
    for(unsigned tick=0;tick<96;++tick){
        const auto releaseResult=live.player.Advance(live.controller,live.evasion,live.explorer,release,H,
            live.solids,{},terrain,[]{return true;});
        Need(releaseResult.accepted&&releaseResult.steps==1&&!releaseResult.poseRejected&&live.controller.Grounded(),
            "real crouched release Player/IK setup rejected");
        live.session.feet=live.controller.Position();
        if(live.explorer.footSwings[0].Active()||live.explorer.footSwings[1].Active()||
            std::hypot(live.explorer.locomotionVelocity.x,live.explorer.locomotionVelocity.z)>=.001f)continue;
        offset=0;
        const auto world=CreateTransformMatrix(live.controller.Position(),{0,live.explorer.yaw,0},{1,1,1});
        for(unsigned side=0;side<2;++side){
            const auto neutral=TransformPoint(world,{side?.12f:-.12f,.1f,0});
            const auto delta=live.explorer.footSwings[side].Landing()-neutral;
            offset=(std::max)(offset,std::hypot(delta.x,delta.z));
        }
        if(offset>.02f){witnessed=true;completedTick=tick;break;}
    }
    Need(witnessed,"fixed crouched release did not naturally finish with nonneutral >2cm anchors");
    Need(live.explorer.locomotionDistance==0&&live.explorer.locomotionSeconds==H&&
        !live.explorer.footSwings[0].Active()&&!live.explorer.footSwings[1].Active(),
        "finished nonneutral witness metadata not genuinely stopped");
    // 僅模擬真站起空間命令的height部分；非完整main或多耗一個Player時間步。
    Need(live.controller.TrySetHeight(1.8f,live.solids,{}),"true uncrouch height gate rejected");
    ++finishedNonneutral;
    std::cout<<"COMPLETED_NONNEUTRAL_SETUP release_tick="<<completedTick<<" offset_bits="<<std::bit_cast<std::uint32_t>(offset)
        <<" offset_gt_2cm=1 both_curves_naturally_complete=1 no_elapsed_write=1 true_uncrouch_height=1 actual_main=0\n";
    SpatialObservation(live,terrain,"naturally-completed-nonneutral-uncrouch",true);
}
}

int main(int argc,char** argv)try{
    const int active=BridgeClockActiveMain(argc,argv);if(active==2)return 2;
    BridgeClockExtra::Turning();BridgeClockExtra::Airborne();BridgeClockExtra::CompletedNonneutral();
    using namespace BridgeClockExtra;
    Need(extraObservations==6&&finishedNonneutral==1,"extra finite spatial inventory incomplete");
    std::cout<<"BRIDGE_PREFLIGHT_POLICY_COMPLETE original_cases=11 active_observations="<<observations
        <<" active_violations="<<violations<<" extra_observations="<<extraObservations
        <<" extra_violations="<<extraViolations<<" finished_nonneutral="<<finishedNonneutral
        <<" support_rejected="<<supportRejected<<" actual_main=0 full_route7200=0 GPU=0\n";
    return violations||extraViolations?1:0;
}catch(const std::exception& error){std::cerr<<"FIRST_UNEXPECTED_DIVERGENCE: "<<error.what()<<'\n';return 2;}
