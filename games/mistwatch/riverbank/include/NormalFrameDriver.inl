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
