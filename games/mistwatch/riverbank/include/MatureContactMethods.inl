    #include "KeyCheckpointMethods.inl"
    #include "RuntimeSourceMethods.inl"
    #include "ManualEntrySmoke.inl"
    #include "IndexedGameplayWorld.inl"
    // 正式移動所有者的實機接觸診斷；尚不是完整鍵盤／AI／植栽／存檔世界接線。
    MatureContact::World matureContactWorld;
    unsigned matureContactTick{},matureContactOldOverlap{},matureContactNewOverlap{},matureContactMatches{};
    bool matureContactFirstImage{},matureContactFinished{};
    CharacterController3D matureContactOld;
    std::ofstream matureContactTrace;
    Vec3 matureContactStart{},matureContactBlocked{},matureContactRetreated{};
    static bool MatureSameVec(Vec3 a,Vec3 b){return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(b.x)&&std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(b.y)&&std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(b.z);}
    static bool MatureSamePose(const Mistwatch::Explorer& a,const Mistwatch::Explorer& b){
        if(!MatureSameVec({a.yaw,a.requestedYaw,a.phase},{b.yaw,b.requestedYaw,b.phase})||a.planted!=b.planted||a.nextFoot!=b.nextFoot||a.pose.globalMatrices.size()!=b.pose.globalMatrices.size())return false;
        for(unsigned side=0;side<2;++side)if(!MatureSameVec(a.footTargets[side],b.footTargets[side]))return false;
        for(std::size_t i=0;i<a.pose.globalMatrices.size();++i)if(std::memcmp(&a.pose.globalMatrices[i],&b.pose.globalMatrices[i],sizeof(Matrix4))!=0)return false;
        return true;
    }
    void RequireMatureCamera(float dt){
        const auto previous=playerCameraMotion;auto reference=previous;
        const auto all=matureContactWorld.All(false);const auto extra=RuntimeCameraOnly();
        const auto expectedFraming=Mistwatch::BuildPlayerCameraFraming(session.feet,controller.Settings().height,orbit,all,terrain,extra);
        const auto expected=reference.Update(Mistwatch::BuildPlayerCameraMotionRequest(session.feet,controller.Settings().height,orbit,
            reference.Initialized(),all,terrain,extra),Mistwatch::PlayerCameraSamples(session.feet,controller.Settings().height),all,
            Mistwatch::CameraCeilingSolids(),terrain,dt,extra);
        const auto* old=previous.Initialized()?&previous.Camera():nullptr;
        const auto query=matureContactWorld.Camera(session.feet,orbit.distance,old,false);
        MatureContact::Require(UpdatePlayerCamera(dt,query)&&expected.accepted&&expected.status==lastCameraMotion.status&&expected.subjectVisible==lastCameraMotion.subjectVisible&&
            MatureSameVec(expected.camera.position,lastCameraMotion.camera.position)&&MatureSameVec(expected.camera.target,lastCameraMotion.camera.target)&&
            MatureSameVec(expected.camera.up,lastCameraMotion.camera.up)&&MatureSameVec(expectedFraming.camera.position,lastPlayerFraming.camera.position)&&
            MatureSameVec(expectedFraming.camera.target,lastPlayerFraming.camera.target),"MATURE_ACTUAL_CAMERA_DIFFERS_FROM_COMPLETE47014_SOURCE");
        lastRenderedCamera=lastCameraMotion.camera;
    }
    void InitializeMatureContact(Engine& e){
        wchar_t flag[3]{};MatureContact::Require(GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_MATURE_CONTACT",flag,3)==1&&flag[0]==L'1'&&
            !inputReplayDirectory.empty()&&capture.empty()&&!session.bridgeRepaired,"MATURE_EXPLICIT_ISOLATED_CONTACT_SCOPE_REQUIRED");
        const auto before=RuntimeWorldSolids(false),after=RuntimeWorldSolids(true);
        matureContactWorld.Load(e.ExecutableDirectory()/"assets/mature-contact",before,after);matureStudy.contactWorld=&matureContactWorld;
        matureContactStart=MatureContact::WitnessBody-Vec3{3,0,0};
        MatureContact::Require(terrain.Contains(matureContactStart.x,matureContactStart.z),"MATURE_SOURCE_ROUTE_OUTSIDE_CURRENT_TERRAIN");
        matureContactStart.y=terrain.SampleHeight(matureContactStart.x,matureContactStart.z);
        controller.Reset(matureContactStart);const auto local=matureContactWorld.Body(matureContactStart,false);solids.assign(local.begin(),local.end());
        MatureContact::Require(controller.Step({},solids,RuntimeRoofHulls(),terrain,1.f/60.f)&&controller.Grounded()&&
            !matureContactWorld.Penetrates(controller.Position(),controller.Extents()),"MATURE_CURRENT_COMPLETE_WORLD_INITIAL_CONTACT_INVALID");
        matureContactOld=CharacterController3D(controller.Settings());matureContactOld.Reset(matureContactStart);
        MatureContact::Require(matureContactOld.Step({},matureContactWorld.Original(false),RuntimeRoofHulls(),terrain,1.f/60.f),"MATURE_OLD_CONTROL_INITIAL_CONTACT_INVALID");
        playerLocomotion.Reset();evasion=Mistwatch::Evasion{};
        InitializeMatureCheckpoint();
        session.feet=controller.Position();started=true;paused=false;orbit.yaw=-1.5707963f;
        playerCameraMotion.Reset();explorer.Update({},1.f/60.f,false);
        MatureContact::Require(explorer.PlaceFeet(session.feet,controller.Grounded(),false,terrain,solids,RuntimeRoofHulls()),"MATURE_INITIAL_ACTUAL_EXPLORER_FOOT_PLACEMENT");
        RequireMatureCamera(1.f/60.f);message="成熟樹木正式移動接觸診斷；尚未完成完整遊戲世界接線。";
        matureContactTrace.open(inputReplayDirectory/"mature-contact-trace.tsv");matureContactTrace.precision(9);
        matureContactTrace<<"tick\told_x\told_y\told_z\tnew_x\tnew_y\tnew_z\tgrounded\tfull_motion_equal\tfull_camera_equal\told_wood_overlap\tnew_overlap\tactive_body\tactive_camera\n";
        MatureContact::Require(bool(matureContactTrace),"MATURE_TRACE_OPEN");
    }
    #include "SaveProbe.inl"
    bool PrepareMatureContact(Engine& e,float dt){
        (void)dt;constexpr float fixed=Mistwatch::LocomotionMotor::FixedStep;
        if(matureKeyActive)return PrepareMatureKeys(e);
        if(matureContactFinished)return true; // Quit請求等待主迴圈處理時不重入已完成交易。
        if(!matureContactFirstImage){matureContactFirstImage=true;inputReplayPhase="mature-contact-start";inputReplayCaptureRequested=true;inputReplayCaptureStart=std::chrono::steady_clock::now();return true;}
        if(matureKeyResume){matureKeyActive=true;return true;}
        if(matureContactTick==420){
            ValidateMatureIndexedSave();
            const auto stats=e.GetRendererStats();MatureContact::Require(!matureContactFinished&&inputReplayImages==15&&matureContactMatches==420&&
                matureContactOldOverlap>0&&matureContactNewOverlap==0&&matureContactBlocked.x-matureContactStart.x>.1f&&
                matureContactBlocked.x-matureContactRetreated.x>1.f&&Length(playerLocomotion.Motor().Velocity())<.0001f&&
                stats.debugErrorCount==0&&stats.debugWarningCount==0,"MATURE_BOUNDED_CONTACT_PROOF_INCOMPLETE");
            matureContactTrace.close();MatureContact::Require(bool(matureContactTrace),"MATURE_COMPLETE_TRACE_WRITE");/* 完整來源只在按鍵／續玩驗證結束後寫一次。 */
            std::ofstream out(inputReplayDirectory/"owner-route-result.txt");out.precision(9);
            out<<"schema=mature-native-owner-probe-v1\nroute=mature-owner-contact-probe\npassed=1\nticks=420\nfixed_hz=120\nforward_ticks=120\nreverse_ticks=240\nstop_ticks=60\ninitialization_ticks=1\nimages=15\nfull_body_camera_matches="<<matureContactMatches
               <<"\nactual_approach="<<matureContactBlocked.x-matureContactStart.x<<"\nactual_retreat="<<matureContactBlocked.x-matureContactRetreated.x
               <<"\nfinal_motor_speed="<<Length(playerLocomotion.Motor().Velocity())<<"\nold_wood_overlap_ticks="<<matureContactOldOverlap<<"\nnew_world_overlap_ticks="<<matureContactNewOverlap
               <<"\nregistered_before=47014\nregistered_after=47015\nactual_gameplay_bridge_repair_tested=0\nplant_ai_save_accepted=0\noriginal_shelter_driver_executed=0\nmanual_play=0\n"
               <<"debug_errors="<<stats.debugErrorCount<<"\ndebug_warnings="<<stats.debugWarningCount<<"\ndebug_layer_enabled="<<stats.debugLayerEnabled
               <<"\ngpu_validation_requested="<<stats.gpuValidationRequested<<"\ngpu_validation_executed="<<stats.gpuValidationExecuted
               <<"\nlimits=One native real PlayerLocomotion owner route with full-source oracle,normal camera,actual displayed12trees. Original direct47failure and short60reverse failure remain. Not keyboard,AI,plant,save,bridge,all-contact,art,FPS or release acceptance.\n";
            out.close();MatureContact::Require(bool(out),"MATURE_RESULT_WRITE");matureContactFinished=true;matureKeyActive=true;return true;
        }
        const auto previous=controller.Position();const Vec3 direction=matureContactTick<120?Vec3{1,0,0}:matureContactTick<360?Vec3{-1,0,0}:Vec3{};
        const auto local=matureContactWorld.Body(previous,false);solids.assign(local.begin(),local.end());
        auto reference=controller;auto referenceOwner=playerLocomotion;auto referencePose=explorer;auto referenceEvasion=evasion;
        Mistwatch::PlayerLocomotionInput input;input.movement=direction;
        const auto applied=playerLocomotion.Advance(controller,evasion,explorer,input,fixed,solids,RuntimeRoofHulls(),terrain,[]{return true;});
        const auto oracle=referenceOwner.Advance(reference,referenceEvasion,referencePose,input,fixed,matureContactWorld.All(false),RuntimeRoofHulls(),terrain,[]{return true;});
        MatureContact::Require(applied.accepted&&oracle.accepted&&applied.steps==1&&oracle.steps==1&&controller.StateHash()==reference.StateHash()&&
            playerLocomotion.StateHash()==referenceOwner.StateHash()&&MatureSamePose(explorer,referencePose),"MATURE_ACTUAL_OWNER_BODY_POSE_DIFFERS_FROM_COMPLETE47014_SOURCE");
        MatureContact::Require(matureContactOld.Step({direction,false,1},matureContactWorld.Original(false),RuntimeRoofHulls(),terrain,fixed),"MATURE_OLD_CONTROL_STEP_REJECTED");
        session.feet=controller.Position();const auto current=matureContactWorld.Body(session.feet,false);solids.assign(current.begin(),current.end());
        const auto world=CreateTransformMatrix(session.feet,{0,explorer.yaw,0},{1,1,1});
        for(unsigned side=0;side<2;++side){const unsigned upper=9+side*3;
            const auto hip=TransformPoint(world,TransformPoint(explorer.pose.globalMatrices[upper],{})),knee=TransformPoint(world,TransformPoint(explorer.pose.globalMatrices[upper+1],{})),ankle=TransformPoint(world,TransformPoint(explorer.pose.globalMatrices[upper+2],{}));
            MatureContact::Require(std::abs(Length(knee-hip)-.4f)<.0001f&&std::abs(Length(ankle-knee)-.38f)<.0001f&&Length(ankle-explorer.footTargets[side])<.005f&&
                explorer.footSwings[side].SpeedBound()<=60,"MATURE_UNCHANGED_BONE_REACH_SWING_GATES");
        }
        RequireMatureCamera(fixed);++matureContactTick;++matureContactMatches;
        const auto oldOverlap=matureContactWorld.Penetrates(matureContactOld.Position(),matureContactOld.Extents(),true);
        const auto overlap=matureContactWorld.Penetrates(session.feet,controller.Extents());matureContactOldOverlap+=unsigned(oldOverlap);matureContactNewOverlap+=unsigned(overlap);
        MatureContact::Require(!overlap,"MATURE_CURRENT_ACTUAL_BODY_PENETRATES_COMPLETE_WORLD");
        const auto old=matureContactOld.Position();matureContactTrace<<matureContactTick<<'\t'<<old.x<<'\t'<<old.y<<'\t'<<old.z<<'\t'<<session.feet.x<<'\t'<<session.feet.y<<'\t'<<session.feet.z<<'\t'<<controller.Grounded()<<"\t1\t1\t"<<oldOverlap<<'\t'<<overlap<<'\t'<<solids.size()<<'\t'<<matureContactWorld.camera.boxes.size()<<'\n';matureContactTrace.flush();
        MatureContact::Require(bool(matureContactTrace),"MATURE_EVERY_TICK_TRACE_WRITE");
        if(matureContactTick==120)matureContactBlocked=session.feet;
        if(matureContactTick==360)matureContactRetreated=session.feet;
        if(matureContactTick%30==0){inputReplayPhase="mature-contact-"+std::to_string(matureContactTick);inputReplayCaptureRequested=true;inputReplayCaptureStart=std::chrono::steady_clock::now();}
        return true;
    }
