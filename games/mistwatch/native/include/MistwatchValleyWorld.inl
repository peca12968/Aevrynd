    void InitializeValleyWorld(Engine& e,const std::function<void(const char*)>& stages={}){
        if(!valleyWorldEnabled)return;
        const auto stage=[&](const char* name){if(stages)stages(name);};
        if(valleyEvidenceDirectory.empty()||(!manualValleyCandidate&&inputReplayDirectory.empty())||!entranceWorld.applied||woodlandWorld||canopyPopulation.plants.size()!=8||
            coreHerbEnabled||routeGrassEnabled||cliffWorldEnabled||cliffReliefEnabled||shoulderGroveEnabled||
            groundMicrorelief||deciduousHabitat||!capture.empty())
            throw std::runtime_error("Valley requires isolated original-eight replay world");
        std::vector<BoxCollider3D> supportPads;
        std::vector<Mistwatch::ValleyTerrain::SupportSurface> supportSurfaces;
        if((valleyBakeEnabled||!valleyBakeExportPath.empty())&&ValleyStartupWitness()!=valleyBakeWitness)
            throw std::runtime_error("Startup bake actual input changed before support binding");
        const auto support=[&](const MeshResource3D& resource,const Transform3D& pose,const BoxCollider3D* collider){
            if(!resource||!resource.cpuMesh)throw std::runtime_error("Valley support actual mesh unavailable");
            const auto bounds=TransformBounds(resource.cpuMesh->Bounds(),pose);
            auto lo=bounds.center-bounds.extents,hi=bounds.center+bounds.extents;
            Mistwatch::Expand(lo,hi,pose.position);
            if(collider){Mistwatch::Expand(lo,hi,collider->center-collider->extents);Mistwatch::Expand(lo,hi,collider->center+collider->extents);}
            supportPads.push_back({(lo+hi)*.5f,(hi-lo)*.5f});
            if(valleyLoadedBake){const auto i=supportSurfaces.size();
                if(i>=valleyLoadedBake->pads.size()||!Mistwatch::FallenLogDetail::SameBox(supportPads.back(),valleyLoadedBake->pads[i]))
                    throw std::runtime_error("Startup bake actual support pad mismatch");
                supportSurfaces.push_back(valleyLoadedBake->surfaces[i]);
            }else supportSurfaces.push_back(Mistwatch::ValleyTerrain::MeasureSupport(*resource.cpuMesh,pose,terrain));
        };
        for(const auto& p:entranceWorld.props)support(p.FallenLog()?fallenLogs.mesh:propMeshes[0],p.transform,&p.collider);
        for(std::size_t i=0;i<canopyPopulation.plants.size();++i)
            support(canopyRootMeshes[i],canopyPopulation.plants[i].pose,&canopyPopulation.plants[i].lower);
        if(!valleyBakeExportPath.empty()){
            const Mistwatch::ValleyStartupBake::Data data{valleyPreparedWood,supportPads,supportSurfaces};
            const auto bytes=Mistwatch::ValleyStartupBake::Encode(valleyBakeWitness,data);
            // CREATE_NEW不覆寫既有檔案；輸出來自本次完整作者，不挪用舊證書當新產物。
            const HANDLE file=CreateFileW(valleyBakeExportPath.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Startup bake fresh export unavailable");
            DWORD written{};const bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file);
            CloseHandle(file);if(!ok)throw std::runtime_error("Startup bake export incomplete");
        }
        stage("valley_support_measurement");
        auto derived=Mistwatch::ValleyTerrain::Build(terrain,entranceWorld.mission,supportPads,supportSurfaces,valleyTerraceEnabled);
        const auto beforeJointBank=derived.terrain;
        if(valleyJointRockBank){
            valleyOutcrop.Initialize(e,derived.terrain,boulderMaterial,true,forestPlayableWorld);
            for(const auto& p:valleyOutcrop.poses){const auto b=TransformBounds(valleyOutcrop.mesh.cpuMesh->Bounds(),p);
                valleyRockSolids.push_back({b.center,b.extents});}
            const auto r=Mistwatch::JointRockBank::Apply(derived,supportPads);
            std::ofstream report(valleyEvidenceDirectory/"joint-rock-bank.txt");report.precision(9);
            report<<"profile=upstream-contact-soil-v1\nchanged="<<r.changed<<"\nmaximum_rise="<<r.maximumRise
                <<"\nsource_lower_samples="<<Mistwatch::JointRockBank::Seeds.size()
                <<"\nconservative_rock_boxes="<<valleyRockSolids.size()<<"\nsource_mesh_deformed=0\nfull_contact_qualified=0\n";
            report.close();if(!report)throw std::runtime_error("Joint bank receipt failed");
        }
        stage("valley_height_solve");
        auto author=entranceWorld;auto canopy=canopyPopulation;
        auto cover=forestCover;auto colonies=fernColonies;auto shrubs=shrubCanopy;
        auto roadside=roadsideCover;auto broadRoadside=roadsideBroadleaf;auto entry=entranceUnderstory;
        auto litter=trailLitter;
        std::ostringstream anchors;anchors.precision(9);anchors<<"x\tbefore_y\tz\tdelta_y\tafter_y\n";
        std::size_t movedRoots{};
        const auto shift=[&](Vec3& p){const float dy=Mistwatch::ValleyTerrain::Delta(terrain,derived.terrain,p);
            anchors<<p.x<<'\t'<<p.y<<'\t'<<p.z<<'\t'<<dy<<'\t'<<(p.y+dy)<<'\n';
            if(dy!=0){p.y+=dy;++movedRoots;}return dy;};
        const auto pose=[&](Transform3D& p){return shift(p.position);};
        for(auto& f:author.firs.firs){const float dy=pose(f.render);f.root.y+=dy;f.collider.center.y+=dy;}
        author.firs.roots.clear();for(const auto& f:author.firs.firs)author.firs.roots.push_back(f.root);
        for(auto& p:author.firs.patchCentres)shift(p);
        for(std::size_t i=0;i<author.props.size();++i){auto& p=author.props[i];const float dy=derived.supportShifts[i];
            anchors<<p.transform.position.x<<'\t'<<p.transform.position.y<<'\t'<<p.transform.position.z<<'\t'<<dy<<'\t'<<p.transform.position.y+dy<<'\n';
            p.transform.position.y+=dy;p.collider.center.y+=dy;movedRoots+=dy!=0;}
        for(auto& p:author.understory.ferns)pose(p);
        for(auto& p:author.understory.saplings)pose(p);
        for(auto& p:author.broadleaf){const float dy=pose(p.transform);p.trunk.center.y+=dy;}
        for(auto& p:author.middle.trees){const float dy=pose(p.transform);p.trunk.center.y+=dy;}
        for(auto& p:cover.plants)pose(p.transform);
        for(auto& p:cover.patches)shift(p.centre);
        for(auto& p:colonies.plants)pose(p.transform);
        for(auto& p:colonies.colonies)shift(p.centre);
        for(auto& p:shrubs.plants)pose(p.pose);
        for(auto& p:roadside.plants)pose(p.transform);
        for(auto& p:broadRoadside.plants)pose(p.transform);
        for(auto& p:entry.plants)pose(p.pose);
        if(valleyJointRockBank){
            std::ofstream treeLog(valleyEvidenceDirectory/"joint-bank-trees.tsv");treeLog.precision(9);
            treeLog<<"group\tid\tbefore_replant_x\tbefore_replant_y\tbefore_replant_z\tafter_x\tafter_y\tafter_z\n";
            const auto rockFits=[&](Vec3 p,float radius){
                for(const auto& b:valleyRockSolids)if(std::abs(p.x-b.center.x)<b.extents.x+radius&&
                    std::abs(p.z-b.center.z)<b.extents.z+radius)return false;
                return true;
            };
            const auto changed=[&](Vec3 p){return Mistwatch::ValleyTerrain::Delta(beforeJointBank,derived.terrain,p)!=0||!rockFits(p,1.4f);};
            const auto firFits=[&](std::size_t id,Vec3 p){const auto& f=author.firs.firs[id];const float radius=f.collider.extents.x;
                if(!rockFits(p,radius))return false;
                if(!derived.terrain.Contains(p.x-radius,p.z-radius)||!derived.terrain.Contains(p.x+radius,p.z+radius)||
                    derived.terrain.SampleNormal(p.x,p.z).y<.78f||!Mistwatch::ClearsForestWoodWorld(p,radius))return false;
                for(float dx:{-radius,radius})for(float dz:{-radius,radius})
                    if(std::abs(derived.terrain.SampleHeight(p.x+dx,p.z+dz)-p.y)>.12f)return false;
                for(std::size_t i=0;i<author.firs.firs.size();++i)if(i!=id){const auto& other=author.firs.firs[i];
                    if(std::hypot(p.x-other.root.x,p.z-other.root.z)<(f.patch==other.patch?2.35f:4.8f))return false;}
                for(const auto& other:author.broadleaf)if(std::hypot(p.x-other.transform.position.x,p.z-other.transform.position.z)<3.f)return false;
                for(const auto& other:author.middle.trees)if(std::hypot(p.x-other.transform.position.x,p.z-other.transform.position.z)<3.f)return false;
                return true;
            };
            // 同一有界作者網格按距離排序，只重排新岩面影響到的根；不是高度／深度參數掃描。
            for(std::size_t id=0;id<author.firs.firs.size();++id){auto& f=author.firs.firs[id];
                if(!changed(f.root))continue;const auto before=f.root;
                if(!firFits(id,f.root)){
                    std::vector<Vec3> candidates;
                    for(float z=52.5f;z<=85.f;z+=2.5f)for(float x=-51;x<=-18.5f;x+=2.5f)
                        candidates.push_back({x,derived.terrain.SampleHeight(x,z),z});
                    std::stable_sort(candidates.begin(),candidates.end(),[&](Vec3 a,Vec3 b){
                        return std::hypot(a.x-before.x,a.z-before.z)<std::hypot(b.x-before.x,b.z-before.z);});
                    const auto it=std::find_if(candidates.begin(),candidates.end(),[&](Vec3 p){return firFits(id,p);});
                    if(it==candidates.end())throw std::runtime_error("Joint bank complete fir placement unavailable id="+std::to_string(id));
                    const auto delta=*it-f.root;f.root=*it;f.render.position=f.render.position+delta;f.collider.center=f.collider.center+delta;
                }
                treeLog<<"fir\t"<<id<<'\t'<<before.x<<'\t'<<before.y<<'\t'<<before.z<<'\t'<<f.root.x<<'\t'<<f.root.y<<'\t'<<f.root.z<<'\n';
            }
            author.firs.roots.clear();for(const auto& f:author.firs.firs)author.firs.roots.push_back(f.root);
            author.firs.fingerprint=Mistwatch::EntranceWorldDetail::FirFingerprint(author.firs);
            std::ofstream firLog(valleyEvidenceDirectory/"joint-bank-final-firs.tsv");firLog.precision(9);
            firLog<<"id\tpatch\tx\ty\tz\tpose_x\tpose_y\tpose_z\tcx\tcy\tcz\tex\tey\tez\n";
            for(std::size_t id=0;id<author.firs.firs.size();++id){const auto& f=author.firs.firs[id];
                firLog<<id<<'\t'<<f.patch<<'\t'<<f.root.x<<'\t'<<f.root.y<<'\t'<<f.root.z
                    <<'\t'<<f.render.position.x<<'\t'<<f.render.position.y<<'\t'<<f.render.position.z
                    <<'\t'<<f.collider.center.x<<'\t'<<f.collider.center.y<<'\t'<<f.collider.center.z
                    <<'\t'<<f.collider.extents.x<<'\t'<<f.collider.extents.y<<'\t'<<f.collider.extents.z<<'\n';}
            firLog.close();if(!firLog)throw std::runtime_error("Joint bank actual fir readback failed");
            for(std::size_t i=0;i<author.firs.patchCentres.size();++i){Vec3 sum{};unsigned count{};
                for(const auto& f:author.firs.firs)if(f.patch==i){sum=sum+f.root;++count;}
                if(count)author.firs.patchCentres[i]=sum*(1.f/float(count));}
            const auto replantTree=[&](Mistwatch::BroadleafTree& tree,const char* group,std::size_t id){
                if(!changed(tree.transform.position))return;
                const auto originalRoot=tree.transform.position;
                const auto record=[&](){const auto p=tree.transform.position;
                    treeLog<<group<<'\t'<<id<<'\t'<<originalRoot.x<<'\t'<<originalRoot.y<<'\t'<<originalRoot.z
                        <<'\t'<<p.x<<'\t'<<p.y<<'\t'<<p.z<<'\n';};
                const float radius=(std::max)(tree.trunk.extents.x,tree.trunk.extents.z);
                const auto fits=[&](Vec3 p){
                    if(!rockFits(p,radius)||!Mistwatch::ClearsForestWoodWorld(p,radius)||
                        derived.terrain.SampleNormal(p.x,p.z).y<.97f)return false;
                    for(float dx:{-1.f,1.f})for(float dz:{-1.f,1.f})
                        if(std::abs(derived.terrain.SampleHeight(p.x+dx*tree.trunk.extents.x,p.z+dz*tree.trunk.extents.z)-p.y)>.1f)return false;
                    for(const auto& f:author.firs.firs)if(std::hypot(p.x-f.root.x,p.z-f.root.z)<3.f)return false;
                    for(const auto& other:author.broadleaf)if(&other!=&tree&&std::hypot(p.x-other.transform.position.x,p.z-other.transform.position.z)<3.f)return false;
                    for(const auto& other:author.middle.trees)if(&other!=&tree&&std::hypot(p.x-other.transform.position.x,p.z-other.transform.position.z)<3.f)return false;
                    return true;
                };
                if(fits(tree.transform.position)){record();return;}
                std::vector<Vec3> candidates;const auto before=tree.transform.position;
                // 闊葉樹放回相鄰低緩河谷平台，冷杉仍在西側林帶；原平整度與間距門不變。
                for(float z=52.5f;z<=85.f;z+=2.5f)for(float x=-18.5f;x<=-4.f;x+=2.5f)candidates.push_back({x,derived.terrain.SampleHeight(x,z),z});
                std::stable_sort(candidates.begin(),candidates.end(),[&](Vec3 a,Vec3 b){
                    return std::hypot(a.x-before.x,a.z-before.z)<std::hypot(b.x-before.x,b.z-before.z);});
                const auto it=std::find_if(candidates.begin(),candidates.end(),fits);
                if(it==candidates.end())throw std::runtime_error("Joint bank complete broadleaf placement unavailable");
                const auto delta=*it-before;tree.transform.position=*it;tree.trunk.center=tree.trunk.center+delta;
                record();
            };
            for(std::size_t i=0;i<author.broadleaf.size();++i)replantTree(author.broadleaf[i],"broadleaf",i);
            for(std::size_t i=0;i<author.middle.trees.size();++i)replantTree(author.middle.trees[i],"middle",i);
            treeLog.close();if(!treeLog)throw std::runtime_error("Joint bank tree receipt failed");
        }
        litter.branches=Mistwatch::ValleyTerrain::Warp(litter.branches,terrain,derived.terrain,false);
        litter.stones=Mistwatch::ValleyTerrain::Warp(litter.stones,terrain,derived.terrain,false);
        for(auto& p:litter.patches)shift(p.centre);
        std::vector<BoxCollider3D> wood;
        std::size_t originalWoodOffset{};
        const auto samePreparedMesh=[](const Mesh3D& a,const Mesh3D& b){
            return a.indices==b.indices&&a.vertices.size()==b.vertices.size()&&!a.vertices.empty()&&
                std::memcmp(a.vertices.data(),b.vertices.data(),a.vertices.size()*sizeof(Vertex3D))==0;
        };
        if(valleyPreparedCanopy.plants.size()!=canopy.plants.size()||valleyPreparedWood.size()!=canopy.plants.size()||
            !samePreparedMesh(valleyPreparedBranches,canopySource[2]))throw std::runtime_error("Valley prepared branch input changed");
        for(std::size_t i=0;i<canopy.plants.size();++i){auto& p=canopy.plants[i];const auto& original=canopyPopulation.plants[i];
            const auto& prepared=valleyPreparedCanopy.plants[i];
            const std::array<float,9> actualPose{original.pose.position.x,original.pose.position.y,original.pose.position.z,
                original.pose.rotation.x,original.pose.rotation.y,original.pose.rotation.z,original.pose.scale.x,original.pose.scale.y,original.pose.scale.z};
            const std::array<float,9> preparedPose{prepared.pose.position.x,prepared.pose.position.y,prepared.pose.position.z,
                prepared.pose.rotation.x,prepared.pose.rotation.y,prepared.pose.rotation.z,prepared.pose.scale.x,prepared.pose.scale.y,prepared.pose.scale.z};
            for(std::size_t j=0;j<actualPose.size();++j)if(std::bit_cast<std::uint32_t>(actualPose[j])!=std::bit_cast<std::uint32_t>(preparedPose[j]))
                throw std::runtime_error("Valley prepared pose input changed");
            if(!samePreparedMesh(prepared.rootedTrunk,original.rootedTrunk))throw std::runtime_error("Valley prepared trunk input changed");
            // 初次建構已計算完整真木體；輸入逐位核對後重用同一份結果，仍逐零件核對真實世界。
            auto proxy=valleyPreparedWood[i];
            if(originalWoodOffset+proxy.parts.size()>canopySolids.size())throw std::runtime_error("Valley original wood owner bounds");
            for(std::size_t j=0;j<proxy.parts.size();++j)
                if(!Mistwatch::FallenLogDetail::SameBox(proxy.parts[j],canopySolids[originalWoodOffset+j]))throw std::runtime_error("Valley original wood owner identity");
            originalWoodOffset+=proxy.parts.size();
            const float dy=derived.supportShifts[author.props.size()+i];
            anchors<<p.pose.position.x<<'\t'<<p.pose.position.y<<'\t'<<p.pose.position.z<<'\t'<<dy<<'\t'<<p.pose.position.y+dy<<'\n';
            p.pose.position.y+=dy;p.lower.center.y+=dy;movedRoots+=dy!=0;
            // 既有根幹完整網格和真正wood分解一起剛體平移；不重新量化體素來變造零件數。
            for(auto& part:proxy.parts)part.center.y+=dy;
            if(wood.size()+proxy.parts.size()>9000)throw std::runtime_error("Valley wood budget exceeded");
            wood.insert(wood.end(),proxy.parts.begin(),proxy.parts.end());}
        if(originalWoodOffset!=canopySolids.size())throw std::runtime_error("Valley original wood owner tail");
        std::ofstream preparedLog(valleyEvidenceDirectory/"valley-canopy-preparation.txt");
        preparedLog<<"source_groups="<<valleyPreparedWood.size()<<"\nreused_parts="<<originalWoodOffset
            <<"\nexact_mesh_pose_inputs_checked=1\nactual_world_parts_checked=1\nregenerated_proxies=0\n";
        preparedLog.close();if(!preparedLog)throw std::runtime_error("Valley prepared canopy receipt failed");
        valleyPreparedCanopy={};valleyPreparedWood.clear();valleyPreparedBranches={};
        stage("valley_root_and_wood_binding");
        auto vegetation=author.WorldSolids(false);
        vegetation.insert(vegetation.end(),valleyRockSolids.begin(),valleyRockSolids.end());
        for(const auto& p:author.middle.trees)vegetation.push_back(p.trunk);
        auto complete=vegetation;complete.insert(complete.end(),wood.begin(),wood.end());
        auto extensions=valleyRockSolids;
        auto plantingWood=wood;
        if(valleySlopeGrove){
            slopeGrove.Plan(derived.terrain,canopySource,complete);
            extensions.insert(extensions.end(),slopeGrove.solids.begin(),slopeGrove.solids.end());
            vegetation.insert(vegetation.end(),slopeGrove.rootMasks.begin(),slopeGrove.rootMasks.end());
            complete.insert(complete.end(),slopeGrove.solids.begin(),slopeGrove.solids.end());
            plantingWood.insert(plantingWood.end(),slopeGrove.solids.begin(),slopeGrove.solids.end());
        }
        // 下列只修正原完整腳印門不成立的植栽；通過的株數、變體及位置保持。
        std::vector<Mistwatch::ValleyPlacement::Plant> placements;
        std::vector<float> originalFernRadii;originalFernRadii.reserve(author.understory.ferns.size());
        for(std::size_t i=0;i<author.understory.ferns.size();++i){auto& p=author.understory.ferns[i];const auto& a=fernFootprints[i%4];
            originalFernRadii.push_back(a.radius*p.scale.x);
            placements.push_back({"original-fern",i,&p,{a.radius,a.minimumY,a.height},&originalFernRadii.back(),false,true});}
        std::ostringstream placementLog;
        Mistwatch::ValleyPlacement::Receipt placementReceipt;
        try{placementReceipt=Mistwatch::ValleyPlacement::Repair(placements,terrain,derived.terrain,vegetation,complete,placementLog);}
        catch(...){std::ofstream failure(valleyEvidenceDirectory/"valley-placement-failed.tsv");failure<<placementLog.str();throw;}
        stage("valley_legacy_fern_repair");
        // 原草patch、混合蕨grassPatch與灌木／接替ID互相依賴，整層重新作者，不能逐株挪到另一patch冒充原配置。
        auto leaves=LoadAevryndMeshFile(e.ExecutableDirectory()/"assets"/"mistwatch-broadleaf"/"meshes"/Mistwatch::BroadleafMeshName(1));
        if(!leaves)throw std::runtime_error("Valley actual shrub leaf source unavailable");
        // 先放大型灌木，再讓軟地被填空；空的規劃層不是拿舊的失效族群冒充新世界。
        const Mistwatch::ForestCoverResult emptyGrass;
        const Mistwatch::FernColonyResult emptyColonies;
        const auto prefix=Mistwatch::CreateShrubCanopy(leaves.mesh,derived.terrain,vegetation,author.understory,emptyGrass,emptyColonies,
            fernFootprints,64,plantingWood,true);
        shrubs=Mistwatch::CreateShrubArcDensity(leaves.mesh,derived.terrain,vegetation,author.understory,emptyGrass,emptyColonies,
            fernFootprints,128,plantingWood,true);
        const auto composition=Mistwatch::ShrubCompositionContract::PreservedPrefix128(prefix,shrubs);
        for(unsigned i=0;i<4;++i)if(!Mistwatch::ShrubCompositionDetail::Same(shrubs.variants[i],shrubCanopy.variants[i]))
            throw std::runtime_error("Valley regenerated shrub mesh differs from actual shared GPU mesh");
        auto after=author.WorldSolids(true);for(const auto& p:author.middle.trees)after.push_back(p.trunk);
        after.insert(after.end(),valleyRockSolids.begin(),valleyRockSolids.end());
        after.insert(after.end(),wood.begin(),wood.end());
        after.insert(after.end(),slopeGrove.solids.begin(),slopeGrove.solids.end());
        auto roadsideBefore=complete,roadsideAfter=after;
        if(valleySlopeGrove){
            const auto separateGrove=[&](std::vector<BoxCollider3D>& list){
                if(slopeGrove.solids.size()!=512||list.size()<512)throw std::runtime_error("Slope grove extension range");
                const auto offset=list.size()-512;
                for(unsigned i=0;i<512;++i)if(!Mistwatch::FallenLogDetail::SameBox(list[offset+i],slopeGrove.solids[i]))
                    throw std::runtime_error("Slope grove exact extension membership");
                list.resize(offset);
            };
            separateGrove(roadsideBefore);separateGrove(roadsideAfter);
        }
        if(valleyJointRockBank){
            // 舊8853/8854世界門保持原樣；新三岩體是明確擴充，稍後對實際最終植株補做同一有限體積門。
            const auto separate=[&](std::vector<BoxCollider3D>& list,std::size_t offset){
                if(valleyRockSolids.size()!=3||offset+3>list.size())throw std::runtime_error("Joint bank extension range");
                for(unsigned i=0;i<3;++i)if(!Mistwatch::FallenLogDetail::SameBox(list[offset+i],valleyRockSolids[i]))
                    throw std::runtime_error("Joint bank extension exact membership");
                list.erase(list.begin()+offset,list.begin()+offset+3);
            };
            separate(roadsideBefore,author.WorldSolids(false).size());
            separate(roadsideAfter,author.WorldSolids(true).size()+author.middle.trees.size());
        }
        if(roadsideBroadleafEnabled)broadRoadside=Mistwatch::CreateRoadsideBroadleaf(derived.terrain,emptyGrass,author.understory,emptyColonies,
            shrubs,grassFootprints,fernFootprints,roadsideBroadleafBounds,roadsideBefore,roadsideAfter,composition,nullptr,Mistwatch::RoadsideInputPhase::PlanBeforeSoftLayers);
        std::vector<Mistwatch::ForestCoverReservation> groundReservations;
        for(const auto& p:shrubs.plants)groundReservations.push_back({p.pose.position,p.radius});
        for(const auto& p:broadRoadside.plants)groundReservations.push_back({p.transform.position,p.radius,Mistwatch::ForestCoverReservationKind::WindMixedRoadside});
        stage("valley_shrub_and_road_plan");
        cover=Mistwatch::CreateForestCover(derived.terrain,author.firs.roots,author.understory,vegetation,
            grassFootprints,forestCoverSettings,plantingWood,true,Mistwatch::ActiveEntryLayout,groundReservations);
        if(!cover.reachedBudget||cover.plants.size()!=3600)throw std::runtime_error("Valley complete grass author incomplete actual="+
            std::to_string(cover.plants.size())+" patches="+std::to_string(cover.patches.size())+" clearance="+std::to_string(cover.rejectedClearance)+
            " terrain="+std::to_string(cover.rejectedTerrain)+" spacing="+std::to_string(cover.rejectedSpacing));
        colonies=Mistwatch::CreateFernColonies(derived.terrain,cover,author.understory.ferns,vegetation,
            fernFootprints,fernColonySettings,plantingWood,true,nullptr,Mistwatch::ActiveEntryLayout,groundReservations);
        if(!colonies.reachedBudget||colonies.plants.size()!=300)throw std::runtime_error("Valley complete fern colony author incomplete actual="+std::to_string(colonies.plants.size()));
        stage("valley_ground_cover_rebuild");
        // 規劃後用真實最終族群重驗原混合間距，保留原.03公尺門；預留區不加入遊戲碰撞。
        for(const auto& s:shrubs.plants){
            for(const auto& p:cover.plants)if(std::hypot(s.pose.position.x-p.transform.position.x,s.pose.position.z-p.transform.position.z)<s.radius+p.radius+.03f)
                throw std::runtime_error("Valley final shrub grass spacing");
            for(const auto& p:colonies.plants)if(std::hypot(s.pose.position.x-p.transform.position.x,s.pose.position.z-p.transform.position.z)<s.radius+p.radius+.03f)
                throw std::runtime_error("Valley final shrub colony spacing");
        }
        // 預留不是合格證；軟地被完成後重驗真實完整族群與原混合半徑規則。
        Mistwatch::ValidateRoadsideCoverInputs(cover,author.understory,colonies,shrubs,grassFootprints,fernFootprints,roadsideBefore,roadsideAfter,composition);
        for(const auto& s:broadRoadside.plants){
            for(const auto& p:cover.plants)if(std::hypot(s.transform.position.x-p.transform.position.x,s.transform.position.z-p.transform.position.z)<.65f*(s.radius+p.radius+.19f))
                throw std::runtime_error("Valley final broadleaf grass spacing");
            for(const auto& p:colonies.plants)if(std::hypot(s.transform.position.x-p.transform.position.x,s.transform.position.z-p.transform.position.z)<.65f*(s.radius+p.radius+.19f))
                throw std::runtime_error("Valley final broadleaf colony spacing");
        }
        if(roadsideCoverEnabled)roadside=Mistwatch::CreateRoadsideCover(derived.terrain,cover,author.understory,colonies,
            shrubs,grassFootprints,fernFootprints,roadsideBefore,roadsideAfter,composition);
        if(entranceUnderstoryEnabled)entry=Mistwatch::CreateEntranceUnderstory(derived.terrain,cover,author.understory,colonies,
            shrubs,grassFootprints,fernFootprints,broadRoadside,roadsideBefore,roadsideAfter,nullptr,composition);
        if(valleyJointRockBank){
            std::size_t checked{};
            const auto extension=[&](const Transform3D& p,const auto& a){
                const float scale=p.scale.x;
                const Mistwatch::CanopyVolume::PlantVolume volume{p.position,
                    Mistwatch::CanopyVolume::ConservativeLocalVolume(a.radius+.19f/scale,a.minimumY,a.maximumY+.011f/scale),scale,p.rotation};
                if(!Mistwatch::CanopyVolume::ClearsCanopyWood(volume,extensions))throw std::runtime_error("Joint bank final plant extension contact");
                ++checked;
            };
            for(const auto& p:roadside.plants)extension(p.transform,grassFootprints[p.variant]);
            for(const auto& p:broadRoadside.plants)extension(p.transform,roadsideBroadleafBounds[p.variant]);
            for(const auto& p:entry.plants)extension(p.pose,shrubs.variants[p.variant]);
            std::ofstream proof(valleyEvidenceDirectory/"joint-bank-roadside-extension.txt");
            proof<<"baseline_before="<<roadsideBefore.size()<<"\nbaseline_after="<<roadsideAfter.size()
                <<"\nactual_before="<<complete.size()<<"\nactual_after="<<after.size()
                <<"\nexact_extension_boxes="<<extensions.size()<<"\nfinal_actual_volumes_checked="<<checked
                <<"\noriginal_8853_8854_gate_unchanged=1\noriginal_finite_volume_and_margin_unchanged=1\n";
            proof.close();if(!proof)throw std::runtime_error("Joint bank extension receipt failed");
        }
        std::size_t fullFootprints{};
        const auto footprint=[&](Vec3 root,float radius){
            if(!Mistwatch::ForestCoverFootprint(derived.terrain,root,radius,{}))throw std::runtime_error("Valley final authored footprint rejected");
            ++fullFootprints;
        };
        for(const auto& p:cover.plants)footprint(p.transform.position,p.radius);
        for(const auto& p:colonies.plants)footprint(p.transform.position,p.radius);
        for(const auto& p:shrubs.plants)footprint(p.pose.position,p.radius);
        for(const auto& p:roadside.plants)footprint(p.transform.position,p.radius);
        for(const auto& p:broadRoadside.plants){auto root=p.transform.position;
            root.y+=roadsideBroadleafBounds[p.variant].minimumY*p.transform.scale.x;footprint(root,p.radius);}
        for(const auto& p:entry.plants){auto root=p.pose.position;
            root.y+=shrubs.variants[p.variant].minimumY*p.pose.scale.x;footprint(root,p.radius);}
        stage("valley_final_footprint_checks");
        // 原世界收據識別原輸入；新世界的地形、全根部及實際碰撞另寫收據，不繼承合格證。
        std::vector<std::pair<MeshResource3D*,MeshResource3D>> uploads;
        const auto upload=[&](MeshResource3D& old,const Mesh3D& mesh){
            if(!old)throw std::runtime_error("Valley missing old GPU resource");
            auto fresh=e.CreateMeshResource3D(mesh);
            if(!fresh)throw std::runtime_error("Valley upload failed");
            if(!fresh.cpuMesh||fresh.cpuMesh->indices!=mesh.indices||fresh.cpuMesh->vertices.size()!=mesh.vertices.size()||
                std::memcmp(fresh.cpuMesh->vertices.data(),mesh.vertices.data(),mesh.vertices.size()*sizeof(Vertex3D))!=0){
                e.DestroyMesh3D(fresh.handle);throw std::runtime_error("Valley actual GPU mismatch");}
            uploads.emplace_back(&old,fresh);
        };
        std::array<Mistwatch::CliffCoverage::Receipt,3> rockCoverageReceipts{};
        const auto rockCoverage=[&](Mesh3D mesh,Mistwatch::CliffCoverage::Domain domain,unsigned slot){
            if(!valleyMaterialAudit||!cliffMaterialEnabled)return mesh;
            auto changed=Mistwatch::CliffCoverage::Create(mesh,derived.terrain,domain);
            const auto& r=changed.receipt;
            if(!r.changed||r.beforeGeometryFingerprint!=r.afterGeometryFingerprint||
                mesh.indices!=changed.mesh.indices||mesh.vertices.size()!=changed.mesh.vertices.size())
                throw std::runtime_error("Valley rock coverage must change material only");
            for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& a=mesh.vertices[i];const auto& b=changed.mesh.vertices[i];
                if(a.x!=b.x||a.y!=b.y||a.z!=b.z||a.normal!=b.normal||a.color!=b.color)
                    throw std::runtime_error("Valley rock coverage changed non-UV attributes");}
            rockCoverageReceipts.at(slot)=r;return std::move(changed.mesh);
        };
        std::optional<Mistwatch::NorthWatershed::Result> north;
        if(valleyNorthWatershed){
            north=Mistwatch::NorthWatershed::Create(*landscapeMesh.cpuMesh,terrain,landscapeTrees,landscapeStandPlan,landscapeStandBounds);
            stage("north_watershed_actual616_authoring");
        }
        try{
            upload(terrainMesh,rockCoverage(Mistwatch::ValleyTerrain::Warp(*terrainMesh.cpuMesh,terrain,derived.terrain,true),Mistwatch::CliffCoverage::Domain::Core,0));
            for(unsigned i=0;i<entranceRoadMeshes.size();++i){auto& mesh=entranceRoadMeshes[i];if(!mesh)continue;
                auto warped=Mistwatch::ValleyTerrain::Warp(*mesh.cpuMesh,terrain,derived.terrain,true);
                if(i==0)warped=rockCoverage(std::move(warped),Mistwatch::CliffCoverage::Domain::Core,1);
                if(i==0&&groveFloorLitterEnabled){
                    if(!valleySlopeGrove||slopeGrove.trees.size()!=8||!entranceRoadMeshes[1].cpuMesh||!entranceRoadMeshes[2].cpuMesh)
                        throw std::runtime_error("Grove floor requires actual complete grove and protected road domains");
                    std::array<Vec3,8> actualRoots{};
                    for(std::size_t tree=0;tree<actualRoots.size();++tree)actualRoots[tree]=slopeGrove.trees[tree].pose.position;
                    auto floor=Mistwatch::GroveFloor::Apply(warped,actualRoots,
                        entranceRoadMeshes[1].cpuMesh->indices,entranceRoadMeshes[2].cpuMesh->indices);
                    if(!floor.receipt.changed)throw std::runtime_error("Grove floor actual leaf coverage unchanged");
                    const auto dump=[&](const Mesh3D& source,const char* name){
                        const auto path=valleyEvidenceDirectory/name;
                        if(std::filesystem::exists(path))throw std::runtime_error("Grove floor export refuses overwrite");
                        const std::array<std::uint32_t,4> header{0x334d4541u,1u,
                            static_cast<std::uint32_t>(source.vertices.size()),static_cast<std::uint32_t>(source.indices.size())};
                        std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(header.data()),sizeof(header));
                        out.write(reinterpret_cast<const char*>(source.vertices.data()),std::streamsize(source.vertices.size()*sizeof(Vertex3D)));
                        out.write(reinterpret_cast<const char*>(source.indices.data()),std::streamsize(source.indices.size()*sizeof(std::uint32_t)));
                        out.close();if(!out)throw std::runtime_error("Grove floor complete export failed");
                    };
                    dump(warped,"grove-floor-before.aemesh");dump(floor.mesh,"grove-floor-after.aemesh");
                    std::ofstream receipt(valleyEvidenceDirectory/"grove-floor.txt");receipt.precision(9);
                    receipt<<"profile=rooted-grove-litter-v1\nactual_roots="<<actualRoots.size()<<"\nreferenced="<<floor.receipt.referenced
                        <<"\nprotected="<<floor.receipt.protectedVertices<<"\nchanged="<<floor.receipt.changed
                        <<"\nmaximum_leaf_increase="<<floor.receipt.maximumIncrease<<"\nart_accepted=0\n";
                    receipt.close();if(!receipt)throw std::runtime_error("Grove floor receipt failed");
                    warped=std::move(floor.mesh);
                }
                upload(mesh,std::move(warped));}
            if(north||valleyMaterialAudit&&cliffMaterialEnabled){
                const auto& source=north?north->mesh:*landscapeMesh.cpuMesh;
                upload(landscapeMesh,rockCoverage(source,Mistwatch::CliffCoverage::Domain::Ring,2));
            }
            // 根幹local geometry未改，保留原GPU資源；真正的世界pose與碰撞一起升降。
            if(trailLitterEnabled){upload(trailLitterMeshes[0],litter.branches);upload(trailLitterMeshes[1],litter.stones);}
        }catch(...){for(auto& p:uploads)e.DestroyMesh3D(p.second.handle);throw;}
        for(auto& p:uploads){e.DestroyMesh3D(p.first->handle);*p.first=p.second;}
        if(valleySlopeGrove)slopeGrove.Upload(e,canopyMeshes,canopySurfaces,forestPlayableWorld);
        if(north){
            std::ofstream roots(valleyEvidenceDirectory/"north-watershed-trees.tsv");roots.precision(9);
            roots<<"id\tbefore_x\tbefore_y\tbefore_z\tafter_x\tafter_y\tafter_z\n";
            for(std::size_t i=0;i<north->plan.combined.size();++i){
                const auto a=landscapeStandPlan.combined[i].position,b=north->plan.combined[i].position;
                roots<<i<<'\t'<<a.x<<'\t'<<a.y<<'\t'<<a.z<<'\t'<<b.x<<'\t'<<b.y<<'\t'<<b.z<<'\n';
            }
            roots.close();if(!roots)throw std::runtime_error("North watershed actual tree receipt failed");
            landscapeTrees=std::move(north->base);landscapeStandPlan=std::move(north->plan);
            std::ofstream report(valleyEvidenceDirectory/"north-watershed.txt");report.precision(9);
            report<<"profile=authored-north-v2\nchanged_vertices="<<north->vertices<<"\nmaximum_height_change="<<north->maximumChange
                <<"\nbase_trees="<<landscapeTrees.size()<<"\nadded_trees="<<landscapeStandPlan.addedBands.size()
                <<"\ninspected_trees="<<north->inspected<<"\nshifted_trees="<<north->shifted<<"\nrelocated_trees="<<north->relocated
                <<"\nmaximum_root_center_error="<<north->maximumRootError
                <<"\ncore_geometry_changed=0\nall_tree_shapes_preserved=1\nfull_finite_tree_contact_qualified=0\nart_accepted=0\n";
            report.close();if(!report)throw std::runtime_error("North watershed report failed");
        }
        if(valleyMaterialAudit){
            std::ofstream out(valleyEvidenceDirectory/"valley-rock-coverage.txt");
            out<<"schema=valley-rock-coverage-v1\nenabled="<<cliffMaterialEnabled<<"\nnon_uv_attributes_unchanged=1\n";
            const std::array<const char*,3> names{"core","forest","ring"};
            for(unsigned i=0;i<3;++i){const auto& r=rockCoverageReceipts[i];
                out<<names[i]<<"_changed="<<r.changed<<'\n'<<names[i]<<"_protected="<<r.protectedVertices<<'\n'
                    <<names[i]<<"_before_geometry="<<r.beforeGeometryFingerprint<<'\n'<<names[i]<<"_after_geometry="<<r.afterGeometryFingerprint<<'\n';}
            out.close();if(!out)throw std::runtime_error("Valley rock coverage evidence write failed");
        }
        entranceWorld=std::move(author);firLayout=entranceWorld.firs;trees=firLayout.roots;
        forestProps=entranceWorld.props;understory=entranceWorld.understory;broadleafTrees=entranceWorld.broadleaf;
        middleLayer=entranceWorld.middle;middleLayerSolids.clear();
        for(const auto& p:middleLayer.trees)middleLayerSolids.push_back(p.trunk);
        canopyPopulation=std::move(canopy);canopySolids=std::move(wood);canopyPose=canopyPopulation.plants.front().pose;
        forestCover=std::move(cover);groundCover.clear();for(const auto& p:forestCover.plants)groundCover.push_back(p.transform.position);
        fernColonies=std::move(colonies);shrubCanopy=std::move(shrubs);roadsideCover=std::move(roadside);
        roadsideBroadleaf=std::move(broadRoadside);entranceUnderstory=std::move(entry);trailLitter=std::move(litter);
        const auto oldTerrain=Mistwatch::WestCliffRelief::Fingerprint(terrain);terrain=derived.terrain;
        if(valleyOutcropPreview&&!valleyOutcrop.mesh)valleyOutcrop.Initialize(e,terrain,boulderMaterial);
        solids=RuntimeWorldSolids(session.bridgeRepaired);
        stage("valley_upload_commit");
        const auto path=valleyEvidenceDirectory/"valley-world.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Valley receipt refuses overwrite");
        std::ofstream out(path);out<<"schema=mistwatch.valley-world.1\nprofile="<<(valleyJointRockBank?"upstream-rock-contact-bank-v12":valleyTerraceEnabled?"south-valley-authored-terrace-v9":Mistwatch::ValleyTerrain::Profile)
            <<"\nstudy_epoch="<<Mistwatch::ValleyTerrain::StudyEpoch<<"\ninput_terrain="<<oldTerrain
            <<"\nactual_terrain="<<Mistwatch::WestCliffRelief::Fingerprint(terrain)<<"\nchanged_samples="<<derived.changed
            <<"\nmaximum_rise="<<derived.maximumRise<<"\nmoved_roots="<<movedRoots<<"\nuploads="<<uploads.size()
            <<"\nheight_solve_iterations="<<derived.solveIterations<<"\nheight_solve_residual="<<derived.solveResidual
            <<"\nraised_support_groups="<<derived.raisedGroups
            <<"\nfir_count="<<trees.size()<<"\nprop_count="<<forestProps.size()<<"\ngrass_count="<<groundCover.size()
            <<"\ncanopy_count="<<canopyPopulation.plants.size()<<"\nwood_count="<<canopySolids.size()
            <<"\ncollision_count="<<solids.size()<<"\nfull_world_qualified=0\nfull_contact_qualified=0\nnormal_save_promotion=0\n";
        out<<"support_pads="<<supportPads.size()<<"\nfootprints_inspected="<<placementReceipt.inspected
            <<"\ninherited_footprint_failures="<<placementReceipt.inheritedFootprintFailures
            <<"\nshifted_footprint_failures="<<placementReceipt.beforeFailures<<"\nrelocated_plants="<<placementReceipt.relocated
            <<"\nplacement_proposals="<<placementReceipt.proposals<<"\nremaining_footprint_failures="<<placementReceipt.afterFailures<<'\n';
        out<<"original_fern_center_policy="<<placementReceipt.centerPolicyInspected
            <<"\nunqualified_original_fern_full_footprints="<<placementReceipt.unqualifiedOriginalFernFootprints<<'\n';
        out<<"reauthored_full_footprints="<<fullFootprints<<"\nreauthored_grass="<<forestCover.plants.size()
            <<"\nreauthored_colony_ferns="<<fernColonies.plants.size()<<"\nreauthored_shrubs="<<shrubCanopy.plants.size()<<'\n';
        out.close();if(!out)throw std::runtime_error("Valley receipt write failed");
        std::ofstream roots(valleyEvidenceDirectory/"valley-anchors.tsv");roots<<anchors.str();roots.close();
        std::ofstream heights(valleyEvidenceDirectory/"valley-heightfield.tsv");heights.precision(9);heights<<"x\tz\ty\n";
        for(unsigned z=0;z<terrain.Settings().depth;++z)for(unsigned x=0;x<terrain.Settings().width;++x){
            const auto p=terrain.SamplePosition(x,z);heights<<p.x<<'\t'<<p.z<<'\t'<<p.y<<'\n';}
        heights.close();if(!roots||!heights)throw std::runtime_error("Valley geometry readback write failed");
        std::ofstream placement(valleyEvidenceDirectory/"valley-placement.tsv");placement<<placementLog.str();placement.close();
        std::ofstream pads(valleyEvidenceDirectory/"valley-support-pads.tsv");pads.precision(9);pads<<"x\ty\tz\tex\tey\tez\n";
        for(const auto& p:supportPads)pads<<p.center.x<<'\t'<<p.center.y<<'\t'<<p.center.z<<'\t'<<p.extents.x<<'\t'<<p.extents.y<<'\t'<<p.extents.z<<'\n';
        pads.close();if(!placement||!pads)throw std::runtime_error("Valley support readback write failed");
        std::ofstream shifts(valleyEvidenceDirectory/"valley-support-shifts.tsv");shifts.precision(9);shifts<<"id\tgroup\tdelta_y\n";
        for(std::size_t i=0;i<supportPads.size();++i)shifts<<i<<'\t'<<derived.supportGroups[i]<<'\t'<<derived.supportShifts[i]<<'\n';
        shifts.close();if(!shifts)throw std::runtime_error("Valley support shifts readback failed");
        std::ofstream contact(valleyEvidenceDirectory/"valley-contact-cells.tsv");contact.precision(9);contact<<"id\tx\tz\tminimum_gap\n";
        for(unsigned i=0;i<supportSurfaces.size();++i)for(const auto& c:supportSurfaces[i].cells)
            contact<<i<<'\t'<<c.x<<'\t'<<c.z<<'\t'<<c.minimumGap<<'\n';
        contact.close();if(!contact)throw std::runtime_error("Valley contact readback failed");
        std::ofstream finalPlants(valleyEvidenceDirectory/"valley-final-plants.tsv");finalPlants.precision(9);
        finalPlants<<"group\tid\tx\torigin_y\tz\tradius\tscale\tyaw\tvariant\tground_root_y\n";
        const auto row=[&](const char* group,std::size_t i,const Transform3D& p,float radius,unsigned variant,float localMinimum=0.f){
            finalPlants<<group<<'\t'<<i<<'\t'<<p.position.x<<'\t'<<p.position.y<<'\t'<<p.position.z<<'\t'<<radius<<'\t'<<p.scale.x<<'\t'<<p.rotation.y<<'\t'<<variant<<'\t'
                <<p.position.y+localMinimum*p.scale.x<<'\n';};
        for(std::size_t i=0;i<forestCover.plants.size();++i){const auto& p=forestCover.plants[i];row("grass",i,p.transform,p.radius,p.variant);}
        for(std::size_t i=0;i<fernColonies.plants.size();++i){const auto& p=fernColonies.plants[i];row("colony",i,p.transform,p.radius,p.variant);}
        for(std::size_t i=0;i<shrubCanopy.plants.size();++i){const auto& p=shrubCanopy.plants[i];row("shrub",i,p.pose,p.radius,p.variant);}
        for(std::size_t i=0;i<roadsideCover.plants.size();++i){const auto& p=roadsideCover.plants[i];row("roadside",i,p.transform,p.radius,p.variant);}
        for(std::size_t i=0;i<roadsideBroadleaf.plants.size();++i){const auto& p=roadsideBroadleaf.plants[i];row("broad-roadside",i,p.transform,p.radius,p.variant,roadsideBroadleafBounds[p.variant].minimumY);}
        for(std::size_t i=0;i<entranceUnderstory.plants.size();++i){const auto& p=entranceUnderstory.plants[i];row("entrance-shrub",i,p.pose,p.radius,p.variant,shrubCanopy.variants[p.variant].minimumY);}
        finalPlants.close();if(!finalPlants)throw std::runtime_error("Valley final plants readback failed");
        valleySaveDomain.emplace(Mistwatch::ValleySaveDomain::Create(terrain,entranceWorld.mission,supportPads,supportSurfaces,
            RuntimeWorldSolids(false),RuntimeWorldSolids(true),valleyTerraceEnabled,valleyJointRockBank));
        if(forestPlayableWorld){
            // 與讀檔實際使用的base+extra順序逐項核對，不只核 RuntimeWorldSolids 自己的快照。
            for(bool repaired:{false,true}){
                auto prepared=RuntimeBaseSolids(repaired);const auto extra=RuntimeExtraSolids();
                prepared.insert(prepared.end(),extra.begin(),extra.end());
                if(!valleySaveDomain->MatchesWorld(repaired,prepared))throw std::runtime_error("Forest playable save extra closure mismatch");
            }
        }
        if(workshopRoof)valleySaveDomain->BindWorkshopRoof(workshopRoof->visualFingerprint,RuntimeRoofHulls(),RuntimeRoofCameraOnly());
        if(!valleySaveDomain->MatchesAttachments(RuntimeRoofHulls(),RuntimeRoofCameraOnly()))
            throw std::runtime_error("Valley actual roof binding mismatch");
        if(manualValleyCandidate)save=save.parent_path()/((forestPlayableWorld?"save-forest-grove-v13-":valleyTerraceEnabled?"save-valley-terrace-v9-":workshopRoof?"save-valley-workshop-v8-":"save-valley-v7-")+std::to_string(valleySaveDomain->Identity())+".txt");
        std::ofstream identity(valleyEvidenceDirectory/"valley-save-domain.txt");
        identity<<"schema=mistwatch.valley-save-domain.1\nworld="<<valleySaveDomain->Identity()<<"\nmanual_entry="<<manualValleyCandidate
            <<"\nforest_playable_profile="<<(forestPlayableWorld?"upstream-grove-wip-v1":"none")
            <<"\nsave_extra_count="<<RuntimeExtraSolids().size()
            <<"\nsave_filename="<<save.filename().string()<<"\nnormal_default_changed=0\n"
            <<"workshop_roof_bound="<<bool(workshopRoof)<<"\nworkshop_roof_hulls="<<RuntimeRoofHulls().size()
            <<"\nworkshop_roof_camera_boxes="<<RuntimeRoofCameraOnly().size()<<'\n';
        identity.close();if(!identity)throw std::runtime_error("Valley save identity receipt failed");
        std::ofstream bakeLog(valleyEvidenceDirectory/"valley-startup-bake.txt");
        bakeLog<<"schema=mistwatch.valley-startup-bake.1\nenabled="<<valleyBakeEnabled<<"\nexported="<<!valleyBakeExportPath.empty()
            <<"\nexact_input_bytes="<<valleyBakeWitness.size()<<"\nwood_groups="<<canopyPopulation.plants.size()<<"\nwood_parts="<<canopySolids.size()
            <<"\nsupport_pads="<<supportPads.size()<<"\nworld="<<valleySaveDomain->Identity()<<'\n';
        bakeLog.close();if(!bakeLog)throw std::runtime_error("Startup bake receipt failed");
        valleyLoadedBake.reset();valleyBakeWitness.clear();
    }
