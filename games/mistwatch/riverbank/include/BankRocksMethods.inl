    bool bankRocksPreviewRequested{},bankRocksPreviewActive{};
    std::vector<Mistwatch::RiverbankConstruction::Guard> bankProtectedDomains;
    std::optional<Mistwatch::BankRocks::Model> bankRockModel;
    MeshResource3D bankRockMesh;
    unsigned bankRockAcquisitions{};
    std::uint64_t bankRockResidentDelta{};
    void InitializeBankRocksPreview(Engine& e){
        if(!bankRocksPreviewActive)return;
        MatureContact::Require(!bankRockModel&&!bankRockMesh&&bankRockAcquisitions==0,"BANK_ROCK_PREVIEW_NO_DUPLICATE_INITIALIZATION");
        const auto cpu=Mistwatch::BoulderDetail::LoadBoulderDetailMesh(e.ExecutableDirectory()/"assets/mistwatch-boulders/meshes"/Mistwatch::BoulderDetail::MeshName);
        bankRockModel=Mistwatch::BankRocks::BuildRejectedPreview(terrain,cpu,bankProtectedDomains,matureContactWorld.All(false));
        MatureContact::Require(bankRockModel->rejectedHeightParts==4&&bankRockModel->measurements.size()==8,"BANK_ROCK_PREVIEW_MUST_PRESERVE_KNOWN_REJECTION");
        const auto before=e.GetRendererStats();
        bankRockMesh=e.CreateMeshResource3D(cpu);if(bankRockMesh)++bankRockAcquisitions;
        if(!bankRockMesh||!bankRockMesh.cpuMesh||bankRockMesh.cpuMesh->indices!=cpu.indices||bankRockMesh.cpuMesh->vertices.size()!=cpu.vertices.size()||std::memcmp(bankRockMesh.cpuMesh->vertices.data(),cpu.vertices.data(),cpu.vertices.size()*sizeof(Vertex3D))!=0){
            if(bankRockMesh)e.DestroyMesh3D(bankRockMesh.handle);bankRockMesh={};throw std::runtime_error("BANK_ROCK_PREVIEW_ACTUAL_UPLOAD_SOURCE_MISMATCH");
        }
        const auto after=e.GetRendererStats();
        MatureContact::Require(after.residentMeshBytes>=before.residentMeshBytes&&after.meshMemoryBudgetBytes==134217728ull&&after.meshBudgetRejections==0,"BANK_ROCK_PREVIEW_ORIGINAL_MESH_BUDGET");
        bankRockResidentDelta=after.residentMeshBytes-before.residentMeshBytes;
    }
    void WriteBankRocksPreviewProfile(){
        std::ofstream entry(indexedGameplayOutput/"bank-rocks-entry.txt");entry.precision(17);
        entry<<"schema=bank-rocks-explicit-unaccepted-preview-v1\nactive="<<bankRocksPreviewActive
            <<"\nmanual_cli="<<bankRocksPreviewRequested<<"\nheight_gate_applicable="<<bankRocksPreviewActive<<"\nheight_gate_passed=0\nformal_adoption=0\nsource_oracles_enabled="<<indexedGameplayProofEnabled
            <<"\nrock_count="<<(bankRockModel?bankRockModel->parts.size():0)<<"\nlegacy_gap_rejections="<<(bankRockModel?bankRockModel->rejectedHeightParts:0)
            <<"\nworld="<<matureIndexedDomain->Identity()<<"\nsave="<<save.string()
            <<"\nwater_level="<<Mistwatch::StormRiver::Height<<"\nmesh_acquisitions="<<bankRockAcquisitions<<"\nresident_mesh_delta="<<bankRockResidentDelta
            <<"\nmesh_handle="<<bankRockMesh.handle.value<<"\noriginal_prop_mesh_handle="<<propMeshes[0].handle.value<<'\n';
        entry.close();MatureContact::Require(bool(entry),"BANK_ROCK_PREVIEW_PROFILE_WRITE_FAILED");
        if(!bankRockModel)return;
        std::ofstream parts(indexedGameplayOutput/"bank-rocks-source.tsv");parts.precision(9);
        parts<<"x\ty\tz\trx\try\trz\tsx\tsy\tsz\tcx\tcy\tcz\tex\tey\tez\tmaximum_above\tmaximum_support_gap\tsupport_vertices\n";
        for(const auto&p:bankRockModel->parts){const auto&t=p.pose;const auto&b=p.collider;parts<<t.position.x<<'\t'<<t.position.y<<'\t'<<t.position.z<<'\t'<<t.rotation.x<<'\t'<<t.rotation.y<<'\t'<<t.rotation.z<<'\t'<<t.scale.x<<'\t'<<t.scale.y<<'\t'<<t.scale.z<<'\t'<<b.center.x<<'\t'<<b.center.y<<'\t'<<b.center.z<<'\t'<<b.extents.x<<'\t'<<b.extents.y<<'\t'<<b.extents.z<<'\t'<<p.maximumAbove<<'\t'<<p.maximumSupportGap<<'\t'<<p.supportVertices<<'\n';}
        parts.close();MatureContact::Require(bool(parts),"BANK_ROCK_PREVIEW_PARTS_WRITE_FAILED");
        std::ofstream metrics(indexedGameplayOutput/"bank-rocks-metrics.tsv");metrics.precision(17);
        metrics<<"part\tworld_height\tmaximum_terrain_gap\tmaximum_emergence\tmaximum_support_gap\n";
        for(std::size_t i=0;i<bankRockModel->measurements.size();++i){const auto&m=bankRockModel->measurements[i];metrics<<i<<'\t'<<m.worldHeight<<'\t'<<m.maximumTerrainGap<<'\t'<<m.maximumEmergence<<'\t'<<m.maximumSupportGap<<'\n';}
        metrics.close();MatureContact::Require(bool(metrics),"BANK_ROCK_PREVIEW_METRICS_WRITE_FAILED");
    }
    void ReleaseBankRocksPreview(Engine& e){
        const auto before=e.GetRendererStats();unsigned released{};
        if(bankRockMesh){e.DestroyMesh3D(bankRockMesh.handle);bankRockMesh={};++released;}
        const auto after=e.GetRendererStats();
        const bool complete=!bankRockMesh&&released==bankRockAcquisitions&&after.residentMeshReferences+released==before.residentMeshReferences;
        if(!indexedGameplayOutput.empty()){
            std::ofstream out(indexedGameplayOutput/"bank-rocks-cleanup.txt");
            out<<"active="<<bankRocksPreviewActive<<"\nacquisitions="<<bankRockAcquisitions<<"\nreleased="<<released<<"\nreferences_before="<<before.residentMeshReferences<<"\nreferences_after="<<after.residentMeshReferences<<"\nremaining_owned="<<unsigned(bool(bankRockMesh))<<"\ncleanup="<<(complete?"success":"failed")<<'\n';
            out.close();MatureContact::Require(bool(out),"BANK_ROCK_PREVIEW_CLEANUP_WRITE_FAILED");
        }
        MatureContact::Require(complete,"BANK_ROCK_PREVIEW_REFERENCE_RELEASE_FAILED");
    }
