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
        const Mesh3D sky=CreateSkyDomeMesh3D(32,16),box=Mesh3D::CreateUnitBox();const MeshHandle3D skyMesh=renderer.CreateMesh(sky.vertices,sky.indices),boxMesh=renderer.CreateMesh(box.vertices,box.indices);if(!skyMesh||!boxMesh){DestroyWindow(window);return 2;}
        Camera3D camera{{0,80,0},{0,170,240},{0,1,0},{}};camera.projection.farPlane=12000.f;
        const auto makeDraw=[&](const Camera3D& view,bool enclosure){DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetViewProjection(CreateViewProjectionMatrix(view,16.f/9.f));draw.SetBackfaceCulling(false);draw.SetAtmosphere({{.3f,.45f,.62f,1},view.position,10000,20000,true});draw.SetDirectionalLight({{-.42f,-1.f,.28f},.2f,{1.f,.9f,.72f,1},2.f});if(enclosure)/* 近距離全畫面遮擋板避開球面／盒體角點接縫，逐像素驗證雲層深度排除。 */draw.MeshInstance(boxMesh,CreateTransformMatrix({view.position.x,view.position.y+20.f,view.position.z+50.f},{},{1000,1000,2}),{1,1,1,1},{1,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,1.f,false);else draw.MeshInstance(boxMesh,CreateTransformMatrix({0,-4,0},{},{400,2,400}),{.18f,.22f,.16f,1},{1,0,0,1},MeshRenderLayer3D::Opaque,{},.8f,false,MaterialClass3D::Surface,1.f,false);return draw;};
        DrawList clearDraw=makeDraw(camera,false),occludedDraw=makeDraw(camera,true);Camera3D movedCamera=camera;movedCamera.position.x=140;movedCamera.target.x=140;DrawList movedDraw=makeDraw(movedCamera,false);
        const std::filesystem::path captures=AEVRYND_TEST_ARTIFACT_DIR;std::filesystem::create_directories(captures);
        auto sample=[&](DrawList& draw,bool enabled,float density,float time,const std::filesystem::path& path){auto settings=draw.VolumetricClouds();settings.enabled=enabled;settings.density=density;settings.sampleCount=16;draw.SetVolumetricClouds(settings);draw.SetEnvironmentTime(time);draw.SetCameraCut3D(true);std::vector<uint64_t> gpu;for(int frame=0;frame<23;++frame){renderer.Render(draw,{.08f,.12f,.18f,1});draw.SetCameraCut3D(false);if(frame>=11&&renderer.GetStats().lastGpuFrameNanoseconds)gpu.push_back(renderer.GetStats().lastGpuFrameNanoseconds);}if(!renderer.CaptureNextFrame(path))return std::pair<uint64_t,RendererStats>{};renderer.Render(draw,{.08f,.12f,.18f,1});renderer.WaitIdle();return std::pair{Median(std::move(gpu)),renderer.GetStats()};};
        const auto offPath=captures/L"clouds-disabled.png",densityZeroPath=captures/L"clouds-density-zero.png",firstPath=captures/L"clouds-time-0.png",repeatPath=captures/L"clouds-time-0-repeat.png",secondPath=captures/L"clouds-time-30.png",movedPath=captures/L"clouds-camera-moved.png",occludedOffPath=captures/L"clouds-occluded-off.png",occludedOnPath=captures/L"clouds-occluded-on.png";
        const auto [offGpu,off]=sample(clearDraw,false,1.15f,0,offPath);const auto [densityZeroGpu,densityZero]=sample(clearDraw,true,0,0,densityZeroPath);const auto [onGpu,on]=sample(clearDraw,true,1.15f,0,firstPath);const auto [repeatGpu,repeat]=sample(clearDraw,true,1.15f,0,repeatPath);const auto [timeGpu,timeMoved]=sample(clearDraw,true,1.15f,30,secondPath);const auto [movedGpu,moved]=sample(movedDraw,true,1.15f,0,movedPath);const auto [occludedOffGpu,occludedOff]=sample(occludedDraw,false,1.15f,0,occludedOffPath);const auto [occludedOnGpu,occludedOn]=sample(occludedDraw,true,1.15f,0,occludedOnPath);
        const uint64_t offHash=HashFile(offPath),densityZeroHash=HashFile(densityZeroPath),onHash=HashFile(firstPath),repeatHash=HashFile(repeatPath),timeHash=HashFile(secondPath),movedHash=HashFile(movedPath),occludedOffHash=HashFile(occludedOffPath),occludedOnHash=HashFile(occludedOnPath);
        DrawList pure2D{DrawListCapacityProfile::Minimal2D};pure2D.Triangle({10,10},{30,10},{20,30},{1,1,1,1});renderer.Render(pure2D,{0,0,0,1});renderer.WaitIdle();const auto pureStats=renderer.GetStats();
        std::cout<<"VolumetricCloudD3D12 gpuOffUs="<<offGpu/1000<<" gpuOnUs="<<onGpu/1000<<" gpuRepeatUs="<<repeatGpu/1000<<" gpuTimeUs="<<timeGpu/1000<<" gpuMovedUs="<<movedGpu/1000<<" physicalResidentBytes="<<on.volumetricAtmosphereResidentBytes<<" samples="<<on.volumetricAtmosphereAppliedCloudSamples<<" dispatches="<<on.volumetricAtmosphereDispatches<<" composites="<<on.volumetricAtmosphereCompositePasses<<" offHash="<<offHash<<" zeroHash="<<densityZeroHash<<" onHash="<<onHash<<" repeatHash="<<repeatHash<<" timeHash="<<timeHash<<" movedHash="<<movedHash<<" occluded="<<occludedOffHash<<'/'<<occludedOnHash<<" captures="<<captures.string()<<'\n';
        const uint64_t cloudGpuDelta=onGpu>offGpu?onGpu-offGpu:0;
        const bool invalidGbv=on.gpuValidationRequested&&(!on.gpuValidationAvailable||!on.gpuValidationExecuted||!on.gpuValidationPassed);
        if(!offGpu||!densityZeroGpu||!onGpu||!repeatGpu||!timeGpu||!movedGpu||!occludedOffGpu||!occludedOnGpu||cloudGpuDelta>1000000ull||off.volumetricCloudsEnabled||densityZero.volumetricCloudsEnabled||!on.volumetricCloudsEnabled||on.volumetricCloudSampleCount!=16||!on.volumetricAtmosphereResidentBytes||!on.volumetricAtmosphereDispatched||!on.volumetricAtmosphereDispatches||!on.volumetricAtmosphereCompositePasses||on.volumetricAtmosphereAppliedCloudSamples!=16||offHash!=densityZeroHash||offHash==onHash||onHash!=repeatHash||onHash==timeHash||onHash==movedHash||occludedOffHash!=occludedOnHash||moved.successfulFrameCaptures<6||on.debugErrorCount||on.debugWarningCount||invalidGbv||pureStats.volumetricCloudsEnabled||pureStats.volumetricCloudSampleCount||pureStats.volumetricCloudResidentBytes)result=3;
        renderer.DestroyMesh(skyMesh);renderer.DestroyMesh(boxMesh);renderer.WaitIdle();if(renderer.GetStats().residentMeshBytes)result=4;
    }DestroyWindow(window);return result;
}
