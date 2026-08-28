#include <Aevrynd/Camera.h>
#include <Aevrynd/EnvironmentMeshes3D.h>
#include <Aevrynd/Mesh3D.h>
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
    HWND window=CreateWindowExW(0,L"STATIC",L"AevryndCloudShadowVisual",WS_OVERLAPPED,0,0,1920,1080,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)return 1;
    int result{};{
        D3D12Renderer renderer;renderer.Initialize(window,1920,1080,1920,1080,false);renderer.SetQualitySettings({false,false,false,false});
        const Mesh3D box=Mesh3D::CreateUnitBox(),grass=CreateGrassClumpMesh3D(7,2);const MeshHandle3D boxMesh=renderer.CreateMesh(box.vertices,box.indices),grassMesh=renderer.CreateMesh(grass.vertices,grass.indices);if(!boxMesh||!grassMesh){DestroyWindow(window);return 2;}
        Camera3D camera{{0,1700,-2600},{0,0,700},{0,1,0},{}};camera.projection.farPlane=12000.f;
        DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetViewProjection(CreateViewProjectionMatrix(camera,16.f/9.f));draw.SetBackfaceCulling(false);draw.SetDirectionalShadowMapEnabled(false);draw.SetAtmosphere({{.3f,.45f,.62f,1},camera.position,20000,30000,true});draw.SetDirectionalLight({{-.42f,-1.f,.28f},.18f,{1.f,.92f,.74f,1},2.6f});
        draw.MeshInstance(boxMesh,CreateTransformMatrix({0,-30,700},{},{6000,40,6000}),{.38f,.52f,.24f,1},{.78f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Terrain,1.f,false);
        for(int index=0;index<9;++index){const float x=-1800.f+float(index%3)*1800.f,z=-200.f+float(index/3)*1500.f;draw.MeshInstance(boxMesh,CreateTransformMatrix({x,180,z},{},{420,360+float(index)*24.f,420}),{.58f,.46f,.31f,1},{.62f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,1.f,false);draw.MeshInstance(grassMesh,CreateTransformMatrix({x+520,65,z+260},{},{180,130,180}),{.28f,.62f,.22f,1},{.82f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Foliage,1.f,false);}
        auto clouds=draw.VolumetricClouds();clouds.coverage=.38f;clouds.shadowStrength=.82f;draw.SetVolumetricClouds(clouds);
        const std::filesystem::path captures=AEVRYND_TEST_ARTIFACT_DIR;std::filesystem::create_directories(captures);
        auto sample=[&](bool shadows,float time,const std::filesystem::path& path){auto settings=draw.VolumetricClouds();settings.shadowsEnabled=shadows;draw.SetVolumetricClouds(settings);draw.SetEnvironmentTime(time);std::vector<uint64_t> gpu;for(int frame=0;frame<24;++frame){renderer.Render(draw,{.11f,.16f,.22f,1});if(frame>=10&&renderer.GetStats().lastGpuFrameNanoseconds)gpu.push_back(renderer.GetStats().lastGpuFrameNanoseconds);}renderer.CaptureNextFrame(path);renderer.Render(draw,{.11f,.16f,.22f,1});renderer.WaitIdle();return std::pair{Median(std::move(gpu)),renderer.GetStats()};};
        const auto offPath=captures/L"cloud-shadow-disabled.png",onPath=captures/L"cloud-shadow-time-0.png",repeatPath=captures/L"cloud-shadow-time-0-repeat.png",movedPath=captures/L"cloud-shadow-time-60.png";
        const auto [offGpu,off]=sample(false,0,offPath);const auto [onGpu,on]=sample(true,0,onPath);const auto [repeatGpu,repeat]=sample(true,0,repeatPath);const auto [movedGpu,moved]=sample(true,60,movedPath);const uint64_t offHash=HashFile(offPath),onHash=HashFile(onPath),repeatHash=HashFile(repeatPath),movedHash=HashFile(movedPath),gpuDelta=onGpu>offGpu?onGpu-offGpu:0;
        DrawList pure2D{DrawListCapacityProfile::Minimal2D};pure2D.Triangle({10,10},{30,10},{20,30},{1,1,1,1});renderer.Render(pure2D,{0,0,0,1});renderer.WaitIdle();const auto pure=renderer.GetStats();
        std::cout<<"CloudShadowD3D12 gpuOffUs="<<offGpu/1000<<" gpuOnUs="<<onGpu/1000<<" gpuDeltaUs="<<gpuDelta/1000<<" gpuRepeatUs="<<repeatGpu/1000<<" gpuMovedUs="<<movedGpu/1000<<" residentBytes="<<on.volumetricCloudShadowResidentBytes<<" offHash="<<offHash<<" onHash="<<onHash<<" repeatHash="<<repeatHash<<" movedHash="<<movedHash<<" captures="<<captures.string()<<'\n';
        if(!offGpu||!onGpu||!repeatGpu||!movedGpu||gpuDelta>1500000ull||off.volumetricCloudShadowsEnabled||!on.volumetricCloudShadowsEnabled||on.volumetricCloudShadowResidentBytes||offHash==onHash||onHash!=repeatHash||onHash==movedHash||moved.successfulFrameCaptures<4||pure.volumetricCloudShadowsEnabled||pure.volumetricCloudShadowResidentBytes)result=3;
        renderer.DestroyMesh(grassMesh);renderer.DestroyMesh(boxMesh);renderer.WaitIdle();if(renderer.GetStats().residentMeshBytes)result=4;
    }DestroyWindow(window);return result;
}
