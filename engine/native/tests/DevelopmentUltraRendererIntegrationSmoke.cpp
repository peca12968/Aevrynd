#include <Aevrynd/Camera.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/Renderer.h>
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace Aevrynd;

namespace {
std::uint64_t Percentile(std::vector<std::uint64_t> samples,double percentile){
 if(samples.empty())return 0;
 std::ranges::sort(samples);
 const auto index=(std::min)(samples.size()-1,std::size_t(std::ceil(percentile*double(samples.size()))-1));
 return samples[index];
}
}

int main(){
 const HINSTANCE instance=GetModuleHandleW(nullptr);
 const wchar_t* className=L"AevryndDevelopmentUltraRendererIntegration";
 WNDCLASSW windowClass{};windowClass.lpfnWndProc=DefWindowProcW;windowClass.hInstance=instance;windowClass.lpszClassName=className;
 RegisterClassW(&windowClass);
 const HWND window=CreateWindowW(className,L"Development Ultra renderer integration",WS_OVERLAPPED,0,0,1920,1080,nullptr,nullptr,instance,nullptr);
 if(!window)return 1;
 int result{};
 {
  D3D12Renderer renderer;
  renderer.Initialize(window,1920,1080,1920,1080,false);
  renderer.SetQualitySettings({false,false,true,false});
  const Mesh3D box=Mesh3D::CreateUnitBox();
  const MeshHandle3D mesh=renderer.CreateMesh(box.vertices,box.indices);
  if(!mesh){DestroyWindow(window);return 2;}
  DrawList draw{DrawListCapacityProfile::Minimal2D};
  const Camera3D camera{{0,90,-240},{0,80,420},{0,1,0},{}};
  draw.SetViewProjection(CreateViewProjectionMatrix(camera,16.f/9.f));
  draw.SetBackfaceCulling(false);
  draw.SetAtmosphere({{.025f,.04f,.075f,1},camera.position,180.f,420.f,true,.008f,0,.02f,.85f});
  draw.SetDirectionalLight({{-.35f,-.82f,.44f},.12f,{1,.9f,.72f,1},2.2f});
  draw.SetDirectionalShadowCascades({4,2048,.1f,220.f,.70f,.10f});
  VolumetricCloudSettings3D clouds{true,180,420,.05f,4.f,0.f,28,28,true,.7f};
  clouds.maximumDistance=520.f;
  draw.SetVolumetricClouds(clouds);
  draw.SetDirectionalLightShafts({true,2.f,.72f,520,10});
  draw.MeshInstance(mesh,CreateTransformMatrix({0,-18,650},{},{1500,20,1800}),{.2f,.31f,.14f,1},{.86f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Terrain,1,true);
  draw.MeshInstance(mesh,CreateTransformMatrix({0,85,430},{},{90,105,90}),{.4f,.32f,.22f,1},{.62f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,1,true);

  if(!renderer.ApplyDevelopmentUltraResourceProfile(2048,4,.5f))result=3;
  for(unsigned frame=0;frame<4&&!result;++frame){draw.SetCameraCut3D(frame==0);renderer.Render(draw,{.015f,.025f,.05f,1});renderer.WaitIdle();}
  const DevelopmentUltraRendererBaseline3D baseline=renderer.CaptureDevelopmentUltraResourceBaseline();
  const RendererStats baselineStats=renderer.GetStats();
  if(!baseline.shadowResident||!baseline.volumetricAtmosphereResident||baseline.shadowResolution!=2048||baseline.shadowCascadeCapacity!=4||baseline.shadowResidentBytes!=std::size_t(2048)*2048*sizeof(float)*4||baseline.volumetricAtmosphereWidth!=960||baseline.volumetricAtmosphereHeight!=540||std::abs(baseline.volumetricAtmosphereResolutionScale-.5f)>.0001f||!baseline.volumetricAtmosphereResidentBytes||baselineStats.pendingGpuReleaseBytes)result=3;
  draw.SetDirectionalShadowCascades({4,4096,.1f,220.f,.70f,.10f});
  if(!renderer.ApplyDevelopmentUltraResourceProfile(4096,4,.75f))result=3;
  std::vector<std::uint64_t> frameTimes;
  for(unsigned frame=0;frame<32&&!result;++frame){
   draw.SetEnvironmentTime(float(frame)*.016f);
   draw.SetCameraCut3D(frame==0);
   renderer.Render(draw,{.015f,.025f,.05f,1});
   renderer.WaitIdle();
   if(frame>=8){const auto timing=renderer.GetStats().lastGpuFrameNanoseconds;if(timing)frameTimes.push_back(timing);}
  }
  const RendererStats ultra=renderer.GetStats();
  const std::size_t expectedShadowBytes=std::size_t(4096)*4096*sizeof(float)*4;
  const bool gpuValidationValid=!ultra.gpuValidationRequested||(ultra.gpuValidationAvailable&&ultra.gpuValidationExecuted&&ultra.gpuValidationPassed);
  const bool resourceTruth=ultra.directionalShadowResolution==4096&&ultra.directionalShadowCascadeCapacity==4&&ultra.directionalShadowMapBytes==expectedShadowBytes&&
   std::abs(ultra.volumetricAtmosphereResolutionScale-.75f)<.0001f&&ultra.volumetricAtmosphereWidth==1440&&ultra.volumetricAtmosphereHeight==810&&ultra.volumetricAtmosphereDispatched&&
   ultra.volumetricAtmosphereAppliedCloudSamples==28&&ultra.volumetricAtmosphereAppliedShadowSamples==7&&ultra.volumetricAtmosphereAppliedShaftSamples==10&&
   !ultra.pendingGpuReleaseBytes&&renderer.MemoryStats().trackedCapacityBytes<=std::size_t(10)*1024*1024*1024ull;
  const auto p95=Percentile(frameTimes,.95),p99=Percentile(frameTimes,.99),maximum=frameTimes.empty()?0:*std::ranges::max_element(frameTimes);
  const bool timingTruth=frameTimes.size()>=20&&p95<=15'000'000ull&&p99<=15'833'000ull&&maximum<=16'667'000ull;
  std::cout<<"ultra shadow="<<ultra.directionalShadowResolution<<'x'<<ultra.directionalShadowCascadeCapacity<<" shadowBytes="<<ultra.directionalShadowMapBytes
   <<" volumetric="<<ultra.volumetricAtmosphereWidth<<'x'<<ultra.volumetricAtmosphereHeight<<" scale="<<ultra.volumetricAtmosphereResolutionScale
   <<" samples="<<ultra.volumetricAtmosphereAppliedCloudSamples<<'/'<<ultra.volumetricAtmosphereAppliedShadowSamples<<'/'<<ultra.volumetricAtmosphereAppliedShaftSamples
   <<" timingSamples="<<frameTimes.size()<<" p95Ns="<<p95<<" p99Ns="<<p99<<" maxNs="<<maximum<<" trackedBytes="<<renderer.MemoryStats().trackedCapacityBytes
   <<" pending="<<ultra.pendingGpuReleaseBytes<<" errors="<<ultra.debugErrorCount<<" warnings="<<ultra.debugWarningCount<<'\n';
  if(!resourceTruth||!timingTruth||!gpuValidationValid||ultra.debugErrorCount||ultra.debugWarningCount)result=4;

  if(renderer.ApplyDevelopmentUltraResourceProfile(8192,4,.625f))result=5;
  const RendererStats rejected=renderer.GetStats();
  if(rejected.directionalShadowResolution!=4096||rejected.directionalShadowCascadeCapacity!=4||rejected.directionalShadowMapBytes!=expectedShadowBytes||
     std::abs(rejected.volumetricAtmosphereResolutionScale-.75f)>.0001f)result=5;

  if(!renderer.RestoreDevelopmentUltraResourceBaseline(baseline))result=6;
  draw.SetDirectionalShadowCascades({4,2048,.1f,220.f,.70f,.10f});
  renderer.WaitIdle();
  const RendererStats restored=renderer.GetStats();
  const bool honestShadowDelta=restored.directionalShadowMapAllocations==baseline.shadowAllocations+2&&restored.directionalShadowMapReleases==baseline.shadowReleases+2&&restored.directionalShadowDescriptorSwitches==baseline.shadowDescriptorSwitches+2&&restored.directionalShadowResourceGeneration==baseline.shadowResourceGeneration+2;
  const bool honestVolumetricDelta=restored.volumetricAtmosphereResourceCreates>ultra.volumetricAtmosphereResourceCreates&&restored.volumetricAtmosphereResourceReleases>ultra.volumetricAtmosphereResourceReleases&&restored.volumetricAtmosphereResourceCreates-ultra.volumetricAtmosphereResourceCreates==restored.volumetricAtmosphereResourceReleases-ultra.volumetricAtmosphereResourceReleases;
  if(restored.directionalShadowResolution!=baseline.shadowResolution||restored.directionalShadowCascadeCapacity!=baseline.shadowCascadeCapacity||restored.directionalShadowMapBytes!=baseline.shadowResidentBytes||restored.volumetricAtmosphereWidth!=baseline.volumetricAtmosphereWidth||restored.volumetricAtmosphereHeight!=baseline.volumetricAtmosphereHeight||restored.volumetricAtmosphereResidentBytes!=baseline.volumetricAtmosphereResidentBytes||std::abs(restored.volumetricAtmosphereResolutionScale-baseline.volumetricAtmosphereResolutionScale)>.0001f||!honestShadowDelta||!honestVolumetricDelta||restored.pendingGpuReleaseBytes)result=7;
  std::cout<<"restore baseline shadow="<<restored.directionalShadowResolution<<'x'<<restored.directionalShadowCascadeCapacity<<" volumetric="<<restored.volumetricAtmosphereWidth<<'x'<<restored.volumetricAtmosphereHeight<<" counterDelta="<<(restored.directionalShadowMapAllocations-baseline.shadowAllocations)<<'/'<<(restored.directionalShadowMapReleases-baseline.shadowReleases)<<'/'<<(restored.directionalShadowDescriptorSwitches-baseline.shadowDescriptorSwitches)<<'/'<<(restored.directionalShadowResourceGeneration-baseline.shadowResourceGeneration)<<" pending="<<restored.pendingGpuReleaseBytes<<'\n';
  renderer.Render(draw,{.015f,.025f,.05f,1});
  renderer.WaitIdle();
  const RendererStats rerendered=renderer.GetStats();
  if(rerendered.directionalShadowResolution!=restored.directionalShadowResolution||rerendered.directionalShadowCascadeCapacity!=restored.directionalShadowCascadeCapacity||rerendered.directionalShadowMapBytes!=restored.directionalShadowMapBytes||rerendered.directionalShadowMapAllocations!=restored.directionalShadowMapAllocations||rerendered.directionalShadowMapReleases!=restored.directionalShadowMapReleases||rerendered.directionalShadowDescriptorSwitches!=restored.directionalShadowDescriptorSwitches||rerendered.directionalShadowResourceGeneration!=restored.directionalShadowResourceGeneration||rerendered.volumetricAtmosphereWidth!=restored.volumetricAtmosphereWidth||rerendered.volumetricAtmosphereHeight!=restored.volumetricAtmosphereHeight||rerendered.volumetricAtmosphereResidentBytes!=restored.volumetricAtmosphereResidentBytes||rerendered.volumetricAtmosphereResourceCreates!=restored.volumetricAtmosphereResourceCreates||rerendered.volumetricAtmosphereResourceReleases!=restored.volumetricAtmosphereResourceReleases||rerendered.pendingGpuReleaseBytes)result=7;
  renderer.DestroyMesh(mesh);
  renderer.WaitIdle();
 }
 DestroyWindow(window);
 std::cout<<"DevelopmentUltraRendererIntegration "<<(result?"FAIL":"PASS")<<'\n';
 return result;
}
