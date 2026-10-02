#include "MistwatchFern.h"
#include "MistwatchWorld.h"
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/NativeMaterialDds3D.h>
#include <filesystem>
#include <iostream>
int main() {
    using namespace Aevrynd;
    const auto root=std::filesystem::path(MISTWATCH_SOURCE_ROOT)/"engine/assets/mistwatch-fern-02";
    const auto definition=Mistwatch::FernMaterialDefinition();
    if(definition.material.alphaMode!=MaterialAlphaMode3D::Mask||definition.material.alphaCutoff!=.5f||
        !definition.material.doubleSided||definition.material.materialClass!=MaterialClass3D::Foliage||
        !definition.material.useUv||definition.material.textureScale!=1.f||
        definition.textures.surfaceEncoding!=MaterialSurfaceMapEncoding3D::PackedOrm||
        definition.textures.maximumResolution!=2048||definition.textures.alphaMode!=MaterialAlphaMode3D::Mask)return 1;
    constexpr const char* files[]{"fern-base.dds","fern-arm.dds","fern-normal.dds"};
    constexpr NativeMaterialDdsRole3D roles[]{NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsRole3D::Surface,NativeMaterialDdsRole3D::Normal};
    constexpr DXGI_FORMAT formats[]{DXGI_FORMAT_BC7_UNORM_SRGB,DXGI_FORMAT_BC7_UNORM,DXGI_FORMAT_BC5_UNORM};
    unsigned rejected{};
    for(unsigned channel=0;channel<3;++channel) {
        const auto& seal=definition.textures.integrity[channel];
        NativeMaterialDds3D image{};NativeMaterialDdsError3D error{};
        const auto path=root/"materials"/files[channel];
        if(!LoadNativeMaterialDds3D(path,roles[channel],2048,false,image,&error,seal.expectedSha256,seal.expectedBytes,true)||
            image.width!=2048||image.height!=2048||image.mipCount!=12||image.format!=formats[channel]||image.sha256!=seal.expectedSha256)return 11;
        if(LoadNativeMaterialDds3D(path,roles[channel],2048,false,image,&error,std::string(64,'0'),seal.expectedBytes,true)||
            error!=NativeMaterialDdsError3D::HashMismatch)return 12;
        ++rejected;
        if(LoadNativeMaterialDds3D(path,roles[channel],2048,false,image,&error,seal.expectedSha256,seal.expectedBytes-1,true)||
            error!=NativeMaterialDdsError3D::HashMismatch)return 13;
        ++rejected;
        if(LoadNativeMaterialDds3D(path,roles[channel],1024,false,image,&error,seal.expectedSha256,seal.expectedBytes,true))return 14;
        ++rejected;
    }
    NativeMaterialDds3D invalid{};
    if(LoadNativeMaterialDds3D(root/"materials"/files[1],roles[0],2048,false,invalid)||
        LoadNativeMaterialDds3D(root/"materials"/files[0],roles[2],2048,false,invalid))return 15;
    rejected+=2;
    const auto terrain=Mistwatch::CreateTerrain();
    const auto trees=Mistwatch::TreePositions();
    const auto props=Mistwatch::ForestProps(terrain,trees);
    const auto plants=Mistwatch::CreateUnderstory(terrain,trees,props);
    if(plants.ferns.size()!=420)return 2;
    std::array<float,4> radii{};
    std::size_t triangles{};
    for(unsigned variant=0;variant<4;++variant) {
        auto loaded=LoadAevryndMeshFile(root/"meshes"/Mistwatch::FernMeshName(variant));
        if(!loaded||loaded.mesh.vertices.size()!=Mistwatch::FernVertexCounts[variant]||
            loaded.mesh.indices.size()!=3u*Mistwatch::FernTriangleCounts[variant])return 3;
        const auto original=loaded.mesh;
        Mistwatch::NormalizeFernAsset(loaded.mesh);
        if(original.indices!=loaded.mesh.indices)return 4;
        float minimumY=100,maximumY=-100;
        bool nonzeroUv=false;
        for(std::size_t index=0;index<loaded.mesh.vertices.size();++index) {
            const auto& a=original.vertices[index];const auto& b=loaded.mesh.vertices[index];
            if(b.x!=a.x*Mistwatch::FernAssetScale||b.y!=a.y*Mistwatch::FernAssetScale||
                b.z!=a.z*Mistwatch::FernAssetScale||a.normal!=b.normal||a.color!=b.color||a.u!=b.u||a.v!=b.v)return 5;
            const auto normal=UnpackNormalSnorm8(b.normal);
            const float length=Length(Vec3{normal.x,normal.y,normal.z});
            if(length<.95f||length>1.05f||normal.w!=1.f||b.u<0||b.u>1||b.v<0||b.v>1)return 6;
            nonzeroUv|=b.u!=0||b.v!=0;
            minimumY=(std::min)(minimumY,b.y);maximumY=(std::max)(maximumY,b.y);
            radii[variant]=(std::max)(radii[variant],std::hypot(b.x,b.z));
        }
        // 原 local pivot 保留小量負 Y；不得用最低葉卡改變整叢接地高度。
        constexpr std::array<float,4> sourceMinimumY{-.0305102803f,-.0287736729f,-.0287735183f,-.0135789448f};
        constexpr std::array<float,4> sourceMaximumY{.397184432f,.320246696f,.258030027f,.199508607f};
        if(!nonzeroUv||std::abs(minimumY-sourceMinimumY[variant]*Mistwatch::FernAssetScale)>.00001f||
            std::abs(maximumY-sourceMaximumY[variant]*Mistwatch::FernAssetScale)>.00001f)return 7;
        triangles+=loaded.mesh.indices.size()/3;
    }
    if(triangles!=6232||!Mistwatch::FernMeshName(4).empty())return 8;
    std::array<unsigned,4> counts{};
    float worldRadius{};
    for(std::size_t index=0;index<plants.ferns.size();++index) {
        const unsigned variant=unsigned(index%4);++counts[variant];
        const auto& plant=plants.ferns[index];
        const float radius=radii[variant]*plant.scale.x;
        worldRadius=(std::max)(worldRadius,radius);
        if(radius>.9f||!Mistwatch::PropClearsObjectives({plant.position,{radius,1,radius}})||
            std::abs(plant.position.y-terrain.SampleHeight(plant.position.x,plant.position.z))>.0001f) {
            std::cerr<<"Fern footprint failed index="<<index<<" radius="<<radius<<'\n';return 9;
        }
    }
    for(const auto count:counts)if(count!=105)return 10;
    std::cout<<"Fern PASS; variants=4 source triangles="<<triangles<<" roots="<<plants.ferns.size()
        <<" maximum world radius="<<worldRadius<<" asset scale="<<Mistwatch::FernAssetScale
        <<" sealed DDS channels=3 hostile rejections="<<rejected<<'\n';
}
