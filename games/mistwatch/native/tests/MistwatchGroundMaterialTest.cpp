#include "MistwatchGroundMaterial.h"
#include <Aevrynd/NativeMaterialDds3D.h>
#include <Aevrynd/PbrMaterialCooker3D.h>
#include <iostream>
int main() {
    using namespace Aevrynd;
    const auto definition=Mistwatch::GroundMaterialDefinition();
    const auto legacy=Mistwatch::GroundMaterialDefinition(true);
    if(definition.name!="ground"||definition.textures.maximumResolution!=2048||
        definition.textures.surfaceEncoding!=MaterialSurfaceMapEncoding3D::PackedOrm||
        definition.material.textureScale!=.25f||definition.material.normalScale!=.65f||
        definition.material.useUv||definition.material.castsShadow||
        definition.material.materialClass!=MaterialClass3D::Surface)return 1;
    if(legacy.material.textureScale!=definition.material.textureScale||legacy.material.normalScale!=definition.material.normalScale||
        legacy.material.tint.r!=definition.material.tint.r||legacy.material.tint.g!=definition.material.tint.g||
        legacy.material.tint.b!=definition.material.tint.b||legacy.material.tint.a!=definition.material.tint.a||
        legacy.material.surface.roughness!=definition.material.surface.roughness||legacy.material.surface.metallic!=definition.material.surface.metallic||
        legacy.material.surface.emissive!=definition.material.surface.emissive||legacy.material.surface.occlusion!=definition.material.surface.occlusion||
        legacy.textures.maximumResolution!=1024||legacy.textures.surfaceEncoding!=MaterialSurfaceMapEncoding3D::RoughnessR)return 2;
    const auto root=std::filesystem::path(MISTWATCH_SOURCE_ROOT)/"engine/assets/realistic-cc0-first-batch/cooked/materials/forest-ground-04";
    constexpr const char* files[]{"slot-0-base.dds","slot-0-arm.dds","slot-0-normal.dds"};
    constexpr NativeMaterialDdsRole3D roles[]{NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsRole3D::Surface,NativeMaterialDdsRole3D::Normal};
    constexpr DXGI_FORMAT formats[]{DXGI_FORMAT_BC7_UNORM_SRGB,DXGI_FORMAT_BC7_UNORM,DXGI_FORMAT_BC5_UNORM};
    unsigned rejected{};
    for(unsigned i=0;i<3;++i) {
        const auto& seal=definition.textures.integrity[i];
        NativeMaterialDds3D image{};NativeMaterialDdsError3D error{};
        if(!LoadNativeMaterialDds3D(root/files[i],roles[i],2048,false,image,&error,seal.expectedSha256,seal.expectedBytes,true)||
            image.width!=2048||image.height!=2048||image.mipCount!=12||image.format!=formats[i]||image.sha256!=seal.expectedSha256) {
            std::cerr<<"FIRST_DIVERGENCE: forest floor DDS channel="<<i<<" error="<<unsigned(error)<<'\n';return 3;
        }
        if(LoadNativeMaterialDds3D(root/files[i],roles[i],2048,false,image,&error,std::string(64,'0'),seal.expectedBytes,true)||error!=NativeMaterialDdsError3D::HashMismatch)return 4;
        ++rejected;
        if(LoadNativeMaterialDds3D(root/files[i],roles[i],2048,false,image,&error,seal.expectedSha256,seal.expectedBytes-1,true)||error!=NativeMaterialDdsError3D::HashMismatch)return 5;
        ++rejected;
        if(LoadNativeMaterialDds3D(root/files[i],roles[i],1024,false,image,&error,seal.expectedSha256,seal.expectedBytes,true))return 6;
        ++rejected;
    }
    NativeMaterialDds3D invalid{};
    if(LoadNativeMaterialDds3D(root/files[1],roles[0],2048,false,invalid)||LoadNativeMaterialDds3D(root/files[0],roles[2],2048,false,invalid))return 7;
    rejected+=2;
    for(unsigned flags=1;flags<=3;++flags) {
        const auto neutral=Mistwatch::GroundDiagnosticDefinition(false,(flags&1)!=0,(flags&2)!=0);
        if(neutral.textures.normalPath!=definition.textures.normalPath||
            neutral.textures.integrity[2].expectedSha256!=definition.textures.integrity[2].expectedSha256||
            neutral.textures.surfaceEncoding!=((flags&2)?MaterialSurfaceMapEncoding3D::RoughnessR:MaterialSurfaceMapEncoding3D::PackedOrm))return 8;
        for(unsigned i=0;i<2;++i) {
            const bool changed=(flags&(1u<<i))!=0;
            if(changed?(!neutral.textures.integrity[i].expectedSha256.empty()||neutral.textures.integrity[i].expectedBytes):
                neutral.textures.integrity[i].expectedSha256!=definition.textures.integrity[i].expectedSha256)return 9;
        }
    }
    static_assert(CanonicalMaterialSurfaceTexel3D({17,83,5,241},MaterialSurfaceMapEncoding3D::PackedOrm)==std::array<std::uint8_t,4>{17,83,5,83});
    std::cout<<"Forest floor DDS PASS; channels=3 hostile rejections="<<rejected<<" projection=triplanar scale=.25 normal=.65\n";
}
