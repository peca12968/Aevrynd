// 納入MistwatchGame；新增草與原3600/96/192各有獨立資源和身份。
void RollbackRouteGrass(Engine& e)noexcept{
    for(auto& mesh:routeGrassMeshes){if(!mesh)continue;try{e.DestroyMesh3D(mesh.handle);mesh={};}catch(...){}}
}
unsigned ReleaseRouteGrass(Engine& e){
    unsigned released{};for(auto& mesh:routeGrassMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++released;}mesh={};}return released;
}
void InitializeRouteGrass(Engine& e){
    if(!routeGrassEnabled)return;
    if(!coreHerbEnabled||!woodlandWorld||completeUnderstoryRows.size()!=3888||roadsideCoverEnabled)
        throw std::runtime_error("Route grass requires complete isolated current understory");
    Mesh3D actualGround;
    for(const auto& resource:entranceRoadMeshes){
        if(!resource||!resource.cpuMesh)throw std::runtime_error("Route grass actual ground unavailable");
        std::map<unsigned,unsigned> remap;
        for(auto index:resource.cpuMesh->indices){
            auto found=remap.find(index);
            if(found==remap.end()){const auto& v=resource.cpuMesh->vertices[index];
                if(!terrain.Contains(v.x,v.z)||std::abs(double(v.y)-terrain.SampleHeight(v.x,v.z))>.000002)
                    throw std::runtime_error("Route grass actual ground collision disagreement");
                const auto next=static_cast<unsigned>(actualGround.vertices.size());actualGround.vertices.push_back(v);
                found=remap.emplace(index,next).first;}
            actualGround.indices.push_back(found->second);
        }
    }
    if(Mistwatch::CoreHerbClusters::Detail::MeshFingerprint(actualGround)!=coreHerbSurfaceFingerprint)
        throw std::runtime_error("Route grass ground epoch mismatch");
    std::vector<BoxCollider3D> retained;
    const auto retain=[&](const Mesh3D& mesh,const Transform3D& pose){
        if(!mesh.Valid())throw std::runtime_error("Route grass retained resource unavailable");
        const auto b=TransformBounds(mesh.Bounds(),pose);retained.push_back({b.center+Vec3{0,.0055f,0},b.extents+Vec3{.19f,.0055f,.19f}});
    };
    for(const auto& p:shrubCanopy.plants){const auto& raw=shrubCanopy.variants[p.variant];retain(raw.stems,p.pose);retain(raw.leaves,p.pose);}
    for(const auto& p:entranceUnderstory.plants){const auto& raw=shrubCanopy.variants[p.variant];retain(raw.stems,p.pose);retain(raw.leaves,p.pose);}
    for(unsigned i=0;i<understory.ferns.size();++i)retain(*scannedFerns[i%4].cpuMesh,understory.ferns[i]);
    for(const auto& p:fernColonies.plants)retain(*scannedFerns[p.variant].cpuMesh,p.transform);
    std::array<const Mesh3D*,5> sources;
    for(unsigned slot=0;slot<5;++slot){if(!scannedGrass[slot].cpuMesh)throw std::runtime_error("Route grass actual source missing");
        sources[slot]=scannedGrass[slot].cpuMesh.get();}
    const auto begin=std::chrono::steady_clock::now();
    if(routeGrassCooked){
#if defined(AEVRYND_ROUTE_GRASS_APPROVED_BUNDLE_SHA256) && defined(AEVRYND_ROUTE_GRASS_APPROVED_BUILD_SHA256)
        routeGrassBundleSha=AEVRYND_ROUTE_GRASS_APPROVED_BUNDLE_SHA256;routeGrassBuildSha=AEVRYND_ROUTE_GRASS_APPROVED_BUILD_SHA256;
        const Mistwatch::RouteGrassCookLoad::Approval approval{
            Mistwatch::RouteGrassCook::ParseDigest(routeGrassBundleSha),Mistwatch::RouteGrassCook::ParseDigest(routeGrassBuildSha)};
        const auto bytes=Mistwatch::RouteGrassCookLoad::Read(e.ExecutableDirectory()/"assets/mistwatch-route-grass/route-grass.aecook");
        routeGrassPlan=Mistwatch::RouteGrassCookLoad::Decode(bytes,approval);
        Mistwatch::RouteGrassCookLoad::Admit(routeGrassPlan,sources,actualGround,terrain,*woodlandWorld,retained,completeUnderstoryRows,approval);
#else
        throw std::runtime_error("ROUTE_GRASS_LOAD_NO_APPROVED_BUILD");
#endif
    }else routeGrassPlan=Mistwatch::RouteGrassPlan::Create(sources,actualGround,terrain,*woodlandWorld,retained,completeUnderstoryRows);
    routeGrassPlacementNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();
    for(unsigned i=0;i<routeGrassPlan.instances.size();++i){
        const auto& p=routeGrassPlan.instances[i].plant;auto& mesh=routeGrassMeshes[i];mesh=e.CreateMeshResource3D(p.derivedMesh);
        if(mesh)++routeGrassAcquisitions;
        if(!mesh||!mesh.cpuMesh||Mistwatch::CoreHerbClusters::Detail::MeshFingerprint(*mesh.cpuMesh)!=p.derivedFingerprint)
            throw std::runtime_error("Route grass GPU resource identity mismatch");
    }
}
void ObserveRouteGrass(const DrawList& draw,const Camera3D& camera,const Scene3DSubmissionOptions& submission){
    std::array<Mistwatch::RouteGrassDraw::Expected,128> expected{};
    if(routeGrassEnabled)for(unsigned i=0;i<128;++i)
        expected[i]={routeGrassMeshes[i],routeGrassPlan.instances[i].plant.pose,grassSurface};
    Mistwatch::RouteGrassDraw::Observer observer(routeGrassEnabled?
        std::span<const Mistwatch::RouteGrassDraw::Expected>(expected):std::span<const Mistwatch::RouteGrassDraw::Expected>{});
    for(const auto& item:draw.meshInstances){
        observer.Observe(item);
        if(item.identity.id.high==Mistwatch::RouteGrassDraw::InstanceNamespace)
            Mistwatch::RouteGrassDraw::ValidateCameraPolicy(item,expected[static_cast<unsigned>(item.identity.id.low-1)],
                camera,submission,draw.DirectionalShadowMapEnabled());
    }
    const auto result=observer.Finish();routeGrassFrame.draws=result.draws;routeGrassFrame.opaque=result.opaque;
    routeGrassFrame.shadowOnly=result.shadowOnly;routeGrassFrame.triangles=result.triangles;
    routeGrassFrame.sceneSerial=landscapeFrame.sceneSerial;routeGrassFrame.cameraHash=landscapeFrame.cameraHash;
    if(routeGrassEnabled)for(unsigned i=0;i<128;++i){routeGrassFrame.visibility[i]=result.visibility[i];
        if(!result.visibility[i])routeGrassFrame.cullReasons[i]=Mistwatch::RouteGrassDraw::MissingReason(
            expected[i],camera,submission,draw.DirectionalShadowMapEnabled());}
}
void WriteRouteGrassReceipt(const std::filesystem::path& prefix,const RouteGrassFrame& frame)const{
    auto path=prefix;path+=L".route-grass.txt";
    if(std::filesystem::exists(path))throw std::runtime_error("Route grass refuses receipt overwrite");
    std::ofstream out(path);out<<std::setprecision(17)<<"schema=mistwatch.route-grass-native.2\nprofile="
        <<(routeGrassEnabled?(routeGrassCooked?Mistwatch::RouteGrassCookLoad::Profile:Mistwatch::RouteGrassPlan::Profile):"original")
        <<"\npopulation="<<routeGrassPlan.instances.size()
        <<"\nplan_fingerprint="<<routeGrassPlan.fingerprint<<"\nsurface_fingerprint="<<routeGrassPlan.surfaceFingerprint
        <<"\nplacement_ns="<<routeGrassPlacementNs<<"\nacquisitions="<<routeGrassAcquisitions
        <<"\naccepted="<<frame.accepted<<"\ndraws="<<frame.draws<<"\nopaque="<<frame.opaque<<"\nshadow_only="<<frame.shadowOnly
        <<"\ntriangles="<<frame.triangles<<"\nscene_serial="<<frame.sceneSerial<<"\ncamera_hash="<<frame.cameraHash
        <<"\ngpu_buffer_readback=0\nart_accepted=0\nproof_mode="<<(routeGrassCooked?"approved-cooked-reuse":"runtime-exact")
        <<"\nreused_proofs="<<(routeGrassCooked?routeGrassPlan.instances.size():0)<<"\nbundle_sha256="<<routeGrassBundleSha
        <<"\ncook_build_sha256="<<routeGrassBuildSha<<'\n';
    if(routeGrassEnabled){out<<"context_schema="<<Mistwatch::RouteGrassContext::Schema<<"\ncontext_fingerprint="
        <<Mistwatch::RouteGrassContext::Fingerprint(routeGrassPlan.context)<<'\n';
        for(unsigned i=0;i<Mistwatch::RouteGrassContext::Names.size();++i)
            out<<"context_"<<Mistwatch::RouteGrassContext::Names[i]<<'='<<routeGrassPlan.context.words[i]<<'\n';}
    for(unsigned i=0;i<routeGrassPlan.instances.size();++i){const auto& instance=routeGrassPlan.instances[i];
        const auto& p=instance.plant;const auto& proof=instance.proof;
        out<<"plant_"<<i<<"_ordinal="<<instance.ordinal<<"\nplant_"<<i<<"_bin="<<instance.bin
            <<"\nplant_"<<i<<"_variant="<<p.variant<<"\nplant_"<<i<<"_source="<<p.sourceFingerprint
            <<"\nplant_"<<i<<"_derived="<<p.derivedFingerprint<<"\nplant_"<<i<<"_root="<<p.root.x<<','<<p.root.y<<','<<p.root.z
            <<"\nplant_"<<i<<"_pose="<<p.pose.position.x<<','<<p.pose.position.y<<','<<p.pose.position.z
            <<"\nplant_"<<i<<"_scale="<<p.pose.scale.x<<"\nplant_"<<i<<"_yaw="<<p.pose.rotation.y
            <<"\nplant_"<<i<<"_radius="<<p.radius<<"\nplant_"<<i<<"_safe="<<proof.contact.safe
            <<"\nplant_"<<i<<"_faces="<<proof.allFaces<<"\nplant_"<<i<<"_foot_pieces="<<proof.sourceFootPieces
            <<"\nplant_"<<i<<"_pinned="<<proof.windPinnedVertices<<"\nplant_"<<i<<"_handle="<<routeGrassMeshes[i].handle.value
            <<"\nplant_"<<i<<"_visible="<<frame.visibility[i]<<"\nplant_"<<i<<"_cull_reason="<<frame.cullReasons[i]<<'\n';
        if(!routeGrassMeshes[i]||!routeGrassMeshes[i].cpuMesh)throw std::runtime_error("Route grass receipt actual resource unavailable");
        Mistwatch::RouteGrassCook::Bytes actual;actual.Mesh(*routeGrassMeshes[i].cpuMesh);
        auto geometry=prefix;geometry+=L".route-grass-"+std::to_wstring(i)+L".aemesh";
        Mistwatch::RouteGrassCook::WriteFresh(geometry,actual.Data());
    }
    out.close();if(!out)throw std::runtime_error("Route grass receipt write failed");
}
