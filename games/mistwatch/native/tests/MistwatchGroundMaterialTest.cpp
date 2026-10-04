#include "MistwatchGroundMaterial.h"
#include <Aevrynd/NativeMaterialDds3D.h>
#include <Aevrynd/PbrMaterialCooker3D.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <fstream>
#include <iostream>
namespace {
using namespace Aevrynd;
std::array<MaterialTextureLayerDefinition3D,3> Layers(const MaterialTextureSetDefinition3D& value) {
    return {MaterialTextureLayerDefinition3D{value.baseColorPath,value.roughnessPath,value.normalPath,value.surfaceEncoding,value.integrity},
        value.additionalLayers[0],value.additionalLayers[1]};
}
std::array<std::filesystem::path,3> Paths(const MaterialTextureLayerDefinition3D& value) {
    return {value.baseColorPath,value.roughnessPath,value.normalPath};
}
bool SameSeal(const MaterialTextureIntegrity3D& a,const MaterialTextureIntegrity3D& b) {
    return a.expectedBytes==b.expectedBytes&&a.expectedSha256==b.expectedSha256;
}
bool SameLayer(const MaterialTextureLayerDefinition3D& a,const MaterialTextureLayerDefinition3D& b) {
    if(Paths(a)!=Paths(b)||a.surfaceEncoding!=b.surfaceEncoding)return false;
    for(unsigned channel=0;channel<3;++channel)if(!SameSeal(a.integrity[channel],b.integrity[channel]))return false;
    return true;
}
bool SameMaterial(const Material3D& a,const Material3D& b) {
    return a.tint.r==b.tint.r&&a.tint.g==b.tint.g&&a.tint.b==b.tint.b&&a.tint.a==b.tint.a&&
        a.surface.roughness==b.surface.roughness&&a.surface.metallic==b.surface.metallic&&
        a.surface.emissive==b.surface.emissive&&a.surface.occlusion==b.surface.occlusion&&
        a.castsShadow==b.castsShadow&&a.textures==b.textures&&a.textureScale==b.textureScale&&
        a.useUv==b.useUv&&a.materialClass==b.materialClass&&a.shadingModel==b.shadingModel&&
        a.alphaMode==b.alphaMode&&a.alphaCutoff==b.alphaCutoff&&a.doubleSided==b.doubleSided&&
        a.normalScale==b.normalScale&&a.baseTextureScale==b.baseTextureScale&&a.detailTextureScale==b.detailTextureScale;
}
bool VerifySeal(const std::filesystem::path& path,const MaterialTextureIntegrity3D& seal) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input)return false;
    const auto length=input.tellg();if(length<0||std::uint64_t(length)!=seal.expectedBytes)return false;
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));input.seekg(0);
    if(length&&!input.read(reinterpret_cast<char*>(bytes.data()),length))return false;
    std::array<std::byte,32> digest{};if(!WorldDeltaSha2563D(bytes,digest))return false;
    constexpr char hex[]="0123456789abcdef";std::string actual;actual.reserve(64);
    for(const auto value:digest){const auto digit=std::to_integer<unsigned>(value);actual.push_back(hex[digit>>4]);actual.push_back(hex[digit&15]);}
    return actual==seal.expectedSha256;
}
}
int main() {
    using namespace Aevrynd;
    const auto definition=Mistwatch::GroundMaterialDefinition();
    const auto legacy=Mistwatch::GroundMaterialDefinition(true);
    const auto single=Mistwatch::GroundMaterialDefinition(false,true);
    if(definition.name!="ground"||definition.textures.maximumResolution!=2048||
        definition.textures.surfaceEncoding!=MaterialSurfaceMapEncoding3D::PackedOrm||
        definition.material.textureScale!=.25f||definition.material.normalScale!=.65f||
        definition.material.useUv||definition.material.castsShadow||
        definition.material.materialClass!=MaterialClass3D::Terrain)return 1;
    if(legacy.material.textureScale!=definition.material.textureScale||legacy.material.normalScale!=definition.material.normalScale||
        legacy.material.tint.r!=definition.material.tint.r||legacy.material.tint.g!=definition.material.tint.g||
        legacy.material.tint.b!=definition.material.tint.b||legacy.material.tint.a!=definition.material.tint.a||
        legacy.material.surface.roughness!=definition.material.surface.roughness||legacy.material.surface.metallic!=definition.material.surface.metallic||
        legacy.material.surface.emissive!=definition.material.surface.emissive||legacy.material.surface.occlusion!=definition.material.surface.occlusion||
        legacy.textures.maximumResolution!=1024||legacy.textures.surfaceEncoding!=MaterialSurfaceMapEncoding3D::RoughnessR)return 2;
    Material3D previous{{1,1,1,1},{.92f,0,0,.8f},false,{},.25f};previous.normalScale=.65f;
    auto terrainExpected=previous;terrainExpected.materialClass=MaterialClass3D::Terrain;
    if(!SameMaterial(single.material,previous)||!SameMaterial(legacy.material,previous)||
        !SameMaterial(definition.material,terrainExpected)||single.name!=definition.name||
        single.textures.maximumResolution!=2048||single.textures.alphaMode!=MaterialAlphaMode3D::Opaque||
        single.textures.alphaCutoff!=.5f||definition.textures.alphaMode!=single.textures.alphaMode||
        definition.textures.alphaCutoff!=single.textures.alphaCutoff||
        !SameLayer(Layers(single.textures)[0],Layers(definition.textures)[0]))return 10;
    for(const auto& baseline:{single,legacy})for(const auto& layer:baseline.textures.additionalLayers)
        if(!SameLayer(layer,MaterialTextureLayerDefinition3D{}))return 11;
    const auto legacyPriority=Mistwatch::GroundMaterialDefinition(true,true);
    if(!SameMaterial(legacyPriority.material,legacy.material)||legacyPriority.textures.maximumResolution!=1024||
        legacyPriority.textures.alphaMode!=legacy.textures.alphaMode||legacyPriority.textures.alphaCutoff!=legacy.textures.alphaCutoff)return 12;
    for(unsigned layer=0;layer<3;++layer)if(!SameLayer(Layers(legacyPriority.textures)[layer],Layers(legacy.textures)[layer]))return 12;
    const MaterialTextureLayerDefinition3D expectedLegacy{
        "ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg","ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",
        "ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"};
    if(!SameLayer(Layers(legacy.textures)[0],expectedLegacy)||legacy.name!="ground"||
        legacy.textures.alphaMode!=MaterialAlphaMode3D::Opaque||legacy.textures.alphaCutoff!=.5f)return 12;
    const auto layers=Layers(definition.textures);
    constexpr const char* expectedPaths[3][3]{
        {"poly-haven/forest-ground-04/slot-0-base.dds","poly-haven/forest-ground-04/slot-0-arm.dds","poly-haven/forest-ground-04/slot-0-normal.dds"},
        {"ambientcg/ground108/slot-0-base.dds","ambientcg/ground108/slot-0-arm.dds","ambientcg/ground108/slot-0-normal.dds"},
        {"poly-haven/rock_surface/1k/base-color.jpg","poly-haven/rock_surface/1k/roughness.jpg","poly-haven/rock_surface/1k/normal-dx.jpg"}};
    constexpr std::uint64_t expectedBytes[3][3]{{5592580,5592580,5592580},{1398276,1398276,1398276},{886291,642680,1161843}};
    constexpr const char* expectedHashes[3][3]{
        {"00850c7c033be2bcb2818c0ab3db91a62c8353eba746daede3c0de373c3e32b4","3427bf0a83e0c33326a207f700305620402fd0fb6c532240f204bcebefeabcc4","56e687b2a208f2eaaf9b0e3670530f0a36e5e88d6e74b26090633daaa4e9db93"},
        {"2703b20ee77d556daa64535af15433bb3157083174e6d71671aa052ccae11589","ce0c636264a9c1c5cb5055259dde80b92c9bea3145dc8ea8f28a7f9f3903e5ed","e63efd69f4d1e7978f1cd152c5b2115151bcbc208f9f661bc35af0ec6af80032"},
        {"07edda2bbe4715b01aae2e4bac9f21f204cf4aae37b4733ef01de33281e5e60e","ca710d895b01cf323bf00f53194c43504a39d2c964fbfc343c4f45c7d41b10dc","1e1aa5b06f74d22734318cd93ecf49ffe7095c4b2307c5c9b3a4550207c6b1b7"}};
    constexpr MaterialSurfaceMapEncoding3D encodings[]{MaterialSurfaceMapEncoding3D::PackedOrm,MaterialSurfaceMapEncoding3D::PackedOrm,MaterialSurfaceMapEncoding3D::RoughnessR};
    const auto source=std::filesystem::path(MISTWATCH_SOURCE_ROOT);
    const std::array sourceDirectories{source/"engine/assets/realistic-cc0-first-batch/cooked/materials/forest-ground-04",
        source/"engine/assets/realistic-cc0-first-batch/cooked/materials/ground108",source/"engine/assets/material-packs/poly-haven/rock_surface/1k"};
    unsigned verified{};
    for(unsigned layer=0;layer<3;++layer){
        if(layers[layer].surfaceEncoding!=encodings[layer])return 13;
        const auto paths=Paths(layers[layer]);
        for(unsigned channel=0;channel<3;++channel){const auto& seal=layers[layer].integrity[channel];
            if(paths[channel]!=expectedPaths[layer][channel]||seal.expectedBytes!=expectedBytes[layer][channel]||
                seal.expectedSha256!=expectedHashes[layer][channel]||!VerifySeal(sourceDirectories[layer]/paths[channel].filename(),seal)){
                std::cerr<<"FIRST_DIVERGENCE: sealed terrain layer="<<layer<<" channel="<<channel<<'\n';return 14;}
            ++verified;
        }
    }
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
    if(rejected!=11)return 15;
    unsigned soilRejected{};
    for(unsigned channel=0;channel<3;++channel){const auto& seal=layers[1].integrity[channel];
        NativeMaterialDds3D image{};NativeMaterialDdsError3D error{};const auto path=sourceDirectories[1]/files[channel];
        if(!LoadNativeMaterialDds3D(path,roles[channel],2048,false,image,&error,seal.expectedSha256,seal.expectedBytes,true)||
            image.width!=1024||image.height!=1024||image.mipCount!=11||image.format!=formats[channel]||image.sha256!=seal.expectedSha256)return 16;
        if(LoadNativeMaterialDds3D(path,roles[channel],2048,false,image,&error,std::string(64,'0'),seal.expectedBytes,true)||error!=NativeMaterialDdsError3D::HashMismatch)return 17;
        ++soilRejected;
        if(LoadNativeMaterialDds3D(path,roles[channel],2048,false,image,&error,seal.expectedSha256,seal.expectedBytes-1,true)||error!=NativeMaterialDdsError3D::HashMismatch)return 18;
        ++soilRejected;
        if(LoadNativeMaterialDds3D(path,roles[channel],512,false,image,&error,seal.expectedSha256,seal.expectedBytes,true)||error!=NativeMaterialDdsError3D::InvalidDimensions)return 19;
        ++soilRejected;
    }
    if(LoadNativeMaterialDds3D(sourceDirectories[1]/files[1],roles[0],2048,false,invalid)||
        LoadNativeMaterialDds3D(sourceDirectories[1]/files[0],roles[2],2048,false,invalid))return 20;
    soilRejected+=2;if(soilRejected!=11||verified!=9)return 21;
    unsigned diagnosticCases{};
    for(unsigned mode=0;mode<3;++mode)for(unsigned flags=0;flags<=3;++flags){
        const bool useLegacy=mode==2,useSingle=mode==1;
        const auto original=Mistwatch::GroundMaterialDefinition(useLegacy,useSingle);
        const auto neutral=Mistwatch::GroundDiagnosticDefinition(useLegacy,(flags&1)!=0,(flags&2)!=0,useSingle);
        if(neutral.name!="ground-diagnostic"||!SameMaterial(neutral.material,original.material)||
            neutral.textures.maximumResolution!=original.textures.maximumResolution||neutral.textures.alphaMode!=original.textures.alphaMode||
            neutral.textures.alphaCutoff!=original.textures.alphaCutoff)return 22;
        const auto before=Layers(original.textures),after=Layers(neutral.textures);
        for(unsigned layer=0;layer<3;++layer){
            if(layer&&mode){if(!SameLayer(before[layer],after[layer]))return 23;continue;}
            const auto oldPaths=Paths(before[layer]),newPaths=Paths(after[layer]);
            if(after[layer].surfaceEncoding!=((flags&2)?MaterialSurfaceMapEncoding3D::RoughnessR:before[layer].surfaceEncoding))return 8;
            for(unsigned channel=0;channel<3;++channel){
                const bool changed=channel<2&&(flags&(1u<<channel))!=0;
                if(changed){if(newPaths[channel]!=(channel==0?"diagnostics/neutral-base.bmp":"diagnostics/neutral-surface.bmp")||
                    after[layer].integrity[channel].expectedBytes||!after[layer].integrity[channel].expectedSha256.empty())return 9;
                }else if(newPaths[channel]!=oldPaths[channel]||!SameSeal(after[layer].integrity[channel],before[layer].integrity[channel]))return 9;
            }
        }
        ++diagnosticCases;
    }
    static_assert(CanonicalMaterialSurfaceTexel3D({17,83,5,241},MaterialSurfaceMapEncoding3D::PackedOrm)==std::array<std::uint8_t,4>{17,83,5,83});
    std::cout<<"Terrain material CPU PASS; sealed_channels="<<verified<<" forest_hostile_rejections="<<rejected<<
        " soil_hostile_rejections="<<soilRejected<<" diagnostic_cases="<<diagnosticCases<<
        " baseline=single-forest-Surface legacy=Ground037-Surface projection=triplanar scale=.25 normal=.65 visual=WIP\n";
}
