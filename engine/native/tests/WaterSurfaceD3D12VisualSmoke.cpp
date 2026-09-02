#include <Aevrynd/Renderer.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
using namespace Aevrynd;
static uint64_t FileHash(const std::filesystem::path& path){std::ifstream stream(path,std::ios::binary);uint64_t hash=14695981039346656037ull;for(char value{};stream.get(value);){hash^=static_cast<unsigned char>(value);hash*=1099511628211ull;}return hash;}
int main(){
 const wchar_t* className=L"AevryndWaterSurfaceVisual";WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=className;RegisterClassW(&wc);HWND window=CreateWindowW(className,L"Water surface visual",WS_OVERLAPPED,0,0,640,360,nullptr,nullptr,wc.hInstance,nullptr);if(!window)return 1;
 D3D12Renderer renderer;renderer.Initialize(window,640,360,640,360,false);renderer.SetQualitySettings({false,false,false,false});
 const std::array<Vertex3D,4> bottom{{{-1,-1,.72f,PackNormalSnorm8({0,0,-1}),0xff305080u,0,1},{-1,1,.72f,PackNormalSnorm8({0,0,-1}),0xff80a050u,0,0},{1,1,.72f,PackNormalSnorm8({0,0,-1}),0xffd08040u,1,0},{1,-1,.72f,PackNormalSnorm8({0,0,-1}),0xff4040d0u,1,1}}};
 const std::array<Vertex3D,4> surface{{{-1,-.86f,.48f,PackNormalSnorm8({0,1,0}),0x99a06020u,0,1},{-1,.86f,.48f,PackNormalSnorm8({0,1,0}),0x99a06020u,0,0},{1,.86f,.48f,PackNormalSnorm8({0,1,0}),0x99a06020u,1,0},{1,-.86f,.48f,PackNormalSnorm8({0,1,0}),0x99a06020u,1,1}}};
 const std::array<uint32_t,6> indices{0,1,2,0,2,3};const auto bottomMesh=renderer.CreateMesh(bottom,indices),waterMesh=renderer.CreateMesh(surface,indices);if(!bottomMesh||!waterMesh)return 2;
 DrawList draw;draw.SetViewProjection(Matrix4::Identity());draw.SetBackfaceCulling(false);draw.SetDirectionalLight({{.3f,-1,.2f},.2f,{1,.96f,.9f,1},1.8f});draw.SetScreenSpaceReflections({true,.82f,24.f,.7f,.32f,12});draw.SetWaterOptics({true,.04f,.65f,.05f});draw.SetWaterSurface({});draw.MeshInstance(bottomMesh,Matrix4::Identity(),{1,1,1,1},{.5f,0,0,1});draw.MeshInstance(waterMesh,Matrix4::Identity(),{.03f,.22f,.34f,.72f},{.08f,0,0,1},MeshRenderLayer3D::Transparent,{},.25f,false,MaterialClass3D::Water,.48f,false);
 const std::filesystem::path output="test-artifacts/water-surface";std::filesystem::create_directories(output);const auto capture=[&](float time,const char* name){draw.SetEnvironmentTime(time);renderer.CaptureNextFrame(output/name);renderer.Render(draw,{.008f,.018f,.03f,1});renderer.WaitIdle();return FileHash(output/name);};
 const uint64_t first=capture(0.f,"water-t0-a.png"),second=capture(0.f,"water-t0-b.png"),moved=capture(1.25f,"water-t1.png");
 auto legacyA=draw.WaterSurface();legacyA.enabled=false;legacyA.absorption={0,0,0};legacyA.foamStartDepth=0;legacyA.foamEndDepth=8;draw.SetWaterSurface(legacyA);const uint64_t legacyHashA=capture(.75f,"water-legacy-a.png");
 auto legacyB=legacyA;legacyB.absorption={4,4,4};legacyB.deepColor={4,0,4};legacyB.foamStartDepth=4;legacyB.foamEndDepth=4.01f;legacyB.maximumNightRadiance=.05f;draw.SetWaterSurface(legacyB);const uint64_t legacyHashB=capture(.75f,"water-legacy-b.png");
 const auto stats=renderer.GetStats();if(!first||first!=second||first==moved||!legacyHashA||legacyHashA!=legacyHashB){std::cerr<<"Water visual determinism/legacy bit-exact contract failed\n";return 3;}if(!stats.depthAwareWaterPasses||!stats.screenSpaceWaterRefractionPasses||stats.pendingGpuReleaseBytes){std::cerr<<"Water render/resource contract failed\n";return 4;}
 renderer.DestroyMesh(bottomMesh);renderer.DestroyMesh(waterMesh);renderer.WaitIdle();renderer.TrimTransientMemory();const auto trimmed=renderer.GetStats();if(trimmed.waterOpaqueSceneColorBytes||trimmed.pendingGpuReleaseBytes){std::cerr<<"Water trim did not return to zero\n";return 5;}DestroyWindow(window);std::cout<<"water_t0="<<first<<" water_t1="<<moved<<" gpu_ns="<<stats.lastGpuFrameNanoseconds<<'\n';
}
