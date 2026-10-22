#pragma once
#include <array>
#include <cstring>
#include <span>
#include <vector>
#include <fstream>
#include <Aevrynd/Engine.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/MeshLoader.h>
namespace MatureStudy {
struct PartData {
    const char* name;
    std::array<const char*,3> textures;
    std::array<std::uint64_t,3> sizes;
    std::array<const char*,3> hashes;
};
}
#include "MatureStudyAssets.h"
namespace MatureStudy {
// 外觀研究的顯示資源獨立於舊身體／鏡頭作者；不能借用舊碰撞證書作為新樹驗收。

// 只觀測第一次完整12樹。矩陣直接來自實際繪製，碰撞盒核對實際角色solids。
struct ContactObservation {
    struct Entry {std::array<float,9> sourcePose{},displayPose{};std::array<float,6> oldBody{};Aevrynd::Matrix4 matrix{};std::uint32_t oldBodyIndex{};};
    std::array<Entry,12> entries{};unsigned count{};std::uint64_t calls{};
    std::vector<Aevrynd::BoxCollider3D> originalSolids;
    static auto Pose(const Aevrynd::Transform3D& p){return std::array<float,9>{p.position.x,p.position.y,p.position.z,p.rotation.x,p.rotation.y,p.rotation.z,p.scale.x,p.scale.y,p.scale.z};}
    static auto Box(const Aevrynd::BoxCollider3D& p){return std::array<float,6>{p.center.x,p.center.y,p.center.z,p.extents.x,p.extents.y,p.extents.z};}
    template<class T> static bool Same(const T& a,const T& b){return std::memcmp(&a,&b,sizeof(T))==0;}
    void Observe(const Aevrynd::Transform3D& oldPose,const Aevrynd::Transform3D& actual,const Aevrynd::BoxCollider3D& oldBody,std::span<const Aevrynd::BoxCollider3D> solids){
        const auto ordinal=unsigned(calls%12);Entry entry{Pose(oldPose),Pose(actual),Box(oldBody),actual.Matrix(),0};
        if(count<12){
            if(count!=ordinal)throw std::runtime_error("Mature contact source order");
            if(count==0)originalSolids.assign(solids.begin(),solids.end());
            unsigned matches{};
            for(std::size_t i=0;i<solids.size();++i)if(Same(Box(solids[i]),entry.oldBody)){entry.oldBodyIndex=std::uint32_t(i);++matches;}
            if(matches!=1)throw std::runtime_error("Mature contact old body must have one exact runtime owner");
            entries[count++]=entry;
        }else{
            const auto& first=entries[ordinal];
            if(!Same(first.sourcePose,entry.sourcePose)||!Same(first.displayPose,entry.displayPose)||!Same(first.oldBody,entry.oldBody)||!Same(first.matrix,entry.matrix))
                throw std::runtime_error("Mature contact actual mesh pose changed across frame");
        }
        ++calls;
    }
    void Save(const std::filesystem::path& output)const{
        if(count!=12||calls<12||calls%12||originalSolids.empty()||originalSolids.size()>10000)throw std::runtime_error("Mature contact complete actual sources missing");
        const auto path=output/"mature-contact-source.bin";
        if(std::filesystem::exists(path))throw std::runtime_error("Mature contact refuses source overwrite");
        std::ofstream out(path,std::ios::binary);
        const std::uint32_t header[]{0x3154434du,1u,12u,std::uint32_t(originalSolids.size())};out.write(reinterpret_cast<const char*>(header),sizeof(header));
        const auto write=[&](const auto& values){out.write(reinterpret_cast<const char*>(values.data()),std::streamsize(values.size()*sizeof(values[0])));};
        for(const auto& entry:entries){write(entry.sourcePose);write(entry.displayPose);write(entry.oldBody);
            for(unsigned row=0;row<4;++row)out.write(reinterpret_cast<const char*>(entry.matrix[row]),4*sizeof(float));
            out.write(reinterpret_cast<const char*>(&entry.oldBodyIndex),sizeof(entry.oldBodyIndex));}
        for(const auto& body:originalSolids)write(Box(body));
        out.close();if(!out)throw std::runtime_error("Mature contact complete actual sources write failed");
    }
};

struct Runtime {
    ContactObservation contact;
    MatureContact::World* contactWorld{};
    std::array<Aevrynd::MeshResource3D,3> meshes;
    std::array<Aevrynd::Material3D,3> surfaces;
    Aevrynd::MaterialLibrary3D materials;
    std::uint64_t submitted{};
    std::size_t initialResident{},fullSceneResident{};
    void Initialize(Aevrynd::Engine& engine) {
        const auto initial=engine.GetRendererStats();
        if(initial.meshMemoryBudgetBytes!=128ull*1024*1024||initial.meshBudgetRejections)throw std::runtime_error("Mature study original128MiB budget required");
        initialResident=initial.residentMeshBytes;
        const auto root=engine.ExecutableDirectory()/"assets"/"mature-study";
        materials.Initialize(engine,root);
        for(unsigned slot=0;slot<3;++slot){const auto& data=Parts[slot];
            Aevrynd::MaterialTextureSetDefinition3D textures{data.textures[0],data.textures[1],1024,data.textures[2]};
            textures.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::PackedOrm;
            for(unsigned channel=0;channel<3;++channel)textures.integrity[channel]={data.sizes[channel],data.hashes[channel]};
            Aevrynd::Material3D material{{1,1,1,1},{.85f,0,0,1},true,{},1.f,true};
            material.doubleSided=true;
            if(slot==1)material.materialClass=Aevrynd::MaterialClass3D::Foliage;
            if(!materials.Load({data.name,textures,material}))throw std::runtime_error("Mature study PBR material unavailable");
            surfaces[slot]=*materials.Find(data.name);
            auto loaded=Aevrynd::LoadAevryndMeshFile(root/"meshes"/(std::string(data.name)+".aemesh"));
            if(!loaded)throw std::runtime_error("Mature study original full mesh unavailable");
            meshes[slot]=engine.CreateMeshResource3D(loaded.mesh);
            if(!meshes[slot]||!meshes[slot].cpuMesh){const auto stats=engine.GetRendererStats();throw std::runtime_error(std::string("Mature study GPU upload failed role=")+data.name+" resident="+std::to_string(stats.residentMeshBytes)+" budget="+std::to_string(stats.meshMemoryBudgetBytes)+" budget_rejections="+std::to_string(stats.meshBudgetRejections)+" bounds_rejections="+std::to_string(stats.invalidMeshBoundsRejections));}
        }
    }
    template<class Scene> void Draw(Scene& scene,const Aevrynd::Transform3D& original,const Aevrynd::BoxCollider3D& oldBody,std::span<const Aevrynd::BoxCollider3D> solids) {
        auto pose=original;pose.scale=pose.scale*.4f;
        const auto pivot=Aevrynd::TransformBounds({RootPivot,0,{0,0,0}},pose).center;
        pose.position=pose.position+(original.position-pivot);
        contact.Observe(original,pose,oldBody,solids);
        if(contactWorld)contactWorld->ObserveDraw(original,pose);
        for(unsigned slot=0;slot<3;++slot){
            if(!scene.Mesh(meshes[slot],pose,surfaces[slot]))throw std::runtime_error("Mature study actual scene insertion failed");
            ++submitted;
        }
    }
    void Shutdown(Aevrynd::Engine& engine,const std::filesystem::path& output,bool recordOriginalContact=true) {
        const auto stats=engine.GetRendererStats();fullSceneResident=stats.residentMeshBytes;
        if(stats.meshMemoryBudgetBytes!=128ull*1024*1024||stats.meshBudgetRejections||stats.residentMeshBytes>stats.meshMemoryBudgetBytes)throw std::runtime_error("Mature study full forest must retain original128MiB with zero allocation rejection");
        materials.Shutdown();unsigned destroyed{};
        for(auto& mesh:meshes){if(mesh){engine.DestroyMesh3D(mesh.handle);++destroyed;}mesh={};}
        if(destroyed!=3||materials.Stats().materials||!submitted)throw std::runtime_error("Mature study cleanup/submission incomplete");
        if(output.empty())throw std::runtime_error("Mature study is replay-only and unaccepted");
        std::ofstream receipt(output/"mature-study-cleanup.txt");
        receipt<<"appearance_only=1\nbody_camera_accepted=0\nsource_parts=3\ndestroyed=3\nsubmitted="<<submitted<<"\n";
        receipt<<"mesh_budget_bytes="<<stats.meshMemoryBudgetBytes<<"\nmesh_budget_rejections="<<stats.meshBudgetRejections<<"\ninitial_resident_bytes="<<initialResident<<"\nfull_scene_resident_bytes="<<fullSceneResident<<"\n";
        receipt.close();if(!receipt)throw std::runtime_error("Mature study cleanup receipt failed");
        if(recordOriginalContact)contact.Save(output);
    }
};
}
