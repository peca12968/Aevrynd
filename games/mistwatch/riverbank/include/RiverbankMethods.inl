    std::optional<HeightField3D> riverbankOriginal;
    void RebuildRiverbank(Engine& e){
        MatureContact::Require(!riverbankOriginal&&matureContactWorld.Ready(),"RIVERBANK_ORIGINAL_WORLD_NOT_READY");
        std::vector<Mistwatch::RiverbankConstruction::Guard> roots;
        const auto add=[&](Vec3 p,float radius){roots.push_back({p.x,p.z,radius,radius});};
        for(const auto& p:forestCover.plants)add(p.transform.position,p.radius);
        for(const auto& p:fernColonies.plants)add(p.transform.position,p.radius);
        for(const auto& p:shrubCanopy.plants)add(p.pose.position,p.radius);
        for(const auto& p:roadsideCover.plants)add(p.transform.position,p.radius);
        for(const auto& p:roadsideBroadleaf.plants)add(p.transform.position,p.radius);
        for(const auto& p:entranceUnderstory.plants)add(p.pose.position,p.radius);
        for(std::size_t i=0;i<understory.ferns.size();++i)add(understory.ferns[i].position,fernFootprints[i%4].radius*understory.ferns[i].scale.x);
        for(const auto& p:understory.saplings)add(p.position,1.4f*p.scale.x);
        auto rebuilt=Mistwatch::RiverbankConstruction::Build(terrain,matureContactWorld.All(false),roots);
        std::vector<std::pair<MeshResource3D*,MeshResource3D>> uploads;
        const auto upload=[&](MeshResource3D& old){
            if(!old)return;
            auto cpu=Mistwatch::ValleyTerrain::Warp(*old.cpuMesh,terrain,rebuilt.field,true);
            for(auto& v:cpu.vertices){
                if(!terrain.Contains(v.x,v.z)||terrain.SampleHeight(v.x,v.z)==rebuilt.field.SampleHeight(v.x,v.z))continue;
                const float exposed=Mistwatch::RiverbankConstruction::Smooth((.1f-v.y)/1.f)*Mistwatch::RiverbankConstruction::Smooth((v.y+4.5f)/1.5f);
                const float rock=(std::max)(v.v,.85f*exposed);Vec2 encoded{};
                if(!TryEncodeTerrainLayerCoverage3D((std::min)(v.u,1.f-rock),rock,encoded))throw std::runtime_error("RIVERBANK_REAL_BANK_MATERIAL_INVALID");
                v.u=encoded.x;v.v=encoded.y;
            }
            auto fresh=e.CreateMeshResource3D(cpu);
            if(!fresh||!fresh.cpuMesh||fresh.cpuMesh->indices!=cpu.indices||fresh.cpuMesh->vertices.size()!=cpu.vertices.size()||std::memcmp(fresh.cpuMesh->vertices.data(),cpu.vertices.data(),cpu.vertices.size()*sizeof(Vertex3D))!=0){if(fresh)e.DestroyMesh3D(fresh.handle);throw std::runtime_error("RIVERBANK_GPU_GEOMETRY_UPLOAD_MISMATCH");}
            uploads.emplace_back(&old,fresh);
        };
        try{upload(terrainMesh);for(auto& mesh:entranceRoadMeshes)upload(mesh);}
        catch(...){for(auto& pair:uploads)e.DestroyMesh3D(pair.second.handle);throw;}
        for(auto& pair:uploads){e.DestroyMesh3D(pair.first->handle);*pair.first=pair.second;}
        riverbankOriginal=terrain;terrain=std::move(rebuilt.field);

        const auto output=indexedGameplayOutput.empty()?valleyEvidenceDirectory:indexedGameplayOutput;
        std::filesystem::create_directories(output);
        std::ofstream field(output/"riverbank-heightfield.tsv");field.precision(9);field<<"x\tz\tbefore\tafter\n";
        for(unsigned z=0;z<terrain.Settings().depth;++z)for(unsigned x=0;x<terrain.Settings().width;++x){const auto p=terrain.SamplePosition(x,z);field<<p.x<<'\t'<<p.z<<'\t'<<riverbankOriginal->Height(x,z)<<'\t'<<p.y<<'\n';}
        field.close();std::ofstream guards(output/"riverbank-guards.tsv");guards.precision(9);guards<<"x\tz\tex\tez\n";
        for(const auto& g:rebuilt.guards)guards<<g.x<<'\t'<<g.z<<'\t'<<g.ex<<'\t'<<g.ez<<'\n';guards.close();
        std::ofstream receipt(output/"riverbank-construction.txt");receipt<<"profile=coherent-riverbend-v1\nchanged_samples="<<rebuilt.changed<<"\nmaximum_change="<<rebuilt.maximumChange<<"\nprotected_samples="<<rebuilt.protectedSamples<<"\nguards="<<rebuilt.guards.size()<<"\nuploads="<<uploads.size()<<"\nold_terrain="<<Mistwatch::WestCliffRelief::Fingerprint(*riverbankOriginal)<<"\nactual_terrain="<<Mistwatch::WestCliffRelief::Fingerprint(terrain)<<"\nart_accepted=0\n";
        receipt.close();if(!field||!guards||!receipt)throw std::runtime_error("RIVERBANK_COMPLETE_SOURCE_EXPORT_FAILED");
    }
