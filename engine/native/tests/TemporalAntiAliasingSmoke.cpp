#include "Aevrynd/Mesh3D.h"
#include "Aevrynd/Renderer.h"
#include "Aevrynd/TemporalMotion3D.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace Aevrynd;

static bool Near(float left,float right,float tolerance=.0001f){return std::abs(left-right)<=tolerance;}
static float MaximumMagnitude(const VelocityCapture3D& capture){float result{};for(const Vec2 velocity:capture.pixels)result=(std::max)(result,std::abs(velocity.x)+std::abs(velocity.y));return result;}

int main(){
    HWND window=CreateWindowExW(0,L"STATIC",L"TemporalAntiAliasingProbe",WS_OVERLAPPED,0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 1;
    try{
        D3D12Renderer renderer;renderer.Initialize(window,128,128,128,128,false);renderer.SetQualitySettings({false,false,false,true});
        DrawList twoDimensional;renderer.Render(twoDimensional,{0,0,0,1});const RendererStats twoDimensionalStats=renderer.GetStats();
        if(twoDimensionalStats.temporalJitterEnabled||twoDimensionalStats.temporalHistoryUsed||twoDimensionalStats.temporalColorHistoryValid||twoDimensionalStats.temporalHistoryResidentBytes||twoDimensionalStats.velocityResidentBytes){std::cerr<<"Temporal 2D isolation failed\n";return 2;}
        const Mesh3D box=Mesh3D::CreateUnitBox();const MeshHandle3D mesh=renderer.CreateMesh(box.vertices,box.indices);
        auto make=[&](const Matrix4& transform,bool cameraCut){DrawList draw;draw.SetViewProjection(Matrix4::Identity());draw.SetBackfaceCulling(false);draw.SetCameraCut3D(cameraCut);draw.MeshInstance(mesh,transform,{1,.6f,.2f,1},{},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false);return draw;};
        const TemporalJitterSample3D jitter0=CalculateTemporalJitterSample3D(0),jitter1=CalculateTemporalJitterSample3D(1);
        renderer.RequestVelocityReadback3D();renderer.Render(make(Matrix4::Identity(),true),{0,0,0,1});const auto cutVelocity=renderer.TakeVelocityReadback3D();RendererStats first=renderer.GetStats();
        if(!mesh||!cutVelocity||MaximumMagnitude(*cutVelocity)>.01f||!first.temporalAntiAliasingEnabled||!first.temporalJitterEnabled||first.temporalHistoryUsed||!first.temporalColorHistoryValid||!first.temporalHistoryValid||first.temporalJitterSequenceIndex!=0||!Near(first.currentTemporalJitterPixels.x,jitter0.pixelOffset.x)||!Near(first.currentTemporalJitterPixels.y,jitter0.pixelOffset.y)||first.temporalJitteredFrames!=1||first.temporalResolvePasses!=1||first.temporalHistoryResidentBytes!=128ull*128*8||first.temporalHistoryAllocations!=1||first.velocityResidentBytes!=128ull*128*4||first.velocityAllocations!=1||first.velocityPasses!=1){std::cerr<<"Temporal first frame failed\n";return 3;}
        renderer.RequestVelocityReadback3D();renderer.Render(make(Matrix4::Identity(),false),{0,0,0,1});const auto stationaryVelocity=renderer.TakeVelocityReadback3D();RendererStats accumulated=renderer.GetStats();
        if(!stationaryVelocity||MaximumMagnitude(*stationaryVelocity)>.01f||!accumulated.temporalAntiAliasingEnabled||!accumulated.temporalJitterEnabled||!accumulated.temporalHistoryUsed||accumulated.temporalJitterSequenceIndex!=1||!Near(accumulated.currentTemporalJitterPixels.x,jitter1.pixelOffset.x)||!Near(accumulated.currentTemporalJitterPixels.y,jitter1.pixelOffset.y)||!Near(accumulated.previousTemporalJitterPixels.x,jitter0.pixelOffset.x)||!Near(accumulated.previousTemporalJitterPixels.y,jitter0.pixelOffset.y)||accumulated.temporalJitteredFrames!=2||accumulated.temporalResolvePasses!=2||accumulated.temporalHistoryAllocations!=1||accumulated.velocityPasses!=2){std::cerr<<"Temporal accumulation failed\n";return 4;}
        renderer.Render(make(Matrix4::Identity(),true),{0,0,0,1});const RendererStats cameraCut=renderer.GetStats();
        if(cameraCut.temporalHistoryUsed||cameraCut.temporalJitterSequenceIndex!=0||!Near(cameraCut.currentTemporalJitterPixels.x,jitter0.pixelOffset.x)||!Near(cameraCut.currentTemporalJitterPixels.y,jitter0.pixelOffset.y)||cameraCut.temporalJitteredFrames!=3||cameraCut.temporalJitterResets<1||cameraCut.temporalResolvePasses!=3){std::cerr<<"Temporal camera cut reset failed\n";return 5;}
        renderer.Resize(96,64);renderer.Render(make(Matrix4::Identity(),true),{0,0,0,1});RendererStats resized=renderer.GetStats();
        if(!resized.temporalAntiAliasingEnabled||resized.temporalHistoryUsed||resized.temporalJitterSequenceIndex!=0||resized.temporalJitteredFrames!=4||resized.temporalJitterResets<2||resized.temporalResolvePasses!=4||resized.temporalHistoryResidentBytes!=96ull*64*8||resized.temporalHistoryAllocations!=2||resized.temporalHistoryReleases!=1||resized.velocityResidentBytes!=96ull*64*4||resized.velocityAllocations!=2||resized.velocityReleases!=1){std::cerr<<"Temporal resize lifecycle failed\n";return 6;}
        renderer.SetQualitySettings({false,false,false,false});RendererStats disabled=renderer.GetStats();
        if(disabled.temporalHistoryResidentBytes||disabled.velocityResidentBytes||disabled.temporalColorHistoryValid||disabled.temporalHistoryValid||disabled.temporalHistoryReleases!=2||disabled.velocityReleases!=2||disabled.qualityResourceReleases<2){std::cerr<<"Temporal quality release failed\n";return 7;}
        renderer.SetQualitySettings({false,false,false,true});renderer.Render(make(Matrix4::Identity(),true),{0,0,0,1});renderer.TrimTransientMemory();RendererStats trimmed=renderer.GetStats();
        if(trimmed.temporalHistoryResidentBytes||trimmed.velocityResidentBytes||trimmed.temporalHistoryReleases!=3||trimmed.velocityReleases!=3){std::cerr<<"Temporal trim lifecycle failed\n";return 8;}
        renderer.DestroyMesh(mesh);renderer.WaitIdle();DestroyWindow(window);std::cout<<"resolves="<<trimmed.temporalResolvePasses<<" allocations="<<trimmed.temporalHistoryAllocations<<" releases="<<trimmed.temporalHistoryReleases<<'\n';return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';DestroyWindow(window);return 9;}
}
