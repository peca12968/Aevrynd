    #include "RiverbankMethods.inl"
    std::ofstream indexedGameplayTrace;
    unsigned indexedGameplayCommits{},indexedGameplayRepairedCommits{},indexedBridgeComparisons{};
    void InitializeIndexedGameplayWorld(Engine& e){
        InitializeManualEntryEvidence();
        MatureContact::Require(valleySaveDomain&&forestPlayableWorld&&capture.empty()&&!session.bridgeRepaired&&!started&&session.collected==0&&session.repairMaterials==0,
            "INDEXED_WORLD_REQUIRES_ORIGINAL_FRESH_FOREST_MENU");
        const auto before=RuntimeWorldSolids(false),after=RuntimeWorldSolids(true);
        matureContactWorld.Load(e.ExecutableDirectory()/"assets/mature-contact",before,after);matureStudy.contactWorld=&matureContactWorld;
        RebuildRiverbank(e);
        matureIndexedDomain=std::make_unique<Mistwatch::IndexedValleySaveDomain>(*valleySaveDomain,*riverbankOriginal,terrain,before,after,
            matureContactWorld.All(false),matureContactWorld.All(true),RuntimeRoofHulls(),RuntimeRoofCameraOnly());
        solids=RuntimeGameplayBody(session.bridgeRepaired,session.feet);
        ResetController();
        MatureContact::Require(UpdatePlayerCamera(1.f/60.f,solids),"INDEXED_NORMAL_ENTRY_INITIAL_CAMERA_UNAVAILABLE");
        if(!indexedGameplayProofEnabled)return;
        indexedGameplayTrace.open(inputReplayDirectory/"indexed-gameplay-commits.tsv");indexedGameplayTrace.precision(9);
        indexedGameplayTrace<<"commit\treplay_tick\tx\ty\tz\tstage\tsupplies\tmaterials\trepaired\tbody_active\tnpc_active\tcamera_active\tbody_oracles\tcamera_oracles\tnpc_queries\tbridge_comparisons\n";
        MatureContact::Require(bool(indexedGameplayTrace),"INDEXED_GAMEPLAY_TRACE_OPEN_FAILED");
    }
    Mistwatch::BridgeRepairPlayerResult PrepareIndexedGameplayBridge(){
        MatureContact::Require(bool(matureIndexedDomain),"INDEXED_BRIDGE_WITHOUT_REGISTERED_WORLD");
        if(!indexedGameplayProofEnabled)return Mistwatch::PrepareBridgeRepairPlayer(controller,explorer,*matureIndexedDomain,RuntimeRoofHulls(),terrain,playerCameraMotion.Camera(),orbit.distance,RuntimeCameraOnly());
        auto fullPose=explorer;
        const auto actual=Mistwatch::PrepareBridgeRepairPlayer(controller,explorer,*matureIndexedDomain,
            RuntimeRoofHulls(),terrain,playerCameraMotion.Camera(),orbit.distance,RuntimeCameraOnly());
        const auto full=Mistwatch::PrepareBridgeRepairPlayer(controller,fullPose,matureIndexedDomain->Registered(true),
            RuntimeRoofHulls(),terrain,playerCameraMotion.Camera(),RuntimeCameraOnly());
        MatureContact::Require(actual.reason==full.reason&&MatureSamePose(explorer,fullPose),"INDEXED_BRIDGE_PREFLIGHT_DIFFERS_FROM_COMPLETE_WORLD");
        ++indexedBridgeComparisons;return actual;
    }
    void RecordIndexedGameplayCommit(){
        if(!indexedGameplayProofEnabled)return;
        ++indexedGameplayCommits;if(session.bridgeRepaired)++indexedGameplayRepairedCommits;
        const auto p=controller.Position();
        indexedGameplayTrace<<indexedGameplayCommits<<'\t'<<inputReplay.Ticks()<<'\t'<<p.x<<'\t'<<p.y<<'\t'<<p.z<<'\t'<<unsigned(session.CurrentStage())
            <<'\t'<<session.collected<<'\t'<<session.repairMaterials<<'\t'<<session.bridgeRepaired<<'\t'<<solids.size()<<'\t'<<runtimeNpcSolids.size()<<'\t'<<runtimeCameraSolids.size()
            <<'\t'<<normalBodyOracleMatches<<'\t'<<normalCameraOracleMatches<<'\t'<<normalNpcSourceMatches<<'\t'<<indexedBridgeComparisons<<'\n';
        indexedGameplayTrace.flush();MatureContact::Require(bool(indexedGameplayTrace),"INDEXED_GAMEPLAY_COMMIT_TRACE_FAILED_AFTER_REAL_COMMIT");
    }
    void WriteIndexedGameplayResult(){
        indexedGameplayTrace.close();MatureContact::Require(bool(indexedGameplayTrace),"INDEXED_GAMEPLAY_COMMIT_TRACE_CLOSE_FAILED");
        matureContactWorld.Write(inputReplayDirectory);
        std::ofstream out(inputReplayDirectory/"indexed-gameplay-result.txt");out.precision(9);
        out<<"schema=indexed-normal-entry-gameplay-v1\noriginal_replay_passed="<<inputReplay.Passed()<<"\noriginal_failure="<<inputReplay.Failure()
            <<"\nworld="<<matureIndexedDomain->Identity()<<"\ncommitted_frames="<<indexedGameplayCommits<<"\nrepaired_world_commits="<<indexedGameplayRepairedCommits
            <<"\nbridge_preflight_full_source_comparisons="<<indexedBridgeComparisons<<"\nbody_full_source_comparisons="<<normalBodyOracleMatches<<"\ncamera_full_source_comparisons="<<normalCameraOracleMatches
            <<"\nnpc_full_source_queries="<<normalNpcSourceMatches<<"\nlive_npc_source_frames="<<normalActiveNpcFrames<<"\nphysics_fixed_steps="<<normalMotionFixedSteps
            <<"\nmaximum_body_active="<<normalBodyMaximum<<"\nmaximum_camera_active="<<normalCameraMaximum<<"\nmaximum_npc_active="<<normalNpcMaximum
            <<"\nbridge_repaired="<<session.bridgeRepaired<<"\nsupplies="<<session.collected<<"\nmaterials="<<session.repairMaterials<<"\nsource_failure="<<runtimeSourceFailure
            <<"\nregistered_before="<<matureIndexedDomain->Registered(false).size()<<"\nregistered_after="<<matureIndexedDomain->Registered(true).size()
            <<"\nentry=original-menu-and-original-key-state-replay\nseeded_progress=0\nteleported_to_objective=0\nmanual_play=0\nformal_adoption=0\n";
        out.close();MatureContact::Require(bool(out),"INDEXED_GAMEPLAY_RESULT_WRITE_FAILED");
    }
