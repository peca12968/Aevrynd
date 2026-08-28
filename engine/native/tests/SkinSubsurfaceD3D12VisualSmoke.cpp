#include <Aevrynd/Camera.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/Renderer.h>
#include <Windows.h>
#include <algorithm>
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
    HWND window=CreateWindowExW(0,L"STATIC",L"AevryndSkinSubsurfaceVisual",WS_OVERLAPPED,0,0,1920,1080,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)return 1;
    int result{};{
        D3D12Renderer renderer;renderer.Initialize(window,1920,1080,1920,1080,false);renderer.SetQualitySettings({false,false,false,false});
        const Mesh3D sphereSource=Mesh3D::CreateUnitSphere(48,32),box=Mesh3D::CreateUnitBox();std::vector<SkinnedVertex3D> sphereVertices;sphereVertices.reserve(sphereSource.vertices.size());for(const Vertex3D& source:sphereSource.vertices){SkinnedVertex3D vertex{};vertex.x=source.x;vertex.y=source.y;vertex.z=source.z;vertex.normal=source.normal;vertex.color=source.color;vertex.u=source.u;vertex.v=source.v;vertex.weights[0]=65535;sphereVertices.push_back(vertex);}const SkeletonCompatibilityId3D compatibility{0x5f8d19c32a17e4b1ull};const SkinnedMeshHandle3D sphere=renderer.CreateSkinnedMesh(sphereVertices,sphereSource.indices,1,compatibility);const MeshHandle3D ground=renderer.CreateMesh(box.vertices,box.indices);if(!sphere||!ground){DestroyWindow(window);return 2;}
        Camera3D camera{{0,1.25f,-4.2f},{0,.8f,0},{0,1,0},{}};camera.projection.farPlane=80.f;
        SkinPaletteJoint3D joint{};joint.skin=Matrix4::Identity();joint.normalRows={Vec4{1,0,0,0},Vec4{0,1,0,0},Vec4{0,0,1,0}};
        auto makeDraw=[&](CharacterShadingModel3D model){DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetViewProjection(CreateViewProjectionMatrix(camera,16.f/9.f));draw.SetBackfaceCulling(true);draw.SetAtmosphere({{.12f,.16f,.2f,1},camera.position,100.f,200.f,false});draw.SetDirectionalLight({{-.82f,-.34f,.18f},.12f,{1.f,.74f,.62f,1},3.4f});draw.MeshInstance(ground,CreateTransformMatrix({0,-.58f,0},{},{8,.12f,8}),{.14f,.16f,.18f,1},{.85f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,1.f,true);const auto palette=draw.AddSkinPalette(std::span<const SkinPaletteJoint3D>{&joint,1},compatibility);draw.SkinnedMeshInstance(sphere,palette,CreateTransformMatrix({0,.45f,0},{},{2.25f,2.7f,2.05f}),{.88f,.48f,.38f,1},{.42f,0,0,1},{},.25f,false,MaterialClass3D::Surface,true,DirectionalShadowAllCascades3D,0,uint32_t(sphereSource.indices.size()),model);return draw;};
        DrawList skin=makeDraw(CharacterShadingModel3D::Skin),ordinary=makeDraw(CharacterShadingModel3D::Default);const std::filesystem::path captures=AEVRYND_TEST_ARTIFACT_DIR;std::filesystem::create_directories(captures);
        auto sample=[&](DrawList& draw,bool enabled,const std::filesystem::path& path){auto settings=draw.SkinSubsurface();settings.enabled=enabled;draw.SetSkinSubsurface(settings);std::vector<uint64_t> gpu;for(int frame=0;frame<28;++frame){renderer.Render(draw,{.035f,.045f,.06f,1});if(frame>=12&&renderer.GetStats().lastGpuFrameNanoseconds)gpu.push_back(renderer.GetStats().lastGpuFrameNanoseconds);}renderer.CaptureNextFrame(path);renderer.Render(draw,{.035f,.045f,.06f,1});renderer.WaitIdle();return std::pair{Median(std::move(gpu)),renderer.GetStats()};};
        const auto offPath=captures/L"skin-subsurface-disabled.png",onPath=captures/L"skin-subsurface-enabled.png",repeatPath=captures/L"skin-subsurface-enabled-repeat.png",ordinaryOffPath=captures/L"ordinary-disabled.png",ordinaryOnPath=captures/L"ordinary-enabled.png",taaPath=captures/L"skin-subsurface-taa.png";
        const auto [offGpu,off]=sample(skin,false,offPath);const auto [onGpu,on]=sample(skin,true,onPath);const auto [repeatGpu,repeat]=sample(skin,true,repeatPath);const auto [ordinaryOffGpu,ordinaryOff]=sample(ordinary,false,ordinaryOffPath);const auto [ordinaryOnGpu,ordinaryOn]=sample(ordinary,true,ordinaryOnPath);const uint64_t offHash=HashFile(offPath),onHash=HashFile(onPath),repeatHash=HashFile(repeatPath),ordinaryOffHash=HashFile(ordinaryOffPath),ordinaryOnHash=HashFile(ordinaryOnPath),gpuDelta=onGpu>offGpu?onGpu-offGpu:0;
        renderer.SetQualitySettings({true,true,false,true});if(!renderer.RequestVelocityReadback3D())result=3;for(int frame=0;frame<12;++frame)renderer.Render(skin,{.035f,.045f,.06f,1});const auto velocity=renderer.TakeVelocityReadback3D();renderer.CaptureNextFrame(taaPath);renderer.Render(skin,{.035f,.045f,.06f,1});renderer.WaitIdle();const auto integrated=renderer.GetStats();
        DrawList pure2D{DrawListCapacityProfile::Minimal2D};pure2D.Triangle({10,10},{30,10},{20,30},{1,1,1,1});renderer.Render(pure2D,{0,0,0,1});renderer.WaitIdle();const auto pure=renderer.GetStats();
        std::cout<<"SkinSubsurfaceD3D12 gpuOffUs="<<offGpu/1000<<" gpuOnUs="<<onGpu/1000<<" gpuDeltaUs="<<gpuDelta/1000<<" gpuRepeatUs="<<repeatGpu/1000<<" ordinaryOffUs="<<ordinaryOffGpu/1000<<" ordinaryOnUs="<<ordinaryOnGpu/1000<<" residentBytes="<<on.skinSubsurfaceResidentBytes<<" offHash="<<offHash<<" onHash="<<onHash<<" repeatHash="<<repeatHash<<" ordinaryOffHash="<<ordinaryOffHash<<" ordinaryOnHash="<<ordinaryOnHash<<" maxStaticVelocity="<<(velocity?MaximumVelocity(*velocity):-1.f)<<" captures="<<captures.string()<<'\n';
        if(!offGpu||!onGpu||!repeatGpu||!ordinaryOffGpu||!ordinaryOnGpu||gpuDelta>1000000ull||off.skinSubsurfaceEnabled||!on.skinSubsurfaceEnabled||on.skinSubsurfaceResidentBytes||offHash==onHash||onHash!=repeatHash||ordinaryOffHash!=ordinaryOnHash||!velocity||MaximumVelocity(*velocity)>.02f||!integrated.skinSubsurfaceEnabled||!integrated.ambientOcclusionEnabled||!integrated.temporalAntiAliasingEnabled||!integrated.directionalShadowMapBytes||integrated.successfulFrameCaptures<6||pure.skinSubsurfaceEnabled||pure.skinSubsurfaceResidentBytes)result=4;
        renderer.DestroySkinnedMesh(sphere);renderer.DestroyMesh(ground);renderer.WaitIdle();const auto released=renderer.GetStats();if(released.residentSkinnedMeshBytes||released.residentMeshBytes)result=5;
    }DestroyWindow(window);return result;
}
