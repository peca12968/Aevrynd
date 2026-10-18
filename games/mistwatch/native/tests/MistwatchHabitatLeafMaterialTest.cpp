#include "MistwatchHabitatLeafMaterial.h"
#include <Aevrynd/NativeMaterialDds3D.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace Aevrynd;
using Microsoft::WRL::ComPtr;
unsigned checks{};
void Require(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
bool SameFloat(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
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
    return SameFloat(a.tint.r,b.tint.r)&&SameFloat(a.tint.g,b.tint.g)&&SameFloat(a.tint.b,b.tint.b)&&SameFloat(a.tint.a,b.tint.a)&&
        SameFloat(a.surface.roughness,b.surface.roughness)&&SameFloat(a.surface.metallic,b.surface.metallic)&&
        SameFloat(a.surface.emissive,b.surface.emissive)&&SameFloat(a.surface.occlusion,b.surface.occlusion)&&
        a.castsShadow==b.castsShadow&&a.textures.value==b.textures.value&&a.textures.semanticIdentity==b.textures.semanticIdentity&&
        SameFloat(a.textureScale,b.textureScale)&&a.useUv==b.useUv&&a.materialClass==b.materialClass&&a.shadingModel==b.shadingModel&&
        a.alphaMode==b.alphaMode&&SameFloat(a.alphaCutoff,b.alphaCutoff)&&a.doubleSided==b.doubleSided&&
        SameFloat(a.normalScale,b.normalScale)&&SameFloat(a.baseTextureScale,b.baseTextureScale)&&SameFloat(a.detailTextureScale,b.detailTextureScale);
}
bool SameDefinition(const MaterialAssetDefinition3D& a,const MaterialAssetDefinition3D& b) {
    if(a.name!=b.name||!SameMaterial(a.material,b.material)||a.textures.maximumResolution!=b.textures.maximumResolution||
        a.textures.alphaMode!=b.textures.alphaMode||!SameFloat(a.textures.alphaCutoff,b.textures.alphaCutoff))return false;
    const auto first=Layers(a.textures),second=Layers(b.textures);
    for(unsigned layer=0;layer<3;++layer)if(!SameLayer(first[layer],second[layer]))return false;
    return true;
}
// 期望值獨立列出，不從待測 definition 複製槽位、大小或雜湊。
MaterialTextureLayerDefinition3D ExpectedMoss() {
    MaterialTextureLayerDefinition3D layer{
        "ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg",
        "ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",
        "ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"};
    layer.surfaceEncoding=MaterialSurfaceMapEncoding3D::RoughnessR;
    layer.integrity={MaterialTextureIntegrity3D{2187764,"d92bec8a35b9cb16ec79b421ca0f572a7400286eae109beafa266629ef49a6cf"},
        MaterialTextureIntegrity3D{694944,"3786b088e981aae562f06b6477972d379cffd09a5cc542ab42f9ffd94f9bcb0e"},
        MaterialTextureIntegrity3D{2474498,"0e2d613d117d9231aa0aa61f39b19755f891b4ed1d1599a004cb018a8224e039"}};
    return layer;
}
MaterialTextureLayerDefinition3D ExpectedLeaf() {
    MaterialTextureLayerDefinition3D layer{
        "poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png",
        "poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png",
        "poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png"};
    layer.surfaceEncoding=MaterialSurfaceMapEncoding3D::RoughnessR;
    layer.integrity={MaterialTextureIntegrity3D{10402792,"40b1786afa17c9930ccae93a9ca75880fc52fc34b96453489c767b2cc82d7ee9"},
        MaterialTextureIntegrity3D{6083648,"713874257d143af49c482ce8b7648cb7c5c748ca6540db0e44563a04a9fe09ce"},
        MaterialTextureIntegrity3D{11539651,"19bbaa0eb67eda146129946b9605cfc9b472a302ba1f75eb8ea0b48aa21baa33"}};
    return layer;
}
MaterialTextureLayerDefinition3D ExpectedRock() {
    MaterialTextureLayerDefinition3D layer{"poly-haven/rock_surface/1k/base-color.jpg",
        "poly-haven/rock_surface/1k/roughness.jpg","poly-haven/rock_surface/1k/normal-dx.jpg"};
    layer.surfaceEncoding=MaterialSurfaceMapEncoding3D::RoughnessR;
    layer.integrity={MaterialTextureIntegrity3D{886291,"07edda2bbe4715b01aae2e4bac9f21f204cf4aae37b4733ef01de33281e5e60e"},
        MaterialTextureIntegrity3D{642680,"ca710d895b01cf323bf00f53194c43504a39d2c964fbfc343c4f45c7d41b10dc"},
        MaterialTextureIntegrity3D{1161843,"1e1aa5b06f74d22734318cd93ecf49ffe7095c4b2307c5c9b3a4550207c6b1b7"}};
    return layer;
}
void ReplaceExpectedLeaf(MaterialAssetDefinition3D& expected,bool neutralBase,bool neutralSurface) {
    const auto leaf=ExpectedLeaf();auto& layer=expected.textures.additionalLayers[0];
    if(!neutralBase){layer.baseColorPath=leaf.baseColorPath;layer.integrity[0]=leaf.integrity[0];}
    if(!neutralSurface){layer.roughnessPath=leaf.roughnessPath;layer.integrity[1]=leaf.integrity[1];}
    layer.normalPath=leaf.normalPath;layer.integrity[2]=leaf.integrity[2];layer.surfaceEncoding=leaf.surfaceEncoding;
}
void DefinitionMatrix() {
    unsigned ordinaryCases{},diagnosticCases{},ordinaryActive{},diagnosticActive{};
    for(bool wood:{false,true})for(bool habitat:{false,true})for(bool legacy:{false,true})for(bool single:{false,true}) {
        const bool active=wood&&habitat&&!legacy&&!single;
        const auto before=Mistwatch::GroundForestWoodArtDefinition(wood,legacy,single);
        const auto after=Mistwatch::HabitatGroundDefinition(wood,habitat,legacy,single);
        auto expected=before;if(active)ReplaceExpectedLeaf(expected,false,false);
        Require(SameDefinition(after,expected),"Ordinary 16-mode replacement escaped leaf layer or inactive definition changed");
        const auto beforeLayers=Layers(before.textures),afterLayers=Layers(after.textures);
        Require(SameLayer(beforeLayers[0],afterLayers[0])&&SameLayer(beforeLayers[2],afterLayers[2]),"Ordinary moss/rock six channels changed");
        Require(SameMaterial(before.material,after.material),"Ordinary scalar/tint/handle/class bit changed");
        const std::string_view expectedSource=legacy||single?"absent":active?"forest-leaves-04-habitat":"ground108-gravel";
        Require(Mistwatch::HabitatGroundLayerSource(after)==expectedSource,"Ordinary final-definition source identity mismatch");
        if(active) {
            Require(SameLayer(afterLayers[0],ExpectedMoss()),"Ground037 base literal source changed");
            Require(SameLayer(afterLayers[1],ExpectedLeaf()),"Leaf three-channel literal source mismatch");
            Require(SameLayer(afterLayers[2],ExpectedRock()),"Original rock literal source changed");
            ++ordinaryActive;
        }
        ++ordinaryCases;
        for(unsigned flags=0;flags<4;++flags) {
            const bool neutralBase=(flags&1u)!=0,neutralSurface=(flags&2u)!=0;
            const auto diagnosticBefore=Mistwatch::GroundForestWoodArtDiagnosticDefinition(wood,legacy,neutralBase,neutralSurface,single);
            const auto diagnosticAfter=Mistwatch::HabitatGroundDiagnosticDefinition(wood,habitat,legacy,neutralBase,neutralSurface,single);
            auto diagnosticExpected=diagnosticBefore;if(active)ReplaceExpectedLeaf(diagnosticExpected,neutralBase,neutralSurface);
            Require(SameDefinition(diagnosticAfter,diagnosticExpected),"Diagnostic 64-mode full definition/neutral isolation mismatch");
            const auto oldLayers=Layers(diagnosticBefore.textures),newLayers=Layers(diagnosticAfter.textures);
            Require(SameLayer(oldLayers[0],newLayers[0])&&SameLayer(oldLayers[2],newLayers[2]),"Diagnostic moss/rock full six channels changed");
            Require(SameMaterial(diagnosticBefore.material,diagnosticAfter.material),"Diagnostic scalar/tint/handle/class bit changed");
            const std::string_view diagnosticSource=legacy||single?"absent":neutralBase?"neutral-base":
                active?"forest-leaves-04-habitat":"ground108-gravel";
            Require(Mistwatch::HabitatGroundLayerSource(diagnosticAfter)==diagnosticSource,"Diagnostic final-definition source identity mismatch");
            if(neutralBase)Require(SameSeal(newLayers[1].integrity[0],oldLayers[1].integrity[0])&&
                newLayers[1].baseColorPath==oldLayers[1].baseColorPath,"Neutral base replaced with leaf image");
            if(neutralSurface)Require(SameSeal(newLayers[1].integrity[1],oldLayers[1].integrity[1])&&
                newLayers[1].roughnessPath==oldLayers[1].roughnessPath,"Neutral surface replaced with leaf image");
            if(active)++diagnosticActive;
            ++diagnosticCases;
        }
    }
    Require(ordinaryCases==16&&diagnosticCases==64&&ordinaryActive==1&&diagnosticActive==4,"Complete ordinary/diagnostic boolean matrix missing");
    std::cout<<"HABITAT_LEAF_MATRIX ordinary=16 diagnostic=64 active_ordinary=1 active_diagnostic=4 moss_rock_exact=1 scalar_bits_exact=1\n";
}
void SourceIdentityNegativeControls() {
    const auto definition=Mistwatch::HabitatGroundDefinition(true,true);
    unsigned negative{};
    for(unsigned channel=0;channel<3;++channel)for(unsigned kind=0;kind<3;++kind) {
        auto candidate=definition;auto& layer=candidate.textures.additionalLayers[0];
        if(kind==0) {
            if(channel==0)layer.baseColorPath="bad/leaf-base.png";
            if(channel==1)layer.roughnessPath="bad/leaf-rough.png";
            if(channel==2)layer.normalPath="bad/leaf-normal.png";
        } else if(kind==1)++layer.integrity[channel].expectedBytes;
        else layer.integrity[channel].expectedSha256=std::string(64,'0');
        Require(Mistwatch::HabitatGroundLayerSource(candidate)=="unrecognized","Corrupted leaf channel identity accepted");++negative;
    }
    auto candidate=definition;
    candidate.textures.additionalLayers[0].surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
    Require(Mistwatch::HabitatGroundLayerSource(candidate)=="unrecognized","Leaf packed-ORM mislabel accepted");++negative;
    Require(negative==10,"Source identity mutation matrix incomplete");
    std::cout<<"HABITAT_LEAF_IDENTITY_NEGATIVE controls="<<negative<<"\n";
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
struct WicImage {UINT width{},height{};std::vector<std::uint8_t> pixels;};
WicImage DecodeLeaf(const std::filesystem::path& path) {
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    Require(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))),"WIC factory unavailable");
    Require(SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder)),"WIC actual leaf PNG OnLoad decode failed");
    GUID container{};
    Require(SUCCEEDED(decoder->GetContainerFormat(&container))&&IsEqualGUID(container,GUID_ContainerFormatPng),"Leaf actual image is not PNG");
    Require(SUCCEEDED(decoder->GetFrame(0,&frame)),"WIC leaf frame unavailable");
    WicImage image;
    Require(SUCCEEDED(frame->GetSize(&image.width,&image.height))&&image.width==2048&&image.height==2048,"Leaf true 2048-square dimensions changed");
    Require(SUCCEEDED(factory->CreateFormatConverter(&converter))&&
        SUCCEEDED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)),"WIC leaf RGBA conversion failed");
    image.pixels.resize(std::size_t(image.width)*image.height*4);
    Require(SUCCEEDED(converter->CopyPixels(nullptr,image.width*4,UINT(image.pixels.size()),image.pixels.data())),"WIC leaf pixels absent");
    return image;
}
void SourceResources() {
    const HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    Require(SUCCEEDED(init)||init==RPC_E_CHANGED_MODE,"COM initialization failed");
    struct Apartment {bool release;~Apartment(){if(release)CoUninitialize();}} apartment{SUCCEEDED(init)};
    const auto source=std::filesystem::path(MISTWATCH_SOURCE_ROOT)/"engine/assets/material-packs";
    const std::array expectedLayers{ExpectedMoss(),ExpectedLeaf(),ExpectedRock()};
    const auto actualLayers=Layers(Mistwatch::HabitatGroundDefinition(true,true).textures);
    unsigned sealed{},negative{},decoded{};
    // 真實九檔逐一核對獨立 literal 封印，不使用舊 Evidence 目錄或待測旗標替代實檔。
    for(unsigned layer=0;layer<3;++layer)for(unsigned channel=0;channel<3;++channel) {
        Require(SameLayer(actualLayers[layer],expectedLayers[layer]),"Actual definition differs from independent layer identity");
        const auto path=source/Paths(expectedLayers[layer])[channel];
        Require(VerifySeal(path,expectedLayers[layer].integrity[channel]),"Actual nine-channel source seal mismatch");++sealed;
    }
    constexpr NativeMaterialDdsRole3D roles[]{NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsRole3D::Surface,NativeMaterialDdsRole3D::Normal};
    for(unsigned channel=0;channel<3;++channel) {
        const auto path=source/Paths(expectedLayers[1])[channel];const auto& seal=expectedLayers[1].integrity[channel];
        auto bad=seal;bad.expectedSha256=std::string(64,'0');
        Require(!VerifySeal(path,bad),"Bad leaf PNG hash accepted");++negative;
        bad=seal;--bad.expectedBytes;
        Require(!VerifySeal(path,bad),"Bad leaf PNG size accepted");++negative;
        NativeMaterialDds3D falseDds;
        Require(!LoadNativeMaterialDds3D(path,roles[channel],2048,false,falseDds),"Leaf PNG falsely accepted as DDS");++negative;
        const auto image=DecodeLeaf(path);++decoded;
        std::uint8_t minimumR=255,maximumR=0,minimumG=255,maximumG=0;
        for(std::size_t offset=0;offset<image.pixels.size();offset+=4) {
            const auto* pixel=image.pixels.data()+offset;
            Require(pixel[3]==255,"Leaf terrain source unexpectedly transparent");
            minimumR=(std::min)(minimumR,pixel[0]);maximumR=(std::max)(maximumR,pixel[0]);
            minimumG=(std::min)(minimumG,pixel[1]);maximumG=(std::max)(maximumG,pixel[1]);
            if(channel==1) {
                const auto canonical=CanonicalMaterialSurfaceTexel3D({pixel[0],pixel[1],pixel[2],pixel[3]},MaterialSurfaceMapEncoding3D::RoughnessR);
                Require(canonical==std::array<std::uint8_t,4>{255,pixel[0],255,pixel[0]},"Leaf roughness R incorrectly gamma/ORM interpreted");
            }
            if(channel==2) {
                // DX 方向由完整已知 DX source 雜湊固定；逐 texel 另查有限且非零向量，不宣稱 GPU 上傳證據。
                const float x=float(pixel[0])/255.f*2.f-1.f,y=float(pixel[1])/255.f*2.f-1.f,z=float(pixel[2])/255.f*2.f-1.f;
                const float lengthSquared=x*x+y*y+z*z;
                Require(std::isfinite(lengthSquared)&&lengthSquared>0.f,"Leaf DX normal contains nonfinite/zero vector");
            }
        }
        Require(maximumR>minimumR,"Leaf PBR image unexpectedly constant");
        if(channel==2)Require(maximumG>minimumG,"Leaf DX normal Y unexpectedly constant");
        std::cout<<"HABITAT_LEAF_WIC channel="<<channel<<" width="<<image.width<<" height="<<image.height
            <<" minimum_R="<<unsigned(minimumR)<<" maximum_R="<<unsigned(maximumR)<<" minimum_G="<<unsigned(minimumG)
            <<" maximum_G="<<unsigned(maximumG)<<" bytes="<<seal.expectedBytes<<" SHA="<<seal.expectedSha256<<'\n';
    }
    Require(sealed==9&&negative==9&&decoded==3,"Nine sealed sources/three actual PNG/negative controls incomplete");
    std::cout<<"HABITAT_LEAF_RESOURCES sealed=9 WIC_PNG=3 resource_negative=9 GPU_upload_observed=0\n";
}
}
int main() try {
    DefinitionMatrix();SourceIdentityNegativeControls();SourceResources();
    std::cout<<"HABITAT_LEAF_CPU_PASS ordinary=16 diagnostic=64 sealed_resources=9 PNG=3 RoughnessR=1 NormalDX_pinned=1"
        <<" moss_rock_exact=1 resource_negative=9 identity_negative=10 checks="<<checks<<" GPU_upload_or_art_pass=0\n";
    return 0;
}catch(const std::exception& error) {
    std::cerr<<"HABITAT_LEAF_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;
}
