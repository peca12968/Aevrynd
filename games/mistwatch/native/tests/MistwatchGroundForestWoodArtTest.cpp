#include "GroundForestWoodArt.h"
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
#include <vector>

namespace {
using namespace Aevrynd;
using Microsoft::WRL::ComPtr;
unsigned checks{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool SameFloat(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
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
        a.castsShadow==b.castsShadow&&a.textures==b.textures&&a.textures.semanticIdentity==b.textures.semanticIdentity&&
        SameFloat(a.textureScale,b.textureScale)&&a.useUv==b.useUv&&a.materialClass==b.materialClass&&a.shadingModel==b.shadingModel&&
        a.alphaMode==b.alphaMode&&SameFloat(a.alphaCutoff,b.alphaCutoff)&&a.doubleSided==b.doubleSided&&
        SameFloat(a.normalScale,b.normalScale)&&SameFloat(a.baseTextureScale,b.baseTextureScale)&&SameFloat(a.detailTextureScale,b.detailTextureScale);
}
bool SameDefinition(const MaterialAssetDefinition3D& a,const MaterialAssetDefinition3D& b) {
    if(a.name!=b.name||!SameMaterial(a.material,b.material)||a.textures.maximumResolution!=b.textures.maximumResolution||
       a.textures.alphaMode!=b.textures.alphaMode||!SameFloat(a.textures.alphaCutoff,b.textures.alphaCutoff))return false;
    const auto aa=Layers(a.textures),bb=Layers(b.textures);
    for(unsigned layer=0;layer<3;++layer)if(!SameLayer(aa[layer],bb[layer]))return false;
    return true;
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
MaterialTextureLayerDefinition3D ExpectedBase() {
    MaterialTextureLayerDefinition3D layer{
        "ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg",
        "ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",
        "ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"};
    layer.surfaceEncoding=MaterialSurfaceMapEncoding3D::RoughnessR;
    constexpr std::uint64_t bytes[]{2187764,694944,2474498};
    constexpr const char* hashes[]{
        "d92bec8a35b9cb16ec79b421ca0f572a7400286eae109beafa266629ef49a6cf",
        "3786b088e981aae562f06b6477972d379cffd09a5cc542ab42f9ffd94f9bcb0e",
        "0e2d613d117d9231aa0aa61f39b19755f891b4ed1d1599a004cb018a8224e039"};
    for(unsigned channel=0;channel<3;++channel)layer.integrity[channel]={bytes[channel],hashes[channel]};
    return layer;
}
void OrdinaryAndDiagnosticMatrix() {
    const auto old=Mistwatch::GroundMaterialDefinition(),art=Mistwatch::GroundForestWoodArtDefinition(true);
    const auto expected=ExpectedBase();
    const auto oldLayers=Layers(old.textures),artLayers=Layers(art.textures);
    Require(SameMaterial(old.material,art.material),"Ordinary all scalar/handle/tint/class bits changed");
    Require(old.name==art.name&&old.textures.maximumResolution==art.textures.maximumResolution&&
        old.textures.alphaMode==art.textures.alphaMode&&SameFloat(old.textures.alphaCutoff,art.textures.alphaCutoff),
        "Ordinary non-base metadata changed");
    Require(art.material.materialClass==MaterialClass3D::Terrain&&art.material.textureScale==.25f&&
        art.material.normalScale==.65f&&!art.material.useUv&&!art.material.castsShadow,"Original Terrain projection contract changed");
    Require(SameLayer(artLayers[0],expected),"Three true JPG paths/full integrity/RoughnessR mismatch");
    Require(SameLayer(oldLayers[1],artLayers[1])&&SameLayer(oldLayers[2],artLayers[2]),"Other six channels changed");
    for(unsigned channel=0;channel<3;++channel)
        Require(Paths(oldLayers[0])[channel]!=Paths(artLayers[0])[channel],"Ordinary intended base replacement absent");
    unsigned cases{};
    for(bool wood:{false,true})for(bool legacy:{false,true})for(bool single:{false,true}) {
        const bool active=wood&&!legacy&&!single;
        const auto before=Mistwatch::GroundMaterialDefinition(legacy,single);
        const auto after=Mistwatch::GroundForestWoodArtDefinition(wood,legacy,single);
        if(!active)Require(SameDefinition(before,after),"Legacy/single/non-ForestWood regression");
        else {
            auto ordinaryExpected=before;
            ordinaryExpected.textures.baseColorPath=expected.baseColorPath;
            ordinaryExpected.textures.roughnessPath=expected.roughnessPath;
            ordinaryExpected.textures.normalPath=expected.normalPath;
            ordinaryExpected.textures.surfaceEncoding=expected.surfaceEncoding;
            ordinaryExpected.textures.integrity=expected.integrity;
            Require(SameDefinition(after,ordinaryExpected),"Ordinary replacement escaped three-channel scope");
        }
        for(unsigned flags=0;flags<4;++flags) {
            const bool neutralBase=(flags&1u)!=0,neutralSurface=(flags&2u)!=0;
            const auto diagnosticBefore=Mistwatch::GroundDiagnosticDefinition(legacy,neutralBase,neutralSurface,single);
            const auto diagnosticAfter=Mistwatch::GroundForestWoodArtDiagnosticDefinition(wood,legacy,neutralBase,neutralSurface,single);
            auto diagnosticExpected=diagnosticBefore;
            if(active) {
                if(!neutralBase){diagnosticExpected.textures.baseColorPath=expected.baseColorPath;diagnosticExpected.textures.integrity[0]=expected.integrity[0];}
                if(!neutralSurface){diagnosticExpected.textures.roughnessPath=expected.roughnessPath;diagnosticExpected.textures.integrity[1]=expected.integrity[1];}
                diagnosticExpected.textures.normalPath=expected.normalPath;
                diagnosticExpected.textures.integrity[2]=expected.integrity[2];
                diagnosticExpected.textures.surfaceEncoding=expected.surfaceEncoding;
            }
            Require(SameDefinition(diagnosticAfter,diagnosticExpected),"32-mode full definition or neutral isolation mismatch");
            const auto beforeLayers=Layers(diagnosticBefore.textures),afterLayers=Layers(diagnosticAfter.textures);
            Require(SameLayer(beforeLayers[1],afterLayers[1])&&SameLayer(beforeLayers[2],afterLayers[2]),"Neutral soil/rock full six-slot regression");
            const std::string_view expectedSource=neutralBase?"neutral-base":legacy?"ground037":active?"ground037-moss-floor":"forest-ground-04";
            Require(Mistwatch::GroundForestWoodArtSource(diagnosticAfter)==expectedSource,"Source label not actual final definition");
            ++cases;
        }
    }
    Require(cases==32,"Definition/neutral cases incomplete");
    std::cout<<"GROUND_FOREST_WOOD_ART_MATRIX cases="<<cases<<" six_other_slots_exact=1 scalar_bits_exact=1\n";
}
struct WicImage {
    UINT width{},height{};
    std::vector<std::uint8_t> pixels;
};
WicImage Decode(const std::filesystem::path& path) {
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    Require(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))),"WIC factory unavailable");
    Require(SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder)),
        "WIC actual JPG source decode failed");
    Require(SUCCEEDED(decoder->GetFrame(0,&frame)),"WIC actual frame unavailable");
    WicImage image;
    Require(SUCCEEDED(frame->GetSize(&image.width,&image.height))&&image.width==1024&&image.height==1024,"True Ground037 dimensions changed");
    Require(SUCCEEDED(factory->CreateFormatConverter(&converter))&&
        SUCCEEDED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)),
        "WIC RGBA conversion failed");
    image.pixels.resize(std::size_t(image.width)*image.height*4);
    Require(SUCCEEDED(converter->CopyPixels(nullptr,image.width*4,UINT(image.pixels.size()),image.pixels.data())),"WIC true pixels absent");
    return image;
}
void SourceResources() {
    const HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    Require(SUCCEEDED(init)||init==RPC_E_CHANGED_MODE,"COM initialization failed");
    struct Apartment {bool release;~Apartment(){if(release)CoUninitialize();}} apartment{SUCCEEDED(init)};
    const auto definition=Mistwatch::GroundForestWoodArtDefinition(true);
    const auto layers=Layers(definition.textures);
    const auto source=std::filesystem::path(MISTWATCH_SOURCE_ROOT);
    const std::array roots{source/"engine/assets/material-packs",
        source/"engine/assets/realistic-cc0-first-batch/cooked/materials/ground108",
        source/"engine/assets/material-packs/poly-haven/rock_surface/1k"};
    unsigned resources{},negative{},decoded{};
    // 全九個實際source seal先真核：新基層三JPG，其餘soil/rock原六槽保持完整原來源。
    for(unsigned layer=0;layer<3;++layer)for(unsigned channel=0;channel<3;++channel) {
        const auto relative=Paths(layers[layer])[channel];
        const auto path=layer==0?roots[layer]/relative:roots[layer]/relative.filename();
        Require(VerifySeal(path,layers[layer].integrity[channel]),"Actual nine-channel source raw seal mismatch");
        ++resources;
    }
    constexpr NativeMaterialDdsRole3D roles[]{NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsRole3D::Surface,NativeMaterialDdsRole3D::Normal};
    for(unsigned channel=0;channel<3;++channel) {
        const auto path=roots[0]/Paths(layers[0])[channel];const auto& seal=layers[0].integrity[channel];
        auto bad=seal;bad.expectedSha256=std::string(64,'0');
        Require(!VerifySeal(path,bad),"Bad JPG hash accepted");++negative;
        bad=seal;--bad.expectedBytes;
        Require(!VerifySeal(path,bad),"Bad JPG size accepted");++negative;
        NativeMaterialDds3D falseDds;
        Require(!LoadNativeMaterialDds3D(path,roles[channel],2048,false,falseDds),"JPG falsely accepted as native DDS");++negative;
        const auto image=Decode(path);++decoded;
        std::uint8_t minimum=255,maximum=0;
        for(std::size_t offset=0;offset<image.pixels.size();offset+=4) {
            const auto* pixel=image.pixels.data()+offset;
            Require(pixel[3]==255,"Opaque ground alpha changed");
            minimum=(std::min)(minimum,pixel[0]);maximum=(std::max)(maximum,pixel[0]);
            if(channel==1) {
                const auto canonical=CanonicalMaterialSurfaceTexel3D(
                    {pixel[0],pixel[1],pixel[2],pixel[3]},MaterialSurfaceMapEncoding3D::RoughnessR);
                Require(canonical==std::array<std::uint8_t,4>{255,pixel[0],255,pixel[0]},
                    "True roughness R treated as packed ORM or gamma colour");
            }
            if(channel==2) {
                const float x=float(pixel[0])/255.f*2.f-1.f,y=float(pixel[1])/255.f*2.f-1.f,z=float(pixel[2])/255.f*2.f-1.f;
                Require(std::isfinite(x*x+y*y+z*z)&&x*x+y*y+z*z>0.f,"True DX normal source nonfinite/zero");
            }
        }
        Require(maximum>minimum,"PBR source unexpectedly constant");
        std::cout<<"GROUND_FOREST_WOOD_ART_WIC channel="<<channel<<" width="<<image.width<<" height="<<image.height
            <<" minimum_R="<<unsigned(minimum)<<" maximum_R="<<unsigned(maximum)<<" bytes="<<seal.expectedBytes
            <<" SHA="<<seal.expectedSha256<<'\n';
    }
    Require(resources==9&&negative==9&&decoded==3,"Actual sources/WIC/fail-closed negative coverage incomplete");
    std::cout<<"GROUND_FOREST_WOOD_ART_RESOURCES sealed=9 WIC_JPG=3 negative_controls=9 GPU_upload_observed=0\n";
}
}
int main() try {
    OrdinaryAndDiagnosticMatrix();SourceResources();
    std::cout<<"GROUND_FOREST_WOOD_ART_CPU_PASS matrix_cases=32 sealed_resources=9 JPG=3 RoughnessR=1 NormalDX=1"
        <<" original_six_slots_exact=1 negative_controls=9 checks="<<checks<<" full_art_or_demo49_pass=0\n";
    return 0;
}catch(const std::exception& error){
    std::cerr<<"GROUND_FOREST_WOOD_ART_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;
}
