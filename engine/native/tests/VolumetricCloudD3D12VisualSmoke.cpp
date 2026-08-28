#include <Aevrynd/Camera.h>
#include <Aevrynd/EnvironmentMeshes3D.h>
#include <Aevrynd/Renderer.h>
#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace Aevrynd;

namespace {
uint64_t Median(std::vector<uint64_t> values){if(values.empty())return 0;std::ranges::sort(values);return values[values.size()/2];}
uint64_t HashFile(const std::filesystem::path& path){std::ifstream stream(path,std::ios::binary);uint64_t hash=1469598103934665603ull;char value{};while(stream.get(value))hash=(hash^uint8_t(value))*1099511628211ull;return hash;}
}

int main(){
    HWND window=CreateWindowExW(0,L"STATIC",L"AevryndVolumetricCloudVisual",WS_OVERLAPPED,0,0,1920,1080,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)return 1;
    int result{};{
        D3D12Renderer renderer;renderer.Initialize(window,1920,1080,1920,1080,false);renderer.SetQualitySettings({false,false,false,false});
        const Mesh3D sky=CreateSkyDomeMesh3D(32,16);const MeshHandle3D skyMesh=renderer.CreateMesh(sky.vertices,sky.indices);if(!skyMesh){DestroyWindow(window);return 2;}
        Camera3D camera{{0,80,0},{0,170,240},{0,1,0},{}};camera.projection.farPlane=1000.f;
        DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetViewProjection(CreateViewProjectionMatrix(camera,16.f/9.f));draw.SetBackfaceCulling(false);draw.SetAtmosphere({{.3f,.45f,.62f,1},camera.position,10000,20000,true});draw.SetDirectionalLight({{-.42f,-1.f,.28f},.2f,{1.f,.9f,.72f,1},2.f});draw.MeshInstance(skyMesh,CreateTransformMatrix(camera.position,{}, {500,500,500}),{1,1,1,1},{1,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,1.f,false);
        const std::filesystem::path captures=AEVRYND_TEST_ARTIFACT_DIR;std::filesystem::create_directories(captures);
        auto sample=[&](bool enabled,float time,const std::filesystem::path& path){auto settings=draw.VolumetricClouds();settings.enabled=enabled;draw.SetVolumetricClouds(settings);draw.SetEnvironmentTime(time);std::vector<uint64_t> gpu;for(int frame=0;frame<20;++frame){renderer.Render(draw,{.08f,.12f,.18f,1});if(frame>=8&&renderer.GetStats().lastGpuFrameNanoseconds)gpu.push_back(renderer.GetStats().lastGpuFrameNanoseconds);}renderer.CaptureNextFrame(path);renderer.Render(draw,{.08f,.12f,.18f,1});renderer.WaitIdle();return std::pair{Median(std::move(gpu)),renderer.GetStats()};};
        const auto offPath=captures/L"clouds-disabled.png",firstPath=captures/L"clouds-time-0.png",repeatPath=captures/L"clouds-time-0-repeat.png",secondPath=captures/L"clouds-time-30.png";
        const auto [offGpu,off]=sample(false,0,offPath);const auto [onGpu,on]=sample(true,0,firstPath);const auto [repeatGpu,repeat]=sample(true,0,repeatPath);const auto [movedGpu,moved]=sample(true,30,secondPath);const uint64_t offHash=HashFile(offPath),onHash=HashFile(firstPath),repeatHash=HashFile(repeatPath),movedHash=HashFile(secondPath);
        DrawList pure2D{DrawListCapacityProfile::Minimal2D};pure2D.Triangle({10,10},{30,10},{20,30},{1,1,1,1});renderer.Render(pure2D,{0,0,0,1});renderer.WaitIdle();const auto pureStats=renderer.GetStats();
        std::cout<<"VolumetricCloudD3D12 gpuOffUs="<<offGpu/1000<<" gpuOnUs="<<onGpu/1000<<" gpuRepeatUs="<<repeatGpu/1000<<" gpuMovedUs="<<movedGpu/1000<<" residentBytes="<<on.volumetricCloudResidentBytes<<" samples="<<on.volumetricCloudSampleCount<<" offHash="<<offHash<<" onHash="<<onHash<<" repeatHash="<<repeatHash<<" movedHash="<<movedHash<<" captures="<<captures.string()<<'\n';
        const uint64_t cloudGpuDelta=onGpu>offGpu?onGpu-offGpu:0;
        if(!offGpu||!onGpu||!repeatGpu||!movedGpu||cloudGpuDelta>1000000ull||off.volumetricCloudsEnabled||!on.volumetricCloudsEnabled||on.volumetricCloudSampleCount!=16||on.volumetricCloudResidentBytes||offHash==onHash||onHash!=repeatHash||onHash==movedHash||moved.successfulFrameCaptures<4||pureStats.volumetricCloudsEnabled||pureStats.volumetricCloudSampleCount||pureStats.volumetricCloudResidentBytes)result=3;
        renderer.DestroyMesh(skyMesh);renderer.WaitIdle();if(renderer.GetStats().residentMeshBytes)result=4;
    }DestroyWindow(window);return result;
}
