#include <Aevrynd/Camera.h>
#include <Aevrynd/Renderer.h>
#include <Aevrynd/NativeMaterialDds3D.h>
#include <Windows.h>
#include <shellapi.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Aevrynd;
using Microsoft::WRL::ComPtr;
namespace {
constexpr unsigned Width=1280, Height=720, RoiSide=80;
constexpr std::array<unsigned,2> RoiLeft{384,816};
constexpr unsigned RoiTop=320;
const std::array<RenderObjectIdentity3D,2> Ids{{{{0x310u,1u},1u},{{0x310u,2u},1u}}};
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T> void Raw(const std::filesystem::path& path,const std::vector<T>& data){
    std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()*sizeof(T)));
    Need(bool(file),"raw output failed");
}
std::vector<std::uint8_t> Decode(const std::filesystem::path& path){
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    Need(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))),"WIC factory failed");
    Need(SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder)),"PNG decode failed");
    Need(SUCCEEDED(decoder->GetFrame(0,&frame)),"PNG frame failed");
    unsigned width{},height{};Need(SUCCEEDED(frame->GetSize(&width,&height))&&width==Width&&height==Height,"PNG shape mismatch");
    Need(SUCCEEDED(factory->CreateFormatConverter(&converter)),"WIC converter failed");
    Need(SUCCEEDED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)),"RGBA conversion failed");
    std::vector<std::uint8_t> result(static_cast<std::size_t>(Width)*Height*4u);
    Need(SUCCEEDED(converter->CopyPixels(nullptr,Width*4u,static_cast<UINT>(result.size()),result.data())),"RGBA read failed");
    return result;
}
MeshHandle3D Card(D3D12Renderer& renderer,unsigned index){
    const float center=index==0u?-1.2f:1.2f;
    const auto normal=PackNormalSnorm8({0,0,-1}), color=PackColorRgba8({1,1,1,1});
    const float leftU=index==0u?0.f:8.f,rightU=8.f-leftU;
    const std::array<Vertex3D,4> vertices{{
        {center-.7f,-.7f,0,normal,color,leftU,8},
        {center-.7f,.7f,0,normal,color,leftU,0},
        {center+.7f,.7f,0,normal,color,rightU,0},
        {center+.7f,-.7f,0,normal,color,rightU,8}}};
    const std::array<std::uint32_t,6> indices{0,1,2,0,2,3};
    // 靜態 Vertex3D 沒有外加 tangent，真實走 UvNormal derivative 分支。
    return renderer.CreateMesh(vertices,indices);
}
bool Quality(RendererQualitySettings3D q){return q.directionalShadows&&q.ambientOcclusion&&q.multisampling&&q.postProcessAntiAliasing;}
void Debug(const RendererStats& stats,bool gbv){
    Need(stats.debugErrorCount==0&&stats.debugWarningCount==0,"debug messages present");
    Need(stats.gpuValidationRequested==gbv&&stats.gpuValidationAvailable==gbv
        &&stats.gpuValidationExecuted==gbv&&stats.gpuValidationPassed==gbv&&stats.debugLayerEnabled==gbv,"validation mode mismatch");
}
void Run(const std::filesystem::path& output,const std::filesystem::path& assets,int sign,bool gbv,const std::string& normalSha){
    Need(sign==1||sign==-1,"light sign invalid");Need(!std::filesystem::exists(output),"output already exists");
    std::filesystem::create_directories(output);
    NativeMaterialDds3D normalDds{};
    Need(LoadNativeMaterialDds3D(assets/"normal.dds",NativeMaterialDdsRole3D::Normal,16,true,normalDds,nullptr,normalSha,516,true),"normal DDS integrity failed");
    Need(normalDds.width==16&&normalDds.height==16&&normalDds.mipCount==5&&normalDds.format==DXGI_FORMAT_BC5_UNORM,"normal full mip mismatch");
    for(const auto& mip:normalDds.mips){
        Need(mip.pixels.size()%16u==0,"BC5 block shape invalid");
        for(std::size_t offset=0;offset<mip.pixels.size();offset+=16u){
            Need(mip.pixels[offset]==179&&mip.pixels[offset+1]==179&&mip.pixels[offset+8]==153&&mip.pixels[offset+9]==153,"BC5 endpoints invalid");
            for(unsigned byte=2;byte<8;++byte)Need(mip.pixels[offset+byte]==0&&mip.pixels[offset+byte+8]==0,"BC5 indices invalid");
        }
    }
    HWND window=CreateWindowExW(0,L"STATIC",L"V310 bounded UV normal fixture",WS_OVERLAPPED,0,0,Width,Height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Need(window!=nullptr,"hidden window failed");
    try {
        D3D12Renderer renderer;renderer.Initialize(window,Width,Height,Width,Height,false);
        Need(renderer.RenderWidth()==Width&&renderer.RenderHeight()==Height,"renderer initialization size mismatch");
        renderer.SetQualitySettings({true,true,true,true});
        std::array<MeshHandle3D,2> meshes{Card(renderer,0),Card(renderer,1)};
        Need(bool(meshes[0])&&bool(meshes[1]),"mesh creation failed");
        MaterialTextureSetDefinition3D material{};material.baseColorPath=assets/"base.bmp";material.roughnessPath=assets/"rough.bmp";
        material.normalPath=assets/"normal.dds";material.maximumResolution=16;
        material.integrity[2]={516,normalSha};
        const auto texture=renderer.CreateMaterialTextureSet(material);Need(bool(texture),"material creation failed");
        Camera3D camera{{0,0,-4},{0,0,0},{0,1,0}};
        camera.projection.mode=ProjectionMode::Orthographic;camera.projection.orthographicHeight=4.f;
        const Vec3 incoming=Normalize(Vec3{.8f*static_cast<float>(sign),-.3f,-.5f});
        DrawList draw;draw.SetViewProjection(CreateViewProjectionMatrix(camera,static_cast<float>(Width)/Height));
        Atmosphere3D atmosphere{};atmosphere.enabled=false;atmosphere.cameraPosition=camera.position;
        draw.SetBackfaceCulling(false);draw.SetAtmosphere(atmosphere);draw.SetBloom({false});draw.SetTimeOfDayExposure({false});
        draw.SetDirectionalLight({incoming*-1.f,.05f,{1,1,1,1},1.f});
        EnvironmentLightingBlendSettings3D blend{};blend.analyticDiffuseWeight=1;blend.iblDiffuseWeight=0;
        blend.analyticSpecularWeight=1;blend.iblSpecularWeight=0;blend.specularIntensity=0;blend.diffuseIntensity=0;
        Need(draw.SetEnvironmentLightingBlend(blend),"light blend rejected");
        for(unsigned index=0;index<2;++index)Need(draw.MeshInstance(meshes[index],Matrix4::Identity(),{1,1,1,1},{1,0,0,1},MeshRenderLayer3D::Opaque,texture,1.f,true,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),true,1.f,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,Matrix4::Identity(),false,1.f,MaterialAlphaMode3D::Opaque,.5f,false,Ids[index]),"draw rejected");
        for(unsigned frame=0;frame<30;++frame)renderer.Render(draw,{0,0,0,1});
        Need(renderer.RequestObjectIdReadback3D({310,311,312}),"object ID request failed");
        Need(renderer.RequestDepthReadback3D({310,311,312}),"depth request failed");
        Need(renderer.CaptureNextFrame(output/"capture.png"),"PNG capture request failed");
        renderer.Render(draw,{0,0,0,1});renderer.WaitIdle();
        const auto ids=renderer.TakeObjectIdReadback3D();const auto depth=renderer.TakeDepthReadback3D();
        Need(ids&&ids->Complete()&&depth&&depth->Complete(),"public readback incomplete");
        Need(ids->width==Width&&ids->height==Height&&depth->width==Width&&depth->height==Height,"public readback shape invalid");
        Need(ids->rasterProvenance.frameSerial==depth->rasterProvenance.frameSerial,"readbacks from different frame");
        const auto rgba=Decode(output/"capture.png");
        std::array<double,2> means{};
        for(unsigned plane=0;plane<2;++plane){
            std::vector<std::uint8_t> roi;roi.reserve(RoiSide*RoiSide*4u);
            for(unsigned y=RoiTop;y<RoiTop+RoiSide;++y)for(unsigned x=RoiLeft[plane];x<RoiLeft[plane]+RoiSide;++x){
                const std::size_t pixel=static_cast<std::size_t>(y)*Width+x;
                const auto* identity=ids->Resolve(ids->pixels[pixel]);
                Need(identity&&*identity==Ids[plane],"fixed ROI object identity mismatch");
                Need(std::isfinite(depth->pixels[pixel])&&depth->pixels[pixel]>0&&depth->pixels[pixel]<1,"fixed ROI depth invalid");
                for(unsigned channel=0;channel<4;++channel)roi.push_back(rgba[pixel*4u+channel]);
                Need(rgba[pixel*4u+3]==255,"PNG alpha invalid");
                means[plane]+=(.2126*rgba[pixel*4u]+.7152*rgba[pixel*4u+1]+.0722*rgba[pixel*4u+2]);
            }
            means[plane]/=static_cast<double>(RoiSide*RoiSide);
            Raw(output/(plane==0?"standard.roi.rgba":"mirror.roi.rgba"),roi);
        }
        Raw(output/"object-id.u32",ids->pixels);Raw(output/"depth.f32",depth->pixels);
        const auto rendered=renderer.GetStats();Debug(rendered,gbv);
        Need(Quality(rendered.requestedQuality)&&Quality(rendered.effectiveQuality),"quality differs from fixed all-on fixture");
        Need(rendered.residentMeshes==2&&rendered.residentMaterialTextureSets==1,"resident resource count mismatch");
        renderer.DestroyMesh(meshes[0]);renderer.DestroyMesh(meshes[1]);renderer.DestroyMaterialTextureSet(texture);renderer.WaitIdle();
        const auto cleaned=renderer.GetStats();Debug(cleaned,gbv);
        Need(cleaned.residentMeshes==0&&cleaned.residentMaterialTextureSets==0,"renderer cleanup failed");
        const float nx=179.f/255.f*2.f-1.f,ny=153.f/255.f*2.f-1.f,nz=std::sqrt(1.f-nx*nx-ny*ny);
        const std::array<Vec3,3> reference{{{nx,-ny,-nz},{-nx,-ny,-nz},{nx,ny,-nz}}};
        std::ofstream receipt(output/"receipt.json");receipt<<std::setprecision(17)<<std::boolalpha;
        receipt<<"{\"schema\":\"v310-public-png-1\",\"light_sign\":"<<sign<<",\"gbv\":"<<gbv
            <<",\"width\":1280,\"height\":720,\"warmup_frames\":30,\"capture_frame\":31,\"plane_vertices\":8,\"plane_indices\":12"
            <<",\"roi\":[[384,320,80,80],[816,320,80,80]],\"means\":["<<means[0]<<","<<means[1]<<"]"
            <<",\"cpu_lambert\":["<<Dot(reference[0],incoming)<<","<<Dot(reference[1],incoming)<<","<<Dot(reference[2],incoming)<<"]"
            <<",\"normal_xy\":["<<nx<<","<<ny<<"],\"normal_mips\":"<<normalDds.mipCount
            <<",\"object_frame\":"<<ids->rasterProvenance.frameSerial<<",\"depth_frame\":"<<depth->rasterProvenance.frameSerial
            <<",\"object_pixels\":"<<ids->semanticPixelCount<<",\"depth_pixels\":"<<depth->foregroundPixelCount
            <<",\"adapter_low\":"<<rendered.adapterLuidLow<<",\"adapter_high\":"<<rendered.adapterLuidHigh<<",\"adapter_valid\":"<<rendered.adapterLuidValid
            <<",\"draw_calls\":"<<rendered.lastDrawCalls<<",\"quality\":[true,true,true,true]"
            <<",\"gpu_flags\":["<<rendered.gpuValidationRequested<<","<<rendered.gpuValidationAvailable<<","<<rendered.gpuValidationExecuted<<","<<rendered.gpuValidationPassed<<","<<rendered.debugLayerEnabled<<"]"
            <<",\"debug_errors\":"<<cleaned.debugErrorCount<<",\"debug_warnings\":"<<cleaned.debugWarningCount
            <<",\"resident_before\":["<<rendered.residentMeshes<<","<<rendered.residentMaterialTextureSets<<"],\"resident_after\":["<<cleaned.residentMeshes<<","<<cleaned.residentMaterialTextureSets<<"]}\n";
        Need(bool(receipt),"receipt write failed");
    } catch(...){DestroyWindow(window);throw;}
    DestroyWindow(window);
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    int argc{};LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    try {
        Need(SUCCEEDED(com)&&argv&&argc==6,"arguments or COM invalid");
        const std::wstring mode=argv[4];Need(mode==L"normal"||mode==L"gbv","mode invalid");
        wchar_t flag[8]{};Need(GetEnvironmentVariableW(L"AEVRYND_GPU_VALIDATION",flag,8)>0,"validation flag missing");
        Need(std::wstring(flag)==(mode==L"gbv"?L"1":L"0"),"validation flag differs from mode");
        const std::wstring wideSha=argv[5];Need(wideSha.size()==64,"normal SHA length invalid");
        std::string sha;sha.reserve(64);
        for(wchar_t character:wideSha){
            Need((character>=L'0'&&character<=L'9')||(character>=L'a'&&character<=L'f'),"normal SHA lowercase hex invalid");
            sha.push_back(static_cast<char>(character));
        }
        Run(argv[1],argv[2],std::stoi(argv[3]),mode==L"gbv",sha);
        LocalFree(argv);CoUninitialize();return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';if(argv)LocalFree(argv);if(SUCCEEDED(com))CoUninitialize();return 1;}
}
