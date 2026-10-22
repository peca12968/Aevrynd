    void RebuildStormRiver(Engine& e){
        MatureContact::Require(riverMesh&&riverMesh.cpuMesh,"STORM_WATER_ORIGINAL_RESOURCE_MISSING");
        const auto cpu=Mistwatch::StormRiver::Build(terrain);
        MatureContact::Require(cpu.Valid()&&!cpu.vertices.empty(),"STORM_WATER_FINAL_TERRAIN_CLIP_EMPTY");
        auto fresh=e.CreateMeshResource3D(cpu);
        if(!fresh||!fresh.cpuMesh||fresh.cpuMesh->indices!=cpu.indices||fresh.cpuMesh->vertices.size()!=cpu.vertices.size()||
            std::memcmp(fresh.cpuMesh->vertices.data(),cpu.vertices.data(),cpu.vertices.size()*sizeof(Vertex3D))!=0){
            if(fresh)e.DestroyMesh3D(fresh.handle);throw std::runtime_error("STORM_WATER_UPLOADED_CPU_SOURCE_MISMATCH");
        }
        bool transferred=false;
        try{
        const auto output=indexedGameplayOutput;
        const auto write=[&](const std::filesystem::path& path,const Mesh3D& source){
            const std::array<std::uint64_t,2> counts{source.vertices.size(),source.indices.size()};
            std::ofstream raw(path,std::ios::binary);raw.write(reinterpret_cast<const char*>(counts.data()),sizeof(counts));
            raw.write(reinterpret_cast<const char*>(source.vertices.data()),std::streamsize(source.vertices.size()*sizeof(Vertex3D)));
            raw.write(reinterpret_cast<const char*>(source.indices.data()),std::streamsize(source.indices.size()*sizeof(std::uint32_t)));
            raw.close();MatureContact::Require(bool(raw),"STORM_WATER_COMPLETE_MESH_SOURCE_WRITE_FAILED");
        };
        const auto& old=*riverMesh.cpuMesh;
        const auto oldVertices=old.vertices.size(),oldIndices=old.indices.size();
        const auto oldBytes=old.vertices.size()*sizeof(Vertex3D)+old.indices.size()*sizeof(std::uint32_t);
        const auto newBytes=cpu.vertices.size()*sizeof(Vertex3D)+cpu.indices.size()*sizeof(std::uint32_t);
        const auto withBoth=e.GetRendererStats();
        MatureContact::Require(withBoth.residentMeshBytes>=newBytes,"STORM_WATER_MEMORY_ACCOUNTING_UNDERFLOW");
        const auto before=withBoth.residentMeshBytes-newBytes;
        write(output/"storm-river-old-actual-mesh.bin",old);write(output/"storm-river-current-actual-mesh.bin",*fresh.cpuMesh);
        e.DestroyMesh3D(riverMesh.handle);riverMesh=fresh;transferred=true;
        const auto after=e.GetRendererStats();
        MatureContact::Require(after.residentMeshBytes==before-oldBytes+newBytes&&after.meshMemoryBudgetBytes==134217728ull&&after.meshBudgetRejections==0,
            "STORM_WATER_ORIGINAL_BUDGET_OR_EXACT_GEOMETRY_DELTA_MISMATCH");
        unsigned shore{},interior{};
        for(const auto& v:cpu.vertices){const float w=UnpackNormalSnorm8(v.normal).w;shore+=unsigned(w==0);interior+=unsigned(w==1);}
        std::ofstream out(output/"storm-river.txt");out.precision(9);
        out<<"profile=storm-final-terrain-shore-weight-v1\nheight="<<Mistwatch::StormRiver::Height<<"\nfade="<<Mistwatch::StormRiver::ShoreFade
           <<"\nold_vertices="<<oldVertices<<"\nold_indices="<<oldIndices<<"\nold_mesh_bytes="<<oldBytes<<"\nnew_mesh_bytes="<<newBytes
           <<"\ncurrent_vertices="<<cpu.vertices.size()<<"\ncurrent_indices="<<cpu.indices.size()<<"\nzero_wave_vertices="<<shore<<"\nfull_wave_vertices="<<interior
           <<"\nprior_scene_resident_bytes="<<before<<"\nexpected_full_scene_resident_bytes="<<after.residentMeshBytes
           <<"\nterrain_fingerprint="<<Mistwatch::WestCliffRelief::Fingerprint(terrain)<<"\ncollision_terrain_or_player_modified=0\nGPU_buffer_readback=0\n";
        out.close();MatureContact::Require(bool(out),"STORM_WATER_PROFILE_RECEIPT_FAILED");
        }catch(...){if(!transferred)e.DestroyMesh3D(fresh.handle);throw;}
    }
