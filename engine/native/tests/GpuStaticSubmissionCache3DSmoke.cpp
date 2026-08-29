#include <Aevrynd/Camera.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/Renderer.h>
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <vector>
using namespace Aevrynd;

static bool IsWarmWithoutCopies(const RendererStats& stats){
    return stats.lastStaticMeshSubmissionWarmHit&&!stats.lastStaticMeshSubmissionCpuRebuild&&
        !stats.lastStaticMeshSubmissionInstanceCopiedBytes&&!stats.lastStaticMeshSubmissionGroupCopiedBytes&&
        !stats.lastStaticMeshSubmissionCommandCopiedBytes;
}
static bool IsRebuilt(const RendererStats& stats){return !stats.lastStaticMeshSubmissionWarmHit&&stats.lastStaticMeshSubmissionCpuRebuild;}

int main(){
    constexpr size_t GroupCount=128,InstancesPerGroup=8;
    HINSTANCE instance=GetModuleHandleW(nullptr);WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=instance;wc.lpszClassName=L"AevryndGpuStaticSubmissionCacheSmoke";RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"GPU static submission cache smoke",WS_OVERLAPPED,0,0,640,360,nullptr,nullptr,instance,nullptr);if(!window)return 1;
    D3D12Renderer renderer;renderer.Initialize(window,640,360,640,360,false);renderer.SetQualitySettings({false,false,false,false});renderer.SetGpuInstanceCullingSettings({true,100.f,GpuInstanceSubmissionMode3D::MultiGroup});
    const Mesh3D source=Mesh3D::CreateUnitBox();std::vector<MeshHandle3D> meshes;meshes.reserve(GroupCount);
    const std::filesystem::path root=std::filesystem::path{AEVRYND_SOURCE_DIR}/"games"/"character_lab"/"assets"/"material-packs"/"original"/"hair";
    std::vector<MaterialTextureSetHandle3D> materials;for(uint32_t resolution:{64u,128u,256u,512u}){MaterialTextureSetDefinition3D definition{root/"base-color.png",root/"roughness.png",resolution,root/"normal-dx.png"};const auto material=renderer.CreateMaterialTextureSet(definition);if(!material)return 2;materials.push_back(material);}
    DrawList draw;Camera3D camera{{0,0,-8},{0,0,0},{0,1,0},{}};draw.SetViewProjection(CreateViewProjectionMatrix(camera,640.f/360.f));
    // 中文註解：四種材質、雙面狀態與 local/global probe 共同形成穩定 bucket 維度。
    for(size_t group=0;group<GroupCount;++group){Mesh3D unique=source;unique.vertices[0].x+=float(group+1)*.00001f;const auto mesh=renderer.CreateMesh(unique.vertices,unique.indices);if(!mesh)return 3;meshes.push_back(mesh);for(size_t item=0;item<InstancesPerGroup;++item){const Vec3 position{float(int(group%16)-8)*.25f,float(int(group/16)-4)*.25f,float(item)*.02f};if(!draw.MeshInstance(mesh,CreateTranslationMatrix(position),{1,1,1,1},{},MeshRenderLayer3D::Opaque,materials[(group/16)%materials.size()],.25f,true,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false,1,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,Matrix4::Identity(),false,1.f,MaterialAlphaMode3D::Opaque,.5f,group>=GroupCount/2))return 4;}}
    const auto capture=renderer.CreateReflectionProbeCapture3D({{0,0,0},32,.1f,20.f});if(!capture)return 5;DrawList captureDraw;captureDraw.SetViewProjection(draw.ViewProjection());renderer.Render(captureDraw,{.01f,.01f,.01f,1});renderer.WaitIdle();const auto captureStatus=renderer.ReflectionProbeCaptureStatus(capture);if(!captureStatus||!captureStatus->environment)return 6;
    if(!renderer.SetEnvironmentLighting(captureStatus->environment)||!draw.AddLocalReflectionProbe({captureStatus->environment,{-2,-2,-2},{0,2,2},{-.5f,0,0},1}))return 7;
    draw.SetStaticMeshSubmission3D(draw.FreezeMeshInstances3D());
    RendererStats coldStats{};for(size_t frame=0;frame<D3D12Renderer::BufferedFrames+1;++frame){renderer.Render(draw,{0,0,0,1});if(frame==0)coldStats=renderer.GetStats();}renderer.WaitIdle();auto stats=renderer.GetStats();
    if(coldStats.lastPackedShadowCascadeMasks.size()!=GroupCount*InstancesPerGroup||stats.lastPackedShadowCascadeMasks!=coldStats.lastPackedShadowCascadeMasks||stats.lastPackedShadowCascadeMaskHash!=coldStats.lastPackedShadowCascadeMaskHash||!stats.lastPackedShadowCascadeMaskHash){std::cerr<<"Packed shadow mask cold/warm order mismatch\n";return 18;}
    if(!stats.lastStaticMeshSubmissionCacheEligible||stats.lastStaticMeshSubmissionPreparationIterations||!IsWarmWithoutCopies(stats)||stats.lastGpuInstanceCullingVisible!=GroupCount*InstancesPerGroup||stats.lastGpuInstanceSubmissionCommands!=GroupCount||stats.lastGpuInstanceSubmissionBuckets<8||stats.lastGpuInstanceSubmissionBuckets>16){std::cerr<<"warm="<<stats.lastStaticMeshSubmissionWarmHit<<" rebuild="<<stats.lastStaticMeshSubmissionCpuRebuild<<" iterations="<<stats.lastStaticMeshSubmissionPreparationIterations<<" copied="<<(stats.lastStaticMeshSubmissionInstanceCopiedBytes+stats.lastStaticMeshSubmissionGroupCopiedBytes+stats.lastStaticMeshSubmissionCommandCopiedBytes)<<" visible="<<stats.lastGpuInstanceCullingVisible<<" commands="<<stats.lastGpuInstanceSubmissionCommands<<" buckets="<<stats.lastGpuInstanceSubmissionBuckets<<'\n';return 8;}
    DrawList mixed=draw;const MeshInstance3D appended=draw.EffectiveMeshInstances3D().front();mixed.meshInstances.push_back(appended);if(mixed.EffectiveMeshInstances3D().size()!=1||mixed.StaticMeshSubmissionRevision3D()!=0||mixed.StaticMeshSubmissionCacheEligible3D())return 17;
    camera.position.x=.1f;draw.SetViewProjection(CreateViewProjectionMatrix(camera,640.f/360.f));renderer.Render(draw,{0,0,0,1});renderer.WaitIdle();if(!IsWarmWithoutCopies(renderer.GetStats()))return 9;
    renderer.SetGpuInstanceCullingSettings({true,90.f,GpuInstanceSubmissionMode3D::MultiGroup});renderer.Render(draw,{0,0,0,1});renderer.WaitIdle();if(!IsRebuilt(renderer.GetStats()))return 10;
    renderer.SetGpuInstanceCullingSettings({true,90.f,GpuInstanceSubmissionMode3D::Aggregate});renderer.Render(draw,{0,0,0,1});renderer.WaitIdle();if(!IsRebuilt(renderer.GetStats()))return 11;
    draw.RemoveLocalReflectionProbe(0);renderer.Render(draw,{0,0,0,1});renderer.WaitIdle();if(!IsRebuilt(renderer.GetStats()))return 12;
    renderer.SetEnvironmentLighting({});renderer.Render(draw,{0,0,0,1});renderer.WaitIdle();if(!IsRebuilt(renderer.GetStats()))return 13;
    renderer.DestroyMaterialTextureSet(materials.back());materials.pop_back();renderer.Render(draw,{0,0,0,1});renderer.WaitIdle();if(!IsRebuilt(renderer.GetStats()))return 14;
    renderer.TrimTransientMemory();const auto trimmedMasks=renderer.GetStats();if(!trimmedMasks.lastPackedShadowCascadeMasks.empty()||trimmedMasks.packedShadowCascadeMaskCapacityBytes)return 19;renderer.Render(draw,{0,0,0,1});renderer.WaitIdle();if(!IsRebuilt(renderer.GetStats())||renderer.GetStats().lastPackedShadowCascadeMasks!=coldStats.lastPackedShadowCascadeMasks||renderer.GetStats().lastPackedShadowCascadeMaskHash!=coldStats.lastPackedShadowCascadeMaskHash)return 15;
    renderer.DestroyMesh(meshes.back());meshes.pop_back();renderer.Render(draw,{0,0,0,1});renderer.WaitIdle();if(!IsRebuilt(renderer.GetStats()))return 16;
    for(auto material:materials)renderer.DestroyMaterialTextureSet(material);for(auto mesh:meshes)renderer.DestroyMesh(mesh);renderer.DestroyReflectionProbeCapture3D(capture);renderer.WaitIdle();DestroyWindow(window);
    std::cout<<"groups="<<GroupCount<<" instances="<<GroupCount*InstancesPerGroup<<" buckets="<<stats.lastGpuInstanceSubmissionBuckets<<" commands="<<stats.lastGpuInstanceSubmissionCommands<<" copied_bytes="<<(stats.lastStaticMeshSubmissionInstanceCopiedBytes+stats.lastStaticMeshSubmissionGroupCopiedBytes+stats.lastStaticMeshSubmissionCommandCopiedBytes)<<'\n';return 0;
}
