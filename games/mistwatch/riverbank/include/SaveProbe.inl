    void ValidateMatureIndexedSave(){
        MatureContact::Require(bool(valleySaveDomain),"MATURE_INDEXED_SAVE_BASE_DOMAIN_MISSING");
        const auto oldBefore=RuntimeWorldSolids(false),oldAfter=RuntimeWorldSolids(true);
        Mistwatch::IndexedValleySaveDomain domain(*valleySaveDomain,terrain,oldBefore,oldAfter,matureContactWorld.All(false),matureContactWorld.All(true),RuntimeRoofHulls(),RuntimeRoofCameraOnly());
        auto snapshot=CaptureSaveGame();snapshot.worldIdentity=domain.Identity();
        const auto prepare=[&](const Mistwatch::SaveGame& saved,const Mistwatch::IndexedValleySaveDomain& source){
            return Mistwatch::PrepareIndexedSavedGame(saved,{controller,explorer,boar,orbit},terrain,RuntimeRoofCameraOnly(),RuntimeRoofHulls(),firLayoutSelector,fallenLogProfile,source);
        };
        const std::array<BoxCollider3D,1> mixedExtra{{{{1,2,3},{.1f,.1f,.1f}}}};
        const auto mixed=Mistwatch::PrepareSavedGame(snapshot,{controller,explorer,boar,orbit},terrain,RuntimeRoofCameraOnly(),mixedExtra,firLayoutSelector,fallenLogProfile,
            nullptr,RuntimeRoofHulls(),Mistwatch::LoadTerrainProfile::ValleyIndexedContactV1,nullptr,&domain);
        MatureContact::Require(!mixed&&mixed.error==Mistwatch::LoadPreparationError::InvalidExtraSolids,"MATURE_INDEXED_MIXED_SOURCE_ARGUMENTS_NOT_REJECTED");
        auto prepared=prepare(snapshot,domain);
        std::ofstream attempt(inputReplayDirectory/"indexed-save-attempt.txt");attempt<<"world="<<domain.Identity()<<"\nlegacy_world="<<valleySaveDomain->Identity()<<"\nprepare_error="<<unsigned(prepared.error)<<"\nvalid_snapshot="<<snapshot.Valid()<<'\n';attempt.close();
        MatureContact::Require(bool(attempt)&&bool(prepared)&&bool(Mistwatch::ValidatePreparedPairLoad(*prepared.candidate)),"MATURE_INDEXED_SAVE_PREPARE_FAILED");
        const auto path=inputReplayDirectory/"indexed-checkpoint.txt";
        MatureContact::Require(Mistwatch::WriteSaveFile(path,snapshot)==Mistwatch::SaveFileResult::Saved,"MATURE_INDEXED_SAVE_WRITE_FAILED");
        Mistwatch::SaveGame decoded;
        MatureContact::Require(Mistwatch::ReadSaveFile(path,decoded)&&decoded.Serialize()==snapshot.Serialize(),"MATURE_INDEXED_SAVE_FILE_ROUNDTRIP_FAILED");
        auto restored=prepare(decoded,domain);
        MatureContact::Require(bool(restored)&&bool(Mistwatch::ValidatePreparedPairLoad(*restored.candidate)),"MATURE_INDEXED_SAVED_FILE_RESTORE_FAILED");
        const auto& candidate=*restored.candidate;auto restoredSnapshot=candidate.snapshot;
        restoredSnapshot.motion=Mistwatch::SavedPlayerMotion{candidate.controller.CaptureMotionState(),candidate.locomotion.CaptureMotionState(),
            candidate.explorer.yaw,candidate.explorer.requestedYaw,candidate.explorer.phase,candidate.explorer.locomotionDistance,candidate.explorer.locomotionSeconds};
        MatureContact::Require(restoredSnapshot.Serialize()==snapshot.Serialize(),"MATURE_INDEXED_ACTUAL_MOTION_RESTORE_NOT_EXACT");
        auto foreign=snapshot;foreign.worldIdentity=valleySaveDomain->Identity();
        const auto rejected=prepare(foreign,domain);
        MatureContact::Require(!rejected&&rejected.error==Mistwatch::LoadPreparationError::InvalidTerrain,"MATURE_INDEXED_LEGACY_WORLD_NOT_REJECTED");
        auto changedBefore=std::vector<BoxCollider3D>(domain.Registered(false).begin(),domain.Registered(false).end());
        auto changedAfter=std::vector<BoxCollider3D>(domain.Registered(true).begin(),domain.Registered(true).end());
        changedBefore.back().center.x+=.125f;changedAfter.back().center.x+=.125f;
        Mistwatch::IndexedValleySaveDomain changed(*valleySaveDomain,terrain,oldBefore,oldAfter,changedBefore,changedAfter,RuntimeRoofHulls(),RuntimeRoofCameraOnly());
        const auto changedRejected=prepare(snapshot,changed);
        MatureContact::Require(changed.Identity()!=domain.Identity()&&!changedRejected&&changedRejected.error==Mistwatch::LoadPreparationError::InvalidTerrain,"MATURE_INDEXED_FULL_SOURCE_CHANGE_NOT_REJECTED");
        const auto originalBody=controller.StateHash(),originalOwner=playerLocomotion.StateHash();const auto originalPose=explorer;
        const auto targets=[&](){return Mistwatch::SavedGameCommitTargets{session,encounter,evasion,controller,explorer,boar,orbit,solids,playerCameraMotion,lastPlayerFraming,lastCameraMotion,lastRenderedCamera,&playerLocomotion};};
        Mistwatch::SwapPreparedSavedGame(*restored.candidate,targets());
        MatureContact::Require(MatureSameVec(controller.Position(),snapshot.session.feet)&&playerLocomotion.StateHash()==originalOwner,"MATURE_INDEXED_SAVE_COMMIT_FAILED");
        Mistwatch::SwapPreparedSavedGame(*restored.candidate,targets());
        MatureContact::Require(controller.StateHash()==originalBody&&playerLocomotion.StateHash()==originalOwner&&MatureSamePose(explorer,originalPose),"MATURE_INDEXED_SAVE_ROLLBACK_NOT_EXACT");
        std::vector<BoxCollider3D> overload(10001,BoxCollider3D{session.feet+Vec3{0,1,0},{.1f,.1f,.1f}});
        Mistwatch::IndexedValleySaveDomain busy(*valleySaveDomain,terrain,oldBefore,oldAfter,overload,overload,RuntimeRoofHulls(),RuntimeRoofCameraOnly());
        std::vector<BoxCollider3D> canary{{{123,456,789},{1,2,3}}};const auto beforeCanary=canary;
        MatureContact::Require(!busy.Body(false,session.feet,canary)&&canary.size()==beforeCanary.size()&&std::memcmp(canary.data(),beforeCanary.data(),sizeof(BoxCollider3D))==0,"MATURE_INDEXED_ACTIVE10000_OVERFLOW_NOT_ATOMIC");
        std::ofstream out(inputReplayDirectory/"indexed-save-result.txt");out<<"schema=indexed-valley-save-probe-v1\npassed=1\nregistered_before="<<domain.Registered(false).size()<<"\nregistered_after="<<domain.Registered(true).size()
            <<"\nworld="<<domain.Identity()<<"\nlegacy_world="<<valleySaveDomain->Identity()<<"\nplayer_active="<<candidate.solids.size()<<"\nenemy_active="<<candidate.enemySolids.size()<<"\ncamera_active="<<candidate.cameraSolids.size()
            <<"\nmixed_source_arguments_rejected=1\nfile_roundtrip=1\nactual_motion_restore_exact=1\nlegacy_world_rejected=1\nfull_source_change_rejected=1\ncommit_rollback_exact=1\nsynthetic10001_active_overflow_atomic=1\nlimits=One native stopped-player checkpoint,unrepaired bridge state only. No fresh-process resume,normal keyboard F5/F9,AI tick,plant replanning,bridge repair,new world adoption or release acceptance.\n";
        out.close();MatureContact::Require(bool(out),"MATURE_INDEXED_SAVE_RECEIPT_WRITE_FAILED");
    }
