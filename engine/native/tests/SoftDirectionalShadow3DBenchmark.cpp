#include <Aevrynd/Renderer.h>
#include <Aevrynd/Camera.h>
#include <Aevrynd/Mesh3D.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <psapi.h>
#include <vector>

#pragma comment(lib,"Psapi.lib")
using namespace Aevrynd;

namespace {
uint64_t Median(std::vector<uint64_t> values){std::ranges::sort(values);return values.empty()?0:values[values.size()/2];}
struct Sample {uint64_t gpuNanoseconds{},cpuNanoseconds{};size_t workingSetBytes{},trackedBytes{};RendererStats stats{};};
}

int main(int argc,char** argv){
    const std::filesystem::path output=argc>1?std::filesystem::path{argv[1]}:std::filesystem::current_path();std::filesystem::create_directories(output);
    HWND window=CreateWindowExW(0,L"STATIC",L"AevryndSoftDirectionalShadowBenchmark",WS_OVERLAPPED,0,0,960,540,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)return 1;
    int result{};{
        D3D12Renderer renderer;renderer.Initialize(window,960,540,960,540,false);renderer.SetQualitySettings({true,false,false,false});
        DrawList pure2D{DrawListCapacityProfile::Minimal2D};pure2D.Quad({8,8},{32,32},{.2f,.5f,.8f,1});renderer.Render(pure2D,{0,0,0,1});renderer.WaitIdle();const RendererStats pure2DStats=renderer.GetStats();
        if(pure2DStats.directionalShadowMapBytes||pure2DStats.softDirectionalShadowsEnabled||pure2DStats.softDirectionalShadowBlockerSamples){std::cerr<<"Pure 2D allocated or executed soft shadow resources\n";result=2;}
        const Mesh3D box=Mesh3D::CreateUnitBox();const auto sceneMesh=renderer.CreateMesh(box.vertices,box.indices);
        const Camera3D camera{{7.f,6.5f,-10.f},{0,.5f,3.f},{0,1,0},{ProjectionMode::Perspective,.82f,20.f,.1f,100.f}};
        DrawList draw;draw.SetViewProjection(CreateViewProjectionMatrix(camera,16.f/9.f));draw.SetBackfaceCulling(true);draw.SetAtmosphere({{.12f,.16f,.2f,1},camera.position,1000,2000,false});draw.SetDirectionalLight({{.55f,-1.f,.2f},.08f,{1.f,.92f,.78f,1},2.2f});draw.MeshInstance(sceneMesh,CreateTransformMatrix({0,-.2f,3},{},{12,.25f,12}),{.48f,.52f,.44f,1},{.84f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,12.f,true);draw.MeshInstance(sceneMesh,CreateTransformMatrix({-1.8f,1.15f,2},{},{2.2f,2.3f,2.2f}),{.55f,.22f,.08f,1},{.42f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,10.f,true);draw.MeshInstance(sceneMesh,CreateTransformMatrix({2.f,.85f,5},{},{1.5f,1.7f,1.5f}),{.18f,.34f,.5f,1},{.35f,.1f,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,14.f,true);
        auto sample=[&](bool enabled,const std::filesystem::path& capture){draw.SetSoftDirectionalShadows({enabled,24.f,128.f,32.f});std::vector<uint64_t> gpu,cpu;for(int frame=0;frame<80;++frame){const auto begin=std::chrono::steady_clock::now();renderer.Render(draw,{.015f,.025f,.04f,1});const auto end=std::chrono::steady_clock::now();if(frame>=24){cpu.push_back(uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()));const auto timing=renderer.GetStats().lastGpuFrameNanoseconds;if(timing)gpu.push_back(timing);}}renderer.CaptureNextFrame(capture);renderer.Render(draw,{.015f,.025f,.04f,1});renderer.WaitIdle();PROCESS_MEMORY_COUNTERS_EX memory{};GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));return Sample{Median(std::move(gpu)),Median(std::move(cpu)),size_t(memory.WorkingSetSize),renderer.MemoryStats().trackedCapacityBytes,renderer.GetStats()};};
        const auto disabled=sample(false,output/"soft-directional-shadow-disabled.png"),enabled=sample(true,output/"soft-directional-shadow-enabled.png");
        const int64_t gpuDelta=int64_t(enabled.gpuNanoseconds)-int64_t(disabled.gpuNanoseconds),cpuDelta=int64_t(enabled.cpuNanoseconds)-int64_t(disabled.cpuNanoseconds),workingSetDelta=int64_t(enabled.workingSetBytes)-int64_t(disabled.workingSetBytes),trackedDelta=int64_t(enabled.trackedBytes)-int64_t(disabled.trackedBytes);
        renderer.SetQualitySettings({false,false,false,false});renderer.Render(pure2D,{0,0,0,1});renderer.WaitIdle();const RendererStats released=renderer.GetStats();renderer.SetQualitySettings({true,false,false,false});renderer.Render(draw,{.015f,.025f,.04f,1});renderer.WaitIdle();const RendererStats restored=renderer.GetStats();
        std::cout<<"soft_shadow_off_gpu_ns="<<disabled.gpuNanoseconds<<" soft_shadow_on_gpu_ns="<<enabled.gpuNanoseconds<<" gpu_delta_ns="<<gpuDelta<<" cpu_off_ns="<<disabled.cpuNanoseconds<<" cpu_on_ns="<<enabled.cpuNanoseconds<<" cpu_delta_ns="<<cpuDelta<<" working_set_delta_bytes="<<workingSetDelta<<" tracked_delta_bytes="<<trackedDelta<<" shadow_bytes="<<enabled.stats.directionalShadowMapBytes<<" blocker_samples="<<enabled.stats.softDirectionalShadowBlockerSamples<<" captures="<<enabled.stats.successfulFrameCaptures<<" releases="<<released.directionalShadowMapReleases<<" restored_allocations="<<restored.directionalShadowMapAllocations<<'\n';
        if(!sceneMesh||!disabled.gpuNanoseconds||!enabled.gpuNanoseconds||disabled.stats.softDirectionalShadowsEnabled||disabled.stats.softDirectionalShadowBlockerSamples||!enabled.stats.softDirectionalShadowsEnabled||enabled.stats.softDirectionalShadowBlockerSamples!=8||enabled.stats.softDirectionalShadowBlockerSearchRadiusTexels!=24.f||enabled.stats.softDirectionalShadowLightSourceRadiusTexels!=128.f||enabled.stats.softDirectionalShadowMaximumPenumbraRadiusTexels!=32.f||enabled.stats.directionalShadowMapBytes!=disabled.stats.directionalShadowMapBytes||enabled.trackedBytes!=disabled.trackedBytes||enabled.stats.successfulFrameCaptures<2||released.directionalShadowMapBytes||released.softDirectionalShadowsEnabled||released.directionalShadowMapReleases!=1||restored.directionalShadowMapAllocations!=2||restored.directionalShadowMapReleases!=1||!restored.softDirectionalShadowsEnabled)result=3;
        renderer.DestroyMesh(sceneMesh);renderer.WaitIdle();
    }DestroyWindow(window);return result;
}
