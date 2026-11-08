    std::filesystem::path indexedGameplayOutput;
    bool indexedGameplayProofEnabled{},manualEntrySmoke{},manualSmokeMenuDone{},manualSmokeCaptureRequested{},manualSmokeCapturePending{},manualSmokeFinished{};
    unsigned manualSmokeCaptureCount{},manualSmokeStage{},manualSmokeIdle{};
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
    std::filesystem::path ManualSmokeCapturePath()const {
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
            manualSmokeCaptureRequested=true;manualSmokeCaptureStart=std::chrono::steady_clock::now();
        }
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
                if(manualClipCapturing){
                    manualClipTimeline<<manualClipFrames<<'\t'<<inputReplay.Ticks()<<'\t'<<s.lastCapturedRenderSerial<<'\t'<<session.feet.z<<'\n';
                    manualClipCapturing=false;
                    if(++manualClipFrames==ManualClipFrames)manualClipTimeline.close();
                    MatureContact::Require(bool(manualClipTimeline),"MANUAL_BRIDGE_CLIP_TIMELINE_WRITE_FAILED");
                    return true;
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
                        matureSaveHandlerCalls==1&&matureLoadHandlerCalls==1&&CaptureSaveGame().Serialize()==manualSmokeExpectedSave,
                        "MANUAL_ENTRY_COMPLETE_OPERATION_MISSING");
                }
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
        if(manualSmokeStage==0){actual.SetKey(VK_F5,true);manualSmokeStage=1;return false;}
        if(manualSmokeStage==1){
            Mistwatch::SaveGame disk;MatureContact::Require(matureSaveHandlerCalls==1&&message=="Saved"&&Mistwatch::ReadSaveFile(save,disk)&&disk.Serialize()==CaptureSaveGame().Serialize(),"MANUAL_REAL_F5_SAVE_FAILED");
            manualSmokeExpectedSave=disk.Serialize();manualSmokeStage=2;return false;
        }
        if(manualSmokeStage==2&&++manualSmokeIdle<30){
            // 完成後世界時間本來就凍結；以既有R重開，再以F9還原完成旅程，不直接改狀態。
            if(manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::Completion){
                if(manualSmokeIdle==1)actual.SetKey('R',true);
                if(manualSmokeIdle==2){
                    MatureContact::Require(!session.beaconLit&&!session.bridgeRepaired&&session.collected==0&&
                        session.repairMaterials==0&&session.CurrentStage()==Mistwatch::Stage::Supplies&&message=="New expedition","MANUAL_ACTUAL_POSTWIN_R_RESTART_MISSING");
                    if(manualClockEnabled){
                        MatureContact::Require(!playtestClock.CompletedTime(),"MANUAL_PLAYTEST_CLOCK_RESTART_RETAINED_OLD_TIME");
                        manualClockRestartCleared=true;
                    }
                    manualSmokeNewExpeditionObserved=true;
                }
            }
            return false;
        }
        if(manualSmokeStage==2){MatureContact::Require(CaptureSaveGame().Serialize()!=manualSmokeExpectedSave,"MANUAL_F9_REQUIRES_ACTUALLY_CHANGED_MOTION_CLOCK");actual.SetKey(VK_F9,true);manualSmokeStage=3;return false;}
        MatureContact::Require(manualSmokeStage==3&&matureLoadHandlerCalls==1&&message=="Loaded"&&CaptureSaveGame().Serialize()==manualSmokeExpectedSave,"MANUAL_REAL_F9_RESTORE_FAILED");
        manualSmokeCaptureRequested=true;manualSmokeCaptureStart=std::chrono::steady_clock::now();return true;
    }
    void RenderManualEntrySmoke(Engine& e,const Matrix4& viewProjection,const std::string& hudContext){
        if(!manualEntrySmoke||!manualSmokeCaptureRequested)return;
        if(manualClockCapturing){
            const auto completed=playtestClock.CompletedTime();
            MatureContact::Require(completed&&hudContext==PlaytestClockContext(*completed),
                "MANUAL_PLAYTEST_CLOCK_HUD_CONTEXT_MISSING");
            manualClockElapsed=*completed;manualClockHud=hudContext;
        }else if(manualClockEnabled&&manualSmokeStage==3){
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
