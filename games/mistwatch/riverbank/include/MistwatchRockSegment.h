#pragma once
#include "MistwatchShoulderGroundcover.h"
#include <Aevrynd/CookedRenderManifest3D.h>
#include <iomanip>

namespace Mistwatch::RockSegment {
// 本輪僅固定 LOD1 實機美術驗收；碰撞與動態 LOD 尚未接入遊戲。
struct Recipe { const char* name; const char* sha; std::size_t bytes,vertices,indices; };
#include "MistwatchRockSegmentData.inl"
inline constexpr std::uint64_t InstanceNamespace=0x4d57524f434b3031ull;
inline Aevrynd::Mesh3D Load(const std::filesystem::path& path,const Recipe& p) {
    struct File { HANDLE h{INVALID_HANDLE_VALUE};~File(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);} };
    File file{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    LARGE_INTEGER size{};
    if(file.h==INVALID_HANDLE_VALUE||!GetFileSizeEx(file.h,&size)||size.QuadPart!=static_cast<LONGLONG>(p.bytes))
        throw std::runtime_error("Rock segment source size mismatch");
    std::vector<std::byte> raw(p.bytes);DWORD count{};
    if(!ReadFile(file.h,raw.data(),static_cast<DWORD>(raw.size()),&count,nullptr)||count!=raw.size()||
       !FallenLogAssets::ExactHash(raw,p.sha))throw std::runtime_error("Rock segment source SHA mismatch");
    auto loaded=Aevrynd::LoadAevryndMeshFile(path);
    if(!loaded||loaded.mesh.vertices.size()!=p.vertices||loaded.mesh.indices.size()!=p.indices||
       raw.size()!=16+p.vertices*28+p.indices*4||
       std::memcmp(raw.data()+16,loaded.mesh.vertices.data(),p.vertices*28)||
       std::memcmp(raw.data()+16+p.vertices*28,loaded.mesh.indices.data(),p.indices*4))
        throw std::runtime_error("Rock segment decoded mesh differs from authored mesh");
    return std::move(loaded.mesh);
}
inline Aevrynd::MaterialAssetDefinition3D MaterialDefinition() {
    Aevrynd::CookedRenderMaterial3D m;
    m.id="coastal_cliff_01:slot-0";
    m.baseColorPath="materials/coastal-cliff-01/slot-0-base.dds";
    m.surfacePath="materials/coastal-cliff-01/slot-0-arm.dds";
    m.normalPath="materials/coastal-cliff-01/slot-0-normal.dds";
    m.baseColorBytes=m.surfaceBytes=m.normalBytes=1398276;
    m.baseColorSha256="410dd7ad59fac581d97c3d11dca6117c4697f8abcb32c5478085421b2585a38c";
    m.surfaceSha256="a9262b140284f58b1574e661c842d026bf76a5596ef561f70b1dfddd34080e02";
    m.normalSha256="93c7e9da6acb5d9237e322b15aae6e907c7b65ef37fd698bbf103a31a12bd09f";
    auto definition=m.Definition(1024);
    // 岩石非金屬；粗糙度使用來源 ORM 原值，不再乘預設 .72。
    definition.material.surface.roughness=1.f;
    return definition;
}
struct Frame { std::uint64_t cameraHash{},serial{};unsigned draws{},cullReason{};bool shadowOnly{}; };
class Resources {
public:
    Aevrynd::MeshResource3D mesh;
    Aevrynd::MaterialLibrary3D materials;
    Aevrynd::Material3D material;
    std::uint64_t fingerprint{};unsigned acquired{},released{};bool releaseFailed{};
    void Initialize(Aevrynd::Engine& e) {
        if(mesh||materials.Initialized())throw std::runtime_error("Rock segment duplicate initialization");
        const auto root=e.ExecutableDirectory()/"assets";
        try {
            // 此候選的粗 LOD 已失敗；只拍已產出的 LOD1，禁止冒稱整套資產合格。
            auto selected=Load(root/"mistwatch-cliff-m3042"/SelectedRecipe.name,SelectedRecipe);
            fingerprint=ShoulderGrove::Detail::MeshFingerprint(selected);
            if(!materials.Initialize(e,root/"photoreal-cooked")||!materials.Load(MaterialDefinition()))
                throw std::runtime_error("Rock segment material load failed");
            const auto* found=materials.Find("coastal_cliff_01:slot-0");
            if(!found)throw std::runtime_error("Rock segment material missing");material=*found;
            mesh=e.CreateMeshResource3D(selected);if(!mesh)throw std::runtime_error("Rock segment upload failed");++acquired;
            if(!mesh.cpuMesh||ShoulderGrove::Detail::MeshFingerprint(*mesh.cpuMesh)!=fingerprint)
                throw std::runtime_error("Rock segment uploaded mesh mismatch");
        }catch(...){Release(e);throw;}
    }
    void Release(Aevrynd::Engine& e)noexcept {
        if(mesh)try{e.DestroyMesh3D(mesh.handle);mesh={};++released;}catch(...){releaseFailed=true;}
        try{materials.Shutdown();}catch(...){releaseFailed=true;}
    }
    void Submit(Aevrynd::Scene3D& scene)const {
        if(!mesh||!scene.Mesh(mesh,Pose,material,{},{{InstanceNamespace,1},1}))
            throw std::runtime_error("Rock segment scene submission failed");
    }
    Frame Observe(std::span<const Aevrynd::MeshInstance3D> items,const Aevrynd::Camera3D& camera,
                  const Aevrynd::Scene3DSubmissionOptions& options,std::uint64_t cameraHash,std::uint64_t serial)const {
        using namespace Aevrynd;Frame frame{cameraHash,serial};
        const auto matrix=Pose.Matrix(),normal=CreateNormalMatrix(matrix);
        for(const auto& item:items)if(item.identity.id.high==InstanceNamespace){
            if(!mesh||++frame.draws!=1||item.identity.id.low!=1||item.identity.generation!=1||
               item.identity.hlodClusterId||item.identity.sharedLodDither||item.identity.hlodProxy||
               !LandscapeTwigDraw::ExactResourceBindings(item,mesh.handle,material.textures)||
               item.tint!=PackColorRgba8(material.tint)||item.surface!=PackSurface3D(material.surface)||
               item.useUv!=material.useUv||item.textureScale!=material.textureScale||item.normalScale!=material.normalScale||
               item.materialClass!=material.materialClass||item.alphaMode!=material.alphaMode||item.doubleSided!=material.doubleSided||
               item.alphaCutoff!=Clamp(material.alphaCutoff,0.f,1.f)||item.occlusionOpacity!=1.f||
               item.lodCoverage!=1.f||item.lodSeed!=0||item.motionValid)
                throw std::runtime_error("Rock segment actual draw resource/material mismatch");
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                if(std::bit_cast<std::uint32_t>(item.transform[r][c])!=std::bit_cast<std::uint32_t>(matrix[r][c])||
                   std::bit_cast<std::uint32_t>(item.previousTransform[r][c])!=std::bit_cast<std::uint32_t>(matrix[r][c])||
                   std::bit_cast<std::uint32_t>(item.normalTransform[r][c])!=std::bit_cast<std::uint32_t>(normal[r][c]))
                    throw std::runtime_error("Rock segment actual draw transform mismatch");
            if(item.layer!=MeshRenderLayer3D::Opaque&&item.layer!=MeshRenderLayer3D::ShadowOnly)
                throw std::runtime_error("Rock segment invalid draw layer");
            if((item.castsShadow&&!material.castsShadow)||(item.layer==MeshRenderLayer3D::ShadowOnly&&!item.castsShadow)||
               (item.castsShadow?item.shadowCascadeMask==0:item.shadowCascadeMask!=0)||
               (item.shadowCascadeMask&~DirectionalShadowAllCascades3D)!=0||
               item.shadowCascadeSelection!=ShadowCascadeSelection3D::Automatic)
                throw std::runtime_error("Rock segment shadow binding mismatch");
            frame.shadowOnly=item.layer==MeshRenderLayer3D::ShadowOnly;
        }
        if(mesh&&!frame.draws){frame.cullReason=ShoulderGroundcover::CullReason({mesh,Pose,material},camera,options);
            if(!frame.cullReason)throw std::runtime_error("Rock segment visible draw missing");}
        return frame;
    }
    void Write(const std::filesystem::path& base,const Frame& frame,bool enabled)const {
        auto path=base;path+=L".rock-segment.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Rock segment receipt refuses overwrite");
        std::ofstream out(path);out<<"schema=rock-segment-v1\nprofile="<<(enabled?"edge-shear-v1":"original")
            <<"\ncamera_hash="<<frame.cameraHash<<"\nscene_serial="<<frame.serial
            <<"\nart_accepted=0\ncollision_integrated=0\nlod=1\nmesh_fnv="<<fingerprint
            <<"\nvertices="<<mesh.vertexCount<<"\nindices="<<mesh.indexCount<<"\ndraws="<<frame.draws
            <<"\ncull_reason="<<frame.cullReason<<"\nshadow_only="<<frame.shadowOnly
            <<"\nmaterial_verified="<<(frame.draws!=0)<<"\ntransform_verified="<<(frame.draws!=0)<<'\n';
        out.close();if(!out)throw std::runtime_error("Rock segment receipt write failed");
    }
    void WriteCleanup(const std::filesystem::path& base)const {
        auto path=base;path+=L".rock-segment-cleanup.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Rock segment cleanup refuses overwrite");
        std::ofstream out(path);out<<"acquired="<<acquired<<"\nreleased="<<released<<"\nremaining="<<bool(mesh)
            <<"\nmaterials_remaining="<<materials.Stats().materials<<"\nrelease_failed="<<releaseFailed<<'\n';
        out.close();if(!out||releaseFailed||mesh||materials.Stats().materials||acquired!=released)
            throw std::runtime_error("Rock segment cleanup incomplete");
    }
};
}
