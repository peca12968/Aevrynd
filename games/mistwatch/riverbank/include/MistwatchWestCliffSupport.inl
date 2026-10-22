// 納入 MistwatchGame 類別內；只驗證目前完整岩層世界，不推測其他診斷分支。
// 投影重疊只在整面高於兩個實際地形的保守上界時放行；接地或不確定仍拒絕。
std::array<std::uint64_t,4> RequireWestCliffSupportClearance(const HeightField3D& derived)const {
    if((!cliffWorldEnabled&&capture.empty())||!cliffMaterialEnabled||!cliffCoverageEnabled||!woodlandWorld||!canopyEnabled||!shrubsEnabled||!middleLayerEnabled||
       !landscapeStandFill||!landscapeTwigDistanceLod||!shoulderGroundcoverEnabled||
       authoredLandscapePlanting||shoulderGroveEnabled||rockSegmentEnabled||
       diagnosticScatterCover||diagnosticProceduralGrass||diagnosticCurvedScannedGrass||
       diagnosticNettleMix||diagnosticLegacyFern||diagnosticLegacyRangerHut||
       !plankedWorkshop||!plankedShelter||!repairToolsDetailed||!trailLitterEnabled||
       fallenLogProfile!=Mistwatch::FallenLogProfile::ScannedStraightV1)
        throw std::runtime_error("WEST_CLIFF_SUPPORT_UNSUPPORTED_WORLD");
    if(landscapeTrees.size()!=520||landscapeStandPlan.combined.size()!=616||
       groundCover.size()!=forestCover.plants.size()||canopyPopulation.plants.size()!=canopyRootMeshes.size())
        throw std::runtime_error("WEST_CLIFF_SUPPORT_COHORT_SIZE");
    if(mixedLandscapeCanopy){
        Mistwatch::MixedLandscapeCanopy::ValidateStand(terrain,landscapeTrees,landscapeStandBounds,landscapeStandPlan);
        if(!landscapePlanting.complete||landscapePlanting.plants.size()!=16||landscapeRootAcquisitions!=16||
            !landscapeMesh.cpuMesh||Mistwatch::RootSupportDetail::MeshFingerprint(*landscapeMesh.cpuMesh)!=landscapePlanting.surfaceFingerprint)
            throw std::runtime_error("WEST_CLIFF_SUPPORT_MIXED_COHORT");
        for(std::size_t i=0;i<landscapePlanting.plants.size();++i){const auto& p=landscapePlanting.plants[i];
            if(p.sourceOrdinal!=Mistwatch::AuthoredLandscapeOrdinals[i]||p.proof.result!=Mistwatch::RootSupportResult::Safe||
                !landscapeRootMeshes[i].cpuMesh||Mistwatch::RootSupportDetail::MeshFingerprint(*landscapeRootMeshes[i].cpuMesh)!=p.proof.derivedMeshFingerprint)
                throw std::runtime_error("WEST_CLIFF_SUPPORT_MIXED_ROOT");
        }
    }
    std::array<std::uint64_t,4> counts{};
    const Mistwatch::WestCliffClearance::Envelope envelope(terrain,derived);
    constexpr auto windSweep=Mistwatch::WestCliffClearance::FoliageSweep;
    constexpr float minX=-49.5f,maxX=-40.5f,minZ=50.5f,maxZ=65.5f;
    const auto fail=[](const std::string& group,std::size_t id,const char* reason){
        throw std::runtime_error("WEST_CLIFF_SUPPORT_"+std::string(reason)+":"+group+":"+std::to_string(id));
    };
    const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    const auto overlaps=[](Vec3 lo,Vec3 hi){
        return !(hi.x<minX||lo.x>maxX||hi.z<minZ||lo.z>maxZ);
    };
    const auto triangle=[&](const std::string& group,std::size_t id,Vec3 a,Vec3 b,Vec3 c){
        if(!finite(a)||!finite(b)||!finite(c))fail(group,id,"NONFINITE_TRIANGLE");
        ++counts[1];
        const Vec3 lo{(std::min)({a.x,b.x,c.x}),(std::min)({a.y,b.y,c.y}),(std::min)({a.z,b.z,c.z})};
        const Vec3 hi{(std::max)({a.x,b.x,c.x}),(std::max)({a.y,b.y,c.y}),(std::max)({a.z,b.z,c.z})};
        if(overlaps(lo-windSweep,hi+windSweep)){
            const auto proof=envelope.Query(a,b,c,windSweep);
            if(proof.disjoint)fail(group,id,"GUARD_PROOF_DISAGREEMENT");
            if(proof.separated){++counts[3];return;}
            // 保留真三角面座標與地形上界，便於分辨保守拒絕與實際接地。
            std::ostringstream detail;
            detail<<"WEST_CLIFF_SUPPORT_UNPROVEN_CLEARANCE:"<<group<<":"<<id
                <<" minY="<<proof.minimumY<<" ceiling="<<proof.ceiling<<" padding="<<proof.padding<<" cells="<<proof.cells
                <<" a="<<a.x<<","<<a.y<<","<<a.z<<" b="<<b.x<<","<<b.y<<","<<b.z<<" c="<<c.x<<","<<c.y<<","<<c.z;
            throw std::runtime_error(detail.str());
        }
    };
    std::vector<std::pair<const Mesh3D*,MeshBounds3D>> sourceBounds;
    const auto mesh=[&](const std::string& group,std::size_t id,const MeshResource3D& resource,const Transform3D& pose){
        if(!resource||!resource.cpuMesh||!finite(pose.position)||!finite(pose.rotation)||!finite(pose.scale)||
           pose.scale.x<=0||pose.scale.y<=0||pose.scale.z<=0)fail(group,id,"MESH_OR_POSE");
        const auto& cpu=*resource.cpuMesh;
        auto found=std::find_if(sourceBounds.begin(),sourceBounds.end(),[&](const auto& entry){return entry.first==&cpu;});
        MeshBounds3D bounds;
        if(found==sourceBounds.end()){
            if(!cpu.Valid())fail(group,id,"TOPOLOGY");
            for(const auto& v:cpu.vertices)if(!finite({v.x,v.y,v.z}))fail(group,id,"SOURCE_VERTEX");
            bounds=cpu.Bounds();sourceBounds.push_back({&cpu,bounds});
        }else bounds=found->second;
        ++counts[0];
        const auto world=TransformBounds(bounds,pose);
        if(!finite(world.center)||!finite(world.extents))fail(group,id,"WORLD_BOUNDS");
        if(!overlaps(world.center-world.extents-windSweep,world.center+world.extents+windSweep))return;
        const auto matrix=pose.Matrix();
        for(std::size_t t=0;t<cpu.indices.size();t+=3){
            const auto point=[&](std::size_t corner){const auto& v=cpu.vertices[cpu.indices[t+corner]];return TransformPoint(matrix,{v.x,v.y,v.z});};
            triangle(group+"/"+std::to_string(id),t/3,point(0),point(1),point(2));
        }
    };
    const auto solid=[&](const std::string& group,std::size_t id,const BoxCollider3D& b){
        if(!finite(b.center)||!finite(b.extents)||b.extents.x<0||b.extents.y<0||b.extents.z<0)
            fail(group,id,"BOX_INVALID");
        ++counts[2];if(overlaps(b.center-b.extents,b.center+b.extents))fail(group,id,"BOX_PROJECTION_OVERLAP");
    };
    const auto visualBox=[&](const std::string& group,std::size_t id,Vec3 centre,Vec3 size){
        mesh(group,id,box,{centre,{},size});
    };
    for(bool repaired:{false,true}){
        const auto bodies=RuntimeWorldSolids(repaired);
        for(std::size_t i=0;i<bodies.size();++i)solid(repaired?"body-repaired":"body-original",i,bodies[i]);
    }
    const auto camera=RuntimeCameraOnly();
    for(std::size_t i=0;i<camera.size();++i)solid("camera",i,camera[i]);
    if(workshopRoof)for(std::size_t i=0;i<workshopRoof->members.size();++i){
        const auto& p=workshopRoof->members[i];
        solid("roof-convex-broadphase",i,{(p.minimum+p.maximum)*.5f,(p.maximum-p.minimum)*.5f});
    }
    const auto fir=[&](const std::string& group,std::size_t id,const Transform3D& pose,bool distant){
        for(unsigned slot=0;slot<4;++slot)mesh(group+"/slot"+std::to_string(slot),id,distant?landscapeFirMeshes[slot]:firMeshes[slot],pose);
        if(distant||nearFirTwigLod)mesh(group+"/near-twig",id,distant?landscapeDetailedFirTwig:detailedFirTwig,pose);
    };
    for(std::size_t i=0;i<firLayout.firs.size();++i)fir("mature-fir",i,firLayout.firs[i].render,false);
    for(std::size_t i=0;i<understory.saplings.size();++i)fir("sapling",i,understory.saplings[i],false);
    // 必須驗證實際繪製的替換群落，不能拿已移除的冷杉代替其空間證明。
    for(std::size_t i=0;i<landscapeTrees.size();++i){
        const auto selected=std::find_if(landscapePlanting.plants.begin(),landscapePlanting.plants.end(),
            [&](const auto& plant){return plant.sourceOrdinal==i;});
        if(mixedLandscapeCanopy&&selected!=landscapePlanting.plants.end()){
            const auto index=static_cast<std::size_t>(selected-landscapePlanting.plants.begin());
            for(unsigned slot=0;slot<3;++slot)mesh("mixed-broadleaf/slot"+std::to_string(slot),i,
                slot==2?landscapeRootMeshes[index]:broadleafMeshes[slot],selected->pose);
        }else if(landscapeCrownGrowth){
            for(unsigned slot=0;slot<4;++slot)mesh("grown-landscape-fir/slot"+std::to_string(slot),i,BaseLandscapeFir(slot),landscapeTrees[i]);
            mesh("grown-landscape-fir/near-twig",i,BaseLandscapeTwig(),landscapeTrees[i]);
        }else fir("landscape-fir",i,landscapeTrees[i],true);
    }
    for(std::size_t i=520;i<landscapeStandPlan.combined.size();++i)fir("stand-fir",i,landscapeStandPlan.combined[i],true);
    for(std::size_t i=0;i<broadleafTrees.size();++i)for(unsigned slot=0;slot<3;++slot)
        mesh("broadleaf/slot"+std::to_string(slot),i,broadleafMeshes[slot],broadleafTrees[i].transform);
    for(std::size_t i=0;i<middleLayer.trees.size();++i)for(unsigned slot=0;slot<3;++slot)
        mesh("middle/slot"+std::to_string(slot),i,broadleafMeshes[slot],middleLayer.trees[i].transform);
    for(std::size_t i=0;i<canopyPopulation.plants.size();++i)for(unsigned slot=0;slot<3;++slot)
        mesh("canopy/slot"+std::to_string(slot),i,slot?canopyMeshes[slot]:canopyRootMeshes[i],canopyPopulation.plants[i].pose);
    const auto shrub=[&](const std::string& group,const auto& plants){
        for(std::size_t i=0;i<plants.size();++i){const auto& p=plants[i];
            if(p.variant>=shrubMeshes.size())fail(group,i,"VARIANT");
            for(unsigned slot=0;slot<2;++slot)mesh(group+"/slot"+std::to_string(slot),i,shrubMeshes[p.variant][slot],p.pose);
        }
    };
    shrub("shrub",shrubCanopy.plants);shrub("entrance-understory",entranceUnderstory.plants);
    for(std::size_t i=0;i<understory.ferns.size();++i)mesh("fern-original",i,scannedFerns[i%4],understory.ferns[i]);
    for(std::size_t i=0;i<fernColonies.plants.size();++i){const auto& p=fernColonies.plants[i];
        if(p.variant>=scannedFerns.size())fail("fern-colony",i,"VARIANT");mesh("fern-colony",i,scannedFerns[p.variant],p.transform);
    }
    // 不依相機距離或目前 suppression 隱藏植物，包含完整保留作者群落。
    for(std::size_t i=0;i<groundCover.size();++i){
        const auto p=groundCover[i];const auto variant=forestCover.plants[i].variant;
        if(variant>=scannedGrass.size())fail("grass",i,"VARIANT");
        const float s=.5f+.25f*std::sin(p.x*1.7f+p.z);
        mesh("grass",i,scannedGrass[variant],{p,{0,p.x+p.z,0},{s,s,s}});
    }
    for(std::size_t i=0;i<roadsideCover.plants.size();++i){const auto& p=roadsideCover.plants[i];
        if(p.variant>=scannedGrass.size())fail("roadside-grass",i,"VARIANT");mesh("roadside-grass",i,scannedGrass[p.variant],p.transform);
    }
    for(std::size_t i=0;i<roadsideBroadleaf.plants.size();++i){const auto& p=roadsideBroadleaf.plants[i];
        if(p.variant>=roadsideBroadleafMeshes.size())fail("roadside-broadleaf",i,"VARIANT");mesh("roadside-broadleaf",i,roadsideBroadleafMeshes[p.variant],p.transform);
    }
    for(unsigned i=0;i<2;++i)mesh("trail-litter",i,trailLitterMeshes[i],{});
    for(std::size_t i=0;i<forestProps.size();++i){const auto& p=forestProps[i];
        mesh("forest-prop",i,p.FallenLog()?fallenLogs.mesh:propMeshes[0],p.transform);
        solid("forest-prop-collider",i,p.collider);
    }
    for(std::size_t i=0;i<shoulderGroundcoverMeshes.size();++i)
        mesh("shoulder-groundcover",i,shoulderGroundcoverMeshes[i],Mistwatch::ShoulderGroundcover::Recipes[i].pose);
    mesh("workshop",0,workshopMesh,{});
    if(shelterDetailEnabled){for(unsigned i=0;i<2;++i)mesh("shelter-detail",i,shelterDetailMeshes[i],{});}
    else mesh("shelter",0,plankedShelterMesh,{});
    mesh("ranger-hut",0,rangerHutMesh,{Mistwatch::Session::rangerLog,{},{1,1,1}});
    const auto desk=Mistwatch::RangerHutSolids()[2];
    mesh("ranger-desk",0,rangerDeskMesh,{desk.center,{},{1,1,1}});
    visualBox("ranger-log",0,Mistwatch::Session::rangerLog+Vec3{.9f,1.02f,0},{.55f,.03f,.65f});
    visualBox("beacon-route-sign",0,{2,.8f,79},{.12f,1.6f,.12f});
    visualBox("beacon-route-sign",1,{2,1.5f,79},{1,.35f,.12f});
    for(unsigned i=0;i<3;++i)for(unsigned slot=0;slot<2;++slot)
        mesh("supply/slot"+std::to_string(slot),i,supplyMeshes[slot],{Mistwatch::Session::supplies[i],{},{1,1,1}});
    for(unsigned i=0;i<2;++i)mesh("bridge-worksite",i,bridgeWorksiteMeshes[i],{Mistwatch::Session::bridge+Vec3{1,0,0},{},{1,1,1}});
    visualBox("bridge-repaired",0,{0,-.1f,36},{4,.2f,8});
    for(unsigned site=0;site<3;++site){const auto point=Mistwatch::Session::repairSites[site];
        if(site==0)for(unsigned slot=0;slot<2;++slot)mesh("repair-tools",slot,repairToolsMeshes[slot],{point+Mistwatch::RepairToolsOffset,{},{1,1,1}});
        else for(unsigned beam=0;beam<3;++beam)mesh("repair-beam",site*3+beam,propMeshes[1],{point+Vec3{0,.18f+.22f*float(beam),0},{0,0,1.5707963f},{.22f,1.8f,.22f}});
        for(unsigned slot=0;slot<repairSignMeshes.size();++slot)
            mesh("repair-sign/slot"+std::to_string(slot),site,repairSignMeshes[slot],{point+Vec3{site==0?2.8f:1.3f,0,0},{},{1,1,1}});
    }
    const auto beacon=Mistwatch::CreateBeaconDetail(session);
    if(!beacon)fail("beacon",0,"AUTHOR");
    for(std::size_t i=0;i<beacon->parts.size();++i)visualBox("beacon",i,beacon->parts[i].center,beacon->parts[i].size);
    const auto cabinet=Mistwatch::FuseCaseSolid();visualBox("fuse-cabinet",0,cabinet.center,cabinet.extents*2.f);
    const auto dials=Mistwatch::CreateFuseDialVisuals(session,cabinet.center);
    if(!dials)fail("fuse-dials",0,"AUTHOR");
    for(std::size_t i=0;i<dials->size();++i){const auto& d=(*dials)[i];
        visualBox("fuse-panel",i,d.centre,d.panelSize);
        for(std::size_t t=0;t<d.triangles.size();++t){const auto& p=d.triangles[t];triangle("fuse-triangle/"+std::to_string(i),t,p.a,p.b,p.c);}
        // 四向刻度、文字與指針均在面板的 YZ 足跡內；X 深度至 .025m。
        solid("fuse-all-directions-envelope",i,{d.centre+Vec3{.0125f,0,0},{.0275f,.105f,.105f}});
    }
    for(std::size_t i=0;i<generatorDetailMeshes.size();++i)
        mesh("generator",i,generatorDetailMeshes[i],{Mistwatch::Session::generator,{},{1,1,1}});
    for(unsigned lamp=0;lamp<3;++lamp){std::size_t t{};for(const auto& p:Mistwatch::GeneratorIndicatorLens(lamp))
        triangle("generator-lens/"+std::to_string(lamp),t++,Mistwatch::Session::generator+p[0],Mistwatch::Session::generator+p[1],Mistwatch::Session::generator+p[2]);}
    for(unsigned i=0;i<3;++i){const auto p=Mistwatch::Session::relaySites[i];const auto base=Mistwatch::RelaySolids(i);
        for(unsigned slot=0;slot<2;++slot)visualBox("relay-base/"+std::to_string(i),slot,base[slot].center,base[slot].extents*2.f);
        for(unsigned sector=0;sector<16;++sector){const float a=float(sector)*6.2831853f/16.f;const Vec3 d{std::sin(a),0,std::cos(a)};
            mesh("relay-dish/"+std::to_string(i),sector,relayDish,{p+Vec3{0,2.15f,0}+d*.5f,{0,a,0},{1,1,1}});
            mesh("relay-boom/"+std::to_string(i),sector,box,{p+Vec3{0,2.15f,0}+d*.15f,{0,a,0},{.065f,.065f,.3f}});
            const auto a0=p+Vec3{0,.74f,0},a1=p+Vec3{d.x,.74f,d.z};triangle("relay-line/"+std::to_string(i),sector,a0,a1,a1);
        }
        visualBox("relay-lamp",i,p+Vec3{i==1?-.69f:.69f,.7f,0},{.08f,.18f,.25f});
    }
    // 排除地形本身、水面與即時角色；前兩者由地形/河道證明，角色由局部操作測試驗收。
    return counts;
}
