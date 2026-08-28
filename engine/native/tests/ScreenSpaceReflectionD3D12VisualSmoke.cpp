#include <Aevrynd/Camera.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/Renderer.h>
#include <Windows.h>
#include <Psapi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace Aevrynd;

namespace {
uint64_t Median(std::vector<uint64_t> values){if(values.empty())return 0;std::ranges::sort(values);return values[values.size()/2];}
uint64_t HashFile(const std::filesystem::path& path){std::ifstream stream(path,std::ios::binary);uint64_t hash=1469598103934665603ull;char value{};while(stream.get(value))hash=(hash^uint8_t(value))*1099511628211ull;return hash;}
float MaximumVelocity(const VelocityCapture3D& capture){float value{};for(Vec2 velocity:capture.pixels)value=(std::max)(value,std::sqrt(velocity.x*velocity.x+velocity.y*velocity.y));return value;}
}

int main(){
    HWND window=CreateWindowExW(0,L"STATIC",L"AevryndScreenSpaceReflectionVisual",WS_OVERLAPPED,0,0,1920,1080,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)return 1;
    int result{};{
        D3D12Renderer renderer;renderer.Initialize(window,1920,1080,1920,1080,false);renderer.SetQualitySettings({false,false,false,false});
        DrawList pure2D{DrawListCapacityProfile::Minimal2D};pure2D.Quad({20,20},{24,24},{1,1,1,1});renderer.Render(pure2D,{0,0,0,1});renderer.WaitIdle();const auto pure=renderer.GetStats();if(pure.screenSpaceReflectionsEnabled||pure.screenSpaceReflectionResidentBytes||pure.waterOpaqueSceneColorBytes){DestroyWindow(window);return 2;}
        const Mesh3D boxSource=Mesh3D::CreateUnitBox();const auto box=renderer.CreateMesh(boxSource.vertices,boxSource.indices);if(!box){DestroyWindow(window);return 3;}
        Camera3D camera{{0,2.4f,-5.6f},{0,.55f,.8f},{0,1,0},{}};camera.projection.farPlane=100.f;
        auto makeDraw=[&](){DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetViewProjection(CreateViewProjectionMatrix(camera,16.f/9.f));draw.SetBackfaceCulling(true);draw.SetAtmosphere({{.16f,.2f,.27f,1},camera.position,80.f,180.f,false});draw.SetEnvironmentTime(2.75f);draw.SetDirectionalLight({{-.55f,-1.f,-.25f},.16f,{1.f,.82f,.68f,1},3.f});draw.SetWaterOptics({false,.018f,.35f,.035f});draw.MeshInstance(box,CreateTransformMatrix({0,-.18f,1.1f},{},{9.f,.18f,10.f}),{.025f,.15f,.19f,.82f},{.07f,0,0,1},MeshRenderLayer3D::Transparent,{},.25f,false,MaterialClass3D::Water,.48f,false);draw.MeshInstance(box,CreateTransformMatrix({-.9f,.9f,1.55f},{},{.75f,2.2f,.75f}),{.95f,.17f,.055f,1},{.18f,.65f,0,1});draw.MeshInstance(box,CreateTransformMatrix({.9f,.7f,2.15f},{},{.9f,1.7f,.9f}),{.12f,.42f,.95f,1},{.12f,.82f,0,1});draw.MeshInstance(box,CreateTransformMatrix({0,-.42f,3.4f},{},{8.f,.3f,2.f}),{.18f,.2f,.23f,1},{.72f,0,0,1});return draw;};
        DrawList draw=makeDraw();const std::filesystem::path captures=AEVRYND_TEST_ARTIFACT_DIR;std::filesystem::create_directories(captures);const auto offPath=captures/L"ssr-disabled.png",onPath=captures/L"ssr-enabled.png",repeatPath=captures/L"ssr-enabled-repeat.png",taaPath=captures/L"ssr-taa.png";
        struct Sample{uint64_t gpu{},cpu{};size_t workingSet{};RendererStats stats{};};auto sample=[&](bool enabled,const std::filesystem::path& path){auto settings=draw.ScreenSpaceReflections();settings.enabled=enabled;draw.SetScreenSpaceReflections(settings);std::vector<uint64_t> gpu,cpu;for(int frame=0;frame<32;++frame){const auto begin=std::chrono::steady_clock::now();renderer.Render(draw,{.025f,.035f,.055f,1});const auto end=std::chrono::steady_clock::now();if(frame>=16){cpu.push_back(uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()));if(renderer.GetStats().lastGpuFrameNanoseconds)gpu.push_back(renderer.GetStats().lastGpuFrameNanoseconds);}}renderer.CaptureNextFrame(path);renderer.Render(draw,{.025f,.035f,.055f,1});renderer.WaitIdle();PROCESS_MEMORY_COUNTERS_EX memory{};GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));return Sample{Median(std::move(gpu)),Median(std::move(cpu)),size_t(memory.WorkingSetSize),renderer.GetStats()};};
        const auto off=sample(false,offPath),on=sample(true,onPath),repeat=sample(true,repeatPath);const uint64_t offHash=HashFile(offPath),onHash=HashFile(onPath),repeatHash=HashFile(repeatPath),gpuDelta=on.gpu>off.gpu?on.gpu-off.gpu:0;
        renderer.SetQualitySettings({true,true,false,true});if(!renderer.RequestVelocityReadback3D())result=4;for(int frame=0;frame<12;++frame)renderer.Render(draw,{.025f,.035f,.055f,1});const auto velocity=renderer.TakeVelocityReadback3D();renderer.CaptureNextFrame(taaPath);renderer.Render(draw,{.025f,.035f,.055f,1});renderer.WaitIdle();const auto integrated=renderer.GetStats();
        const auto allocationsBeforeResize=integrated.waterOpaqueSceneColorAllocations,releasesBeforeResize=integrated.waterOpaqueSceneColorReleases;renderer.Resize(1280,720);renderer.Render(draw,{.025f,.035f,.055f,1});renderer.WaitIdle();const auto resized=renderer.GetStats();auto disabled=draw.ScreenSpaceReflections();disabled.enabled=false;draw.SetScreenSpaceReflections(disabled);renderer.Render(draw,{.025f,.035f,.055f,1});renderer.WaitIdle();renderer.TrimTransientMemory();const auto trimmed=renderer.GetStats();
        std::cout<<"ScreenSpaceReflectionD3D12 gpuOffUs="<<off.gpu/1000<<" gpuOnUs="<<on.gpu/1000<<" gpuDeltaUs="<<gpuDelta/1000<<" gpuRepeatUs="<<repeat.gpu/1000<<" cpuOffUs="<<off.cpu/1000<<" cpuOnUs="<<on.cpu/1000<<" workingSetBytes="<<on.workingSet<<" sharedSceneBytes="<<on.stats.waterOpaqueSceneColorBytes<<" ssrResidentBytes="<<on.stats.screenSpaceReflectionResidentBytes<<" offHash="<<offHash<<" onHash="<<onHash<<" repeatHash="<<repeatHash<<" maxStaticVelocity="<<(velocity?MaximumVelocity(*velocity):-1.f)<<" captures="<<captures.string()<<'\n';
        if(!off.gpu||!on.gpu||!repeat.gpu||gpuDelta>1000000ull||off.stats.screenSpaceReflectionsEnabled||off.stats.waterOpaqueSceneColorBytes||!on.stats.screenSpaceReflectionsEnabled||on.stats.screenSpaceReflectionSampleCount!=12||on.stats.screenSpaceReflectionResidentBytes||on.stats.waterOpaqueSceneColorBytes!=1920ull*1080*8||offHash==onHash||onHash!=repeatHash||!velocity||MaximumVelocity(*velocity)>.02f||!integrated.temporalAntiAliasingEnabled||!integrated.screenSpaceReflectionsEnabled||resized.waterOpaqueSceneColorAllocations!=allocationsBeforeResize+1||resized.waterOpaqueSceneColorReleases!=releasesBeforeResize+1||resized.waterOpaqueSceneColorBytes!=1280ull*720*8||trimmed.screenSpaceReflectionsEnabled||trimmed.screenSpaceReflectionResidentBytes||trimmed.waterOpaqueSceneColorBytes)result=5;
        renderer.DestroyMesh(box);renderer.WaitIdle();if(renderer.GetStats().residentMeshBytes)result=6;
    }DestroyWindow(window);return result;
}
