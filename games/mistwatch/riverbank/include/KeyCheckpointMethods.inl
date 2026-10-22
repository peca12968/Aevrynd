    std::unique_ptr<Mistwatch::IndexedValleySaveDomain> matureIndexedDomain;
    Input matureKeyControls;
    bool matureKeyResume{},matureKeyActive{},matureKeyIssued{},matureKeyLoaded{},matureKeyFinished{};
    unsigned matureKeyStage{},matureSaveHandlerCalls{},matureLoadHandlerCalls{},matureAfterLoadSteps{};
    bool normalWalkObserved{},normalLowClearanceObserved{},normalReturnedStanding{};
    std::ofstream normalActionTrace;
    bool normalFrameIssued{},normalCrouchObserved{},normalManualJumpObserved{},normalJumpAirborneObserved{};
    unsigned normalBodyBefore{},normalCameraBefore{},normalNpcBefore{};
    std::uint64_t normalManualJumpBaseline{};float normalRunPeak{};
    Vec3 matureLoadedFeet{},matureBeforeKeyLoad{};
    std::string matureExpectedCheckpoint;
    std::ofstream matureKeyTrace,matureAfterLoadTrace;
    void InitializeMatureCheckpoint(){
        MatureContact::Require(bool(valleySaveDomain),"MATURE_KEY_QUALIFIED_BASE_DOMAIN_MISSING");
        const auto before=RuntimeWorldSolids(false),after=RuntimeWorldSolids(true);
        matureIndexedDomain=std::make_unique<Mistwatch::IndexedValleySaveDomain>(*valleySaveDomain,terrain,before,after,
            matureContactWorld.All(false),matureContactWorld.All(true),RuntimeRoofHulls(),RuntimeRoofCameraOnly());
        wchar_t resume[2048]{};const auto length=GetEnvironmentVariableW(L"AEVRYND_MATURE_RESUME_SAVE",resume,2048);
        MatureContact::Require(length<2048,"MATURE_KEY_RESUME_PATH_TOO_LONG");matureKeyResume=length>0;
        save=matureKeyResume?std::filesystem::path(resume):inputReplayDirectory/"production-key-checkpoint.txt";
        if(matureKeyResume)MatureContact::Require(std::filesystem::is_regular_file(save),"MATURE_KEY_RESUME_CHECKPOINT_MISSING");
        matureKeyStage=matureKeyResume?3u:0u;
        matureKeyTrace.open(inputReplayDirectory/"key-handler-trace.tsv");matureKeyTrace.precision(9);
        matureKeyTrace<<"stage\tissued\tpaused\tsave_calls\tload_calls\tx\ty\tz\tfeedback\n";
        matureAfterLoadTrace.open(inputReplayDirectory/"after-load-trace.tsv");matureAfterLoadTrace.precision(9);
        normalActionTrace.open(inputReplayDirectory/"normal-action-trace.tsv");normalActionTrace.precision(9);
        normalActionTrace<<"frame\tphase\tcamera_yaw\tmotor_heading\tspeed\tvx\tvy\tvz\theight\tgrounded\tjumps\tfixed_steps\tbody_matches\tcamera_matches\tnpc_queries\n";
        MatureContact::Require(bool(normalActionTrace),"NORMAL_ACTION_TRACE_OPEN_FAILED");
        matureAfterLoadTrace<<"tick\tx\ty\tz\towner_equal\tpose_equal\tcamera_equal\n";
        MatureContact::Require(bool(matureKeyTrace)&&bool(matureAfterLoadTrace),"MATURE_KEY_TRACE_OPEN_FAILED");
    }
    void RecordMatureKey(){
        const auto p=controller.Position();matureKeyTrace<<matureKeyStage<<'\t'<<matureKeyIssued<<'\t'<<paused<<'\t'<<matureSaveHandlerCalls<<'\t'<<matureLoadHandlerCalls
            <<'\t'<<p.x<<'\t'<<p.y<<'\t'<<p.z<<'\t'<<message<<'\n';matureKeyTrace.flush();
        MatureContact::Require(bool(matureKeyTrace),"MATURE_KEY_TRACE_WRITE_FAILED");
    }
    void CaptureMatureKey(const std::string& name){
        inputReplayPhase=name;inputReplayCaptureRequested=true;inputReplayCaptureStart=std::chrono::steady_clock::now();
    }
    bool PrepareMatureKeys(Engine& e){
        if(matureKeyFinished)return true;
        if(matureKeyStage<4){
            if(!matureKeyIssued){
                matureKeyControls.ConsumePresses();for(unsigned key:{unsigned(VK_ESCAPE),unsigned(VK_F5),unsigned(VK_F9)})matureKeyControls.SetKey(key,false);
                const unsigned key=matureKeyStage==1?VK_F5:matureKeyStage==3?VK_F9:VK_ESCAPE;
                if(matureKeyStage==3){
                    Mistwatch::SaveGame saved;MatureContact::Require(Mistwatch::ReadSaveFile(save,saved),"MATURE_KEY_SOURCE_SAVE_UNREADABLE");
                    matureExpectedCheckpoint=saved.Serialize();matureBeforeKeyLoad=controller.Position();
                }
                matureKeyControls.SetKey(key,true);matureKeyIssued=true;RecordMatureKey();
                return false; // 只交付按鍵，讓原OnUpdate處理器執行，不直接呼叫存讀檔。
            }
            matureKeyControls.ConsumePresses();for(unsigned key:{unsigned(VK_ESCAPE),unsigned(VK_F5),unsigned(VK_F9)})matureKeyControls.SetKey(key,false);
            RecordMatureKey();
            if(matureKeyStage==0)MatureContact::Require(paused,"MATURE_KEY_ESCAPE_DID_NOT_PAUSE");
            else if(matureKeyStage==1){
                Mistwatch::SaveGame written;MatureContact::Require(paused&&message=="Saved"&&matureSaveHandlerCalls==1&&
                    Mistwatch::ReadSaveFile(save,written)&&written.Serialize()==CaptureSaveGame().Serialize(),"MATURE_PRODUCTION_F5_DID_NOT_SAVE_CURRENT_WORLD");
            }else if(matureKeyStage==2)MatureContact::Require(!paused,"MATURE_KEY_ESCAPE_DID_NOT_RESUME");
            else{
                MatureContact::Require(started&&!paused&&message=="Loaded"&&matureLoadHandlerCalls==1&&
                    CaptureSaveGame().Serialize()==matureExpectedCheckpoint,"MATURE_PRODUCTION_F9_DID_NOT_RESTORE_EXACT_MOTION");
                matureLoadedFeet=controller.Position();
                if(matureKeyResume)MatureContact::Require(Length(matureLoadedFeet-matureBeforeKeyLoad)>1,"MATURE_FRESH_PROCESS_DID_NOT_ACTUALLY_RELOCATE_TO_SAVED_ROOT");
                matureKeyLoaded=true;CaptureMatureKey("key-loaded");
            }
            ++matureKeyStage;matureKeyIssued=false;return true;
        }
        MatureContact::Require(matureKeyLoaded&&matureIndexedDomain&&matureIndexedDomain->Identity()==*CaptureSaveGame().worldIdentity,"MATURE_SOURCE_OWNER_LOST_AFTER_LOAD");
        if(matureAfterLoadSteps<510){
            if(!normalFrameIssued){
                matureKeyControls.ConsumePresses();for(unsigned key:{unsigned('W'),unsigned('S'),unsigned(VK_SHIFT),unsigned(VK_CONTROL),unsigned(VK_SPACE)})matureKeyControls.SetKey(key,false);
                if(matureAfterLoadSteps<210)matureKeyControls.SetKey('W',true);
                if(matureAfterLoadSteps>=90&&matureAfterLoadSteps<150)matureKeyControls.SetKey(VK_SHIFT,true);
                if(matureAfterLoadSteps>=150&&matureAfterLoadSteps<180)matureKeyControls.SetKey(VK_CONTROL,true);
                if(matureAfterLoadSteps>=210&&matureAfterLoadSteps<390){matureKeyControls.SetKey('S',true);matureKeyControls.SetKey(VK_CONTROL,true);}
                if(matureAfterLoadSteps==180||matureAfterLoadSteps==420){normalManualJumpBaseline=controller.Stats().jumps;matureKeyControls.SetKey(VK_SPACE,true);}
                normalBodyBefore=normalBodyOracleMatches;normalCameraBefore=normalCameraOracleMatches;normalNpcBefore=normalNpcSourceMatches;
                normalFrameIssued=true;return false;
            }
            matureKeyControls.ConsumePresses();for(unsigned key:{unsigned('W'),unsigned('S'),unsigned(VK_SHIFT),unsigned(VK_CONTROL),unsigned(VK_SPACE)})matureKeyControls.SetKey(key,false);
            if(!inputReplayMotionAccepted||playableOutcome!=PlayableOutcome::WorldCommitted||normalBodyOracleMatches!=normalBodyBefore+1||normalCameraOracleMatches<=normalCameraBefore||normalNpcSourceMatches<=normalNpcBefore)
                throw std::runtime_error("NORMAL_WORLD_FRAME_NOT_COMMITTED: "+message+"; "+runtimeSourceFailure);
            const auto velocity=controller.Velocity();const float speed=std::hypot(velocity.x,velocity.z);
            if(matureAfterLoadSteps<90&&speed>.1f&&speed<=6.01f)normalWalkObserved=true;
            if(matureAfterLoadSteps>=90&&matureAfterLoadSteps<150)normalRunPeak=(std::max)(normalRunPeak,speed);
            if(matureAfterLoadSteps>=150&&matureAfterLoadSteps<180&&controller.Settings().height<1.5f)normalCrouchObserved=true;
            if(matureAfterLoadSteps>=180&&matureAfterLoadSteps<210){
                MatureContact::Require(controller.Settings().height<1.5f&&controller.Stats().jumps==normalManualJumpBaseline,"LOW_BRANCH_MUST_RETAIN_CROUCH_AND_DENY_JUMP");normalLowClearanceObserved=true;
            }
            if(matureAfterLoadSteps>=390&&matureAfterLoadSteps<420&&controller.Settings().height>1.79f)normalReturnedStanding=true;
            if(matureAfterLoadSteps>=420){normalManualJumpObserved|=controller.Stats().jumps>normalManualJumpBaseline;normalJumpAirborneObserved|=!controller.Grounded();}
            MatureContact::Require(!matureContactWorld.Penetrates(controller.Position(),controller.Extents()),"NORMAL_WORLD_COMMITTED_BODY_OVERLAP");
            ++matureAfterLoadSteps;normalFrameIssued=false;const auto p=controller.Position();
            normalActionTrace<<matureAfterLoadSteps<<'\t'<<(matureAfterLoadSteps<=90?"walk":matureAfterLoadSteps<=150?"run":matureAfterLoadSteps<=180?"crouch":matureAfterLoadSteps<=210?"blocked-jump":matureAfterLoadSteps<=390?"crouch-retreat":matureAfterLoadSteps<=420?"stand-stop":matureAfterLoadSteps<=450?"jump":"release-land")
                <<'\t'<<Mistwatch::PlayerCameraMovementYaw(playerCameraMotion.Camera(),orbit.yaw)<<'\t'<<playerLocomotion.Motor().CaptureMotionState().heading
                <<'\t'<<speed<<'\t'<<velocity.x<<'\t'<<velocity.y<<'\t'<<velocity.z<<'\t'<<controller.Settings().height<<'\t'<<controller.Grounded()<<'\t'<<controller.Stats().jumps
                <<'\t'<<normalMotionFixedSteps<<'\t'<<normalBodyOracleMatches<<'\t'<<normalCameraOracleMatches<<'\t'<<normalNpcSourceMatches<<'\n';normalActionTrace.flush();
            MatureContact::Require(bool(normalActionTrace),"NORMAL_ACTION_TRACE_WRITE_FAILED");
            matureAfterLoadTrace<<matureAfterLoadSteps<<'\t'<<p.x<<'\t'<<p.y<<'\t'<<p.z<<"\t1\t1\t1\n";matureAfterLoadTrace.flush();
            MatureContact::Require(bool(matureAfterLoadTrace),"NORMAL_WORLD_FRAME_TRACE_WRITE_FAILED");
            if(matureAfterLoadSteps%30==0)CaptureMatureKey("key-after-load-"+std::to_string(matureAfterLoadSteps));return true;
        }
        {std::ofstream diagnostic(inputReplayDirectory/"normal-action-summary.txt");diagnostic.precision(9);
        diagnostic<<"schema=normal-action-observation-v1\nnormal_frames="<<matureAfterLoadSteps<<"\nbody_matches="<<normalBodyOracleMatches<<"\nwalk_observed="<<normalWalkObserved
            <<"\nrun_peak_speed="<<normalRunPeak<<"\ncrouch_observed="<<normalCrouchObserved<<"\nmanual_jump_observed="<<normalManualJumpObserved<<"\nairborne_observed="<<normalJumpAirborneObserved
            <<"\ngrounded="<<controller.Grounded()<<"\nheight="<<controller.Settings().height<<"\nfixed_steps="<<normalMotionFixedSteps<<"\nactive_npc_frames="<<normalActiveNpcFrames<<'\n';
        diagnostic.close();MatureContact::Require(bool(diagnostic),"NORMAL_ACTION_SUMMARY_WRITE_FAILED");}
        MatureContact::Require(normalBodyOracleMatches==510&&normalLowClearanceObserved&&normalReturnedStanding&&normalWalkObserved&&normalCrouchObserved&&normalRunPeak>6.05f&&normalManualJumpObserved&&normalJumpAirborneObserved&&controller.Grounded()&&controller.Settings().height>1.79f,"NORMAL_WORLD_WALK_RUN_CROUCH_JUMP_LAND_NOT_ALL_OBSERVED");
        const auto stats=e.GetRendererStats();const unsigned expectedImages=matureKeyResume?19:33;
        MatureContact::Require(inputReplayImages==expectedImages&&matureSaveHandlerCalls==(matureKeyResume?0u:1u)&&matureLoadHandlerCalls==1&&
            Length(controller.Position()-matureLoadedFeet)>.1f&&stats.debugErrorCount==0&&stats.debugWarningCount==0,"MATURE_KEY_COMPLETE_PROOF_MISSING");
        matureContactWorld.Write(inputReplayDirectory);matureKeyTrace.close();matureAfterLoadTrace.close();normalActionTrace.close();
        MatureContact::Require(bool(matureKeyTrace)&&bool(matureAfterLoadTrace)&&bool(normalActionTrace),"MATURE_KEY_COMPLETE_TRACE_CLOSE_FAILED");
        std::ofstream out(inputReplayDirectory/"result.txt");out.precision(9);
        out<<"schema=mature-native-normal-frame-v1\npassed=1\nmode="<<(matureKeyResume?"fresh-process-resume":"production-key-save-load")
            <<"\nowner_route_ticks="<<matureContactTick<<"\nafter_load_frames="<<matureAfterLoadSteps<<"\nimages="<<inputReplayImages
            <<"\nproduction_save_handler_calls="<<matureSaveHandlerCalls<<"\nproduction_load_handler_calls="<<matureLoadHandlerCalls
            <<"\nrestored_snapshot_exact=1\nafter_load_full_source_owner_pose_camera_matches=510\nafter_load_actual_travel="<<Length(controller.Position()-matureLoadedFeet)
            <<"\ninitial_to_loaded_distance="<<Length(matureLoadedFeet-matureBeforeKeyLoad)<<"\nworld="<<matureIndexedDomain->Identity()
            <<"\nnormal_onupdate_frames="<<matureAfterLoadSteps<<"\nnormal_body_oracle_matches="<<normalBodyOracleMatches<<"\nnormal_camera_oracle_matches="<<normalCameraOracleMatches
            <<"\nnormal_npc_query_matches="<<normalNpcSourceMatches<<"\nnormal_active_npc_frames="<<normalActiveNpcFrames<<"\nnormal_motion_fixed_steps="<<normalMotionFixedSteps
            <<"\nlow_clearance_denied_standing_jump="<<normalLowClearanceObserved<<"\nretreated_and_stood="<<normalReturnedStanding<<"\nwalk_observed="<<normalWalkObserved<<"\nrun_peak_speed="<<normalRunPeak<<"\ncrouch_observed="<<normalCrouchObserved<<"\nmanual_jump_observed="<<normalManualJumpObserved<<"\njump_airborne_observed="<<normalJumpAirborneObserved
            <<"\nlanded_at_end="<<controller.Grounded()<<"\nmaximum_body_active="<<normalBodyMaximum<<"\nmaximum_camera_active="<<normalCameraMaximum<<"\nmaximum_npc_active="<<normalNpcMaximum
            <<"\nregistered_before=47014\nregistered_after=47015\ndebug_errors="<<stats.debugErrorCount<<"\ndebug_warnings="<<stats.debugWarningCount
            <<"\ndebug_layer_enabled="<<stats.debugLayerEnabled<<"\ngpu_validation_requested="<<stats.gpuValidationRequested<<"\ngpu_validation_executed="<<stats.gpuValidationExecuted
            <<"\ninput_source=isolated-key-state-original-OnUpdate-handlers\nmanual_play=0\nlimits=Production paused F5 and unpaused F9 handlers,or a separate process F9 from the exact same binary. Following510frames execute original whole OnUpdate movement/world-commit branch with real walk/run/crouch/jump/stop input. Body/pose and camera compare complete source;NPC query compares complete full scan. Active NPC combat,actual bridge repair,plant replanning and world release remain unaccepted. Unrepaired bridge only,no art/FPS/release acceptance.\n";
        out.close();MatureContact::Require(bool(out),"MATURE_KEY_RESULT_WRITE_FAILED");matureKeyFinished=true;inputReplayRecorded=true;e.Quit();return true;
    }
