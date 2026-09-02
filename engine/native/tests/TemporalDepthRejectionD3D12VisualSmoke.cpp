#include <Aevrynd/Renderer.h>
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <dxgiformat.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <vector>

using namespace Aevrynd;
using Microsoft::WRL::ComPtr;

namespace {
constexpr unsigned Width=320,Height=180;
struct Image {unsigned width{},height{};std::vector<std::uint8_t> rgba;};
struct Capture {Image image;ObjectIdCapture3D objectId;RendererStats stats;bool valid{};};
enum class DepthMutation {Production,CurrentDepth,LooseFarSky};
struct Residual {double mean{};double maximum{};};

bool SetDepthMutation(D3D12Renderer& renderer,DepthMutation mutation){return D3D12RendererTestAccess3D::SetLegacyTemporalDepthRejection(renderer,mutation==DepthMutation::CurrentDepth)&&D3D12RendererTestAccess3D::SetLegacyTemporalFarSkyClassification(renderer,mutation==DepthMutation::LooseFarSky);}
bool ValidationReceiptMatches(const RendererStats& stats,const char*& mode){wchar_t expected[8]{};const DWORD length=GetEnvironmentVariableW(L"AEVRYND_EXPECT_GPU_VALIDATION_MODE",expected,DWORD(std::size(expected)));if(length==6&&lstrcmpW(expected,L"normal")==0){mode="normal";return !stats.gpuValidationRequested&&!stats.gpuValidationAvailable&&!stats.gpuValidationExecuted&&!stats.debugLayerEnabled&&!stats.gpuValidationPassed&&!stats.debugErrorCount&&!stats.debugWarningCount;}if(length==3&&lstrcmpW(expected,L"gbv")==0){mode="gbv";return stats.gpuValidationRequested&&stats.gpuValidationAvailable&&stats.gpuValidationExecuted&&stats.debugLayerEnabled&&stats.gpuValidationPassed&&!stats.debugErrorCount&&!stats.debugWarningCount;}mode="missing-or-invalid";return false;}

bool DecodePng(const std::filesystem::path& path,Image& output){ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;UINT width{},height{};if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&frame))||FAILED(frame->GetSize(&width,&height))||FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return false;output={width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};return SUCCEEDED(converter->CopyPixels(nullptr,width*4,UINT(output.rgba.size()),output.rgba.data()));}

MeshHandle3D CreateMesh(D3D12Renderer& renderer,std::span<const Vec3> positions,std::span<const std::uint32_t> indices){std::vector<Vertex3D> vertices;vertices.reserve(positions.size());const auto normal=PackNormalSnorm8({0,0,-1});for(const Vec3 position:positions)vertices.push_back({position.x,position.y,position.z,normal,0xffffffffu,0,0});return renderer.CreateMesh(vertices,indices);}

bool SubmitScene(DrawList& draw,MeshHandle3D opaque,MeshHandle3D foliage,MeshHandle3D highlight,MeshHandle3D background,MeshHandle3D farBackground,bool bloom,Matrix4 highlightTransform=Matrix4::Identity(),Matrix4 previousHighlightTransform=Matrix4::Identity(),bool highlightMotion=false,Matrix4 farTransform=Matrix4::Identity()){
    draw.Clear();draw.SetViewProjection(Matrix4::Identity());draw.SetBackfaceCulling(false);draw.SetCameraCut3D(false);
    draw.SetDirectionalLight({{0,0,-1},.08f,{1,1,1,1},1.f});
    draw.SetPhaseRadianceProfile({{.01f,.018f,.03f},.35f,{.025f,.035f,.05f},.5f,{.01f,.012f,.01f},.00935f,{.28f,.3f,.3f},.5f,{1.f,.9f,.7f},0.f,0x48414c4f});
    draw.SetBloom({bloom,.55f,.5f,.18f,3.f});
    const RenderObjectIdentity3D opaqueId{{0x48414c4f,1},1},foliageId{{0x48414c4f,2},1},highlightId{{0x48414c4f,3},1},backgroundId{{0x48414c4f,4},1},moverId{{0x48414c4f,5},1},farId{{0x48414c4f,6},1};
    bool accepted=true;
    for(unsigned strip=0;strip<31;++strip){const Color color=strip%2?Color{.5f,.1f,.2f,1}:Color{.1f,.3f,.1f,1};accepted=draw.MeshInstance(background,CreateTranslationMatrix({float(strip)*.0125f,0,0}),color,{.5f,0,.25f,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false,1,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,Matrix4::Identity(),false,1,MaterialAlphaMode3D::Opaque,.5f,false,backgroundId)&&accepted;}
    for(unsigned strip=0;strip<31;++strip){const Color color=strip%2?Color{.045f,.01f,.05f,1}:Color{.015f,.04f,.015f,1};accepted=draw.MeshInstance(farBackground,CreateTranslationMatrix({float(strip)*.0125f,0,0})*farTransform,color,{.85f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false,1,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,Matrix4::Identity(),false,1,MaterialAlphaMode3D::Opaque,.5f,false,farId)&&accepted;}
    return accepted&&draw.MeshInstance(opaque,Matrix4::Identity(),{.025f,.03f,.035f,1},{.85f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false,1,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,Matrix4::Identity(),false,1,MaterialAlphaMode3D::Opaque,.5f,false,opaqueId)
        &&draw.MeshInstance(foliage,Matrix4::Identity(),{.02f,.035f,.018f,1},{.8f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Foliage,std::numeric_limits<float>::quiet_NaN(),false,1,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,Matrix4::Identity(),false,1,MaterialAlphaMode3D::Opaque,.5f,true,foliageId)
        &&draw.MeshInstance(highlight,Matrix4::Identity(),{1,1,.9f,1},{.18f,0,1,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false,1,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,Matrix4::Identity(),false,1,MaterialAlphaMode3D::Opaque,.5f,false,highlightId)
        &&draw.MeshInstance(highlight,CreateTranslationMatrix({0,-.45f,0})*highlightTransform,{.3f,.2f,.15f,1},{.5f,0,.25f,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false,1,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,CreateTranslationMatrix({0,-.45f,0})*previousHighlightTransform,highlightMotion,1,MaterialAlphaMode3D::Opaque,.5f,false,moverId);
}

Capture CaptureScene(D3D12Renderer& renderer,MeshHandle3D opaque,MeshHandle3D foliage,MeshHandle3D highlight,MeshHandle3D background,MeshHandle3D farBackground,DepthMutation mutation,bool bloom,bool groundTruth,const std::filesystem::path& path){Capture result{};if(!SetDepthMutation(renderer,mutation))return result;DrawList draw;const Matrix4 previousHighlight=CreateTranslationMatrix({-.12f,0,0}),previousFar=CreateTranslationMatrix({-.12f,0,0});for(unsigned frame=0;frame<8;++frame){if(!SubmitScene(draw,opaque,foliage,highlight,background,farBackground,bloom,previousHighlight,previousHighlight,false,previousFar))return result;draw.SetCameraCut3D(frame==0);renderer.Render(draw,{.12f,.16f,.22f,1});}if(!SubmitScene(draw,opaque,foliage,highlight,background,farBackground,bloom,Matrix4::Identity(),previousHighlight,true)||!renderer.RequestObjectIdReadback3D({uint64_t(mutation)+1u,bloom?2u:1u,groundTruth?2u:1u})||!renderer.CaptureNextFrame(path))return result;draw.SetCameraCut3D(groundTruth);renderer.Render(draw,{.12f,.16f,.22f,1});auto objectId=renderer.TakeObjectIdReadback3D();renderer.WaitIdle();result.stats=renderer.GetStats();if(!objectId||!objectId->Complete()||!DecodePng(path,result.image)||result.image.width!=Width||result.image.height!=Height)return result;result.objectId=std::move(*objectId);result.valid=true;return result;}

float Luma(const Image& image,std::size_t pixel){const auto* c=image.rgba.data()+pixel*4;return (float(c[0])*.2126f+float(c[1])*.7152f+float(c[2])*.0722f)/255.f;}
std::vector<bool> Mask(const ObjectIdCapture3D& capture,RenderObjectIdentity3D identity){std::vector<bool> result(capture.pixels.size());for(std::size_t i=0;i<capture.pixels.size();++i){const auto* resolved=capture.Resolve(capture.pixels[i]);result[i]=resolved&&*resolved==identity;}return result;}
float MaximumBoundaryReduction(const Capture& current,const Capture& legacy,RenderObjectIdentity3D identity){const auto mask=Mask(current.objectId,identity);float maximum{};for(unsigned y=1;y+1<Height;++y)for(unsigned x=1;x+1<Width;++x){const std::size_t i=std::size_t(y)*Width+x;if(!mask[i])continue;const bool boundary=!mask[i-1]||!mask[i+1]||!mask[i-Width]||!mask[i+Width];if(boundary)maximum=(std::max)(maximum,Luma(legacy.image,i)-Luma(current.image,i));}return maximum;}
float MaximumAbsoluteBoundaryHalo(const Capture& capture,RenderObjectIdentity3D identity);
float MaximumBoundaryGain(const Capture& current,const Capture& legacy,RenderObjectIdentity3D identity){const auto mask=Mask(current.objectId,identity);float maximum{};for(unsigned y=1;y+1<Height;++y)for(unsigned x=1;x+1<Width;++x){const std::size_t i=std::size_t(y)*Width+x;if(!mask[i])continue;const bool boundary=!mask[i-1]||!mask[i+1]||!mask[i-Width]||!mask[i+Width];if(boundary)maximum=(std::max)(maximum,Luma(current.image,i)-Luma(legacy.image,i));}const auto currentGpu=current.stats.lastGpuFrameNanoseconds,legacyGpu=legacy.stats.lastGpuFrameNanoseconds;const float absoluteHalo=MaximumAbsoluteBoundaryHalo(current,identity);std::cout<<" boundaryReduction="<<MaximumBoundaryReduction(current,legacy,identity)<<" absoluteBoundaryHalo="<<absoluteHalo<<" depthAwareGpuNs="<<currentGpu<<" legacyGpuNs="<<legacyGpu<<" gpuDeltaNs="<<(currentGpu>legacyGpu?currentGpu-legacyGpu:0);return (std::max)(maximum,absoluteHalo);}
float MaximumAbsoluteBoundaryHalo(const Capture& capture,RenderObjectIdentity3D identity){const auto mask=Mask(capture.objectId,identity);std::vector<float> surface;for(unsigned y=1;y+1<Height;++y)for(unsigned x=1;x+1<Width;++x){const std::size_t i=std::size_t(y)*Width+x;if(mask[i]&&mask[i-1]&&mask[i+1]&&mask[i-Width]&&mask[i+Width])surface.push_back(Luma(capture.image,i));}if(surface.empty())for(std::size_t i=0;i<mask.size();++i)if(mask[i])surface.push_back(Luma(capture.image,i));if(surface.empty())return 1.f;const float reference=*std::ranges::max_element(surface);float maximum{};for(unsigned y=1;y+1<Height;++y)for(unsigned x=1;x+1<Width;++x){const std::size_t i=std::size_t(y)*Width+x;if(mask[i]&&(!mask[i-1]||!mask[i+1]||!mask[i-Width]||!mask[i+Width]))maximum=(std::max)(maximum,Luma(capture.image,i)-reference);}return maximum;}
double BloomRingEnergy(const Capture& capture,RenderObjectIdentity3D identity){
    const auto mask=Mask(capture.objectId,identity);double sum{};std::size_t count{};
    for(unsigned y=8;y+8<Height;++y)for(unsigned x=8;x+8<Width;++x){
        const std::size_t i=std::size_t(y)*Width+x;if(mask[i])continue;bool nearby{};
        for(int dy=-8;dy<=8&&!nearby;++dy)for(int dx=-8;dx<=8;++dx){
            const int radius=(std::max)(std::abs(dx),std::abs(dy));
            const std::size_t sample=std::size_t(int(y)+dy)*Width+std::size_t(int(x)+dx);
            if(radius>=6&&mask[sample]){nearby=true;break;}
        }
        if(nearby){sum+=Luma(capture.image,i);++count;}
    }
    return count?sum/double(count):0;
}
double DisocclusionTrailEnergy(const Capture& production,const Capture& legacy){
    // 等亮度異色的前景移動後露出後景；色彩夾制會保留舊色，只有真正的前幀深度能拒絕它。
    double sum{};std::size_t count{};
    for(unsigned y=116;y<146;++y)for(unsigned x=232;x<254;++x){const std::size_t pixel=std::size_t(y)*Width+x;const auto* current=production.image.rgba.data()+pixel*4;const auto* old=legacy.image.rgba.data()+pixel*4;const double delta=(std::abs(int(old[0])-int(current[0]))+std::abs(int(old[1])-int(current[1]))+std::abs(int(old[2])-int(current[2])))/(3.0*255.0);if(delta>0){sum+=delta;++count;}}
    return count?sum/double(count):0;
}
Residual RegionResidual(const Capture& capture,const Capture& baseline,unsigned left,unsigned top,unsigned right,unsigned bottom){Residual result{};std::size_t count{};for(unsigned y=top;y<bottom;++y)for(unsigned x=left;x<right;++x){const std::size_t pixel=std::size_t(y)*Width+x;const auto* current=capture.image.rgba.data()+pixel*4;const auto* truth=baseline.image.rgba.data()+pixel*4;const double value=(std::abs(int(current[0])-int(truth[0]))+std::abs(int(current[1])-int(truth[1]))+std::abs(int(current[2])-int(truth[2])))/(3.0*255.0);result.mean+=value;result.maximum=(std::max)(result.maximum,value);++count;}if(count)result.mean/=double(count);return result;}
}

int main(){
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    HWND window=CreateWindowExW(0,L"STATIC",L"TemporalDepthRejectionOracle",WS_OVERLAPPED,0,0,Width,Height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 1;
    int result{};
    try{
        D3D12Renderer renderer;renderer.Initialize(window,Width,Height,Width,Height,false);renderer.SetQualitySettings({false,false,false,true});
        const std::array<Vec3,4> opaquePositions{{{-.82f,-.68f,.25f},{-.82f,.68f,.25f},{-.28f,.68f,.25f},{-.28f,-.68f,.25f}}};
        const std::array<std::uint32_t,6> quadIndices{{0,1,2,0,2,3}};
        const std::array<Vec3,9> foliagePositions{{{-.12f,-.7f,.2f},{.02f,.68f,.2f},{.12f,-.7f,.2f},{-.02f,-.25f,.2f},{.36f,.5f,.2f},{.18f,-.55f,.2f},{.05f,-.05f,.2f},{.46f,.12f,.2f},{.16f,-.32f,.2f}}};
        const std::array<std::uint32_t,9> foliageIndices{{0,1,2,3,4,5,6,7,8}};
        const std::array<Vec3,4> highlightPositions{{{.58f,-.14f,.15f},{.58f,.14f,.15f},{.78f,.14f,.15f},{.78f,-.14f,.15f}}};
        const std::array<Vec3,4> backgroundPositions{{{.44f,-.62f,.65f},{.44f,-.28f,.65f},{.4525f,-.28f,.65f},{.4525f,-.62f,.65f}}};
        const std::array<Vec3,4> farBackgroundPositions{{{.44f,.3f,.9998f},{.44f,.65f,.9998f},{.4525f,.65f,.9998f},{.4525f,.3f,.9998f}}};
        const auto opaque=CreateMesh(renderer,opaquePositions,quadIndices),foliage=CreateMesh(renderer,foliagePositions,foliageIndices),highlight=CreateMesh(renderer,highlightPositions,quadIndices),background=CreateMesh(renderer,backgroundPositions,quadIndices),farBackground=CreateMesh(renderer,farBackgroundPositions,quadIndices);
        const auto output=std::filesystem::temp_directory_path()/"aevrynd-temporal-depth-oracle";std::filesystem::create_directories(output);
        const Capture legacy=CaptureScene(renderer,opaque,foliage,highlight,background,farBackground,DepthMutation::CurrentDepth,true,false,output/"legacy.png"),looseFarSky=CaptureScene(renderer,opaque,foliage,highlight,background,farBackground,DepthMutation::LooseFarSky,true,false,output/"loose-far-sky.png"),fixed=CaptureScene(renderer,opaque,foliage,highlight,background,farBackground,DepthMutation::Production,true,false,output/"fixed.png"),repeat=CaptureScene(renderer,opaque,foliage,highlight,background,farBackground,DepthMutation::Production,true,false,output/"repeat.png"),groundTruth=CaptureScene(renderer,opaque,foliage,highlight,background,farBackground,DepthMutation::Production,true,true,output/"ground-truth.png"),bloomOff=CaptureScene(renderer,opaque,foliage,highlight,background,farBackground,DepthMutation::Production,false,false,output/"bloom-off.png");
        const RenderObjectIdentity3D opaqueId{{0x48414c4f,1},1},foliageId{{0x48414c4f,2},1},highlightId{{0x48414c4f,3},1};
        if(!opaque||!foliage||!highlight||!background||!farBackground||!legacy.valid||!looseFarSky.valid||!fixed.valid||!repeat.valid||!groundTruth.valid||!bloomOff.valid)result=2;
        if(!result){
            const float opaqueGain=MaximumBoundaryGain(fixed,legacy,opaqueId),foliageGain=MaximumBoundaryGain(fixed,legacy,foliageId);
            const double bloomOnEnergy=BloomRingEnergy(fixed,highlightId),bloomOffEnergy=BloomRingEnergy(bloomOff,highlightId),legacyBloomEnergy=BloomRingEnergy(legacy,highlightId),disocclusionTrail=DisocclusionTrailEnergy(fixed,legacy);
            const Residual productionResidual=RegionResidual(fixed,groundTruth,238,120,250,141),legacyResidual=RegionResidual(legacy,groundTruth,238,120,250,141),farProductionResidual=RegionResidual(fixed,groundTruth,274,30,293,65),farLooseResidual=RegionResidual(looseFarSky,groundTruth,274,30,293,65);
            const bool deterministic=fixed.image.rgba==repeat.image.rgba;
            const bool depthHistoryResources=fixed.stats.temporalDepthHistoryValid&&fixed.stats.temporalDepthHistoryResidentBytes==std::size_t(Width)*Height*8&&fixed.stats.temporalDepthHistoryAllocations&&fixed.stats.temporalDepthHistoryCopies>=9&&fixed.stats.temporalHistoryResourceFormat==uint32_t(DXGI_FORMAT_R11G11B10_FLOAT)&&fixed.stats.temporalSequenceOutputFormat==fixed.stats.temporalHistoryResourceFormat;
            const char* validationMode{};const bool validationClean=ValidationReceiptMatches(fixed.stats,validationMode);
            std::cout<<"opaqueBoundaryGain="<<opaqueGain<<" foliageBoundaryGain="<<foliageGain<<" bloomOn="<<bloomOnEnergy<<" bloomOff="<<bloomOffEnergy<<" legacyBloom="<<legacyBloomEnergy<<" bloomRetention="<<(legacyBloomEnergy>0?bloomOnEnergy/legacyBloomEnergy:0)<<" disocclusionTrail="<<disocclusionTrail<<" productionResidual="<<productionResidual.mean<<'/'<<productionResidual.maximum<<" legacyResidual="<<legacyResidual.mean<<'/'<<legacyResidual.maximum<<" farProductionResidual="<<farProductionResidual.mean<<'/'<<farProductionResidual.maximum<<" farLooseResidual="<<farLooseResidual.mean<<'/'<<farLooseResidual.maximum<<" deterministic="<<deterministic<<" historyFormat="<<fixed.stats.temporalHistoryResourceFormat<<'/'<<fixed.stats.temporalSequenceOutputFormat<<" depthHistory="<<fixed.stats.temporalDepthHistoryResidentBytes<<'/'<<fixed.stats.temporalDepthHistoryCopies<<" validationMode="<<validationMode<<" validation="<<validationClean<<" gpuNs="<<fixed.stats.lastGpuFrameNanoseconds<<'\n';
            if(!deterministic||!depthHistoryResources||!validationClean||disocclusionTrail<=1.f/255.f||productionResidual.maximum>1.f/255.f||legacyResidual.mean<=1.f/255.f||farProductionResidual.maximum>1.f/255.f||farLooseResidual.maximum<=1.f/255.f||opaqueGain>2.f/255.f||foliageGain>2.f/255.f||bloomOnEnergy<=bloomOffEnergy+.001||bloomOnEnergy<legacyBloomEnergy*.95)result=3;
        }
        renderer.DestroyMesh(farBackground);renderer.DestroyMesh(background);renderer.DestroyMesh(highlight);renderer.DestroyMesh(foliage);renderer.DestroyMesh(opaque);renderer.WaitIdle();
        std::error_code ignored;if(!result)std::filesystem::remove_all(output,ignored);else std::cerr<<"captures="<<output.string()<<'\n';
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=4;}
    DestroyWindow(window);CoUninitialize();return result;
}
