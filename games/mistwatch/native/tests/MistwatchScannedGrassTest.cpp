#include "MistwatchScannedGrass.h"
#include "MistwatchWorld.h"
#include <Aevrynd/EnvironmentMeshes3D.h>
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/NativeMaterialDds3D.h>
#include <filesystem>
#include <iostream>
int main(){
    using namespace Aevrynd;
    const auto root=std::filesystem::path(MISTWATCH_SOURCE_ROOT)/"engine/assets/mistwatch-grass-medium-02";
    const auto definition=Mistwatch::GrassMaterialDefinition();
    if(definition.material.alphaMode!=MaterialAlphaMode3D::Mask||definition.material.alphaCutoff!=.5f||
        !definition.material.doubleSided||definition.material.materialClass!=MaterialClass3D::Foliage||
        !definition.material.useUv||definition.material.textureScale!=1.f||
        definition.textures.surfaceEncoding!=MaterialSurfaceMapEncoding3D::PackedOrm)return 1;
    constexpr const char* files[]{"grass-base.dds","grass-arm.dds","grass-normal.dds"};
    constexpr NativeMaterialDdsRole3D roles[]{NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsRole3D::Surface,NativeMaterialDdsRole3D::Normal};
    constexpr DXGI_FORMAT formats[]{DXGI_FORMAT_BC7_UNORM_SRGB,DXGI_FORMAT_BC7_UNORM,DXGI_FORMAT_BC5_UNORM};
    unsigned rejected{};
    for(unsigned i=0;i<3;++i){
        NativeMaterialDds3D image{};NativeMaterialDdsError3D error{};
        const auto& seal=definition.textures.integrity[i];
        const auto path=root/"materials"/files[i];
        if(!LoadNativeMaterialDds3D(path,roles[i],2048,false,image,&error,seal.expectedSha256,seal.expectedBytes,true)||
            image.mipCount!=12||image.format!=formats[i])return 2;
        if(LoadNativeMaterialDds3D(path,roles[i],2048,false,image,&error,std::string(64,'0'),seal.expectedBytes,true))return 3;
        ++rejected;
        if(LoadNativeMaterialDds3D(path,roles[i],2048,false,image,&error,seal.expectedSha256,seal.expectedBytes-1,true))return 4;
        ++rejected;
        if(LoadNativeMaterialDds3D(path,roles[i],1024,false,image,&error,seal.expectedSha256,seal.expectedBytes,true))return 5;
        ++rejected;
    }
    const auto procedural=CreateGrassClumpMesh3D(9,5,GrassBladeStyle3D::Curved);
    float oldRadius{};for(const auto& v:procedural.vertices)oldRadius=(std::max)(oldRadius,std::hypot(v.x,v.z));
    unsigned total{},rootVertices{},movingVertices{};
    for(unsigned variant=0;variant<5;++variant){
        auto loaded=LoadAevryndMeshFile(root/"meshes"/Mistwatch::GrassMeshName(variant));
        if(!loaded||loaded.mesh.vertices.size()!=Mistwatch::GrassVertexCounts[variant]||
            loaded.mesh.indices.size()!=3u*Mistwatch::GrassTriangleCounts[variant])return 6;
        const auto original=loaded.mesh;
        float originalHeight{};for(const auto& v:original.vertices)originalHeight=(std::max)(originalHeight,v.y);
        const float scale=Mistwatch::NormalizeGrassAsset(loaded.mesh);
        if(loaded.mesh.indices!=original.indices)return 7;
        float radius{},height{},minimumY=10.f;
        bool hasUv=false;
        for(std::size_t index=0;index<loaded.mesh.vertices.size();++index){
            const auto& a=original.vertices[index];const auto& b=loaded.mesh.vertices[index];
            if(a.x*scale!=b.x||a.y*scale!=b.y||a.z*scale!=b.z||
                (a.normal&0x00ffffffu)!=(b.normal&0x00ffffffu)||a.u!=b.u||a.v!=b.v||a.color!=b.color)return 8;
            const auto n=UnpackNormalSnorm8(b.normal);
            if(std::abs(Length({n.x,n.y,n.z})-1.f)>.02f||n.w<0.f||n.w>1.f||b.u<0.f||b.u>1.f||b.v<0.f||b.v>1.f)return 9;
            const float expected=1.f-(std::clamp)(a.y/originalHeight,0.f,1.f);
            if(std::abs(expected-n.w)>.008f)return 10;
            if(a.y<=0.f){if(n.w!=1.f)return 11;++rootVertices;}
            if(a.y>originalHeight*.95f){if(n.w>.06f)return 12;++movingVertices;}
            hasUv|=b.u!=0.f||b.v!=0.f;
            height=(std::max)(height,b.y);minimumY=(std::min)(minimumY,b.y);
            radius=(std::max)(radius,std::hypot(b.x,b.z));
        }
        std::cout<<"variant="<<variant<<" triangles="<<Mistwatch::GrassTriangleCounts[variant]<<" uniform_scale="<<scale
            <<" height="<<height<<" minimum_y="<<minimumY<<" radius="<<radius<<" procedural_radius="<<oldRadius<<'\n';
        if(!hasUv||height<Mistwatch::GrassAssetHeight-.00001f||height>1.2f||minimumY<-.03f||radius<oldRadius||radius>.95f)return 13;
        total+=unsigned(loaded.mesh.indices.size()/3);
    }
    if(total!=7842||!rootVertices||!movingVertices||!Mistwatch::GrassMeshName(5).empty())return 14;
    bool invalidRejected=false;try{Mesh3D invalid;Mistwatch::NormalizeGrassAsset(invalid);}catch(const std::runtime_error&){invalidRejected=true;}
    if(!invalidRejected)return 15;
    const auto terrain=Mistwatch::CreateTerrain();
    const auto roots=Mistwatch::GroundCoverPositions(terrain);
    if(roots.size()!=1363)return 16;
    for(const auto p:roots)if(std::abs(p.y-terrain.SampleHeight(p.x,p.z))>.0001f)return 17;
    std::cout<<"Scanned grass PASS; variants=5 roots="<<roots.size()<<" source_triangles="<<total
        <<" fixed_root_vertices="<<rootVertices<<" moving_tip_vertices="<<movingVertices<<" hostile_rejections="<<rejected+1<<'\n';
}
