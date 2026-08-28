#include "Aevrynd/Mesh3D.h"
#include "Aevrynd/Renderer.h"
#include <iostream>
#include <limits>
using namespace Aevrynd;

int main(){
    HWND window=CreateWindowExW(0,L"STATIC",L"TemporalAntiAliasingProbe",WS_OVERLAPPED,0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 1;
    try{
        D3D12Renderer renderer;renderer.Initialize(window,128,128,128,128,false);renderer.SetQualitySettings({false,false,false,true});
        const Mesh3D box=Mesh3D::CreateUnitBox();const MeshHandle3D mesh=renderer.CreateMesh(box.vertices,box.indices);
        auto make=[&](const Matrix4& transform,bool cameraCut){DrawList draw;draw.SetViewProjection(Matrix4::Identity());draw.SetBackfaceCulling(false);draw.SetCameraCut3D(cameraCut);draw.MeshInstance(mesh,transform,{1,.6f,.2f,1},{},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false);return draw;};
        renderer.Render(make(Matrix4::Identity(),true),{0,0,0,1});RendererStats first=renderer.GetStats();
        if(!mesh||!first.temporalAntiAliasingEnabled||!first.temporalColorHistoryValid||!first.temporalHistoryValid||first.temporalResolvePasses!=1||first.temporalHistoryResidentBytes!=128ull*128*8||first.temporalHistoryAllocations!=1||first.velocityResidentBytes!=128ull*128*4||first.velocityAllocations!=1||first.velocityPasses!=1){std::cerr<<"Temporal first frame failed\n";return 2;}
        renderer.Render(make(CreateTranslationMatrix({.08f,0,0}),false),{0,0,0,1});RendererStats accumulated=renderer.GetStats();
        if(!accumulated.temporalAntiAliasingEnabled||accumulated.temporalResolvePasses!=2||accumulated.temporalHistoryAllocations!=1||accumulated.velocityPasses!=2){std::cerr<<"Temporal accumulation failed\n";return 3;}
        renderer.Resize(96,64);renderer.Render(make(Matrix4::Identity(),true),{0,0,0,1});RendererStats resized=renderer.GetStats();
        if(!resized.temporalAntiAliasingEnabled||resized.temporalResolvePasses!=3||resized.temporalHistoryResidentBytes!=96ull*64*8||resized.temporalHistoryAllocations!=2||resized.temporalHistoryReleases!=1||resized.velocityResidentBytes!=96ull*64*4||resized.velocityAllocations!=2||resized.velocityReleases!=1){std::cerr<<"Temporal resize lifecycle failed\n";return 4;}
        renderer.SetQualitySettings({false,false,false,false});RendererStats disabled=renderer.GetStats();
        if(disabled.temporalHistoryResidentBytes||disabled.velocityResidentBytes||disabled.temporalColorHistoryValid||disabled.temporalHistoryValid||disabled.temporalHistoryReleases!=2||disabled.velocityReleases!=2||disabled.qualityResourceReleases<2){std::cerr<<"Temporal quality release failed\n";return 5;}
        renderer.SetQualitySettings({false,false,false,true});renderer.Render(make(Matrix4::Identity(),true),{0,0,0,1});renderer.TrimTransientMemory();RendererStats trimmed=renderer.GetStats();
        if(trimmed.temporalHistoryResidentBytes||trimmed.velocityResidentBytes||trimmed.temporalHistoryReleases!=3||trimmed.velocityReleases!=3){std::cerr<<"Temporal trim lifecycle failed\n";return 6;}
        renderer.DestroyMesh(mesh);renderer.WaitIdle();DestroyWindow(window);std::cout<<"resolves="<<trimmed.temporalResolvePasses<<" allocations="<<trimmed.temporalHistoryAllocations<<" releases="<<trimmed.temporalHistoryReleases<<'\n';return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';DestroyWindow(window);return 7;}
}
