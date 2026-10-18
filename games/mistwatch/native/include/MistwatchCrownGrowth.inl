// 納入 MistwatchGame：基底大樹與補植小樹各有真正資源，原根位置與 Stand 封印不變。
const MeshResource3D& BaseLandscapeFir(unsigned slot)const {
    if(slot>=4)throw std::runtime_error("Crown growth slot invalid");
    return landscapeCrownGrowth?grownLandscapeFir[slot]:landscapeFirMeshes[slot];
}
const MeshResource3D& BaseLandscapeTwig()const {
    return landscapeCrownGrowth?grownLandscapeFir[4]:landscapeDetailedFirTwig;
}
unsigned ReleaseCrownGrowth(Engine& e) {
    unsigned count{};for(auto& mesh:grownLandscapeFir){if(mesh){e.DestroyMesh3D(mesh.handle);++count;}mesh={};}return count;
}
// 回收失敗仍保留 handle；逐個嘗試，不能以 noexcept 終止程序或假稱全部已釋放。
void RollbackCrownGrowth(Engine& e)noexcept {
    for(auto& mesh:grownLandscapeFir){if(!mesh)continue;
        try{e.DestroyMesh3D(mesh.handle);mesh={};}catch(...){}
    }
}
void InitializeCrownGrowth(Engine& e){
    if(!landscapeCrownGrowth)return;
    if(!landscapeStandFill||!landscapeTwigDistanceLod||landscapeTrees.size()!=520||landscapeStandPlan.combined.size()!=616||
        firLayoutSelector!=Mistwatch::FirLayoutSelector::ForestWood||mixedLandscapeCanopy||authoredLandscapePlanting)
        throw std::runtime_error("Crown growth source population invalid");
    Mistwatch::MixedLandscapeCanopy::ValidateStand(terrain,landscapeTrees,landscapeStandBounds,landscapeStandPlan);
    Vec3 lo{10000,10000,10000},hi{-10000,-10000,-10000};
    for(unsigned slot=0;slot<5;++slot){const auto& source=slot<4?landscapeFirMeshes[slot]:landscapeDetailedFirTwig;
        if(!source||!source.cpuMesh)throw std::runtime_error("Crown growth source unavailable");
        crownGrowthProof[slot]=Mistwatch::LandscapeCrownGrowth::Grow(*source.cpuMesh);
        if(crownGrowthProof[slot].sourceFingerprint!=Mistwatch::LandscapeCrownGrowth::SourceFingerprints[slot])
            throw std::runtime_error("Crown growth source slot mismatch");
        grownLandscapeFir[slot]=e.CreateMeshResource3D(crownGrowthProof[slot].mesh);
        if(!grownLandscapeFir[slot]||!grownLandscapeFir[slot].cpuMesh||
            Mistwatch::RootSupportDetail::MeshFingerprint(*grownLandscapeFir[slot].cpuMesh)!=crownGrowthProof[slot].derivedFingerprint)
            throw std::runtime_error("Crown growth GPU resource identity mismatch");
        ++crownGrowthAcquisitions;
        for(const auto& v:grownLandscapeFir[slot].cpuMesh->vertices){
            lo.x=(std::min)(lo.x,v.x);lo.y=(std::min)(lo.y,v.y);lo.z=(std::min)(lo.z,v.z);
            hi.x=(std::max)(hi.x,v.x);hi.y=(std::max)(hi.y,v.y);hi.z=(std::max)(hi.z,v.z);
        }
    }
    const auto ex=(hi-lo)*.5f;const MeshBounds3D bounds{(hi+lo)*.5f,Length(ex),ex};
    for(const auto& pose:landscapeTrees){const auto b=TransformBounds(bounds,pose);const auto a=b.center-b.extents,z=b.center+b.extents;
        if(!(z.x< -50.f||a.x>50.f||z.z< -20.f||a.z>100.f)||a.x< -200.f||z.x>200.f||a.z< -170.f||z.z>250.f)
            throw std::runtime_error("Crown growth actual core or edge intrusion");
    }
    grownTwigUnionBounds=Mistwatch::LandscapeTwigDraw::UnionBounds(grownLandscapeFir[4],grownLandscapeFir[1]);
}
void RequireCrownGrowthDraw(const MeshInstance3D& item,const MeshResource3D& mesh,
                          const Transform3D& pose,const Material3D& m)const {
    const auto matrix=pose.Matrix(),normal=CreateNormalMatrix(matrix);
    if(std::memcmp(&item.transform,&matrix,sizeof(Matrix4))||std::memcmp(&item.previousTransform,&matrix,sizeof(Matrix4))||
        std::memcmp(&item.normalTransform,&normal,sizeof(Matrix4))||
        !Mistwatch::LandscapeTwigDraw::ExactResourceBindings(item,mesh.handle,m.textures)||
        item.tint!=PackColorRgba8(m.tint)||item.surface!=PackSurface3D(m.surface)||
        item.textureScale!=(std::max)(.001f,m.textureScale)||item.normalScale!=Clamp(m.normalScale,-16.f,15.96875f)||
        item.useUv!=m.useUv||item.alphaMode!=m.alphaMode||item.alphaCutoff!=Clamp(m.alphaCutoff,0.f,1.f)||
        item.doubleSided!=m.doubleSided||item.materialClass!=m.materialClass||item.occlusionOpacity!=1.f)
        throw std::runtime_error("Crown growth post-cull actual pose or material mismatch");
}
void WriteCrownGrowthReceipt(const std::filesystem::path& frame,const LandscapeFrame& observed)const {
    auto path=frame;path+=L".crown-growth.txt";
    if(std::filesystem::exists(path))throw std::runtime_error("Crown growth receipt refuses overwrite");
    std::ofstream out(path);out<<"schema=mistwatch.crown-growth-native.1\nprofile="
        <<(landscapeCrownGrowth?Mistwatch::LandscapeCrownGrowth::Profile:"original")
        <<"\nbase_population="<<landscapeTrees.size()<<"\nunderstory_population="<<(landscapeStandFill?96u:0u)
        <<"\nstand_fingerprint="<<landscapeStandPlan.fingerprint<<"\nacquisitions="<<crownGrowthAcquisitions
        <<"\nobserved_draws="<<observed.grownDraws<<"\nobserved_draws_cumulative="<<crownGrowthObservedDraws
        <<"\naccepted_base_slots="<<observed.acceptedFir
        <<"\naccepted_broadleaf_slots="<<observed.acceptedBroadleaf<<"\nscene_serial="<<observed.sceneSerial
        <<"\ncamera_hash="<<observed.cameraHash<<"\nroot_repair_passed=0\nart_accepted=0\n";
    for(unsigned slot=0;slot<5;++slot){const auto& proof=crownGrowthProof[slot];const auto& mesh=grownLandscapeFir[slot];
        out<<"source_"<<slot<<"="<<proof.sourceFingerprint<<"\nderived_"<<slot<<"="<<proof.derivedFingerprint
            <<"\nhandle_"<<slot<<"="<<mesh.handle.value<<"\nmoved_"<<slot<<"="<<proof.movedVertices
            <<"\nfrozen_root_faces_"<<slot<<"="<<proof.frozenRootFaces<<"\nnormal_updates_"<<slot<<"="<<proof.affectedNormals<<'\n';
    }
    out.close();if(!out)throw std::runtime_error("Crown growth receipt write failed");
}
