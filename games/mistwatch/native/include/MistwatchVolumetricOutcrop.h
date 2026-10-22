#pragma once
#include "MistwatchBoulderDetail.h"

namespace Mistwatch::VolumetricOutcrop {
inline constexpr std::uint64_t InstanceNamespace=0x4d574f5554435231ull;
// 隔離岩體原型；新土坡模式附保守盒與植被重排，尚未全面接觸驗收，不作正式世界使用。
class Resources {
public:
    Aevrynd::MeshResource3D mesh;
    Aevrynd::Material3D material;
    std::array<Aevrynd::Transform3D,3> poses{};
    std::array<std::uint64_t,3> observed{};
    bool jointBankWorld{};
    bool playableContext{};
    unsigned acquired{},released{};std::uint64_t submittedFrames{};
    void Initialize(Aevrynd::Engine& e,const Aevrynd::HeightField3D& terrain,const Aevrynd::Material3D& selected,bool upstreamBank=false,bool playable=false){
        using namespace Aevrynd;
        if(mesh||acquired||!selected.textures||!selected.useUv||
            WestCliffRelief::Fingerprint(terrain)!=16546567984029864090ull)
            throw std::runtime_error("Outcrop requires actual v9 terrain and source PBR material");
        constexpr std::array<Vec3,3> anchors{{{-34,0,58},{-39,0,63},{-28,0,59}}};
        constexpr std::array<float,3> scales{12,9,7},yawDegrees{31,-28,112},burial{2.4f,1.5f,1.f};
        jointBankWorld=upstreamBank;
        playableContext=playable;
        for(unsigned i=0;i<3;++i){auto p=anchors[i];p.y=terrain.SampleHeight(p.x,p.z)-burial[i];
            if(upstreamBank){p.x-=2.f;p.z+=4.f;}
            poses[i]={p,{0,yawDegrees[i]*3.14159265358979323846f/180.f,0},{scales[i],scales[i],scales[i]}};}
        auto source=BoulderDetail::LoadBoulderDetailMesh(e.ExecutableDirectory()/"assets/mistwatch-boulders/meshes"/BoulderDetail::MeshName);
        material=selected;mesh=e.CreateMeshResource3D(source);
        if(!mesh)throw std::runtime_error("Outcrop GPU acquisition failed");++acquired;
        if(!mesh.cpuMesh||mesh.cpuMesh->indices!=source.indices||mesh.cpuMesh->vertices.size()!=source.vertices.size()||
            std::memcmp(mesh.cpuMesh->vertices.data(),source.vertices.data(),source.vertices.size()*sizeof(Vertex3D)))
            throw std::runtime_error("Outcrop actual GPU source mismatch");
    }
    void Submit(Aevrynd::Scene3D& scene){
        if(!mesh)throw std::runtime_error("Outcrop missing prepared resource");
        for(unsigned i=0;i<3;++i)if(!scene.Mesh(mesh,poses[i],material,{},{{InstanceNamespace,i+1},1}))
            throw std::runtime_error("Outcrop scene submission failed");
        ++submittedFrames;
    }
    void Observe(std::span<const Aevrynd::MeshInstance3D> items){
        std::array<bool,3> seen{};
        for(const auto& item:items)if(item.identity.id.high==InstanceNamespace){
            if(item.identity.id.low<1||item.identity.id.low>3||item.identity.generation!=1)
                throw std::runtime_error("Outcrop actual identity mismatch");
            const auto i=unsigned(item.identity.id.low-1);const auto matrix=poses[i].Matrix();
            if(seen[i]||!LandscapeTwigDraw::ExactResourceBindings(item,mesh.handle,material.textures)||
                item.tint!=Aevrynd::PackColorRgba8(material.tint)||item.surface!=Aevrynd::PackSurface3D(material.surface)||
                item.useUv!=material.useUv||item.textureScale!=material.textureScale||item.normalScale!=material.normalScale)
                throw std::runtime_error("Outcrop actual resource/material mismatch");
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                if(std::bit_cast<std::uint32_t>(matrix[r][c])!=std::bit_cast<std::uint32_t>(item.transform[r][c]))
                    throw std::runtime_error("Outcrop actual world transform mismatch");
            seen[i]=true;++observed[i];
        }
    }
    void Release(Aevrynd::Engine& e){if(mesh){e.DestroyMesh3D(mesh.handle);mesh={};++released;}}
    void Write(const std::filesystem::path& folder)const{
        const auto path=folder/"volumetric-outcrop.txt";if(std::filesystem::exists(path))throw std::runtime_error("Outcrop receipt refuses overwrite");
        std::ofstream out(path);out.precision(9);
        out<<"profile=three-rock-visual-v1\nrender_preview_only="<<!playableContext<<"\ncollision_integrated="<<jointBankWorld
            <<"\nvegetation_replanned="<<jointBankWorld<<"\nmanual_save_allowed="<<playableContext<<"\nart_accepted=0\n"
            <<"collision_representation="<<(jointBankWorld?"three-conservative-source-AABBs":"none")<<'\n'
            <<"source_sha="<<BoulderDetail::MeshSha<<"\nsource_vertices="<<BoulderDetail::VertexCount<<"\nsource_triangles="<<BoulderDetail::IndexCount/3
            <<"\ninstances=3\nsubmitted_frames="<<submittedFrames<<"\nacquired="<<acquired<<"\nreleased="<<released
            <<"\nmaterial_texture="<<material.textures.semanticIdentity<<'\n';
        for(unsigned i=0;i<3;++i)out<<"observed_"<<i<<'='<<observed[i]<<'\n';
        out.close();if(!out)throw std::runtime_error("Outcrop receipt write failed");
    }
};
}
