// 納入 MistwatchGame；新增軟植被獨立身分，保留所有舊作者族群與碰撞。
unsigned ReleaseCoreHerbs(Engine& e){unsigned count{};
    for(auto& mesh:coreHerbMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++count;}mesh={};}return count;
}
void RollbackCoreHerbs(Engine& e)noexcept{
    for(auto& mesh:coreHerbMeshes){if(!mesh)continue;try{e.DestroyMesh3D(mesh.handle);mesh={};}catch(...){}}
}
void InitializeCoreHerbs(Engine& e){
    if(!coreHerbEnabled)return;
    if(!woodlandWorld||!cliffReliefEnabled||!cliffCoverageEnabled||!cliffMaterialEnabled||!entranceRoadEnabled||!settlementSurfaceEnabled||!roadsideBroadleafEnabled||
        !entranceUnderstoryEnabled||mixedLandscapeCanopy||authoredLandscapePlanting||shoulderGroveEnabled||outerForestFloor||
        rightForeground||deciduousHabitat||rockSegmentEnabled||diagnosticScatterCover||diagnosticProceduralGrass||diagnosticLegacyFern)
        throw std::runtime_error("Core herbs require the isolated complete original woodland world");
    Mistwatch::CoreHerbClusters::Sources sources;
    for(unsigned slot=0;slot<2;++slot){if(!roadsideBroadleafMeshes[slot].cpuMesh)throw std::runtime_error("Core herb source unavailable");
        sources[slot]=roadsideBroadleafMeshes[slot].cpuMesh.get();}
    Mesh3D actualGround;
    for(unsigned slot=0;slot<3;++slot){const auto& resource=entranceRoadMeshes[slot];
        if(!resource||!resource.cpuMesh)throw std::runtime_error("Core herb actual ground unavailable");
        const auto& source=*resource.cpuMesh;std::map<unsigned,unsigned> remap;
        for(auto index:source.indices){auto found=remap.find(index);
            if(found==remap.end()){const auto& v=source.vertices[index];
                if(!terrain.Contains(v.x,v.z)||std::abs(double(v.y)-terrain.SampleHeight(v.x,v.z))>0.000002)
                    throw std::runtime_error("Core herb actual ground/collision disagreement");
                const auto next=static_cast<unsigned>(actualGround.vertices.size());actualGround.vertices.push_back(v);
                found=remap.emplace(index,next).first;}
            actualGround.indices.push_back(found->second);
        }
    }
    coreHerbSurfaceFingerprint=Mistwatch::OuterUnderstory::Detail::MeshFingerprint(actualGround);
    const Mistwatch::LandscapeSpeciesDetail::Surface surface(actualGround);
    std::vector<BoxCollider3D> retained;
    const auto retain=[&](const Mesh3D& mesh,const Transform3D& pose){
        if(!mesh.Valid())throw std::runtime_error("Core herb retained vegetation resource unavailable");
        const auto b=TransformBounds(mesh.Bounds(),pose);
        retained.push_back({b.center+Vec3{0,.0055f,0},b.extents+Vec3{.19f,.0055f,.19f}});
    };
    for(const auto& p:shrubCanopy.plants){const auto& raw=shrubCanopy.variants[p.variant];retain(raw.stems,p.pose);retain(raw.leaves,p.pose);}
    for(const auto& p:entranceUnderstory.plants){const auto& raw=shrubCanopy.variants[p.variant];retain(raw.stems,p.pose);retain(raw.leaves,p.pose);}
    for(unsigned i=0;i<understory.ferns.size();++i)retain(*scannedFerns[i%4].cpuMesh,understory.ferns[i]);
    for(const auto& p:fernColonies.plants)retain(*scannedFerns[p.variant].cpuMesh,p.transform);
    // 留下目前真世界的保守障礙資料；供下一輪選擇空隙，不把2D圖當作碰撞或美術證據。
    const auto occupancyPrefix=CapturePath().empty()?inputReplayDirectory/"first-frame":CapturePath();
    auto occupancyPath=occupancyPrefix;occupancyPath+=L".core-herb-occupancy.csv";
    if(std::filesystem::exists(occupancyPath))throw std::runtime_error("Core herb occupancy refuses overwrite");
    std::ofstream occupancy(occupancyPath);occupancy<<std::setprecision(17)<<"kind,id,x,y,z,ex,ey,ez\n";
    const auto writeBoxes=[&](std::string_view kind,std::span<const BoxCollider3D> boxes){
        for(unsigned i=0;i<boxes.size();++i){const auto& b=boxes[i];occupancy<<kind<<','<<i<<','<<b.center.x<<','<<b.center.y
            <<','<<b.center.z<<','<<b.extents.x<<','<<b.extents.y<<','<<b.extents.z<<'\n';}
    };
    writeBoxes("retained-vegetation",retained);writeBoxes("world-before",woodlandWorld->Bodies(false));
    writeBoxes("world-after",woodlandWorld->Bodies(true));
    unsigned woodGroup{};
    for(const auto& group:woodlandWorld->Wood().Groups())
        writeBoxes("world-wood-"+std::to_string(woodGroup++),group);
    occupancy.close();if(!occupancy)throw std::runtime_error("Core herb occupancy write failed");
    coreHerbProofs.reserve(Mistwatch::CoreHerbClusters::Target);
    const auto placementStart=std::chrono::steady_clock::now();
    unsigned proofCalls{};
    std::array<unsigned,static_cast<unsigned>(Mistwatch::OuterUnderstory::Reject::Count)> proofRejected{};
    const auto choose=[&](const Mistwatch::CoreHerbClusters::Plant& p){
        ++proofCalls;
        auto proof=Mistwatch::CoreHerbSupport::Check(surface,*sources[p.variant-2],p.derivedMesh,p.pose,p.root,p.radius,coreHerbSurfaceFingerprint);
        if(!proof.contact.safe){++proofRejected[static_cast<unsigned>(proof.contact.rejection)];return false;}
        coreHerbProofs.push_back(proof);
        if(coreHerbProofs.size()%16==0)std::cout<<"CORE_HERB_SUPPORT_PROGRESS accepted="<<coreHerbProofs.size()
            <<" proof_calls="<<proofCalls<<" elapsed_ms="<<std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-placementStart).count()<<'\n'<<std::flush;
        return true;
    };
    coreHerbs=Mistwatch::CoreHerbClusters::Create(sources,terrain,{}, {},&*woodlandWorld,retained,{},choose,
        Mistwatch::CoreHerbRouteLayout::Layout(coreHerbLayoutProfile));
    const auto placementEnd=std::chrono::steady_clock::now();
    const auto prefix=CapturePath().empty()?inputReplayDirectory/"first-frame":CapturePath();
    if(prefix.empty())throw std::runtime_error("Core herb diagnostic output unavailable");
    auto placementPath=prefix;placementPath+=L".core-herb-placement.txt";
    if(std::filesystem::exists(placementPath))throw std::runtime_error("Core herb placement receipt refuses overwrite");
    std::ofstream placement(placementPath);
    placement<<"profile="<<Mistwatch::CoreHerbClusters::Profile<<"\nsampling="<<Mistwatch::CoreHerbClusters::SamplingProfile<<"\ncomplete="<<coreHerbs.complete
        <<"\npopulation="<<coreHerbs.plants.size()<<"\nproposals="<<coreHerbs.proposals
        <<"\nproof_calls="<<proofCalls<<"\nproof_safe="<<coreHerbProofs.size()
        <<"\nplacement_ns="<<std::chrono::duration_cast<std::chrono::nanoseconds>(placementEnd-placementStart).count()<<'\n';
    placement<<"author_fingerprint="<<coreHerbs.authorFingerprint<<'\n';
    placement<<"author_profile="<<Mistwatch::CoreHerbRouteLayout::Name(coreHerbLayoutProfile)<<'\n';
    for(unsigned reason=0;reason<coreHerbs.rejected.size();++reason)placement<<"reject_"<<reason<<'='<<coreHerbs.rejected[reason]<<'\n';
    for(unsigned reason=0;reason<proofRejected.size();++reason)placement<<"proof_reject_"<<reason<<'='<<proofRejected[reason]<<'\n';
    for(unsigned cluster=0;cluster<coreHerbs.clusters.size();++cluster){const auto& c=coreHerbs.clusters[cluster];
        placement<<"cluster_"<<cluster<<"_accepted="<<c.count<<"\ncluster_"<<cluster<<"_proposals="<<c.proposals<<'\n';
        placement<<std::setprecision(17)<<"cluster_"<<cluster<<"_centre="<<c.centre.x<<','<<c.centre.y<<','<<c.centre.z
            <<"\ncluster_"<<cluster<<"_author_rotation="<<c.authorRotation<<'\n';
        for(unsigned reason=0;reason<c.rejected.size();++reason)placement<<"cluster_"<<cluster<<"_reject_"<<reason<<'='<<c.rejected[reason]<<'\n';
    }
    placement.close();if(!placement)throw std::runtime_error("Core herb placement receipt write failed");
    if(!coreHerbs.complete||coreHerbs.plants.size()!=Mistwatch::CoreHerbClusters::Target||!coreHerbs.groupedWorldVerified)
        throw std::runtime_error("Core herb full-world cluster shortfall:"+std::to_string(coreHerbs.plants.size()));
    for(unsigned ordinal=0;ordinal<coreHerbs.plants.size();++ordinal){const auto& p=coreHerbs.plants[ordinal];
        if(coreHerbProofs.size()!=coreHerbs.plants.size()||!coreHerbProofs[ordinal].contact.safe||
            coreHerbProofs[ordinal].derivedFingerprint!=p.derivedFingerprint||coreHerbProofs[ordinal].sourceFingerprint!=p.sourceFingerprint||
            coreHerbProofs[ordinal].surfaceFingerprint!=coreHerbSurfaceFingerprint||coreHerbProofs[ordinal].allFaces!=p.derivedMesh.indices.size()/3)
            throw std::runtime_error("Core herb selected proof identity mismatch");
    }
    // 完整證明後才上傳，任何局部失敗不渲染半份群落或放寬原始支撐門。
    for(unsigned i=0;i<coreHerbs.plants.size();++i){auto& mesh=coreHerbMeshes[i];const auto& p=coreHerbs.plants[i];
        mesh=e.CreateMeshResource3D(p.derivedMesh);
        if(!mesh||!mesh.cpuMesh||Mistwatch::CoreHerbClusters::Detail::MeshFingerprint(*mesh.cpuMesh)!=p.derivedFingerprint)
            throw std::runtime_error("Core herb GPU geometry identity mismatch");++coreHerbAcquisitions;
    }
}
void WriteCoreHerbReceipt(const std::filesystem::path& frame,const CoreHerbFrame& observed)const{
    auto path=frame;path+=L".core-herbs.txt";
    if(std::filesystem::exists(path))throw std::runtime_error("Core herb receipt refuses overwrite");
    std::ofstream out(path);out<<"schema=mistwatch.core-herbs-native.1\nprofile="
        <<(coreHerbEnabled?Mistwatch::CoreHerbClusters::Profile:"original")
        <<"\nsampling="<<Mistwatch::CoreHerbClusters::SamplingProfile<<"\npopulation="<<coreHerbs.plants.size()
        <<"\nplan_fingerprint="<<coreHerbs.fingerprint<<"\nsurface_fingerprint="<<coreHerbSurfaceFingerprint
        <<"\nauthor_fingerprint="<<coreHerbs.authorFingerprint
        <<"\nauthor_profile="<<Mistwatch::CoreHerbRouteLayout::Name(coreHerbLayoutProfile)
        <<"\nexact_support_count="<<coreHerbProofs.size()<<"\nworld_verified="<<coreHerbs.groupedWorldVerified
        <<"\nroot_policy="<<Mistwatch::CoreHerbSupport::Profile<<"\nroot_wind="<<Mistwatch::CoreHerbClusters::RootWindProfile
        <<"\nselection_rejections="<<coreHerbs.rejected[static_cast<unsigned>(Mistwatch::CoreHerbClusters::Rejection::SupportSelection)]
        <<"\nsource_2="<<coreHerbs.sourceFingerprints[0]<<"\nsource_3="<<coreHerbs.sourceFingerprints[1]
        <<"\nacquisitions="<<coreHerbAcquisitions<<"\naccepted="<<observed.accepted<<"\nactual_draws="<<observed.draws
        <<"\nopaque="<<observed.opaque<<"\nshadow_only="<<observed.shadowOnly<<"\ntriangles="<<observed.triangles
        <<"\nscene_serial="<<observed.sceneSerial<<"\ncamera_hash="<<observed.cameraHash<<"\nart_accepted=0\n";
    for(unsigned i=0;i<coreHerbs.plants.size();++i){const auto& p=coreHerbs.plants[i];out<<"plant_"<<i<<"_derived="<<p.derivedFingerprint
        <<"\nplant_"<<i<<"_variant="<<p.variant<<"\nplant_"<<i<<"_cluster="<<p.cluster
        <<"\nplant_"<<i<<"_source="<<p.sourceFingerprint
        <<std::setprecision(17)<<"\nplant_"<<i<<"_root="<<p.root.x<<','<<p.root.y<<','<<p.root.z
        <<"\nplant_"<<i<<"_pose="<<p.pose.position.x<<','<<p.pose.position.y<<','<<p.pose.position.z
        <<"\nplant_"<<i<<"_scale="<<p.pose.scale.x<<"\nplant_"<<i<<"_yaw="<<p.pose.rotation.y
        <<"\nplant_"<<i<<"_handle="<<coreHerbMeshes[i].handle.value<<"\nplant_"<<i<<"_visible="<<observed.visibility[i]
        <<"\nplant_"<<i<<"_cull_reason="<<observed.cullReasons[i]
        <<"\nplant_"<<i<<"_all_faces="<<coreHerbProofs[i].allFaces<<"\nplant_"<<i<<"_root_pieces="<<coreHerbProofs[i].sourceFootPieces
        <<"\nplant_"<<i<<"_safe="<<coreHerbProofs[i].contact.safe<<'\n';}
    out.close();if(!out)throw std::runtime_error("Core herb receipt write failed");
}
