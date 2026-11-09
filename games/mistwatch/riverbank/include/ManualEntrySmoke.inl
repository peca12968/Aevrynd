    std::filesystem::path indexedGameplayOutput;
    bool indexedGameplayProofEnabled{},manualEntrySmoke{},manualSmokeMenuDone{},manualSmokeCaptureRequested{},manualSmokeCapturePending{},manualSmokeFinished{};
    enum class ManualCheckpointStep { Begin,LoadMissing,VerifyMissing,VerifySave,ResumeSaved,ChangeState,LoadPaused,VerifyRestore,Capture };
    ManualCheckpointStep manualCheckpointStep{ManualCheckpointStep::Begin};
    unsigned manualSmokeCaptureCount{},manualSmokeIdle{};
    bool manualPausedCheckpointCapturing{};
    std::uint64_t manualSmokeCaptureBase{};
    std::chrono::steady_clock::time_point manualSmokeCaptureStart{};
    std::string manualSmokeExpectedSave;
    Mistwatch::ManualEntryRoute::Route manualSmokeRoute{Mistwatch::ManualEntryRoute::Route::Shelter};
    bool manualSmokeBridgeDone{},manualSmokeNewExpeditionObserved{};
    static constexpr unsigned ManualClipFrames=60,ManualClipTickStep=3;
    bool manualClipEnabled{},manualClipCapturing{};
    unsigned manualClipFirstTick{},manualClipFrames{};
    std::ofstream manualClipTimeline;
    bool manualClockEnabled{},manualClockCapturing{},manualClockCaptured{},manualClockRestartCleared{},manualClockLoadCleared{};
    std::chrono::seconds manualClockElapsed{};
    std::string manualClockHud;
    bool manualBeaconReadyEnabled{},manualBeaconReadyCapturing{},manualBeaconReadyCaptured{},manualWaterAudioEnabled{};
    bool manualShadowOptionsEnabled{};
    unsigned manualShadowOptionsStep{};
    std::uint64_t manualShadowOptionsRender{};
    std::filesystem::path ManualSmokeCapturePath()const {
        if(manualPausedCheckpointCapturing)return indexedGameplayOutput/"paused-checkpoint.png";
        if(manualBeaconReadyCapturing)return indexedGameplayOutput/"beacon-ready.png";
        if(manualClockCapturing)return indexedGameplayOutput/"2-manual-playtest-clock.png";
        if(!manualClipCapturing)
            return indexedGameplayOutput/Mistwatch::ManualEntryRoute::CaptureName(manualSmokeRoute,manualSmokeCaptureCount);
        auto frame=std::to_string(manualClipFrames);
        frame.insert(0,3-frame.size(),'0');
        return indexedGameplayOutput/"bridge-clip"/("bridge-"+frame+".png");
    }
    void InitializeManualEntryEvidence(){
        indexedGameplayProofEnabled=!inputReplayDirectory.empty();
        save=save.parent_path()/(save.stem().wstring()+L"-riverbank-v1-bearing-bridge-v1"+save.extension().wstring());
        if(bankRocksPreviewActive)save=save.parent_path()/(save.stem().wstring()+L"-bank-rocks-preview-unaccepted"+save.extension().wstring());
        wchar_t route[64]{};const auto routeLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_MANUAL_SMOKE_ROUTE",route,64);
        MatureContact::Require(routeLength<64,"MANUAL_SMOKE_ROUTE_TRUNCATED");
        wchar_t clip[3]{};const auto clipLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_MANUAL_BRIDGE_CLIP",clip,3);
        MatureContact::Require(!clipLength||(!indexedGameplayProofEnabled&&clipLength==1&&clip[0]==L'1'),
            "MANUAL_BRIDGE_CLIP_REQUIRES_ISOLATED_COMPLETION");
        manualClipEnabled=clipLength==1;
        wchar_t clock[3]{};const auto clockLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_MANUAL_PLAYTEST_CLOCK",clock,3);
        MatureContact::Require(!clockLength||(!indexedGameplayProofEnabled&&!manualClipEnabled&&clockLength==1&&clock[0]==L'1'),
            "MANUAL_PLAYTEST_CLOCK_REQUIRES_ISOLATED_COMPLETION");
        manualClockEnabled=clockLength==1;
        wchar_t shadowOptions[3]{};
        const auto shadowOptionsLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_MANUAL_SHADOW_OPTIONS",shadowOptions,3);
        MatureContact::Require(!shadowOptionsLength||(!indexedGameplayProofEnabled&&!manualClipEnabled&&!manualClockEnabled&&
            shadowOptionsLength==1&&shadowOptions[0]==L'1'),"MANUAL_SHADOW_OPTIONS_REQUIRES_ISOLATED_MENU");
        manualShadowOptionsEnabled=shadowOptionsLength==1;
        wchar_t beaconReady[3]{};
        const auto beaconReadyLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_MANUAL_BEACON_READY",beaconReady,3);
        MatureContact::Require(!beaconReadyLength||(!indexedGameplayProofEnabled&&!manualClipEnabled&&!manualClockEnabled&&
            !manualShadowOptionsEnabled&&beaconReadyLength==1&&beaconReady[0]==L'1'),
            "MANUAL_BEACON_READY_REQUIRES_ISOLATED_COMPLETION");
        manualBeaconReadyEnabled=beaconReadyLength==1;
        wchar_t waterAudio[3]{};
        const auto waterAudioLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_MANUAL_WATER_AUDIO",waterAudio,3);
        MatureContact::Require(!waterAudioLength||(!indexedGameplayProofEnabled&&!manualClipEnabled&&!manualClockEnabled&&
            !manualShadowOptionsEnabled&&!manualBeaconReadyEnabled&&waterAudioLength==1&&waterAudio[0]==L'1'),
            "MANUAL_WATER_AUDIO_REQUIRES_ISOLATED_COMPLETION");
        manualWaterAudioEnabled=waterAudioLength==1;
        if(indexedGameplayProofEnabled){
            static_cast<void>(Mistwatch::ManualEntryRoute::Resolve(false,true,std::wstring_view(route,routeLength)));
            indexedGameplayOutput=inputReplayDirectory;return;
        }
        MatureContact::Require(manualForestGroveCandidate&&forestPlayableWorld&&save.is_absolute(),"INDEXED_MANUAL_REQUIRES_EXISTING_FOREST_ARGUMENT");
        save=save.parent_path()/(save.stem().string()+"-mature-indexed-v1"+save.extension().string());
        wchar_t raw[32768]{};const auto n=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_MANUAL_SMOKE_OUTPUT",raw,32768);
        MatureContact::Require(n<32768,"MANUAL_SMOKE_OUTPUT_TRUNCATED");manualEntrySmoke=n>0;
        manualSmokeRoute=Mistwatch::ManualEntryRoute::Resolve(manualEntrySmoke,false,std::wstring_view(route,routeLength));
        MatureContact::Require(!manualClipEnabled||manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::Completion,
            "MANUAL_BRIDGE_CLIP_REQUIRES_ISOLATED_COMPLETION");
        MatureContact::Require(!manualClockEnabled||manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::Completion,
            "MANUAL_PLAYTEST_CLOCK_REQUIRES_ISOLATED_COMPLETION");
        MatureContact::Require(!manualShadowOptionsEnabled||(manualEntrySmoke&&manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::Shelter),
            "MANUAL_SHADOW_OPTIONS_REQUIRES_ISOLATED_MENU");
        MatureContact::Require(!manualBeaconReadyEnabled||(manualEntrySmoke&&manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::Completion),
            "MANUAL_BEACON_READY_REQUIRES_ISOLATED_COMPLETION");
        MatureContact::Require(!manualWaterAudioEnabled||(manualEntrySmoke&&manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::Completion),
            "MANUAL_WATER_AUDIO_REQUIRES_ISOLATED_COMPLETION");
        if(manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::Completion)
            MatureContact::Require(inputReplay.EnableCompletionRoute(),"MANUAL_COMPLETION_ROUTE_REJECTED");
        if(manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::DeathRetry)
            MatureContact::Require(inputReplay.EnableDeathRetryRoute(),"MANUAL_DEATH_RETRY_ROUTE_REJECTED");
        indexedGameplayOutput=manualEntrySmoke?std::filesystem::path(raw):save.parent_path()/"mature-indexed-runs"/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
        MatureContact::Require(indexedGameplayOutput.is_absolute()&&!std::filesystem::exists(indexedGameplayOutput),"MANUAL_EVIDENCE_REQUIRES_FRESH_ABSOLUTE_OUTPUT");
        std::filesystem::create_directories(indexedGameplayOutput);
        if(manualEntrySmoke){
            if(manualSmokeCostEnabled){
                MatureContact::Require(inputReplayCost.BeginStreaming(indexedGameplayOutput,true),"MANUAL_SMOKE_COST_STREAM_REJECTED");
                frameCostDirectory=indexedGameplayOutput;
            }
            MatureContact::Require(!std::filesystem::exists(save),"MANUAL_SMOKE_REQUIRES_ISOLATED_FRESH_PLAYER_SAVE");
            std::ofstream entry(indexedGameplayOutput/"entry.txt");entry<<"entry=existing-forest-manual-argument\ninput_replay_directory_empty=1\nsource_oracles_enabled="<<indexedGameplayProofEnabled
                <<"\nsave="<<save.string()<<"\nphysical_keyboard_verified=0\ninput=engine-GetInput-test-driven-not-replay-controls\n";
            entry.close();MatureContact::Require(bool(entry),"MANUAL_ENTRY_RECEIPT_FAILED");
            if(manualWaterAudioEnabled){
                std::ofstream water(indexedGameplayOutput/"water-audio.tsv");
                water<<"listener_x\tlistener_y\tlistener_z\tsource_x\tsource_y\tsource_z\tpan\tgain\tmaster_volume\tplayed\n";
                water.close();MatureContact::Require(bool(water),"MANUAL_WATER_AUDIO_HEADER_WRITE_FAILED");
            }
            manualSmokeCaptureRequested=true;manualSmokeCaptureStart=std::chrono::steady_clock::now();
        }
    }
    bool PrepareManualShadowOptions(Engine& e){
        struct Step {const char* name;bool g;unsigned menuKey;bool started,paused,shadows;};
        constexpr std::array steps{
            Step{"menu-off",true,0,false,false,false},
            Step{"menu-release",false,0,false,false,false},
            Step{"menu-on",true,0,false,false,true},
            Step{"release-before-start",false,0,false,false,true},
            Step{"start-with-g",true,VK_RETURN,true,false,true},
            Step{"release-in-game",false,0,true,false,true},
            Step{"g-in-game",true,0,true,false,true},
            Step{"release-before-pause",false,0,true,false,true},
            Step{"pause-with-g",true,VK_ESCAPE,true,true,true},
            Step{"release-in-pause",false,0,true,true,true},
            Step{"pause-off",true,0,true,true,false},
            Step{"release-before-resume",false,0,true,true,false},
            Step{"resume-with-g",true,VK_ESCAPE,true,false,false}
        };
        const auto stats=e.GetRendererStats();
        const auto& shadow=stats.lastDirectionalShadowProvenance;
        if(!shadow.frameSerial||shadow.frameSerial==manualShadowOptionsRender)return true;
        const Step initial{"default-menu",false,0,false,false,true};
        const auto& observed=manualShadowOptionsStep?steps[manualShadowOptionsStep-1]:initial;
        const bool rendererAgrees=observed.shadows?shadow.activeCascades>0:
            shadow.activeCascades==0&&shadow.drawCalls==0&&shadow.cascadeDrawCalls==0;
        MatureContact::Require(started==observed.started&&paused==observed.paused&&sunShadowsEnabled==observed.shadows&&rendererAgrees,
            ("MANUAL_SHADOW_OPTIONS_DIVERGED: "+std::string(observed.name)).c_str());
        const bool done=manualShadowOptionsStep==steps.size();
        std::ofstream out(indexedGameplayOutput/"sun-shadow-options.tsv",std::ios::app);
        if(!manualShadowOptionsStep)out<<"step\trender_serial\tstarted\tpaused\tshadows\tactive_cascades\tdraw_calls\n";
        out<<observed.name<<'\t'<<shadow.frameSerial<<'\t'<<started<<'\t'<<paused<<'\t'<<sunShadowsEnabled<<'\t'
            <<shadow.activeCascades<<'\t'<<shadow.drawCalls<<'\n';
        if(done)out<<"passed=1\n";
        out.close();
        MatureContact::Require(bool(out),"MANUAL_SHADOW_OPTIONS_RECEIPT_FAILED");
        if(done){manualSmokeFinished=true;e.Quit();return true;}
        manualShadowOptionsRender=shadow.frameSerial;
        const auto& next=steps[manualShadowOptionsStep++];
        auto& input=e.GetInput();
        input.SetKey('G',next.g);
        input.SetKey(VK_RETURN,next.menuKey==VK_RETURN);
        input.SetKey(VK_ESCAPE,next.menuKey==VK_ESCAPE);
        return false;
    }
    bool PrepareManualEntrySmoke(Engine& e,float dt){
        if(!manualEntrySmoke)return false;
        auto& actual=e.GetInput();actual.SetFocused(true);actual.ConsumePresses();
        if(manualSmokeFinished)return true;
        const bool deathRetry=manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::DeathRetry;
        if(manualSmokeCaptureRequested||manualSmokeCapturePending){
            const auto s=e.GetRendererStats();
            if(manualSmokeCapturePending&&s.successfulFrameCaptures>manualSmokeCaptureBase){
                MatureContact::Require(s.successfulFrameCaptures==manualSmokeCaptureBase+1&&s.lastCapturedRenderSerial,"MANUAL_SMOKE_CAPTURE_IDENTITY_MISSING");
                auto path=ManualSmokeCapturePath();path+=L".uploaded-view.txt";
                std::ofstream receipt(path);receipt<<"render_serial="<<s.lastCapturedRenderSerial<<"\nuploaded_unjittered_vp_bits=";
                for(float v:s.lastCapturedUnjitteredViewProjection)receipt<<std::bit_cast<std::uint32_t>(v)<<',';
                receipt<<'\n';receipt.close();MatureContact::Require(bool(receipt),"MANUAL_UPLOADED_VIEW_WRITE_FAILED");
                manualSmokeCapturePending=false;
                if(manualPausedCheckpointCapturing){manualPausedCheckpointCapturing=false;return true;}
                if(manualClipCapturing){
                    manualClipTimeline<<manualClipFrames<<'\t'<<inputReplay.Ticks()<<'\t'<<s.lastCapturedRenderSerial<<'\t'<<session.feet.z<<'\n';
                    manualClipCapturing=false;
                    if(++manualClipFrames==ManualClipFrames)manualClipTimeline.close();
                    MatureContact::Require(bool(manualClipTimeline),"MANUAL_BRIDGE_CLIP_TIMELINE_WRITE_FAILED");
                    return true;
                }
                if(manualBeaconReadyCapturing){
                    const auto detail=Mistwatch::CreateBeaconDetail(session);
                    MatureContact::Require(detail&&!detail->lit&&session.Alive()&&session.CurrentStage()==Mistwatch::Stage::Beacon&&
                        session.Near(Mistwatch::Session::beacon),"MANUAL_BEACON_READY_CAPTURE_STATE");
                    const auto& lens=detail->parts[Mistwatch::BeaconLensPart].material;
                    const auto& status=detail->parts[Mistwatch::BeaconStatusPart].material;
                    MatureContact::Require(lens.tint.b>lens.tint.g&&lens.tint.g>lens.tint.r&&
                        status.tint.b>status.tint.g&&status.tint.g>status.tint.r&&lens.surface.emissive==0.f&&status.surface.emissive==0.f,
                        "MANUAL_BEACON_READY_PASSIVE_BLUE_MATERIAL");
                    std::ofstream beacon(indexedGameplayOutput/"beacon-ready.txt");
                    beacon<<"stage="<<static_cast<unsigned>(session.CurrentStage())<<"\nbeacon_lit="<<session.beaconLit
                        <<"\npassive_blue_material=1\nrender_serial="<<s.lastCapturedRenderSerial<<'\n';
                    beacon.close();MatureContact::Require(bool(beacon),"MANUAL_BEACON_READY_RECEIPT_FAILED");
                    manualBeaconReadyCapturing=false;manualBeaconReadyCaptured=true;return true;
                }
                if(manualClockCapturing){
                    MatureContact::Require(manualClockElapsed.count()>0&&!manualClockHud.empty(),"MANUAL_PLAYTEST_CLOCK_CAPTURE_NOT_PRESENTED");
                    manualClockCapturing=false;manualClockCaptured=true;return true;
                }
                ++manualSmokeCaptureCount;
                if(!manualSmokeMenuDone){manualSmokeMenuDone=true;return true;}
                const bool completion=manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::Completion;
                if(deathRetry&&manualSmokeCaptureCount==2){
                    MatureContact::Require(!session.Alive()&&inputReplay.DamageEvents()==5&&inputReplay.Retries()==0,
                        "MANUAL_ACTUAL_DEFEAT_CAPTURE_MISSING");
                    return true;
                }
                if(deathRetry&&manualSmokeCaptureCount==3){
                    MatureContact::Require(session.health==100&&session.bridgeRepaired&&message=="Retry from bridgehead"&&
                        actionFeedback.Visible()&&actionFeedback.Event()==Mistwatch::ActionFeedbackEvent::RetryFromBridge,
                        "MANUAL_ACTUAL_RETRY_FEEDBACK_MISSING");
                    return true;
                }
                if(completion&&!manualSmokeBridgeDone){
                    MatureContact::Require(session.bridgeRepaired&&!inputReplay.Finished(),"MANUAL_BRIDGE_CAPTURE_STATE_MISSING");
                    if(manualClipEnabled){
                        manualClipFirstTick=inputReplay.Ticks();
                        const auto clipDirectory=indexedGameplayOutput/"bridge-clip";
                        MatureContact::Require(std::filesystem::create_directory(clipDirectory),"MANUAL_BRIDGE_CLIP_OUTPUT_NOT_FRESH");
                        manualClipTimeline.open(clipDirectory/"timeline.tsv");
                        manualClipTimeline.precision(9);
                        manualClipTimeline<<"frame\ttick_60hz\trender_serial\tplayer_z\n";
                        MatureContact::Require(bool(manualClipTimeline),"MANUAL_BRIDGE_CLIP_TIMELINE_OPEN_FAILED");
                    }
                    manualSmokeBridgeDone=true;return true;
                }
                MatureContact::Require(manualSmokeCaptureCount==Mistwatch::ManualEntryRoute::CaptureCount(manualSmokeRoute)&&inputReplay.Passed()&&started&&!paused&&
                    controller.Settings().height>1.79f,"MANUAL_ENTRY_COMPLETE_OPERATION_MISSING");
                if(deathRetry){
                    MatureContact::Require(session.health==100&&session.collected==7&&session.repairMaterials==7&&session.bridgeRepaired&&
                        session.CurrentStage()==Mistwatch::Stage::Encounter&&session.feet.z>40.f&&controller.Grounded()&&
                        inputReplay.DamageEvents()==5&&inputReplay.Retries()==1&&matureSaveHandlerCalls==0&&matureLoadHandlerCalls==0,
                        "MANUAL_ACTUAL_DEATH_RETRY_ROUTE_INCOMPLETE");
                }else{
                    MatureContact::Require(
                        (completion?manualSmokeNewExpeditionObserved&&session.bridgeRepaired&&session.beaconLit&&session.CurrentStage()==Mistwatch::Stage::Complete&&session.repairMaterials==7:session.repairMaterials==1)&&
                        matureSaveHandlerCalls==1&&matureLoadHandlerCalls==(completion?2u:1u)&&
                        manualCheckpointStep==ManualCheckpointStep::Capture&&CaptureSaveGame().Serialize()==manualSmokeExpectedSave,
                        "MANUAL_ENTRY_COMPLETE_OPERATION_MISSING");
                }
                MatureContact::Require(!manualBeaconReadyEnabled||manualBeaconReadyCaptured,"MANUAL_BEACON_READY_NOT_CAPTURED");
                if(manualClockEnabled){
                    MatureContact::Require(manualClockCaptured&&manualClockRestartCleared&&manualClockLoadCleared,
                        "MANUAL_PLAYTEST_CLOCK_FLOW_INCOMPLETE");
                    std::ofstream clockReceipt(indexedGameplayOutput/"playtest-clock.txt");
                    clockReceipt<<"synthetic_input=1\nelapsed_seconds="<<manualClockElapsed.count()
                        <<"\nhud_context="<<manualClockHud<<"\nrestart_cleared=1\nload_cleared=1\n";
                    clockReceipt.close();MatureContact::Require(bool(clockReceipt),"MANUAL_PLAYTEST_CLOCK_RECEIPT_FAILED");
                }
                std::ofstream out(indexedGameplayOutput/"result.txt");out<<"passed=1\ninput=engine-GetInput\ninput_replay_directory_empty=1\nsource_oracles_enabled="<<indexedGameplayProofEnabled
                    <<"\nticks="<<inputReplay.Ticks()<<"\nmaterials="<<session.repairMaterials<<"\nheight="<<controller.Settings().height<<"\nsave_calls="<<matureSaveHandlerCalls<<"\nload_calls="<<matureLoadHandlerCalls
                    <<"\nworld="<<matureIndexedDomain->Identity()<<"\nrestored_snapshot_exact="<<!deathRetry<<"\nimages="<<manualSmokeCaptureCount
                    <<"\nroute="<<(deathRetry?"death-retry":completion?"completion":"shelter")<<"\nbridge_repaired="<<session.bridgeRepaired<<"\nstage="<<static_cast<unsigned>(session.CurrentStage())<<"\nbeacon_lit="<<session.beaconLit
                    <<"\npostwin_restart_observed="<<manualSmokeNewExpeditionObserved
                    <<"\npaused_checkpoint_verified="<<completion
                    <<"\nforest_surface="<<Mistwatch::ForestSurfaceProfile::Name(forestSurfaceSelection.profile)
                    <<"\ndebug_errors="<<s.debugErrorCount<<"\ndebug_warnings="<<s.debugWarningCount
                    <<"\ndebug_layer_enabled="<<s.debugLayerEnabled<<"\ngpu_validation_requested="<<s.gpuValidationRequested<<"\ngpu_validation_executed="<<s.gpuValidationExecuted
                    <<"\nmanual_physical_input=0\nfocus=synthetic-engine-input-focus-only-not-OS-focus\n";
                if(deathRetry)out<<"damage_events="<<inputReplay.DamageEvents()<<"\nretries="<<inputReplay.Retries()
                    <<"\nretry_feedback_observed=1\nhealth="<<session.health<<"\ncollected="<<session.collected<<'\n';
                out.close();MatureContact::Require(bool(out),"MANUAL_SMOKE_RESULT_FAILED");manualSmokeFinished=true;e.Quit();return true;
            }
            MatureContact::Require(std::chrono::steady_clock::now()-manualSmokeCaptureStart<=std::chrono::seconds(15),"MANUAL_SMOKE_CAPTURE15S_TIMEOUT");return true;
        }
        if(manualShadowOptionsEnabled)return PrepareManualShadowOptions(e);
        if(!inputReplay.Finished()){
            // 每三個實際遊戲更新取一張，等待讀回期間凍結遊戲更新。
            if(manualClipEnabled&&manualSmokeBridgeDone&&manualClipFrames<ManualClipFrames){
                const auto next=manualClipFirstTick+manualClipFrames*ManualClipTickStep;
                MatureContact::Require(inputReplay.Ticks()<=next,"MANUAL_BRIDGE_CLIP_TICK_MISSED");
                if(inputReplay.Ticks()==next){
                    manualClipCapturing=true;manualSmokeCaptureRequested=true;
                    manualSmokeCaptureStart=std::chrono::steady_clock::now();return true;
                }
            }
            const std::string_view phase=inputReplay.Phase();
            if(manualBeaconReadyEnabled&&!manualBeaconReadyCaptured&&phase=="completion-beacon"&&
                session.CurrentStage()==Mistwatch::Stage::Beacon&&controller.Grounded()&&
                std::hypot(session.feet.x-Mistwatch::Session::beacon.x,session.feet.z-Mistwatch::Session::beacon.z)<=1.95f){
                manualBeaconReadyCapturing=true;manualSmokeCaptureRequested=true;
                manualSmokeCaptureStart=std::chrono::steady_clock::now();return true;
            }
            if(deathRetry&&((manualSmokeCaptureCount==1&&phase=="death-retry-press")||
                (manualSmokeCaptureCount==2&&phase=="death-retry-verify"))){
                manualSmokeCaptureRequested=true;manualSmokeCaptureStart=std::chrono::steady_clock::now();return true;
            }
            if(manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::Completion&&!manualSmokeBridgeDone&&std::string_view(inputReplay.Phase())=="cross-bridge") {
                MatureContact::Require(session.bridgeRepaired,"MANUAL_BRIDGE_REQUIRES_REAL_REPAIR");
                manualSmokeCaptureRequested=true;manualSmokeCaptureStart=std::chrono::steady_clock::now();return true;
            }
            const auto snapshot=CaptureSaveGame();
            inputReplay.Tick(session,Mistwatch::PlayerCameraMovementYaw(playerCameraMotion.Camera(),orbit.yaw),controller.Settings().height,started,paused,dt,&encounter,&evasion,controller.Grounded(),nullptr,message,controller.Velocity(),&snapshot,
                !explorer.footSwings[0].Active()&&!explorer.footSwings[1].Active());
            const auto& desired=inputReplay.Controls();for(unsigned k=0;k<256;++k)actual.SetKey(k,desired.Down(k));
            return false;
        }
        MatureContact::Require(inputReplay.Passed(),("MANUAL_ENGINE_INPUT_ROUTE_FAILED: "+std::string(inputReplay.Failure())).c_str());
        MatureContact::Require(!manualClipEnabled||manualClipFrames==ManualClipFrames,"MANUAL_BRIDGE_CLIP_INCOMPLETE");
        for(unsigned k=0;k<256;++k)actual.SetKey(k,false);
        if(deathRetry){
            manualSmokeCaptureRequested=true;manualSmokeCaptureStart=std::chrono::steady_clock::now();return true;
        }
        if(manualClockEnabled&&!manualClockCaptured){
            MatureContact::Require(session.beaconLit&&playtestClock.CompletedTime().has_value(),"MANUAL_PLAYTEST_CLOCK_RESULT_MISSING");
            manualClockCapturing=true;manualSmokeCaptureRequested=true;
            manualSmokeCaptureStart=std::chrono::steady_clock::now();return true;
        }
        // 通關路線在地圖暫停期間測試存讀檔；低棚路線仍覆蓋遊玩中存讀檔。
        const bool pauseCheckpoint=manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::Completion;
        switch(manualCheckpointStep){
        case ManualCheckpointStep::Begin:
            actual.SetKey(pauseCheckpoint?VK_ESCAPE:VK_F5,true);
            manualCheckpointStep=pauseCheckpoint?ManualCheckpointStep::LoadMissing:ManualCheckpointStep::VerifySave;
            return false;
        case ManualCheckpointStep::LoadMissing:
            MatureContact::Require(started&&paused,"MANUAL_CHECKPOINT_FIRST_PAUSE_MISSING");
            manualSmokeExpectedSave=CaptureSaveGame().Serialize();
            actual.SetKey(VK_F9,true);
            manualCheckpointStep=ManualCheckpointStep::VerifyMissing;
            return false;
        case ManualCheckpointStep::VerifyMissing:
            MatureContact::Require(paused&&matureLoadHandlerCalls==1&&matureSaveHandlerCalls==0&&
                !std::filesystem::exists(save)&&CaptureSaveGame().Serialize()==manualSmokeExpectedSave&&
                actionFeedback.Event()==Mistwatch::ActionFeedbackEvent::LoadUnavailable,
                "MANUAL_PAUSED_MISSING_SAVE_CHANGED_STATE");
            actual.SetKey(VK_F5,true);
            manualCheckpointStep=ManualCheckpointStep::VerifySave;
            return false;
        case ManualCheckpointStep::VerifySave:{
            Mistwatch::SaveGame disk;
            MatureContact::Require(paused==pauseCheckpoint&&matureSaveHandlerCalls==1&&message=="Saved"&&
                Mistwatch::ReadSaveFile(save,disk)&&disk.Serialize()==CaptureSaveGame().Serialize(),
                "MANUAL_REAL_F5_SAVE_FAILED");
            manualSmokeExpectedSave=disk.Serialize();
            if(pauseCheckpoint){
                manualCheckpointStep=ManualCheckpointStep::ResumeSaved;
                manualPausedCheckpointCapturing=true;
                manualSmokeCaptureRequested=true;
                manualSmokeCaptureStart=std::chrono::steady_clock::now();
                return true;
            }
            manualCheckpointStep=ManualCheckpointStep::ChangeState;
            return false;
        }
        case ManualCheckpointStep::ResumeSaved:
            actual.SetKey(VK_ESCAPE,true);
            manualCheckpointStep=ManualCheckpointStep::ChangeState;
            return false;
        case ManualCheckpointStep::ChangeState:
            if(++manualSmokeIdle<30){
                if(pauseCheckpoint&&manualSmokeIdle==1){
                    MatureContact::Require(!paused,"MANUAL_CHECKPOINT_RESUME_MISSING");
                    actual.SetKey('R',true);
                }
                if(pauseCheckpoint&&manualSmokeIdle==2){
                    MatureContact::Require(!session.beaconLit&&!session.bridgeRepaired&&session.collected==0&&
                        session.repairMaterials==0&&session.CurrentStage()==Mistwatch::Stage::Supplies&&message=="New expedition",
                        "MANUAL_ACTUAL_POSTWIN_R_RESTART_MISSING");
                    if(manualClockEnabled){
                        MatureContact::Require(!playtestClock.CompletedTime(),"MANUAL_PLAYTEST_CLOCK_RESTART_RETAINED_OLD_TIME");
                        manualClockRestartCleared=true;
                    }
                    manualSmokeNewExpeditionObserved=true;
                }
                return false;
            }
            MatureContact::Require(CaptureSaveGame().Serialize()!=manualSmokeExpectedSave,
                "MANUAL_F9_REQUIRES_ACTUALLY_CHANGED_MOTION_CLOCK");
            actual.SetKey(pauseCheckpoint?VK_ESCAPE:VK_F9,true);
            manualCheckpointStep=pauseCheckpoint?ManualCheckpointStep::LoadPaused:ManualCheckpointStep::VerifyRestore;
            return false;
        case ManualCheckpointStep::LoadPaused:
            MatureContact::Require(paused,"MANUAL_CHECKPOINT_SECOND_PAUSE_MISSING");
            actual.SetKey(VK_F9,true);
            actual.SetKey(VK_F5,true);
            manualCheckpointStep=ManualCheckpointStep::VerifyRestore;
            return false;
        case ManualCheckpointStep::VerifyRestore:
            MatureContact::Require(paused==pauseCheckpoint&&matureLoadHandlerCalls==(pauseCheckpoint?2u:1u)&&
                matureSaveHandlerCalls==1&&message=="Loaded"&&CaptureSaveGame().Serialize()==manualSmokeExpectedSave&&
                actionFeedback.Event()==Mistwatch::ActionFeedbackEvent::Loaded,"MANUAL_REAL_F9_RESTORE_FAILED");
            manualCheckpointStep=ManualCheckpointStep::Capture;
            if(pauseCheckpoint){actual.SetKey(VK_ESCAPE,true);return false;}
            break;
        case ManualCheckpointStep::Capture:break;
        }
        MatureContact::Require(!paused&&CaptureSaveGame().Serialize()==manualSmokeExpectedSave,
            "MANUAL_CHECKPOINT_CAPTURE_STATE_CHANGED");
        manualSmokeCaptureRequested=true;manualSmokeCaptureStart=std::chrono::steady_clock::now();return true;
    }
    void RenderManualEntrySmoke(Engine& e,const Matrix4& viewProjection,const std::string& hudContext){
        if(!manualEntrySmoke||!manualSmokeCaptureRequested)return;
        if(manualPausedCheckpointCapturing)
            MatureContact::Require(started&&paused&&surveyMapDraw.drawn&&
                actionFeedback.Event()==Mistwatch::ActionFeedbackEvent::Saved,"MANUAL_PAUSED_CHECKPOINT_UI_MISSING");
        if(manualClockCapturing){
            const auto completed=playtestClock.CompletedTime();
            MatureContact::Require(completed&&hudContext==PlaytestClockContext(*completed),
                "MANUAL_PLAYTEST_CLOCK_HUD_CONTEXT_MISSING");
            manualClockElapsed=*completed;manualClockHud=hudContext;
        }else if(manualClockEnabled&&manualCheckpointStep==ManualCheckpointStep::Capture){
            MatureContact::Require(!playtestClock.CompletedTime()&&!hudContext.starts_with("測試計時 "),
                "MANUAL_PLAYTEST_CLOCK_LOADED_OLD_TIME");
            manualClockLoadCleared=true;
        }
        manualSmokeCaptureBase=e.GetRendererStats().successfulFrameCaptures;
        const auto path=ManualSmokeCapturePath();
        MatureContact::Require(e.CaptureNextFrame3D(path),"MANUAL_SMOKE_CAPTURE_REJECTED");
        auto metadata=path;metadata+=L".view.txt";std::ofstream out(metadata);out<<"expected_render_serial="<<e.GetRendererStats().submittedFrames+1<<"\nrequested_unjittered_vp_bits=";
        for(const auto& row:viewProjection.values)for(float v:row)out<<std::bit_cast<std::uint32_t>(v)<<',';
        out<<'\n';out.close();MatureContact::Require(bool(out),"MANUAL_SMOKE_VIEW_WRITE_FAILED");manualSmokeCaptureRequested=false;manualSmokeCapturePending=true;
    }
