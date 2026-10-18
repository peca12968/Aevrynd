// 納入MistwatchGame；匯出初始化完成的完整软根作者族群，與當幀剔除結果分開。
void WriteCompleteUnderstoryRetention(){
    if(!coreHerbEnabled)return;
    namespace R=Mistwatch::UnderstoryRetention;
    if(groundCover.size()!=3600||!forestCover.reachedBudget||forestCover.plants.size()!=3600||roadsideBroadleaf.plants.size()!=96||
       roadsideBroadleaf.left!=48||roadsideBroadleaf.right!=48||!coreHerbs.complete||coreHerbs.plants.size()!=192||
       coreHerbs.sides[0]!=96||coreHerbs.sides[1]!=96||coreHerbAcquisitions!=192||coreHerbProofs.size()!=192||!woodlandWorld||!coreHerbSurfaceFingerprint||
       diagnosticNettleMix||diagnosticCurvedScannedGrass||diagnosticProceduralGrass||!entranceUnderstoryEnabled)
        throw std::runtime_error("UNDERSTORY_RETENTION_COMPLETE_WORLD_REQUIRED");
    std::array<std::uint64_t,5> grassSeals{};std::array<std::uint64_t,2> nettleSeals{};
    const auto sourceSeal=[](const MeshResource3D& resource){
        if(!resource||!resource.cpuMesh||!resource.handle.contentPrimary||!resource.handle.contentSecondary)
            throw std::runtime_error("UNDERSTORY_RETENTION_ACTUAL_RESOURCE");
        return Mistwatch::CoreHerbClusters::Detail::MeshFingerprint(*resource.cpuMesh);
    };
    for(unsigned i=0;i<5;++i){grassSeals[i]=sourceSeal(scannedGrass[i]);
        const auto measured=Mistwatch::ForestCoverAssetFromMesh(*scannedGrass[i].cpuMesh);
        if(grassSeals[i]!=R::GrassSources[i]||measured.radius!=grassFootprints[i].radius||measured.minimumY!=grassFootprints[i].minimumY||
           measured.maximumY!=grassFootprints[i].maximumY)
            throw std::runtime_error("UNDERSTORY_RETENTION_GRASS_SOURCE");}
    for(unsigned i=0;i<2;++i){nettleSeals[i]=sourceSeal(roadsideBroadleafMeshes[i]);
        const auto measured=Mistwatch::MeasureNettleAssetBounds(*roadsideBroadleafMeshes[i].cpuMesh);
        if(nettleSeals[i]!=R::NettleSources[i]||measured.radius!=roadsideBroadleafBounds[i].radius||
           measured.minimumY!=roadsideBroadleafBounds[i].minimumY||measured.maximumY!=roadsideBroadleafBounds[i].maximumY)
            throw std::runtime_error("UNDERSTORY_RETENTION_NETTLE_SOURCE");}
    if(!std::is_sorted(entranceUnderstory.suppressedGrassIds.begin(),entranceUnderstory.suppressedGrassIds.end())||
       std::adjacent_find(entranceUnderstory.suppressedGrassIds.begin(),entranceUnderstory.suppressedGrassIds.end())!=entranceUnderstory.suppressedGrassIds.end()||
       !std::is_sorted(entranceUnderstory.suppressedBroadleafIds.begin(),entranceUnderstory.suppressedBroadleafIds.end())||
       std::adjacent_find(entranceUnderstory.suppressedBroadleafIds.begin(),entranceUnderstory.suppressedBroadleafIds.end())!=entranceUnderstory.suppressedBroadleafIds.end())
        throw std::runtime_error("UNDERSTORY_RETENTION_SUPPRESSION_ORDER");
    if(entranceUnderstory.suppressedGrassIds.size()>180||entranceUnderstory.suppressedBroadleafIds.size()>48)
        throw std::runtime_error("UNDERSTORY_RETENTION_SUPPRESSION_BUDGET");
    for(auto id:entranceUnderstory.suppressedGrassIds)if(id>=3600)throw std::runtime_error("UNDERSTORY_RETENTION_SUPPRESSION_ID");
    for(auto id:entranceUnderstory.suppressedBroadleafIds)if(id>=96)throw std::runtime_error("UNDERSTORY_RETENTION_SUPPRESSION_ID");
    std::vector<R::Row> rows;rows.reserve(R::Total);
    for(unsigned i=0;i<groundCover.size();++i){const auto anchor=groundCover[i];const auto variant=forestCover.plants[i].variant;
        if(variant>=5)throw std::runtime_error("UNDERSTORY_RETENTION_GRASS_VARIANT");
        const float scale=Mistwatch::ForestCoverScale(anchor);
        const Transform3D pose{anchor,{0,anchor.x+anchor.z,0},{scale,scale,scale}};
        const auto& original=forestCover.plants[i];
        const auto same=[](float a,float b){return std::bit_cast<unsigned>(a)==std::bit_cast<unsigned>(b);};
        if(!same(original.transform.position.x,pose.position.x)||!same(original.transform.position.y,pose.position.y)||
           !same(original.transform.position.z,pose.position.z)||!same(original.transform.rotation.x,pose.rotation.x)||
           !same(original.transform.rotation.y,pose.rotation.y)||!same(original.transform.rotation.z,pose.rotation.z)||
           !same(original.transform.scale.x,scale)||!same(original.transform.scale.y,scale)||!same(original.transform.scale.z,scale)||
           !same(original.radius,grassFootprints[variant].radius*scale))
            throw std::runtime_error("UNDERSTORY_RETENTION_AUTHOR_RENDER_DISAGREEMENT");
        const float radius=grassFootprints[variant].radius*scale+Mistwatch::CoreHerbClusters::WindRadius;
        rows.push_back({R::Family::ForestGrass,i,variant,anchor,pose,radius,
            std::binary_search(entranceUnderstory.suppressedGrassIds.begin(),entranceUnderstory.suppressedGrassIds.end(),i),
            grassSeals[variant],grassSeals[variant],scannedGrass[variant].handle.value});
    }
    for(unsigned i=0;i<roadsideBroadleaf.plants.size();++i){const auto& p=roadsideBroadleaf.plants[i];
        if(p.variant>=2)throw std::runtime_error("UNDERSTORY_RETENTION_BROADLEAF_VARIANT");
        const auto& bounds=roadsideBroadleafBounds[p.variant];
        const Vec3 anchor{p.transform.position.x,p.transform.position.y+bounds.minimumY*p.transform.scale.x,p.transform.position.z};
        const float radius=bounds.radius*p.transform.scale.x+Mistwatch::CoreHerbClusters::WindRadius;
        if(radius!=p.radius)throw std::runtime_error("UNDERSTORY_RETENTION_BROADLEAF_RADIUS");
        rows.push_back({R::Family::RoadsideBroadleaf,i,p.variant+2,anchor,p.transform,radius,
            std::binary_search(entranceUnderstory.suppressedBroadleafIds.begin(),entranceUnderstory.suppressedBroadleafIds.end(),i),
            nettleSeals[p.variant],nettleSeals[p.variant],roadsideBroadleafMeshes[p.variant].handle.value});
    }
    for(unsigned i=0;i<coreHerbs.plants.size();++i){const auto& p=coreHerbs.plants[i];const auto actual=sourceSeal(coreHerbMeshes[i]);
        if(actual!=p.derivedFingerprint||coreHerbProofs[i].derivedFingerprint!=actual||coreHerbProofs[i].sourceFingerprint!=p.sourceFingerprint||
           !coreHerbProofs[i].contact.safe||!coreHerbProofs[i].sourceFootPieces||!coreHerbProofs[i].windPinnedVertices||
           coreHerbProofs[i].allFaces!=coreHerbMeshes[i].cpuMesh->indices.size()/3||coreHerbProofs[i].surfaceFingerprint!=coreHerbSurfaceFingerprint)
            throw std::runtime_error("UNDERSTORY_RETENTION_CORE_RESOURCE_PROOF");
        rows.push_back({R::Family::CoreHerb,i,p.variant,p.root,p.pose,p.radius,false,p.sourceFingerprint,actual,coreHerbMeshes[i].handle.value});
    }
    const auto summary=R::Validate(rows);
    completeUnderstoryRows=rows;
    if(summary.suppressed[0]!=entranceUnderstory.suppressedGrassIds.size()||summary.suppressed[1]!=entranceUnderstory.suppressedBroadleafIds.size())
        throw std::runtime_error("UNDERSTORY_RETENTION_SUPPRESSION_COUNT");
    const auto prefix=CapturePath().empty()?inputReplayDirectory/"first-frame":CapturePath();
    if(prefix.empty())throw std::runtime_error("UNDERSTORY_RETENTION_OUTPUT_CONTEXT");
    auto csvPath=prefix;csvPath+=L".understory-retention.csv";auto metaPath=prefix;metaPath+=L".understory-retention.txt";
    if(std::filesystem::exists(csvPath)||std::filesystem::exists(metaPath))throw std::runtime_error("UNDERSTORY_RETENTION_REFUSES_OVERWRITE");
    std::ofstream csv(csvPath);csv.imbue(std::locale::classic());csv<<std::setprecision(17);
    csv<<"family,id,variant,anchor_x,anchor_y,anchor_z,pose_x,pose_y,pose_z,rx,ry,rz,sx,sy,sz,radius,suppressed,source_fingerprint,actual_fingerprint,handle\n";
    for(const auto& r:rows){csv<<R::Name(r.family)<<','<<r.id<<','<<r.variant;
        for(float f:{r.anchor.x,r.anchor.y,r.anchor.z,r.pose.position.x,r.pose.position.y,r.pose.position.z,
                     r.pose.rotation.x,r.pose.rotation.y,r.pose.rotation.z,r.pose.scale.x,r.pose.scale.y,r.pose.scale.z,r.footprintRadius})csv<<','<<f;
        csv<<','<<r.suppressed<<','<<r.sourceFingerprint<<','<<r.actualFingerprint<<','<<r.handle<<'\n';}
    csv.close();if(!csv)throw std::runtime_error("UNDERSTORY_RETENTION_CSV_WRITE");
    std::ofstream meta(metaPath);meta<<"schema=mistwatch.complete-soft-root-retention.1\nrows="<<rows.size()
        <<"\nscope=initial-resource-plans-not-opaque-draws\ncomplete_soft_roots=1\nopaque_observation=0\nall_soft_root_support_proven=0"
        <<"\nwind_radius="<<std::setprecision(17)<<Mistwatch::CoreHerbClusters::WindRadius
        <<"\nwind_up="<<Mistwatch::CoreHerbClusters::WindUp<<"\nsurface_fingerprint="<<coreHerbSurfaceFingerprint
        <<"\ncore_plan_fingerprint="<<coreHerbs.fingerprint<<"\ncore_author_fingerprint="<<coreHerbs.authorFingerprint
        <<"\nretained_hard_volume_csv=separate-core-herb-occupancy\nart_accepted=0"
        <<"\nepoch=post-relief-post-core-upload-pre-update\nfamily_0_anchor=author-pivot\nfamily_1_anchor=reconstructed-source-minimum-datum"
        <<"\nfamily_2_anchor=final-ground-sampler-datum\nradius_encoding=world-rest-radius-plus-wind-once-core-outward-rounded"
        <<"\nfamily_0_wind=normalized-source-tip-height\nfamily_1_wind=original-normalized-nettle\nfamily_2_wind=source-height-root-pinned"
        <<"\noriginal_grass_gpu_object_identity=not-present\n";
    for(unsigned i=0;i<3;++i)meta<<"domain_"<<i<<"_full_mesh_fingerprint="<<sourceSeal(entranceRoadMeshes[i])
        <<"\ndomain_"<<i<<"_handle="<<entranceRoadMeshes[i].handle.value<<'\n';
    for(unsigned i=0;i<3;++i)meta<<"family_"<<i<<"_count="<<summary.counts[i]<<"\nfamily_"<<i<<"_suppressed="<<summary.suppressed[i]
        <<"\nfamily_"<<i<<"_fingerprint="<<summary.fingerprints[i]<<'\n';
    meta.close();if(!meta)throw std::runtime_error("UNDERSTORY_RETENTION_META_WRITE");
}
