    std::filesystem::path indexedGameplayOutput;
    bool indexedGameplayProofEnabled{},manualEntrySmoke{},manualSmokeMenuDone{},manualSmokeCaptureRequested{},manualSmokeCapturePending{},manualSmokeFinished{};
    unsigned manualSmokeCaptureCount{},manualSmokeStage{},manualSmokeIdle{};
    std::uint64_t manualSmokeCaptureBase{};
    std::chrono::steady_clock::time_point manualSmokeCaptureStart{};
    std::string manualSmokeExpectedSave;
    Mistwatch::ManualEntryRoute::Route manualSmokeRoute{Mistwatch::ManualEntryRoute::Route::Shelter};
    bool manualSmokeBridgeDone{},manualSmokeNewExpeditionObserved{};
    void InitializeManualEntryEvidence(){
        indexedGameplayProofEnabled=!inputReplayDirectory.empty();
        save=save.parent_path()/(save.stem().wstring()+L"-riverbank-v1-bearing-bridge-v1"+save.extension().wstring());
        if(bankRocksPreviewActive)save=save.parent_path()/(save.stem().wstring()+L"-bank-rocks-preview-unaccepted"+save.extension().wstring());
        wchar_t route[64]{};const auto routeLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_MANUAL_SMOKE_ROUTE",route,64);
        MatureContact::Require(routeLength<64,"MANUAL_SMOKE_ROUTE_TRUNCATED");
        if(indexedGameplayProofEnabled){
            static_cast<void>(Mistwatch::ManualEntryRoute::Resolve(false,true,std::wstring_view(route,routeLength)));
            indexedGameplayOutput=inputReplayDirectory;return;
        }
        MatureContact::Require(manualForestGroveCandidate&&forestPlayableWorld&&save.is_absolute(),"INDEXED_MANUAL_REQUIRES_EXISTING_FOREST_ARGUMENT");
        save=save.parent_path()/(save.stem().string()+"-mature-indexed-v1"+save.extension().string());
        wchar_t raw[32768]{};const auto n=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_MANUAL_SMOKE_OUTPUT",raw,32768);
        MatureContact::Require(n<32768,"MANUAL_SMOKE_OUTPUT_TRUNCATED");manualEntrySmoke=n>0;
        manualSmokeRoute=Mistwatch::ManualEntryRoute::Resolve(manualEntrySmoke,false,std::wstring_view(route,routeLength));
        if(manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::Completion)
            MatureContact::Require(inputReplay.EnableCompletionRoute(),"MANUAL_COMPLETION_ROUTE_REJECTED");
        indexedGameplayOutput=manualEntrySmoke?std::filesystem::path(raw):save.parent_path()/"mature-indexed-runs"/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
        MatureContact::Require(indexedGameplayOutput.is_absolute()&&!std::filesystem::exists(indexedGameplayOutput),"MANUAL_EVIDENCE_REQUIRES_FRESH_ABSOLUTE_OUTPUT");
        std::filesystem::create_directories(indexedGameplayOutput);
        if(manualEntrySmoke){
            if(manualSmokeCostEnabled){
                MatureContact::Require(inputReplayCost.BeginStreaming(indexedGameplayOutput),"MANUAL_SMOKE_COST_STREAM_REJECTED");
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
        if(manualSmokeCaptureRequested||manualSmokeCapturePending){
            const auto s=e.GetRendererStats();
            if(manualSmokeCapturePending&&s.successfulFrameCaptures>manualSmokeCaptureBase){
                MatureContact::Require(s.successfulFrameCaptures==manualSmokeCaptureBase+1&&s.lastCapturedRenderSerial,"MANUAL_SMOKE_CAPTURE_IDENTITY_MISSING");
                auto path=indexedGameplayOutput/Mistwatch::ManualEntryRoute::CaptureName(manualSmokeRoute,manualSmokeCaptureCount);path+=L".uploaded-view.txt";
                std::ofstream receipt(path);receipt<<"render_serial="<<s.lastCapturedRenderSerial<<"\nuploaded_unjittered_vp_bits=";
                for(float v:s.lastCapturedUnjitteredViewProjection)receipt<<std::bit_cast<std::uint32_t>(v)<<',';
                receipt<<'\n';receipt.close();MatureContact::Require(bool(receipt),"MANUAL_UPLOADED_VIEW_WRITE_FAILED");
                manualSmokeCapturePending=false;++manualSmokeCaptureCount;
                if(!manualSmokeMenuDone){manualSmokeMenuDone=true;return true;}
                const bool completion=manualSmokeRoute==Mistwatch::ManualEntryRoute::Route::Completion;
                if(completion&&!manualSmokeBridgeDone){
                    MatureContact::Require(session.bridgeRepaired&&!inputReplay.Finished(),"MANUAL_BRIDGE_CAPTURE_STATE_MISSING");
                    manualSmokeBridgeDone=true;return true;
                }
                MatureContact::Require(manualSmokeCaptureCount==Mistwatch::ManualEntryRoute::CaptureCount(manualSmokeRoute)&&inputReplay.Passed()&&started&&!paused&&
                    (completion?manualSmokeNewExpeditionObserved&&session.bridgeRepaired&&session.beaconLit&&session.CurrentStage()==Mistwatch::Stage::Complete&&session.repairMaterials==7:session.repairMaterials==1)&&
                    controller.Settings().height>1.79f&&matureSaveHandlerCalls==1&&matureLoadHandlerCalls==1&&CaptureSaveGame().Serialize()==manualSmokeExpectedSave,"MANUAL_ENTRY_COMPLETE_OPERATION_MISSING");
                std::ofstream out(indexedGameplayOutput/"result.txt");out<<"passed=1\ninput=engine-GetInput\ninput_replay_directory_empty=1\nsource_oracles_enabled="<<indexedGameplayProofEnabled
                    <<"\nticks="<<inputReplay.Ticks()<<"\nmaterials="<<session.repairMaterials<<"\nheight="<<controller.Settings().height<<"\nsave_calls="<<matureSaveHandlerCalls<<"\nload_calls="<<matureLoadHandlerCalls
                    <<"\nworld="<<matureIndexedDomain->Identity()<<"\nrestored_snapshot_exact=1\nimages="<<manualSmokeCaptureCount
                    <<"\nroute="<<(completion?"completion":"shelter")<<"\nbridge_repaired="<<session.bridgeRepaired<<"\nstage="<<static_cast<unsigned>(session.CurrentStage())<<"\nbeacon_lit="<<session.beaconLit
                    <<"\npostwin_restart_observed="<<manualSmokeNewExpeditionObserved
                    <<"\nforest_surface="<<Mistwatch::ForestSurfaceProfile::Name(forestSurfaceSelection.profile)
                    <<"\ndebug_errors="<<s.debugErrorCount<<"\ndebug_warnings="<<s.debugWarningCount
                    <<"\ndebug_layer_enabled="<<s.debugLayerEnabled<<"\ngpu_validation_requested="<<s.gpuValidationRequested<<"\ngpu_validation_executed="<<s.gpuValidationExecuted
                    <<"\nmanual_physical_input=0\nfocus=synthetic-engine-input-focus-only-not-OS-focus\n";
                out.close();MatureContact::Require(bool(out),"MANUAL_SMOKE_RESULT_FAILED");manualSmokeFinished=true;e.Quit();return true;
            }
            MatureContact::Require(std::chrono::steady_clock::now()-manualSmokeCaptureStart<=std::chrono::seconds(15),"MANUAL_SMOKE_CAPTURE15S_TIMEOUT");return true;
        }
        if(!inputReplay.Finished()){
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
        for(unsigned k=0;k<256;++k)actual.SetKey(k,false);
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
                    manualSmokeNewExpeditionObserved=true;
                }
            }
            return false;
        }
        if(manualSmokeStage==2){MatureContact::Require(CaptureSaveGame().Serialize()!=manualSmokeExpectedSave,"MANUAL_F9_REQUIRES_ACTUALLY_CHANGED_MOTION_CLOCK");actual.SetKey(VK_F9,true);manualSmokeStage=3;return false;}
        MatureContact::Require(manualSmokeStage==3&&matureLoadHandlerCalls==1&&message=="Loaded"&&CaptureSaveGame().Serialize()==manualSmokeExpectedSave,"MANUAL_REAL_F9_RESTORE_FAILED");
        manualSmokeCaptureRequested=true;manualSmokeCaptureStart=std::chrono::steady_clock::now();return true;
    }
    void RenderManualEntrySmoke(Engine& e,const Matrix4& viewProjection){
        if(!manualEntrySmoke||!manualSmokeCaptureRequested)return;
        manualSmokeCaptureBase=e.GetRendererStats().successfulFrameCaptures;
        const auto path=indexedGameplayOutput/Mistwatch::ManualEntryRoute::CaptureName(manualSmokeRoute,manualSmokeCaptureCount);
        MatureContact::Require(e.CaptureNextFrame3D(path),"MANUAL_SMOKE_CAPTURE_REJECTED");
        auto metadata=path;metadata+=L".view.txt";std::ofstream out(metadata);out<<"expected_render_serial="<<e.GetRendererStats().submittedFrames+1<<"\nrequested_unjittered_vp_bits=";
        for(const auto& row:viewProjection.values)for(float v:row)out<<std::bit_cast<std::uint32_t>(v)<<',';
        out<<'\n';out.close();MatureContact::Require(bool(out),"MANUAL_SMOKE_VIEW_WRITE_FAILED");manualSmokeCaptureRequested=false;manualSmokeCapturePending=true;
    }
