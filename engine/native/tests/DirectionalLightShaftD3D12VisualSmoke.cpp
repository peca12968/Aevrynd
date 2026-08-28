#include <Aevrynd/Camera.h>
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
    HWND window=CreateWindowExW(0,L"STATIC",L"AevryndDirectionalLightShaftVisual",WS_OVERLAPPED,0,0,1920,1080,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)return 1;
    int result{};{
        D3D12Renderer renderer;renderer.Initialize(window,1920,1080,1920,1080,false);renderer.SetQualitySettings({true,false,false,false});
        const Mesh3D box=Mesh3D::CreateUnitBox();const MeshHandle3D boxMesh=renderer.CreateMesh(box.vertices,box.indices);if(!boxMesh){DestroyWindow(window);return 2;}
        Camera3D camera{{0,78,-250},{0,58,180},{0,1,0},{}};camera.projection.farPlane=1800.f;
        DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetBackfaceCulling(false);draw.SetViewProjection(CreateViewProjectionMatrix(camera,16.f/9.f));draw.SetDirectionalLight({{.58f,-1.f,.18f},.14f,{1.f,.9f,.7f,1.f},3.2f});
        Atmosphere3D atmosphere{{.22f,.29f,.34f,.78f},camera.position,1200.f,1800.f,true,.0085f,.012f,25.f,.86f};draw.SetAtmosphere(atmosphere);
        auto clouds=draw.VolumetricClouds();clouds.baseAltitude=210.f;clouds.thickness=360.f;clouds.coverage=.12f;clouds.density=2.8f;clouds.erosion=.1f;clouds.shadowStrength=1.f;draw.SetVolumetricClouds(clouds);
        auto shafts=draw.DirectionalLightShafts();shafts.intensity=.82f;shafts.maximumDistance=520.f;draw.SetDirectionalLightShafts(shafts);
        draw.MeshInstance(boxMesh,CreateTransformMatrix({0,-12,260},{},{760,20,1100}),{.24f,.34f,.19f,1},{.9f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Terrain,1.f,false);
        for(int index=0;index<8;++index){const float x=-190.f+float(index%4)*125.f,z=15.f+float(index/4)*170.f,height=125.f+float(index%3)*34.f;draw.MeshInstance(boxMesh,CreateTransformMatrix({x,height*.5f-2.f,z},{},{34.f,height,34.f}),{.38f,.3f,.22f,1},{.72f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,1.f,false);}
        const std::filesystem::path captures=AEVRYND_TEST_ARTIFACT_DIR;std::filesystem::create_directories(captures);
        auto sample=[&](bool enabled,float intensity,float time,const std::filesystem::path& path){auto settings=draw.DirectionalLightShafts();settings.enabled=enabled;settings.intensity=intensity;draw.SetDirectionalLightShafts(settings);draw.SetEnvironmentTime(time);std::vector<uint64_t> gpu;for(int frame=0;frame<28;++frame){renderer.Render(draw,{.055f,.075f,.095f,1});if(frame>=12&&renderer.GetStats().lastGpuFrameNanoseconds)gpu.push_back(renderer.GetStats().lastGpuFrameNanoseconds);}renderer.CaptureNextFrame(path);renderer.Render(draw,{.055f,.075f,.095f,1});renderer.WaitIdle();return std::pair{Median(std::move(gpu)),renderer.GetStats()};};
        const auto offPath=captures/L"directional-light-shafts-disabled.png",zeroPath=captures/L"directional-light-shafts-zero-intensity.png",onPath=captures/L"directional-light-shafts-time-0.png",repeatPath=captures/L"directional-light-shafts-time-0-repeat.png",timePath=captures/L"directional-light-shafts-time-45.png",movedPath=captures/L"directional-light-shafts-camera-moved.png";
        const auto [offGpu,off]=sample(false,.82f,0.f,offPath);const auto [zeroGpu,zero]=sample(true,0.f,0.f,zeroPath);const auto [onGpu,on]=sample(true,.82f,0.f,onPath);const auto [repeatGpu,repeat]=sample(true,.82f,0.f,repeatPath);const auto [timeGpu,timeMoved]=sample(true,.82f,45.f,timePath);
        camera.position.x+=28.f;camera.target.x+=16.f;atmosphere.cameraPosition=camera.position;draw.SetAtmosphere(atmosphere);draw.SetViewProjection(CreateViewProjectionMatrix(camera,16.f/9.f));const auto [movedGpu,moved]=sample(true,.82f,45.f,movedPath);
        const uint64_t offHash=HashFile(offPath),zeroHash=HashFile(zeroPath),onHash=HashFile(onPath),repeatHash=HashFile(repeatPath),timeHash=HashFile(timePath),movedHash=HashFile(movedPath);const uint64_t gpuDelta=onGpu>offGpu?onGpu-offGpu:0;
        DrawList pure2D{DrawListCapacityProfile::Minimal2D};pure2D.Triangle({10,10},{30,10},{20,30},{1,1,1,1});renderer.Render(pure2D,{0,0,0,1});renderer.WaitIdle();const auto pure=renderer.GetStats();
        std::cout<<"DirectionalLightShaftD3D12 gpuOffUs="<<offGpu/1000<<" gpuZeroUs="<<zeroGpu/1000<<" gpuOnUs="<<onGpu/1000<<" gpuDeltaUs="<<gpuDelta/1000<<" gpuRepeatUs="<<repeatGpu/1000<<" gpuTimeUs="<<timeGpu/1000<<" gpuMovedUs="<<movedGpu/1000<<" samples="<<on.directionalLightShaftSampleCount<<" residentBytes="<<on.directionalLightShaftResidentBytes<<" offHash="<<offHash<<" zeroHash="<<zeroHash<<" onHash="<<onHash<<" repeatHash="<<repeatHash<<" timeHash="<<timeHash<<" movedHash="<<movedHash<<" captures="<<captures.string()<<'\n';
        if(!offGpu||!zeroGpu||!onGpu||!repeatGpu||!timeGpu||!movedGpu||gpuDelta>1500000ull||off.directionalLightShaftsEnabled||zero.directionalLightShaftsEnabled||!on.directionalLightShaftsEnabled||on.directionalLightShaftSampleCount!=4||on.directionalLightShaftResidentBytes||offHash!=zeroHash||offHash==onHash||onHash!=repeatHash||onHash==timeHash||timeHash==movedHash||moved.successfulFrameCaptures<6||pure.directionalLightShaftsEnabled||pure.directionalLightShaftSampleCount||pure.directionalLightShaftResidentBytes)result=3;
        renderer.DestroyMesh(boxMesh);renderer.WaitIdle();if(renderer.GetStats().residentMeshBytes)result=4;
    }DestroyWindow(window);return result;
}
